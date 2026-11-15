#include "yt2/intro.h"

#include "yt2/cursor.h"
#include "yt2/pcx.h"
#include "yt2/text.h"

#include <stdlib.h>
#include <string.h>

#define INTRO_FRAME_SIZE (YT2_INTRO_WIDTH * YT2_INTRO_HEIGHT)
#define INTRO_BG_ROWS 404u
#define INTRO_CLASS_SHIFT 4u

const char *const yt2_intro_story_text[9] = {
    /* DGROUP:BFD9h, five lines (sub_054B1). */
    "   IT HAS BEEN ONLY A FEW SHORT MONTHS SINCE YOUR\0"
    "TREK THROUGH THE LAND OF YENDOR.  YOUR TRAVELS\0"
    "NOW TAKE YOU TO THE SOUTHERN PENINSULA, KNOWN AS\0"
    "PORT HOPE, WHERE YOU ARE STAYING AS THE HONORED\0"
    "GUESTS OF THE GOVERNOR.",
    /* DGROUP:C0B3h, two lines at (10,174): the story-sleep screen. */
    "NIGHT HAS FALLEN, AND AS YOU SLEEP, YOU DREAM OF\0"
    "YOUR RECENT ADVENTURE... ",
    /* DGROUP:C0FEh, two lines at (34,174): the story-knock screen. */
    "A LOUD KNOCK ON THE DOOR STARTLES YOU FROM\0"
    "YOUR SLEEP.",
    /* DGROUP:C135h, two lines. */
    "\"ONCE ENCHANTED, THIS ORB WILL VANQUISH ALL EVIL\0"
    "CREATURES FROM THE LAND.\"",
    /* DGROUP:C180h, one line. */
    "A SINISTER LAUGH IS HEARD IN THE DISTANCE...",
    /* DGROUP:C1ADh, three lines. */
    "\"ZAMORA HAS FALLEN!\"\0"
    "              \"SUMMON THE HEALERS!\"\0"
    "                         \"WHO HAS STOLEN THE ORB?\"",
    /* DGROUP:C219h, two lines. */
    "\"YOU MUST VENTURE FORTH AND RECOVER THE ORB\0"
    "OF ZAMORA.\"",
    /* DGROUP:C251h, four lines. */
    "\"BECAUSE PALTIVAR WAS ONCE A MEMBER OF OUR SOCIETY\0"
    "OF WIZARDS, HE CAN NOT BE DESTROYED.  THEREFORE,\0"
    "WE SHALL BANISH HIM FOREVER INTO A SINGLE INSTANT\0"
    "IN TIME.\"",
    /* DGROUP:C2F1h, two lines. */
    "\"I AM SORRY TO HAVE TO DISTURB YOU, BUT THE\0"
    "GOVERNOR REQUESTS YOUR PRESENCE IMMEDIATELY.\""
};

/* DGROUP:901Bh: the white caption sub_052BF draws at (88,191). */
static const char intro_copyright[] = "COPYRIGHT 1996 SW GAMES";

