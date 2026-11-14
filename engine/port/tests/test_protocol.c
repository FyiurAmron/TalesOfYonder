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
    CHECK(yt2_video_encode_scroll(payload, sizeof(payload), 3) == 2);
    {
        static const uint8_t expected[2] = { 0x05, 0x03 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_video_encode_scroll(payload, sizeof(payload), -5) == 2);
    {
        static const uint8_t expected[2] = { 0x05, 0xFB };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }

    /* Codec-1 row runs, hand-computed from the spec. */
    {
        static const uint8_t flat[8] = { 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u };
        static const uint8_t expected[13] = { 0x02, 0x01, 0x00, 0x02, 0x00,
                                              0x08, 0x00, 0x01, 0x00, 0x01,
                                              0x08, 0x07, 0x03 };

        CHECK(yt2_video_encode_rect_runs(payload, sizeof(payload), 1u, 2u, 8u,
                                         1u, flat) == 13);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        /* One- and two-pixel runs merge into literals. */
        static const uint8_t pixels[4] = { 1u, 2u, 2u, 3u };
        static const uint8_t expected[16] = { 0x02, 0x00, 0x00, 0x00, 0x00,
                                              0x04, 0x00, 0x01, 0x00, 0x02,
                                              0x04, 0x01, 0x02, 0x02, 0x03,
                                              0x03 };

        CHECK(yt2_video_encode_rect_runs(payload, sizeof(payload), 0u, 0u, 4u,
                                         1u, pixels) == 16);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        /* A run followed by a lone pixel: 01, then a 1-wide literal. */
        static const uint8_t pixels[4] = { 5u, 5u, 5u, 9u };
        static const uint8_t expected[16] = { 0x02, 0x00, 0x00, 0x00, 0x00,
                                              0x04, 0x00, 0x01, 0x00, 0x01,
                                              0x03, 0x05, 0x02, 0x01, 0x09,
                                              0x03 };

        CHECK(yt2_video_encode_rect_runs(payload, sizeof(payload), 0u, 0u, 4u,
                                         1u, pixels) == 16);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        /* Rows encode independently: no 00 carries into the second row. */
        static const uint8_t pixels[6] = { 7u, 7u, 7u, 7u, 7u, 7u };
        static const uint8_t expected[17] = { 0x02, 0x00, 0x00, 0x00, 0x00,
                                              0x03, 0x00, 0x02, 0x00, 0x01,
                                              0x03, 0x07, 0x03, 0x01, 0x03,
                                              0x07, 0x03 };

        CHECK(yt2_video_encode_rect_runs(payload, sizeof(payload), 0u, 0u, 3u,
                                         2u, pixels) == 17);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        /* A 300-run splits at 255; the tail repeats via 00. */
        static uint8_t row[300];
        static const uint8_t expected[15] = { 0x02, 0x00, 0x00, 0x00, 0x00,
                                              0x2C, 0x01, 0x01, 0x00, 0x01,
                                              0xFF, 0x09, 0x00, 0x2D, 0x03 };

        memset(row, 9u, sizeof(row));
        CHECK(yt2_video_encode_rect_runs(payload, sizeof(payload), 0u, 0u,
                                         300u, 1u, row) == 15);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        /* Worst case: 640 noise pixels ship as 255+255+130 literals. */
        static uint8_t row[640];
        uint8_t wide[1024];

        for (unsigned int i = 0u; i < 640u; i++) {
            row[i] = (uint8_t)(i & 0xFFu);
        }
        CHECK(yt2_video_encode_rect_runs(wide, sizeof(wide), 0u, 0u, 640u, 1u,
                                         row) == 656);
        CHECK(wide[9] == 0x02 && wide[10] == 0xFF);
        CHECK(656u <= YT2_PROTOCOL_MAX_PAYLOAD);
    }

    /*
     * Codec-1 differential: the B1 seam bytes from
     * tests/data/sample-video-rle.bin (pixels re-decoded with
     * tools/stream/replay.py, bytes pinned verbatim).
     */
    {
        /* seq 197: flat 320x0x00. */
        static uint8_t row[320];
        static const uint8_t expected[15] = { 0x02, 0x00, 0x00, 0x00, 0x00,
                                              0x40, 0x01, 0x01, 0x00, 0x01,
                                              0xFF, 0x00, 0x00, 0x41, 0x03 };

        memset(row, 0u, sizeof(row));
        CHECK(yt2_video_encode_rect_runs(payload, sizeof(payload), 0u, 0u,
                                         320u, 1u, row) == 15);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        /* seq 139: 194x00, 276x13, 170x00. */
        static uint8_t row[640];
        static const uint8_t expected[21] = { 0x02, 0x00, 0x00, 0x1E, 0x01,
                                              0x80, 0x02, 0x01, 0x00, 0x01,
                                              0xC2, 0x00, 0x01, 0xFF, 0x13,
                                              0x00, 0x15, 0x01, 0xAA, 0x00,
                                              0x03 };
        uint8_t wide[1024];

        memset(row, 0u, 194u);
        memset(row + 194u, 0x13u, 276u);
        memset(row + 470u, 0u, 170u);
        CHECK(yt2_video_encode_rect_runs(wide, sizeof(wide), 0u, 286u, 640u,
                                         1u, row) == 21);
        CHECK(memcmp(wide, expected, sizeof(expected)) == 0);
    }
    {
        /* seq 138: 273x00, 8xa3, 1xa4, 358x00. */
        static uint8_t row[640];
        static const uint8_t expected[26] = { 0x02, 0x00, 0x00, 0xCE, 0x00,
                                              0x80, 0x02, 0x01, 0x00, 0x01,
                                              0xFF, 0x00, 0x00, 0x12, 0x01,
                                              0x08, 0xA3, 0x02, 0x01, 0xA4,
                                              0x01, 0xFF, 0x00, 0x00, 0x67,
                                              0x03 };
        uint8_t wide[1024];

        memset(row, 0u, 273u);
        memset(row + 273u, 0xA3u, 8u);
        row[281u] = 0xA4u;
        memset(row + 282u, 0u, 358u);
        CHECK(yt2_video_encode_rect_runs(wide, sizeof(wide), 0u, 206u, 640u,
                                         1u, row) == 26);
        CHECK(memcmp(wide, expected, sizeof(expected)) == 0);
    }

    /* AUDIO payloads, hand-computed from the spec. */
    CHECK(yt2_audio_encode_opl_write(payload, sizeof(payload), 0x228u, 0x20u,
                                     0x01u) == 5);
    {
        static const uint8_t expected[5] = { 0x00, 0x28, 0x02, 0x20, 0x01 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        static const yt2_opl_write writes[2] = { { 0x228u, 0x20u, 0x01u },
                                                 { 0x228u, 0x40u, 0x3Fu } };
        static const uint8_t expected[10] = { 0x01, 0x02, 0x28, 0x02, 0x20,
                                              0x01, 0x28, 0x02, 0x40, 0x3F };

        CHECK(yt2_audio_encode_opl_batch(payload, sizeof(payload), 2u, writes) ==
              10);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_audio_encode_ctrl(payload, sizeof(payload), 22050u, 1u, 1u) == 6);
    {
        static const uint8_t expected[6] = { 0x02, 0x22, 0x56, 0x01, 0x00,
                                             0x01 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    {
        static const yt2_opl_timed_write writes[1] = { { 0x12345678u, 0xB0u,
                                                         0x2Au } };
        static const uint8_t expected[10] = { 0x03, 0x28, 0x02, 0x01, 0x78,
                                              0x56, 0x34, 0x12, 0xB0, 0x2A };

        CHECK(yt2_audio_encode_opl_batch_timed(payload, sizeof(payload),
                                               0x228u, 1u, writes) == 10);
        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_audio_encode_speaker(payload, sizeof(payload), 1u, 662u,
                                   0x01020304u) == 8);
    {
        static const uint8_t expected[8] = { 0x04, 0x01, 0x96, 0x02, 0x04,
                                             0x03, 0x02, 0x01 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_audio_encode_sample(payload, sizeof(payload), 3u, 0x09FFE6u,
                                  0x11223344u) == 11);
    {
        static const uint8_t expected[11] = { 0x05, 0x03, 0x00, 0xE6, 0xFF,
                                              0x09, 0x00, 0x44, 0x33, 0x22,
                                              0x11 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_control_encode_hello(payload, sizeof(payload), 0u, 0u) == 3);
    CHECK(payload[0] == 0x00 && payload[1] == 0x00 && payload[2] == 0x00);

    /* Bounds. */
    CHECK(yt2_video_encode_set_mode(payload, 5u, 320u, 200u, 8u) == -1);
    CHECK(yt2_video_encode_palette(payload, sizeof(payload), 255u, 2u,
                                   payload) == -1);
    CHECK(yt2_video_encode_rect(payload, 12u, 0u, 0u, 2u, 2u, payload) == -1);
    CHECK(yt2_video_encode_frame_end(payload, 0u) == -1);
    CHECK(yt2_video_encode_scroll(payload, 1u, 3) == -1);
    CHECK(yt2_video_encode_scroll(payload, sizeof(payload), 128) == -1);
    CHECK(yt2_video_encode_scroll(payload, sizeof(payload), -129) == -1);
    CHECK(yt2_video_encode_scroll(NULL, sizeof(payload), 3) == -1);
    CHECK(yt2_tick_encode_tick(payload, sizeof(payload), 0x12345678u) == 5);
    {
        static const uint8_t expected[5] = { 0x00, 0x78, 0x56, 0x34, 0x12 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_tick_encode_pace(payload, sizeof(payload), 55u) == 3);
    {
        static const uint8_t expected[3] = { 0x01, 0x37, 0x00 };

        CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    }
    CHECK(yt2_tick_encode_tick(payload, 4u, 0u) == -1);
    CHECK(yt2_tick_encode_tick(NULL, sizeof(payload), 0u) == -1);
    CHECK(yt2_tick_encode_pace(payload, 2u, 55u) == -1);
    CHECK(yt2_tick_encode_pace(payload, sizeof(payload), 0x10000u) == -1);
    CHECK(yt2_tick_encode_pace(NULL, sizeof(payload), 55u) == -1);
    {
        static const uint8_t pixels[4] = { 1u, 2u, 3u, 4u };
        static const yt2_opl_write writes[1] = { { 0x228u, 0x20u, 0x01u } };
        static const yt2_opl_timed_write timed[1] = { { 1u, 0x20u, 0x01u } };

        CHECK(yt2_video_encode_rect_runs(payload, 12u, 0u, 0u, 2u, 2u,
                                         pixels) == -1);
        CHECK(yt2_video_encode_rect_runs(payload, sizeof(payload), 0u, 0u, 0u,
                                         1u, pixels) == -1);
        CHECK(yt2_video_encode_rect_runs(NULL, sizeof(payload), 0u, 0u, 2u,
                                         1u, pixels) == -1);
        CHECK(yt2_audio_encode_opl_write(payload, 4u, 0x228u, 0x20u, 0x01u) ==
              -1);
        CHECK(yt2_audio_encode_opl_batch(payload, sizeof(payload), 0u, writes) ==
              -1);
        CHECK(yt2_audio_encode_opl_batch(payload, sizeof(payload),
                                         YT2_AUDIO_BATCH_MAX + 1u,
                                         writes) == -1);
        CHECK(yt2_audio_encode_ctrl(payload, 5u, 22050u, 1u, 1u) == -1);
        CHECK(yt2_audio_encode_opl_batch_timed(payload, sizeof(payload),
                                               0x228u, 0u, timed) == -1);
        CHECK(yt2_audio_encode_speaker(payload, 7u, 1u, 662u, 0u) == -1);
        CHECK(yt2_audio_encode_sample(payload, 10u, 3u, 0u, 0u) == -1);
    }

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
