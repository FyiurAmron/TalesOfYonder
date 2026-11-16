#ifndef YT2_ROOM_H
#define YT2_ROOM_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/palette.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * Exploration slice: dungeon window, first-person room renderer, movement
 * and the game clock.  See docs/structures.md "Exploration".
 *
 * The room owns no file data: the world, pictures and save outlive it.
 * Coordinates are world tiles; the window is a 78x78 cell cache into the
 * 800x144 grid.  Facings are the original bits: 8000h north (-y), 4000h
 * south (+y), 1000h east (+x), 2000h west (-x).
 */

#define YT2_ROOM_WINDOW_SIDE 78u
#define YT2_ROOM_WINDOW_ROW_BYTES 0x270u
#define YT2_ROOM_CELL_SIZE 8u
#define YT2_ROOM_VIEW_COLUMNS 51u
#define YT2_ROOM_OBJECT_SLOTS 80u
#define YT2_ROOM_OBJECT_SIZE 0x9Cu
#define YT2_ROOM_SHADES 7u

#define YT2_ROOM_FACING_NORTH 0x8000u
#define YT2_ROOM_FACING_SOUTH 0x4000u
#define YT2_ROOM_FACING_EAST 0x1000u
#define YT2_ROOM_FACING_WEST 0x2000u

#define YT2_ROOM_BOUND_X0 0x28u
#define YT2_ROOM_BOUND_X1 0x2F7u
#define YT2_ROOM_BOUND_Y0 0x18u
#define YT2_ROOM_BOUND_Y1 0x77u

/* IBM set-1 scancodes of the movement keys. */
#define YT2_ROOM_SCAN_UP 0x48u
#define YT2_ROOM_SCAN_DOWN 0x50u
#define YT2_ROOM_SCAN_LEFT 0x4Bu
#define YT2_ROOM_SCAN_RIGHT 0x4Du
#define YT2_ROOM_SCAN_STRAFE 0x73u

/* Walkable-with-animation terrain range (v_5146/v_5144). */
#define YT2_ROOM_ANIM_TERRAIN_LO 6u
#define YT2_ROOM_ANIM_TERRAIN_HI 11u

/* Occlusion solid range (v_5134/v_5132). */
#define YT2_ROOM_SOLID_TERRAIN_LO 2u
#define YT2_ROOM_SOLID_TERRAIN_HI 5u

/* Clock countdown reloads and M5-pinned embark schedule. */
#define YT2_ROOM_CLOCK_RELOAD 91u
#define YT2_ROOM_ROTATION_RELOAD 5u
#define YT2_ROOM_GATE_CLOCK 0x4000u
#define YT2_ROOM_GATE_SKY 0x2000u
#define YT2_ROOM_GATE_ROTATION 0x1000u
#define YT2_ROOM_EMBARK_SCENE 0x1A4u
/* Live E0-FF window at embark (v_9581 itself sits one step ahead). */
#define YT2_ROOM_EMBARK_SKY 0xEAu
#define YT2_ROOM_EMBARK_SKY_LEFT 0x22u
#define YT2_ROOM_SCENE_WRAP 0x5A1u

typedef struct yt2_room_cell {
    uint16_t terrain;
    uint16_t object;
    uint16_t misc;
    uint16_t flags;
} yt2_room_cell;

#define YT2_ROOM_CELL_VISITED 0x8000u
#define YT2_ROOM_CELL_OBJECT 0x0400u
#define YT2_ROOM_CELL_DOOR 0x6000u
#define YT2_ROOM_CELL_HIDDEN 0x0001u

/*
 * A runtime object record (DGROUP:0xF20 slots, 0x9C bytes).  Only the
 * fields the renderer, AI and actions use are decoded; the rest stays in
 * the definition bytes the loader reads.
 */
typedef struct yt2_room_object {
    uint16_t id;                /* +0x00: nonzero while claimed */
    uint16_t x;                 /* +0x02: world tile */
    uint16_t y;                 /* +0x04: world tile */
    uint16_t cell;              /* +0x06: window byte offset */
    uint16_t pic;               /* +0x08: sprite picture */
    uint16_t mode;              /* +0x0A: blitter mode (0xA/0xD) */
    uint16_t flags;             /* +0x0C: bit 0 engaged */
    uint16_t flags2;            /* +0x0E: direction bits */
    uint16_t counter;           /* +0x10: attack counter */
    uint16_t data14;            /* +0x14: E54B payload */
    uint16_t data16;            /* +0x16: E54B payload */
    uint16_t clip;              /* +0x18: blitter clip */
    uint16_t pic2;              /* +0x1A: second layer picture */
    uint16_t step;              /* +0x1C: attack step */
    uint16_t cooldown;          /* +0x1E: attack cooldown */
    uint16_t anim_base;         /* +0x4C: animation picture base */
    uint16_t def50;             /* +0x50: definition word */
    uint16_t range60;           /* +0x60: engage range (0: always) */
    uint16_t over_dx;           /* +0x68: overlay x adjust */
    uint16_t over_dy;           /* +0x6A: overlay y adjust */
    uint8_t shade[6];           /* +0x72: sprite shade bytes */
    uint8_t dice[16];           /* +0x7E: attack dice parameters */
    uint16_t group;             /* +0x92 bit 0: group 3 else group 2 */
    uint16_t cull;              /* +0x94: visibility range flags */
} yt2_room_object;