int yt2_intro_init(yt2_intro *intro, const yt2_pictures *pictures,
                   const yt2_world *world, yt2_intro_emit_fn emit,
                   yt2_intro_poll_fn poll, void *user)
{
    if (intro == NULL || pictures == NULL || world == NULL ||
        emit == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(intro, 0, sizeof(*intro));
    intro->pictures = pictures;
    intro->world = world;
    intro->emit = emit;
    intro->poll = poll;
    intro->user = user;
    if (yt2_surface_init(&intro->work, YT2_INTRO_WIDTH,
                         YT2_INTRO_HEIGHT) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    yt2_surface_clear(&intro->work, 0u);
    intro->bg = calloc(INTRO_BG_ROWS, YT2_INTRO_WIDTH);
    if (intro->bg == NULL) {
        yt2_surface_free(&intro->work);
        return YT2_STATUS_ERROR;
    }
    /* The VBE bank assembly area; a short allocation only loses the logo. */
    intro->vbe_frame =
        calloc(YT2_INTRO_VBE_HEIGHT, YT2_INTRO_VBE_WIDTH);
    intro->bg_rows = INTRO_BG_ROWS;
    intro->frame_div = 5u;
    intro->frame_count = 5u;
    /*
     * B1 and CH2.BAT both boot " /W /B".  Music plays; the SFX bank stays
     * off (B1's SoundState reads 8003h: no bit 8), so effect calls are
     * silent and sub_0614B waits its count out.
     */
    intro->flag_w = 1;
    intro->engine_cursor = 1;
    return YT2_STATUS_OK;
}

void yt2_intro_free(yt2_intro *intro)
{
    if (intro == NULL) {
        return;
    }
    free(intro->vbe_frame);
    intro->vbe_frame = NULL;
    free(intro->bg);
    intro->bg = NULL;
    yt2_surface_free(&intro->work);
}

/* One BIOS tick: fold it into the frame-event countdown (fn_TickISR). */
static void intro_tick(yt2_intro *intro)
{
    intro->tick++;
    intro->tick_wait++;
    if (!intro->countdown_a_on) {
        return;
    }
    if (--intro->frame_count == 0u) {
        intro->frame_event = 1;
        intro->frame_count = intro->frame_div;
    }
}

static void intro_advance(yt2_intro *intro, unsigned long ticks)
{
    while (ticks-- > 0u) {
        intro_tick(intro);
    }
}

/* Emit the work surface unless it matches the last emission (B1 dedup). */
static int intro_emit(yt2_intro *intro, int force);

/*
 * fn_WriteDAC: entries [first, first+entries) of `src` reach the DAC, and
 * B1 mirrors every DAC byte back into 4124h (`shown`).
 */
static void intro_dac_write(yt2_intro *intro, const uint8_t *src,
                            unsigned int first, unsigned int entries)
{
    memcpy(intro->dac.rgb + (size_t)first * YT2_PALETTE_ENTRY_SIZE,
           src + (size_t)first * YT2_PALETTE_ENTRY_SIZE,
           (size_t)entries * YT2_PALETTE_ENTRY_SIZE);
    memcpy(intro->palettes.shown.rgb + (size_t)first * YT2_PALETTE_ENTRY_SIZE,
           src + (size_t)first * YT2_PALETTE_ENTRY_SIZE,
           (size_t)entries * YT2_PALETTE_ENTRY_SIZE);
}

/*
 * One fade pass over `shown` towards black (modes 0/3) or towards the
 * target (modes 2/4).  Returns non-zero when any byte moved (the early
 * exit).
 */
static int intro_fade_step_shown(yt2_intro *intro, int up,
                                 unsigned int first, unsigned int entries)
{
    unsigned int offset = first * YT2_PALETTE_ENTRY_SIZE;
    unsigned int count = entries * YT2_PALETTE_ENTRY_SIZE;
    int moved = 0;

    for (unsigned int i = 0; i < count; i++) {
        uint8_t *slot = &intro->palettes.shown.rgb[offset + i];

        if (up) {
            if (*slot < intro->palettes.target.rgb[offset + i]) {
                (*slot)++;
                moved = 1;
            }
        } else if (*slot > 0u) {
            (*slot)--;
            moved = 1;
        }
    }
    return moved;
}

/* One mode-1/mode-5 scratch step (sub_05FED inner pass). */
static void intro_fade_step_scratch(uint8_t *scratch, const uint8_t *target,
                                    uint8_t *staging, int up,
                                    unsigned int first, unsigned int entries)
{
    unsigned int offset = first * YT2_PALETTE_ENTRY_SIZE;
    unsigned int count = entries * YT2_PALETTE_ENTRY_SIZE;

    for (unsigned int i = 0; i < count; i++) {
        if (up) {
            uint8_t value = scratch[offset + i];

            if ((value & 0x80u) == 0u && value == target[offset + i]) {
                continue;
            }
            value++;
            scratch[offset + i] = value;
            if ((value & 0x80u) == 0u) {
                staging[offset + i] = value;
            }
        } else if (scratch[offset + i] != target[offset + i]) {
            scratch[offset + i]--;
        }
    }
}

void yt2_intro_fade(yt2_intro *intro, unsigned int mode,
                    unsigned int passes, unsigned int entries,
                    unsigned int first)
{
    unsigned int offset;
    unsigned int count;
    unsigned int pass;

    if (first + entries > YT2_PALETTE_ENTRY_COUNT) {
        return;
    }
    offset = first * YT2_PALETTE_ENTRY_SIZE;
    count = entries * YT2_PALETTE_ENTRY_SIZE;
    if (mode == 0u || mode == 2u) {
        /* Show 4124h first (a no-op: the DAC already mirrors it). */
        intro_dac_write(intro, intro->palettes.shown.rgb, first, entries);
        intro_emit(intro, 0);
    } else if (mode == 1u) {
        /* Fill the work buffer from the target (target - 63, wrapping). */
        for (unsigned int i = 0; i < count; i++) {
            intro->palettes.work.rgb[offset + i] =
                (uint8_t)(intro->palettes.target.rgb[offset + i] - 63u);
        }
    }
    for (pass = 0; pass < passes; pass++) {
        switch (mode) {
        case 0u:
        case 3u:
            if (!intro_fade_step_shown(intro, 0, first, entries)) {
                goto done;
            }
            intro_dac_write(intro, intro->palettes.shown.rgb, first,
                            entries);
            break;
        case 2u:
        case 4u:
            if (!intro_fade_step_shown(intro, 1, first, entries)) {
                goto done;
            }
            intro_dac_write(intro, intro->palettes.shown.rgb, first,
                            entries);
            break;
        case 1u:
            intro_fade_step_scratch(intro->palettes.work.rgb,
                                    intro->palettes.target.rgb,
                                    intro->palettes.staging.rgb, 1, first,
                                    entries);
            intro_dac_write(intro, intro->palettes.staging.rgb, first,
                            entries);
            break;
        case 5u:
            intro_fade_step_scratch(intro->palettes.work.rgb,
                                    intro->palettes.target.rgb, NULL, 0,
                                    first, entries);
            intro_dac_write(intro, intro->palettes.work.rgb, first,
                            entries);
            break;
        default:
            goto done;
        }
        /*
         * Every step emits (the B1 throttle samples these full-speed
         * passes, so the port shows the whole sequence and lets the gate
         * match B1's samples as a subsequence).  The player paces by the
         * tick stamp, so full-speed steps flush instantly.
         */
        intro_emit(intro, 0);
        if (intro->retrace_wait && ++intro->fade_steps % 4u == 0u) {
            intro_advance(intro, 1u);
        }
        if (intro->stop) {
            goto done;
        }
    }
done:
    if (mode == 1u) {
        memset(intro->palettes.staging.rgb, 0, YT2_PALETTE_SIZE);
    }
}

void yt2_intro_fade_out(yt2_intro *intro)
{
    yt2_intro_fade(intro, 0u, 63u, 256u, 0u);
}

void yt2_intro_fade_in(yt2_intro *intro)
{
    yt2_intro_fade(intro, 1u, 63u, 256u, 0u);
}

void yt2_intro_adopt_screen(yt2_intro *intro, const uint8_t *pixels,
                            const uint8_t *palette)
{
    if (intro == NULL || pixels == NULL || palette == NULL) {
        return;
    }
    memcpy(intro->presented, pixels, INTRO_FRAME_SIZE);
    memcpy(intro->palettes.shown.rgb, palette, YT2_PALETTE_SIZE);
    memcpy(intro->dac.rgb, palette, YT2_PALETTE_SIZE);
    intro->have_last = 0;
}

void yt2_intro_fade_step(yt2_intro *intro, unsigned int entries,
                         unsigned int first)
{
    if (first + entries > YT2_PALETTE_ENTRY_COUNT) {
        return;
    }
    intro_fade_step_scratch(intro->palettes.work.rgb,
                            intro->palettes.target.rgb,
                            intro->palettes.staging.rgb, 1, first, entries);
    intro_dac_write(intro, intro->palettes.staging.rgb, first, entries);
    intro_emit(intro, 0);
    if (intro->retrace_wait && ++intro->fade_steps % 4u == 0u) {
        intro_advance(intro, 1u);
    }
}

int yt2_intro_flush(yt2_intro *intro)
{
    memcpy(intro->presented, intro->work.pixels, INTRO_FRAME_SIZE);
    return intro_emit(intro, 0);
}

void yt2_intro_flush_cursor(yt2_intro *intro, unsigned int index, int x,
                            int y)
{
    yt2_surface view;

    memcpy(intro->presented, intro->work.pixels, INTRO_FRAME_SIZE);
    if (intro->engine_cursor) {
        view.pixels = intro->presented;
        view.width = YT2_INTRO_WIDTH;
        view.height = YT2_INTRO_HEIGHT;
        view.stride = YT2_INTRO_WIDTH;
        yt2_cursor_draw(&view, intro->pictures, index, x, y);
    }
    intro_emit(intro, 0);
}

/* One sprite-table animation step (sub_05185, flags bit 4000h). */
static void intro_sprite_animate(yt2_intro_sprite *sprite)
{
    if ((sprite->flags & 0xC0u) == 0u) {
        unsigned int pic = (unsigned int)(sprite->pic - 1u) & 0xFFFFu;

        if ((int)pic >= (int)sprite->pic_min) {
            sprite->pic = (uint16_t)pic;
            return;
        }
    } else {
        unsigned int pic = (unsigned int)(sprite->pic + 1u) & 0xFFFFu;

        if ((int)pic <= (int)sprite->pic_max) {
            sprite->pic = (uint16_t)pic;
            return;
        }
    }
    if ((sprite->flags & 0x60u) != 0u) {
        sprite->flags &= (uint16_t)~0x4060u;
    } else if ((sprite->flags & 0x80u) != 0u) {
        sprite->pic = sprite->pic_min;
    }
}

int yt2_intro_compose(yt2_intro *intro)
{
    int event = intro->frame_event;
    int emitted;



    intro->tick_wait = 0u;
    memcpy(intro->work.pixels,
           intro->bg + (size_t)intro->bg_off * YT2_INTRO_WIDTH,
           INTRO_FRAME_SIZE);
    for (unsigned int i = 0; i < YT2_INTRO_SPRITES; i++) {
        yt2_intro_sprite *sprite = &intro->sprites[i];
        int x;
        int y;

        if ((sprite->flags & 0x8000u) == 0u) {
            continue;
        }
        x = (int)sprite->x - intro->view_x;
        y = (int)sprite->y - intro->view_y;
        if (y > 199) {
            continue;
        }
        if (y + (int)sprite->clip_h <= 0) {
            sprite->flags &= (uint16_t)~0x8000u;
            continue;
        }
        if (y < 0) {
            unsigned int skip = (unsigned int)(intro->view_y - (int)sprite->y);

            yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, sprite->klass,
                           sprite->pic, x, 0, 4u, 0u, skip, sprite->clip_w,
                           sprite->clip_h);
        } else if (y + (int)sprite->clip_h > 199) {
            yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, sprite->klass,
                           sprite->pic, x, y, 4u, 0u, 0u, sprite->clip_w,
                           (unsigned int)(200 - y));
        } else {
            yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, sprite->klass,
                           sprite->pic, x, y, 1u, 0u, 0u, 0u, 0u);
        }
        if ((sprite->flags & 0x4000u) != 0u && event) {
            intro_sprite_animate(sprite);
        }
    }
    intro->frame_event = 0;
    emitted = yt2_intro_flush(intro);
    /* The v_50F2 spin: the compose takes one tick. */
    intro_advance(intro, 1u);
    return emitted;
}

