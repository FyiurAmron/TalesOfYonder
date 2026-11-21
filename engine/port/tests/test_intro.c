#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/intro.h"
#include "yt2/titlemenu.h"
#include "yt2/pcx.h"

typedef struct capture {
    unsigned int frames;
    uint8_t last_frame[320u * 200u];
    uint8_t last_palette[768u];
    unsigned long last_tick;
} capture;

static int capture_emit(const uint8_t *frame, const uint8_t *palette,
                        unsigned long tick, void *user)
{
    capture *seen = user;

    seen->frames++;
    memcpy(seen->last_frame, frame, sizeof(seen->last_frame));
    memcpy(seen->last_palette, palette, sizeof(seen->last_palette));
    seen->last_tick = tick;
    return 0;
}

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

/* Fade vectors over a synthetic two-entry range (no data files needed). */
static void fade_vectors(yt2_intro *intro, capture *seen)
{
    /* Mode 0/3: down to black with the early exit. */
    memset(intro->palettes.shown.rgb, 0, YT2_PALETTE_SIZE);
    intro->palettes.shown.rgb[0] = 3u;
    intro->palettes.shown.rgb[4] = 1u;
    intro->retrace_wait = 1;
    seen->frames = 0;
    yt2_intro_fade(intro, 0u, 63u, 2u, 0u);
    CHECK(seen->frames == 4u);   /* show + 3 steps, then the early exit */
    CHECK(intro->palettes.shown.rgb[0] == 0u);
    CHECK(intro->palettes.shown.rgb[4] == 0u);
    CHECK(intro->tick == 0u);    /* 3 steps burn no whole tick */

    /* Mode 2/4: up to the target with the early exit. */
    memset(intro->palettes.shown.rgb, 0, YT2_PALETTE_SIZE);
    memset(intro->palettes.target.rgb, 0, YT2_PALETTE_SIZE);
    intro->palettes.target.rgb[1] = 2u;
    seen->frames = 0;
    yt2_intro_fade(intro, 4u, 63u, 2u, 0u);
    CHECK(seen->frames == 2u);
    CHECK(intro->palettes.shown.rgb[1] == 2u);

    /* Mode 1: the wrapping work fill lands exactly on the target. */
    memset(intro->palettes.staging.rgb, 0xAAu, YT2_PALETTE_SIZE);
    memset(intro->palettes.target.rgb, 0, YT2_PALETTE_SIZE);
    intro->palettes.target.rgb[0] = 0u;
    intro->palettes.target.rgb[1] = 10u;
    intro->palettes.target.rgb[2] = 63u;
    seen->frames = 0;
    intro->tick = 0u;
    intro->fade_steps = 0u;
    yt2_intro_fade(intro, 1u, 63u, 1u, 0u);
    CHECK(intro->palettes.shown.rgb[0] == 0u);
    CHECK(intro->palettes.shown.rgb[1] == 10u);
    CHECK(intro->palettes.shown.rgb[2] == 63u);
    CHECK(intro->tick == 15u);   /* 63 steps at a tick per four */
    {
        unsigned int i;

        for (i = 0; i < YT2_PALETTE_SIZE; i++) {
            if (intro->palettes.staging.rgb[i] != 0u) {
                break;
            }
        }
        CHECK(i == YT2_PALETTE_SIZE);
    }

    /* Mode 5: down to the target, no early exit (dedup drops the
       converged passes). */
    memset(intro->palettes.work.rgb, 9u, YT2_PALETTE_SIZE);
    memset(intro->palettes.target.rgb, 7u, YT2_PALETTE_SIZE);
    seen->frames = 0;
    yt2_intro_fade(intro, 5u, 63u, 1u, 0u);
    CHECK(intro->palettes.work.rgb[0] == 7u);
    CHECK(intro->palettes.shown.rgb[0] == 7u);
    CHECK(seen->frames == 2u);

    /* Ranges stay inside [first, first + entries). */
    memset(intro->palettes.shown.rgb, 5u, YT2_PALETTE_SIZE);
    seen->frames = 0;
    yt2_intro_fade(intro, 3u, 63u, 1u, 100u);
    CHECK(intro->palettes.shown.rgb[99u * 3u] == 5u);
    CHECK(intro->palettes.shown.rgb[100u * 3u] == 0u);
    CHECK(intro->palettes.shown.rgb[101u * 3u] == 5u);
}

