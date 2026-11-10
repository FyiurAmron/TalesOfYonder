#define _POSIX_C_SOURCE 200809L

#include "test.h"

#include <stdlib.h>
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

static void test_write16(uint16_t offset, uint16_t value)
{
    test_write8(offset, (uint8_t)(value & 0xFFu));
    test_write8((uint16_t)(offset + 1u), (uint8_t)(value >> 8));
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

    /* CMPS reads its source through the passed segment (DS:SI), not ES:SI.
       The emitter used Capstone's second operand (es:[di]) here, so the EMS
       probe compared ES:SI with ES:DI and passed on zeroed memory. */
    YT2_MEM8(0x4000u, 0x0100u) = 'B';
    YT2_MEM8(TEST_SEG, 0x0100u) = 'A';
    YT2_MEM8(0x4000u, 0x0300u) = 'A';
    g_m.r.ds = TEST_SEG;
    g_m.r.es = 0x4000u;
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0300u;
    g_m.r.flags = 0;
    yt2_string_cmps(&g_m, 1, TEST_SEG, 0);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));

    /* CMPS subtracts the destination from the source (DS:SI - ES:DI), so
       'A' vs 'B' borrows; the old reversed helper produced 1 instead. */
    YT2_MEM8(TEST_SEG, 0x0100u) = 'A';
    YT2_MEM8(0x4000u, 0x0300u) = 'B';
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0300u;
    g_m.r.flags = 0;
    yt2_string_cmps(&g_m, 1, TEST_SEG, 0);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_SF));
    YT2_MEM8(0x4000u, 0x0300u) = 'A';

    /* REPE keeps scanning while equal and stops on the first mismatch. */
    g_m.r.si = 0x0100u;
    g_m.r.di = 0x0300u;
    g_m.r.cx = 3u;
    g_m.r.flags = 0;
    YT2_MEM8(TEST_SEG, 0x0101u) = 'X';
    YT2_MEM8(0x4000u, 0x0301u) = 'Y';
    yt2_string_cmps(&g_m, 1, TEST_SEG, 1);
    CHECK(g_m.r.cx == 1u);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));
    CHECK(g_m.r.si == 0x0102u && g_m.r.di == 0x0302u);
    g_m.r.ds = TEST_SEG;
    g_m.r.es = TEST_SEG;

    /* A prefixed string op with CX=0 is a no-op: no body, no pointer move,
       no flag update and no port write.  The helpers used to execute once
       because their loop was do/while. */
    test_write8(0x0700u, 0x11u);
    test_write8(0x0800u, 0x22u);
    g_m.r.si = 0x0700u;
    g_m.r.di = 0x0800u;
    g_m.r.cx = 0u;
    g_m.r.ax = 0x3344u;
    g_m.r.flags = YT2_FLAG_ZF;
    {
        uint64_t before = g_m.cycles;

        yt2_string_movs(&g_m, 1, TEST_SEG, 1);
        CHECK(g_m.cycles == before);       /* no iteration was charged */
    }
    CHECK(test_read8(0x0800u) == 0x22u);
    CHECK(g_m.r.si == 0x0700u && g_m.r.di == 0x0800u);
    yt2_string_stos(&g_m, 1, 1);
    CHECK(test_read8(0x0800u) == 0x22u);
    CHECK(g_m.r.di == 0x0800u);
    g_m.r.ax = 0x3344u;
    yt2_string_lods(&g_m, 1, TEST_SEG, 1);
    CHECK(g_m.r.ax == 0x3344u && g_m.r.si == 0x0700u);
    yt2_string_cmps(&g_m, 1, TEST_SEG, 1);
    CHECK(g_m.r.si == 0x0700u && g_m.r.di == 0x0800u);
    CHECK(g_m.r.flags == YT2_FLAG_ZF);
    yt2_string_scas(&g_m, 1, 1);
    CHECK(g_m.r.di == 0x0800u);
    CHECK(g_m.r.flags == YT2_FLAG_ZF);
    g_m.port61 = 0x5Au;
    g_m.r.dx = 0x0061u;
    yt2_string_outs(&g_m, 1, TEST_SEG, 1);
    CHECK(g_m.port61 == 0x5Au);
    CHECK(g_m.r.si == 0x0700u);
    g_m.r.flags = 0;

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

/*
 * The clock is the DOSBox-X normal core: one cycle per instruction and one
 * per repeated-string iteration.  The derived hardware periods are pinned
 * here so a model change cannot quietly move the intro or the star, and the
 * REP test proves the per-iteration charge happens while the copy runs (a
 * present can see the first word, not just the completed flush).
 */
