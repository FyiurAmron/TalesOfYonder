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
