#ifndef YT2_COMBAT_H
#define YT2_COMBAT_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/items.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/room.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * Explore combat (S rounds, formal combat, victory award) and the
 * chest UI (SPACE search/open/loot/close).
 *
 * Decoded dispatch (main loop in S0000/start, table-driven):
 * S (explore) sets v_5062 bit 0x100 and runs fn_0D620's main path:
 * every assigned member with a missile weapon (equipment[0],
 * status-checked, wear-counted) fires down the center columns
 * (0x31/0x2B/0x28/0x24/0x19): wall stop, glance, or monster hit,
 * then fn_15D89 damage with resistance halving, portrait-column
 * erase per shot, and the monster turn in the tail. A in explore
 * is a pure skip; in formal combat (v_5064 bit 0x1000) the main
 * loop runs fn_06382 instead, whose member turns take A as a melee
 * strike (equipment[2], raw HP subtract, per-weapon hit SFX).
 * Contact (step-onto or move-into) moves the record to the engaged
 * row and raises 0x1000; the death scan (sub_06BEF) zeroes kills,
 * accumulates the loot BCDs, and on victory clears 0x1000, credits
 * XP per survivor, credits the DGROUP purse (gold/ore/nuore),
 * and prints the award side panel. The purse never reaches the save
 * (DGROUP-only, parse zeroes it), but the quick access grid and the
 * currency pane read it live.
 * Rolls use the room's live v_F7AC seed; matrix runs set it
 * explicitly. SFX append to the session log (no driver here, so
 * the SoundWaitIdle gate always passes and nothing is dropped).
 * Paint is incremental like vanilla VRAM: engaged names, the
 * attacker highlight and the award side panel go straight onto the
 * borrowed surface; the next room render wipes them.
 *
 * Deferred (see porting notes): monster melee damage/SFX/death
 * (the effect system: records 0x90AB, fn_08172), monster
 * missiles, the second engager, the S-round missile flight
 * sprites, spell effects, wall-column 0x800 semantics, and the
 * 0xE54B table. Poses, DoT, button flashes, injury levels,
 * blood, target select and the formal panel are in.
 *
 * SPACE searches the party cell then the faced cell (fn_11A11 over
 * the WORLD.DAT interactable table at 0x1A11C2) and dispatches on the
 * hit's flag word: 0x8000 chests open here, 0x2000/0x1000/0x400
 * (transfers, talk, stairs) are deferred below. A chest prompts for
 * the opener ("WHO WILL OPEN?", prompt 5; "WHO WILL SEARCH?", 6 when
 * the record has bit 0x2); the portrait click runs the trap roll
 * (difficulty record/100 vs 5*(level-diff)+thievery, d101 over the
 * threshold fires effect record%100) and sets the CURGAME unlock bit
 * (0xF7E8+idx/8), then the modal shows the 4x2 loot grid at the
 * bottom right with the 3x3 currency/quick access pane above it. Currency
 * takes (def flag 0x80/0x40/0x20) credit at once with SFX 7; plain
 * items go to the cursor and drop into the quick access pane (def flag
 * 0x2000 rejected with the SFX 3 buzz), onto portraits (equip slots
 * with the weight check, pack insert fallback), back into the loot
 * (own items only, taken bit cleared) or onto the ground (prompt 4
 * confirm). SPACE closes: the taken byte flushes to CURGAME
 * (0xFA6C+idx) and explore re-renders. The modal holds the clock
 * (v_50F6 bit 0x6000 clear) like combat; rotation continues.
 *
 * Vanilla bug kept as an exception (see porting notes): quick-access-pane
 * drops land in DGROUP staging (0x9569) that nothing ever writes
 * back to the roster, so they vanish on close; only portrait drops
 * persist. B2 replicates the staging exactly.
 */

