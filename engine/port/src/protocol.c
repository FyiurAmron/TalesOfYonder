#include "yt2/protocol.h"

static void put_le16(uint8_t *out, unsigned int value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint16_t get_le16(const uint8_t *in)
{
    return (uint16_t)((uint16_t)in[0] | ((uint16_t)in[1] << 8));
}

static uint32_t get_le32(const uint8_t *in)
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

void yt2_frame_encode_header(uint8_t *out, uint8_t family, uint8_t flags,
                             uint8_t codec, uint32_t seq, uint32_t length)
{
    put_le16(out, YT2_PROTOCOL_MAGIC);
    out[2] = YT2_PROTOCOL_VERSION;
    out[3] = family;
    out[4] = flags;
    out[5] = codec;
    put_le32(out + 6, seq);
    put_le32(out + 10, length);
}

int yt2_frame_decode_header(const uint8_t *frame, size_t size,
                            yt2_frame_header *header)
{
    uint32_t length;

    if (frame == NULL || header == NULL || size < YT2_FRAME_HEADER_SIZE) {
        return -1;
    }
    if (get_le16(frame) != YT2_PROTOCOL_MAGIC ||
        frame[2] != YT2_PROTOCOL_VERSION) {
        return -1;
    }
    length = get_le32(frame + 10);
    if (length > YT2_PROTOCOL_MAX_FRAME ||
        (size_t)length > size - YT2_FRAME_HEADER_SIZE) {
        return -1;
    }
    header->version = frame[2];
    header->family = frame[3];
    header->flags = frame[4];
    header->codec = frame[5];
    header->seq = get_le32(frame + 6);
    header->length = length;
    return (int)length;
}

int yt2_video_encode_set_mode(uint8_t *out, size_t capacity, unsigned int w,
                              unsigned int h, unsigned int bpp)
{
    if (out == NULL || capacity < 6u || w > 0xFFFFu || h > 0xFFFFu ||
        bpp > 0xFFu) {
        return -1;
    }
    out[0] = YT2_VIDEO_SET_MODE;
    put_le16(out + 1, w);
    put_le16(out + 3, h);
    out[5] = (uint8_t)bpp;
    return 6;
}

int yt2_video_encode_palette(uint8_t *out, size_t capacity, unsigned int start,
                             unsigned int count, const uint8_t *rgb)
{
    size_t need;

    if (out == NULL || rgb == NULL || count == 0u || count > 256u ||
        start > 255u || start + count > 256u) {
        return -1;
    }
    need = 5u + (size_t)count * 3u;
    if (capacity < need) {
        return -1;
    }
    out[0] = YT2_VIDEO_PALETTE;
    put_le16(out + 1, start);
    put_le16(out + 3, count);
    for (size_t i = 0; i < (size_t)count * 3u; i++) {
        out[5 + i] = (uint8_t)(rgb[i] & 0x3Fu);
    }
    return (int)need;
}

int yt2_video_encode_rect(uint8_t *out, size_t capacity, unsigned int x,
                          unsigned int y, unsigned int w, unsigned int h,
                          const uint8_t *pixels)
{
    size_t need;

    if (out == NULL || pixels == NULL || w == 0u || h == 0u || w > 0xFFFFu ||
        h > 0xFFFFu || x > 0xFFFFu || y > 0xFFFFu ||
        (size_t)w > (SIZE_MAX - 9u) / (size_t)h) {
        return -1;
    }
    need = 9u + (size_t)w * (size_t)h;
    if (capacity < need || need > YT2_PROTOCOL_MAX_FRAME) {
        return -1;
    }
    out[0] = YT2_VIDEO_RECT;
    put_le16(out + 1, x);
    put_le16(out + 3, y);
    put_le16(out + 5, w);
    put_le16(out + 7, h);
    for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
        out[9 + i] = pixels[i];
    }
    return (int)need;
}

int yt2_video_encode_frame_end(uint8_t *out, size_t capacity)
{
    if (out == NULL || capacity < 1u) {
        return -1;
    }
    out[0] = YT2_VIDEO_FRAME_END;
    return 1;
}

/* Emit one `02` literal chunk; `pending` is 1..255. */
static int flush_runs_literals(uint8_t *out, size_t capacity, size_t *length,
                               const uint8_t *literals, unsigned int pending)
{
    if (*length + 2u + pending > capacity) {
        return -1;
    }
    out[(*length)++] = 0x02u;
    out[(*length)++] = (uint8_t)pending;
    for (unsigned int i = 0u; i < pending; i++) {
        out[(*length)++] = literals[i];
    }
    return 0;
}

/* One codec-1 row: runs of 3+ pixels as 00/01 split at 255, the rest merged
 * into literal chunks of at most 255.  Returns the encoded length. */
