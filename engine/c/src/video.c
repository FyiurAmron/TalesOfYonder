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

static int yt2_video_send_rows(yt2_video *video, yt2_service *service)
{
    uint32_t row_bytes = (uint32_t)video->width;
    int sent = 0;

    for (uint16_t row = 0; row < video->height; row++) {
        const uint8_t *pixels = video->pixels + (uint32_t)row * row_bytes;
        uint8_t *shadow = video->shadow + (uint32_t)row * row_bytes;
        int changed = video->force_full != 0u;

        if (!changed) {
            for (uint32_t index = 0; index < row_bytes; index++) {
                if (pixels[index] != shadow[index]) {
                    changed = 1;
                    break;
                }
            }
        }
        if (changed) {
            if (yt2_service_video_rect(service, 0, 0, row, video->width, 1,
                                       pixels) != YT2_OK) {
                return YT2_ERR;
            }
            for (uint32_t index = 0; index < row_bytes; index++) {
                shadow[index] = pixels[index];
            }
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
