#ifndef YT2_INTRO_H
#define YT2_INTRO_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The scripted start-up sequence: VBE publisher logo, Webfoot logo, story
 * cutscene, attract pictures and the main menu (game `fn_11B00` with the
 * `fn_15DC2` LOGO.PCX show, `fn_050C4`, `fn_0617A` and `fn_0D36C`).
 *
 * The engine is a straight-line transcription of the original's scripts:
 * every wait, fade step, blit and input poll below names its game function.
 * Time is the BIOS tick (18.2 Hz); the frame event (`v_505E` bit 400h) fires
 * every `frame_div` ticks (5 during the cutscene, 3 in the menu).  Every fade
 * step emits a frame; steps that share a tick stamp (full-speed fades) flush
 * instantly when the player paces by the stamp.  The gate matches B1's
 * throttle-sampled fade frames as a subsequence of the port's full steps.
 *
 * Audio has no source yet (M9): music/SFX calls append to the trigger log
 * with the tick they fire on.
 */

#define YT2_INTRO_WIDTH 320u
#define YT2_INTRO_HEIGHT 200u
#define YT2_INTRO_VBE_WIDTH 640u
#define YT2_INTRO_VBE_HEIGHT 480u
#define YT2_INTRO_SPRITES 7u
#define YT2_INTRO_AUDIO_LOG 64u

/* One sprite-table entry (DGROUP:6D66h, nine words, stride 20h). */
typedef struct yt2_intro_sprite {
    uint16_t flags;             /* +0: 8000h active, 4000h animate */
    uint16_t pic_min;           /* +2: animation range, low */
    uint16_t pic_max;           /* +4: animation range, high */
    uint16_t klass;             /* +6: picture class << 4 (v_0CCC) */
    uint16_t x;                 /* +8: screen x before the viewport */
    uint16_t clip_w;            /* +0Ah: visible width */
    uint16_t y;                 /* +0Ch: screen y before the viewport */
    uint16_t clip_h;            /* +0Eh: visible height */
    uint16_t pic;               /* +10h: current picture (v_0CCA) */
} yt2_intro_sprite;

/* The four palette buffers the fade engine works with. */
typedef struct yt2_intro_palettes {
    yt2_palette shown;          /* 4124h: what the DAC shows */
    yt2_palette target;         /* 4424h: what fades head for */
    yt2_palette staging;        /* 4754h: fade-up scratch */
    yt2_palette work;           /* 4D56h: fade-down scratch */
} yt2_intro_palettes;

typedef enum yt2_intro_draw_dest {
    YT2_INTRO_DRAW_WORK = 0,    /* v_WorkSeg: the composed screen */
    YT2_INTRO_DRAW_BG = 1       /* v_50E2: the scrolling background */
} yt2_intro_draw_dest;

typedef enum yt2_intro_audio_op {
    YT2_INTRO_AUDIO_MUSIC = 0,
    YT2_INTRO_AUDIO_SFX = 1,
    YT2_INTRO_AUDIO_MUSIC_STOP = 2,
    YT2_INTRO_AUDIO_MUSIC_SERVICE = 3
} yt2_intro_audio_op;

typedef struct yt2_intro_audio_event {
    unsigned long tick;
    yt2_intro_audio_op op;
    unsigned int id;
} yt2_intro_audio_event;

/* A polled input snapshot.  Key codes are game key codes (1Bh = ESC). */
typedef struct yt2_intro_input {
    int have_key;
    uint8_t key;
    int left_pressed;
    int mouse_x;
    int mouse_y;
} yt2_intro_input;

/*
 * Callbacks.  `emit` receives one composed frame (320x200 indices plus the
 * 768 DAC bytes) with the tick it presents on; return non-zero to stop the
 * run.  `poll` fills the newest input (all fields zero/negative when idle).
 */
typedef int (*yt2_intro_emit_fn)(const uint8_t *frame, const uint8_t *palette,
                                 unsigned long tick, void *user);
typedef void (*yt2_intro_poll_fn)(yt2_intro_input *input, void *user);
/*
 * The VBE logo show: 640x480 indices plus the 768 6-bit DAC bytes the
 * game shrinks the PCX palette to.  Return non-zero to stop the run.
 */
typedef int (*yt2_intro_vbe_show_fn)(const uint8_t *indices,
                                    const uint8_t *palette,
                                    unsigned long tick, void *user);