static void test_cycle_model(void)
{
    uint32_t saved_cycles_per_ms = g_m.cycles_per_ms;
    uint64_t saved_cycles_per_tick = g_m.cycles_per_tick;
    uint32_t saved_frame = g_m.vga_frame_cycles;
    uint32_t saved_retrace = g_m.vga_retrace_cycles;
    uint32_t saved_gap = g_m.present_gap_cycles;
    uint64_t saved_tick_at = g_m.tick_at;
    uint64_t saved_pace_at = g_m.pace_at;
    int saved_pace = g_m.pace;
    int saved_pace_started = g_m.pace_started;
    uint32_t saved_pace_wall = g_m.pace_wall_start;
    uint64_t saved_pace_cycle = g_m.pace_cycle_start;
    uint64_t saved_cycles = g_m.cycles;
    uint8_t saved_dirty = g_m.video_dirty;
    uint64_t saved_present_cycles = g_m.present_cycles;
    uint32_t saved_present_tick = g_m.present_tick;
    unsigned saved_present_passes = g_m.present_passes;
    int saved_have_cycle = g_m.present_have_cycle;
    unsigned long long saved_frames = g_m.present_frames;
    uint8_t *screen = g_m.mem.bytes + ((uint32_t)YT2_SCREEN_SEG << 4);
    unsigned i;

    yt2_game_set_clock(&g_m, 3000u);
    CHECK(g_m.cycles_per_tick == 164776u);
    CHECK(g_m.vga_frame_cycles == 42804u);
    CHECK(g_m.vga_retrace_cycles == 192u);
    CHECK(g_m.present_gap_cycles == 50000u);
    CHECK(YT2_CYCLES_INSN == 1u);
    CHECK(YT2_CYCLES_STRING_ITER == 1u);

    g_m.cycles = 0u;
    g_m.tick_at = 1000000u;
    g_m.pace = 0;
    g_m.video_dirty = 1;
    g_m.present_have_cycle = 1;
    g_m.present_cycles = 0u;
    g_m.present_gap_cycles = 1u;
    g_m.present_tick = g_m.ticks;
    g_m.present_passes = 0u;
    memset(screen, 0, 0x10000u);
    for (i = 0; i < 100u; i++) {
        YT2_MEM8(TEST_SEG, (uint16_t)(0x0800u + 2u * i)) = 0x11u;
        YT2_MEM8(TEST_SEG, (uint16_t)(0x0801u + 2u * i)) = 0x22u;
    }
    g_m.r.es = YT2_SCREEN_SEG;
    g_m.r.di = 0u;
    g_m.r.si = 0x0800u;
    g_m.r.cx = 100u;
    yt2_string_movs(&g_m, 2, TEST_SEG, 1);
    CHECK(g_m.cycles == 100u);
    CHECK(g_m.r.cx == 0u);
    CHECK(g_m.present_frames > saved_frames);
    CHECK(g_m.present_frame[0] == 0x11u);
    CHECK(g_m.present_frame[1] == 0x22u);
    /* The presented copy is a prefix, not the completed 100-word flush: a
       tick or present inside the REP lands between two copied words. */
    CHECK(g_m.present_frame[6] == 0x00u);
    CHECK(g_m.present_frame[7] == 0x00u);

    /* The pacer deadline is 64-bit too: past the 32-bit cycle boundary an
       armed deadline must stay ahead of the guest clock. */
    g_m.video_dirty = 0;
    g_m.cycles = 0x100000000ull;
    g_m.tick_at = g_m.cycles + 1000000u;
    g_m.pace = 1;
    g_m.pace_started = 0;
    g_m.pace_at = 0xFFFFFFFFu;
    yt2_cycles(1u);
    CHECK(g_m.pace_at > g_m.cycles);

    g_m.cycles_per_ms = saved_cycles_per_ms;
    g_m.cycles_per_tick = saved_cycles_per_tick;
    g_m.vga_frame_cycles = saved_frame;
    g_m.vga_retrace_cycles = saved_retrace;
    g_m.present_gap_cycles = saved_gap;
    g_m.tick_at = saved_tick_at;
    g_m.pace_at = saved_pace_at;
    g_m.pace = saved_pace;
    g_m.pace_started = saved_pace_started;
    g_m.pace_wall_start = saved_pace_wall;
    g_m.pace_cycle_start = saved_pace_cycle;
    g_m.cycles = saved_cycles;
    g_m.video_dirty = saved_dirty;
    g_m.present_cycles = saved_present_cycles;
    g_m.present_tick = saved_present_tick;
    g_m.present_passes = saved_present_passes;
    g_m.present_have_cycle = saved_have_cycle;
}

static void test_vbe_modes(void)
{
    uint32_t screen = (uint32_t)YT2_SCREEN_SEG << 4;

    /* 640x400 mode 100h uses four 64 KB pages. */
    g_m.r.ax = 0x4F02u;
    g_m.r.bx = 0x0100u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_OK);
    CHECK(g_m.vbe_mode == 1);
    CHECK(g_m.video_mode == 0x0100);
    CHECK(g_m.vbe_bank_active == -1);
    CHECK(g_m.vbe_wait_palette == 1);

    /* Get-current-mode answers the mode in BX. */
    g_m.r.ax = 0x4F03u;
    g_m.r.bx = 0u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_OK);
    CHECK(g_m.r.bx == 0x0100u);

    g_m.r.ax = 0x4F05u;
    g_m.r.bx = 0u;
    g_m.r.dx = 3u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_OK);
    CHECK(g_m.vbe_bank_active == 3);
    CHECK(memcmp(g_m.mem.bytes + screen, g_m.vbe_bank[3], 0x10000u) == 0);

    /* Writes to page 0 used to vanish because index 0 doubled as "none". */
    g_m.mem.bytes[screen + 0x1234u] = 0x5Au;
    g_m.r.ax = 0x4F05u;
    g_m.r.bx = 0u;
    g_m.r.dx = 0u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_OK);
    CHECK(g_m.vbe_bank[3][0x1234u] == 0x5Au);
    CHECK(g_m.vbe_bank_active == 0);

    /* 640x480 mode 101h needs and accepts page 4; page 5 is out of range. */
    g_m.r.ax = 0x4F02u;
    g_m.r.bx = 0x0101u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_OK);
    CHECK(g_m.video_mode == 0x0101);
    g_m.r.ax = 0x4F05u;
    g_m.r.bx = 0u;
    g_m.r.dx = 4u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_OK);
    CHECK(g_m.vbe_bank_active == 4);
    g_m.r.ax = 0x4F05u;
    g_m.r.bx = 0u;
    g_m.r.dx = 5u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_FAIL);
    CHECK(g_m.vbe_bank_active == 4);

    /* Window B does not exist, and an unsupported mode must not report
       success while the current mode stays up. */
    g_m.r.ax = 0x4F05u;
    g_m.r.bx = 0x0001u;
    g_m.r.dx = 0u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_FAIL);
    CHECK(g_m.vbe_bank_active == 4);

    g_m.r.ax = 0x4F02u;
    g_m.r.bx = 0x0102u;
    yt2_video_int10(&g_m);
    CHECK(g_m.r.ax == YT2_VBE_FAIL);
    CHECK(g_m.video_mode == 0x0101);
    CHECK(g_m.vbe_mode == 1);
}

