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

int yt2_present_frame(yt2_presenter *presenter, yt2_folder_out *out,
                      const yt2_surface *surface, const yt2_palette *palette,
                      int force_keyframe)
{
    uint8_t *payload;
    size_t capacity;
    int length;
    unsigned int first;
    unsigned int last;

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

    if (force_keyframe || !presenter->have_mode ||
        surface->width != presenter->width ||
        surface->height != presenter->height) {
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

    /* One rect per changed row: a full-frame rect would breach the
       2048-byte payload limit the client enforces.  Rows are tightly packed
       (stride == width) in every B2 surface. */
    if (9u + surface->width > capacity ||
        9u + surface->width > YT2_PROTOCOL_MAX_PAYLOAD) {
        return YT2_STATUS_ERROR;
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
        length = yt2_video_encode_rect(payload, capacity, 0u, row,
                                       surface->width, 1u, pixels);
        if (emit(presenter, out, YT2_FAMILY_VIDEO, YT2_CODEC_RAW, length) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        memcpy(shadow_row, pixels, surface->width);
    }

    length = yt2_video_encode_frame_end(payload, capacity);
    if (emit(presenter, out, YT2_FAMILY_VIDEO, YT2_CODEC_RAW, length) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return yt2_folder_out_commit(out);
}
