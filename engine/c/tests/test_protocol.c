#include "test.h"

#include "yt2/protocol.h"
#include "yt2/transport.h"

#include <string.h>

static uint32_t make_frame(uint8_t *destination, uint8_t family, uint32_t sequence,
                           const uint8_t *payload, uint32_t length)
{
    yt2_frame_header_write(destination, family, 0, 0, sequence, length);
    for (uint32_t index = 0; index < length; index++) {
        destination[YT2_FRAME_HEADER_SIZE + index] = payload[index];
    }
    return YT2_FRAME_HEADER_SIZE + length;
}

static int feed_all(yt2_input_parser *parser, const uint8_t *data, uint32_t length,
                    yt2_input_event *event)
{
    int result = 0;

    for (uint32_t index = 0; index < length; index++) {
        result = yt2_input_parser_feed(parser, data[index], event);
        if (result != 0) {
            return result;
        }
    }
    return result;
}

int test_protocol(void)
{
    uint8_t payload[YT2_MAX_PAYLOAD];
    uint8_t frame[YT2_MAX_FRAME];
    uint8_t a[4] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
    uint8_t dac[6] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u};
    yt2_frame_header header;
    yt2_frame_reader reader;
    yt2_frame_view view;
    yt2_input_parser parser;
    yt2_input_event event;
    uint32_t size;
    int length;

    yt2_frame_header_write(frame, 1, 0, 0, 0x12345678u, 6);
    CHECK(frame[0] == 0x54u && frame[1] == 0x59u);
    CHECK(frame[2] == 0u);
    CHECK(frame[3] == 1u);
    CHECK(frame[4] == 0u && frame[5] == 0u);
    CHECK(frame[6] == 0x78u && frame[9] == 0x12u);
    CHECK(frame[10] == 6u && frame[13] == 0u);
    CHECK(yt2_frame_header_parse(frame, sizeof(frame), &header) == YT2_OK);
    CHECK(header.sequence == 0x12345678u && header.length == 6u);

    {
        uint8_t bin_header[16];

        yt2_transport_bin_header(bin_header, 1);
        CHECK(memcmp(bin_header, "YT2B", 4) == 0);
        CHECK(yt2_le16(bin_header + 4) == 1u);
        CHECK(yt2_le16(bin_header + 6) == 16u);
        CHECK(yt2_le32(bin_header + 8) == YT2_PIT_RATE);
        CHECK(bin_header[12] == 1u);
        CHECK(bin_header[13] == 0u && bin_header[14] == 0u && bin_header[15] == 0u);
    }

    length = yt2_payload_video_set_mode(payload, 320, 200, 8);
    CHECK(length == 6);
    CHECK(payload[0] == 0u && payload[1] == 0x40u && payload[2] == 0x01u);
    CHECK(payload[3] == 0xC8u && payload[4] == 0x00u && payload[5] == 8u);

    length = yt2_payload_video_palette(payload, 2, 2, dac);
    CHECK(length == 11);
    CHECK(payload[0] == 1u && payload[1] == 0x02u && payload[2] == 0x00u);
    CHECK(payload[3] == 0x02u && payload[4] == 0x00u);
    CHECK(memcmp(payload + 5, dac, 6) == 0);

    length = yt2_payload_video_rect(payload, 1, 2, 3, 1, a);
    CHECK(length == 12);
    CHECK(payload[0] == 2u && payload[1] == 1u && payload[2] == 0u);
    CHECK(payload[3] == 2u && payload[4] == 0u);
    CHECK(payload[5] == 3u && payload[6] == 0u);
    CHECK(payload[7] == 1u && payload[8] == 0u);
    CHECK(payload[9] == 0xAAu && payload[10] == 0xBBu && payload[11] == 0xCCu);

    length = yt2_payload_video_frame_end(payload);
    CHECK(length == 1 && payload[0] == 3u);
    length = yt2_payload_video_reset(payload);
    CHECK(length == 1 && payload[0] == 4u);
    length = yt2_payload_video_scroll(payload, -3);
    CHECK(length == 2 && payload[0] == 5u && payload[1] == 0xFDu);

    length = yt2_payload_control_hello(payload, 0, 0);
    CHECK(length == 3 && payload[0] == 0u && payload[1] == 0u && payload[2] == 0u);
    length = yt2_payload_control_error(payload, 2, "bad message");
    CHECK(length == 14 && payload[0] == 2u && payload[1] == 2u && payload[2] == 0u);
    CHECK(memcmp(payload + 3, "bad message", 11) == 0);
    length = yt2_payload_control_debug(payload, 'x', a, 4);
    CHECK(length == 6 && payload[0] == 3u && payload[1] == 'x');
    CHECK(memcmp(payload + 2, a, 4) == 0);
    length = yt2_payload_control_keyframe(payload);
    CHECK(length == 1 && payload[0] == 4u);

    length = yt2_payload_input_key(payload, 1, 0x1Eu, 0x61u);
    CHECK(length == 4 && payload[0] == 0u && payload[1] == 1u);
    CHECK(payload[2] == 0x1Eu && payload[3] == 0x61u);
    length = yt2_payload_input_mouse_move(payload, -5, 2);
    CHECK(length == 5 && payload[0] == 1u && payload[1] == 0xFBu);
    CHECK(payload[2] == 0xFFu && payload[3] == 0x02u && payload[4] == 0u);
    length = yt2_payload_input_mouse_button(payload, 0, 1);
    CHECK(length == 3 && payload[0] == 2u && payload[1] == 0u && payload[2] == 1u);
    length = yt2_payload_input_mouse_abs(payload, 100, 50);
    CHECK(length == 5 && payload[0] == 3u && payload[1] == 100u);
    CHECK(payload[3] == 50u && payload[4] == 0u);
    length = yt2_payload_input_quit(payload);
    CHECK(length == 1 && payload[0] == 3u);

    length = yt2_payload_audio_opl_write(payload, 0x228u, 1, 2);
    CHECK(length == 5 && payload[0] == 0u && payload[1] == 0x28u);
    CHECK(payload[2] == 0x02u && payload[3] == 1u && payload[4] == 2u);
    {
        uint8_t writes[4] = {0x28u, 0x02u, 0x01u, 0x02u};

        length = yt2_payload_audio_opl_batch(payload, 1, writes);
        CHECK(length == 6 && payload[0] == 1u && payload[1] == 1u);
        CHECK(memcmp(payload + 2, writes, 4) == 0);
    }
    {
        uint8_t writes[6] = {0x78u, 0x56u, 0x34u, 0x12u, 0x01u, 0x02u};

        length = yt2_payload_audio_opl_batch_timed(payload, 0x228u, 1, writes);
        CHECK(length == 10 && payload[0] == 3u && payload[1] == 0x28u);
        CHECK(payload[2] == 0x02u && payload[3] == 1u);
        CHECK(memcmp(payload + 4, writes, 6) == 0);
    }
    length = yt2_payload_audio_ctrl(payload, 44100, 0, 1);
    CHECK(length == 6 && payload[0] == 2u && payload[1] == 0x44u);
    CHECK(payload[2] == 0xACu && payload[5] == 1u);
    length = yt2_payload_audio_speaker(payload, 1, 662, 0x12345678u);
    CHECK(length == 8 && payload[0] == 4u && payload[1] == 1u);
    CHECK(payload[2] == 0x96u && payload[3] == 0x02u);
    CHECK(payload[4] == 0x78u && payload[7] == 0x12u);
    length = yt2_payload_audio_sample(payload, 16, 0x09FFE6u, 0x12345678u);
    CHECK(length == 11 && payload[0] == 5u && payload[1] == 16u);
    CHECK(payload[3] == 0xE6u && payload[5] == 0x09u);
    CHECK(payload[7] == 0x78u && payload[10] == 0x12u);

    /* Frame reader: two records, then clean end. */
    memcpy(payload, a, 4);
    size = make_frame(frame, 1, 7, payload, 4);
    CHECK(size == 18);
    yt2_frame_reader_init(&reader, frame, size);
    CHECK(yt2_frame_reader_next(&reader, &view) == 1);
    CHECK(view.header.family == 1u && view.header.sequence == 7u);
    CHECK(view.header.length == 4u && view.payload[0] == 0xAAu);
    CHECK(yt2_frame_reader_next(&reader, &view) == 0);
    yt2_frame_reader_init(&reader, frame, size - 1);
    CHECK(yt2_frame_reader_next(&reader, &view) == YT2_ERR);

    /* The incremental parser accepts a frame split into single bytes. */
    yt2_input_parser_init(&parser);
    size = make_frame(frame, 3, 0, NULL, 0);
    CHECK(size == 14);
    for (uint32_t index = 0; index < size; index++) {
        int result = yt2_input_parser_feed(&parser, frame[index], &event);

        CHECK(result == (index + 1u == size ? 1 : 0));
    }
    CHECK(event.kind == YT2_INPUT_EVENT_UNKNOWN_MESSAGE);

    {
        uint8_t key[4] = {0u, 1u, 0x1Eu, 0x61u};

        size = make_frame(frame, 3, 1, key, 4);
        yt2_input_parser_init(&parser);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_KEY);
        CHECK(event.down == 1u && event.scan == 0x1Eu && event.ascii == 0x61u);
    }
    {
        uint8_t move[5] = {1u, 0xFBu, 0xFFu, 0x02u, 0x00u};

        size = make_frame(frame, 3, 2, move, 5);
        yt2_input_parser_init(&parser);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_MOUSE_MOVE);
        CHECK(event.dx == -5 && event.dy == 2);
    }
    {
        uint8_t press[3] = {2u, 2u, 1u};

        size = make_frame(frame, 3, 3, press, 3);
        yt2_input_parser_init(&parser);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_MOUSE_BUTTON);
        CHECK(event.button == 2u && event.down == 1u);
    }
    {
        uint8_t absolute[5] = {3u, 100u, 0u, 50u, 0u};

        size = make_frame(frame, 3, 4, absolute, 5);
        yt2_input_parser_init(&parser);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_MOUSE_ABS);
        CHECK(event.x == 100u && event.y == 50u);
    }
    {
        uint8_t hello[3] = {0u, 1u, 0u};

        size = make_frame(frame, 5, 5, hello, 3);
        yt2_input_parser_init(&parser);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_HELLO);
        CHECK(event.role == 1u && event.version == 0u);
    }
    {
        uint8_t request[1] = {4u};

        size = make_frame(frame, 5, 6, request, 1);
        yt2_input_parser_init(&parser);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_KEYFRAME_REQUEST);
    }
    {
        uint8_t tick[1] = {0u};

        size = make_frame(frame, 4, 7, tick, 1);
        yt2_input_parser_init(&parser);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_UNKNOWN_FAMILY);
        CHECK(event.family == 4u);
    }
    yt2_input_parser_init(&parser);
    CHECK(yt2_input_parser_feed(&parser, 'K', &event) == 1);
    CHECK(event.kind == YT2_INPUT_EVENT_KEYFRAME_REQUEST);
    CHECK(yt2_input_parser_feed(&parser, 'K', &event) == 1);

    /* Noise before the magic is dropped; the next frame still parses. */
    yt2_input_parser_init(&parser);
    CHECK(yt2_input_parser_feed(&parser, 'x', &event) == 0);
    {
        uint8_t quit[1] = {3u};

        size = make_frame(frame, 3, 8, quit, 1);
        CHECK(feed_all(&parser, frame, size, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_QUIT);
    }
    return 0;
}