static int encode_runs_row(const uint8_t *pixels, unsigned int w, uint8_t *out,
                           size_t capacity)
{
    uint8_t literals[255];
    unsigned int pending = 0u;
    unsigned int at = 0u;
    size_t length = 0u;
    uint8_t last = 0u;
    int have_last = 0;

    while (at < w) {
        unsigned int run = 1u;

        while (at + run < w && pixels[at + run] == pixels[at]) {
            run++;
        }
        if (run < 3u) {
            for (unsigned int i = 0u; i < run; i++) {
                literals[pending++] = pixels[at];
                if (pending == 255u) {
                    if (flush_runs_literals(out, capacity, &length, literals,
                                            pending) != 0) {
                        return -1;
                    }
                    last = literals[254];
                    have_last = 1;
                    pending = 0u;
                }
            }
            at += run;
            continue;
        }
        if (pending > 0u) {
            if (flush_runs_literals(out, capacity, &length, literals, pending) !=
                0) {
                return -1;
            }
            last = literals[pending - 1u];
            have_last = 1;
            pending = 0u;
        }
        {
            unsigned int total = run;

            while (run > 0u) {
                unsigned int chunk = run > 255u ? 255u : run;

                if (have_last && pixels[at] == last) {
                    if (length + 2u > capacity) {
                        return -1;
                    }
                    out[length++] = 0x00u;
                    out[length++] = (uint8_t)chunk;
                } else {
                    if (length + 3u > capacity) {
                        return -1;
                    }
                    out[length++] = 0x01u;
                    out[length++] = (uint8_t)chunk;
                    out[length++] = pixels[at];
                }
                last = pixels[at];
                have_last = 1;
                run -= chunk;
            }
            at += total;
        }
    }
    if (pending > 0u &&
        flush_runs_literals(out, capacity, &length, literals, pending) != 0) {
        return -1;
    }
    if (length + 1u > capacity) {
        return -1;
    }
    out[length++] = 0x03u;
    return (int)length;
}

int yt2_video_encode_scroll(uint8_t *out, size_t capacity, int dy)
{
    if (out == NULL || capacity < 2u || dy < -128 || dy > 127) {
        return -1;
    }
    out[0] = YT2_VIDEO_SCROLL;
    out[1] = (uint8_t)(int8_t)dy;
    return 2;
}

int yt2_tick_encode_tick(uint8_t *out, size_t capacity, uint32_t n)
{
    if (out == NULL || capacity < 5u) {
        return -1;
    }
    out[0] = YT2_TICK_TICK;
    put_le32(out + 1, n);
    return 5;
}

int yt2_tick_encode_pace(uint8_t *out, size_t capacity, unsigned int ms)
{
    if (out == NULL || capacity < 3u || ms > 0xFFFFu) {
        return -1;
    }
    out[0] = YT2_TICK_PACE;
    put_le16(out + 1, ms);
    return 3;
}

int yt2_video_encode_rect_runs(uint8_t *out, size_t capacity, unsigned int x,
                               unsigned int y, unsigned int w, unsigned int h,
                               const uint8_t *pixels)
{
    size_t length;

    if (out == NULL || pixels == NULL || w == 0u || h == 0u || w > 0xFFFFu ||
        h > 0xFFFFu || x > 0xFFFFu || y > 0xFFFFu ||
        (size_t)w > (SIZE_MAX - 9u) / (size_t)h) {
        return -1;
    }
    if (capacity < 9u) {
        return -1;
    }
    out[0] = YT2_VIDEO_RECT;
    put_le16(out + 1, x);
    put_le16(out + 3, y);
    put_le16(out + 5, w);
    put_le16(out + 7, h);
    length = 9u;
    for (unsigned int row = 0u; row < h; row++) {
        int encoded = encode_runs_row(pixels + (size_t)row * w, w,
                                      out + length, capacity - length);

        if (encoded < 0 ||
            length + (size_t)encoded > YT2_PROTOCOL_MAX_FRAME) {
            return -1;
        }
        length += (size_t)encoded;
    }
    return (int)length;
}

int yt2_audio_encode_opl_write(uint8_t *out, size_t capacity, unsigned int port,
                               unsigned int reg, unsigned int val)
{
    if (out == NULL || capacity < 5u || port > 0xFFFFu || reg > 0xFFu ||
        val > 0xFFu) {
        return -1;
    }
    out[0] = YT2_AUDIO_OPL_WRITE;
    put_le16(out + 1, port);
    out[3] = (uint8_t)reg;
    out[4] = (uint8_t)val;
    return 5;
}

int yt2_audio_encode_opl_batch(uint8_t *out, size_t capacity,
                               unsigned int count,
                               const yt2_opl_write *writes)
{
    size_t need;

    if (out == NULL || writes == NULL || count == 0u ||
        count > YT2_AUDIO_BATCH_MAX) {
        return -1;
    }
    need = 2u + (size_t)count * 4u;
    if (capacity < need) {
        return -1;
    }
    out[0] = YT2_AUDIO_OPL_BATCH;
    out[1] = (uint8_t)count;
    for (unsigned int i = 0u; i < count; i++) {
        put_le16(out + 2u + (size_t)i * 4u, writes[i].port);
        out[2u + (size_t)i * 4u + 2u] = writes[i].reg;
        out[2u + (size_t)i * 4u + 3u] = writes[i].val;
    }
    return (int)need;
}

