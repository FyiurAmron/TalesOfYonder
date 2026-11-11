#include "test.h"

#include <string.h>

#include "yt2/protocol.h"

static void build_frame(uint8_t *frame, uint8_t family, uint8_t type,
                        const uint8_t *body, size_t body_length,
                        uint32_t *seq)
{
    frame[14] = type;
    if (body_length > 0u) {
        memcpy(frame + 15, body, body_length);
    }
    yt2_frame_encode_header(frame, family, 0u, YT2_CODEC_RAW, (*seq)++,
                            (uint32_t)(1u + body_length));
}

int test_protocol(void)
{
    uint8_t frame[128];
    uint8_t payload[64];
    yt2_frame_header header;
    yt2_input_event event;
    uint32_t seq = 0u;

    /* v0 header bytes, pinned exactly (magic 54 59 on the wire). */
    yt2_frame_encode_header(frame, YT2_FAMILY_VIDEO, 0u, YT2_CODEC_RAW, 7u,
                            9u);
    {
        static const uint8_t expected[14] = {
            0x54, 0x59, 0x00, 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x09,
            0x00, 0x00, 0x00
        };

        CHECK(memcmp(frame, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_frame_decode_header(frame, sizeof(frame), &header) == 9);
    CHECK(header.family == YT2_FAMILY_VIDEO && header.seq == 7u &&
          header.length == 9u);
    frame[0] = 0x00;
    CHECK(yt2_frame_decode_header(frame, sizeof(frame), &header) == -1);

    /* VIDEO payloads. */
    CHECK(yt2_video_encode_set_mode(payload, sizeof(payload), 320u, 200u,
                                    8u) == 6);
    {
        static const uint8_t expected[6] = { 0x00, 0x40, 0x01, 0xC8, 0x00,
                                             0x08 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        static const uint8_t rgb[6] = { 0x3F, 0x00, 0x1B, 0x10, 0x20, 0x30 };

        CHECK(yt2_video_encode_palette(payload, sizeof(payload), 223u, 2u,
                                       rgb) == 11);
        CHECK(payload[0] == 0x01 && payload[1] == 0xDF && payload[2] == 0x00 &&
              payload[3] == 0x02 && payload[4] == 0x00 &&
              memcmp(payload + 5, rgb, sizeof(rgb)) == 0);
    }
    {
        static const uint8_t pixels[4] = { 1u, 2u, 3u, 4u };

        CHECK(yt2_video_encode_rect(payload, sizeof(payload), 7u, 9u, 2u, 2u,
                                    pixels) == 13);
        CHECK(payload[0] == 0x02 && payload[1] == 0x07 && payload[2] == 0x00 &&
              payload[3] == 0x09 && payload[4] == 0x00 && payload[5] == 0x02 &&
              payload[6] == 0x00 && payload[7] == 0x02 && payload[8] == 0x00 &&
              memcmp(payload + 9, pixels, sizeof(pixels)) == 0);
    }
    CHECK(yt2_video_encode_frame_end(payload, sizeof(payload)) == 1);
    CHECK(payload[0] == YT2_VIDEO_FRAME_END);
    CHECK(yt2_control_encode_hello(payload, sizeof(payload), 0u, 0u) == 3);
    CHECK(payload[0] == 0x00 && payload[1] == 0x00 && payload[2] == 0x00);

    /* Bounds. */
    CHECK(yt2_video_encode_set_mode(payload, 5u, 320u, 200u, 8u) == -1);
    CHECK(yt2_video_encode_palette(payload, sizeof(payload), 255u, 2u,
                                   payload) == -1);
    CHECK(yt2_video_encode_rect(payload, 12u, 0u, 0u, 2u, 2u, payload) == -1);
    CHECK(yt2_video_encode_frame_end(payload, 0u) == -1);

    /* INPUT decoding. */
    {
        static const uint8_t body[3] = { 0x01, 0x02, 0x31 };

        build_frame(frame, YT2_FAMILY_INPUT, YT2_INPUT_KEY, body, sizeof(body),
                    &seq);
        CHECK(yt2_input_decode_frame(frame, 14u + 4u, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_KEY && event.down == 1 &&
              event.scancode == 0x02 && event.ascii == 0x31);
    }
    {
        static const uint8_t body[4] = { 0xFE, 0xFF, 0x05, 0x00 };

        build_frame(frame, YT2_FAMILY_INPUT, YT2_INPUT_MOUSE_MOVE, body,
                    sizeof(body), &seq);
        CHECK(yt2_input_decode_frame(frame, 14u + 5u, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_MOUSE_MOVE && event.dx == -2 &&
              event.dy == 5);
    }
    {
        static const uint8_t body[2] = { 0x00, 0x01 };

        build_frame(frame, YT2_FAMILY_INPUT, YT2_INPUT_MOUSE_BUTTON, body,
                    sizeof(body), &seq);
        CHECK(yt2_input_decode_frame(frame, 14u + 3u, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_MOUSE_BUTTON &&
              event.button == 0 && event.down == 1);
    }
    build_frame(frame, YT2_FAMILY_INPUT, YT2_INPUT_QUIT, NULL, 0u, &seq);
    CHECK(yt2_input_decode_frame(frame, 14u + 1u, &event) == 1);
    CHECK(event.kind == YT2_INPUT_EVENT_QUIT);
    {
        static const uint8_t body[4] = { 0xA0, 0x00, 0x64, 0x00 };

        build_frame(frame, YT2_FAMILY_INPUT, YT2_INPUT_QUIT, body,
                    sizeof(body), &seq);
        CHECK(yt2_input_decode_frame(frame, 14u + 5u, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_MOUSE_ABS && event.x == 160 &&
              event.y == 100);
    }

    /* CONTROL decoding. */
    {
        static const uint8_t body[2] = { 0x01, 0x00 };

        build_frame(frame, YT2_FAMILY_CONTROL, YT2_CONTROL_HELLO, body,
                    sizeof(body), &seq);
        CHECK(yt2_input_decode_frame(frame, 14u + 3u, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_HELLO && event.role == 1 &&
              event.version == 0);
    }
    build_frame(frame, YT2_FAMILY_CONTROL, YT2_CONTROL_KEYFRAME_REQUEST, NULL,
                0u, &seq);
    CHECK(yt2_input_decode_frame(frame, 14u + 1u, &event) == 1);
    CHECK(event.kind == YT2_INPUT_EVENT_KEYFRAME_REQUEST);

    /* Anything else is well-framed but ignored. */
    build_frame(frame, YT2_FAMILY_AUDIO, 0u, NULL, 0u, &seq);
    CHECK(yt2_input_decode_frame(frame, 14u + 1u, &event) == 0);
    CHECK(event.kind == YT2_INPUT_EVENT_NONE);
    build_frame(frame, YT2_FAMILY_INPUT, 9u, NULL, 0u, &seq);
    CHECK(yt2_input_decode_frame(frame, 14u + 1u, &event) == 0);
    {
        static const uint8_t body[1] = { 0x01 };

        build_frame(frame, YT2_FAMILY_INPUT, YT2_INPUT_KEY, body, sizeof(body),
                    &seq);
        CHECK(yt2_input_decode_frame(frame, 14u + 2u, &event) == 0);
    }
    CHECK(yt2_input_decode_frame(frame, 10u, &event) == 0);
    CHECK(yt2_input_decode_frame(NULL, 100u, &event) == 0);
    return 0;
}