const char *yt2_intro_text(yt2_intro *intro, yt2_intro_draw_dest dest,
                           const char *lines, unsigned int count, int x,
                           int y, uint8_t shadow, uint8_t body)
{
    yt2_surface view;
    yt2_surface *target = &intro->work;
    const char *line = lines;

    if (dest == YT2_INTRO_DRAW_BG) {
        view.pixels = intro->bg + (size_t)intro->bg_off * YT2_INTRO_WIDTH;
        view.width = YT2_INTRO_WIDTH;
        view.height = intro->bg_rows - intro->bg_off;
        view.stride = YT2_INTRO_WIDTH;
        target = &view;
    }
    for (unsigned int pass = 0; pass < 2u; pass++) {
        const char *row = lines;
        int ox = pass == 0u ? x : x - 1;
        int oy = pass == 0u ? y : y - 1;
        uint8_t color = pass == 0u ? shadow : body;

        for (unsigned int i = 0; i < count; i++) {
            yt2_text_draw(target, ox, oy + (int)i * 6, row, color);
            row += strlen(row) + 1u;
        }
    }
    for (unsigned int i = 0; i < count; i++) {
        line += strlen(line) + 1u;
    }
    return line;
}

/* Spin until the next frame event (sub_06086/sub_06097).  The event survives
   for the compose to see (sub_06097); quiet waits clear it themselves. */
static void intro_wait_event(yt2_intro *intro)
{
    while (!intro->frame_event && !intro->stop) {
        intro_advance(intro, 1u);
    }
}

/*
 * A wait is N event/compose pairs.  Each pair leaves the countdown one
 * tick past its reload (the fire resets it to frame_div, the compose's
 * tick burns one), so every fade loop after a wait starts from the same
 * leftover with no seeding.
 */
void yt2_intro_wait_frames(yt2_intro *intro, unsigned int count)
{
    for (unsigned int i = 0; i < count && !intro->stop; i++) {
        intro_wait_event(intro);
        if (intro->stop) {
            return;
        }
        yt2_intro_compose(intro);
    }
}

void yt2_intro_wait_frames_quiet(yt2_intro *intro, unsigned int count)
{
    for (unsigned int i = 0; i < count && !intro->stop; i++) {
        intro_wait_event(intro);
        if (intro->stop) {
            return;
        }
        intro->frame_event = 0;
    }
}

void yt2_intro_wait_ticks(yt2_intro *intro, unsigned int count)
{
    intro_advance(intro, count);
}

/*
 * The fn_11B00 deadline loops (loc_11B4D/11BE6/11C58): burn `count` ticks
 * polling ESC once per tick, non-zero when ESC (or stop) cut the wait.
 * The music driver keeps the original waiting while a jingle plays; with
 * no audio engine yet the port always runs the hold.
 */
static int intro_wait_ticks_esc(yt2_intro *intro, unsigned int count)
{
    for (unsigned int i = 0; i < count && !intro->stop; i++) {
        if (yt2_intro_poll_esc(intro)) {
            return 1;
        }
        intro_advance(intro, 1u);
    }
    return 0;
}

int yt2_intro_poll_esc(yt2_intro *intro)
{
    if (intro->poll != NULL) {
        yt2_intro_input input;
        int esc;

        memset(&input, 0, sizeof(input));
        input.mouse_x = -1;
        input.mouse_y = -1;
        intro->peek_only = 1;
        intro->poll(&input, intro->user);
        intro->peek_only = 0;
        esc = input.have_key && input.key == 0x1Bu;
        if (esc) {
            return 1;
        }
    }
    return 0;
}

void yt2_intro_pixel_restore(yt2_intro *intro, int x, int y)
{
    if (x < 0 || x >= (int)YT2_INTRO_WIDTH || y < 0 ||
        y >= (int)YT2_INTRO_HEIGHT) {
        return;
    }
    intro->work.pixels[(size_t)y * YT2_INTRO_WIDTH + (size_t)x] =
        intro->pixel_saved;
}

void yt2_intro_pixel_plot(yt2_intro *intro, int x, int y, uint8_t color)
{
    if (x < 0 || x >= (int)YT2_INTRO_WIDTH || y < 0 ||
        y >= (int)YT2_INTRO_HEIGHT) {
        return;
    }
    intro->pixel_saved =
        intro->work.pixels[(size_t)y * YT2_INTRO_WIDTH + (size_t)x];
    intro->work.pixels[(size_t)y * YT2_INTRO_WIDTH + (size_t)x] = color;
}

static void intro_audio(yt2_intro *intro, yt2_intro_audio_op op,
                        unsigned int id)
{
    if (intro->audio_count >= YT2_INTRO_AUDIO_LOG) {
        return;
    }
    intro->audio[intro->audio_count].tick = intro->tick;
    intro->audio[intro->audio_count].op = op;
    intro->audio[intro->audio_count].id = id;
    intro->audio_count++;
}

void yt2_intro_music(yt2_intro *intro, unsigned int id)
{
    intro_audio(intro, YT2_INTRO_AUDIO_MUSIC, id);
}