/*
 * INT 10h AX=1017h writes a block of DAC registers from ES:DX and keeps the
 * DGROUP copy of the live palette in step.  The register index is an 8-bit
 * field: a block that runs past 255 wraps to 0, as the DAC does.
 */
static void test_video_palette_block(void)
{
    uint8_t saved_palette[768];
    uint8_t saved_mirror[768];
    uint8_t *mirror = g_m.mem.bytes + ((uint32_t)YT2_DGROUP_SEG << 4) + 0x4124u;
    unsigned index;

    memcpy(saved_palette, g_m.palette, sizeof(saved_palette));
    memcpy(saved_mirror, mirror, sizeof(saved_mirror));

    /* 6-bit components: the source's bit 6/7 must be dropped. */
    for (index = 0; index < 768u; index++) {
        YT2_MEM8(TEST_SEG, (uint16_t)(0x1000u + index)) =
            (uint8_t)(index | 0x40u);
    }
    g_m.r.ax = 0x1017u;
    g_m.r.es = TEST_SEG;
    g_m.r.dx = 0x1000u;
    g_m.r.cx = 0x0100u;
    g_m.r.bx = 0u;
    yt2_video_int10(&g_m);
    for (index = 0; index < 768u; index++) {
        CHECK(g_m.palette[index] == (uint8_t)(index & 0x3Fu));
        CHECK(mirror[index] == g_m.palette[index]);
    }

    /* Start at entry 240 for 32 entries: the last 16 wrap to entries 0..15. */
    for (index = 0; index < 96u; index++) {
        YT2_MEM8(TEST_SEG, (uint16_t)(0x2000u + index)) = (uint8_t)(0x80u + index);
    }
    g_m.r.ax = 0x1017u;
    g_m.r.es = TEST_SEG;
    g_m.r.dx = 0x2000u;
    g_m.r.cx = 32u;
    g_m.r.bx = 0x00F0u;
    yt2_video_int10(&g_m);
    CHECK(g_m.palette[0x00F0u * 3u + 0u] == 0x00u);      /* 0x80 & 3Fh */
    CHECK(g_m.palette[0x0000u * 3u + 0u] == 0x30u);      /* 0xB0 & 3Fh */
    CHECK(g_m.palette[0x000Fu * 3u + 0u] == 0x1Du);      /* 0xDD & 3Fh */
    CHECK(mirror[0x0000u * 3u + 0u] == g_m.palette[0x0000u * 3u + 0u]);

    memcpy(g_m.palette, saved_palette, sizeof(saved_palette));
    memcpy(mirror, saved_mirror, sizeof(saved_mirror));
}

/*
 * The BIOS clock answers INT 1Ah AH=00h from 0040:006Ch and reports the
 * 24-hour rollover flag at 0040:0070h in AL, clearing it.  The tick handler
 * owns both; a wrap used to reset the count without ever raising the flag,
 * so the answer was always AL=0.
 */
