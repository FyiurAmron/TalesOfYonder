#include "yt2/video.h"

#include <string.h>

void yt2_video_init(yt2_video *video, uint16_t width, uint16_t height,
                    uint8_t bpp)
{
    memset(video, 0, sizeof(*video));
    video->width = width;
    video->height = height;
    video->bpp = bpp;
    video->force_full = 1;
    video->force_palette = 1;
}

int yt2_video_set_mode(yt2_video *video, uint16_t width, uint16_t height,
                       uint8_t bpp)
{
    if (width == 0u || height == 0u ||
        width > YT2_VIDEO_MAX_WIDTH || height > YT2_VIDEO_MAX_HEIGHT) {
        return YT2_ERR;
    }
    if (video->width != width || video->height != height ||
        video->bpp != bpp) {
        video->width = width;
        video->height = height;
        video->bpp = bpp;
        video->mode_sent = 0;
        video->force_full = 1;
        video->force_palette = 1;
    }
    return YT2_OK;
}

int yt2_video_set_palette(yt2_video *video, uint16_t start, uint16_t count,
                          const uint8_t *dac)
{
    uint32_t bytes;
    uint32_t offset;

    if (start > 255u || count > 256u || start + count > 256u) {
        return YT2_ERR;
    }
    bytes = (uint32_t)count * 3u;
    offset = (uint32_t)start * 3u;
    for (uint32_t index = 0; index < bytes; index++) {
        video->palette[offset + index] = dac[index];
    }
    return YT2_OK;
}

void yt2_video_force_keyframe(yt2_video *video)
{
    video->mode_sent = 0;
    video->force_full = 1;
    video->force_palette = 1;
}

/* The seam's row checksum: a 16-bit sum and a rotate/xor hash over the row's
   little-endian words.  It is the comparison key for the row pass and the
   scroll detector, exactly as in the asm seam. */
static uint32_t yt2_video_row_sum(const uint8_t *pixels, uint16_t width)
{
    uint16_t sum = 0;
    uint16_t hash = 0;
    uint32_t index;

    for (index = 0; index + 1u < width; index += 2u) {
        uint16_t word = (uint16_t)(pixels[index] |
                                   ((uint16_t)pixels[index + 1u] << 8));

        sum = (uint16_t)(sum + word);
        hash = (uint16_t)((hash << 1) | (hash >> 15));
        hash ^= sum;
    }
    if ((width & 1u) != 0u) {
        sum = (uint16_t)(sum + pixels[width - 1u]);
        hash = (uint16_t)((hash << 1) | (hash >> 15));
        hash ^= sum;
    }
    return ((uint32_t)hash << 16) | sum;
}

/*
 * Run-length row encoder, the C port of sub_SeamEncodeRow.  Runs of one or
 * two pixels become literals; longer runs use 00 (repeat the previous pixel)
 * or 01 (a new value), split at 255.  Literals ship in chunks of at most 255
 * and remember their last pixel, so a following run can use 00.
 */
typedef struct yt2_row_runs {
    uint8_t *out;
    uint32_t length;
    uint8_t literals[255];
    uint16_t literal_count;
    uint8_t last;
    uint8_t have_last;
} yt2_row_runs;

static void yt2_row_runs_flush_literals(yt2_row_runs *runs)
{
    if (runs->literal_count == 0u) {
        return;
    }
    runs->out[runs->length++] = 0x02u;
    runs->out[runs->length++] = (uint8_t)runs->literal_count;
    for (uint16_t index = 0; index < runs->literal_count; index++) {
        runs->out[runs->length++] = runs->literals[index];
    }
    runs->last = runs->literals[runs->literal_count - 1u];
    runs->have_last = 1;
    runs->literal_count = 0;
}

static void yt2_row_runs_add_literal(yt2_row_runs *runs, uint8_t value)
{
    runs->literals[runs->literal_count++] = value;
    if (runs->literal_count == 255u) {
        yt2_row_runs_flush_literals(runs);
    }
}

static void yt2_row_runs_flush_run(yt2_row_runs *runs, uint8_t value,
                                   uint16_t count)
{
    if (count < 3u) {
        while (count > 0u) {
            yt2_row_runs_add_literal(runs, value);
            count--;
        }
        return;
    }
    yt2_row_runs_flush_literals(runs);
    while (count > 0u) {
        uint8_t chunk = count > 255u ? 255u : (uint8_t)count;

        if (runs->have_last != 0u && value == runs->last) {
            runs->out[runs->length++] = 0x00u;
            runs->out[runs->length++] = chunk;
        } else {
            runs->out[runs->length++] = 0x01u;
            runs->out[runs->length++] = chunk;
            runs->out[runs->length++] = value;
        }
        runs->last = value;
        runs->have_last = 1;
        count = (uint16_t)(count - chunk);
    }
}