void yt2_intro_sfx(yt2_intro *intro, unsigned int id)
{
    if (intro->sfx_on) {
        intro_audio(intro, YT2_INTRO_AUDIO_SFX, id);
    }
}

void yt2_intro_music_stop(yt2_intro *intro)
{
    intro_audio(intro, YT2_INTRO_AUDIO_MUSIC_STOP, 0u);
}

void yt2_intro_music_service(yt2_intro *intro)
{
    intro_audio(intro, YT2_INTRO_AUDIO_MUSIC_SERVICE, 0u);
}

/* ------------------------------------------------------------------------ */
/* The scripts                                                               */
/* ------------------------------------------------------------------------ */

/* sub_060E7's gated effect call (ax = 0xFFFF means none). */
static void intro_text_sfx(yt2_intro *intro, unsigned int id)
{
    if (id != 0xFFFFu) {
        yt2_intro_sfx(intro, id);
    }
}

/*
 * sub_0614B: with the SFX bank off this waits the count out; a count of
 * 0xFFFF never waits.  (With the bank on it would spin for the effect tail;
 * M9 owns effect durations.)
 */
static void intro_sound_wait(yt2_intro *intro, unsigned int count)
{
    if (!intro->sfx_on && count != 0xFFFFu) {
        yt2_intro_wait_frames_quiet(intro, count);
    }
}

/* sub_0615B: stage the whole DAC range to 3Fh grey and show it (the flash). */
static void intro_flash_grey(yt2_intro *intro)
{
    memset(intro->palettes.work.rgb, 0x3Fu, YT2_PALETTE_SIZE);
    intro_dac_write(intro, intro->palettes.work.rgb, 0u, 255u);
    intro_emit(intro, 0);
}

/* A writable 320-wide view of the background at the scroll offset. */
static yt2_surface intro_bg_view(yt2_intro *intro)
{
    yt2_surface view;

    view.pixels = intro->bg + (size_t)intro->bg_off * YT2_INTRO_WIDTH;
    view.width = YT2_INTRO_WIDTH;
    view.height = intro->bg_rows - intro->bg_off;
    view.stride = YT2_INTRO_WIDTH;
    return view;
}

static void intro_set_sprite(yt2_intro *intro, unsigned int slot,
                             uint16_t flags, uint16_t min, uint16_t max,
                             uint16_t klass, uint16_t x, uint16_t clip_w,
                             uint16_t y, uint16_t clip_h, uint16_t pic)
{
    yt2_intro_sprite *sprite = &intro->sprites[slot];

    sprite->flags = flags;
    sprite->pic_min = min;
    sprite->pic_max = max;
    sprite->klass = klass;
    sprite->x = x;
    sprite->clip_w = clip_w;
    sprite->y = y;
    sprite->clip_h = clip_h;
    sprite->pic = pic;
}

void yt2_intro_story_setup(yt2_intro *intro)
{
    unsigned int pic;

    /* v_0C2C reads idle here (B1 dumps 0 at tick 250): no stop, no wait. */
    intro->bg_off = 0u;
    yt2_intro_clear(intro, YT2_INTRO_DRAW_BG);
    intro->bg_off = 196u;
    yt2_intro_blit(intro, YT2_INTRO_DRAW_BG, 0x00u, 6u, 1, 0, 0u, 0u, 0u, 0u,
                   0u);
    intro->bg_off = 0u;
    yt2_intro_blit(intro, YT2_INTRO_DRAW_BG, 0x00u, 5u, 1, 0, 0u, 0u, 0u, 0u,
                   0u);
    intro->retrace_wait = 0;
    yt2_intro_load_palette(intro, 3u);
    yt2_intro_fade_out(intro);
    /* Cache warmers: group 2 then group 6 at (0,0); the last one wins and
       the flush below shows it until the first compose restores the sky. */
    for (pic = 10u; pic <= 18u; pic++) {
        yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x20u, pic, 0, 0, 0u, 0u,
                       0u, 0u, 0u);
    }
    for (pic = 23u; pic <= 31u; pic++) {
        yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x60u, pic, 0, 0, 0u, 0u,
                       0u, 0u, 0u);
    }
    intro->view_x = 0;
    intro->view_y = 0;
    intro_set_sprite(intro, 0u, 0xC080u, 26u, 31u, 0x60u, 72u, 56u,
                     94u, 47u, 26u);
    intro_set_sprite(intro, 1u, 0xC080u, 26u, 31u, 0x60u, 240u, 56u,
                     94u, 47u, 28u);
    intro_set_sprite(intro, 2u, 0xC080u, 23u, 25u, 0x60u, 68u, 56u,
                     237u, 136u, 25u);
    intro_set_sprite(intro, 3u, 0xC080u, 23u, 25u, 0x60u, 236u, 56u,
                     237u, 136u, 23u);
    intro_set_sprite(intro, 4u, 0x8040u, 10u, 15u, 0x20u, 105u, 140u,
                     242u, 151u, 10u);
    intro_set_sprite(intro, 5u, 0x0040u, 16u, 19u, 0x20u, 180u, 140u,
                     48u, 155u, 16u);
    intro_set_sprite(intro, 6u, 0x0040u, 73u, 78u, 0x10u, 86u, 210u,
                     46u, 105u, 73u);
    yt2_intro_compose(intro);
    intro->countdown_a_on = 1;
    intro->frame_event = 0;
}