typedef struct yt2_room {
    const yt2_world *world;
    const yt2_pictures *pictures;
    yt2_save *save;
    yt2_party *party; /* owns assigned[]; the room keeps no party copy */
    /* Dungeon window cache. */
    yt2_room_cell window[YT2_ROOM_WINDOW_SIDE][YT2_ROOM_WINDOW_SIDE];
    int origin_x;
    int origin_y;
    /* Gathered view line (near to far: column 0 is six cells ahead). */
    yt2_room_cell view[YT2_ROOM_VIEW_COLUMNS];
    yt2_room_object objects[YT2_ROOM_OBJECT_SLOTS];
    /* Special combat-adjacent records (0x51BC). */
    yt2_room_object near_objects[3];
    int16_t shades[YT2_ROOM_SHADES];
    /* Blitter parameters (v_0CCA/v_0CCC/v_0B26/v_5120/v_50C8/v_50C2). */
    uint16_t blit_pic;
    uint16_t blit_class;
    uint16_t blit_transparent;
    uint16_t blit_clip;
    uint16_t blit_column;
    int16_t blit_shade;
    uint16_t blit_offset;
    uint16_t blit_mode;
    /* Sprite shade bytes (v_0C28/v_0C2A) for the v_5060 shade table. */
    uint8_t shade_src[6];
    /* Render-mode and engine flags (v_946F lives in the party). */
    uint16_t r9469;
    uint16_t f505e;
    uint16_t f5060;
    uint16_t f5062;
    uint16_t f5064;
    /* Game clock (INT 1Ch countdowns B/C/D + gates). */
    uint16_t gates;
    uint16_t cd_clock;
    uint16_t cd_sky;
    uint16_t cd_rotation;
    uint16_t sky;               /* live E0-FF window offset in the
                                   scene palette record (one step
                                   behind v_9581) */
    int16_t sky_step;           /* v_9583: +3 or -3 */
    uint16_t sky_left;          /* v_95E5: steps remaining */
    uint16_t speed_left;        /* v_50E8: game-speed countdown */
    uint16_t rng_seed;          /* v_F7AC */
    uint16_t time_word;         /* DOS time seeding word */
    /* Step deltas (v_0B9C/v_0BA0/v_0B9E) and key state. */
    int16_t step_dx;
    int16_t step_dy;
    int16_t step_cell;
    uint8_t keycode;
    unsigned int tick;
    /* Last sound effect id (v_5142 step / v_5148 bump / 0xA teleport). */
    uint16_t last_sfx;
    /* Mutable terrain row 60 (the step animation rewrites it). */
    uint16_t terrain60[6];
    /* D0-DF rotator phase (sub_100F3 steps). */
    unsigned int rotation_phase;
    /* Set when the last tick fired a clock/sky/rotation event. */
    int tick_dirty;
    /* Set when save bytes changed (visited bits); vanilla flushes per cell. */
    int save_dirty;
} yt2_room;

/* Present callback: the engine calls it after every presented frame. */
typedef int (*yt2_room_present_fn)(void *context);

/* Returns and clears the save-dirty flag for the caller to flush. */
int yt2_room_take_save_dirty(yt2_room *room);

/* fn_12602: paint the four portraits for the assigned party. Shared
   with the in-game menu, which shows them under its chrome at boot. */
void yt2_room_paint_portraits(const yt2_party *party,
                              const yt2_pictures *pictures, yt2_surface *dst);

/* fn_0ABEC: recompute the skill means and flags from the party. Runs on
   menu E (like vanilla) and at explore entry. */
void yt2_room_update_skill_flags(yt2_party *party);

int yt2_room_init(yt2_room *room, const yt2_world *world,
                  const yt2_pictures *pictures, yt2_save *save,
                  yt2_party *party, uint16_t time_word);

/*
 * Fresh embark (menu E + ESC): zero the volatile save chunks, place the
 * party, load the window, stamp doors/objects, render one frame and enable
 * the clock gates.  Presents through `present` (fade out, frames, fade
 * in); `surface` is the 320x200 composition target.
 */
int yt2_room_embark(yt2_room *room, yt2_surface *surface,
                    yt2_room_present_fn present, void *context);

/* Reload the window cache at the party's current position. */
int yt2_room_relocate(yt2_room *room);

/* One game tick: countdowns, gates, rotation/sky/scene events. */
int yt2_room_tick(yt2_room *room);

/* True when the rotator stepped on the last tick (caller re-presents). */
int yt2_room_tick_dirty(const yt2_room *room);

/* Arrow/turn key; presents every frame the original shows (flash, step). */
int yt2_room_key(yt2_room *room, yt2_surface *surface, unsigned int scancode,
                 yt2_room_present_fn present, void *context);

/* Compose the current explore frame into `surface` (cursor-free). */
int yt2_room_render(yt2_room *room, yt2_surface *surface);

/*
 * Live palette: palette 0 with the D0h..DFh rotation at the room's phase
 * and the E0h..FFh sky window.
 */
int yt2_room_palette(const yt2_room *room, yt2_palette *palette);

#endif /* YT2_ROOM_H */
