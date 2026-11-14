#include "yt2/present.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/protocol.h"
#include "yt2/status.h"

#define YT2_PRESENT_SCRATCH (YT2_PROTOCOL_MAX_FRAME + YT2_FRAME_HEADER_SIZE)

int yt2_presenter_init(yt2_presenter *presenter)
{
    if (presenter == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(presenter, 0, sizeof(*presenter));
    presenter->scratch = malloc(YT2_PRESENT_SCRATCH);
    if (presenter->scratch == NULL) {
        return YT2_STATUS_ERROR;
    }
    presenter->scratch_size = YT2_PRESENT_SCRATCH;
    return YT2_STATUS_OK;
}

void yt2_presenter_done(yt2_presenter *presenter)
{
    if (presenter == NULL) {
        return;
    }
    free(presenter->scratch);
    free(presenter->shadow);
    memset(presenter, 0, sizeof(*presenter));
}

static int emit(yt2_presenter *presenter, yt2_folder_out *out, uint8_t family,
                uint8_t codec, int payload_length)
{
    if (payload_length < 0) {
        return YT2_STATUS_ERROR;
    }
    yt2_frame_encode_header(presenter->scratch, family, 0u, codec,
                            presenter->seq,
                            (uint32_t)payload_length);
    if (yt2_folder_out_write(out, presenter->scratch,
                             YT2_FRAME_HEADER_SIZE +
                                 (uint32_t)payload_length) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    presenter->seq++;
    return YT2_STATUS_OK;
}

int yt2_present_hello(yt2_presenter *presenter, yt2_folder_out *out)
{
    int length;

    if (presenter == NULL || presenter->scratch == NULL || out == NULL) {
        return YT2_STATUS_ERROR;
    }
    length = yt2_control_encode_hello(presenter->scratch + YT2_FRAME_HEADER_SIZE,
                                      presenter->scratch_size -
                                          YT2_FRAME_HEADER_SIZE,
                                      YT2_ROLE_ENGINE, YT2_PROTOCOL_VERSION);
    if (emit(presenter, out, YT2_FAMILY_CONTROL, YT2_CODEC_RAW, length) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return yt2_folder_out_commit(out);
}

/* Scroll detector tuning, the spec's rules shared with the seam and C
 * engines: steps in +-8, only when more rows move than stay, at least 100
 * explained, and never for fewer than 32 changed rows.  Unlike those two
 * the port compares full shadow rows (it keeps the whole shadow, so no
 * checksum can collide) and applies the rules at any height. */
#define YT2_SCROLL_MAX_STEP 8
#define YT2_SCROLL_MIN_CHANGED 32
#define YT2_SCROLL_MIN_MATCH 100

/* Rows of the overlap explained by a shift (memcmp, exact). */
static unsigned int scroll_score(const yt2_surface *surface,
                                 const uint8_t *shadow, int delta)
{
    unsigned int matches = 0u;
    unsigned int height = surface->height;

    if (delta >= 0) {
        for (unsigned int row = (unsigned int)delta; row < height; row++) {
            if (memcmp(surface->pixels + (size_t)row * surface->stride,
                       shadow +
                           (size_t)(row - (unsigned int)delta) *
                               surface->width,
                       surface->width) == 0) {
                matches++;
            }
        }
    } else {
        unsigned int up = (unsigned int)(-delta);

        for (unsigned int row = 0u; row + up < height; row++) {
            if (memcmp(surface->pixels + (size_t)row * surface->stride,
                       shadow + (size_t)(row + up) * surface->width,
                       surface->width) == 0) {
                matches++;
            }
        }
    }
    return matches;
}

/*
 * Recognise a whole-screen vertical pan against the shadow: emit one
 * scroll message, shift the shadow with it and poison the slid-in rows
 * (bitwise NOT of the new pixels, so the row pass below must resend
 * them).  Returns 1 when a scroll went out, 0 for plain rows, and
 * YT2_STATUS_ERROR when the emit fails.
 */
static int try_scroll(yt2_presenter *presenter, yt2_folder_out *out,
                      const yt2_surface *surface)
{
    uint8_t *payload = presenter->scratch + YT2_FRAME_HEADER_SIZE;
    size_t capacity = presenter->scratch_size - YT2_FRAME_HEADER_SIZE;
    unsigned int changed = 0u;
    unsigned int best;
    int best_delta = 0;
    int length;

    for (unsigned int row = 0u; row < surface->height; row++) {
        if (memcmp(surface->pixels + (size_t)row * surface->stride,
                   presenter->shadow + (size_t)row * surface->width,
                   surface->width) != 0) {
            changed++;
        }
    }
    if (changed < YT2_SCROLL_MIN_CHANGED) {
        return 0;
    }
    best = scroll_score(surface, presenter->shadow, 0);
    for (int step = 1; step <= YT2_SCROLL_MAX_STEP; step++) {
        unsigned int score = scroll_score(surface, presenter->shadow, step);

        if (score > best) {
            best = score;
            best_delta = step;
        }
        score = scroll_score(surface, presenter->shadow, -step);
        if (score > best) {
            best = score;
            best_delta = -step;
        }
    }
    if (best_delta == 0 || best < YT2_SCROLL_MIN_MATCH) {
        return 0;
    }
    length = yt2_video_encode_scroll(payload, capacity, best_delta);
    if (emit(presenter, out, YT2_FAMILY_VIDEO, YT2_CODEC_RAW, length) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    if (best_delta > 0) {
        unsigned int down = (unsigned int)best_delta;

        for (unsigned int row = surface->height; row-- > down;) {
            memcpy(presenter->shadow + (size_t)row * surface->width,
                   presenter->shadow +
                       (size_t)(row - down) * surface->width,
                   surface->width);
        }
        for (unsigned int row = 0u; row < down; row++) {
            const uint8_t *pixels =
                surface->pixels + (size_t)row * surface->stride;
            uint8_t *shadow_row =
                presenter->shadow + (size_t)row * surface->width;

            for (unsigned int i = 0u; i < surface->width; i++) {
                shadow_row[i] = (uint8_t)~pixels[i];
            }
        }
    } else {
        unsigned int up = (unsigned int)(-best_delta);

        for (unsigned int row = 0u; row + up < surface->height; row++) {
            memcpy(presenter->shadow + (size_t)row * surface->width,
                   presenter->shadow + (size_t)(row + up) * surface->width,
                   surface->width);
        }
        for (unsigned int row = surface->height - up; row < surface->height;
             row++) {
            const uint8_t *pixels =
                surface->pixels + (size_t)row * surface->stride;
            uint8_t *shadow_row =
                presenter->shadow + (size_t)row * surface->width;

            for (unsigned int i = 0u; i < surface->width; i++) {
                shadow_row[i] = (uint8_t)~pixels[i];
            }
        }
    }
    return 1;
}

int yt2_present_pace(yt2_presenter *presenter, yt2_folder_out *out,
                     unsigned int ms)
{
    int length;

    if (presenter == NULL || presenter->scratch == NULL || out == NULL) {
        return YT2_STATUS_ERROR;
    }
    length = yt2_tick_encode_pace(presenter->scratch + YT2_FRAME_HEADER_SIZE,
                                  presenter->scratch_size -
                                      YT2_FRAME_HEADER_SIZE,
                                  ms);
    if (emit(presenter, out, YT2_FAMILY_TICK, YT2_CODEC_RAW, length) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return yt2_folder_out_commit(out);
}

int yt2_present_frame(yt2_presenter *presenter, yt2_folder_out *out,
                      const yt2_surface *surface, const yt2_palette *palette,
                      int force_keyframe, uint32_t tick)
{
    uint8_t *payload;
    size_t capacity;
    int length;
    unsigned int first;
    unsigned int last;
    int keyframe;

    if (presenter == NULL || presenter->scratch == NULL || out == NULL ||
        surface == NULL || surface->pixels == NULL || palette == NULL ||
        surface->stride != surface->width) {
        return YT2_STATUS_ERROR;
    }
    payload = presenter->scratch + YT2_FRAME_HEADER_SIZE;
    capacity = presenter->scratch_size - YT2_FRAME_HEADER_SIZE;

    if (presenter->shadow == NULL ||
        surface->width != presenter->shadow_width ||
        surface->height != presenter->shadow_height) {
        uint8_t *shadow;

        if (surface->width == 0u || surface->height == 0u ||
            surface->width > SIZE_MAX / surface->height) {
            return YT2_STATUS_ERROR;
        }
        shadow = realloc(presenter->shadow,
                         (size_t)surface->width * surface->height);
        if (shadow == NULL) {
            return YT2_STATUS_ERROR;
        }
        presenter->shadow = shadow;
        presenter->shadow_width = surface->width;
        presenter->shadow_height = surface->height;
        force_keyframe = 1;
    }

    keyframe = force_keyframe || !presenter->have_mode ||
                 surface->width != presenter->width ||
                 surface->height != presenter->height;

    if (keyframe) {
        length = yt2_video_encode_set_mode(payload, capacity, surface->width,
                                           surface->height, 8u);
        if (emit(presenter, out, YT2_FAMILY_VIDEO, YT2_CODEC_RAW, length) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        presenter->width = surface->width;
        presenter->height = surface->height;
        presenter->have_mode = 1;
    }

    first = 0u;
    last = 0u;
    if (force_keyframe || !presenter->palette_valid) {
        last = YT2_PALETTE_ENTRY_COUNT - 1u;
    } else {
        while (first < YT2_PALETTE_ENTRY_COUNT &&
               memcmp(palette->rgb + first * YT2_PALETTE_ENTRY_SIZE,
                      presenter->sent_palette + first * YT2_PALETTE_ENTRY_SIZE,
                      YT2_PALETTE_ENTRY_SIZE) == 0) {
            first++;
        }
        if (first < YT2_PALETTE_ENTRY_COUNT) {
            last = YT2_PALETTE_ENTRY_COUNT - 1u;
            while (memcmp(palette->rgb + last * YT2_PALETTE_ENTRY_SIZE,
                          presenter->sent_palette +
                              last * YT2_PALETTE_ENTRY_SIZE,
                          YT2_PALETTE_ENTRY_SIZE) == 0) {
                last--;
            }
        }
    }
    if (first < YT2_PALETTE_ENTRY_COUNT) {
        length = yt2_video_encode_palette(
            payload, capacity, first, last - first + 1u,
            palette->rgb + first * YT2_PALETTE_ENTRY_SIZE);
        if (emit(presenter, out, YT2_FAMILY_VIDEO, YT2_CODEC_RAW, length) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        memcpy(presenter->sent_palette, palette->rgb, YT2_PALETTE_SIZE);
        presenter->palette_valid = 1;
    }

    /* Whole-screen pans go out as one scroll plus residue rects; repaints
       (key frames) always go out as rows.  The scroll shifts the shadow
       with it, so the row pass below sends exactly the slid-in rows plus
       whatever did not move with the picture. */
    if (!keyframe) {
        int scrolled = try_scroll(presenter, out, surface);

        if (scrolled < 0) {
            return YT2_STATUS_ERROR;
        }
    }

    /* One run-coded rect per changed row: a full-frame rect would breach the
       2048-byte payload limit the client enforces.  Rows are tightly packed
       (stride == width) in every B2 surface.  The worst case per row is one
       literal chunk per 255 pixels plus the end-of-row byte. */
    {
        size_t worst = 10u + (size_t)surface->width +
                       2u * (((size_t)surface->width + 254u) / 255u);

        if (worst > capacity || worst > YT2_PROTOCOL_MAX_PAYLOAD) {
            return YT2_STATUS_ERROR;
        }
    }
    for (unsigned int row = 0; row < surface->height; row++) {
        const uint8_t *pixels =
            surface->pixels + (size_t)row * surface->stride;
        uint8_t *shadow_row =
            presenter->shadow + (size_t)row * surface->width;

        if (!force_keyframe &&
            memcmp(pixels, shadow_row, surface->width) == 0) {
            continue;
        }
        length = yt2_video_encode_rect_runs(payload, capacity, 0u, row,
                                            surface->width, 1u, pixels);
        if (length < 0 || length > (int)YT2_PROTOCOL_MAX_PAYLOAD) {
            return YT2_STATUS_ERROR;
        }
        if (emit(presenter, out, YT2_FAMILY_VIDEO, YT2_CODEC_RUNS, length) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        memcpy(shadow_row, pixels, surface->width);
    }

    length = yt2_tick_encode_tick(payload, capacity, tick);
    if (emit(presenter, out, YT2_FAMILY_TICK, YT2_CODEC_RAW, length) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    length = yt2_video_encode_frame_end(payload, capacity);
    if (emit(presenter, out, YT2_FAMILY_VIDEO, YT2_CODEC_RAW, length) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return yt2_folder_out_commit(out);
}