int yt2_intro_story_flags(yt2_intro *intro)
{
    unsigned int i;
    int x;
    int y;
    unsigned int color;

    /* The dark copy sub_052BF starts from (target - 63, wrapping). */
    for (i = 0; i < YT2_PALETTE_SIZE; i++) {
        intro->palettes.work.rgb[i] =
            (uint8_t)(intro->palettes.target.rgb[i] - 63u);
    }
    yt2_intro_music(intro, 18u);
    intro->retrace_wait = 1;
    yt2_intro_flush(intro);
    for (i = 0; i < 63u && !intro->stop; i++) {
        yt2_intro_fade_step(intro, 0x40u, 0u);
        yt2_intro_fade_step(intro, 0x7Fu, 0x80u);
        if (intro->frame_event) {
            yt2_intro_compose(intro);
        }
    }
    memset(intro->palettes.staging.rgb, 0, YT2_PALETTE_SIZE);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_wait_frames(intro, 5u);
    /*
     * The shooting star: (268,8) down-left in steps of (-2,+2), bright
     * white for 33 plots then fading out over 20.  The original plots
     * and flushes every step with no waits, so on this boundary the
     * whole flight lands on one tick stamp and plays as an instant
     * burst; reimagined as four plots per tick instead (~0.7 s across,
     * every one of the 53 plots kept).  The background holds its pose
     * meanwhile (frame events are swallowed, never composed), matching
     * the fast machine's frames exactly, and ESC cuts the flight short.
     */
    {
        x = 268u;
        y = 8;
        color = 0x1Fu;
        yt2_intro_pixel_plot(intro, x, y, 0x1Fu);
        yt2_intro_flush(intro);
        yt2_intro_wait_ticks(intro, 1u);
        intro->frame_event = 0;
        for (i = 0; i < 32u && !intro->stop;) {
            unsigned int step;

            for (step = 0; step < 4u && i < 32u; step++, i++) {
                yt2_intro_pixel_restore(intro, x, y);
                x -= 2;
                y += 2;
                yt2_intro_pixel_plot(intro, x, y, 0x1Fu);
                yt2_intro_flush(intro);
            }
            if (yt2_intro_poll_esc(intro) || intro->stop) {
                yt2_intro_pixel_restore(intro, x, y);
                return 0;
            }
            yt2_intro_wait_ticks(intro, 1u);
            intro->frame_event = 0;
        }
        for (i = 0; i < 20u && !intro->stop;) {
            unsigned int step;

            for (step = 0; step < 4u && i < 20u; step++, i++) {
                yt2_intro_pixel_restore(intro, x, y);
                x -= 2;
                y += 2;
                color = (color - 1u) & 0xFFFFu;
                yt2_intro_pixel_plot(intro, x, y,
                                     (uint8_t)(color & 0xFFu));
                yt2_intro_flush(intro);
            }
            if (yt2_intro_poll_esc(intro) || intro->stop) {
                yt2_intro_pixel_restore(intro, x, y);
                return 0;
            }
            yt2_intro_wait_ticks(intro, 1u);
            intro->frame_event = 0;
        }
        yt2_intro_pixel_restore(intro, x, y);
    }
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_wait_frames(intro, 10u);
    for (i = 0; i < 63u && !intro->stop; i++) {
        yt2_intro_fade(intro, 4u, 1u, 0x10u, 0x40u);
        if (intro->frame_event) {
            yt2_intro_compose(intro);
        }
    }
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_wait_frames(intro, 10u);
    {
        yt2_surface view = intro_bg_view(intro);

        yt2_text_draw(&view, 88, 191, intro_copyright, 0xFFu);
    }
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    for (i = 0; i < 63u && !intro->stop; i++) {
        yt2_intro_fade(intro, 4u, 1u, 0x30u, 0x50u);
        yt2_intro_fade(intro, 4u, 1u, 1u, 0xFFu);
        if (intro->frame_event) {
            yt2_intro_compose(intro);
        }
    }
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_wait_frames(intro, 20u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    for (i = 0; i < 54u && !intro->stop; i++) {
        yt2_intro_fade(intro, 3u, 1u, 0x10u, 0x40u);
        yt2_intro_fade(intro, 3u, 1u, 0x30u, 0x50u);
        yt2_intro_fade(intro, 3u, 1u, 1u, 0xFFu);
        if (intro->frame_event) {
            yt2_intro_compose(intro);
        }
    }
    yt2_intro_blit(intro, YT2_INTRO_DRAW_BG, 0x00u, 5u, 1, 0, 0u, 0u, 0u, 0u,
                   0u);
    return !intro->stop;
}

/*
 * The story sweep at two rows per frame (the loc_054C0/054DD/05505 loops
 * step one row; the vanilla 55/62-step fades between the row groups all
 * land on two tick stamps, so live they strobe as 0.1 s flickers between
 * smooth pans - the longstanding chop).  The pan here runs straight
 * through at a uniform frame per tick instead, with each inter-group
 * fade replaced by one palette cut: the fade runs silently (exact end
 * state, no ticks) and a single flush shows it.
 */
static void intro_scroll_fast(yt2_intro *intro, unsigned int count)
{
    unsigned int i = 0u;

    while (i < count && !intro->stop) {
        unsigned int step = (count - i >= 2u) ? 2u : 1u;

        intro->bg_off += step;
        intro->view_y += (int)step;
        /* Uniform beats: re-emit identical holds instead of dropping a tick. */
        if (!yt2_intro_compose(intro)) {
            intro_emit(intro, 1);
        }
        i += step;
    }
}

static void intro_fade_63(yt2_intro *intro, unsigned int mode,
                          unsigned int first);

static void intro_scroll_cut(yt2_intro *intro, int second)
{
    intro->quiet = 1;
    intro->retrace_wait = 0;
    if (second) {
        yt2_intro_fade(intro, 1u, 63u, 0x30u, 0x50u);
    } else {
        intro_fade_63(intro, 4u, 0x40u);
    }
    intro->quiet = 0;
    yt2_intro_flush(intro);
}

/* sub_06070/sub_0607B: 63 single fade-down/up steps over 16 entries. */
static void intro_fade_63(yt2_intro *intro, unsigned int mode,
                          unsigned int first)
{
    yt2_intro_fade(intro, mode, 63u, 0x10u, first);
}

int yt2_intro_story_scroll(yt2_intro *intro)
{
    unsigned int i;

    intro->view_x = 0;
    intro->view_y = 0;
    /*
     * The vanilla groups (0x1F/0x27/0x80) repartitioned to even counts:
     * a lone single-row step among two-row steps reads as a half-speed
     * hitch, so rows1 borrows one row from rows2 (same 198-row journey,
     * cuts land one row off - invisible).
     */
    intro_scroll_fast(intro, 32u);
    intro_scroll_cut(intro, 0);
    intro_scroll_fast(intro, 38u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_scroll_cut(intro, 1);
    intro_scroll_fast(intro, 128u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->sprites[4].flags |= 0x4000u;
    yt2_intro_wait_frames(intro, 7u);
    yt2_intro_fade(intro, 0u, 63u, 6u, 0x93u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_BG, yt2_intro_story_text[0], 5u,
                   10u, 8, 0x93u, 0x98u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 1;
    for (i = 0; i < 63u && !intro->stop; i++) {
        yt2_intro_fade(intro, 4u, 1u, 0x10u, 0x93u);
        if (intro->frame_event) {
            yt2_intro_compose(intro);
        }
    }
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_wait_frames(intro, 45u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->sprites[4].flags |= 0x4020u;
    yt2_intro_wait_frames(intro, 15u);
    for (i = 0; i < 63u && !intro->stop; i++) {
        yt2_intro_fade(intro, 3u, 1u, 0xFFu, 0u);
        if (intro->frame_event) {
            yt2_intro_compose(intro);
        }
    }
    for (i = 0; i < YT2_INTRO_SPRITES; i++) {
        intro->sprites[i].flags &= 0x3FFFu;
    }
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 0;
    yt2_intro_load_palette(intro, 0u);
    yt2_intro_fade_out(intro);
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[1], 2u,
                   10u, 174, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_wait_frames_quiet(intro, 25u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 3u, 0x90u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x10u, 39u, 55, 47, 1u,
                   0u, 0u, 0u, 0u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0xC0u);
    intro_sound_wait(intro, 0xFFFFu);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_text_sfx(intro, 16u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[3], 2u,
                   10u, 8, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0xB0u);
    intro_sound_wait(intro, 30u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_sfx(intro, 17u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 3u, 0xB0u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 3u, 0x90u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x10u, 40u, 55, 47, 1u,
                   0u, 0u, 0u, 0u);
    intro_sound_wait(intro, 0xFFFFu);
    intro_flash_grey(intro);
    yt2_intro_sfx(intro, 18u);
    yt2_intro_flush(intro);
    yt2_intro_fade(intro, 5u, 63u, 0xFFu, 0u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_sound_wait(intro, 0xFFFFu);
    yt2_intro_sfx(intro, 19u);
    intro->retrace_wait = 0;
    /* sub_060DA/sub_060CD set the retrace wait back on themselves. */
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    intro_fade_63(intro, 3u, 0x90u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_text_sfx(intro, 19u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[4], 1u,
                   28u, 8, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    yt2_intro_clear_rect(intro, YT2_INTRO_DRAW_WORK, 0x8C0u, 0x640u);
    intro_sound_wait(intro, 15u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 3u, 0x90u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 1;
    intro_text_sfx(intro, 23u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[5], 3u,
                   10u, 8, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    intro_sound_wait(intro, 0xFFFFu);
    yt2_intro_sfx(intro, 24u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_sound_wait(intro, 25u);
    yt2_intro_sfx(intro, 25u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 3u, 0x90u);
    intro->retrace_wait = 0;
    yt2_intro_fade(intro, 0u, 63u, 0x60u, 0u);
    yt2_intro_fade(intro, 3u, 63u, 0x40u, 0x80u);
    yt2_intro_fade(intro, 3u, 63u, 0x30u, 0xD0u);
    yt2_intro_clear_rect(intro, YT2_INTRO_DRAW_WORK, 0x8C0u, 0xD20u);
    intro_sound_wait(intro, 0xFFFFu);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 1;
    intro_text_sfx(intro, 20u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[6], 2u,
                   28u, 8, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    intro_sound_wait(intro, 20u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 3u, 0x90u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_clear_rect(intro, YT2_INTRO_DRAW_WORK, 0x8C0u, 0x8C0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 175u, 142, 1, 1u, 0u,
                   0u, 0u, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 176u, 50, 23, 1u,
                   0u, 0u, 0u, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 177u, 236, 23, 1u,
                   0u, 0u, 0u, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 178u, 1, 82, 1u, 0u,
                   0u, 0u, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 179u, 286, 82, 1u,
                   0u, 0u, 0u, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 180u, 50, 143, 1u,
                   0u, 0u, 0u, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 181u, 236, 143, 1u,
                   0u, 0u, 0u, 0u);
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x70u, 182u, 142, 162, 1u,
                   0u, 0u, 0u, 0u);
    yt2_intro_flush(intro);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 1;
    yt2_intro_fade(intro, 4u, 63u, 0x20u, 0xA0u);
    intro_fade_63(intro, 4u, 0x30u);
    intro->retrace_wait = 0;
    intro_fade_63(intro, 4u, 0x10u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x50u);
    intro_fade_63(intro, 4u, 0x40u);
    intro->retrace_wait = 0;
    intro_fade_63(intro, 4u, 0x90u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x20u);
    intro_fade_63(intro, 4u, 0x80u);
    /* Countdown D is on for this block but rotates zeros over black. */
    intro_fade_63(intro, 4u, 0xD0u);
    intro_fade_63(intro, 4u, 0u);
    intro_fade_63(intro, 3u, 0xC0u);
    intro_fade_63(intro, 3u, 0x60u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_flash_grey(intro);
    intro_sound_wait(intro, 0xFFFFu);
    yt2_intro_sfx(intro, 18u);
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x10u, 39u, 55, 47, 1u,
                   0u, 0u, 0u, 0u);
    for (i = 0; i < 42u; i++) {
        yt2_intro_clear_rect(intro, YT2_INTRO_DRAW_WORK,
                             0x6753u + i * 0x140u, 0x1Du);
    }
    yt2_intro_flush(intro);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_fade(intro, 5u, 63u, 0xFFu, 0u);
    yt2_intro_wait_frames_quiet(intro, 10u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 0;
    yt2_intro_fade(intro, 0u, 63u, 0xC0u, 0u);
    yt2_intro_fade(intro, 3u, 63u, 0x30u, 0xD0u);
    intro_text_sfx(intro, 21u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[7], 4u,
                   10u, 8, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    intro->bg_off = 0u;
    memcpy(intro->bg, intro->work.pixels, INTRO_FRAME_SIZE);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->view_x = 0;
    intro->view_y = 0;
    intro->sprites[5].flags |= 0xC000u;
    yt2_intro_compose(intro);
    intro->retrace_wait = 1;
    yt2_intro_fade(intro, 2u, 63u, 0xC0u, 0u);
    yt2_intro_fade(intro, 2u, 63u, 0x30u, 0xD0u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_sound_wait(intro, 45u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_sfx(intro, 22u);
    intro_sound_wait(intro, 0xFFFFu);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    if (!intro->sfx_on) {
        yt2_intro_clear_rect(intro, YT2_INTRO_DRAW_BG, 0x8C0u, 0x12C0u);
    }
    yt2_intro_wait_frames(intro, 3u);
    yt2_intro_sfx(intro, 26u);
    yt2_intro_wait_frames(intro, 2u);
    yt2_intro_fade(intro, 0u, 63u, 0xC0u, 0u);
    yt2_intro_fade(intro, 3u, 63u, 0x30u, 0xD0u);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    yt2_intro_wait_frames_quiet(intro, 3u);
    intro->sprites[5].flags &= 0x3FFFu;
    intro->retrace_wait = 0;
    yt2_intro_load_palette(intro, 3u);
    yt2_intro_fade_out(intro);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_sound_wait(intro, 0xFFFFu);
    yt2_intro_sfx(intro, 27u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[2], 2u,
                   34u, 174, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_wait_frames_quiet(intro, 20u);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 3u, 0x90u);
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_sfx(intro, 27u);
    intro->sprites[6].flags |= 0xC000u;
    yt2_intro_compose(intro);
    intro->bg_off = 0u;
    memcpy(intro->bg, intro->work.pixels, INTRO_FRAME_SIZE);
    yt2_intro_fade(intro, 1u, 63u, 0x100u, 0u);
    yt2_intro_wait_frames_quiet(intro, 1u);
    intro_sound_wait(intro, 0xFFFFu);
    yt2_intro_sfx(intro, 5u);
    yt2_intro_wait_frames(intro, 5u);
    intro_sound_wait(intro, 0xFFFFu);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro_text_sfx(intro, 28u);
    yt2_intro_text(intro, YT2_INTRO_DRAW_WORK, yt2_intro_story_text[8], 2u,
                   34u, 8, 0x93u, 0x98u);
    yt2_intro_flush(intro);
    intro->retrace_wait = 1;
    intro_fade_63(intro, 4u, 0x90u);
    yt2_intro_wait_frames_quiet(intro, 20u);
    return !intro->stop;
}

int yt2_intro_attract(yt2_intro *intro)
{
    int esc;

    yt2_intro_story_setup(intro);
    esc = yt2_intro_poll_esc(intro) || intro->stop;
    if (!esc && !yt2_intro_story_flags(intro)) {
        esc = 1;
    }
    if (!esc && !yt2_intro_story_scroll(intro)) {
        esc = 1;
    }
    /* sub_050FD: fade out, park the countdown, palette 0, clear screen. */
    yt2_intro_fade_out(intro);
    intro->retrace_wait = 0;
    intro->countdown_a_on = 0;
    intro->frame_event = 0;
    yt2_intro_load_palette(intro, 0u);
    yt2_intro_fade_out(intro);
    yt2_intro_fade_out(intro);
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    /* v_0C2C reads idle here (B1 dumps 0 at tick 2880): music keeps playing. */
    return !esc && !intro->stop;
}

void yt2_intro_gate_star(yt2_intro *intro, unsigned int hold)
{
    unsigned int pic;

    /* sub_06280: retrace on, fade out, menu music service, palette 0. */
    intro->retrace_wait = 1;
    yt2_intro_fade_out(intro);
    /* v_0C2C reads idle (B1 dumps 0 at tick 2880): no music stop. */
    yt2_intro_music_service(intro);
    yt2_intro_load_palette(intro, 0u);
    yt2_intro_fade_out(intro);
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    intro->countdown_a_on = 1;
    for (pic = 17u; pic <= 22u && !intro->stop; pic++) {
        unsigned int wait;
        int esc = 0;

        yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x00u, pic, 1, 0, 0u, 0u,
                       0u, 0u, 0u);
        yt2_intro_flush(intro);
        yt2_intro_fade_in(intro);
        for (wait = 0; wait < hold && !intro->stop; wait++) {
            if (yt2_intro_poll_esc(intro)) {
                esc = 1;
                break;
            }
            yt2_intro_wait_frames_quiet(intro, 1u);
        }
        yt2_intro_fade_out(intro);
        if (esc || intro->stop) {
            break;
        }
    }
    /* sub_06208: fade out, clear, retrace off.  (fn_00000 never returns 2,
       so the palette-7 coda below it is dead.) */
    yt2_intro_fade_out(intro);
    yt2_intro_clear(intro, YT2_INTRO_DRAW_WORK);
    intro->retrace_wait = 0;
}

/* B1's mode-13h default palette (index >> 2 grey); the real BIOS differs.
   The set clears the screen and loads the DAC without touching 4124h (or
   the work surface, which keeps whatever the last draws left). */
static void intro_mode_13h(yt2_intro *intro)
{
    unsigned int i;

    memset(intro->presented, 0, INTRO_FRAME_SIZE);
    for (i = 0; i < YT2_PALETTE_ENTRY_COUNT; i++) {
        intro->dac.rgb[i * 3u] = (uint8_t)(i >> 2);
        intro->dac.rgb[i * 3u + 1u] = (uint8_t)(i >> 2);
        intro->dac.rgb[i * 3u + 2u] = (uint8_t)(i >> 2);
    }
    intro_emit(intro, 0);
}

/*
 * fn_15DC2's show without the temp file: decode the WORLD.DAT LOGO.PCX
 * embed straight into the VBE bank assembly area (the original spills it
 * through LOGO.PCX on disk and deletes it right after; same bytes, no FS
 * litter).  The 8-bit PCX palette shrinks to the 6-bit DAC like the
 * game's shr loop, mirrored into 4124h the way B1 does.  Returns 0
 * without a show path, a short buffer, or a bad file.
 */
static int intro_logo_show(yt2_intro *intro)
{
    const uint8_t *file;
    size_t size;
    uint8_t pal8[YT2_PALETTE_SIZE];
    unsigned int w;
    unsigned int h;
    unsigned int i;

    if (intro->vbe_show == NULL || intro->vbe_frame == NULL ||
        yt2_world_logo(intro->world, &file, &size) != YT2_STATUS_OK ||
        yt2_pcx_decode(file, size, intro->vbe_frame,
                       (size_t)YT2_INTRO_VBE_WIDTH * YT2_INTRO_VBE_HEIGHT,
                       pal8, &w, &h) != YT2_STATUS_OK ||
        w != YT2_INTRO_VBE_WIDTH || h != YT2_INTRO_VBE_HEIGHT) {
        return 0;
    }
    for (i = 0; i < YT2_PALETTE_SIZE; i++) {
        intro->vbe_pal.rgb[i] = (uint8_t)(pal8[i] >> 2);
    }
    /* B1's decode runs ~23 ticks before the palette lands and the logo
       shows; the CPU phase takes no input. */
    yt2_intro_wait_ticks(intro, 23u);
    memcpy(intro->dac.rgb, intro->vbe_pal.rgb, YT2_PALETTE_SIZE);
    memcpy(intro->palettes.shown.rgb, intro->vbe_pal.rgb,
           YT2_PALETTE_SIZE);
    if (intro->vbe_show(intro->vbe_frame, intro->vbe_pal.rgb, intro->tick,
                        intro->user) != 0) {
        intro->stop = 1;
    }
    return 1;
}


int yt2_intro_logo(yt2_intro *intro)
{
    /*
     * fn_15DC2: the VBE mode set blanks the screen (B1's bank-0 leak is
     * not reproduced), the decoded LOGO.PCX shows at 640x480, and the
     * display hold runs 90 ticks - 63 with /W - polling ESC (loc_11B4D:
     * ESC stops the music and ends the logo outright).
     */
    memset(intro->presented, 0, INTRO_FRAME_SIZE);
    intro_emit(intro, 1);
    if (intro_logo_show(intro)) {
        unsigned int hold = intro->flag_w ? 63u : 90u;
        unsigned int i;

        yt2_intro_music(intro, 3u);
        intro->reshow = 0;
        for (i = 0; i < hold && !intro->stop; i++) {
            if (yt2_intro_poll_esc(intro)) {
                yt2_intro_music_stop(intro);
                return 0;
            }
            if (intro->reshow) {
                intro->reshow = 0;
                if (intro->vbe_show(intro->vbe_frame,
                                    intro->vbe_pal.rgb, intro->tick,
                                    intro->user) != 0) {
                    intro->stop = 1;
                }
            }
            intro_advance(intro, 1u);
        }
    } else {
        /* No show path or a bad embed: a short black beat instead. */
        memset(intro->dac.rgb, 0, YT2_PALETTE_SIZE);
        memset(intro->palettes.shown.rgb, 0, YT2_PALETTE_SIZE);
        yt2_intro_music(intro, 3u);
        if (intro_wait_ticks_esc(intro, 10u)) {
            yt2_intro_music_stop(intro);
            return 0;
        }
    }
    if (intro->stop) {
        return 0;
    }
    intro_mode_13h(intro);
    if (intro->flag_w) {
        yt2_intro_load_palette(intro, 8u);
        yt2_intro_fade_out(intro);
        if (yt2_intro_poll_esc(intro) || intro->stop) {
            return 0;
        }
        yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x00u, 16u, 1, 1, 0u, 0u,
                       0u, 0u, 0u);
        yt2_intro_flush(intro);
        if (yt2_intro_poll_esc(intro) || intro->stop) {
            return 0;
        }
        intro->retrace_wait = 1;
        yt2_intro_fade_in(intro);
        /* loc_11BE6: the Webfoot hold polls ESC the same way. */
        if (intro_wait_ticks_esc(intro, 63u)) {
            yt2_intro_music_stop(intro);
            return 0;
        }
        if (intro->stop) {
            return 0;
        }
    }
    yt2_intro_load_palette(intro, 1u);
    yt2_intro_fade_out(intro);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    yt2_intro_blit(intro, YT2_INTRO_DRAW_WORK, 0x00u, 0u, 1, 1, 0u, 0u, 0u,
                   0u, 0u);
    yt2_intro_flush(intro);
    if (yt2_intro_poll_esc(intro) || intro->stop) {
        return 0;
    }
    intro->retrace_wait = 1;
    yt2_intro_fade_in(intro);
    /*
     * loc_11C58: the SW hold the transcription skipped (90 ticks, ESC
     * stops the music and ends the logo).  B1's capture skips it - its
     * sound driver reads idle right there - but on the port the logo
     * holds so it can be seen; with no audio engine the hold always runs.
     */
    if (intro_wait_ticks_esc(intro, 90u)) {
        yt2_intro_music_stop(intro);
        return 0;
    }
    if (intro->stop) {
        return 0;
    }
    yt2_intro_fade_out(intro);
    intro->retrace_wait = 0;
    intro_mode_13h(intro);
    return !intro->stop;
}

/* Emit the presented surface unless it matches the last emission. */
static int intro_emit(yt2_intro *intro, int force)
{
    if (intro->quiet) {
        return 0;
    }
    if (!force && intro->have_last &&
        memcmp(intro->presented, intro->last_frame,
               INTRO_FRAME_SIZE) == 0 &&
        memcmp(intro->dac.rgb, intro->last_palette,
               YT2_PALETTE_SIZE) == 0) {
        return 0;
    }
    memcpy(intro->last_frame, intro->presented, INTRO_FRAME_SIZE);
    memcpy(intro->last_palette, intro->dac.rgb, YT2_PALETTE_SIZE);
    intro->have_last = 1;
    if (intro->emit(intro->presented, intro->dac.rgb, intro->tick,
                    intro->user) != 0) {
        intro->stop = 1;
    }
    return 1;
}

int yt2_intro_load_palette(yt2_intro *intro, unsigned int index)
{
    const yt2_palette *base;

    if (intro == NULL) {
        return YT2_STATUS_ERROR;
    }
    base = yt2_world_palette(intro->world, index);
    if (base == NULL) {
        return YT2_STATUS_ERROR;
    }
    intro->palettes.target = *base;
    return YT2_STATUS_OK;
}

static uint8_t *intro_dest_row(yt2_intro *intro, yt2_intro_draw_dest dest,
                               unsigned int row)
{
    if (dest == YT2_INTRO_DRAW_BG) {
        if (intro->bg_off + row >= intro->bg_rows) {
            return NULL;
        }
        return intro->bg + (size_t)(intro->bg_off + row) * YT2_INTRO_WIDTH;
    }
    if (row >= intro->work.height) {
        return NULL;
    }
    return intro->work.pixels + row * intro->work.stride;
}

int yt2_intro_blit(yt2_intro *intro, yt2_intro_draw_dest dest,
                   unsigned int klass, unsigned int pic, int x, int y,
                   unsigned int mode, unsigned int src_x, unsigned int src_y,
                   unsigned int clip_w, unsigned int clip_h)
{
    const yt2_picture_group *group;
    const uint8_t *image;
    unsigned int width;
    unsigned int height;

    if (intro == NULL) {
        return YT2_STATUS_ERROR;
    }
    group = yt2_pictures_group(intro->pictures, klass >> INTRO_CLASS_SHIFT);
    if (group == NULL) {
        return YT2_STATUS_ERROR;
    }
    image = yt2_picture_data(intro->pictures, klass >> INTRO_CLASS_SHIFT,
                             pic);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    width = group->width;
    height = group->height;
    if (mode == 4u) {
        if (src_x >= width || src_y >= height) {
            return YT2_STATUS_OK;
        }
        image += (size_t)src_y * width + src_x;
        if (clip_w < width - src_x) {
            width = clip_w;
        } else {
            width -= src_x;
        }
        if (clip_h < height - src_y) {
            height = clip_h;
        } else {
            height -= src_y;
        }
    } else if (mode != 0u && mode != 1u) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int row = 0; row < height; row++) {
        uint8_t *out;
        int target_y = y + (int)row;

        if (target_y < 0) {
            continue;
        }
        out = intro_dest_row(intro, dest, (unsigned int)target_y);
        if (out == NULL) {
            break;
        }
        for (unsigned int col = 0; col < width; col++) {
            int target_x = x + (int)col;
            uint8_t pixel = image[(size_t)row * group->width + col];

            if (target_x < 0 || target_x >= (int)YT2_INTRO_WIDTH) {
                continue;
            }
            if (mode != 0u && pixel == 0xFFu) {
                continue;
            }
            out[target_x] = pixel;
        }
    }
    return YT2_STATUS_OK;
}

void yt2_intro_clear(yt2_intro *intro, yt2_intro_draw_dest dest)
{
    if (dest == YT2_INTRO_DRAW_BG) {
        memset(intro->bg, 0,
               (size_t)intro->bg_rows * YT2_INTRO_WIDTH);
    } else {
        yt2_surface_clear(&intro->work, 0u);
    }
}

/* The rep stosw rectangle clears (offset/words in the 320-wide buffer). */
void yt2_intro_clear_rect(yt2_intro *intro, yt2_intro_draw_dest dest,
                          unsigned int offset, unsigned int words)
{
    size_t bytes = (size_t)words * 2u;
    size_t limit;

    if (dest == YT2_INTRO_DRAW_BG) {
        /* Segment-absolute like the original's rep stosw (the scripts only
           clear at scroll offset zero). */
        limit = (size_t)intro->bg_rows * YT2_INTRO_WIDTH;
        if ((size_t)offset >= limit) {
            return;
        }
        if ((size_t)offset + bytes > limit) {
            bytes = limit - (size_t)offset;
        }
        memset(intro->bg + offset, 0, bytes);
    } else {
        limit = INTRO_FRAME_SIZE;
        if ((size_t)offset >= limit) {
            return;
        }
        if ((size_t)offset + bytes > limit) {
            bytes = limit - (size_t)offset;
        }
        memset(intro->work.pixels + offset, 0, bytes);
    }
}
