#include "yt2/spell_detail.h"

#include <stdio.h>
#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"
#include "yt2/text.h"

static const unsigned int spell_detail_icons[YT2_LIST_ICON_COUNT] = {
    32u, 325u, 328u, 339u, 329u, 331u, 333u
};

static const char *const type_names[3] = {
    "SCROLL", "TRAINING", "CREATION"
};

static const uint8_t type_colors[3] = {
    YT2_SPELL_DETAIL_SCROLL_COLOR, YT2_SPELL_DETAIL_TRAINING_COLOR,
    YT2_SPELL_DETAIL_CREATION_COLOR
};

/* Generated from 105 B1 oracle samples; do not hand-edit. */
static const uint8_t spell_effect_priv[18u * 6u] = {
    0x20, 0x10, 0x88, 0x40, 0x20, 0x00, /* 2 */
    0x08, 0x10, 0x10, 0x50, 0x20, 0x00, /* A */
    0x88, 0x50, 0x50, 0x50, 0x20, 0x00, /* B */
    0x50, 0x20, 0x20, 0x20, 0x50, 0x00, /* C */
    0x08, 0x48, 0xA8, 0x98, 0x88, 0x00, /* E */
    0xC0, 0xA0, 0x90, 0x90, 0x98, 0x00, /* F */
    0x20, 0x20, 0x40, 0x80, 0xC0, 0x00, /* G */
    0x20, 0x20, 0x20, 0x50, 0x88, 0x00, /* H */
    0x80, 0x40, 0x20, 0x20, 0x30, 0x00, /* I */
    0x08, 0x50, 0xA0, 0x60, 0x20, 0x00, /* J */
    0x88, 0x50, 0x50, 0x50, 0x88, 0x00, /* L */
    0x88, 0x88, 0xF0, 0x80, 0x80, 0x00, /* P */
    0x60, 0x20, 0x30, 0x20, 0x60, 0x00, /* Q */
    0x60, 0x50, 0x20, 0x50, 0x60, 0x00, /* S */
    0x20, 0x50, 0x70, 0x50, 0x20, 0x00, /* W */
    0x80, 0x40, 0x20, 0xA0, 0xC0, 0x00, /* Y */
    0x40, 0x20, 0x10, 0x10, 0x30, 0x00, /* [402010103000] */
    0x00, 0x00, 0x00, 0x00, 0x88, 0xD8, /* [0000000088D8] */
};

typedef struct spell_detail_row {
    const char *class_name;
    uint8_t level;
    uint8_t type;
} spell_detail_row;

typedef struct spell_detail_entry {
    spell_detail_row rows[6];
    uint8_t row_count;
    const char *scope;
    const char *middle;
    const char *target;
    const char *when;
    const char *effect[5];
} spell_detail_entry;