/* The sprite animation state machine (sub_05185, no pixels asserted). */
static void sprite_vectors(yt2_intro *intro)
{
    /* Wrap-around animation (flags 0xC080: increment, wrap to min). */
    memset(intro->sprites, 0, sizeof(intro->sprites));
    intro->sprites[0].flags = 0xC080u;
    intro->sprites[0].pic_min = 26u;
    intro->sprites[0].pic_max = 31u;
    intro->sprites[0].klass = 0x60u;
    intro->sprites[0].x = 72u;
    intro->sprites[0].clip_w = 56u;
    intro->sprites[0].y = 94u;
    intro->sprites[0].clip_h = 47u;
    intro->sprites[0].pic = 30u;
    intro->frame_event = 1;
    yt2_intro_compose(intro);
    CHECK(intro->sprites[0].pic == 31u);
    intro->frame_event = 1;
    yt2_intro_compose(intro);
    CHECK(intro->sprites[0].pic == 26u);
    /* Without the event the picture holds. */
    yt2_intro_compose(intro);
    CHECK(intro->sprites[0].pic == 26u);

    /* One-shot animation (flags 0x60: the overflowed step is dropped
       and the animate bit clears, keeping the last in-range picture). */
    intro->sprites[1].flags = 0xC060u;
    intro->sprites[1].pic_min = 23u;
    intro->sprites[1].pic_max = 25u;
    intro->sprites[1].y = 10u;
    intro->sprites[1].clip_h = 10u;
    intro->sprites[1].pic = 25u;
    intro->frame_event = 1;
    yt2_intro_compose(intro);
    CHECK(intro->sprites[1].pic == 25u);
    CHECK((intro->sprites[1].flags & 0x4000u) == 0u);

    /* Entries fully above the screen deactivate. */
    intro->sprites[2].flags = 0x8000u;
    intro->sprites[2].y = 10u;
    intro->sprites[2].clip_h = 5u;
    intro->view_y = 20;
    yt2_intro_compose(intro);
    CHECK((intro->sprites[2].flags & 0x8000u) == 0u);
    intro->view_y = 0;

    /*
     * The WAIT15 down-count stop (flags 0xC020) mirrors the one-shot up
     * stop: the underflowed step is dropped and e4 rests at pic_min with
     * the animate bit cleared.
     */
    intro->sprites[3].flags = 0xC020u;
    intro->sprites[3].pic_min = 10u;
    intro->sprites[3].pic_max = 15u;
    intro->sprites[3].y = 10u;
    intro->sprites[3].clip_h = 10u;
    intro->sprites[3].pic = 15u;
    for (unsigned int step = 0; step < 15u; step++) {
        intro->frame_event = 1;
        yt2_intro_compose(intro);
    }
    CHECK(intro->sprites[3].pic == 10u);
    CHECK((intro->sprites[3].flags & 0x4000u) == 0u);
    intro->view_y = 0;
}

/* A wait is N event/compose pairs ending one tick past the reload. */
static void wait_vectors(yt2_intro *intro, capture *seen)
{
    unsigned long tick0;

    memset(intro->sprites, 0, sizeof(intro->sprites));
    intro->sprites[0].flags = 0xC080u;
    intro->sprites[0].pic_min = 26u;
    intro->sprites[0].pic_max = 31u;
    intro->sprites[0].klass = 0x60u;
    intro->sprites[0].x = 72u;
    intro->sprites[0].clip_w = 56u;
    intro->sprites[0].y = 94u;
    intro->sprites[0].clip_h = 47u;
    intro->sprites[0].pic = 26u;
    intro->countdown_a_on = 1;
    intro->frame_div = 5u;
    intro->frame_count = 5u;
    intro->frame_event = 0;
    tick0 = intro->tick;
    seen->frames = 0;
    yt2_intro_wait_frames(intro, 5u);
    CHECK(intro->tick - tick0 == 26u);   /* 6 + 4 * 5 */
    CHECK(intro->frame_count == 4u);
    CHECK(intro->frame_event == 0);
    CHECK(intro->sprites[0].pic == 31u);
    CHECK(seen->frames == 5u);
}

typedef struct menu_probe {
    capture seen;
    int armed;
} menu_probe;

static void poll_quit(yt2_intro_input *input, void *user)
{
    menu_probe *probe = user;

    /* Persistent: fades poll too (early-out checks) and must not
       consume the only quit. */
    if (probe->armed) {
        input->have_key = 1;
        input->key = 0x11u;
    }
}

static int probe_emit(const uint8_t *frame, const uint8_t *palette,
                      unsigned long tick, void *user)
{
    return capture_emit(frame, palette, tick, &((menu_probe *)user)->seen);
}

typedef struct reshow_probe {
    capture seen;
    yt2_intro *intro;
    int polls;
    int want_reshow;
} reshow_probe;

static void poll_reshow(yt2_intro_input *input, void *user)
{
    reshow_probe *probe = user;

    probe->polls++;
    /* Fades poll too (early-out checks): reshow lands late, in the
       steady menu, and quit persists (a one-shot would be eaten). */
    if (probe->want_reshow && probe->polls == 140 &&
        probe->intro != NULL) {
        probe->intro->reshow = 1;
    }
    if (probe->polls >= 145) {
        input->have_key = 1;
        input->key = 0x11u;
    }
}