static void test_clock_int1a(void)
{
    uint32_t saved_ticks = g_m.ticks;
    uint8_t saved_timer[4];
    uint8_t saved_flag = g_m.mem.bytes[YT2_BIOS_MIDNIGHT];

    memcpy(saved_timer, g_m.mem.bytes + YT2_BIOS_TICK_COUNT, sizeof(saved_timer));

    g_m.ticks = 0x00012344u;
    g_m.mem.bytes[YT2_BIOS_MIDNIGHT] = 0u;
    yt2_machine_tick(&g_m);
    CHECK(g_m.ticks == 0x00012345u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 0u] == 0x45u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 1u] == 0x23u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 2u] == 0x01u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 3u] == 0x00u);
    CHECK(g_m.mem.bytes[YT2_BIOS_MIDNIGHT] == 0u);

    /* Last tick of the day wraps to zero and raises the flag. */
    g_m.ticks = 0x001800AFu;
    g_m.mem.bytes[YT2_BIOS_MIDNIGHT] = 0u;
    yt2_machine_tick(&g_m);
    CHECK(g_m.ticks == 0u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 0u] == 0u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 1u] == 0u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 2u] == 0u);
    CHECK(g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 3u] == 0u);
    CHECK(g_m.mem.bytes[YT2_BIOS_MIDNIGHT] == 0x01u);

    g_m.r.ax = 0x0000u;
    yt2_clock_int1a(&g_m);
    CHECK(g_m.r.ax == 0x0001u);         /* AH=0, AL=the rollover flag */
    CHECK(g_m.r.cx == 0u);
    CHECK(g_m.r.dx == 0u);
    CHECK(g_m.mem.bytes[YT2_BIOS_MIDNIGHT] == 0u);

    g_m.r.ax = 0x0000u;
    yt2_clock_int1a(&g_m);
    CHECK(g_m.r.ax == 0u);
    CHECK(g_m.r.cx == 0u && g_m.r.dx == 0u);

    /* The answer follows the BIOS data area, not an internal counter. */
    g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 0u] = 0x45u;
    g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 1u] = 0x23u;
    g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 2u] = 0x01u;
    g_m.mem.bytes[YT2_BIOS_TICK_COUNT + 3u] = 0x00u;
    g_m.r.ax = 0x0000u;
    yt2_clock_int1a(&g_m);
    CHECK(g_m.r.cx == 0x0001u);
    CHECK(g_m.r.dx == 0x2345u);

    g_m.ticks = saved_ticks;
    memcpy(g_m.mem.bytes + YT2_BIOS_TICK_COUNT, saved_timer, sizeof(saved_timer));
    g_m.mem.bytes[YT2_BIOS_MIDNIGHT] = saved_flag;
}

/*
 * INT 33h keeps a signed position, motion counters in the current ratio and
 * two button bits.  The counters are mickeys: a 4:1 ratio turns two 1-pixel
 * host moves into one counter unit, and reading 0Bh clears them.
 */
static void test_mouse_int33(void)
{
    uint16_t saved_x = g_m.mouse_x;
    uint16_t saved_y = g_m.mouse_y;
    uint16_t saved_buttons = g_m.mouse_buttons;
    uint16_t saved_ratio_x = g_m.mouse_ratio_x;
    uint16_t saved_ratio_y = g_m.mouse_ratio_y;
    int16_t saved_dx = g_m.mouse_dx;
    int16_t saved_dy = g_m.mouse_dy;
    int16_t saved_frac_x = g_m.mouse_frac_x;
    int16_t saved_frac_y = g_m.mouse_frac_y;
    uint16_t saved_handler_cs = g_m.mouse_handler_cs;
    uint16_t saved_handler_ip = g_m.mouse_handler_ip;
    uint16_t saved_mask = g_m.mouse_mask;

    g_m.mouse_handler_cs = 0u;
    g_m.mouse_handler_ip = 0u;
    g_m.mouse_mask = 0u;
    g_m.mouse_x = 100u;
    g_m.mouse_y = 100u;
    g_m.mouse_buttons = 0u;
    g_m.mouse_dx = 0;
    g_m.mouse_dy = 0;
    g_m.mouse_frac_x = 0;
    g_m.mouse_frac_y = 0;

    g_m.r.ax = 0x000Fu;
    g_m.r.cx = 4u;
    g_m.r.dx = 4u;
    yt2_mouse_int33(&g_m);
    CHECK(g_m.mouse_ratio_x == 4u && g_m.mouse_ratio_y == 4u);

    yt2_mouse_move(&g_m, 1, 0);
    CHECK(g_m.mouse_dx == 0);
    CHECK(g_m.mouse_frac_x == 4);
    yt2_mouse_move(&g_m, 1, 0);
    CHECK(g_m.mouse_dx == 1);
    CHECK(g_m.mouse_frac_x == 0);
    CHECK(g_m.mouse_x == 102u);

    g_m.r.ax = 0x000Bu;
    yt2_mouse_int33(&g_m);
    CHECK(g_m.r.cx == 1u);
    CHECK(g_m.r.dx == 0u);
    CHECK(g_m.r.bx == 0u);              /* no button is down */
    CHECK(g_m.mouse_dx == 0);
    g_m.r.ax = 0x000Bu;
    yt2_mouse_int33(&g_m);
    CHECK(g_m.r.cx == 0u && g_m.r.dx == 0u);

    /* Button edges set the state bits 03h/0Bh report and clear again. */
    yt2_mouse_button(&g_m, 0u, 1u);
    CHECK(g_m.mouse_buttons == YT2_MOUSE_LEFT);
    yt2_mouse_button(&g_m, 2u, 1u);
    CHECK(g_m.mouse_buttons == (YT2_MOUSE_LEFT | YT2_MOUSE_RIGHT));
    yt2_mouse_button(&g_m, 0u, 0u);
    CHECK(g_m.mouse_buttons == YT2_MOUSE_RIGHT);
    yt2_mouse_button(&g_m, 2u, 0u);
    CHECK(g_m.mouse_buttons == 0u);

    /* Function 04h takes signed coordinates: -1 clamps to the minimum edge,
       not to the maximum one. */
    g_m.r.ax = 0x0004u;
    g_m.r.cx = 0xFFFFu;
    g_m.r.dx = 0xFFFFu;
    yt2_mouse_int33(&g_m);
    CHECK(g_m.mouse_x == 0u);
    CHECK(g_m.mouse_y == 0u);
    g_m.r.cx = 400u;
    g_m.r.dx = 250u;
    yt2_mouse_int33(&g_m);
    CHECK(g_m.mouse_x == 319u);
    CHECK(g_m.mouse_y == 199u);

    g_m.r.ax = 0x0003u;
    yt2_mouse_int33(&g_m);
    CHECK(g_m.r.cx == 319u && g_m.r.dx == 199u);
    CHECK(g_m.r.bx == 0u);

    g_m.mouse_x = saved_x;
    g_m.mouse_y = saved_y;
    g_m.mouse_buttons = saved_buttons;
    g_m.mouse_ratio_x = saved_ratio_x;
    g_m.mouse_ratio_y = saved_ratio_y;
    g_m.mouse_dx = saved_dx;
    g_m.mouse_dy = saved_dy;
    g_m.mouse_frac_x = saved_frac_x;
    g_m.mouse_frac_y = saved_frac_y;
    g_m.mouse_handler_cs = saved_handler_cs;
    g_m.mouse_handler_ip = saved_handler_ip;
    g_m.mouse_mask = saved_mask;
}

