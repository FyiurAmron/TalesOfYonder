#include "yt2/protocol.h"

#include <string.h>

void yt2_frame_header_write(uint8_t *destination, uint8_t family, uint8_t flags,
                            uint8_t codec, uint32_t sequence, uint32_t length)
{
    yt2_put_le16(destination, YT2_PROTOCOL_MAGIC);
    destination[2] = YT2_PROTOCOL_VERSION;
    destination[3] = family;
    destination[4] = flags;
    destination[5] = codec;
    yt2_put_le32(destination + 6, sequence);
    yt2_put_le32(destination + 10, length);
}

int yt2_frame_header_parse(const uint8_t *source, uint32_t available,
                           yt2_frame_header *header)
{
    if (available < YT2_FRAME_HEADER_SIZE) {
        return YT2_ERR;
    }
    if (yt2_le16(source) != YT2_PROTOCOL_MAGIC ||
        source[2] != YT2_PROTOCOL_VERSION) {
        return YT2_ERR;
    }
    if (source[3] < (uint8_t)YT2_FAMILY_VIDEO ||
        source[3] > (uint8_t)YT2_FAMILY_FILES) {
        return YT2_ERR;
    }
    header->version = source[2];
    header->family = source[3];
    header->flags = source[4];
    header->codec = source[5];
    header->sequence = yt2_le32(source + 6);
    header->length = yt2_le32(source + 10);
    if (header->length > YT2_MAX_PAYLOAD) {
        return YT2_ERR;
    }
    return YT2_OK;
}

int yt2_payload_video_set_mode(uint8_t *destination, uint16_t width,
                               uint16_t height, uint8_t bpp)
{
    destination[0] = (uint8_t)YT2_VIDEO_SET_MODE;
    yt2_put_le16(destination + 1, width);
    yt2_put_le16(destination + 3, height);
    destination[5] = bpp;
    return 6;
}

int yt2_payload_video_palette(uint8_t *destination, uint16_t start,
                              uint16_t count, const uint8_t *dac)
{
    uint32_t bytes;
    uint32_t index;

    if (start > 255u || count > 256u || start + count > 256u) {
        return YT2_ERR;
    }
    bytes = (uint32_t)count * 3u;
    if (5u + bytes > YT2_MAX_PAYLOAD) {
        return YT2_ERR;
    }
    destination[0] = (uint8_t)YT2_VIDEO_PALETTE;
    yt2_put_le16(destination + 1, start);
    yt2_put_le16(destination + 3, count);
    for (index = 0; index < bytes; index++) {
        destination[5u + index] = dac[index];
    }
    return (int)(5u + bytes);
}

int yt2_payload_video_rect(uint8_t *destination, uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           const uint8_t *pixels)
{
    uint32_t count;

    if (width == 0u || height == 0u) {
        return YT2_ERR;
    }
    count = (uint32_t)width * (uint32_t)height;
    if (9u + count > YT2_MAX_PAYLOAD) {
        return YT2_ERR;
    }
    destination[0] = (uint8_t)YT2_VIDEO_RECT;
    yt2_put_le16(destination + 1, x);
    yt2_put_le16(destination + 3, y);
    yt2_put_le16(destination + 5, width);
    yt2_put_le16(destination + 7, height);
    for (uint32_t index = 0; index < count; index++) {
        destination[9u + index] = pixels[index];
    }
    return (int)(9u + count);
}

int yt2_payload_video_frame_end(uint8_t *destination)
{
    destination[0] = (uint8_t)YT2_VIDEO_FRAME_END;
    return 1;
}

int yt2_payload_video_reset(uint8_t *destination)
{
    destination[0] = (uint8_t)YT2_VIDEO_RESET;
    return 1;
}

int yt2_payload_video_scroll(uint8_t *destination, int8_t delta)
{
    destination[0] = (uint8_t)YT2_VIDEO_SCROLL;
    destination[1] = (uint8_t)delta;
    return 2;
}