typedef struct yt2_intro {
    const yt2_pictures *pictures;
    const yt2_world *world;
    yt2_surface work;           /* v_WorkSeg */
    /*
     * What the last flush showed (the VGA frame: work plus any cursor
     * bytes).  Fades emit this, not the work surface, so a cursor drawn
     * by one flush persists until the next flush overwrites it.
     */
    uint8_t presented[YT2_INTRO_WIDTH * YT2_INTRO_HEIGHT];
    uint8_t *bg;                /* v_50E2 scroll buffer, rows of 320 */
    unsigned int bg_rows;       /* rows allocated */
    unsigned int bg_off;        /* rows scrolled (v_50E2 bumps) */
    yt2_intro_sprite sprites[YT2_INTRO_SPRITES];
    int view_x;                 /* v_0B9C */
    int view_y;                 /* v_0BA0 */
    yt2_intro_palettes palettes;
    /*
     * The emitted DAC.  B1 mirrors every DAC byte into 4124h (`shown`
     * above), so the two only differ right after a mode set (which loads
     * the default palette without touching the fade source).
     */
    yt2_palette dac;
    int retrace_wait;           /* v_5062 bit 800h */
    unsigned long tick;         /* consumed BIOS ticks */
    unsigned int frame_div;     /* v_94D7: ticks per frame event */
    unsigned int frame_count;   /* v_50E8 countdown */
    int frame_event;            /* v_505E bit 400h */
    int countdown_a_on;         /* v_50F6 bit 8000h */
    unsigned int tick_wait;     /* v_50F2 accumulator */
    uint8_t pixel_saved;        /* v_0B1C: byte under the flying pixel */
    unsigned int fade_steps;    /* retraced steps since the last tick */
    int flag_w;                 /* /W: the Webfoot screen + short first wait */
    int flag_p;                 /* /P: skip intro, story and attract */
    int sfx_on;                 /* v_SoundState bit 8 */
    int engine_cursor;          /* composite the cursor on menu flushes */
    yt2_intro_audio_event audio[YT2_INTRO_AUDIO_LOG];
    unsigned int audio_count;
    yt2_intro_emit_fn emit;
    yt2_intro_poll_fn poll;
    void *user;
    /* May be NULL: without it the VBE window stays a black beat. */
    yt2_intro_vbe_show_fn vbe_show;
    uint8_t *vbe_frame;
    yt2_palette vbe_pal;
    /*
     * The live link joins at the end of the file, so a client attaching
     * mid-hold misses the logo show; it asks for a keyframe instead and
     * the tool raises this to re-show the current logo (same bytes).
     * Headless runs never set it.
     */
    int reshow;
    /*
     * Suppress emission (palette/state still update): the scroll-cut
     * fades run silently to their exact end state, then emit once.
     */
    int quiet;
    int stop;
    /*
     * Set while a checkpoint poll runs: the queue should report ESC if
     * one is pending but leave every other key (and clicks) queued, DOS
     * typeahead style, so a menu key pressed during the cutscene still
     * lands in the menu.  Queues that consume unconditionally (the
     * headless tool) ignore this.
     */
    int peek_only;
    /*
     * Latched ESC skip: one tap skips every remaining phase (B1's
     * sticky KeyCode cascades the same way). Set by the skip poll,
     * consumed when the menu is reached.
     */
    int esc_hit;
    int have_last;              /* emission dedup (B1 presents only changes) */
    uint8_t last_frame[YT2_INTRO_WIDTH * YT2_INTRO_HEIGHT];
    uint8_t last_palette[YT2_PALETTE_SIZE];
} yt2_intro;

int yt2_intro_init(yt2_intro *intro, const yt2_pictures *pictures,
                   const yt2_world *world, yt2_intro_emit_fn emit,
                   yt2_intro_poll_fn poll, void *user);
void yt2_intro_free(yt2_intro *intro);

/* Load WORLD.DAT palette `index` as the fade target (sub_11CE3/fn_1821A). */
int yt2_intro_load_palette(yt2_intro *intro, unsigned int index);

/* Blit class `klass` (v_0CCC, class << 4) picture `pic` (fn_19E12 modes 0/1/4;
   `clip_w`/`clip_h`/`src_x`/`src_y` are the mode-4 window, ignored else). */
int yt2_intro_blit(yt2_intro *intro, yt2_intro_draw_dest dest,
                   unsigned int klass, unsigned int pic, int x, int y,
                   unsigned int mode, unsigned int src_x, unsigned int src_y,
                   unsigned int clip_w, unsigned int clip_h);

/* Clear a destination (sub_05177) or one rectangle of it (rep stosw). */
void yt2_intro_clear(yt2_intro *intro, yt2_intro_draw_dest dest);
void yt2_intro_clear_rect(yt2_intro *intro, yt2_intro_draw_dest dest,
                          unsigned int offset, unsigned int words);

/* One palette-fade run (fn_PaletteFade modes 0..5). */
void yt2_intro_fade(yt2_intro *intro, unsigned int mode,
                    unsigned int passes, unsigned int entries,
                    unsigned int first);
void yt2_intro_fade_out(yt2_intro *intro);      /* fn_FadeOut */
void yt2_intro_fade_in(yt2_intro *intro);       /* fn_FadeIn */
/*
 * Adopt the harness's live screen (320x200 indices + 768 DAC bytes) as
 * the presented frame and DAC mirror, and drop the emission dedup.
 * Sessions present outside the intro, so without this a menu re-entry
 * fade-out would darken a stale intro frame instead of the outgoing
 * screen. The caller refreshes this at every menu entry.
 */