static int yt2_video_encode_row(const uint8_t *pixels, uint16_t width,
                                uint8_t *encoded)
{
    yt2_row_runs runs;
    uint8_t value;
    uint16_t count;

    runs.out = encoded;
    runs.length = 0;
    runs.literal_count = 0;
    runs.last = 0;
    runs.have_last = 0;
    if (width == 0u) {
        encoded[0] = 0x03u;
        return 1;
    }
    value = pixels[0];
    count = 1;
    for (uint16_t index = 1; index < width; index++) {
        if (pixels[index] == value) {
            count++;
        } else {
            yt2_row_runs_flush_run(&runs, value, count);
            value = pixels[index];
            count = 1;
        }
    }
    yt2_row_runs_flush_run(&runs, value, count);
    yt2_row_runs_flush_literals(&runs);
    encoded[runs.length++] = 0x03u;
    return (int)runs.length;
}

static void yt2_video_scan_rows(yt2_video *video)
{
    uint32_t row_bytes = (uint32_t)video->width;

    for (uint16_t row = 0; row < video->height; row++) {
        video->row_sum[row] =
            yt2_video_row_sum(video->pixels + (uint32_t)row * row_bytes,
                              video->width);
    }
}

/* Score a candidate shift: how many rows of the overlap it explains. */
static int yt2_video_scroll_score(const yt2_video *video, int delta)
{
    int matches = 0;

    if (delta >= 0) {
        for (int row = delta; row < (int)video->height; row++) {
            if (video->row_sum[row] == video->shadow_sum[row - delta]) {
                matches++;
            }
        }
    } else {
        int up = -delta;

        for (int row = 0; row + up < (int)video->height; row++) {
            if (video->row_sum[row] == video->shadow_sum[row + up]) {
                matches++;
            }
        }
    }
    return matches;
}

/*
 * Recognise a whole-screen vertical pan, the C port of sub_SeamScrollCheck.
 * A pan makes this pass's row y equal the client's row y-dy; score every
 * step in +-8 and take the best only when it beats no shift at all and
 * explains at least 100 rows.  The client's checksum table is shifted with
 * the message and the rows that slid in are poisoned so the row pass sends
 * them.
 */
#define YT2_SCROLL_MAX_STEP 8
#define YT2_SCROLL_MIN_CHANGED 32
#define YT2_SCROLL_MIN_MATCH 100

static int yt2_video_scroll_check(yt2_video *video, yt2_service *service)
{
    int changed = 0;
    int best;
    int best_delta = 0;

    if (video->width != 320u || video->height != 200u) {
        return 0;
    }
    for (uint16_t row = 0; row < video->height; row++) {
        if (video->row_sum[row] != video->shadow_sum[row]) {
            changed++;
        }
    }
    if (changed < YT2_SCROLL_MIN_CHANGED) {
        return 0;
    }
    best = yt2_video_scroll_score(video, 0);
    for (int step = 1; step <= YT2_SCROLL_MAX_STEP; step++) {
        int score = yt2_video_scroll_score(video, step);

        if (score > best) {
            best = score;
            best_delta = step;
        }
        score = yt2_video_scroll_score(video, -step);
        if (score > best) {
            best = score;
            best_delta = -step;
        }
    }
    if (best_delta == 0 || best < YT2_SCROLL_MIN_MATCH) {
        return 0;
    }
    if (yt2_service_video_scroll(service, (int8_t)best_delta) != YT2_OK) {
        return YT2_ERR;
    }
    if (best_delta > 0) {
        for (int row = (int)video->height - 1; row >= best_delta; row--) {
            video->shadow_sum[row] = video->shadow_sum[row - best_delta];
        }
        for (int row = 0; row < best_delta; row++) {
            video->shadow_sum[row] = ~video->row_sum[row];
        }
    } else {
        int up = -best_delta;

        for (int row = 0; row + up < (int)video->height; row++) {
            video->shadow_sum[row] = video->shadow_sum[row + up];
        }
        for (int row = (int)video->height - up;
             row < (int)video->height; row++) {
            video->shadow_sum[row] = ~video->row_sum[row];
        }
    }
    video->scrolls++;
    return 1;
}