int yt2_audio_encode_ctrl(uint8_t *out, size_t capacity, unsigned int rate,
                          unsigned int fmt, unsigned int on)
{
    if (out == NULL || capacity < 6u || rate > 0xFFFFu || fmt > 0xFFFFu ||
        on > 0xFFu) {
        return -1;
    }
    out[0] = YT2_AUDIO_CTRL;
    put_le16(out + 1, rate);
    put_le16(out + 3, fmt);
    out[5] = (uint8_t)on;
    return 6;
}

int yt2_audio_encode_opl_batch_timed(uint8_t *out, size_t capacity,
                                     unsigned int port, unsigned int count,
                                     const yt2_opl_timed_write *writes)
{
    size_t need;

    if (out == NULL || writes == NULL || port > 0xFFFFu || count == 0u ||
        count > YT2_AUDIO_BATCH_MAX) {
        return -1;
    }
    need = 4u + (size_t)count * 6u;
    if (capacity < need) {
        return -1;
    }
    out[0] = YT2_AUDIO_OPL_BATCH_TIMED;
    put_le16(out + 1, port);
    out[3] = (uint8_t)count;
    for (unsigned int i = 0u; i < count; i++) {
        put_le32(out + 4u + (size_t)i * 6u, writes[i].pit);
        out[4u + (size_t)i * 6u + 4u] = writes[i].reg;
        out[4u + (size_t)i * 6u + 5u] = writes[i].val;
    }
    return (int)need;
}

int yt2_audio_encode_speaker(uint8_t *out, size_t capacity, unsigned int gate,
                             unsigned int divisor, uint32_t pit)
{
    if (out == NULL || capacity < 8u || gate > 0xFFu || divisor > 0xFFFFu) {
        return -1;
    }
    out[0] = YT2_AUDIO_SPEAKER;
    out[1] = (uint8_t)gate;
    put_le16(out + 2, divisor);
    put_le32(out + 4, pit);
    return 8;
}

int yt2_audio_encode_sample(uint8_t *out, size_t capacity, unsigned int id,
                            uint32_t offset, uint32_t pit)
{
    if (out == NULL || capacity < 11u || id > 0xFFFFu) {
        return -1;
    }
    out[0] = YT2_AUDIO_SAMPLE;
    put_le16(out + 1, id);
    put_le32(out + 3, offset);
    put_le32(out + 7, pit);
    return 11;
}

int yt2_control_encode_hello(uint8_t *out, size_t capacity, unsigned int role,
                             unsigned int version)
{
    if (out == NULL || capacity < 3u || role > 0xFFu || version > 0xFFu) {
        return -1;
    }
    out[0] = YT2_CONTROL_HELLO;
    out[1] = (uint8_t)role;
    out[2] = (uint8_t)version;
    return 3;
}

int yt2_input_decode_frame(const uint8_t *frame, size_t size,
                           yt2_input_event *event)
{
    yt2_frame_header header;
    const uint8_t *payload;
    int length;

    if (event == NULL) {
        return 0;
    }
    event->kind = YT2_INPUT_EVENT_NONE;
    length = yt2_frame_decode_header(frame, size, &header);
    if (length < 1) {
        return 0;
    }
    payload = frame + YT2_FRAME_HEADER_SIZE;
    if (header.family == YT2_FAMILY_INPUT) {
        switch (payload[0]) {
        case YT2_INPUT_KEY:
            if (length < 4) {
                return 0;
            }
            event->kind = YT2_INPUT_EVENT_KEY;
            event->down = payload[1];
            event->scancode = payload[2];
            event->ascii = payload[3];
            return 1;
        case YT2_INPUT_MOUSE_MOVE:
            if (length < 5) {
                return 0;
            }
            event->kind = YT2_INPUT_EVENT_MOUSE_MOVE;
            event->dx = (int16_t)get_le16(payload + 1);
            event->dy = (int16_t)get_le16(payload + 3);
            return 1;
        case YT2_INPUT_MOUSE_BUTTON:
            if (length < 3) {
                return 0;
            }
            event->kind = YT2_INPUT_EVENT_MOUSE_BUTTON;
            event->button = payload[1];
            event->down = payload[2];
            return 1;
        case YT2_INPUT_QUIT:
            if (length >= 5) {
                event->kind = YT2_INPUT_EVENT_MOUSE_ABS;
                event->x = get_le16(payload + 1);
                event->y = get_le16(payload + 3);
                return 1;
            }
            event->kind = YT2_INPUT_EVENT_QUIT;
            return 1;
        default:
            return 0;
        }
    }
    if (header.family == YT2_FAMILY_CONTROL) {
        switch (payload[0]) {
        case YT2_CONTROL_HELLO:
            if (length < 3) {
                return 0;
            }
            event->kind = YT2_INPUT_EVENT_HELLO;
            event->role = payload[1];
            event->version = payload[2];
            return 1;
        case YT2_CONTROL_KEYFRAME_REQUEST:
            event->kind = YT2_INPUT_EVENT_KEYFRAME_REQUEST;
            return 1;
        default:
            return 0;
        }
    }
    return 0;
}