/*
 * A mouse callback is an interrupt: the driver restores the interrupted
 * program's complete register file when the handler returns.  A callback
 * delivered from a present hook used to leave its AX/BX/CX/DX event
 * parameters in the drawing code - BX (a sprite copy width) became the
 * button mask, 0, and the row loop underflowed into the next segment.
 * The handler early-outs without a driver, which is enough: it preserves
 * its entry registers, so the values after the call are the driver's
 * event arguments unless the wrapper restores the interrupted set.
 */
static void test_mouse_callback_preserves_registers(void)
{
    uint16_t handler_segment = (uint16_t)(YT2_LOAD_SEG + 0x13A0u);
    yt2_registers saved;

    g_m.mouse_handler_cs = handler_segment;
    g_m.mouse_handler_ip = 0x0002u;
    g_m.mouse_mask = 0x0001u;
    g_m.mouse_buttons = 0;
    g_m.r.ax = 0x1234u;
    g_m.r.bx = 0x008Cu;
    g_m.r.cx = 0x5678u;
    g_m.r.dx = 0x9ABCu;
    g_m.r.si = 0x1111u;
    g_m.r.di = 0x2222u;
    g_m.r.bp = 0x3333u;
    g_m.r.ds = 0x4444u;
    g_m.r.es = 0x5555u;
    g_m.r.flags = YT2_FLAG_DF;
    saved = g_m.r;

    yt2_mouse_abs(&g_m, 100u, 50u);

    CHECK(g_m.r.ax == saved.ax);
    CHECK(g_m.r.bx == saved.bx);
    CHECK(g_m.r.cx == saved.cx);
    CHECK(g_m.r.dx == saved.dx);
    CHECK(g_m.r.si == saved.si);
    CHECK(g_m.r.di == saved.di);
    CHECK(g_m.r.bp == saved.bp);
    CHECK(g_m.r.ds == saved.ds);
    CHECK(g_m.r.es == saved.es);
    CHECK(g_m.r.sp == saved.sp);
    CHECK(g_m.r.flags == saved.flags);
}

typedef struct test_input_source {
    const uint8_t *data;
    uint32_t length;
    uint32_t offset;
    int reads;
} test_input_source;

static int test_input_read(void *context, uint8_t *destination, uint32_t capacity,
                           uint32_t *read)
{
    test_input_source *source = (test_input_source *)context;
    uint32_t left = source->length - source->offset;

    source->reads++;
    if (left > capacity) {
        left = capacity;
    }
    memcpy(destination, source->data + source->offset, left);
    source->offset += left;
    *read = left;
    return YT2_OK;
}

static int test_input_write(void *context, const uint8_t *frame, uint32_t length)
{
    (void)context;
    (void)frame;
    (void)length;
    return YT2_OK;
}

static void test_count_button(void *context, uint8_t button, uint8_t down)
{
    int *count = (int *)context;

    (void)button;
    (void)down;
    (*count)++;
}

/*
 * Input may only be taken from the link where the INT 33h callback can run.
 * A present pass inside a nested guest callback (the mouse handler itself,
 * or the INT 1Ch ISR) used to consume a queued button edge and drop it,
 * which is what made a physical click disappear until the next one.  With
 * the guard the record stays in the transport until a safe pass.
 */
static void test_link_poll_defers_input_while_nested(void)
{
    uint8_t frame[YT2_FRAME_HEADER_SIZE + YT2_INPUT_MAX_PAYLOAD];
    test_input_source source;
    yt2_transport transport;
    yt2_service service;
    yt2_input_handlers handlers;
    yt2_service saved_service = g_m.service;
    yt2_transport saved_transport = g_m.transport;
    int saved_ready = g_m.link_ready;
    int saved_nested = g_m.nested;
    int buttons = 0;
    uint32_t payload;

    memset(frame, 0, sizeof(frame));
    yt2_put_le16(frame + 0u, YT2_PROTOCOL_MAGIC);
    frame[2] = YT2_PROTOCOL_VERSION;
    frame[3] = YT2_FAMILY_INPUT;
    yt2_put_le32(frame + 6u, 0u);
    payload = (uint32_t)yt2_payload_input_mouse_button(
        frame + YT2_FRAME_HEADER_SIZE, 0u, 1u);
    yt2_put_le32(frame + 10u, payload);

    memset(&source, 0, sizeof(source));
    source.data = frame;
    source.length = YT2_FRAME_HEADER_SIZE + payload;

    memset(&transport, 0, sizeof(transport));
    transport.context = &source;
    transport.write_frame = test_input_write;
    transport.read = test_input_read;

    memset(&handlers, 0, sizeof(handlers));
    handlers.mouse_button = test_count_button;
    yt2_service_init(&service, &transport, &handlers, &buttons);

    g_m.transport = transport;
    g_m.service = service;
    g_m.link_ready = 1;

    g_m.nested = 1;
    yt2_link_poll(&g_m);
    CHECK(source.reads == 0);
    CHECK(buttons == 0);

    g_m.nested = 0;
    yt2_link_poll(&g_m);
    CHECK(source.reads > 0);
    CHECK(buttons == 1);

    g_m.link_ready = saved_ready;
    g_m.nested = saved_nested;
    g_m.service = saved_service;
    g_m.transport = saved_transport;
}

