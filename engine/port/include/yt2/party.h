#ifndef YT2_PARTY_H
#define YT2_PARTY_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/status.h"

/*
 * Live party state for B2.
 *
 * This is deliberately not a view over the save bytes.  The parser copies the
 * fields the game actually uses into ordinary C structures with names and
 * ownership; the on-disk byte layout lives in savefile.c.  Unknown/reserved
 * fields are not part of the live model yet.
 */

#define YT2_CHARACTER_NAME_LENGTH 14u
#define YT2_CHARACTER_NAME_SIZE (YT2_CHARACTER_NAME_LENGTH + 1u)
#define YT2_ROSTER_SLOTS 9u
#define YT2_PARTY_BLOCK_SIZE 0x1388u
#define YT2_PARTY_ASSIGNED_MAX 4u
#define YT2_PARTY_ACTIVE_MAX 5u
#define YT2_PACK_SLOTS 8u
#define YT2_EQUIPMENT_SLOTS 6u
#define YT2_EFFECT_SLOTS 5u

/* Character-creation order (README.DOC, the "PICK A CLASS" menu). */
typedef enum yt2_class {
    YT2_CLASS_NONE = 0,
    YT2_CLASS_FIGHTER = 1,
    YT2_CLASS_MERCHANT = 2,
    YT2_CLASS_ROGUE = 3,
    YT2_CLASS_MONK = 4,
    YT2_CLASS_ALCHEMIST = 5,
    YT2_CLASS_PALADIN = 6,
    YT2_CLASS_MAGE = 7,
    YT2_CLASS_DRUID = 8,
    YT2_CLASS_MARKSMAN = 9
} yt2_class;

typedef enum yt2_sex {
    YT2_SEX_NONE = 0,
    YT2_SEX_MALE = 1,
    YT2_SEX_FEMALE = 2
} yt2_sex;

typedef struct yt2_attributes {
    uint16_t strength;
    uint16_t dexterity;
    uint16_t stamina;
    uint16_t intelligence;
    uint16_t wisdom;
    uint16_t charisma;
} yt2_attributes;

typedef struct yt2_combat_stats {
    uint16_t projectile_accuracy;
    uint16_t projectile_damage;
    uint16_t battle_accuracy;
    uint16_t battle_damage;
    uint16_t absorption;
    uint16_t hit_points;
    uint16_t magic_points;
    uint16_t carry_capacity;
} yt2_combat_stats;

typedef struct yt2_skills {
    uint16_t survival;
    uint16_t projectile;
    uint16_t slashing;
    uint16_t bashing;
    uint16_t polearm;
    uint16_t casting;
    uint16_t mapping;
    uint16_t navigation;
    uint16_t bartering;
    uint16_t repair;
    uint16_t thievery;
    uint16_t linguistics;
    uint16_t chemistry;
} yt2_skills;

typedef struct yt2_stats {
    yt2_attributes attributes;
    yt2_combat_stats combat;
    yt2_skills skills;
} yt2_stats;

/* A 4-byte pack/equipment entry: item id and count/charges/state word. */
typedef struct yt2_item {
    uint16_t id;
    uint16_t count;
} yt2_item;

/* Container staging slots (+0x17C/+0x1A2/+0x1C8, 38 bytes each):
   the staged bag id, its file-store index (the bag's count),
   the contents weight, and the 8 content cells. Occupied flags
   live in character flags (+0x15C): 0x1000 cell-11, 0x2000 /
   0x4000 / 0x8000 pack (1C8/1A2/17C). */
#define YT2_CONTAINER_SLOTS 3u
#define YT2_CONTAINER_CELLS 8u

typedef struct yt2_container_stage {
    uint16_t id;
    uint16_t count;
    uint16_t weight;
    yt2_item cells[YT2_CONTAINER_CELLS];
} yt2_container_stage;

/* Known spellbook bits (sub_17FD1 addresses +0xCA legs by spell/16). */
#define YT2_SPELLBOOK_WORDS 16u

#define YT2_BUFF_SLOTS 9u
#define YT2_TRANSPORT_SLOTS 4u
#define YT2_POOL_SLOTS 6u

