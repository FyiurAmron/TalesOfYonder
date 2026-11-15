#ifndef YT2_CREATE_H
#define YT2_CREATE_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/items.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * Character creation and the roster (fn_13EC4, fn_1C354, fn_13F2E).
 *
 * The session is event-driven: the caller feeds key presses and clicks, and
 * the session repaints the shared surface after every accepted event, like
 * one fn_1A6B5 present per state change.  Screens are cursor-free; the
 * caller draws the cursor (yt2_cursor_draw) from yt2_create_cursor() when
 * it needs gate parity with the original's software cursor.
 *
 * Offsets, tables, key sets and formulas follow the disassembly verbatim;
 * each stage cites its function.  The only deliberate host differences:
 * the blocking line editor (fn_0D29A) is a keystroke state machine, and
 * the clock-seeded RNG takes its time word from the caller so the gate
 * can pin it (the original reads DOS time when the seed is zero).
 */

#define YT2_CREATE_NAME_MAX 12u /* fn_0D29A with CX=0Dh stores 12, then buzzes */
#define YT2_CREATE_TAKE_MAX 4u  /* v_50DA cap on the items screen */
#define YT2_CREATE_ITEMS 8u
#define YT2_PORTRAIT_FIRST_PIC 19u
#define YT2_CREATE_SFX_LOG 64u
#define YT2_ROSTER_PARTY_MAX YT2_PARTY_ASSIGNED_MAX

/* Cursor ids the session reports (group-8 pictures, fn_SetMouseCursor). */
#define YT2_CREATE_CURSOR_ARROW 0u

typedef enum yt2_create_stage {
    YT2_CREATE_STAGE_CLASS = 0,
    YT2_CREATE_STAGE_PORTRAIT,
    YT2_CREATE_STAGE_SHEET,
    YT2_CREATE_STAGE_ITEMS,
    YT2_CREATE_STAGE_NAME,
    YT2_CREATE_STAGE_PREVIEW,
    YT2_CREATE_STAGE_KEEP,   /* KEEP accepted: caller persists, session ends */
    YT2_CREATE_STAGE_QUIT    /* Q accepted: caller returns to the menu */
} yt2_create_stage;

typedef struct yt2_create {
    yt2_party *party;             /* live roster, caller-owned */
    const yt2_items *items;
    const yt2_pictures *pictures;
    yt2_surface *surface;
    unsigned int slot;            /* v_5070 roster index */
    yt2_create_stage stage;
    int repick;                   /* v_5064 bit 0x8000 (preview re-entry) */
    uint16_t taken;               /* v_505E low bits */
    unsigned int taken_count;     /* v_50DA */
    uint16_t last_returned;       /* v_50DC re-pickup guard */
    uint16_t carried_id;          /* v_40E2 */
    uint16_t carried_count;       /* v_40E6 */
    uint16_t carried_weight;      /* v_40E4 */
    unsigned int cursor;          /* v_MouseCursor group-8 index */
    int cursor_shown;             /* v_MouseState visible bit */
    uint16_t rng_seed;            /* v_F7AC; 0 seeds from time_word */
    uint16_t time_word;           /* pinned DOS time (DX of int 21h AH=2Ch) */
    char name[YT2_CHARACTER_NAME_SIZE];
    unsigned int name_length;
    unsigned int sfx_count;
    unsigned int sfx[YT2_CREATE_SFX_LOG];
    int dirty;                    /* KEEP/wipe happened: caller must persist */
} yt2_create;

/*
 * The eight takeable items (sub_01DFC boot list, fixed for every class),
 * 1-based file ids into the item table.
 */
extern const uint16_t yt2_create_item_ids[YT2_CREATE_ITEMS];

/* First free roster slot (class 0), or -1 when all nine are taken. */
int yt2_create_alloc_slot(const yt2_party *party);

/*
 * Begin creation on the first free slot (sub_1585A): paints the background
 * and title and opens the class screen.  The time word seeds the RNG on
 * the first roll; pass 0x1234 for M5-gate parity, a clock reading live.
 */