int yt2_payload_control_hello(uint8_t *destination, uint8_t role, uint8_t version)
{
    destination[0] = (uint8_t)YT2_CONTROL_HELLO;
    destination[1] = role;
    destination[2] = version;
    return 3;
}

int yt2_payload_control_bye(uint8_t *destination)
{
    destination[0] = (uint8_t)YT2_CONTROL_BYE;
    return 1;
}

int yt2_payload_control_error(uint8_t *destination, uint16_t code,
                              const char *text)
{
    size_t length = strlen(text);

    if (length > (size_t)(YT2_MAX_PAYLOAD - 2u)) {
        return YT2_ERR;
    }
    destination[0] = (uint8_t)YT2_CONTROL_ERROR;
    yt2_put_le16(destination + 1, code);
    memcpy(destination + 3, text, length);
    return (int)(3u + length);
}

int yt2_payload_control_debug(uint8_t *destination, uint8_t channel,
                              const uint8_t *data, uint32_t length)
{
    if (length > YT2_MAX_PAYLOAD - 2u) {
        return YT2_ERR;
    }
    destination[0] = (uint8_t)YT2_CONTROL_DEBUG;
    destination[1] = channel;
    for (uint32_t index = 0; index < length; index++) {
        destination[2u + index] = data[index];
    }
    return (int)(2u + length);
}

int yt2_payload_control_keyframe(uint8_t *destination)
{
    destination[0] = (uint8_t)YT2_CONTROL_KEYFRAME_REQUEST;
    return 1;
}

int yt2_payload_input_key(uint8_t *destination, uint8_t down, uint8_t scan,
                          uint8_t ascii)
{
    destination[0] = (uint8_t)YT2_INPUT_KEY;
    destination[1] = down;
    destination[2] = scan;
    destination[3] = ascii;
    return 4;
}

int yt2_payload_input_mouse_move(uint8_t *destination, int16_t dx, int16_t dy)
{
    destination[0] = (uint8_t)YT2_INPUT_MOUSE_MOVE;
    yt2_put_le16(destination + 1, (uint16_t)dx);
    yt2_put_le16(destination + 3, (uint16_t)dy);
    return 5;
}

int yt2_payload_input_mouse_button(uint8_t *destination, uint8_t button,
                                   uint8_t down)
{
    destination[0] = (uint8_t)YT2_INPUT_MOUSE_BUTTON;
    destination[1] = button;
    destination[2] = down;
    return 3;
}

int yt2_payload_input_mouse_abs(uint8_t *destination, uint16_t x, uint16_t y)
{
    destination[0] = (uint8_t)YT2_INPUT_MOUSE_ABS;
    yt2_put_le16(destination + 1, x);
    yt2_put_le16(destination + 3, y);
    return 5;
}

int yt2_payload_input_quit(uint8_t *destination)
{
    destination[0] = (uint8_t)YT2_INPUT_QUIT;
    return 1;
}

int yt2_payload_audio_opl_write(uint8_t *destination, uint16_t port,
                                uint8_t reg, uint8_t value)
{
    destination[0] = (uint8_t)YT2_AUDIO_OPL_WRITE;
    yt2_put_le16(destination + 1, port);
    destination[3] = reg;
    destination[4] = value;
    return 5;
}

int yt2_payload_audio_opl_batch(uint8_t *destination, uint8_t count,
                                const uint8_t *writes)
{
    uint32_t bytes = (uint32_t)count * 4u;

    if (2u + bytes > YT2_MAX_PAYLOAD) {
        return YT2_ERR;
    }
    destination[0] = (uint8_t)YT2_AUDIO_OPL_BATCH;
    destination[1] = count;
    for (uint32_t index = 0; index < bytes; index++) {
        destination[2u + index] = writes[index];
    }
    return (int)(2u + bytes);
}