#define YT2_COMBAT_SFX_LOG 64u
#define YT2_COMBAT_ATTACKERS_MAX 4u
#define YT2_COMBAT_QUEUE_MAX 7u
#define YT2_COMBAT_ENGAGED_MAX 3u

#define YT2_CHEST_SLOTS 8u
#define YT2_QUICK_ACCESS_SLOTS 6u
#define YT2_CHEST_RECORD_SIZE 26u

/* WORLD.DAT offsets behind the EMS work pages (boot sub_02001/02064). */
#define YT2_CHEST_RECORDS_OFFSET 0x7E5BAu
#define YT2_CHEST_OBJECTS_OFFSET 0x822AAu
#define YT2_CHEST_SEARCH_OFFSET 0x1A11C2u

/* CURGAME bit files (fn_18330/fn_1834F): taken bytes per chest, unlock
   bits packed 8 per byte. */
#define YT2_CHEST_TAKEN_OFFSET 0x0FA6Cu
#define YT2_CHEST_UNLOCK_OFFSET 0x0F7E8u

/* One sub_07A66 staging entry: loot id and count/charges. */
typedef struct yt2_chest_slot {
    uint16_t id;
    uint16_t count;
} yt2_chest_slot;

/* One fn_08172 damage slot (DGROUP 0x0C4A, stride 0x14, per member
   index): the staged effect id, target member, status bits and
   damage/amount. Positions (+0/+2/+4/+6) are static portrait
   origins, derived at paint time instead of stored. */
typedef struct yt2_combat_damage {
    unsigned int effect;  /* +8: v_50DA effect id */
    int member;           /* +0x0C: assigned index 0-3, -1 = empty */
    uint16_t status;      /* +0x0E: v_0C34 status bits */
    uint16_t amount;      /* +0x10: v_0C36 damage (or item id) */
    uint16_t aux;         /* +0x12: second word (offsets/die) */
    int staged;           /* +0x0A nonzero: slot has an effect */
} yt2_combat_damage;

#define YT2_COMBAT_DAMAGE_SLOTS 4u

/* One sub_06AC5 queue entry: a member (assigned index) or a monster
   (engaged-row index), ordered by initiative descending. */
typedef struct yt2_combat_entry {
    int monster;          /* 0: member, 1: monster */
    unsigned int index;   /* assigned 0..3 or engaged 0..2 */
    uint16_t initiative;  /* roster dexterity or record +0x56 */
    uint16_t flags;       /* +6: 0x8000 monster, 0x4000 resolved */
} yt2_combat_entry;

