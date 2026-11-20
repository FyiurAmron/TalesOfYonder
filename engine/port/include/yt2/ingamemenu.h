#ifndef YT2_INGAMEMENU_H
#define YT2_INGAMEMENU_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"

/*
 * The pre-explore in-game menu (fn_0EBD6) with the save/load slot panel.
 *
 * Vanilla shows this screen after menu E (boot path) and on the in-explore
 * D key, with different option masks: fresh boot without saves 0x9D, boot
 * with saves 0x71, explore panel 0xFE (v_505E low byte). Bit gates per
 * option: 0x80 save, 0x40 load, 0x20 new game, 0x10 quit to DOS, 0x08 game
 * speed, 0x04 continue; music/sound/ordering-info carry no gate. R, ESC and
 * right-click all mean continue when bit 2 is set.
 *
 * Options: A cycles game speed 5/1/9 (sub_0F356), M toggles music, U toggles
 * sound effects (both state-only until the M9 audio engine), D confirms
 * quit to DOS, N confirms return to the menu, S opens the save slot list, L
 * the load slot list, O is a no-op (fn_00000 always returns 1, so fn_1BFFC
 * is dead code). Six save slots (SAVGAME1..6); each slot record carries the
 * file digit, an exists/selected flag byte and the 25-byte list text.
 *
 * Saving writes the typed name over the party header bytes via strcpy (a
 * typed name kills the signature and vanilla loads it anyway), refreshes
 * CURGAME's party block from live state, then copies CURGAME over the slot
 * file (sub_0F823 net effect). Loading copies the slot file over CURGAME
 * and reloads live state, then leaves for explore. The boot scan
 * (sub_01D84) fills the slot records from disk before the first paint.
 *
 * The session is event-driven like creation/roster: the caller feeds keys
 * and clicks, the session repaints after every accepted event. Screens are
 * cursor-free; SFX go to the session log for the gate.
 */

#define YT2_INGAMEMENU_SLOTS 6u
#define YT2_INGAMEMENU_NAME_TEXT 25u
#define YT2_INGAMEMENU_NAME_MAX 23u
#define YT2_INGAMEMENU_SFX_LOG 64u

/* v_505E option bits (low byte). */
#define YT2_INGAMEMENU_BIT_CONTINUE 0x04u
#define YT2_INGAMEMENU_BIT_SPEED 0x08u
#define YT2_INGAMEMENU_BIT_QUIT 0x10u
#define YT2_INGAMEMENU_BIT_NEWGAME 0x20u
#define YT2_INGAMEMENU_BIT_LOAD 0x40u
#define YT2_INGAMEMENU_BIT_SAVE 0x80u

/* Sound-state bits (v_SoundState): music/sound present + enabled. */
#define YT2_INGAMEMENU_SOUND_MUSIC_PRESENT 0x01u
#define YT2_INGAMEMENU_SOUND_MUSIC_ON 0x02u
#define YT2_INGAMEMENU_SOUND_SFX_PRESENT 0x04u
#define YT2_INGAMEMENU_SOUND_SFX_ON 0x08u

/* 6CC4h slot record flags. */
#define YT2_INGAMEMENU_SLOT_EXISTS 0x80u
#define YT2_INGAMEMENU_SLOT_SELECTED 0x40u

typedef enum yt2_ingamemenu_exit {
    YT2_INGAMEMENU_EXIT_NONE = 0,
    YT2_INGAMEMENU_EXIT_CONTINUE, /* R/ESC/right-click: enter explore */
    YT2_INGAMEMENU_EXIT_QUIT,     /* D confirmed: quit to DOS */
    YT2_INGAMEMENU_EXIT_MENU      /* N confirmed: back to the menu */
} yt2_ingamemenu_exit;

typedef struct yt2_ingamemenu_slot {
    char file_digit; /* '1'..'6' */
    uint8_t flags;   /* exists/selected bits */
    char text[YT2_INGAMEMENU_NAME_TEXT + 1u]; /* list line, NUL-terminated */
} yt2_ingamemenu_slot;