int yt2_payload_audio_ctrl(uint8_t *destination, uint16_t rate, uint16_t format,
                           uint8_t on)
{
    destination[0] = (uint8_t)YT2_AUDIO_CTRL;
    yt2_put_le16(destination + 1, rate);
    yt2_put_le16(destination + 3, format);
    destination[5] = on;
    return 6;
}

int yt2_payload_audio_opl_batch_timed(uint8_t *destination, uint16_t port,
                                      uint8_t count, const uint8_t *writes)
{
    uint32_t bytes = (uint32_t)count * 6u;

    if (4u + bytes > YT2_MAX_PAYLOAD) {
        return YT2_ERR;
    }
    destination[0] = (uint8_t)YT2_AUDIO_OPL_BATCH_TIMED;
    yt2_put_le16(destination + 1, port);
    destination[3] = count;
    for (uint32_t index = 0; index < bytes; index++) {
        destination[4u + index] = writes[index];
    }
    return (int)(4u + bytes);
}

int yt2_payload_audio_speaker(uint8_t *destination, uint8_t gate,
                              uint16_t divisor, uint32_t pit)
{
    destination[0] = (uint8_t)YT2_AUDIO_SPEAKER;
    destination[1] = gate;
    yt2_put_le16(destination + 2, divisor);
    yt2_put_le32(destination + 4, pit);
    return 8;
}

int yt2_payload_audio_sample(uint8_t *destination, uint16_t id, uint32_t offset,
                             uint32_t pit)
{
    destination[0] = (uint8_t)YT2_AUDIO_SAMPLE;
    yt2_put_le16(destination + 1, id);
    yt2_put_le32(destination + 3, offset);
    yt2_put_le32(destination + 7, pit);
    return 11;
}

void yt2_frame_reader_init(yt2_frame_reader *reader, const uint8_t *data,
                           uint32_t size)
{
    reader->data = data;
    reader->size = size;
    reader->position = 0;
}

int yt2_frame_reader_next(yt2_frame_reader *reader, yt2_frame_view *view)
{
    uint32_t remaining;
    uint32_t length;

    if (reader->position >= reader->size) {
        return 0;
    }
    remaining = reader->size - reader->position;
    if (yt2_frame_header_parse(reader->data + reader->position, remaining,
                              &view->header) != YT2_OK) {
        return YT2_ERR;
    }
    length = view->header.length;
    if (remaining < YT2_FRAME_HEADER_SIZE + length) {
        return YT2_ERR;
    }
    view->payload = reader->data + reader->position + YT2_FRAME_HEADER_SIZE;
    reader->position += YT2_FRAME_HEADER_SIZE + length;
    return 1;
}

void yt2_input_parser_init(yt2_input_parser *parser)
{
    parser->length = 0;
    parser->needed = 0;
}

void yt2_input_parser_reset(yt2_input_parser *parser)
{
    parser->length = 0;
    parser->needed = 0;
}

static void yt2_input_event_clear(yt2_input_event *event)
{
    memset(event, 0, sizeof(*event));
}

