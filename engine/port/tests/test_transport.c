#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt2/protocol.h"
#include "yt2/status.h"
#include "yt2/transport.h"

static void make_frame(uint8_t *frame, uint8_t family, uint8_t type,
                       uint32_t *seq, uint32_t *length)
{
    frame[14] = type;
    yt2_frame_encode_header(frame, family, 0u, YT2_CODEC_RAW, (*seq)++, 1u);
    *length = YT2_FRAME_HEADER_SIZE + 1u;
}

int test_transport(void)
{
    const char *tmp = getenv("YT2_PORT_TMP");
    yt2_folder_out out;
    yt2_folder_in in;
    uint8_t frame[64];
    uint32_t seq = 0u;
    uint32_t length = 0u;
    const uint8_t *got;
    uint32_t got_length = 0u;

    CHECK(yt2_folder_out_open(NULL, ".") == YT2_STATUS_ERROR);
    CHECK(yt2_folder_out_open(&out, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_folder_in_open(NULL, ".") == YT2_STATUS_ERROR);

    if (tmp == NULL || tmp[0] == '\0') {
        return 0;
    }

    /* OUTPUT.BIN carries direction 0 and timed records. */
    CHECK(yt2_folder_out_open(&out, tmp) == YT2_STATUS_OK);
    {
        FILE *check;
        char path[2048];
        uint8_t header[16];

        snprintf(path, sizeof(path), "%s/OUTPUT.BIN", tmp);
        check = fopen(path, "rb");
        CHECK(check != NULL);
        if (check != NULL) {
            CHECK(fread(header, 1, sizeof(header), check) == sizeof(header));
            CHECK(header[0] == 'Y' && header[1] == 'T' && header[2] == '2' &&
                  header[3] == 'B' && header[4] == 1u && header[6] == 16u &&
                  header[8] == 0xDEu && header[9] == 0x34u &&
                  header[10] == 0x12u && header[11] == 0x00u &&
                  header[12] == 0u && header[13] == 0u && header[14] == 0u &&
                  header[15] == 0u);
            fclose(check);
        }
    }
    make_frame(frame, YT2_FAMILY_VIDEO, YT2_VIDEO_FRAME_END, &seq, &length);
    CHECK(yt2_folder_out_write(&out, frame, length) == YT2_STATUS_OK);
    CHECK(yt2_folder_out_write(&out, frame, 3u) == YT2_STATUS_ERROR);
    CHECK(yt2_folder_out_commit(&out) == YT2_STATUS_OK);
    CHECK(yt2_folder_out_take_wrapped(&out) == 0);
    yt2_folder_out_close(&out);

    /* A hand-written INPUT.BIN (direction 1) unwraps frame by frame,
       including content written before the attach. */
    {
        char path[2048];
        FILE *input;
        uint8_t header[16] = { 'Y', 'T', '2', 'B', 1u, 0u, 16u, 0u,
                               0xDEu, 0x34u, 0x12u, 0x00u, 1u, 0u, 0u, 0u };
        uint8_t record[8];
        uint8_t key[14 + 4];

        snprintf(path, sizeof(path), "%s/INPUT.BIN", tmp);
        input = fopen(path, "wb");
        CHECK(input != NULL);
        if (input == NULL) {
            return 0;
        }
        CHECK(fwrite(header, 1, sizeof(header), input) == sizeof(header));
        /* Three records in one batch: key '1' down, mouse_abs, quit. */
        key[14] = YT2_INPUT_KEY;
        key[15] = 1u;
        key[16] = 0x02u;
        key[17] = 0x31u;
        yt2_frame_encode_header(key, YT2_FAMILY_INPUT, 0u, YT2_CODEC_RAW, 0u,
                                4u);
        memset(record, 0, 4);
        record[4] = (uint8_t)sizeof(key);
        record[5] = 0u;
        record[6] = 0u;
        record[7] = 0u;
        CHECK(fwrite(record, 1, sizeof(record), input) == sizeof(record));
        CHECK(fwrite(key, 1, sizeof(key), input) == sizeof(key));
        {
            uint8_t absolute[14 + 5];

            absolute[14] = YT2_INPUT_QUIT;
            absolute[15] = 0xA0u;
            absolute[16] = 0x00u;
            absolute[17] = 0x64u;
            absolute[18] = 0x00u;
            yt2_frame_encode_header(absolute, YT2_FAMILY_INPUT, 0u, YT2_CODEC_RAW,
                                    1u, 5u);
            record[4] = (uint8_t)sizeof(absolute);
            CHECK(fwrite(record, 1, sizeof(record), input) == sizeof(record));
            CHECK(fwrite(absolute, 1, sizeof(absolute), input) == sizeof(absolute));
        }
        key[14] = YT2_INPUT_QUIT;
        yt2_frame_encode_header(key, YT2_FAMILY_INPUT, 0u, YT2_CODEC_RAW, 2u,
                                1u);
        record[4] = (uint8_t)(YT2_FRAME_HEADER_SIZE + 1u);
        CHECK(fwrite(record, 1, sizeof(record), input) == sizeof(record));
        CHECK(fwrite(key, 1, YT2_FRAME_HEADER_SIZE + 1u, input) ==
              YT2_FRAME_HEADER_SIZE + 1u);
        fclose(input);
    }
    CHECK(yt2_folder_in_open(&in, tmp) == YT2_STATUS_OK);
    CHECK(yt2_folder_in_poll(&in, &got, &got_length) == 0);
    CHECK(yt2_folder_in_poll(&in, &got, &got_length) == 1);
    CHECK(got_length == YT2_FRAME_HEADER_SIZE + 4u);
    CHECK(got[14] == YT2_INPUT_KEY && got[17] == 0x31u);
    {
        yt2_input_event event;

        CHECK(yt2_input_decode_frame(got, got_length, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_KEY && event.down == 1 &&
              event.scancode == 0x02u && event.ascii == 0x31u);
    }
    CHECK(yt2_folder_in_poll(&in, &got, &got_length) == 1);
    CHECK(got_length == YT2_FRAME_HEADER_SIZE + 5u);
    {
        yt2_input_event event;

        CHECK(yt2_input_decode_frame(got, got_length, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_MOUSE_ABS && event.x == 160u &&
              event.y == 100u);
    }
    CHECK(yt2_folder_in_poll(&in, &got, &got_length) == 1);
    CHECK(got_length == YT2_FRAME_HEADER_SIZE + 1u);
    {
        yt2_input_event event;

        CHECK(yt2_input_decode_frame(got, got_length, &event) == 1);
        CHECK(event.kind == YT2_INPUT_EVENT_QUIT);
    }
    CHECK(yt2_folder_in_poll(&in, &got, &got_length) == 0);
    yt2_folder_in_close(&in);
    return 0;
}