static int reshow_emit(const uint8_t *frame, const uint8_t *palette,
                       unsigned long tick, void *user)
{
    return capture_emit(frame, palette, tick, &((reshow_probe *)user)->seen);
}

/* Menu reshow runs: identical late-quit runs with and without a
   mid-menu keyframe request; returns the emission count. */
static unsigned int run_menu_reshow(yt2_pictures *pictures, yt2_world *world,
                                    int want_reshow)
{
    yt2_intro intro_menu;
    yt2_titlemenu titlemenu;
    reshow_probe probe;
    unsigned int frames = 0u;

    memset(&probe, 0, sizeof(probe));
    probe.want_reshow = want_reshow;
    if (yt2_intro_init(&intro_menu, pictures, world, reshow_emit,
                       poll_reshow, &probe) != YT2_STATUS_OK) {
        return 0u;
    }
    probe.intro = &intro_menu;
    if (yt2_titlemenu_init(&titlemenu, &intro_menu, 0u) != YT2_STATUS_OK) {
        yt2_intro_free(&intro_menu);
        return 0u;
    }
    if (yt2_titlemenu_run(&titlemenu) != YT2_INTRO_TRANSITION_QUIT) {
        yt2_intro_free(&intro_menu);
        return 0u;
    }
    frames = probe.seen.frames;
    yt2_intro_free(&intro_menu);
    return frames;
}

/* Skip probes: ESC (0x1B) reported persistently, or never. */
typedef struct esc_probe {
    capture seen;
    int esc;
} esc_probe;

static void poll_skip(yt2_intro_input *input, void *user)
{
    esc_probe *probe = user;

    if (probe->esc) {
        input->have_key = 1;
        input->key = 0x1Bu;
    }
}

static int skip_emit(const uint8_t *frame, const uint8_t *palette,
                     unsigned long tick, void *user)
{
    return capture_emit(frame, palette, tick, &((esc_probe *)user)->seen);
}