typedef struct yt2_combat {
    yt2_room *room;             /* borrowed: objects, flags, rng, tick */
    yt2_save *save;             /* borrowed: persistence */
    const yt2_pictures *pictures; /* borrowed */
    const yt2_world *world;     /* borrowed: monster/item defs */
    const yt2_items *items;     /* borrowed: equipment checks */
    yt2_surface *surface;       /* borrowed: paint target */
    unsigned int sfx[YT2_COMBAT_SFX_LOG];
    unsigned int sfx_count;
    /* Round scratch (v_5074 words, v_0CDE, v_0C34/36/38). */
    uint16_t damage[YT2_COMBAT_ATTACKERS_MAX];
    unsigned int members_left;
    uint16_t status_bits;
    uint16_t damage_roll;
    uint16_t resist_bits;
    /* Formal-combat state (v_5390 queue, v_50C0 member, v_5070
       attacker roster slot, v_51BA engaged target). */
    yt2_combat_entry queue[YT2_COMBAT_QUEUE_MAX];
    unsigned int queue_count;
    unsigned int queue_pos;
    unsigned int current;   /* assigned index serving its turn */
    unsigned int attacker;  /* 1-based roster slot, 0 = none */
    int target;             /* engaged index, -1 = none */
    /* Kill award BCDs (v_51B6 gold, v_5392 nuore, v_5396 ore,
       v_51B2 XP): four big-endian BCD bytes each, low word used.
       Slots are [0] gold, [1] nuore, [2] ore, [3] XP, matching the
       fn_12EA0 accumulation order (loot +0/+4/+8/+12). */
    uint8_t award[4][4];
    /* Last award snapshot for repaint (the side panel zeroes live). */
    uint8_t last_award[4][4];
    int award_pending;
    /* Chest UI (v_5558 index, v_556A record, v_5568 taken mask,
       v_512C opener, v_40E2/40E4/40E6 carried, v_5060 bit 0x200). */
    int chest_open;
    int chest_prompt;
    uint16_t chest_index;
    uint8_t chest_record[YT2_CHEST_RECORD_SIZE];
    uint8_t chest_taken;
    unsigned int chest_opener;
    yt2_chest_slot chest_staging[YT2_CHEST_SLOTS];
    yt2_chest_slot quick_access[YT2_QUICK_ACCESS_SLOTS];
    uint16_t chest_cursor;
    uint16_t chest_carried;
    uint16_t chest_carried_weight;
    uint16_t chest_carried_count;
    /* Side panel message for repaint (prompt, take credit, trap/lock,
       currency trio). Rows are explicit: takes/prompt pack from
       row 96, the currency pairs skip a row each (fn_1765A). */
    char chest_side_panel[6][24];
    uint8_t chest_side_panel_color[6];
    int chest_side_panel_row[6];
    unsigned int chest_side_panel_lines;
    /* The repaint clears the compass row when set (prompt, instant
       take, pack/portrait drop); cursor takes keep the stale label.
       Consumed by repaint. */
    unsigned int chest_side_panel_compass;
    /* v_505E bit 8 mirror: takes, pickups, swaps, drop-backs and
       the currency pane set it; gated clears consume it. Persists
       across close/cancel like the original. */
    unsigned int chest_side_panel_sticky;
    /* The repaint keeps the surface (no gather) when set: the
       currency pane overpaints the stale frame (loc_0023B).
       Consumed by repaint. */
    unsigned int chest_paint_stale;
    /* The repaint paints the compass fresh when set (explore
       returns: close, prompt cancel); combat rounds keep the
       stale row. Consumed by repaint. */
    unsigned int chest_compass_fresh;
    /* Message-queue counter ([v_5560]+2, 0x6ED6/0x6EEE select). */
    uint16_t chest_messages;
    /* Saved panel bits (explore_flags & 0x7000) across the modal
       and the formal fight (B1 pushes them on the stack); each
       side restores its own on close/victory. */
    uint16_t panel_saved_modal;
    int panel_saved_modal_valid;
    uint16_t panel_saved_formal;
    int panel_saved_formal_valid;
    /* Counter base (v_5560): 0x6ED6, or 0x6EEE when the search hit
       the party cell (v_50CC == 1). The word aliases view column
       46/49's object id, so the open/close ++/-- flips the chest
       picture while the modal is up. */
    uint16_t chest_v5560;
    /* Pressed verb button (WorkSeg persistence): 0, or the pressed
       icon pic (4 swords, 6 sun) the A/C verbs flash while they
       resolve. The panel painter reads it; verbs set/clear it. */
    unsigned int button_pressed;
    /* Damage slots (fn_08120 clears, sub_06C82 stages, fn_08172
       applies). The chest trap uses the same DGROUP words at a
       different time; the lifecycles never overlap. */
    yt2_combat_damage damage_slots[YT2_COMBAT_DAMAGE_SLOTS];
    /* Wipe pending (fn_15FBC found no live member): the death
       screen is painted, and the caller runs menu 0x70 + load.
       Cleared when the caller takes it. */
    int death_pending;
    /* Menu requested by a verb click (sun/disk, explore/formal):
       the caller nests the in-game menu (mask 0xFE explore,
       0x5E formal per menu_formal). Cleared when taken. */
    int menu_requested;
    int menu_formal;
    /* Save power (v_555C): monster +0x52, spell id, or event
       param feeding fn_17D2C as the difficulty. */
    uint16_t save_power;
    /* Rolled effect id (v_50DA): picked by the effect roll,
       staged into every slot this turn. */
    unsigned int effect_id;
    /* Status mask words (v_5AA2/v_5A7E, camp-owned, zero until
       camp lands): gate the sub_08386 status clear. */
    uint16_t status_mask_any;
    uint16_t status_mask;
    /* Trap staging damage words (0x0C4A slots +0x10, never cleared). */
    uint16_t chest_effect_damage[4];
    /* Instance mint (fn_16532): bump counter (v_95FD) and the last
       save index (v_9053, set by every taken/unlock access). */
    uint16_t chest_instance_next;
    uint16_t chest_save_index;
} yt2_combat;

