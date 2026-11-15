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

/* Known spellbook bits (sub_17FD1 addresses +0xCA legs by spell/16). */
#define YT2_SPELLBOOK_WORDS 16u

#define YT2_BUFF_SLOTS 9u

typedef struct yt2_character {
    char name[YT2_CHARACTER_NAME_SIZE]; /* always NUL-terminated */
    yt2_class class_id;
    yt2_sex sex;
    uint16_t portrait_pic;   /* +0x12: group-7 picture id (19 + index) */
    uint16_t portrait_index; /* +0x14: 0..17, even male, odd female */
    uint16_t level;          /* +0x16: 1 for a newly created character */
    uint32_t experience;     /* +0x18: shown hex as EMP (fn_09C46) */
    uint8_t weight;
    uint16_t status;
    uint16_t ready;                 /* +0x1E: ready cell glyph when set */
    uint16_t buffs[YT2_BUFF_SLOTS]; /* +0x20: buff counters, icon 0xE if any */
    uint16_t spellbook[YT2_SPELLBOOK_WORDS]; /* +0xCA bit legs */
    yt2_stats current;
    yt2_stats maximum;
    uint16_t icon_sel; /* +0xB4: ready-cell picture (0x14 when set, 0x15) */
    uint16_t carried_weight;
    yt2_item pack[YT2_PACK_SLOTS];
    yt2_item equipment[YT2_EQUIPMENT_SLOTS];
    uint16_t effects[YT2_EFFECT_SLOTS];
    uint16_t flags;
    uint16_t overlay; /* +0x15E: 0x8000 paints the skull overlay */
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
    /* Header 0xA4: v_94F3 highlight slots (opaque words, preserved). */
    uint16_t active[YT2_PARTY_ACTIVE_MAX];
    /* Header 0x1EC: v_963B assigned party, 1-based roster slots
       (0 = empty).  The roster keeps this and the per-slot 0x800
       flags in sync on every toggle (fn_1C354); DONE compacts it
       in place (sub_1C576); party_ok is derived from its sum
       (fn_0D36C), never stored. */
    uint16_t assigned[YT2_PARTY_ASSIGNED_MAX];
    unsigned int roster_count; /* non-empty roster slots */
    yt2_character roster[YT2_ROSTER_SLOTS];
} yt2_party;

const char *yt2_class_name(yt2_class class_id);
const char *yt2_sex_name(yt2_sex sex);

/* Parse the first 5000-byte block of CURGAME / SAVEGAMEn. */
int yt2_party_parse(const uint8_t *block, size_t size, yt2_party *party);

/*
 * Write the live party state back over an existing party block.
 *
 * The block keeps its signature and every byte the live model does not name
 * yet (reserved spans, containers), so loading and writing an untouched save
 * is byte-identical.  Fields the model does own are replaced from `party`.
 */
int yt2_party_write(uint8_t *block, size_t size, const yt2_party *party);

#endif /* YT2_PARTY_H */