/*
 * The game's EMS probe asks DOS for the int 67h vector and compares its own
 * "EMMXXXX0" copy with the device name at vector:000Ah.  The emulated manager
 * answers with the sentinel paragraph, which carries that name; without it the
 * generated repe cmpsb (once it read DS:SI correctly) would abort EMS init.
 */
static void test_ems_probe(void)
{
    static const char name[9] = "EMMXXXX0";
    uint16_t es;
    unsigned index;
    uint16_t saved_ds = g_m.r.ds;
    uint16_t saved_si = g_m.r.si;
    uint16_t saved_di = g_m.r.di;
    uint16_t saved_cx = g_m.r.cx;
    uint16_t saved_flags = g_m.r.flags;
    uint16_t saved_ax = g_m.r.ax;

    g_m.r.ax = 0x3567u;
    yt2_dos_int21(&g_m);
    es = g_m.r.es;
    CHECK(es == YT2_EMS_VECTOR_SEG);
    for (index = 0; index < 9u; index++) {
        CHECK(YT2_MEM8(es, (uint16_t)(YT2_EMS_VECTOR_NAME + index)) ==
              (uint8_t)name[index]);
    }

    /* The exact comparison the generated code performs. */
    for (index = 0; index < 8u; index++) {
        YT2_MEM8(YT2_DGROUP_SEG, (uint16_t)(0x0CA0u + index)) =
            (uint8_t)name[index];
    }
    g_m.r.ds = YT2_DGROUP_SEG;
    g_m.r.es = es;
    g_m.r.si = 0x0CA0u;
    g_m.r.di = YT2_EMS_VECTOR_NAME;
    g_m.r.cx = 8u;
    g_m.r.flags = 0;
    yt2_string_cmps(&g_m, 1, g_m.r.ds, 1);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_ZF));

    g_m.r.ds = saved_ds;
    g_m.r.si = saved_si;
    g_m.r.di = saved_di;
    g_m.r.cx = saved_cx;
    g_m.r.flags = saved_flags;
    g_m.r.ax = saved_ax;
}

/*
 * EMS 4.0 basics the picture cache relies on: allocate a handle, map logical
 * pages into the frame, flush on remap, load on map, and give the pages back.
 */
static void test_ems_map(void)
{
    uint16_t saved_ds = g_m.r.ds;
    uint16_t saved_si = g_m.r.si;
    uint16_t free_pages;
    uint16_t handle;
    unsigned index;

    g_m.r.ax = 0x4200u;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x00u);
    free_pages = g_m.r.bx;
    CHECK(g_m.r.dx == g_m.ems_total_pages);

    g_m.r.ax = 0x4300u;
    g_m.r.bx = 4u;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x00u);
    handle = g_m.r.dx;
    CHECK(handle != 0u && handle < YT2_EMS_HANDLES);
    CHECK(g_m.ems_handle[handle].used);
    CHECK(g_m.ems_handle[handle].pages == 4u);

    /* Map logical 0/1 to physical 0/1 and write a pattern. */
    for (index = 0; index < 4u; index++) {
        test_write16((uint16_t)(0x0200u + index * 4u),
                     (uint16_t)index);
        test_write16((uint16_t)(0x0202u + index * 4u),
                     (uint16_t)index);
    }
    g_m.r.ax = 0x5000u;
    g_m.r.dx = handle;
    g_m.r.cx = 2u;
    g_m.r.ds = TEST_SEG;
    g_m.r.si = 0x0200u;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x00u);
    YT2_MEM8(YT2_EMS_FRAME_SEG, 0x0000u) = 0xAAu;
    YT2_MEM8(YT2_EMS_FRAME_SEG, 0x4000u) = 0xBBu;

    /* Map logical 2/3 over the same physical slots: the frame flushes the old
       pages first, then loads the new (untouched, zeroed) ones. */
    test_write16(0x0210u, 2u);
    test_write16(0x0212u, 0u);
    test_write16(0x0214u, 3u);
    test_write16(0x0216u, 1u);
    g_m.r.ax = 0x5000u;
    g_m.r.dx = handle;
    g_m.r.cx = 2u;
    g_m.r.ds = TEST_SEG;
    g_m.r.si = 0x0210u;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x00u);
    CHECK(YT2_MEM8(YT2_EMS_FRAME_SEG, 0x0000u) == 0x00u);
    CHECK(YT2_MEM8(YT2_EMS_FRAME_SEG, 0x4000u) == 0x00u);
    YT2_MEM8(YT2_EMS_FRAME_SEG, 0x0000u) = 0xCCu;
    YT2_MEM8(YT2_EMS_FRAME_SEG, 0x4000u) = 0xDDu;

    /* Map back: the first pattern must survive in the page pool. */
    g_m.r.ax = 0x5000u;
    g_m.r.dx = handle;
    g_m.r.cx = 2u;
    g_m.r.ds = TEST_SEG;
    g_m.r.si = 0x0200u;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x00u);
    CHECK(YT2_MEM8(YT2_EMS_FRAME_SEG, 0x0000u) == 0xAAu);
    CHECK(YT2_MEM8(YT2_EMS_FRAME_SEG, 0x4000u) == 0xBBu);

    /* Invalid logical page, physical page and handle. */
    test_write16(0x0220u, 4u);
    test_write16(0x0222u, 0u);
    g_m.r.ax = 0x5000u;
    g_m.r.dx = handle;
    g_m.r.cx = 1u;
    g_m.r.ds = TEST_SEG;
    g_m.r.si = 0x0220u;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x8Au);

    test_write16(0x0220u, 0u);
    test_write16(0x0222u, 4u);
    g_m.r.ax = 0x5000u;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x8Bu);

    g_m.r.ax = 0x5000u;
    g_m.r.dx = 0x000Fu;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x83u);

    /* Free the handle once; a second free is invalid. */
    g_m.r.ax = 0x4500u;
    g_m.r.dx = handle;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x00u);
    CHECK(!g_m.ems_handle[handle].used);
    g_m.r.ax = 0x4500u;
    g_m.r.dx = handle;
    yt2_ems_int67(&g_m);
    CHECK((uint8_t)(g_m.r.ax >> 8) == 0x83u);

    g_m.r.ax = 0x4200u;
    yt2_ems_int67(&g_m);
    CHECK(g_m.r.bx == free_pages);
    g_m.r.ds = saved_ds;
    g_m.r.si = saved_si;
}