static int yt2_video_send_rows(yt2_video *video, yt2_service *service)
{
    uint8_t encoded[YT2_VIDEO_MAX_WIDTH + 8u];
    uint32_t row_bytes = (uint32_t)video->width;
    int sent = 0;

    for (uint16_t row = 0; row < video->height; row++) {
        const uint8_t *pixels = video->pixels + (uint32_t)row * row_bytes;
        int changed = video->force_full != 0u ||
                      video->row_sum[row] != video->shadow_sum[row];

        if (changed) {
            int length = yt2_video_encode_row(pixels, video->width, encoded);

            if (length < 0 ||
                yt2_service_video_rect_rle(service, 0, row, video->width, 1,
                                           encoded,
                                           (uint32_t)length) != YT2_OK) {
                return YT2_ERR;
            }
            video->shadow_sum[row] = video->row_sum[row];
            video->rects++;
            sent = 1;
        }
    }
    return sent;
}

static int yt2_video_send_palette(yt2_video *video, yt2_service *service)
{
    uint16_t first = 0xFFFFu;
    uint16_t last = 0;
    int changed = 0;

    if (video->force_palette != 0u || video->palette_sent == 0u) {
        if (yt2_service_video_palette(service, 0, 256, video->palette) != YT2_OK) {
            return YT2_ERR;
        }
        changed = 1;
    } else {
        for (uint16_t entry = 0; entry < 256u; entry++) {
            uint32_t offset = (uint32_t)entry * 3u;

            if (video->palette[offset] != video->sent_palette[offset] ||
                video->palette[offset + 1u] != video->sent_palette[offset + 1u] ||
                video->palette[offset + 2u] != video->sent_palette[offset + 2u]) {
                if (first == 0xFFFFu) {
                    first = entry;
                }
                last = entry;
                changed = 1;
            }
        }
        if (changed) {
            uint16_t count = (uint16_t)(last - first + 1u);
            const uint8_t *dac = video->palette + (uint32_t)first * 3u;

            if (yt2_service_video_palette(service, first, count, dac) != YT2_OK) {
                return YT2_ERR;
            }
        }
    }
    if (changed) {
        for (uint32_t index = 0; index < YT2_VIDEO_PALETTE_SIZE; index++) {
            video->sent_palette[index] = video->palette[index];
        }
        video->palette_sent = 1;
        video->palette_messages++;
    }
    return changed;
}

int yt2_video_present_blank(yt2_video *video, yt2_service *service)
{
    /* Switch the client and leave it on a blank screen.  A VBE decoder fills
       the hidden banks first and loads its palette last; sending those rows
       now would show the picture with the previous scene's colours. */
    if (video->mode_sent == 0u || video->sent_width != video->width ||
        video->sent_height != video->height || video->sent_bpp != video->bpp) {
        if (yt2_service_video_set_mode(service, video->width, video->height,
                                       video->bpp) != YT2_OK) {
            return YT2_ERR;
        }
        video->sent_width = video->width;
        video->sent_height = video->height;
        video->sent_bpp = video->bpp;
        video->mode_sent = 1;
        video->force_full = 1;
        video->force_palette = 1;
        if (yt2_service_video_frame_end(service) != YT2_OK) {
            return YT2_ERR;
        }
        video->frame_ends++;
        video->frames++;
    }
    return YT2_OK;
}

int yt2_video_present(yt2_video *video, yt2_service *service)
{
    int dirty = 0;
    int rows;
    int palette;

    if (video->width == 0u || video->height == 0u ||
        video->width > YT2_VIDEO_MAX_WIDTH ||
        video->height > YT2_VIDEO_MAX_HEIGHT) {
        return YT2_ERR;
    }
    if (video->mode_sent == 0u || video->sent_width != video->width ||
        video->sent_height != video->height || video->sent_bpp != video->bpp) {
        if (yt2_service_video_set_mode(service, video->width, video->height,
                                       video->bpp) != YT2_OK) {
            return YT2_ERR;
        }
        video->sent_width = video->width;
        video->sent_height = video->height;
        video->sent_bpp = video->bpp;
        video->mode_sent = 1;
        video->force_full = 1;
        dirty = 1;
    }
    yt2_video_scan_rows(video);
    if (video->force_full == 0u) {
        int scroll = yt2_video_scroll_check(video, service);

        if (scroll < 0) {
            return YT2_ERR;
        }
        if (scroll > 0) {
            dirty = 1;
        }
    }
    rows = yt2_video_send_rows(video, service);
    if (rows < 0) {
        return YT2_ERR;
    }
    palette = yt2_video_send_palette(video, service);
    if (palette < 0) {
        return YT2_ERR;
    }
    if (dirty || rows > 0 || palette > 0 || video->force_full != 0u) {
        if (yt2_service_video_frame_end(service) != YT2_OK) {
            return YT2_ERR;
        }
        video->frame_ends++;
    }
    video->force_full = 0;
    video->force_palette = 0;
    video->frames++;
    return YT2_OK;
}
