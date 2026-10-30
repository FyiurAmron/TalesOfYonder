#include "test.h"

#include <string.h>

#include "yt2/game.h"

/*
 * Regression tests for the string instructions the emitted code leans on.
 * The shared step helper used to advance both pointers for every operation,
 * so a LODSB/STOSB sprite loop moved the destination twice per pixel and
 * smeared the intro's animated sprites across each row.
 */

#define TEST_SEG 0x3000u

int test_failures;

static uint8_t test_read8(uint16_t offset)
{
    return YT2_MEM8(TEST_SEG, offset);
}

static void test_write8(uint16_t offset, uint8_t value)
{
    YT2_MEM8(TEST_SEG, offset) = value;
}

static void test_strings(void)
{
    g_m.r.ds = TEST_SEG;
    g_m.r.es = TEST_SEG;

    /* LODS moves the source pointer only. */
    test_write8(0x0100u, 0x5Au);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    g_m.r.ax = 0;
    g_m.r.flags = 0;
    yt2_string_lods(&g_m, 1, TEST_SEG, 0);
    CHECK((g_m.r.ax & 0xFFu) == 0x5Au);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0200u);

    /* STOS moves the destination pointer only. */
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    g_m.r.ax = 0xA5u;
    yt2_string_stos(&g_m, 1, 0);
    CHECK(test_read8(0x0200u) == 0xA5u);
    CHECK(g_m.r.di == 0x0201u);
    CHECK(g_m.r.si == 0x0100u);

    /* MOVS moves both. */
    test_write8(0x0100u, 0x3Cu);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    yt2_string_movs(&g_m, 1, TEST_SEG, 0);
    CHECK(test_read8(0x0200u) == 0x3Cu);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0201u);

    /* SCAS compares against the destination and moves the destination only. */
    test_write8(0x0300u, 0x77u);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0300u;
    g_m.r.ax = 0x77u;
    yt2_string_scas(&g_m, 1, 0);
    CHECK(g_m.r.di == 0x0301u);
    CHECK(g_m.r.si == 0x0100u);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));

    /* CMPS moves both and compares. */
    test_write8(0x0100u, 0x11u);
    test_write8(0x0300u, 0x11u);
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0300u;
    yt2_string_cmps(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0301u);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));

    /* The direction flag reverses the step. */
    g_m.r.flags = YT2_FLAG_DF;
    g_m.r.si = 0x0101u;
    g_m.r.di = 0x0201u;
    yt2_string_lods(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x0100u);
    CHECK(g_m.r.di == 0x0201u);
    yt2_string_stos(&g_m, 1, 0);
    CHECK(g_m.r.di == 0x0200u);
    CHECK(g_m.r.si == 0x0100u);
    yt2_string_movs(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x00FFu);
    CHECK(g_m.r.di == 0x01FFu);
    g_m.r.flags = 0;

    /* Word forms step by two. */
    test_write8(0x0400u, 0x34u);
    test_write8(0x0401u, 0x12u);
    g_m.r.si = 0x0400u;
    g_m.r.di = 0x0500u;
    yt2_string_lods(&g_m, 2, TEST_SEG, 0);
    CHECK(g_m.r.ax == 0x1234u);
    CHECK(g_m.r.si == 0x0402u);
    CHECK(g_m.r.di == 0x0500u);
    yt2_string_stos(&g_m, 2, 0);
    CHECK(test_read8(0x0500u) == 0x34u);
    CHECK(test_read8(0x0501u) == 0x12u);
    CHECK(g_m.r.di == 0x0502u);

    /* REP counts CX down and advances by the whole block. */
    g_m.r.si = 0x0400u;
    g_m.r.di = 0x0600u;
    g_m.r.cx = 3u;
    yt2_string_movs(&g_m, 1, TEST_SEG, 1);
    CHECK(g_m.r.cx == 0u);
    CHECK(g_m.r.si == 0x0403u);
    CHECK(g_m.r.di == 0x0603u);
    CHECK(test_read8(0x0600u) == 0x34u);

    /* OUTS moves the source only. */
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0200u;
    g_m.r.dx = 0x0080u;
    yt2_string_outs(&g_m, 1, TEST_SEG, 0);
    CHECK(g_m.r.si == 0x0101u);
    CHECK(g_m.r.di == 0x0200u);
}

static void test_vbe_modes(void)
{
    uint32_t screen = (uint32_t)YT2_SCREEN_SEG << 4;

    /* 640x400 mode 100h uses four 64 KB pages. */
    g_m.r.ax = 0x4F02u;
    g_m.r.bx = 0x0100u;
    yt2_video_int10(&g_m);
    CHECK(g_m.vbe_mode == 1);
    CHECK(g_m.video_mode == 0x0100);
    CHECK(g_m.vbe_bank_active == -1);
    CHECK(g_m.vbe_wait_palette == 1);

    g_m.r.ax = 0x4F05u;
    g_m.r.dx = 3u;
    yt2_video_int10(&g_m);
    CHECK(g_m.vbe_bank_active == 3);
    CHECK(memcmp(g_m.mem.bytes + screen, g_m.vbe_bank[3], 0x10000u) == 0);

    /* Writes to page 0 used to vanish because index 0 doubled as "none". */
    g_m.mem.bytes[screen + 0x1234u] = 0x5Au;
    g_m.r.dx = 0u;
    yt2_video_int10(&g_m);
    CHECK(g_m.vbe_bank[3][0x1234u] == 0x5Au);
    CHECK(g_m.vbe_bank_active == 0);

    /* 640x480 mode 101h needs and accepts page 4; page 5 is out of range. */
    g_m.r.ax = 0x4F02u;
    g_m.r.bx = 0x0101u;
    yt2_video_int10(&g_m);
    CHECK(g_m.video_mode == 0x0101);
    g_m.r.ax = 0x4F05u;
    g_m.r.dx = 4u;
    yt2_video_int10(&g_m);
    CHECK(g_m.vbe_bank_active == 4);
    g_m.r.dx = 5u;
    yt2_video_int10(&g_m);
    CHECK(g_m.vbe_bank_active == 4);
}

int main(void)
{
    yt2_game_init(&g_m, NULL);
    if (g_m.exited) {
        fprintf(stderr, "yt2game-tests: cannot initialise the machine\n");
        return 1;
    }
    test_strings();
    test_vbe_modes();
    yt2_game_shutdown(&g_m);
    if (test_failures != 0) {
        fprintf(stderr, "yt2game-tests: %d failure(s)\n", test_failures);
        return 1;
    }
    printf("yt2game-tests: OK\n");
    return 0;
}