/* The DOS arena behind int 21h 48h/49h/4Ah: first fit, free coalescing and
   the resize paths the game's sound-buffer allocation uses. */
static void test_dos_arena(void)
{
    uint16_t saved_es = g_m.r.es;
    uint16_t first;
    uint16_t second;
    uint16_t small;

    g_m.r.ax = 0x4800u;
    g_m.r.bx = 0xFFFFu;
    yt2_dos_int21(&g_m);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    CHECK(g_m.r.ax == 8u && g_m.r.bx == 0x6000u);

    g_m.r.ax = 0x4800u;
    g_m.r.bx = 0x1000u;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    first = g_m.r.ax;
    CHECK(first == 0x4000u);
    g_m.r.ax = 0x4800u;
    g_m.r.bx = 0x1000u;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    second = g_m.r.ax;
    CHECK(second == 0x5000u);

    g_m.r.ax = 0x4900u;
    g_m.r.es = first;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    g_m.r.ax = 0x4900u;
    g_m.r.es = second;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));

    g_m.r.ax = 0x4800u;
    g_m.r.bx = 0xFFFFu;
    yt2_dos_int21(&g_m);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    CHECK(g_m.r.bx == 0x6000u);       /* both frees coalesced */

    g_m.r.ax = 0x4800u;
    g_m.r.bx = 0x0100u;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    small = g_m.r.ax;
    CHECK(small == 0x4000u);

    g_m.r.ax = 0x4A00u;
    g_m.r.es = small;
    g_m.r.bx = 0xFFFFu;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    CHECK(g_m.r.bx == 0x6000u);       /* every following paragraph is free */
    g_m.r.ax = 0x4A00u;
    g_m.r.es = small;
    g_m.r.bx = 0x0080u;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    g_m.r.ax = 0x4A00u;
    g_m.r.es = small;
    g_m.r.bx = 0x0200u;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));

    g_m.r.ax = 0x4900u;
    g_m.r.es = small;
    yt2_dos_int21(&g_m);
    CHECK(!yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    g_m.r.ax = 0x4800u;
    g_m.r.bx = 0xFFFFu;
    yt2_dos_int21(&g_m);
    CHECK(yt2_cpu_get_flag(&g_m.r, YT2_FLAG_CF));
    CHECK(g_m.r.bx == 0x6000u);
    g_m.r.es = saved_es;
}

/*
 * Shift flags: a masked count of zero leaves the flags alone; for a count of
 * one SHL compares the new sign with the carry, SHR keeps the original sign,
 * and SAR clears OF.  The old shared helper tested bit 15 for every width and
 * used the SHL rule for all three operations.
 */
static void test_shift_flags(void)
{
    yt2_registers *r = &g_m.r;

    r->flags = 0;
    CHECK(yt2_cpu_shl8(r, 0x40u, 1u) == 0x80u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_shl8(r, 0x80u, 1u) == 0x00u);
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_shl8(r, 0xC0u, 1u) == 0x80u);
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_OF));

    r->flags = 0;
    CHECK(yt2_cpu_shr8(r, 0x40u, 1u) == 0x20u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_shr8(r, 0x80u, 1u) == 0x40u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));

    r->flags = 0;
    CHECK(yt2_cpu_sar8(r, 0x80u, 1u) == 0xC0u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_sar8(r, 0x81u, 1u) == 0xC0u);
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_OF));

    r->flags = 0;
    CHECK(yt2_cpu_shl16(r, 0x4000u, 1u) == 0x8000u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_shr16(r, 0x8000u, 1u) == 0x4000u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_sar16(r, 0x8000u, 1u) == 0xC000u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_OF));

    r->flags = YT2_FLAG_CF | YT2_FLAG_OF;
    CHECK(yt2_cpu_shl16(r, 0x1234u, 0u) == 0x1234u);
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));
    CHECK(yt2_cpu_shl32(r, 0x12345678u, 0u) == 0x12345678u);
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));

    r->flags = 0;
    CHECK(yt2_cpu_shl32(r, 0x40000000u, 1u) == 0x80000000u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_shr32(r, 0x80000000u, 1u) == 0x40000000u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(r, YT2_FLAG_OF));
    r->flags = 0;
    CHECK(yt2_cpu_sar32(r, 0x80000000u, 1u) == 0xC0000000u);
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(r, YT2_FLAG_OF));
    g_m.r.flags = 0;
}