int yt2_create_begin(yt2_create *create, yt2_party *party,
                     const yt2_items *items, const yt2_pictures *pictures,
                     yt2_surface *surface, uint16_t time_word);

/*
 * Paint the class screen after begin (sub_146E9).  Split so the caller
 * can present the setup-only frame in between, like the original's
 * KEEP-to-next-character transition.
 */
int yt2_create_open_class(yt2_create *create);

/*
 * Feed one key press (ASCII, uppercased by the caller like v_KeyCode) or
 * one click at surface pixels.  Returns YT2_STATUS_OK and repaints when
 * the event lands; inert events leave the surface alone.
 */
int yt2_create_key(yt2_create *create, unsigned int ascii);
int yt2_create_click(yt2_create *create, int x, int y);
int yt2_create_right_click(yt2_create *create, int x, int y);

/* True once the session reached KEEP or QUIT. */
int yt2_create_done(const yt2_create *create);

/* Current cursor image and SFX log for the caller/gate. */
unsigned int yt2_create_cursor(const yt2_create *create);

/* True when the cursor composites (name editor and setup hide it). */
int yt2_create_cursor_shown(const yt2_create *create);
unsigned int yt2_create_sfx_count(const yt2_create *create);
unsigned int yt2_create_sfx(const yt2_create *create, unsigned int index);

/*
 * fn_1797C verbatim: uniform-ish draw from [0, max].  A zero seed reads
 * the time word first, exactly like the DOS-time seeding.
 */
uint16_t yt2_create_rand(uint16_t *seed, uint16_t time_word, uint16_t max);

/* fn_15D7C: (value * percent + 50) / 100. */
uint16_t yt2_create_percent(uint16_t value, uint16_t percent);

/* sub_15605 + sub_1426E + fn_0B3D2: roll, derive, apply equipment. */
void yt2_create_roll(yt2_create *create, yt2_character *character);

/* Current rating word for a level-1 character, or NULL above level 1. */
const char *yt2_create_rating(const yt2_character *character);

/* sub_1576C: starting spells for classes 4..9 (KEEP only). */
void yt2_create_grant_spells(yt2_character *character);

/* fn_0B3D2: recompute combat stats from base values and equipment. */
void yt2_create_apply_equipment(yt2_create *create, yt2_character *character);

typedef enum yt2_roster_view {
    YT2_ROSTER_VIEW_GRID = 0,
    YT2_ROSTER_VIEW_SHEET,   /* fn_13F2E character sheet + DELETE/RETURN */
    YT2_ROSTER_VIEW_CONFIRM, /* ARE YOU SURE + YES/NO */
    YT2_ROSTER_VIEW_DONE     /* D accepted: caller returns to the menu */
} yt2_roster_view;

typedef struct yt2_roster {
    yt2_party *party; /* owns assigned[]/active[]; the session keeps no copy */
    const yt2_items *items;
    const yt2_pictures *pictures;
    yt2_surface *surface;
    yt2_roster_view view;
    unsigned int slot; /* fn_13F2E v_5070 index */
    int party_ok;      /* v_505E bit 0x200 after DONE */
    int dirty;
    unsigned int sfx_count;
    unsigned int sfx[YT2_CREATE_SFX_LOG];
} yt2_roster;

/* fn_1C354: paint the grid and open input. */
int yt2_roster_begin(yt2_roster *roster, yt2_party *party,
                     const yt2_items *items, const yt2_pictures *pictures,
                     yt2_surface *surface);

/* ASCII keys and clicks (F1..F9 need yt2_roster_fkey: their scancodes
 * collide with ASCII ";<=>?@ABC", and the original tells them apart by
 * event type). */
int yt2_roster_key(yt2_roster *roster, unsigned int ascii);
int yt2_roster_click(yt2_roster *roster, int x, int y);

/* F-key 1..9 on the grid: view that slot's sheet. */
int yt2_roster_fkey(yt2_roster *roster, unsigned int number);

int yt2_roster_done(const yt2_roster *roster);
unsigned int yt2_roster_sfx_count(const yt2_roster *roster);
unsigned int yt2_roster_sfx(const yt2_roster *roster, unsigned int index);

#endif /* YT2_CREATE_H */