typedef struct yt2_ingamemenu {
    yt2_party *party; /* live state, caller-owned */
    yt2_save *save;   /* live CURGAME image, caller-owned */
    yt2_ingamemenu_slot *slots; /* session slot records, caller-owned, scanned once */
    const yt2_pictures *pictures;
    yt2_surface *surface;
    char dir[1024]; /* game directory holding CURGAME/SAVEGAMEn */
    uint16_t enable_mask;
    int combat; /* v_5064 bit 0x1000: selects the wide clear path */
    unsigned int view;   /* internal state, see ingamemenu.c */
    unsigned int slot;   /* v_50BA picked slot 1..6 */
    unsigned int saving; /* v_50AC mode: 1 save list, 0 load list */
    char editor[YT2_INGAMEMENU_NAME_MAX + 1u];
    unsigned int editor_length;
    unsigned int dialog; /* active confirm record id (0 none) */
    int dialog_named;    /* save dialog reached via the editor (not Enter) */
    yt2_ingamemenu_exit exit;
    unsigned int sfx_count;
    unsigned int sfx[YT2_INGAMEMENU_SFX_LOG];
} yt2_ingamemenu;

/* Scan SAVGAME1..6 like sub_01D84: fill six slot records (names from the
   files' first 25 bytes, EMPTY default) and return nonzero when any slot
   file exists (v_505E bit 0x4000). Runs once per boot, like vanilla. */
int yt2_ingamemenu_scan_slots(yt2_ingamemenu_slot *slots, const char *dir);

/* Paint the base screen and open input (fn_0EBD6 entry). The enable mask
   selects the available options; sound_state drives the music/sound rows.
   A nonzero cursor id skips the screen like vanilla (mid-drag entry).
   Without explore set, begin paints the boot underlay first (the explore
   backdrop plus the sub_128FB panel); from explore the screen draws over
   the live frame already on the surface. */
int yt2_ingamemenu_begin(yt2_ingamemenu *ingamemenu, yt2_party *party, yt2_save *save,
                   yt2_ingamemenu_slot *slots, const yt2_pictures *pictures,
                   yt2_surface *surface, const char *dir,
                   uint16_t enable_mask, int combat, int explore,
                   unsigned int cursor);

/* Feed one key press (ASCII, uppercased by the caller like v_KeyCode) or
   one click at surface pixels (right edge for right-click). Returns
   YT2_STATUS_OK and repaints when the event lands. */
int yt2_ingamemenu_key(yt2_ingamemenu *ingamemenu, unsigned int ascii);
int yt2_ingamemenu_click(yt2_ingamemenu *ingamemenu, int x, int y, int right);

/* Any extended key (arrows, function keys) by scancode: opens the editor
   from the overwrite prompt (typed as punctuation like vanilla), inert
   everywhere else including the editor itself. */
int yt2_ingamemenu_fkey(yt2_ingamemenu *ingamemenu, unsigned int scancode);

/* fn_06EA4 half of menu open: wipe the side panel rects (loot/message ink)
   so stale side panels never show under the menu. B1 wipes first thing. */
void yt2_ingamemenu_wipe_side_panels(yt2_ingamemenu *ingamemenu);

/* True once the session reached an exit (continue/quit/menu). */
int yt2_ingamemenu_done(const yt2_ingamemenu *ingamemenu);
yt2_ingamemenu_exit yt2_ingamemenu_result(const yt2_ingamemenu *ingamemenu);

/* SFX log for the caller/gate. */
unsigned int yt2_ingamemenu_sfx_count(const yt2_ingamemenu *ingamemenu);
unsigned int yt2_ingamemenu_sfx(const yt2_ingamemenu *ingamemenu, unsigned int index);

#endif /* YT2_INGAMEMENU_H */