/* IDIV must report a quotient overflow instead of dividing INT32_MIN by -1
   (undefined in C) and must leave AX/DX untouched on failure. */
static void test_divide_overflow(void)
{
    g_m.r.ax = 0x0000u;
    g_m.r.dx = 0x8000u;
    CHECK(yt2_cpu_idiv16(&g_m.r, 0xFFFFu) == 0);
    CHECK(g_m.r.ax == 0x0000u && g_m.r.dx == 0x8000u);

    g_m.r.ax = 0xFF80u;
    CHECK(yt2_cpu_idiv8(&g_m.r, 0xFFu) == 0);
    CHECK(g_m.r.ax == 0xFF80u);

    g_m.r.ax = 7u;
    g_m.r.dx = 0u;
    CHECK(yt2_cpu_idiv16(&g_m.r, 2u) == 1);
    CHECK(g_m.r.ax == 3u && g_m.r.dx == 1u);
}

/* Run the game's own fn_SpeakerBeep (S06E7) through the dispatcher and check
   the PC-speaker fallback: PIT channel 2 divisor 662 (~1.8 kHz), a gate-on
   edge, four BIOS ticks of delay, and the matching gate-off edge.  The log
   is the same AUDIO boundary the client consumes, so the line format is part
   of what M6 checks. */
static void test_speaker_beep(void)
{
    char *buffer = NULL;
    size_t size = 0;
    FILE *log = open_memstream(&buffer, &size);
    uint16_t saved_ss = g_m.r.ss;
    uint16_t saved_sp = g_m.r.sp;
    uint16_t saved_next_cs = g_m.next_cs;
    uint16_t saved_next_ip = g_m.next_ip;
    int saved_pending = g_m.next_pending;
    uint32_t on_pit = 0;
    uint32_t off_pit = 0;
    unsigned on_divisor = 0;
    int on = 0;
    int off = 0;

    CHECK(log != NULL);
    if (log == NULL) {
        return;
    }
    yt2_sound_set_log(&g_m, log);
    /* The generated code fetches the INT number from the instruction stream
       at CS:IP-1, so the game image must be mapped for fn_DelayTicks. */
    yt2_game_load(&g_m);
    g_m.r.ss = YT2_LOAD_SEG;
    g_m.r.sp = 0xFF00u;
    yt2_push16(YT2_SENTINEL_SEG);
    yt2_push16(YT2_SENTINEL_RESUME);
    yt2_go((uint16_t)(YT2_LOAD_SEG + 0x06E7u), 0x0006u);
    (void)yt2_game_run(&g_m, 2000000u);
    yt2_sound_set_log(&g_m, NULL);
    fflush(log);
    fclose(log);
    g_m.r.ss = saved_ss;
    g_m.r.sp = saved_sp;
    g_m.next_cs = saved_next_cs;
    g_m.next_ip = saved_next_ip;
    g_m.next_pending = saved_pending;

    if (buffer != NULL) {
        char *line = buffer;
        while (line != NULL && *line != '\0') {
            char *end = strchr(line, '\n');
            unsigned pit;
            unsigned gate;
            unsigned divisor;
            if (end != NULL) {
                *end = '\0';
            }
            if (sscanf(line, "S %x %u %u", &pit, &gate, &divisor) == 3) {
                if (gate != 0u) {
                    on = 1;
                    on_pit = pit;
                    on_divisor = divisor;
                } else {
                    off = 1;
                    off_pit = pit;
                }
            }
            line = end != NULL ? end + 1 : NULL;
        }
    }
    CHECK(on);
    CHECK(off);
    CHECK(on_divisor == 662u);
    CHECK(off_pit > on_pit);
    CHECK(off_pit - on_pit > 200000u && off_pit - on_pit < 330000u);
    free(buffer);
}

int main(void)
{
    yt2_game_init(&g_m, NULL);
    if (g_m.exited) {
        fprintf(stderr, "yt2game-tests: cannot initialise the machine\n");
        return 1;
    }
    test_strings();
    test_shift_flags();
    test_divide_overflow();
    test_cycle_model();
    test_ems_probe();
    test_ems_map();
    test_dos_arena();
    test_mouse_callback_preserves_registers();
    test_link_poll_defers_input_while_nested();
    test_video_palette_block();
    test_vbe_modes();
    test_clock_int1a();
    test_mouse_int33();
    test_speaker_beep();
    yt2_game_shutdown(&g_m);
    if (test_failures != 0) {
        fprintf(stderr, "yt2game-tests: %d failure(s)\n", test_failures);
        return 1;
    }
    printf("yt2game-tests: OK\n");
    return 0;
}