void yt2_intro_adopt_screen(yt2_intro *intro, const uint8_t *pixels,
                            const uint8_t *palette);

/* One scratch fade-up step over `entries` at `first` (sub_05FED, bx = 1). */
void yt2_intro_fade_step(yt2_intro *intro, unsigned int entries,
                         unsigned int first);

/*
 * Compose background + sprites, flush, wait one tick (sub_05185).  Returns
 * non-zero when the flush emitted (deduped identical frames return zero).
 */
int yt2_intro_compose(yt2_intro *intro);

/*
 * Flush the work surface (fn_1A6B5, without the mouse cursor).  Returns
 * non-zero when it emitted.
 */
int yt2_intro_flush(yt2_intro *intro);

/*
 * Flush with cursor `index` composited at (`x`, `y`) (fn_1A6B5 with
 * v_MouseState bit 2); the work surface keeps no cursor bytes.  Honours
 * `engine_cursor`: with it clear this is a plain flush (live play lets the
 * client draw the pointer).
 */
void yt2_intro_flush_cursor(yt2_intro *intro, unsigned int index, int x,
                            int y);

/*
 * Draw a shadowed text block (sub_060E7): `count` NUL-separated lines at
 * (`x`, `y`) stepping six pixels, shadow `shadow` at (x,y), body `body` at
 * (x-1,y-1).  Returns the end of the last line.
 */
const char *yt2_intro_text(yt2_intro *intro, yt2_intro_draw_dest dest,
                           const char *lines, unsigned int count, int x,
                           int y, uint8_t shadow, uint8_t body);

/* Waits: sub_06097 (compose per frame event), sub_06086/fn_062DD (quiet). */
void yt2_intro_wait_frames(yt2_intro *intro, unsigned int count);
void yt2_intro_wait_frames_quiet(yt2_intro *intro, unsigned int count);

/* Wait `count` BIOS ticks with no emission (the deadline spins). */
void yt2_intro_wait_ticks(yt2_intro *intro, unsigned int count);

/*
 * Poll input like the cutscene wait points (sub_050DF): a fresh sample,
 * non-zero when this sample is ESC.  Like the original's key-code slot,
 * nothing sticks between polls: phases report ESC through their return
 * value and the caller decides whether to skip ahead.
 */
int yt2_intro_poll_esc(yt2_intro *intro);

/* The flying-pixel save/plot pair (sub_060A5/sub_060B6). */
void yt2_intro_pixel_restore(yt2_intro *intro, int x, int y);
void yt2_intro_pixel_plot(yt2_intro *intro, int x, int y, uint8_t color);

/* Audio triggers (fn_PlayMusic/fn_PlaySFX/fn_MusicStop/fn_MusicService). */
void yt2_intro_music(yt2_intro *intro, unsigned int id);
void yt2_intro_sfx(yt2_intro *intro, unsigned int id);
void yt2_intro_music_stop(yt2_intro *intro);
void yt2_intro_music_service(yt2_intro *intro);

/*
 * The sequences.  Each runs to its ESC/quit point; the start-up flow and
 * the menu loop live in `yt2/titlemenu.h`.  Menu selections that need unported
 * slices (create, load, embark, rules) return as a transition instead of
 * running them.
 */
typedef enum yt2_intro_transition {
    YT2_INTRO_TRANSITION_NONE = 0,
    YT2_INTRO_TRANSITION_QUIT = 1,      /* ^Q */
    YT2_INTRO_TRANSITION_CREATE = 2,    /* C: fn_13EC4 (yt2_create) */
    YT2_INTRO_TRANSITION_LOAD = 3,      /* A: fn_1C354 (yt2_roster) */
    YT2_INTRO_TRANSITION_EMBARK = 4,    /* E with a party, unported */
    YT2_INTRO_TRANSITION_RULES = 5      /* R: fn_01488, unported */
} yt2_intro_transition;

void yt2_intro_story_setup(yt2_intro *intro);   /* sub_05D5B */
int yt2_intro_story_flags(yt2_intro *intro);    /* sub_052BF, 0 on ESC */
int yt2_intro_story_scroll(yt2_intro *intro);   /* sub_054B1, 0 on ESC */
int yt2_intro_attract(yt2_intro *intro);        /* fn_050C4, 0 on ESC */
void yt2_intro_gate_star(yt2_intro *intro, unsigned int hold); /* fn_0617A */
int yt2_intro_logo(yt2_intro *intro);           /* fn_11B00, 0 on ESC */

/* The nine story text blocks (DGROUP:BFD9h..C2F1h), NUL-separated lines. */
extern const char *const yt2_intro_story_text[9];

#endif /* YT2_INTRO_H */
