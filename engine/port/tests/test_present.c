#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/palette.h"
#include "yt2/present.h"
#include "yt2/protocol.h"
#include "yt2/status.h"
#include "yt2/surface.h"

static int read_frames(const char *tmp, uint8_t *types, uint16_t *rows,
                       uint8_t *codecs, int8_t *dys, uint8_t *families,
                       uint32_t *tickns, uint16_t *paces, size_t capacity,
                       size_t *count)
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
        families[n] = frame[3];
        codecs[n] = frame[5];
        rows[n] = 0u;
        dys[n] = 0;
        tickns[n] = 0u;
        paces[n] = 0u;
        if (types[n] == YT2_VIDEO_RECT && length >= YT2_FRAME_HEADER_SIZE + 9u) {
            rows[n] = (uint16_t)(frame[YT2_FRAME_HEADER_SIZE + 3] |
                                 ((uint16_t)frame[YT2_FRAME_HEADER_SIZE + 4]
                                  << 8));
        }
        if (types[n] == YT2_VIDEO_SCROLL &&
            length >= YT2_FRAME_HEADER_SIZE + 2u) {
            dys[n] = (int8_t)frame[YT2_FRAME_HEADER_SIZE + 1];
        }
        if (families[n] == YT2_FAMILY_TICK &&
            length >= YT2_FRAME_HEADER_SIZE + 5u) {
            tickns[n] = (uint32_t)frame[YT2_FRAME_HEADER_SIZE + 1] |
                        ((uint32_t)frame[YT2_FRAME_HEADER_SIZE + 2] << 8) |
                        ((uint32_t)frame[YT2_FRAME_HEADER_SIZE + 3] << 16) |
                        ((uint32_t)frame[YT2_FRAME_HEADER_SIZE + 4] << 24);
        }
        if (families[n] == YT2_FAMILY_TICK &&
            types[n] == YT2_TICK_PACE &&
            length >= YT2_FRAME_HEADER_SIZE + 3u) {
            paces[n] = (uint16_t)(frame[YT2_FRAME_HEADER_SIZE + 1] |
                                  ((uint16_t)frame[YT2_FRAME_HEADER_SIZE + 2]
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

/* Row content keyed by id (distinct for every id below 65536). */
static void fill_id_row(uint8_t *row, unsigned int width, unsigned int id)
{
    for (unsigned int i = 0u; i < width; i++) {
        row[i] = (uint8_t)((i & 1u) ? (id >> 8) & 0xFFu : id & 0xFFu);
    }
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
    uint8_t codecs[24];
    int8_t dys[24];
    uint8_t families[24];
    uint32_t tickns[24];
    uint16_t paces[24];
    size_t count = 0u;

    CHECK(yt2_presenter_init(NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_present_frame(NULL, NULL, NULL, NULL, 0, 0u) == YT2_STATUS_ERROR);

    if (tmp == NULL || tmp[0] == '\0') {
        return 0;
    }
    CHECK(yt2_surface_init(&surface, 32u, 2u) == YT2_STATUS_OK);
    memset(surface.pixels, 7u, 32u * 2u);
    memset(palette.rgb, 0x11u, sizeof(palette.rgb));
    CHECK(yt2_presenter_init(&presenter) == YT2_STATUS_OK);
    CHECK(yt2_folder_out_open(&out, tmp) == YT2_STATUS_OK);

    /* First present: set_mode, full palette, every row, tick, end. */
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0, 0u) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, codecs, dys, families, tickns, paces, 24u, &count));
    CHECK(count == 6u);
    CHECK(types[0] == YT2_VIDEO_SET_MODE && types[1] == YT2_VIDEO_PALETTE &&
          types[2] == YT2_VIDEO_RECT && types[3] == YT2_VIDEO_RECT &&
          types[4] == YT2_TICK_TICK && types[5] == YT2_VIDEO_FRAME_END);
    CHECK(rows[2] == 0u && rows[3] == 1u);
    /* Row rects travel run-coded; everything else stays codec 0. */
    CHECK(codecs[0] == YT2_CODEC_RAW && codecs[1] == YT2_CODEC_RAW &&
          codecs[2] == YT2_CODEC_RUNS && codecs[3] == YT2_CODEC_RUNS &&
          codecs[4] == YT2_CODEC_RAW && codecs[5] == YT2_CODEC_RAW);
    CHECK(families[4] == YT2_FAMILY_TICK && tickns[4] == 0u);

    /* Unchanged: a tick stamp, then frame_end. */
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0, 1u) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, codecs, dys, families, tickns, paces, 24u, &count));
    CHECK(count == 8u);
    CHECK(types[6] == YT2_TICK_TICK && types[7] == YT2_VIDEO_FRAME_END);
    CHECK(families[6] == YT2_FAMILY_TICK && tickns[6] == 1u);

    /* One palette entry: the span closes with tick and frame_end. */
    palette.rgb[100 * 3u] = 0x22u;
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0, 2u) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, codecs, dys, families, tickns, paces, 24u, &count));
    CHECK(count == 11u);
    CHECK(types[8] == YT2_VIDEO_PALETTE && types[9] == YT2_TICK_TICK &&
          types[10] == YT2_VIDEO_FRAME_END);
    CHECK(families[9] == YT2_FAMILY_TICK && tickns[9] == 2u);

    /* One changed row: that row travels, with a skipped tick number. */
    surface.pixels[32u] = 9u;
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 0, 42u) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, codecs, dys, families, tickns, paces, 24u, &count));
    CHECK(count == 14u);
    CHECK(types[11] == YT2_VIDEO_RECT && rows[11] == 1u &&
          types[12] == YT2_TICK_TICK && types[13] == YT2_VIDEO_FRAME_END);
    CHECK(families[12] == YT2_FAMILY_TICK && tickns[12] == 42u);

    /* Forced keyframe: mode, full palette and every row again. */
    CHECK(yt2_present_frame(&presenter, &out, &surface, &palette, 1, 43u) ==
          YT2_STATUS_OK);
    CHECK(read_frames(tmp, types, rows, codecs, dys, families, tickns, paces, 24u, &count));
    CHECK(count == 20u);
    CHECK(types[14] == YT2_VIDEO_SET_MODE && types[15] == YT2_VIDEO_PALETTE &&
          types[16] == YT2_VIDEO_RECT && types[17] == YT2_VIDEO_RECT &&
          types[18] == YT2_TICK_TICK && types[19] == YT2_VIDEO_FRAME_END);
    CHECK(rows[16] == 0u && rows[17] == 1u);
    CHECK(families[18] == YT2_FAMILY_TICK && tickns[18] == 43u);

    /* Pace announcements ride on their own, outside any frame. */
    CHECK(yt2_present_pace(NULL, NULL, 55u) == YT2_STATUS_ERROR);
    CHECK(yt2_present_pace(&presenter, &out, 55u) == YT2_STATUS_OK);
    CHECK(yt2_present_pace(&presenter, &out, 0x10000u) == YT2_STATUS_ERROR);
    CHECK(read_frames(tmp, types, rows, codecs, dys, families, tickns, paces, 24u, &count));
    CHECK(count == 21u);
    CHECK(types[20] == YT2_TICK_PACE && families[20] == YT2_FAMILY_TICK &&
          paces[20] == 55u && codecs[20] == YT2_CODEC_RAW);

    /* Whole-screen pans: scroll plus residue, or plain rows. */
    {
        yt2_surface big;
        yt2_surface wide;
        yt2_presenter scroller;
        static uint8_t stypes[3100];
        static uint16_t srows[3100];
        static uint8_t scodecs[3100];
        static int8_t sdys[3100];
        static uint8_t sfamilies[3100];
        static uint32_t stickns[3100];
        static uint16_t spaces[3100];
        size_t total = 0u;
        size_t base;

        CHECK(yt2_surface_init(&big, 320u, 200u) == YT2_STATUS_OK);
        CHECK(yt2_surface_init(&wide, 640u, 400u) == YT2_STATUS_OK);
        CHECK(yt2_folder_out_open(&out, tmp) == YT2_STATUS_OK);

        /* Pan +3, clean picture: one scroll, three slid-in rows. */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 200u; y++) {
            unsigned int id = y < 3u ? 1000u + y : y - 3u;

            fill_id_row(big.pixels + (size_t)y * 320u, 320u, id);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 6u);
        CHECK(stypes[base] == YT2_VIDEO_SCROLL && sdys[base] == 3);
        CHECK(scodecs[base] == YT2_CODEC_RAW);
        CHECK(stypes[base + 1] == YT2_VIDEO_RECT && srows[base + 1] == 0u);
        CHECK(stypes[base + 2] == YT2_VIDEO_RECT && srows[base + 2] == 1u);
        CHECK(stypes[base + 3] == YT2_VIDEO_RECT && srows[base + 3] == 2u);
        CHECK(stypes[base + 4] == YT2_TICK_TICK &&
              sfamilies[base + 4] == YT2_FAMILY_TICK &&
              stickns[base + 4] == 0u);
        CHECK(stypes[base + 5] == YT2_VIDEO_FRAME_END);
        CHECK(scodecs[base + 1] == YT2_CODEC_RUNS &&
              scodecs[base + 2] == YT2_CODEC_RUNS &&
              scodecs[base + 3] == YT2_CODEC_RUNS);
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /* Pan -5 with one animated row mid-picture. */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 200u; y++) {
            unsigned int id =
                y >= 195u ? 1000u + y - 195u : y == 100u ? 2000u : y + 5u;

            fill_id_row(big.pixels + (size_t)y * 320u, 320u, id);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 9u);
        CHECK(stypes[base] == YT2_VIDEO_SCROLL && sdys[base] == -5);
        CHECK(stypes[base + 1] == YT2_VIDEO_RECT && srows[base + 1] == 100u);
        for (unsigned int k = 0u; k < 5u; k++) {
            CHECK(stypes[base + 2u + k] == YT2_VIDEO_RECT &&
                  srows[base + 2u + k] == 195u + k);
        }
        CHECK(stypes[base + 7u] == YT2_TICK_TICK &&
              sfamilies[base + 7u] == YT2_FAMILY_TICK);
        CHECK(stypes[base + 8u] == YT2_VIDEO_FRAME_END);
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /* Pan +8, the far edge of the range. */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 200u; y++) {
            unsigned int id = y < 8u ? 1000u + y : y - 8u;

            fill_id_row(big.pixels + (size_t)y * 320u, 320u, id);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 11u);
        CHECK(stypes[base] == YT2_VIDEO_SCROLL && sdys[base] == 8);
        for (unsigned int k = 0u; k < 8u; k++) {
            CHECK(stypes[base + 1u + k] == YT2_VIDEO_RECT &&
                  srows[base + 1u + k] == k);
        }
        CHECK(stypes[base + 9u] == YT2_TICK_TICK &&
              sfamilies[base + 9u] == YT2_FAMILY_TICK);
        CHECK(stypes[base + 10u] == YT2_VIDEO_FRAME_END);
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /* Pan +12, past the range: plain rows, no scroll. */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 200u; y++) {
            unsigned int id = y < 12u ? 1000u + y : y - 12u;

            fill_id_row(big.pixels + (size_t)y * 320u, 320u, id);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 202u);
        for (size_t i = base; i < total; i++) {
            CHECK(stypes[i] != YT2_VIDEO_SCROLL);
        }
        CHECK(stypes[total - 1u] == YT2_VIDEO_FRAME_END);
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /* Repaint: nothing aligns, plain rows. */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, 3000u + y);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 202u);
        for (size_t i = base; i < total; i++) {
            CHECK(stypes[i] != YT2_VIDEO_SCROLL);
        }
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /* Ten changed rows: below the attempt threshold, plain rows. */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 50u; y < 60u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, 2000u + y);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 12u);
        for (size_t i = base; i < total; i++) {
            CHECK(stypes[i] != YT2_VIDEO_SCROLL);
        }
        CHECK(stypes[total - 1u] == YT2_VIDEO_FRAME_END);
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /*
         * Tie with no shift: forty changed rows on a flat picture match
         * shift 0 and every +step equally, so the strict rule keeps rows.
         */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, 7u);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 40u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, 8u);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 42u);
        for (size_t i = base; i < total; i++) {
            CHECK(stypes[i] != YT2_VIDEO_SCROLL);
        }
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /*
         * Phase-flipped stripes: +1 and -1 explain 199 rows each, so the
         * first wins and only the slid-in row follows.
         */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u,
                        y & 1u ? 101u : 100u);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u,
                        y & 1u ? 100u : 101u);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 4u);
        CHECK(stypes[base] == YT2_VIDEO_SCROLL && sdys[base] == 1);
        CHECK(stypes[base + 1u] == YT2_VIDEO_RECT && srows[base + 1u] == 0u);
        CHECK(stypes[base + 2u] == YT2_TICK_TICK &&
              sfamilies[base + 2u] == YT2_FAMILY_TICK);
        CHECK(stypes[base + 3u] == YT2_VIDEO_FRAME_END);
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        /* Tall screens pan too: the port applies the rules at any height. */
        for (unsigned int y = 0u; y < 400u; y++) {
            fill_id_row(wide.pixels + (size_t)y * 640u, 640u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &wide, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 400u; y++) {
            unsigned int id = y < 2u ? 1000u + y : y - 2u;

            fill_id_row(wide.pixels + (size_t)y * 640u, 640u, id);
        }
        CHECK(yt2_present_frame(&scroller, &out, &wide, &palette, 0, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 5u);
        CHECK(stypes[base] == YT2_VIDEO_SCROLL && sdys[base] == 2);
        CHECK(stypes[base + 1u] == YT2_VIDEO_RECT && srows[base + 1u] == 0u);
        CHECK(stypes[base + 2u] == YT2_VIDEO_RECT && srows[base + 2u] == 1u);
        CHECK(stypes[base + 3u] == YT2_TICK_TICK &&
              sfamilies[base + 3u] == YT2_FAMILY_TICK);
        CHECK(stypes[base + 4u] == YT2_VIDEO_FRAME_END);
        CHECK(memcmp(scroller.shadow, wide.pixels, 640u * 400u) == 0);
        yt2_presenter_done(&scroller);

        /* Keyframes never scroll, even with pan content. */
        for (unsigned int y = 0u; y < 200u; y++) {
            fill_id_row(big.pixels + (size_t)y * 320u, 320u, y);
        }
        CHECK(yt2_presenter_init(&scroller) == YT2_STATUS_OK);
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        base = total;
        for (unsigned int y = 0u; y < 200u; y++) {
            unsigned int id = y < 3u ? 1000u + y : y - 3u;

            fill_id_row(big.pixels + (size_t)y * 320u, 320u, id);
        }
        CHECK(yt2_present_frame(&scroller, &out, &big, &palette, 1, 0u) ==
              YT2_STATUS_OK);
        CHECK(read_frames(tmp, stypes, srows, scodecs, sdys, sfamilies, stickns, spaces, 3100u, &total));
        CHECK(total - base == 204u);
        CHECK(stypes[base] == YT2_VIDEO_SET_MODE);
        for (size_t i = base; i < total; i++) {
            CHECK(stypes[i] != YT2_VIDEO_SCROLL);
        }
        CHECK(memcmp(scroller.shadow, big.pixels, 320u * 200u) == 0);
        yt2_presenter_done(&scroller);

        yt2_surface_free(&wide);
        yt2_surface_free(&big);
    }

    yt2_folder_out_close(&out);
    yt2_presenter_done(&presenter);
    yt2_surface_free(&surface);
    return 0;
}