static int yt2_input_dispatch(yt2_input_parser *parser, yt2_input_event *event)
{
    const uint8_t *frame = parser->buffer;
    const uint8_t *payload = frame + YT2_FRAME_HEADER_SIZE;
    uint32_t length = yt2_le32(frame + 10);
    yt2_frame_header header;

    if (yt2_frame_header_parse(frame, parser->length, &header) != YT2_OK) {
        yt2_input_parser_reset(parser);
        return YT2_ERR;
    }
    yt2_input_event_clear(event);
    event->family = header.family;
    if (length > 0u) {
        event->type = payload[0];
    }
    if (header.family == (uint8_t)YT2_FAMILY_INPUT) {
        if (length == 0u) {
            event->kind = YT2_INPUT_EVENT_UNKNOWN_MESSAGE;
        } else if (payload[0] == (uint8_t)YT2_INPUT_KEY && length >= 4u) {
            event->kind = YT2_INPUT_EVENT_KEY;
            event->down = payload[1];
            event->scan = payload[2];
            event->ascii = payload[3];
        } else if (payload[0] == (uint8_t)YT2_INPUT_MOUSE_MOVE && length >= 5u) {
            event->kind = YT2_INPUT_EVENT_MOUSE_MOVE;
            event->dx = (int16_t)yt2_le16(payload + 1);
            event->dy = (int16_t)yt2_le16(payload + 3);
        } else if (payload[0] == (uint8_t)YT2_INPUT_MOUSE_BUTTON && length >= 3u) {
            event->kind = YT2_INPUT_EVENT_MOUSE_BUTTON;
            event->button = payload[1];
            event->down = payload[2];
        } else if (payload[0] == YT2_INPUT_MOUSE_ABS && length >= 5u) {
            event->kind = YT2_INPUT_EVENT_MOUSE_ABS;
            event->x = yt2_le16(payload + 1);
            event->y = yt2_le16(payload + 3);
        } else if (payload[0] == (uint8_t)YT2_INPUT_QUIT) {
            event->kind = YT2_INPUT_EVENT_QUIT;
        } else {
            event->kind = YT2_INPUT_EVENT_UNKNOWN_MESSAGE;
        }
    } else if (header.family == (uint8_t)YT2_FAMILY_CONTROL) {
        if (length == 0u) {
            event->kind = YT2_INPUT_EVENT_UNKNOWN_MESSAGE;
        } else if (payload[0] == (uint8_t)YT2_CONTROL_HELLO && length >= 3u) {
            event->kind = YT2_INPUT_EVENT_HELLO;
            event->role = payload[1];
            event->version = payload[2];
        } else if (payload[0] == (uint8_t)YT2_CONTROL_BYE) {
            event->kind = YT2_INPUT_EVENT_BYE;
        } else if (payload[0] == (uint8_t)YT2_CONTROL_ERROR && length >= 3u) {
            event->kind = YT2_INPUT_EVENT_ERROR;
            event->code = yt2_le16(payload + 1);
        } else if (payload[0] == (uint8_t)YT2_CONTROL_DEBUG) {
            event->kind = YT2_INPUT_EVENT_DEBUG;
        } else if (payload[0] == (uint8_t)YT2_CONTROL_KEYFRAME_REQUEST) {
            event->kind = YT2_INPUT_EVENT_KEYFRAME_REQUEST;
        } else {
            event->kind = YT2_INPUT_EVENT_UNKNOWN_MESSAGE;
        }
    } else {
        event->kind = YT2_INPUT_EVENT_UNKNOWN_FAMILY;
    }
    yt2_input_parser_reset(parser);
    return 1;
}

int yt2_input_parser_feed(yt2_input_parser *parser, uint8_t byte,
                          yt2_input_event *event)
{
    if (parser->length == 0u && byte == 'K') {
        yt2_input_event_clear(event);
        event->kind = YT2_INPUT_EVENT_KEYFRAME_REQUEST;
        return 1;
    }
    if (parser->length == 0u && byte != 'T') {
        return 0;
    }
    if (parser->length >= (uint16_t)sizeof(parser->buffer)) {
        yt2_input_parser_reset(parser);
        return YT2_ERR;
    }
    parser->buffer[parser->length] = byte;
    parser->length++;

    if (parser->length < YT2_FRAME_HEADER_SIZE) {
        return 0;
    }
    if (parser->needed == 0u) {
        yt2_frame_header header;

        if (yt2_frame_header_parse(parser->buffer, parser->length, &header) != YT2_OK ||
            header.length > YT2_INPUT_MAX_PAYLOAD) {
            yt2_input_parser_reset(parser);
            return YT2_ERR;
        }
        parser->needed = (uint16_t)(YT2_FRAME_HEADER_SIZE + header.length);
    }
    if (parser->length < parser->needed) {
        return 0;
    }
    return yt2_input_dispatch(parser, event);
}