static const spell_detail_entry spell_details[105] = {
    /* file 0 */ {
        { { "MONK", 1u, 2u }, { "ALCHEMIST", 1u, 2u }, { "PALADIN", 1u, 2u }, { "DRUID", 1u, 2u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 1 */ {
        { { "ALCHEMIST", 1u, 2u }, { "PALADIN", 4u, 1u }, { "MAGE", 1u, 2u }, { "DRUID", 1u, 2u }, { "MARKSMAN", 1u, 2u }, { "", 0u, 0u } }, 5u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 2 */ {
        { { "MONK", 1u, 2u }, { "PALADIN", 2u, 1u }, { "MAGE", 1u, 2u }, { "MARKSMAN", 2u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 3 */ {
        { { "MAGE", 2u, 1u }, { "DRUID", 2u, 1u }, { "MARKSMAN", 4u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 4 */ {
        { { "MONK", 2u, 1u }, { "ALCHEMIST", 2u, 1u }, { "PALADIN", 6u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 5 */ {
        { { "MONK", 3u, 0u }, { "ALCHEMIST", 3u, 0u }, { "PALADIN", 4u, 1u }, { "MAGE", 3u, 0u }, { "DRUID", 3u, 0u }, { "MARKSMAN", 4u, 1u } }, 6u,
        "", "", "", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 6 */ {
        { { "MONK", 4u, 1u }, { "ALCHEMIST", 4u, 1u }, { "DRUID", 4u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "", "", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 7 */ {
        { { "MONK", 4u, 0u }, { "ALCHEMIST", 4u, 0u }, { "PALADIN", 4u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 8 */ {
        { { "MAGE", 4u, 1u }, { "MARKSMAN", 8u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "7", "", "" },
    },
    /* file 9 */ {
        { { "ALCHEMIST", 6u, 1u }, { "MAGE", 4u, 1u }, { "DRUID", 4u, 1u }, { "MARKSMAN", 6u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "\x83", "\\\x83", "\x81h\x83", "\x8E\x81t\x83", "" },
    },
    /* file 10 */ {
        { { "MONK", 4u, 1u }, { "ALCHEMIST", 4u, 1u }, { "PALADIN", 6u, 1u }, { "MAGE", 4u, 0u }, { "DRUID", 4u, 0u }, { "MARKSMAN", 4u, 0u } }, 6u,
        "ONE", "INSECT", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "L", "", "\x8A", "" },
    },
    /* file 11 */ {
        { { "MAGE", 5u, 0u }, { "DRUID", 6u, 1u }, { "MARKSMAN", 5u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "\x82", "", "h", "" },
    },
    /* file 12 */ {
        { { "MAGE", 6u, 1u }, { "MARKSMAN", 10u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 13 */ {
        { { "MONK", 6u, 1u }, { "ALCHEMIST", 6u, 1u }, { "PALADIN", 8u, 1u }, { "DRUID", 8u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 14 */ {
        { { "ALCHEMIST", 10u, 1u }, { "MAGE", 6u, 1u }, { "DRUID", 6u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "", "", "", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 15 */ {
        { { "MONK", 6u, 1u }, { "ALCHEMIST", 8u, 1u }, { "PALADIN", 10u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 16 */ {
        { { "MAGE", 7u, 0u }, { "DRUID", 7u, 0u }, { "MARKSMAN", 7u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "", "", "", "OUT OF HAND TO HAND",
        { "", "", "", "", "\x8F" },
    },
    /* file 17 */ {
        { { "ALCHEMIST", 7u, 0u }, { "DRUID", 7u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN A STRAIGHT LINE", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 18 */ {
        { { "MONK", 7u, 0u }, { "PALADIN", 7u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 19 */ {
        { { "MONK", 7u, 0u }, { "ALCHEMIST", 7u, 0u }, { "PALADIN", 10u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "UNDEADS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "\x87", "\x81\x88\x91", "\x82\x8D\x88", "", "" },
    },
    /* file 20 */ {
        { { "ALCHEMIST", 8u, 1u }, { "DRUID", 8u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "", "", "", "OUT OF HAND TO HAND",
        { " \x82\\\x88S", "", "", "", "" },
    },
    /* file 21 */ {
        { { "MONK", 8u, 1u }, { "PALADIN", 8u, 1u }, { "MAGE", 8u, 1u }, { "MARKSMAN", 8u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "", "", "", "OUT OF HAND TO HAND",
        { "E", "", "\x87", "", "" },
    },
    /* file 22 */ {
        { { "MONK", 8u, 1u }, { "ALCHEMIST", 10u, 1u }, { "PALADIN", 12u, 1u }, { "DRUID", 12u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "\x8C", "", "", "" },
    },
    /* file 23 */ {
        { { "ALCHEMIST", 12u, 1u }, { "MAGE", 8u, 1u }, { "DRUID", 10u, 1u }, { "MARKSMAN", 14u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ALL", "MONSTERS", "IN A 3X3 AREA", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 24 */ {
        { { "MONK", 9u, 0u }, { "ALCHEMIST", 9u, 0u }, { "PALADIN", 16u, 1u }, { "DRUID", 16u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ALL", "CHARACTERS", "", "ANYTIME",
        { "", "", "i\x82", "", "" },
    },
    /* file 25 */ {
        { { "ALCHEMIST", 9u, 0u }, { "PALADIN", 9u, 0u }, { "DRUID", 9u, 0u }, { "MARKSMAN", 9u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 26 */ {
        { { "MONK", 10u, 1u }, { "PALADIN", 12u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 27 */ {
        { { "MAGE", 10u, 1u }, { "DRUID", 10u, 1u }, { "MARKSMAN", 10u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "\x89", "", "" },
    },
    /* file 28 */ {
        { { "MONK", 10u, 1u }, { "ALCHEMIST", 14u, 1u }, { "MAGE", 10u, 1u }, { "DRUID", 12u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "", "", "", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 29 */ {
        { { "MONK", 11u, 0u }, { "ALCHEMIST", 11u, 0u }, { "PALADIN", 14u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "UNDEADS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 30 */ {
        { { "MONK", 11u, 0u }, { "ALCHEMIST", 11u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "VISIBLE UNDEADS", "", "ANYTIME",
        { "", "", "\x81hg\x85", "", "" },
    },
    /* file 31 */ {
        { { "MAGE", 11u, 0u }, { "DRUID", 11u, 0u }, { "MARKSMAN", 11u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "", "", "", "OUT OF HAND TO HAND",
        { "ER2\\BK1_CH2\\GAME\\DAD\\WORLD.DAT", "KEY", "STEEL CHEST KEY", "KEY", "" },
    },
    /* file 32 */ {
        { { "ALCHEMIST", 16u, 1u }, { "MAGE", 12u, 1u }, { "DRUID", 14u, 1u }, { "MARKSMAN", 14u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "BRONZE DOOR KEY", "Y", "STEEL DOOR KEY", "", "" },
    },
    /* file 33 */ {
        { { "MONK", 12u, 1u }, { "ALCHEMIST", 12u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "UNDEAD", "IN HAND TO HAND", "IN HAND TO HAND",
        { "EY", "%EAST%", "", "", "" },
    },
    /* file 34 */ {
        { { "MONK", 12u, 1u }, { "PALADIN", 12u, 0u }, { "MAGE", 12u, 1u }, { "MARKSMAN", 12u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "", "", "", "OUT OF HAND TO HAND",
        { "", "GOLDEN AMULET", "", "", "" },
    },
    /* file 35 */ {
        { { "MONK", 13u, 0u }, { "ALCHEMIST", 13u, 0u }, { "PALADIN", 14u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "KEY OF PORT HOPE", "", "", "", "" },
    },
    /* file 36 */ {
        { { "MAGE", 13u, 0u }, { "DRUID", 13u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN A STRAIGHT LINE", "OUT OF HAND TO HAND",
        { "%SAFARI%", " KEY", "DERRICK", "", "USE WITH CAUTION" },
    },
    /* file 37 */ {
        { { "MONK", 14u, 1u }, { "PALADIN", 18u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "EPAIR", "REQUIRES COPPER KEY", "", "", "" },
    },
    /* file 38 */ {
        { { "MONK", 14u, 1u }, { "ALCHEMIST", 14u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "CALUM", "", "THIEF'S GUILD", "", "" },
    },
    /* file 39 */ {
        { { "MAGE", 14u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "ROJECTILE", "DUNGEON", "E 2 KEY", "", "" },
    },
    /* file 40 */ {
        { { "MAGE", 14u, 1u }, { "DRUID", 14u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN A 3X3 AREA", "OUT OF HAND TO HAND",
        { "REQUIRES STEEL KEY", "", "%MYSELF%", "", "" },
    },
    /* file 41 */ {
        { { "MONK", 15u, 0u }, { "ALCHEMIST", 15u, 0u }, { "PALADIN", 18u, 1u }, { "DRUID", 20u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "%TREES%", "LD", "", "", "" },
    },
    /* file 42 */ {
        { { "MAGE", 15u, 0u }, { "DRUID", 15u, 0u }, { "MARKSMAN", 18u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "", "OUT OF HAND TO HAND",
        { "MAYOR", "", "TATE", "", "" },
    },
    /* file 43 */ {
        { { "MAGE", 16u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "%ALEXANDER%", "", "TORCHLIGHT MINE", "" },
    },
    /* file 44 */ {
        { { "MONK", 16u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "MONSTERS", "IN A 3X3 AREA", "OUT OF HAND TO HAND",
        { "AGE", "PACES..", "", "", "" },
    },
    /* file 45 */ {
        { { "MONK", 16u, 1u }, { "ALCHEMIST", 16u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "`(", "", "", "", "" },
    },
    /* file 46 */ {
        { { "ALCHEMIST", 20u, 1u }, { "MAGE", 16u, 1u }, { "DRUID", 16u, 1u }, { "MARKSMAN", 16u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ALL", "MONSTERS", "IN A STRAIGHT LINE", "OUT OF HAND TO HAND",
        { "`", "", "", "", "" },
    },
    /* file 47 */ {
        { { "PALADIN", 16u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 48 */ {
        { { "MONK", 17u, 0u }, { "ALCHEMIST", 17u, 0u }, { "PALADIN", 20u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "VISIBLE MONSTERS", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 49 */ {
        { { "MAGE", 17u, 0u }, { "DRUID", 17u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "", "", "", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 50 */ {
        { { "MAGE", 18u, 1u }, { "DRUID", 22u, 1u }, { "MARKSMAN", 24u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "MONSTERS", "IN A 3X3 AREA", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 51 */ {
        { { "MONK", 18u, 1u }, { "ALCHEMIST", 22u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "", "OUT OF HAND TO HAND",
        { "", "?", "", "", "" },
    },
    /* file 52 */ {
        { { "ALCHEMIST", 18u, 1u }, { "DRUID", 18u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "m?", "", "", "", "" },
    },
    /* file 53 */ {
        { { "MAGE", 18u, 1u }, { "DRUID", 18u, 1u }, { "MARKSMAN", 22u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 54 */ {
        { { "MONK", 18u, 1u }, { "ALCHEMIST", 18u, 1u }, { "PALADIN", 22u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "CHARACTERS", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 55 */ {
        { { "MAGE", 19u, 0u }, { "DRUID", 19u, 0u }, { "MARKSMAN", 19u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "MONSTERS", "IN A 3X3 AREA", "OUT OF HAND TO HAND",
        { "x\x84\x87", "", "", "\x86\x82", "" },
    },
    /* file 56 */ {
        { { "MAGE", 19u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "", "", "", "OUT OF HAND TO HAND",
        { "KEY", "", "", "", "" },
    },
    /* file 57 */ {
        { { "MONK", 19u, 0u }, { "ALCHEMIST", 19u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "\x83", "OULDER", "", "", "" },
    },
    /* file 58 */ {
        { { "MONK", 19u, 0u }, { "ALCHEMIST", 19u, 0u }, { "PALADIN", 19u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "NE", "", "" },
    },
    /* file 59 */ {
        { { "MAGE", 20u, 1u }, { "DRUID", 20u, 1u }, { "MARKSMAN", 26u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "\x81", "LO", "", "" },
    },
    /* file 60 */ {
        { { "MONK", 20u, 1u }, { "ALCHEMIST", 20u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "CTS", "", "", "", "" },
    },
    /* file 61 */ {
        { { "MAGE", 20u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "MONSTERS", "IN A 3X3 AREA", "OUT OF HAND TO HAND",
        { "RSHIP", "", "", "BERSHIP", "" },
    },
    /* file 62 */ {
        { { "MONK", 20u, 1u }, { "ALCHEMIST", 26u, 1u }, { "PALADIN", 24u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", " \x81", "", "", "" },
    },
    /* file 63 */ {
        { { "MARKSMAN", 20u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "MONSTER", "", "OUT OF HAND TO HAND",
        { "", "CE", "", "", "" },
    },
    /* file 64 */ {
        { { "MAGE", 21u, 0u }, { "DRUID", 21u, 0u }, { "MARKSMAN", 24u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "P", "", "", "", "" },
    },
    /* file 65 */ {
        { { "MONK", 21u, 0u }, { "ALCHEMIST", 21u, 0u }, { "PALADIN", 21u, 0u }, { "MAGE", 21u, 0u }, { "DRUID", 21u, 0u }, { "MARKSMAN", 21u, 0u } }, 6u,
        "", "", "", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 66 */ {
        { { "MONK", 21u, 0u }, { "ALCHEMIST", 21u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN A STRAIGHT LINE", "OUT OF HAND TO HAND",
        { "", "", "", "@", "" },
    },
    /* file 67 */ {
        { { "MONK", 22u, 1u }, { "ALCHEMIST", 22u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ALL", "MONSTERS", "IN A 3X3 AREA", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 68 */ {
        { { "MONK", 22u, 1u }, { "PALADIN", 26u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 69 */ {
        { { "MAGE", 22u, 1u }, { "DRUID", 26u, 1u }, { "MARKSMAN", 28u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "MONSTERS", "IN A STRAIGHT LINE", "OUT OF HAND TO HAND",
        { "*?", "\x8BX\x81\x90\x85\x84", "", "", "" },
    },
    /* file 70 */ {
        { { "MAGE", 22u, 1u }, { "DRUID", 22u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 71 */ {
        { { "MAGE", 24u, 1u }, { "DRUID", 24u, 1u }, { "MARKSMAN", 32u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "", "", "", "." },
    },
    /* file 72 */ {
        { { "MONK", 24u, 1u }, { "ALCHEMIST", 24u, 1u }, { "PALADIN", 32u, 1u }, { "DRUID", 32u, 1u }, { "", 0u, 0u }, { "", 0u, 0u } }, 4u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 73 */ {
        { { "ALCHEMIST", 24u, 1u }, { "DRUID", 24u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "\x82\x83", "DONE", "", "\x81", "" },
    },
    /* file 74 */ {
        { { "MONK", 24u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "OVERNOR", "", "", "", "" },
    },
    /* file 75 */ {
        { { "MAGE", 26u, 1u }, { "DRUID", 26u, 1u }, { "MARKSMAN", 30u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "", "OUT OF HAND TO HAND",
        { "LLAGE", "", "", "", "" },
    },
    /* file 76 */ {
        { { "MONK", 26u, 1u }, { "PALADIN", 30u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "ERIENCE", "", "", "", "" },
    },
    /* file 77 */ {
        { { "MAGE", 26u, 1u }, { "MARKSMAN", 36u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "", "OUT OF HAND TO HAND",
        { "", "", "E", "", "" },
    },
    /* file 78 */ {
        { { "MONK", 26u, 1u }, { "ALCHEMIST", 26u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "@", "", "", "" },
    },
    /* file 79 */ {
        { { "MONK", 27u, 0u }, { "ALCHEMIST", 27u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "E", "", "", "", "" },
    },
    /* file 80 */ {
        { { "MONK", 28u, 1u }, { "ALCHEMIST", 28u, 1u }, { "PALADIN", 34u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "", "", "\x90", " FOOD" },
    },
    /* file 81 */ {
        { { "MONK", 28u, 1u }, { "ALCHEMIST", 28u, 1u }, { "PALADIN", 28u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "U", "", "" },
    },
    /* file 82 */ {
        { { "MAGE", 28u, 1u }, { "DRUID", 28u, 1u }, { "MARKSMAN", 34u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "", "????", "", "" },
    },
    /* file 83 */ {
        { { "ALCHEMIST", 30u, 1u }, { "DRUID", 30u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "\x80", "", "" },
    },
    /* file 84 */ {
        { { "MONK", 30u, 1u }, { "PALADIN", 36u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 85 */ {
        { { "MAGE", 30u, 1u }, { "DRUID", 34u, 1u }, { "MARKSMAN", 38u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ALL", "VISIBLE MONSTERS", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 86 */ {
        { { "MAGE", 32u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "MONSTERS", "IN A STRAIGHT LINE", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 87 */ {
        { { "DRUID", 32u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "MONSTER", "AT A DISTANCE", "OUT OF HAND TO HAND",
        { "", "", "?\x81l", "", "" },
    },
    /* file 88 */ {
        { { "MONK", 32u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "UNDEAD", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 89 */ {
        { { "MONK", 32u, 1u }, { "PALADIN", 38u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 90 */ {
        { { "MAGE", 32u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "MONSTER", "", "OUT OF HAND TO HAND",
        { "", "", "\x81\x84", "HELLO", "" },
    },
    /* file 91 */ {
        { { "ALCHEMIST", 32u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "VISIBLE MONSTERS", "", "ANYTIME",
        { "", "\x86", "RAIN NOW", "", "" },
    },
    /* file 92 */ {
        { { "ALCHEMIST", 36u, 1u }, { "MAGE", 34u, 1u }, { "DRUID", 36u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 3u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 93 */ {
        { { "MAGE", 34u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "MONSTERS", "IN HAND TO HAND", "IN HAND TO HAND",
        { "NISHED", "", "", "LO-2", "" },
    },
    /* file 94 */ {
        { { "ALCHEMIST", 34u, 1u }, { "DRUID", 34u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 95 */ {
        { { "MONK", 34u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 96 */ {
        { { "MONK", 34u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "CHARACTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
    /* file 97 */ {
        { { "MAGE", 36u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "VISIBLE MONSTERS", "", "ANYTIME",
        { "", "", "OFFER", "", "" },
    },
    /* file 98 */ {
        { { "MONK", 36u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "CHARACTERS", "", "ANYTIME",
        { "", "\x8B", "", "", "" },
    },
    /* file 99 */ {
        { { "DRUID", 36u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "MONSTER", "", "OUT OF HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 100 */ {
        { { "MONK", 36u, 1u }, { "ALCHEMIST", 38u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 101 */ {
        { { "MAGE", 36u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 102 */ {
        { { "MAGE", 38u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ALL", "VISIBLE MONSTERS", "", "ANYTIME",
        { "", "", "\x87", "", "" },
    },
    /* file 103 */ {
        { { "MONK", 38u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 1u,
        "ONE", "MONSTER", "IN HAND TO HAND", "IN HAND TO HAND",
        { "", "", "", "", "" },
    },
    /* file 104 */ {
        { { "MAGE", 38u, 1u }, { "DRUID", 38u, 1u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u }, { "", 0u, 0u } }, 2u,
        "ONE", "MONSTER", "", "ANYTIME",
        { "", "", "", "", "" },
    },
};
static const unsigned int spell_detail_f4_icons[YT2_LIST_ICON_COUNT] = {
    32u, 325u, 327u, 340u, 329u, 331u, 333u
};

/* First private code; spell_effect_priv holds 18 glyphs of six bytes. */
#define SPELL_EFFECT_PRIV_FIRST 0x80u

static void draw_private(yt2_surface *surface, int x, int y,
                         const uint8_t *glyph, uint8_t color)
{
    for (unsigned int row = 0; row < YT2_TEXT_GLYPH_HEIGHT; row++) {
        int pixel_y = y + (int)row;

        if (pixel_y < 0 || (unsigned int)pixel_y >= surface->height) {
            continue;
        }
        for (unsigned int column = 0; column < YT2_TEXT_GLYPH_WIDTH;
             column++) {
            int pixel_x = x + (int)column;

            if (pixel_x < 0 || (unsigned int)pixel_x >= surface->width) {
                continue;
            }
            if ((glyph[row] << column) & 0x80u) {
                surface->pixels[(size_t)pixel_y * surface->stride +
                                (size_t)pixel_x] = color;
            }
        }
    }
}

/* An EFFECT line: normal bytes go through the text face, bytes at or above
 * SPELL_EFFECT_PRIV_FIRST index the private symbol glyphs. */
static void draw_effect_line(yt2_surface *surface, int x, int y,
                             const char *text, uint8_t color)
{
    char run[64];
    size_t length = 0u;
    int cursor = x;

    for (const unsigned char *at = (const unsigned char *)text; *at != 0;
         at++) {
        if (*at < SPELL_EFFECT_PRIV_FIRST) {
            if (length + 1u < sizeof(run)) {
                run[length++] = (char)*at;
            }
            continue;
        }
        if (length > 0u) {
            run[length] = 0;
            yt2_text_draw(surface, cursor, y, run, color);
            cursor += (int)(length * YT2_TEXT_GLYPH_WIDTH);
            length = 0u;
        }
        draw_private(surface, cursor, y,
                     spell_effect_priv +
                         (*at - SPELL_EFFECT_PRIV_FIRST) *
                             YT2_TEXT_GLYPH_HEIGHT,
                     color);
        cursor += (int)YT2_TEXT_GLYPH_WIDTH;
    }
    if (length > 0u) {
        run[length] = 0;
        yt2_text_draw(surface, cursor, y, run, color);
    }
}

static uint16_t read_u16(const uint8_t *at)
{
    return (uint16_t)((uint16_t)at[0] | ((uint16_t)at[1] << 8));
}

static void draw_value(yt2_surface *surface, int y, uint16_t value)
{
    char text[8];

    snprintf(text, sizeof(text), "%u", value);
    yt2_text_draw(surface, YT2_SPELL_DETAIL_VALUE_X, y, text,
                  YT2_SPELL_DETAIL_TRAINING_COLOR);
}

static int render_index(yt2_surface *surface,
                          const yt2_pictures *pictures,
                          const char *name, const uint8_t *record,
                          const spell_detail_entry *entry,
                          const unsigned int *icons)
{
    const yt2_picture_group *group;
    const uint8_t *image;
    char text[8];

    group = yt2_pictures_group(pictures, YT2_SPELL_DETAIL_PICTURE_GROUP);
    image = yt2_picture_data(pictures, YT2_SPELL_DETAIL_PICTURE_GROUP,
                             YT2_SPELL_DETAIL_PICTURE_INDEX);
    if (group == NULL || image == NULL) {
        return YT2_STATUS_ERROR;
    }

    yt2_surface_clear(surface, 0u);
    if (yt2_surface_blit(surface, YT2_SPELL_DETAIL_PICTURE_X,
                         YT2_SPELL_DETAIL_PICTURE_Y, image, group->width,
                         group->height) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }

    yt2_text_draw(surface, YT2_SPELL_DETAIL_NAME_X, YT2_SPELL_DETAIL_TITLE_Y,
                  name, YT2_SPELL_DETAIL_TITLE_COLOR);
    yt2_text_draw(surface, YT2_SPELL_DETAIL_HEADER_X,
                  YT2_SPELL_DETAIL_TITLE_Y, YT2_SPELLS_HEADER,
                  YT2_SPELL_DETAIL_TITLE_COLOR);

    yt2_text_draw(surface, YT2_SPELL_DETAIL_CLASS_LABEL_X,
                  YT2_SPELL_DETAIL_CLASS_LABEL_Y, "CLASS:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    yt2_text_draw(surface, YT2_SPELL_DETAIL_LEVEL_LABEL_X,
                  YT2_SPELL_DETAIL_CLASS_LABEL_Y, "LEVEL:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    for (unsigned int row = 0u; row < entry->row_count; row++) {
        int y = YT2_SPELL_DETAIL_ROW_Y +
                (int)row * YT2_SPELL_DETAIL_ROW_STEP;

        yt2_text_draw(surface, YT2_SPELL_DETAIL_CLASS_X, y,
                      entry->rows[row].class_name,
                      YT2_SPELL_DETAIL_TEXT_COLOR);
        snprintf(text, sizeof(text), "%u", entry->rows[row].level);
        yt2_text_draw(surface, YT2_SPELL_DETAIL_LEVEL_X, y, text,
                      YT2_SPELL_DETAIL_TEXT_COLOR);
        yt2_text_draw(surface, YT2_SPELL_DETAIL_TYPE_X, y,
                      type_names[entry->rows[row].type],
                      type_colors[entry->rows[row].type]);
    }

    yt2_text_draw(surface, YT2_SPELL_DETAIL_MP_LABEL_X,
                  YT2_SPELL_DETAIL_MP_Y, "MP:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    draw_value(surface, YT2_SPELL_DETAIL_MP_Y, read_u16(record + 24));
    yt2_text_draw(surface, YT2_SPELL_DETAIL_NUORE_LABEL_X,
                  YT2_SPELL_DETAIL_NUORE_Y, "NUORE:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    draw_value(surface, YT2_SPELL_DETAIL_NUORE_Y, read_u16(record + 26));
    yt2_text_draw(surface, YT2_SPELL_DETAIL_ORE_LABEL_X,
                  YT2_SPELL_DETAIL_ORE_Y, "ORE:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    draw_value(surface, YT2_SPELL_DETAIL_ORE_Y, read_u16(record + 28));

    yt2_text_draw(surface, YT2_SPELL_DETAIL_AFFECTS_LABEL_X,
                  YT2_SPELL_DETAIL_AFFECTS_Y, "AFFECTS:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    if (entry->scope[0] != 0) {
        yt2_text_draw(surface, YT2_SPELL_DETAIL_SCOPE_X,
                      YT2_SPELL_DETAIL_AFFECTS_Y, entry->scope,
                      YT2_SPELL_DETAIL_SCROLL_COLOR);
        yt2_text_draw(surface, YT2_SPELL_DETAIL_MIDDLE_X,
                      YT2_SPELL_DETAIL_AFFECTS_Y, entry->middle,
                      YT2_SPELL_DETAIL_TEXT_COLOR);
        if (entry->target[0] != 0) {
            yt2_text_draw(surface,
                          YT2_SPELL_DETAIL_MIDDLE_X +
                              (int)(strlen(entry->middle) *
                                    YT2_TEXT_GLYPH_WIDTH) +
                              (int)YT2_TEXT_GLYPH_WIDTH,
                          YT2_SPELL_DETAIL_AFFECTS_Y, entry->target,
                          YT2_SPELL_DETAIL_SCROLL_COLOR);
        }
    }

    yt2_text_draw(surface, YT2_SPELL_DETAIL_WHEN_LABEL_X,
                  YT2_SPELL_DETAIL_WHEN_Y, "WHEN:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    yt2_text_draw(surface, YT2_SPELL_DETAIL_WHEN_X, YT2_SPELL_DETAIL_WHEN_Y,
                  entry->when, YT2_SPELL_DETAIL_CREATION_COLOR);

    yt2_text_draw(surface, YT2_SPELL_DETAIL_EFFECT_LABEL_X,
                  YT2_SPELL_DETAIL_EFFECT_Y, "EFFECT:",
                  YT2_SPELL_DETAIL_LABEL_COLOR);
    for (unsigned int slot = 0u; slot < YT2_SPELL_DETAIL_EFFECT_SLOTS;
         slot++) {
        if (entry->effect[slot][0] == 0) {
            continue;
        }
        draw_effect_line(surface, YT2_SPELL_DETAIL_EFFECT_X,
                         YT2_SPELL_DETAIL_EFFECT_FIRST_Y +
                             (int)slot * YT2_SPELL_DETAIL_EFFECT_STEP,
                         entry->effect[slot], YT2_SPELL_DETAIL_TEXT_COLOR);
    }

    yt2_text_draw(surface, YT2_SPELL_DETAIL_FOOTER_X,
                  YT2_SPELL_DETAIL_FOOTER_Y, YT2_SPELL_DETAIL_FOOTER_TEXT,
                  YT2_SPELL_DETAIL_FOOTER_COLOR);
    for (unsigned int index = 0; index < YT2_LIST_ICON_COUNT; index++) {
        const uint8_t *icon = yt2_picture_data(pictures, YT2_LIST_ICON_GROUP,
                                               icons[index]);

        if (icon == NULL) {
            return YT2_STATUS_ERROR;
        }
        if (yt2_surface_blit_transparent(
                surface, YT2_LIST_ICON_X + (int)index * YT2_LIST_ICON_STEP,
                YT2_LIST_ICON_Y, icon, YT2_LIST_ICON_SIZE,
                YT2_LIST_ICON_SIZE, YT2_LIST_ICON_TRANSPARENT) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    return YT2_STATUS_OK;
}

int yt2_spell_detail_render(yt2_surface *surface,
                            const yt2_pictures *pictures,
                            const yt2_spells *spells, unsigned int rank)
{
    const uint8_t *record;
    char name[YT2_SPELLS_NAME_SIZE + 1u];

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        spells == NULL || spells->bytes == NULL ||
        rank >= YT2_SPELLS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    record = yt2_spell_record(spells, rank);
    if (record == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (yt2_spell_name(spells, rank, name, sizeof(name)) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return render_index(surface, pictures, name, record,
                        &spell_details[spells->order[rank]],
                        spell_detail_icons);
}

int yt2_spell_detail_render_index(yt2_surface *surface,
                                  const yt2_pictures *pictures,
                                  const yt2_spells *spells,
                                  unsigned int file_index, int f4_icons)
{
    char name[YT2_SPELLS_NAME_SIZE + 1u];

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        spells == NULL || spells->bytes == NULL ||
        file_index >= YT2_SPELLS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    if (yt2_spell_name_by_index(spells, file_index, name, sizeof(name)) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return render_index(
        surface, pictures, name,
        spells->bytes + YT2_SPELLS_OFFSET +
            (size_t)file_index * YT2_SPELLS_RECORD_SIZE,
        &spell_details[file_index],
        f4_icons ? spell_detail_f4_icons : spell_detail_icons);
}