int yt2_combat_init(yt2_combat *combat, yt2_room *room, yt2_save *save,
                    const yt2_pictures *pictures, const yt2_world *world,
                    const yt2_items *items, yt2_surface *surface);

/*
 * One explore key (S/A/SPACE/...). Runs the action to quiescence,
 * re-renders explore, paints the combat overlays (engaged names,
 * attacker highlight, award side panel) and presents. Returns OK when
 * handled (an explore A is a handled skip).
 */
int yt2_combat_key(yt2_combat *combat, unsigned int ascii,
                   yt2_room_present_fn present, void *context);

/* F-key member pick (B1 fn_0A834: F1-F4 answer wherever a person is
   picked). Number is 1-based; today only the chest opener prompt
   picks, anything else is an error. Consumed-but-silent picks (an
   unassigned slot, B1's repoll) still return OK. */
int yt2_combat_fkey(yt2_combat *combat, unsigned int number,
                    yt2_room_present_fn present, void *context);

/* B1's fn_06382 entry on fresh formal contact (no queue yet): build
   the queue, run pending leading monster turns, and set the first
   member current (the caller repaints the border). Silent unless
   formal with an empty queue; never runs under the chest
   prompt/modal (B1's fn_073E3 owns input there). */
void yt2_combat_formal_entry(yt2_combat *combat,
                             yt2_room_present_fn present, void *context);

/* Re-render explore with the current overlays (repaint after a tick). */
int yt2_combat_repaint(yt2_combat *combat, yt2_room_present_fn present,
                       void *context);

/* Modal-close restore: same, but portraits (and the border) stay
   retained like B1's gather. */
int yt2_combat_repaint_keep_portraits(yt2_combat *combat,
                                      yt2_room_present_fn present,
                                      void *context);

/* Explore/formal click (arrows, verbs, targets, viewport, chest UI).
   Formal verbs present through the callback like keys do. */
int yt2_combat_click(yt2_combat *combat, int x, int y, int right,
                     yt2_room_present_fn present, void *context);

/* SFX log drain for the gate. */
unsigned int yt2_combat_sfx_count(const yt2_combat *combat);
unsigned int yt2_combat_sfx_at(const yt2_combat *combat, unsigned int index);

/* fn_0EBD6 opens the nested menu with fn_06EA4: consume the side panels so a
   pending award or message never paints under the menu. */
void yt2_combat_menu_open(yt2_combat *combat);

/* fn_117AC + fn_11836 fill: the explore top-right panel by state
   (0x2000 chapter, 0x1000 pack, else the 9x7 minimap). The room
   calls it on gathers; callers present (stale, never gather). */
void yt2_combat_panel_paint(yt2_combat *combat, yt2_surface *dst);

/* F5 (B1 path B): QUICK ACCESS<->MAP with the skill, QUICK ACCESS<->CHAPTER else;
   paints, blips, presents stale. Silent ERROR when formal. */
int yt2_combat_panel_toggle(yt2_combat *combat,
                            yt2_room_present_fn present, void *context);

#endif /* YT2_COMBAT_H */