int test_intro(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_intro intro;
    capture seen;

    memset(&seen, 0, sizeof(seen));
    if (pictures_path == NULL || world_path == NULL) {
        fprintf(stderr, "test_intro: need YT2_PICTURES and YT2_WORLD\n");
        return 0;
    }
    if (yt2_pictures_load(pictures_path, &pictures) != YT2_STATUS_OK ||
        yt2_world_load(world_path, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "test_intro: cannot load data\n");
        return 0;
    }
    if (yt2_intro_init(&intro, &pictures, &world, capture_emit, NULL,
                       &seen) != YT2_STATUS_OK) {
        fprintf(stderr, "test_intro: cannot init\n");
        yt2_world_free(&world);
        yt2_pictures_free(&pictures);
        return 0;
    }
    fade_vectors(&intro, &seen);
    sprite_vectors(&intro);
    wait_vectors(&intro, &seen);

    /* The pixel save/plot round trip. */
    yt2_intro_clear(&intro, YT2_INTRO_DRAW_WORK);
    intro.work.pixels[8u * 320u + 268u] = 0x41u;
    yt2_intro_pixel_plot(&intro, 268, 8, 0x1Fu);
    CHECK(intro.work.pixels[8u * 320u + 268u] == 0x1Fu);
    yt2_intro_pixel_restore(&intro, 268, 8);
    CHECK(intro.work.pixels[8u * 320u + 268u] == 0x41u);

    /* A blit + flush reproduces the pinned Webfoot composition. */
    yt2_intro_clear(&intro, YT2_INTRO_DRAW_WORK);
    CHECK(yt2_intro_blit(&intro, YT2_INTRO_DRAW_WORK, 0x00u, 16u, 1, 1, 0u,
                         0u, 0u, 0u, 0u) == YT2_STATUS_OK);
    yt2_intro_flush(&intro);
    CHECK(checksum(seen.last_frame, sizeof(seen.last_frame)) ==
          UINT64_C(0xC5B1519EF4C1133A));
    CHECK(yt2_intro_blit(&intro, YT2_INTRO_DRAW_WORK, 0xF0u, 0u, 0, 0, 0u,
                         0u, 0u, 0u, 0u) == YT2_STATUS_ERROR);

    /* A story text block reproduces the pinned sleep screen. */
    yt2_intro_clear(&intro, YT2_INTRO_DRAW_WORK);
    yt2_intro_text(&intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[1], 2u,
                   10, 174, 0x93u, 0x98u);
    yt2_intro_flush(&intro);
    CHECK(checksum(seen.last_frame, sizeof(seen.last_frame)) ==
          UINT64_C(0xFB816B027A864AD1));

    /* Palette loads come from WORLD.DAT. */
    CHECK(yt2_intro_load_palette(&intro, 0u) == YT2_STATUS_OK);
    CHECK(checksum(intro.palettes.target.rgb, YT2_PALETTE_SIZE) ==
          UINT64_C(0xE3275EE3AE459FB5));
    CHECK(yt2_intro_load_palette(&intro, 99u) == YT2_STATUS_ERROR);

    /* Emission dedups identical frames. */
    seen.frames = 0;
    yt2_intro_flush(&intro);
    yt2_intro_flush(&intro);
    CHECK(seen.frames == 0u);

    /* The WORLD.DAT LOGO.PCX embed decodes to B1's live VBE frame. */
    {
        const uint8_t *file;
        size_t size;
        static uint8_t indices[640u * 480u];
        static uint8_t palette[768u];
        unsigned int w = 0u;
        unsigned int h = 0u;

        CHECK(yt2_world_logo(&world, &file, &size) == YT2_STATUS_OK);
        CHECK(yt2_pcx_decode(file, size, indices, sizeof(indices), palette,
                             &w, &h) == YT2_STATUS_OK);
        CHECK(w == 640u && h == 480u);
        CHECK(checksum(indices, sizeof(indices)) ==
              UINT64_C(0xAFDB05A705155977));
        CHECK(checksum(palette, sizeof(palette)) ==
              UINT64_C(0x3852AFFCE35D8B61));
        CHECK(yt2_pcx_decode(file, 100u, indices, sizeof(indices), palette,
                             &w, &h) == YT2_STATUS_ERROR);
    }

    /* The menu answers ^Q with the quit transition and a music stop. */
    {
        yt2_intro intro_menu;
        yt2_titlemenu titlemenu;
        menu_probe probe;
        unsigned int stops = 0;

        memset(&probe, 0, sizeof(probe));
        probe.armed = 1;
        CHECK(yt2_intro_init(&intro_menu, &pictures, &world, probe_emit,
                             poll_quit, &probe) == YT2_STATUS_OK);
        CHECK(yt2_titlemenu_init(&titlemenu, &intro_menu, 0u) == YT2_STATUS_OK);
        CHECK(yt2_titlemenu_run(&titlemenu) == YT2_INTRO_TRANSITION_QUIT);
        for (unsigned int i = 0; i < intro_menu.audio_count; i++) {
            if (intro_menu.audio[i].op == YT2_INTRO_AUDIO_MUSIC_STOP) {
                stops++;
            }
        }
        CHECK(stops == 1u);
        CHECK(probe.seen.frames > 0u);
        yt2_intro_free(&intro_menu);
    }

    /* An idle menu answers a keyframe request with exactly one
       extra emission (late joiners would stare at black else). */
    {
        unsigned int plain = run_menu_reshow(&pictures, &world, 0);
        unsigned int reshow = run_menu_reshow(&pictures, &world, 1);

        CHECK(plain > 0u);
        CHECK(reshow == plain + 1u);
    }

    /* Long fades and frame waits abort promptly on ESC (a tap
       anywhere skips within a pass, not at the next phase). */
    {
        yt2_intro fade_plain;
        yt2_intro fade_esc;
        esc_probe quiet;
        esc_probe loud;
        unsigned long tick_plain;
        unsigned long tick_esc;

        memset(&quiet, 0, sizeof(quiet));
        memset(&loud, 0, sizeof(loud));
        loud.esc = 1;
        CHECK(yt2_intro_init(&fade_plain, &pictures, &world, skip_emit,
                             poll_skip, &quiet) == YT2_STATUS_OK);
        CHECK(yt2_intro_init(&fade_esc, &pictures, &world, skip_emit,
                             poll_skip, &loud) == YT2_STATUS_OK);
        fade_plain.countdown_a_on = 1;
        fade_esc.countdown_a_on = 1;
        memset(fade_plain.palettes.shown.rgb, 63u, YT2_PALETTE_SIZE);
        memset(fade_esc.palettes.shown.rgb, 63u, YT2_PALETTE_SIZE);
        yt2_intro_fade(&fade_plain, 0u, 63u, 256u, 0u);
        yt2_intro_fade(&fade_esc, 0u, 63u, 256u, 0u);
        CHECK(quiet.seen.frames > 50u);
        CHECK(loud.seen.frames < 5u);
        tick_plain = fade_plain.tick;
        tick_esc = fade_esc.tick;
        yt2_intro_wait_frames(&fade_plain, 45u);
        yt2_intro_wait_frames(&fade_esc, 45u);
        CHECK(fade_plain.tick - tick_plain > 200u);
        CHECK(fade_esc.tick - tick_esc < 20u);
        yt2_intro_free(&fade_plain);
        yt2_intro_free(&fade_esc);
    }

    yt2_intro_free(&intro);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return test_failures == 0;
}