typedef struct yt2_character {
    char name[YT2_CHARACTER_NAME_SIZE]; /* always NUL-terminated */
    yt2_class class_id;
    yt2_sex sex;
    uint16_t portrait_pic;   /* +0x12: group-7 picture id (19 + index) */
    uint16_t portrait_index; /* +0x14: 0..17, even male, odd female */
    uint16_t level;          /* +0x16: 1 for a newly created character */
    uint32_t experience;     /* +0x18: 4-byte BCD (fn_09ADC); byte +0x1B
                                is its top byte, not a weight */
    uint16_t status;
    /* Base combat stats (+0x32/+0x36/+0x34/+0x38/+0x3A, in current
       field order); fn_0B3D2 resets current from these plus equipment.
       [3]/[4] derive from strength/dexterity (fn_0AB61). */
    uint16_t base[5];
    /* Maximum bases (+0x72/+0x76/+0x74/+0x78/+0x7A, same order). */
    uint16_t max_base[5];
    uint16_t ready;                 /* +0x1E: ready cell glyph when set */
    uint16_t buffs[YT2_BUFF_SLOTS]; /* +0x20: buff counters, icon 0xE if any */
    uint16_t spellbook[YT2_SPELLBOOK_WORDS]; /* +0xCA bit legs */
    yt2_stats current;
    yt2_stats maximum;
    uint16_t icon_sel; /* +0xB4: ready-cell picture (0x14 when set, 0x15);
                        top four bits are the fn_1620A transport mask */
    /* Per-ride counters (+0xB6, fn_1620A compares each against the
       ride's uses from the 0x77CC table). */
    uint16_t transport[YT2_TRANSPORT_SLOTS];
    uint16_t carried_weight;
    yt2_item pack[YT2_PACK_SLOTS];
    yt2_item equipment[YT2_EQUIPMENT_SLOTS];
    uint16_t effects[YT2_EFFECT_SLOTS];
    uint16_t flags;
    yt2_container_stage stage[YT2_CONTAINER_SLOTS];
    uint16_t overlay; /* +0x15E: 0x8000 paints the skull overlay */
    /* Weapon wear counters (fn_0AD9D): +0xBE missile (cap 0x78),
       +0xC0 melee (cap 0x50), +0xC2 spell (cap 0x14).  Past the cap the
       weapon rolls breakage (rand 1000 under the param-row threshold
       breaks it: counter reset, the attack fumbles). */
    uint16_t wear[3];
} yt2_character;

typedef struct yt2_party {
    uint16_t game_speed;
    uint16_t x;
    uint16_t y;
    uint16_t facing;
    uint16_t day;
    uint16_t month;
    uint16_t year;
    uint16_t scene;
    /* Header 0x20: v_946F explore/panel flags (boot 0x2000). */
    uint16_t explore_flags;
    /* Header 0x52: v_94A1 music track (boot 0x0C). */
    uint16_t music_id;
    /* Header 0x86: v_SoundState music/sound present+enabled bits. */
    uint16_t sound_state;
    /* Header 0x46/0x48/0x4A: party mean mapping/navigation/survival
       (fn_0ABEC, DGROUP-live; saved with the block, never pre-set). */
    uint16_t mean_mapping;
    uint16_t mean_navigation;
    uint16_t mean_survival;
    /* Header 0xA4: v_94F3 highlight slots (opaque words, preserved). */
    uint16_t active[YT2_PARTY_ACTIVE_MAX];
    /* Shared pool (header 0x11A, DGROUP 0x9569): the P pack UI's
       six grid slots (the chest modal stages opener-synced packs
       instead, pre-existing). */
    yt2_item pool[YT2_POOL_SLOTS];
    /* Header 0x1EC: v_963B assigned party, 1-based roster slots
       (0 = empty).  The roster keeps this and the per-slot 0x800
       flags in sync on every toggle (fn_1C354); DONE compacts it
       in place (sub_1C576); party_ok is derived from its sum
       (fn_0D36C), never stored. */
    uint16_t assigned[YT2_PARTY_ASSIGNED_MAX];
    unsigned int roster_count; /* non-empty roster slots */
    yt2_character roster[YT2_ROSTER_SLOTS];
    /* Party purse (DGROUP 0x9503/0x9507/0x950B): 4-byte BCD each,
       credited by chest takes (fn_09B02). Vanilla keeps it in
       DGROUP only; no CURGAME chunk holds it, so parse zeroes it
       and write leaves it alone. Embark/setup sets the purse. */
    uint8_t gold[4];
    uint8_t ore[4];
    uint8_t nuore[4];
} yt2_party;

const char *yt2_class_name(yt2_class class_id);
const char *yt2_sex_name(yt2_sex sex);

/* Parse the first 5000-byte block of CURGAME / SAVEGAMEn. */
int yt2_party_parse(const uint8_t *block, size_t size, yt2_party *party);

/* Reparse one 0x1F4 roster slot (bonus application writes raw). */
void yt2_party_parse_character(const uint8_t *slot, yt2_character *character);

/*
 * Write the live party state back over an existing party block.
 *
 * The block keeps its signature and every byte the live model does not name
 * yet (reserved spans, containers), so loading and writing an untouched save
 * is byte-identical.  Fields the model does own are replaced from `party`.
 */
int yt2_party_write(uint8_t *block, size_t size, const yt2_party *party);

#endif /* YT2_PARTY_H */
