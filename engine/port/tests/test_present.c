#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/palette.h"
#include "yt2/present.h"
#include "yt2/protocol.h"
#include "yt2/status.h"
#include "yt2/surface.h"

static int read_frames(const char *tmp, uint8_t *types, uint16_t *rows,
                       size_t capacity, size_t *count)
{
    char path[2048];
    FILE *file;
    uint8_t header[16];
    size_t n = 0;

    snprintf(path, sizeof(path), "%s/OUTPUT.BIN", tmp);
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return 0;
    }
    for (;;) {
        uint8_t record[8];
        uint8_t frame[16 + 9];
        uint32_t length;
        uint32_t head;

        if (fread(record, 1, sizeof(record), file) != sizeof(record)) {
            break;
        }
        length = (uint32_t)record[4] | ((uint32_t)record[5] << 8) |
                 ((uint32_t)record[6] << 16) | ((uint32_t)record[7] << 24);
        if (length < YT2_FRAME_HEADER_SIZE + 1u) {
            fclose(file);
            return 0;
        }
        head = YT2_FRAME_HEADER_SIZE + 9u;
        if (head > length) {
            head = length;
        }
        if (fread(frame, 1, head, file) != head) {
            fclose(file);
            return 0;
        }
        if (n >= capacity) {
            fclose(file);
            return 0;
        }
        types[n] = frame[YT2_FRAME_HEADER_SIZE];
        rows[n] = 0u;
        if (types[n] == YT2_VIDEO_RECT && length >= YT2_FRAME_HEADER_SIZE + 9u) {
            rows[n] = (uint16_t)(frame[YT2_FRAME_HEADER_SIZE + 3] |
                                 ((uint16_t)frame[YT2_FRAME_HEADER_SIZE + 4]
                                  << 8));
        }
        n++;
        if (fseek(file, (long)(length - head), SEEK_CUR) != 0) {
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    *count = n;
    return 1;
}

int test_present(void)
{
    const char *tmp = getenv("YT2_PORT_TMP");
    yt2_folder_out out;
    yt2_presenter presenter;
    yt2_surface surface;
    yt2_palette palette;
    uint8_t types[24];
    uint16_t rows[24];
    size_t count = 0u;

    CHECK(yt2_presenter_init(NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_present_frame(NULL, NULL, NULL, NULL, 0) == YT2_STATUS_ERROR);

    if (tmp == NULL || tmp[0] == '\0') {
        return 0;
    }
    CHECK(yt2_surface_init(&surface, 32u, 2u) == YT2_STATUS_OK);
    memset(surface.pixels, 7u, 32u * 2u);
    memset(palette.rgb, 0x11u, sizeof(palette.rgb));
    CHECK(yt2_presenter_init(&presenter) == YT2_STATUS_OK);
    CHECK(yt2_folder_out_open(&out, tmp) == YT2_STATUS_OK);

    /* First present: set_mode, full palette, every row, end. */
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, 24u, &count));
    CHECK(count == 5u);
    CHECK(types[0] == YT2_VIDEO_SET_MODE && types[1] == YT2_VIDEO_PALETTE &&
          types[2] == YT2_VIDEO_RECT && types[3] == YT2_VIDEO_RECT &&
          types[4] == YT2_VIDEO_FRAME_END);
    CHECK(rows[2] == 0u && rows[3] == 1u);

    /* Unchanged: only frame_end goes out. */
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, 24u, &count));
    CHECK(count == 6u);
    CHECK(types[5] == YT2_VIDEO_FRAME_END);

    /* One palette entry: the span closes with frame_end, no rects. */
    palette.rgb[100 * 3u] = 0x22u;
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, 24u, &count));
    CHECK(count == 8u);
    CHECK(types[6] == YT2_VIDEO_PALETTE && types[7] == YT2_VIDEO_FRAME_END);

    /* One changed row: exactly that row travels. */
    surface.pixels[32u] = 9u;
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, 24u, &count));
    CHECK(count == 10u);
    CHECK(types[8] == YT2_VIDEO_RECT && rows[8] == 1u &&
          types[9] == YT2_VIDEO_FRAME_END);

    /* Forced keyframe: mode, full palette and every row again. */
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 1) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, 24u, &count));
    CHECK(count == 15u);
    CHECK(types[10] == YT2_VIDEO_SET_MODE && types[11] == YT2_VIDEO_PALETTE &&
          types[12] == YT2_VIDEO_RECT && types[13] == YT2_VIDEO_RECT &&
          types[14] == YT2_VIDEO_FRAME_END);
    CHECK(rows[12] == 0u && rows[13] == 1u);

    yt2_folder_out_close(&out);
    yt2_presenter_done(&presenter);
    yt2_surface_free(&surface);
    return 0;
}
