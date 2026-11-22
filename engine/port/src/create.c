#include "yt2/create.h"

#include <stdio.h>
#include <string.h>

#include "yt2/endian.h"
#include "yt2/item_detail.h"
#include "yt2/text.h"

/*
 * Character creation, the roster and the sheet viewer.
 *
 * Function map to the disassembly:
 *   yt2_create_begin/key/click  fn_13EC4 + sub_1585A/146E9/14D71/14F08/148C4
 *                               + sub_14CA1/15419 (class, portrait, sheet,
 *                               items, name, preview, keep, quit)
 *   yt2_create_roll             sub_15605 (roll) + sub_1426E (derive) +
 *                               fn_0B3D2 (equipment)
 *   yt2_create_grant_spells     sub_1576C
 *   yt2_roster_*                fn_1C354 (grid) + fn_13F2E (sheet viewer
 *                               with delete) + sub_1C576 (DONE)
 * Mouse regions are the DS tables 66DCh/6742h/5F84h/606Eh/6098h/60F4h/5B58h
 * dumped from the reference binary; keys are the v_KeyCode compares.
 *
 * Literal style: countable quantities (coordinates, counts, thresholds,
 * table indices, file/region IDs) are decimal; bit flags and masks,
 * palette colors, and ASCII control codes stay hex.
 */

const uint16_t yt2_create_item_ids[YT2_CREATE_ITEMS] = {
    542u, 412u, 418u, 424u, 362u, 312u, 6u, 54u
};

/* Class names (DS:7985h, 11 bytes each) and their hotkeys. */
static const char *const class_names[] = {
    "FIGHTER   ", "MERCHANT  ", "ROGUE     ", "MONK      ", "ALCHEMIST ",
    "PALADIN   ", "MAGE      ", "DRUID     ", "MARKSMAN  "
};
static const char class_keys[] = { 'F', 'M', 'R', 'O', 'A', 'P', 'G', 'D', 'K' };
/* Red-letter positions for fn_13E08 (1-based, per row). */
static const unsigned int class_red[] = { 1, 1, 1, 2, 1, 1, 3, 1, 4 };
static const int class_rows[] = {
    51, 60, 69, 96, 105, 114, 141, 150, 159
};

/* sub_1576C starting spells and school bits per class 4..9. */
static const uint16_t grant_spells[][2] = {
    { 1, 3 }, { 1, 2 }, { 1, 0 }, { 2, 3 }, { 1, 2 }, { 2, 0 }
};
static const uint16_t grant_bits[] = { 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u };

/* A mouse region: x0, x1, y0, y1, id (fn_0D1DE, 10 bytes each). */
typedef struct create_region {
    int x0;
    int x1;
    int y0;
    int y1;
    unsigned int id;
} create_region;

static int hit_test(const create_region *table, size_t count, int x, int y)
{
    for (size_t i = 0; i < count; i++) {
        if (x >= table[i].x0 && x <= table[i].x1 && y >= table[i].y0 &&
            y <= table[i].y1) {
            return (int)table[i].id;
        }
    }
    return 0;
}

/* DS:66DCh: class rows + QUIT. */
static const create_region class_table[] = {
    { 8, 107, 51, 57, 1 }, { 8, 107, 60, 66, 2 }, { 8, 107, 69, 74, 3 },
    { 8, 107, 96, 101, 4 }, { 8, 107, 105, 110, 5 }, { 8, 107, 114, 119, 6 },
    { 8, 107, 141, 146, 7 }, { 8, 107, 150, 155, 8 }, { 8, 107, 159, 164, 9 },
    { 8, 107, 185, 190, 17 }
};

/* DS:6742h: portrait grid + sex toggle + QUIT. */
static const create_region portrait_table[] = {
    { 8, 39, 42, 73, 1 }, { 41, 72, 42, 73, 2 }, { 74, 105, 42, 73, 3 },
    { 8, 39, 75, 106, 4 }, { 41, 72, 75, 106, 5 }, { 74, 105, 75, 106, 6 },
    { 8, 39, 108, 139, 7 }, { 41, 72, 108, 139, 8 }, { 74, 105, 108, 139, 9 },
    { 8, 107, 150, 155, 13 }, { 8, 107, 185, 190, 17 }
};

/* DS:5F84h: option rows 1..8 + QUIT (sheet, preview, viewer, confirm). */
static const create_region option_table[] = {
    { 8, 107, 42, 47, 1 }, { 8, 107, 51, 57, 2 }, { 8, 107, 60, 66, 3 },
    { 8, 107, 69, 74, 4 }, { 8, 107, 78, 83, 5 }, { 8, 107, 87, 92, 6 },
    { 8, 107, 96, 101, 7 }, { 8, 107, 105, 110, 8 },
    { 8, 107, 185, 190, 17 }
};

/* DS:606Eh: items screen (list, paperdoll, NAME, QUIT). */
static const create_region items_table[] = {
    { 7, 108, 41, 175, 1 }, { 116, 169, 60, 195, 2 },
    { 8, 107, 176, 181, 16 }, { 8, 107, 185, 190, 17 }
};

/* DS:6098h: the eight take rows + QUIT. */
static const create_region take_table[] = {
    { 8, 107, 42, 57, 1 }, { 8, 107, 58, 73, 2 }, { 8, 107, 74, 89, 3 },
    { 8, 107, 90, 105, 4 }, { 8, 107, 106, 121, 5 }, { 8, 107, 122, 137, 6 },
    { 8, 107, 138, 154, 7 }, { 8, 107, 154, 169, 8 },
    { 8, 107, 185, 190, 17 }
};

#define PAPERDOLL_ORIGIN_X 116
#define PAPERDOLL_ORIGIN_Y 60

uint16_t yt2_create_rand(uint16_t *seed, uint16_t time_word, uint16_t max)
{
    uint32_t product;
    uint16_t current;
    uint16_t masked;
    uint16_t span;
    unsigned int keep;

    if (max == 0u) {
        return 0u;
    }
    current = *seed;
    if (current == 0u) {
        current = time_word;
    }
    product = (uint32_t)(uint16_t)(0u - current) * 2053u;
    masked = (uint16_t)(product & 0xFFFFu);
    masked = (uint16_t)(((masked & 0xFFu) << 8) | (masked >> 8));
    *seed = masked;
    {
        /* The js tests the 16-bit SUB sign flag, not the C int sign. */
        uint16_t high = (uint16_t)(product >> 16);
        uint16_t diff = (uint16_t)(high - masked);

        if ((diff & 0x8000u) == 0u) {
            masked = (uint16_t)(masked + 1u);
        }
    }
    span = max;
    keep = 0u;
    for (;;) {
        uint16_t shifted = (uint16_t)(span << 1);

        if ((span & 0x8000u) != 0u) {
            break;
        }
        keep++;
        span = shifted;
    }
    /* 16-bit shl/shr pair: keep the low (16 - keep) bits. */
    masked = (uint16_t)(masked & (0xFFFFu >> keep));
    if (masked > max) {
        masked = (uint16_t)(masked - max);
    }
    return masked;
}

uint16_t yt2_create_percent(uint16_t value, uint16_t percent)
{
    return (uint16_t)(((uint32_t)value * percent + 50u) / 100u);
}

int yt2_create_alloc_slot(const yt2_party *party)
{
    unsigned int index;

    if (party == NULL) {
        return -1;
    }
    for (index = 0; index < YT2_ROSTER_SLOTS; index++) {
        if (party->roster[index].class_id == YT2_CLASS_NONE) {
            return (int)index;
        }
    }
    return -1;
}

static void log_sfx(yt2_create *create, unsigned int id)
{
    if (create->sfx_count < YT2_CREATE_SFX_LOG) {
        create->sfx[create->sfx_count++] = id;
    }
}

/* Item record helpers (fn_02262 copies, read straight from the table). */

static const uint8_t *item_record(const yt2_create *create, unsigned int id)
{
    if (id == 0u || id > YT2_ITEMS_COUNT) {
        return NULL;
    }
    return yt2_item_record(create->items, id - 1u);
}

static uint16_t record_u16(const uint8_t *record, unsigned int offset)
{
    return yt2_read_u16(record + offset);
}

/* sub_15605: the six attributes plus hit points, magic points, capacity. */

static void roll_attributes(yt2_create *create, yt2_character *character)
{
    yt2_attributes *attributes = &character->current.attributes;
    yt2_attributes *max = &character->maximum.attributes;
    uint16_t casting_base;
    uint16_t casting_extra;
    unsigned int class_id = (unsigned int)character->class_id;

    /* Roll order matters: STA comes last (sub_15605 writes +40h sixth). */
    attributes->strength =
        (uint16_t)(yt2_create_rand(&create->rng_seed, create->time_word, 15u) +
                   45u);
    attributes->dexterity =
        (uint16_t)(yt2_create_rand(&create->rng_seed, create->time_word, 15u) +
                   45u);
    attributes->intelligence =
        (uint16_t)(yt2_create_rand(&create->rng_seed, create->time_word, 15u) +
                   45u);
    attributes->wisdom =
        (uint16_t)(yt2_create_rand(&create->rng_seed, create->time_word, 15u) +
                   45u);
    attributes->charisma =
        (uint16_t)(yt2_create_rand(&create->rng_seed, create->time_word, 15u) +
                   45u);
    attributes->stamina =
        (uint16_t)(yt2_create_rand(&create->rng_seed, create->time_word, 15u) +
                   45u);
    *max = *attributes;

    character->current.combat.hit_points =
        yt2_create_percent(attributes->stamina, 25u);
    character->maximum.combat.hit_points =
        character->current.combat.hit_points;
    character->current.combat.carry_capacity =
        (uint16_t)(attributes->strength * 10u);
    character->maximum.combat.carry_capacity =
        character->current.combat.carry_capacity;

    casting_base = 0u;
    casting_extra = 0u;
    switch (class_id) {
    case 4u:
        casting_base = max->wisdom;
        casting_extra = 10u;
        break;
    case 5u:
        casting_base =
            (uint16_t)(yt2_create_percent(max->wisdom, 75u) +
                       yt2_create_percent(max->intelligence, 25u));
        casting_extra = 5u;
        break;
    case 6u:
        casting_base = yt2_create_percent(max->wisdom, 50u);
        casting_extra = casting_base;
        if ((max->wisdom & 1u) != 0u) {
            casting_extra--;
        }
        break;
    case 7u:
        casting_base = max->intelligence;
        casting_extra = 10u;
        break;
    case 8u:
        casting_base =
            (uint16_t)(yt2_create_percent(max->intelligence, 75u) +
                       yt2_create_percent(max->wisdom, 25u));
        casting_extra = 5u;
        break;
    case 9u:
        casting_base = yt2_create_percent(max->intelligence, 50u);
        casting_extra = casting_base;
        if ((max->intelligence & 1u) != 0u) {
            casting_extra--;
        }
        break;
    default:
        break;
    }
    character->current.combat.magic_points = (uint16_t)(casting_base >> 2);
    character->maximum.combat.magic_points =
        character->current.combat.magic_points;
    character->current.skills.casting =
        (uint16_t)(casting_base + casting_extra);
    character->maximum.skills.casting = character->current.skills.casting;
}

/* sub_1426E: the derived skills from attributes plus class bonuses. */

static void derive_skills(yt2_character *character)
{
    yt2_attributes *a = &character->current.attributes;
    yt2_skills *s = &character->current.skills;
    unsigned int class_id = (unsigned int)character->class_id;
    int16_t bonus;

    bonus = class_id == 1u ? 5 : class_id == 2u ? 4 : class_id == 3u ? 3
                         : class_id == 9u      ? 2
                         : class_id == 6u      ? 1
                                               : 0;
    s->survival = (uint16_t)(yt2_create_percent(a->strength, 10u) +
                             yt2_create_percent(a->dexterity, 30u) +
                             yt2_create_percent(a->stamina, 60u) + bonus);

    bonus = (class_id == 1u || class_id == 3u) ? 5
        : (class_id == 4u || class_id == 5u || class_id == 7u ||
           class_id == 8u)                     ? -5
                                               : 0;
    s->projectile = (uint16_t)(yt2_create_percent(a->strength, 80u) +
                               yt2_create_percent(a->dexterity, 20u) + bonus);

    bonus = class_id == 1u ? 5
        : (class_id == 4u || class_id == 5u || class_id == 7u ||
           class_id == 8u)                     ? -7
        : (class_id == 6u || class_id == 9u)   ? -5
                                               : 0;
    s->slashing = (uint16_t)(yt2_create_percent(a->strength, 20u) +
                             yt2_create_percent(a->dexterity, 80u) + bonus);
    s->bashing = (uint16_t)(a->strength + bonus);
    s->polearm = (uint16_t)(yt2_create_percent(a->strength, 50u) +
                            yt2_create_percent(a->dexterity, 50u) + bonus);

    if (class_id == 1u || class_id == 6u || class_id == 7u) {
        s->thievery = 40u;
    } else if (class_id == 4u || class_id == 8u) {
        s->thievery = 0u;
    } else {
        bonus = class_id == 3u ? 5 : class_id == 2u ? -7 : 0;
        s->thievery = (uint16_t)(a->dexterity + bonus);
    }

    bonus = (class_id == 6u || class_id == 9u) ? 5
        : (class_id == 2u || class_id == 3u)   ? 3
                                               : 0;
    s->navigation = (uint16_t)(a->dexterity + bonus);

    if (class_id == 5u || class_id == 9u) {
        s->bartering = 0u;
    } else if (class_id == 1u || class_id == 4u || class_id == 7u ||
               class_id == 8u) {
        s->bartering = 40u;
    } else {
        bonus = class_id == 2u ? 5 : class_id == 3u ? -2 : 0;
        s->bartering = (uint16_t)(yt2_create_percent(a->charisma, 85u) +
                                  yt2_create_percent(a->intelligence, 15u) +
                                  bonus);
    }

    if (class_id == 6u || class_id == 7u) {
        s->repair = 0u;
    } else if (class_id == 4u || class_id == 5u || class_id == 9u) {
        s->repair = 40u;
    } else {
        bonus = class_id == 2u ? 5 : class_id == 1u ? 3 : class_id == 3u ? -2
                                                                        : 0;
        s->repair = (uint16_t)(yt2_create_percent(a->dexterity, 80u) +
                               yt2_create_percent(a->intelligence, 20u) +
                               bonus);
    }

    if (class_id == 1u || class_id == 3u || class_id == 6u || class_id == 5u) {
        s->mapping = 40u;
    } else {
        bonus = class_id == 7u ? 5 : (class_id == 9u || class_id == 8u) ? 3
                                                                       : 0;
        s->mapping = (uint16_t)(yt2_create_percent(a->intelligence, 90u) +
                                yt2_create_percent(a->wisdom, 10u) + bonus);
    }

    if (class_id == 1u || class_id == 3u) {
        s->linguistics = 0u;
    } else if (class_id == 4u || class_id == 5u || class_id == 6u) {
        s->linguistics = 40u;
    } else {
        /* Class 9 shares class 2's -5 by fallthrough (loc_14631). */
        bonus = class_id == 8u ? 5
            : class_id == 7u   ? 1
            : (class_id == 2u || class_id == 9u) ? -5
                                                : 0;
        s->linguistics =
            (uint16_t)(yt2_create_percent(a->intelligence, 70u) +
                       yt2_create_percent(a->wisdom, 30u) + bonus);
    }

    if (class_id == 1u || class_id == 2u || class_id == 3u) {
        s->chemistry = 0u;
    } else if (class_id == 7u || class_id == 8u || class_id == 9u) {
        s->chemistry = 40u;
    } else {
        bonus = class_id == 5u ? 5 : class_id == 6u ? -5 : 0;
        s->chemistry = (uint16_t)(yt2_create_percent(a->intelligence, 20u) +
                                  yt2_create_percent(a->wisdom, 80u) + bonus);
    }

    character->maximum.skills = character->current.skills;
}

void yt2_create_apply_equipment(yt2_create *create, yt2_character *character)
{
    /*
     * fn_0B3D2, shared with the in-game panes: fresh slots come
     * zeroed (base[]/max_base[] stay zero through creation, so
     * the copy step resets to zero) and the fixed take list has
     * no bonus lists (the paperdoll bonus hook no-ops).
     */
    if (create == NULL || character == NULL) {
        return;
    }
    yt2_paperdoll_recompute(&create->paperdoll, character);
}

void yt2_create_roll(yt2_create *create, yt2_character *character)
{
    /* sub_15605 zeroes the hidden ACC bases (+32h/+72h/+34h/+74h)
       on every roll; the other three pairs stay alloc-zeroed. */
    character->base[0] = 0u;
    character->base[2] = 0u;
    character->max_base[0] = 0u;
    character->max_base[2] = 0u;
    roll_attributes(create, character);
    derive_skills(character);
    yt2_create_apply_equipment(create, character);
}

const char *yt2_create_rating(const yt2_character *character)
{
    unsigned int total;

    if (character->level != 1u) {
        return NULL;
    }
    total = character->current.attributes.strength +
            character->current.attributes.dexterity +
            character->current.attributes.stamina +
            character->current.attributes.intelligence +
            character->current.attributes.wisdom +
            character->current.attributes.charisma;
    total /= 6u;
    if (total > 55u) {
        return "GREAT  ";
    }
    if (total > 52u) {
        return "GOOD   ";
    }
    if (total > 49u) {
        return "AVERAGE";
    }
    return "POOR   ";
}

void yt2_create_grant_spells(yt2_character *character)
{
    unsigned int class_id = (unsigned int)character->class_id;
    unsigned int row;

    if (class_id < 4u || class_id > 9u) {
        return;
    }
    if (character->maximum.combat.magic_points == 0u) {
        return;
    }
    row = class_id - 4u;
    character->status =
        (uint16_t)(character->status | grant_bits[row]);
    for (unsigned int leg = 0; leg < 2u; leg++) {
        uint16_t spell = grant_spells[row][leg];
        unsigned int word;
        unsigned int bit;

        if (spell == 0u) {
            break;
        }
        /* sub_17FD1: leg (spell/16), bit 0x8000 >> ((spell%16)-1). */
        word = spell / 16u;
        bit = spell % 16u;
        if (bit == 0u) {
            bit = 16u;
            word--;
        }
        if (word < YT2_SPELLBOOK_WORDS) {
            character->spellbook[word] =
                (uint16_t)(character->spellbook[word] |
                           (0x8000u >> (bit - 1u)));
        }
    }
}

/* Painters. */

#define CREATE_BG_GROUP 0u
#define CREATE_BG_PIC 3u
#define CREATE_BG_X 1
#define CREATE_BG_Y 1

#define COLOR_TITLE 0x8Au
#define COLOR_TEXT 0x0Fu
#define COLOR_RED 0x7Bu
#define COLOR_DIM 0x33u
#define COLOR_RATING 0xDFu
#define COLOR_HIGHLIGHT 0xCBu
#define COLOR_HIGHLIGHT_DIM 0x9Bu

static int blit_pic(const yt2_create *create, unsigned int group,
                    unsigned int pic, int x, int y, int transparent)
{
    const uint8_t *pixels;
    const yt2_picture_group *info;

    info = yt2_pictures_group(create->pictures, group);
    if (info == NULL) {
        return YT2_STATUS_ERROR;
    }
    pixels = yt2_picture_data(create->pictures, group, pic);
    if (pixels == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (transparent) {
        return yt2_surface_blit_transparent(create->surface, x, y, pixels,
                                            info->width, info->height, 0xFFu);
    }
    return yt2_surface_blit(create->surface, x, y, pixels, info->width,
                            info->height);
}

/* fn_13E74: plain transparent line. */
static void paint_line(yt2_create *create, int x, int y, const char *text,
                       uint8_t color)
{
    yt2_text_draw(create->surface, x, y, text, color);
}

/* fn_13E08/fn_13D92: transparent line with the red-th (1-based) letter red. */
static void paint_red(yt2_create *create, int x, int y, const char *text,
                      unsigned int red)
{
    char cell[2];
    unsigned int at = 1u;

    cell[1] = '\0';
    for (const char *cursor = text; *cursor != '\0'; cursor++, at++) {
        cell[0] = *cursor;
        yt2_text_draw(create->surface, x, y, cell,
                      at == red ? COLOR_RED : COLOR_TEXT);
        x += (int)YT2_TEXT_GLYPH_WIDTH;
    }
}

/* sub_153A7: plain %u, opaque, dimmed when current exceeds maximum. */
static void paint_number(yt2_create *create, int x, int y, uint16_t current,
                         uint16_t maximum)
{
    char text[16];

    snprintf(text, sizeof(text), "%u", current);
    yt2_text_draw_opaque(create->surface, x, y, text,
                         current > maximum ? COLOR_TITLE : COLOR_TEXT,
                         COLOR_DIM);
}

/* sub_153D1: commas, opaque. */
static void paint_commas(yt2_create *create, int x, int y, uint16_t current,
                         uint16_t maximum)
{
    char text[16];

    yt2_item_format_commas(text, sizeof(text), current);
    yt2_text_draw_opaque(create->surface, x, y, text,
                         current > maximum ? COLOR_TITLE : COLOR_TEXT,
                         COLOR_DIM);
}

/* fn_09C46: four experience bytes as hex pairs with commas, or "0". */
static void paint_emp(yt2_create *create, int x, int y, uint32_t xp)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[12];
    char *out = text;

    if (xp == 0u) {
        yt2_text_draw_opaque(create->surface, x, y, "0", COLOR_TEXT,
                             COLOR_DIM);
        return;
    }
    for (unsigned int i = 0; i < 4u; i++) {
        unsigned int byte = (xp >> (i * 8u)) & 0xFFu;

        if (i > 0u) {
            *out++ = ',';
        }
        *out++ = digits[byte >> 4];
        *out++ = digits[byte & 15u];
    }
    *out = '\0';
    yt2_text_draw_opaque(create->surface, x, y, text, COLOR_TEXT, COLOR_DIM);
}

/* fn_12691 + title (sub_1585A tail). */
static void paint_setup(yt2_create *create)
{
    blit_pic(create, CREATE_BG_GROUP, CREATE_BG_PIC, CREATE_BG_X, CREATE_BG_Y,
             0);
    paint_line(create, 107, 6, "CHARACTER CREATION", COLOR_TEXT);
}

/*
 * Left-panel restore (sub_146A1): the snapshot over x 0..107, y 25..199.
 * The snapshot is the post-background screen, so column 0 is the black
 * border (untouched) and columns 1..107 are picture columns 0..106.
 */
static void paint_left_clear(yt2_create *create)
{
    const yt2_picture_group *info =
        yt2_pictures_group(create->pictures, CREATE_BG_GROUP);
    const uint8_t *pixels =
        yt2_picture_data(create->pictures, CREATE_BG_GROUP, CREATE_BG_PIC);

    if (info == NULL || pixels == NULL) {
        return;
    }
    for (unsigned int row = 0; row < 174u; row++) {
        yt2_surface_blit(create->surface, 1, 25 + (int)row,
                         pixels + ((size_t)row + 24u) * info->width,
                         107u, 1u);
    }
    /* Snapshot row 199 is the black border below the picture. */
    memset(create->surface->pixels + (size_t)199 * create->surface->stride + 1,
           0, 107u);
}

/* QUIT "CREATE" / RETURN footer (sub_158AB). */
static void paint_quit(yt2_create *create)
{
    if (create->repick) {
        paint_red(create, 8, 185, "RETURN", 2u);
    } else {
        paint_red(create, 8, 185, "QUIT \"CREATE\"", 1u);
    }
}

/* Right panel: portrait, paperdoll, equipment (fn_1658F creation subset). */

static void paint_equipped_slot(yt2_create *create, unsigned int id, int x,
                                int y, int equipped_view)
{
    const uint8_t *record;
    unsigned int pic;

    if (id == 0u) {
        return;
    }
    record = item_record(create, id);
    if (record == NULL) {
        return;
    }
    pic = record_u16(record, 8u);
    if (equipped_view && (record_u16(record, 12u) & 0x400u) != 0u) {
        pic = record_u16(record, 4u);
    }
    blit_pic(create, 8u, pic, x, y, 1);
}

/* An open bag's variant icon (def icon+1, B1 sub_16D00/16D2B). */
static void paint_open_variant(yt2_create *create, unsigned int id, int x,
                               int y)
{
    const uint8_t *record;

    if (id == 0u) {
        return;
    }
    record = item_record(create, id);
    if (record == NULL) {
        return;
    }
    blit_pic(create, 8u, (unsigned int)record_u16(record, 8u) + 1u, x, y,
             1);
}

static void paint_paperdoll(yt2_create *create, const yt2_character *character)
{
    static const int pack_cells[8][2] = {
        { 2, 2 }, { 20, 2 }, { 38, 2 }, { 2, 19 },
        { 20, 19 }, { 38, 19 }, { 2, 36 }, { 20, 36 }
    };
    static const int effect_xy[5][2] = {
        { 12, 49 }, { 12, 72 }, { 12, 93 }, { 10, 104 }, { 12, 86 }
    };
    unsigned int slot;

    blit_pic(create, 6u, character->portrait_index, PAPERDOLL_ORIGIN_X,
             PAPERDOLL_ORIGIN_Y, 0);
    paint_equipped_slot(create, character->equipment[0].id, PAPERDOLL_ORIGIN_X,
                        PAPERDOLL_ORIGIN_Y + 64, 0);
    if ((character->flags & 0x1000u) != 0u) {
        /* A staged cell-11 bag paints its open variant over cell
           11 instead (B1 sub_16D00); the bag itself stays hidden
           by the grid redirect. */
        paint_open_variant(create, character->stage[2].id,
                           PAPERDOLL_ORIGIN_X + 40, PAPERDOLL_ORIGIN_Y + 64);
    } else {
        paint_equipped_slot(create, character->equipment[1].id,
                            PAPERDOLL_ORIGIN_X + 40, PAPERDOLL_ORIGIN_Y + 64, 0);
    }
    paint_equipped_slot(create, character->equipment[2].id, PAPERDOLL_ORIGIN_X,
                        PAPERDOLL_ORIGIN_Y + 103, 0);
    paint_equipped_slot(create, character->equipment[3].id, PAPERDOLL_ORIGIN_X + 40,
                        PAPERDOLL_ORIGIN_Y + 103, 0);
    paint_equipped_slot(create, character->equipment[4].id, PAPERDOLL_ORIGIN_X + 4,
                        PAPERDOLL_ORIGIN_Y + 95, 1);
    paint_equipped_slot(create, character->equipment[5].id, PAPERDOLL_ORIGIN_X + 44,
                        PAPERDOLL_ORIGIN_Y + 95, 1);
    if ((character->flags & 0x20u) != 0u) {
        blit_pic(create, 8u, character->sex == YT2_SEX_MALE ? 12u : 13u,
                 PAPERDOLL_ORIGIN_X + 40, PAPERDOLL_ORIGIN_Y + 103, 1);
    }
    /* Grid: staged container cells when a bag is open, else the
       pack. Cell 9 shows the open pack bag's variant icon. */
    {
        const yt2_item *grid = character->pack;

        for (slot = 0; slot < YT2_CONTAINER_SLOTS; slot++) {
            if (character->stage[slot].id == 0u) {
                continue;
            }
            grid = character->stage[slot].cells;
            /* Cell-11 bags indicate at cell 11, not cell 9. */
            if (slot == 2u && (character->flags & 0x1000u) != 0u) {
                break;
            }
            paint_open_variant(create, character->stage[slot].id,
                               PAPERDOLL_ORIGIN_X + 38, PAPERDOLL_ORIGIN_Y + 36);
            break;
        }
        for (slot = 0; slot < YT2_PACK_SLOTS; slot++) {
            paint_equipped_slot(create, grid[slot].id,
                                PAPERDOLL_ORIGIN_X + pack_cells[slot][0],
                                PAPERDOLL_ORIGIN_Y + pack_cells[slot][1], 0);
        }
    }
    /* Effects (group 7): B1 resolves through fn_02262's row gate
       (flags 0xE00/0xC000/0x100 pick the row, anything else paints
       nothing); the pic is row+4, +1 unless male. */
    for (slot = 0; slot < YT2_EFFECT_SLOTS; slot++) {
        const uint8_t *record;
        const uint8_t *row;
        unsigned int pic;

        if (character->effects[slot] == 0u) {
            continue;
        }
        record = item_record(create, character->effects[slot]);
        if (record == NULL) {
            continue;
        }
        row = yt2_item_param_row(create->items, record);
        if (row == NULL || (record_u16(record, 12u) & 0xE700u) == 0u) {
            continue;
        }
        pic = yt2_read_u16(row + 4u);
        if (character->sex != YT2_SEX_MALE) {
            pic++;
        }
        blit_pic(create, 7u, pic,
                 PAPERDOLL_ORIGIN_X + effect_xy[slot][0],
                 PAPERDOLL_ORIGIN_Y + effect_xy[slot][1], 1);
    }
}

/* Right panel stats (sub_15046); highlight[i] marks the row's slot. */

static void paint_stat_rows(yt2_create *create, const yt2_character *character,
                            const uint16_t *highlight)
{
    static const int rows[6] = { 60, 70, 80, 90, 100, 110 };
    static const int skill_rows[8] = {
        60, 70, 80, 90, 100, 110, 120, 130
    };
    static const int low_rows[5] = { 140, 150, 160, 170, 180 };
    const yt2_stats *current = &character->current;
    const yt2_stats *max = &character->maximum;
    const uint16_t cur_attrs[6] = {
        current->attributes.strength, current->attributes.dexterity,
        current->attributes.stamina, current->attributes.intelligence,
        current->attributes.wisdom, current->attributes.charisma
    };
    const uint16_t max_attrs[6] = {
        max->attributes.strength, max->attributes.dexterity,
        max->attributes.stamina, max->attributes.intelligence,
        max->attributes.wisdom, max->attributes.charisma
    };
    const uint16_t cur_skills[8] = {
        current->skills.survival, current->skills.projectile,
        current->skills.slashing, current->skills.bashing,
        current->skills.polearm, current->skills.casting,
        current->skills.mapping, current->skills.navigation
    };
    const uint16_t max_skills[8] = {
        max->skills.survival, max->skills.projectile, max->skills.slashing,
        max->skills.bashing, max->skills.polearm, max->skills.casting,
        max->skills.mapping, max->skills.navigation
    };
    const uint16_t cur_low[5] = {
        current->skills.bartering, current->skills.repair,
        current->skills.thievery, current->skills.linguistics,
        current->skills.chemistry
    };
    const uint16_t max_low[5] = {
        max->skills.bartering, max->skills.repair, max->skills.thievery,
        max->skills.linguistics, max->skills.chemistry
    };
    unsigned int slot_number = create->slot + 1u;

    for (unsigned int i = 0; i < 6u; i++) {
        paint_number(create, 203, rows[i], cur_attrs[i], max_attrs[i]);
    }
    for (unsigned int i = 0; i < 8u; i++) {
        paint_number(create, 297, skill_rows[i], cur_skills[i],
                     max_skills[i]);
    }
    for (unsigned int i = 0; i < 5u; i++) {
        if (highlight != NULL && highlight[i] == slot_number) {
            char text[16];

            snprintf(text, sizeof(text), "%u", cur_low[i]);
            yt2_text_draw_opaque(create->surface, 297, low_rows[i], text,
                                 cur_low[i] > max_low[i] ? COLOR_HIGHLIGHT_DIM
                                                        : COLOR_HIGHLIGHT,
                                 COLOR_DIM);
        } else {
            paint_number(create, 297, low_rows[i], cur_low[i], max_low[i]);
        }
    }
}

/* ACC/DAM/ABS rows (sub_14FC3). */
static void paint_combat_rows(yt2_create *create,
                              const yt2_character *character)
{
    /* Opaque blanks (v_0B26 == 0): erase the previous values first. */
    yt2_text_draw_opaque(create->surface, 203, 150, "   ", COLOR_TEXT,
                         COLOR_DIM);
    yt2_text_draw_opaque(create->surface, 203, 140, "   ", COLOR_TEXT,
                         COLOR_DIM);
    yt2_text_draw_opaque(create->surface, 203, 130, "   ", COLOR_TEXT,
                         COLOR_DIM);
    paint_number(create, 203, 130, character->current.combat.battle_accuracy,
                 character->maximum.combat.battle_accuracy);
    paint_number(create, 203, 140, character->current.combat.battle_damage,
                 character->maximum.combat.battle_damage);
    paint_number(create, 203, 150, character->current.combat.absorption,
                 character->maximum.combat.absorption);
}

/* Class name + level (sub_15365). */
static void paint_class_level(yt2_create *create,
                              const yt2_character *character)
{
    unsigned int class_id = (unsigned int)character->class_id;

    if (class_id >= 1u && class_id <= 9u) {
        yt2_text_draw_opaque(create->surface, 156, 38,
                             class_names[class_id - 1u], COLOR_TEXT,
                             COLOR_DIM);
    }
    paint_number(create, 256, 38, character->level, character->level);
}

/* Full right panel (sub_157E2): portrait, paperdoll, stats, name. */
static void paint_right_panel(yt2_create *create,
                              const yt2_character *character,
                              const uint16_t *highlight)
{
    const char *rating;

    paint_paperdoll(create, character);
    blit_pic(create, 7u, character->portrait_pic, 116, 19, 0);
    paint_stat_rows(create, character, highlight);
    paint_combat_rows(create, character);
    paint_class_level(create, character);
    yt2_text_draw_opaque(create->surface, 156, 26, character->name,
                         COLOR_TEXT, COLOR_DIM);
    paint_commas(create, 191, 170, character->current.combat.hit_points,
                 character->maximum.combat.hit_points);
    paint_commas(create, 191, 180, character->current.combat.magic_points,
                 character->maximum.combat.magic_points);
    rating = yt2_create_rating(character);
    if (rating != NULL) {
        yt2_text_draw_opaque(create->surface, 254, 26, rating,
                             COLOR_RATING, COLOR_DIM);
    }
    paint_emp(create, 203, 190, character->experience);
}

/* sub_15312 name: field 1 + ' ' + field 2, trailing blanks trimmed. */

static void item_name(const yt2_create *create, unsigned int id, char *buffer,
                      size_t size)
{
    const uint8_t *record = item_record(create, id);
    size_t out = 0u;
    size_t end;

    if (buffer == NULL || size == 0u) {
        return;
    }
    buffer[0] = '\0';
    if (record == NULL) {
        return;
    }
    for (unsigned int field = 0; field < 2u; field++) {
        const uint8_t *name = record + 19u + field * 13u;
        size_t length = 0u;

        if (field > 0u && out + 1u < size) {
            buffer[out++] = ' ';
        }
        while (length < 13u && name[length] != 0) {
            length++;
        }
        while (length > 0u && name[length - 1u] == ' ') {
            length--;
        }
        if (out + length >= size) {
            length = size - out - 1u;
        }
        memcpy(buffer + out, name, length);
        out += length;
    }
    buffer[out] = '\0';
    end = out;
    while (end > 0u && buffer[end - 1u] == ' ') {
        buffer[--end] = '\0';
    }
}

/* Stage painters. */

static void paint_class(yt2_create *create)
{
    paint_left_clear(create);
    paint_line(create, 8, 25, "PICK A CLASS", COLOR_TITLE);
    paint_line(create, 8, 42, "NON-MAGIC USERS:", COLOR_TITLE);
    paint_line(create, 8, 87, "CLERIC TYPES:", COLOR_TITLE);
    paint_line(create, 8, 132, "WIZARD TYPES:", COLOR_TITLE);
    for (unsigned int i = 0; i < 9u; i++) {
        paint_red(create, 8, class_rows[i], class_names[i], class_red[i]);
    }
    paint_quit(create);
}

static void paint_portrait(yt2_create *create)
{
    const yt2_character *character = &create->party->roster[create->slot];
    unsigned int base =
        character->sex == YT2_SEX_MALE ? 19u : 20u;

    paint_left_clear(create);
    paint_line(create, 8, 25, "PICK A PORTRAIT", COLOR_TITLE);
    for (unsigned int row = 0; row < 3u; row++) {
        for (unsigned int col = 0; col < 3u; col++) {
            blit_pic(create, 7u, base + (row * 3u + col) * 2u,
                     8 + (int)(col * 33u), 42 + (int)(row * 33u), 0);
        }
    }
    /* The label names the toggle target, not the current sex. */
    paint_red(create, 8, 150,
              character->sex == YT2_SEX_MALE ? "FEMALE" : "MALE  ", 1u);
    paint_quit(create);
}

static void paint_sheet_options(yt2_create *create)
{
    paint_left_clear(create);
    paint_line(create, 8, 25, "SELECT AN", COLOR_TITLE);
    paint_line(create, 8, 31, "OPTION", COLOR_TITLE);
    paint_red(create, 8, 51, "ROLL ATTRIBUTES", 1u);
    paint_red(create, 8, 69, "PICK ITEMS", 6u);
    paint_quit(create);
}

static void paint_sheet(yt2_create *create)
{
    paint_setup(create);
    paint_right_panel(create, &create->party->roster[create->slot], NULL);
    paint_sheet_options(create);
}

static void paint_items(yt2_create *create)
{
    char name[40];

    paint_left_clear(create);
    paint_line(create, 8, 25, "TAKE UP TO FOUR", COLOR_TITLE);
    paint_line(create, 8, 31, "ITEMS", COLOR_TITLE);
    for (unsigned int row = 0; row < YT2_CREATE_ITEMS; row++) {
        const uint8_t *record;
        int y = 42 + (int)(row * 16u);

        if ((create->taken & (0x80u >> row)) != 0u) {
            continue;
        }
        record = item_record(create, yt2_create_item_ids[row]);
        if (record == NULL) {
            continue;
        }
        blit_pic(create, 8u, record_u16(record, 8u), 8, y, 1);
        item_name(create, yt2_create_item_ids[row], name, sizeof(name));
        paint_line(create, 25, y + 5, name, COLOR_TEXT);
    }
    if (!create->repick) {
        paint_red(create, 8, 176, "NAME CHARACTER", 1u);
    }
    paint_quit(create);
}

static void paint_name_editor(yt2_create *create)
{
    char line[YT2_CHARACTER_NAME_SIZE + 2u];
    size_t row;

    paint_left_clear(create);
    paint_line(create, 8, 25, "ENTER THE NAME", COLOR_TITLE);
    /* Opaque echo (v_0B26 == 0): buffer plus the '-' cursor. */
    for (row = 0; row < create->name_length; row++) {
        line[row] = create->name[row];
    }
    line[row++] = '-';
    line[row] = '\0';
    yt2_text_draw_opaque(create->surface, 8, 51, line, COLOR_TEXT,
                         COLOR_DIM);
}

static void paint_preview_options(yt2_create *create)
{
    paint_left_clear(create);
    paint_line(create, 8, 25, "SELECT AN", COLOR_TITLE);
    paint_line(create, 8, 31, "OPTION", COLOR_TITLE);
    paint_red(create, 8, 51, "KEEP CHARACTER", 1u);
    paint_red(create, 8, 69, "CLASS", 1u);
    paint_red(create, 8, 78, "PORTRAIT", 1u);
    paint_red(create, 8, 87, "ROLL ATTRIBUTES", 1u);
    paint_red(create, 8, 96, "PICK ITEMS", 6u);
    paint_red(create, 8, 105, "NAME CHARACTER", 1u);
    paint_quit(create);
}

static void paint_preview(yt2_create *create)
{
    paint_setup(create);
    paint_right_panel(create, &create->party->roster[create->slot], NULL);
    paint_preview_options(create);
}

/* Stage transitions. */

static void wipe_slot(yt2_create *create)
{
    memset(&create->party->roster[create->slot], 0,
           sizeof(create->party->roster[create->slot]));
}

static void enter_portrait(yt2_create *create)
{
    create->party->roster[create->slot].sex = YT2_SEX_MALE;
    create->stage = YT2_CREATE_STAGE_PORTRAIT;
    create->cursor_shown = 1;
    paint_portrait(create);
}

static void enter_sheet(yt2_create *create)
{
    create->stage = YT2_CREATE_STAGE_SHEET;
    create->cursor_shown = 1;
    yt2_create_roll(create, &create->party->roster[create->slot]);
    paint_sheet(create);
}

static void enter_items(yt2_create *create, int reset_taken)
{
    create->stage = YT2_CREATE_STAGE_ITEMS;
    create->cursor_shown = 1;
    if (reset_taken) {
        create->taken = 0u;
        create->taken_count = 0u;
        create->last_returned = 0u;
        create->carried_id = 0u;
        create->carried_count = 0u;
        create->carried_weight = 0u;
        create->cursor = YT2_CREATE_CURSOR_ARROW;
    }
    paint_items(create);
}

static void enter_name(yt2_create *create)
{
    create->stage = YT2_CREATE_STAGE_NAME;
    create->cursor_shown = 0;
    create->name_length = 0u;
    create->name[0] = '\0';
    paint_name_editor(create);
}

static void enter_preview(yt2_create *create)
{
    create->stage = YT2_CREATE_STAGE_PREVIEW;
    create->cursor_shown = 1;
    paint_preview(create);
}

static void quit_create(yt2_create *create, int wipe)
{
    if (wipe) {
        wipe_slot(create);
    }
    create->carried_id = 0u;
    create->carried_count = 0u;
    create->carried_weight = 0u;
    create->cursor = YT2_CREATE_CURSOR_ARROW;
    create->taken = (uint16_t)(create->taken & 0xFF00u);
    create->stage = YT2_CREATE_STAGE_QUIT;
    create->dirty = 1;
}

static void keep_character(yt2_create *create)
{
    yt2_create_grant_spells(&create->party->roster[create->slot]);
    create->stage = YT2_CREATE_STAGE_KEEP;
    create->dirty = 1;
}

static void clear_carried(yt2_create *create)
{
    create->carried_id = 0u;
    create->carried_count = 0u;
    create->carried_weight = 0u;
    create->cursor = YT2_CREATE_CURSOR_ARROW;
}

/* Class stage (sub_146E9). */

static void enter_class(yt2_create *create)
{
    yt2_character *character = &create->party->roster[create->slot];

    character->status = (uint16_t)(character->status & 0xFFC0u);
    memset(character->spellbook, 0, sizeof(character->spellbook));
    create->stage = YT2_CREATE_STAGE_CLASS;
    create->cursor_shown = 1;
    paint_class(create);
}

static int select_class(yt2_create *create, unsigned int class_id)
{
    yt2_character *character = &create->party->roster[create->slot];

    if (class_id < 1u || class_id > 9u) {
        return YT2_STATUS_ERROR;
    }
    character->class_id = (yt2_class)class_id;
    character->level = 1u;
    if (create->repick) {
        yt2_create_roll(create, character);
        create->repick = 0;
        enter_preview(create);
        return YT2_STATUS_OK;
    }
    paint_class_level(create, character);
    enter_portrait(create);
    return YT2_STATUS_OK;
}

static int class_key(yt2_create *create, unsigned int ascii)
{
    if (create->repick) {
        if (ascii == 'E') {
            create->repick = 0;
            enter_preview(create);
            return YT2_STATUS_OK;
        }
        for (unsigned int i = 0; i < 9u; i++) {
            if (ascii == (unsigned int)class_keys[i]) {
                return select_class(create, i + 1u);
            }
        }
        return YT2_STATUS_ERROR;
    }
    for (unsigned int i = 0; i < 9u; i++) {
        if (ascii == (unsigned int)class_keys[i]) {
            return select_class(create, i + 1u);
        }
    }
    if (ascii == 'Q') {
        quit_create(create, 1);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int class_click(yt2_create *create, int x, int y)
{
    int id = hit_test(class_table,
                      sizeof(class_table) / sizeof(class_table[0]), x, y);

    if (id >= 1 && id <= 9) {
        return select_class(create, (unsigned int)id);
    }
    if (id == 17) {
        if (create->repick) {
            create->repick = 0;
            enter_preview(create);
        } else {
            quit_create(create, 1);
        }
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

/* Portrait stage (sub_14D71). */

static void toggle_sex(yt2_create *create)
{
    yt2_character *character = &create->party->roster[create->slot];

    character->sex = character->sex == YT2_SEX_FEMALE ? YT2_SEX_MALE
                                                     : YT2_SEX_FEMALE;
    paint_portrait(create);
}

static int select_portrait(yt2_create *create, unsigned int number)
{
    yt2_character *character = &create->party->roster[create->slot];
    unsigned int index;

    if (number < 1u || number > 9u) {
        return YT2_STATUS_ERROR;
    }
    index = (number - 1u) * 2u;
    if (character->sex != YT2_SEX_MALE) {
        index++;
    }
    character->portrait_index = (uint16_t)index;
    character->portrait_pic = (uint16_t)(YT2_PORTRAIT_FIRST_PIC + index);
    if (create->repick) {
        create->repick = 0;
        enter_preview(create);
        return YT2_STATUS_OK;
    }
    enter_sheet(create);
    return YT2_STATUS_OK;
}

static int portrait_key(yt2_create *create, unsigned int ascii)
{
    yt2_character *character = &create->party->roster[create->slot];

    if (ascii >= '1' && ascii <= '9') {
        return select_portrait(create, ascii - '0');
    }
    if (ascii == 'M') {
        if (character->sex == YT2_SEX_MALE) {
            return YT2_STATUS_ERROR;
        }
        toggle_sex(create);
        return YT2_STATUS_OK;
    }
    if (ascii == 'F') {
        if (character->sex == YT2_SEX_FEMALE) {
            return YT2_STATUS_ERROR;
        }
        toggle_sex(create);
        return YT2_STATUS_OK;
    }
    if (create->repick) {
        if (ascii == 'E') {
            create->repick = 0;
            enter_preview(create);
            return YT2_STATUS_OK;
        }
        return YT2_STATUS_ERROR;
    }
    if (ascii == 'Q') {
        quit_create(create, 1);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int portrait_click(yt2_create *create, int x, int y)
{
    int id = hit_test(portrait_table,
                      sizeof(portrait_table) / sizeof(portrait_table[0]), x,
                      y);

    if (id >= 1 && id <= 9) {
        return select_portrait(create, (unsigned int)id);
    }
    if (id == 13) {
        toggle_sex(create);
        return YT2_STATUS_OK;
    }
    if (id == 17) {
        if (create->repick) {
            create->repick = 0;
            enter_preview(create);
        } else {
            quit_create(create, 1);
        }
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

/* Sheet stage (sub_14F08). */

static int sheet_key(yt2_create *create, unsigned int ascii)
{
    if (ascii == 'R') {
        yt2_create_roll(create, &create->party->roster[create->slot]);
        paint_sheet(create);
        return YT2_STATUS_OK;
    }
    if (ascii == 'I') {
        enter_items(create, 1);
        return YT2_STATUS_OK;
    }
    if (ascii == 'Q') {
        quit_create(create, 1);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int sheet_click(yt2_create *create, int x, int y)
{
    int id = hit_test(option_table,
                      sizeof(option_table) / sizeof(option_table[0]), x, y);

    if (id == 2) {
        yt2_create_roll(create, &create->party->roster[create->slot]);
        paint_sheet(create);
        return YT2_STATUS_OK;
    }
    if (id == 4) {
        enter_items(create, 1);
        return YT2_STATUS_OK;
    }
    if (id == 17) {
        quit_create(create, 1);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

/* Items stage (sub_148C4 + fn_1672C creation subset). */

static uint16_t item_weight(const yt2_create *create, unsigned int id)
{
    const uint8_t *record = item_record(create, id);

    if (record == NULL) {
        return 0u;
    }
    return record_u16(record, 10u);
}

static uint16_t item_icon(const yt2_create *create, unsigned int id)
{
    const uint8_t *record = item_record(create, id);

    if (record == NULL) {
        return 0u;
    }
    return record_u16(record, 8u);
}

/* List click (loc_14AD5): putback, then pickup on the same click. */
static int items_list_click(yt2_create *create, int x, int y)
{
    int row;

    if (create->cursor != YT2_CREATE_CURSOR_ARROW) {
        uint16_t clear = 0xFFFEu;

        if (create->carried_count != 0u) {
            /* Container content (loc_14C36): unreachable here. */
            log_sfx(create, 3u);
            return YT2_STATUS_OK;
        }
        log_sfx(create, 6u);
        for (unsigned int i = 0; i < YT2_CREATE_ITEMS; i++) {
            if (yt2_create_item_ids[i] == create->carried_id) {
                clear = (uint16_t)(~(0x80u >> i));
                break;
            }
        }
        create->taken = (uint16_t)(create->taken & clear);
        create->last_returned = create->carried_id;
        clear_carried(create);
        create->taken_count--;
    }
    if (create->taken_count == YT2_CREATE_TAKE_MAX) {
        log_sfx(create, 3u);
        paint_items(create);
        return YT2_STATUS_OK;
    }
    row = hit_test(take_table, sizeof(take_table) / sizeof(take_table[0]), x,
                   y);
    if (row < 1 || row > 8) {
        paint_items(create);
        return YT2_STATUS_OK;
    }
    create->carried_id = yt2_create_item_ids[(unsigned int)row - 1u];
    if ((create->taken & (0x80u >> ((unsigned int)row - 1u))) == 0u &&
        create->carried_id != create->last_returned) {
        create->taken =
            (uint16_t)(create->taken | (0x80u >> ((unsigned int)row - 1u)));
        create->cursor = item_icon(create, create->carried_id);
        create->carried_count = 0u;
        create->carried_weight = item_weight(create, create->carried_id);
        create->taken_count++;
    }
    create->last_returned = 0u;
    paint_items(create);
    return YT2_STATUS_OK;
}


static void repaint_paperdoll(yt2_create *create)
{
    yt2_character *character = &create->party->roster[create->slot];

    paint_paperdoll(create, character);
    paint_combat_rows(create, character);
}

/* Paperdoll click (fn_1672C): the shared verb moves the item; the paperdoll
   and combat rows repaint (B1 fn_1658F + sub_14FC3). */
static int items_paperdoll_click(yt2_create *create, int x, int y)
{
    yt2_character *character = &create->party->roster[create->slot];
    unsigned int id = yt2_paperdoll_hit(PAPERDOLL_ORIGIN_X, PAPERDOLL_ORIGIN_Y, x, y);
    int outcome;

    if (id == 0u || id == 9u) {
        return YT2_STATUS_ERROR;
    }
    create->paperdoll.cursor = (uint16_t)create->cursor;
    create->paperdoll.carried = create->carried_id;
    create->paperdoll.carried_count = create->carried_count;
    create->paperdoll.carried_weight = create->carried_weight;
    outcome = yt2_paperdoll_left(&create->paperdoll, character, create->slot + 1u,
                            id);
    create->cursor = create->paperdoll.cursor;
    create->carried_id = create->paperdoll.carried;
    create->carried_count = create->paperdoll.carried_count;
    create->carried_weight = create->paperdoll.carried_weight;
    if (outcome != YT2_PAPERDOLL_CHANGED) {
        return YT2_STATUS_OK;
    }
    repaint_paperdoll(create);
    paint_items(create);
    return YT2_STATUS_OK;
}

static int items_key(yt2_create *create, unsigned int ascii)
{
    if (create->repick) {
        if (ascii == 'E') {
            create->repick = 0;
            enter_preview(create);
            return YT2_STATUS_OK;
        }
        return YT2_STATUS_ERROR;
    }
    if (ascii == 'N') {
        clear_carried(create);
        enter_name(create);
        return YT2_STATUS_OK;
    }
    if (ascii == 'Q') {
        quit_create(create, 0);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int items_click(yt2_create *create, int x, int y)
{
    int id = hit_test(items_table,
                      sizeof(items_table) / sizeof(items_table[0]), x, y);

    if (id == 1) {
        return items_list_click(create, x, y);
    }
    if (id == 2) {
        return items_paperdoll_click(create, x, y);
    }
    if (id == 16 && !create->repick) {
        clear_carried(create);
        enter_name(create);
        return YT2_STATUS_OK;
    }
    if (id == 17) {
        if (create->repick) {
            /* Cursor image resets; a carried item stays carried. */
            create->cursor = YT2_CREATE_CURSOR_ARROW;
            create->repick = 0;
            enter_preview(create);
        } else {
            quit_create(create, 0);
        }
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

/* Name stage (sub_14CA1 + fn_0D29A as a keystroke machine). */

static void name_commit(yt2_create *create)
{
    yt2_character *character = &create->party->roster[create->slot];
    size_t length = create->name_length;

    while (length > 0u && create->name[length - 1u] == ' ') {
        length--;
    }
    create->name_length = (unsigned int)length;
    if (length == 0u) {
        create->name[0] = '\0';
        paint_name_editor(create);
        return;
    }
    memcpy(character->name, create->name, length);
    character->name[length] = '\0';
    if (create->repick) {
        create->repick = 0;
    }
    enter_preview(create);
}

static int name_key(yt2_create *create, unsigned int ascii)
{
    if (ascii == 0x0Du) {
        name_commit(create);
        return YT2_STATUS_OK;
    }
    if (ascii == 0x1Bu) {
        if (create->repick) {
            create->repick = 0;
            enter_preview(create);
            return YT2_STATUS_OK;
        }
        create->name_length = 0u;
        create->name[0] = '\0';
        paint_name_editor(create);
        return YT2_STATUS_OK;
    }
    if (ascii == 0x08u) {
        if (create->name_length == 0u) {
            log_sfx(create, 3u);
            return YT2_STATUS_OK;
        }
        create->name_length--;
        create->name[create->name_length] = '\0';
        paint_name_editor(create);
        return YT2_STATUS_OK;
    }
    if (ascii < 0x20u || ascii > 0x7Eu) {
        return YT2_STATUS_ERROR;
    }
    if (create->name_length == YT2_CREATE_NAME_MAX) {
        log_sfx(create, 3u);
        return YT2_STATUS_OK;
    }
    create->name[create->name_length++] = (char)ascii;
    create->name[create->name_length] = '\0';
    paint_name_editor(create);
    return YT2_STATUS_OK;
}

/* Preview stage (sub_15419). */

static int preview_repick(yt2_create *create, yt2_create_stage stage)
{
    create->repick = 1;
    switch (stage) {
    case YT2_CREATE_STAGE_CLASS:
        enter_class(create);
        return YT2_STATUS_OK;
    case YT2_CREATE_STAGE_PORTRAIT:
        create->stage = YT2_CREATE_STAGE_PORTRAIT;
        paint_portrait(create);
        return YT2_STATUS_OK;
    case YT2_CREATE_STAGE_ITEMS:
        enter_items(create, 0);
        return YT2_STATUS_OK;
    case YT2_CREATE_STAGE_NAME:
        enter_name(create);
        return YT2_STATUS_OK;
    default:
        create->repick = 0;
        return YT2_STATUS_ERROR;
    }
}

static int preview_key(yt2_create *create, unsigned int ascii)
{
    if (ascii == 'K') {
        keep_character(create);
        return YT2_STATUS_OK;
    }
    if (ascii == 'C') {
        return preview_repick(create, YT2_CREATE_STAGE_CLASS);
    }
    if (ascii == 'P') {
        return preview_repick(create, YT2_CREATE_STAGE_PORTRAIT);
    }
    if (ascii == 'R') {
        yt2_create_roll(create, &create->party->roster[create->slot]);
        paint_preview(create);
        return YT2_STATUS_OK;
    }
    if (ascii == 'I') {
        return preview_repick(create, YT2_CREATE_STAGE_ITEMS);
    }
    if (ascii == 'N') {
        return preview_repick(create, YT2_CREATE_STAGE_NAME);
    }
    if (ascii == 'Q') {
        quit_create(create, 1);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int preview_click(yt2_create *create, int x, int y)
{
    int id = hit_test(option_table,
                      sizeof(option_table) / sizeof(option_table[0]), x, y);

    switch (id) {
    case 2:
        keep_character(create);
        return YT2_STATUS_OK;
    case 4:
        return preview_repick(create, YT2_CREATE_STAGE_CLASS);
    case 5:
        return preview_repick(create, YT2_CREATE_STAGE_PORTRAIT);
    case 6:
        yt2_create_roll(create, &create->party->roster[create->slot]);
        paint_preview(create);
        return YT2_STATUS_OK;
    case 7:
        return preview_repick(create, YT2_CREATE_STAGE_ITEMS);
    case 8:
        return preview_repick(create, YT2_CREATE_STAGE_NAME);
    case 17:
        quit_create(create, 1);
        return YT2_STATUS_OK;
    default:
        return YT2_STATUS_ERROR;
    }
}

/* Paperdoll bonus hook: the fixed sub_01DFC take list carries no bonus
   lists (all eight adds offsets are zero), so B1's fn_0ACF5/0B30D
   is a no-op throughout creation (asserted by the unit gate). */
static void create_paperdoll_bonus(void *bonus_ctx, unsigned int slot,
                              unsigned int id, int plus)
{
    (void)bonus_ctx;
    (void)slot;
    (void)id;
    (void)plus;
}

int yt2_create_begin(yt2_create *create, yt2_save *save,
                     const yt2_items *items, const yt2_pictures *pictures,
                     yt2_surface *surface, uint16_t time_word)
{
    int slot;
    uint8_t *store_base = NULL;
    size_t store_size = 0u;

    if (create == NULL || save == NULL || items == NULL ||
        pictures == NULL || surface == NULL) {
        return YT2_STATUS_ERROR;
    }
    slot = yt2_create_alloc_slot(&save->party);
    if (slot < 0) {
        return YT2_STATUS_ERROR;
    }
    memset(create, 0, sizeof(*create));
    /* sub_146D9: the fresh slot's whole record zeroes at alloc. */
    memset(&save->party.roster[slot], 0,
           sizeof(save->party.roster[slot]));
    create->party = &save->party;
    create->items = items;
    create->pictures = pictures;
    create->surface = surface;
    create->slot = (unsigned int)slot;
    create->time_word = time_word;
    create->cursor = YT2_CREATE_CURSOR_ARROW;
    if (save->bytes != NULL && save->size > YT2_SAVE_STORE_OFFSET) {
        store_base = save->bytes + YT2_SAVE_STORE_OFFSET;
        store_size = save->size - YT2_SAVE_STORE_OFFSET;
    }
    yt2_paperdoll_init(&create->paperdoll, create->items, store_base, store_size,
                  &save->store_bump, &save->store_free, create->sfx,
                  &create->sfx_count, YT2_CREATE_SFX_LOG,
                  create_paperdoll_bonus, create);
    /* The work buffer borders are black from boot; the background repaint
     * covers everything else, so start cleared. */
    yt2_surface_clear(surface, 0u);
    paint_setup(create);
    create->stage = YT2_CREATE_STAGE_CLASS;
    return YT2_STATUS_OK;
}

int yt2_create_open_class(yt2_create *create)
{
    if (create == NULL || yt2_create_done(create) ||
        create->stage != YT2_CREATE_STAGE_CLASS) {
        return YT2_STATUS_ERROR;
    }
    enter_class(create);
    return YT2_STATUS_OK;
}

int yt2_create_key(yt2_create *create, unsigned int ascii)
{
    if (create == NULL || yt2_create_done(create)) {
        return YT2_STATUS_ERROR;
    }
    switch (create->stage) {
    case YT2_CREATE_STAGE_CLASS:
        return class_key(create, ascii);
    case YT2_CREATE_STAGE_PORTRAIT:
        return portrait_key(create, ascii);
    case YT2_CREATE_STAGE_SHEET:
        return sheet_key(create, ascii);
    case YT2_CREATE_STAGE_ITEMS:
        return items_key(create, ascii);
    case YT2_CREATE_STAGE_NAME:
        return name_key(create, ascii);
    case YT2_CREATE_STAGE_PREVIEW:
        return preview_key(create, ascii);
    default:
        return YT2_STATUS_ERROR;
    }
}

int yt2_create_click(yt2_create *create, int x, int y)
{
    if (create == NULL || yt2_create_done(create)) {
        return YT2_STATUS_ERROR;
    }
    switch (create->stage) {
    case YT2_CREATE_STAGE_CLASS:
        return class_click(create, x, y);
    case YT2_CREATE_STAGE_PORTRAIT:
        return portrait_click(create, x, y);
    case YT2_CREATE_STAGE_SHEET:
        return sheet_click(create, x, y);
    case YT2_CREATE_STAGE_ITEMS:
        return items_click(create, x, y);
    case YT2_CREATE_STAGE_NAME:
        /* fn_0D29A polls keys only; the mouse never repaints. */
        return YT2_STATUS_ERROR;
    case YT2_CREATE_STAGE_PREVIEW:
        return preview_click(create, x, y);
    default:
        return YT2_STATUS_ERROR;
    }
}

int yt2_create_right_click(yt2_create *create, int x, int y)
{
    yt2_character *character;
    unsigned int id;
    unsigned int examined = 0u;
    int hit;
    int outcome;

    if (create == NULL || yt2_create_done(create)) {
        return YT2_STATUS_ERROR;
    }
    /* fn_16925 runs only on the items stage (sub_148C4's loop);
       every other stage ignores the right button. */
    if (create->stage != YT2_CREATE_STAGE_ITEMS) {
        return YT2_STATUS_OK;
    }
    /* The 0x606E dispatch only reaches the paperdoll on id 2. */
    hit = hit_test(items_table, sizeof(items_table) / sizeof(items_table[0]),
                   x, y);
    if (hit != 2) {
        return YT2_STATUS_OK;
    }
    character = &create->party->roster[create->slot];
    id = yt2_paperdoll_hit(PAPERDOLL_ORIGIN_X, PAPERDOLL_ORIGIN_Y, x, y);
    create->paperdoll.cursor = (uint16_t)create->cursor;
    create->paperdoll.carried = create->carried_id;
    create->paperdoll.carried_count = create->carried_count;
    create->paperdoll.carried_weight = create->carried_weight;
    outcome = yt2_paperdoll_right(&create->paperdoll, character, create->slot + 1u,
                             id, &examined);
    (void)examined;
    create->cursor = create->paperdoll.cursor;
    create->carried_id = create->paperdoll.carried;
    create->carried_count = create->paperdoll.carried_count;
    create->carried_weight = create->paperdoll.carried_weight;
    if (outcome == YT2_PAPERDOLL_CHANGED) {
        /* A container opened or closed (B1 fn_1658F + present). */
        repaint_paperdoll(create);
        paint_items(create);
    }
    /* B1 presents after 50CC 0/2 alike and loops silent on 1;
       the caller presents on OK either way (unchanged pixels). */
    return YT2_STATUS_OK;
}

int yt2_create_done(const yt2_create *create)
{
    return create != NULL &&
           (create->stage == YT2_CREATE_STAGE_KEEP ||
            create->stage == YT2_CREATE_STAGE_QUIT);
}

unsigned int yt2_create_cursor(const yt2_create *create)
{
    return create != NULL ? create->cursor : YT2_CREATE_CURSOR_ARROW;
}

int yt2_create_cursor_shown(const yt2_create *create)
{
    return create != NULL && create->cursor_shown;
}

unsigned int yt2_create_sfx_count(const yt2_create *create)
{
    return create != NULL ? create->sfx_count : 0u;
}

unsigned int yt2_create_sfx(const yt2_create *create, unsigned int index)
{
    if (create == NULL || index >= create->sfx_count) {
        return 0u;
    }
    return create->sfx[index];
}

/* Roster (fn_1C354), sheet viewer (fn_13F2E) and delete. */

/* DS:5B58h geometry, generated: the third row sits one pixel lower. */
static const int roster_portrait_y[3] = { 5, 65, 126 };
static const int roster_name_y[3] = { 43, 103, 164 };
static const int roster_class_y[3] = { 55, 115, 176 };
static const int roster_check_y[3] = { 29, 89, 150 };

static int roster_hit(int x, int y)
{
    for (unsigned int slot = 0; slot < YT2_ROSTER_SLOTS; slot++) {
        unsigned int col = slot % 3u;
        unsigned int row = slot / 3u;
        int px = 15 + (int)(col * 102u);

        if (x >= px && x <= px + 31 && y >= roster_portrait_y[row] &&
            y <= roster_portrait_y[row] + 31) {
            return (int)(slot + 1u);
        }
        if (x >= px && x <= px + 84 && y >= roster_name_y[row] &&
            y <= roster_name_y[row] + 5) {
            return (int)(slot + 1u);
        }
        if (x >= px && x <= px + 60 && y >= roster_class_y[row] &&
            y <= roster_class_y[row] + 5) {
            return (int)(slot + 1u);
        }
        /* The toggle zone spans the portrait's full height and touches
           its right edge: the original 8x8 box is unhittable on modern
           displays (near-misses by 7-17px end the roster empty), and the
           name/class rows below do not overlap this band. */
        if (x >= px + 32 && x <= px + 60 && y >= roster_portrait_y[row] &&
            y <= roster_portrait_y[row] + 31) {
            return (int)(slot + 10u);
        }
    }
    if (x >= 229 && x <= 255 && y >= 188 && y <= 196) {
        return 19;
    }
    return 0;
}

/*
 * The sheet painters only borrow party/items/pictures/surface/slot, so
 * the viewer drives them through a stack session instead of duplicating
 * them (the original shares sub_15046/sub_14FC3/sub_15365 the same way).
 */
static void roster_paint_borrow(yt2_roster *roster, yt2_create *paint)
{
    memset(paint, 0, sizeof(*paint));
    paint->party = roster->party;
    paint->items = roster->items;
    paint->pictures = roster->pictures;
    paint->surface = roster->surface;
    paint->slot = roster->slot;
}

static void roster_log_sfx(yt2_roster *roster, unsigned int id)
{
    if (roster->sfx_count < YT2_CREATE_SFX_LOG) {
        roster->sfx[roster->sfx_count++] = id;
    }
}

static void paint_check(yt2_roster *roster, unsigned int slot, int assigned)
{
    yt2_create paint;
    unsigned int col = slot % 3u;
    unsigned int row = slot / 3u;

    roster_paint_borrow(roster, &paint);
    blit_pic(&paint, 9u, assigned ? 18u : 17u,
             15 + (int)(col * 102u) + 37, roster_check_y[row], 1);
}

static void paint_roster_grid(yt2_roster *roster)
{
    yt2_create paint;

    roster_paint_borrow(roster, &paint);
    blit_pic(&paint, 0u, 4u, 1, 1, 0);
    for (unsigned int slot = 0; slot < YT2_ROSTER_SLOTS; slot++) {
        const yt2_character *character = &roster->party->roster[slot];
        unsigned int col = slot % 3u;
        unsigned int row = slot / 3u;
        unsigned int class_id;
        int px;

        if (character->level == 0u) {
            continue;
        }
        px = 15 + (int)(col * 102u);
        blit_pic(&paint, 7u, character->portrait_pic, px,
                 roster_portrait_y[row], 0);
        paint_line(&paint, px, roster_name_y[row], character->name,
                   COLOR_TEXT);
        class_id = (unsigned int)character->class_id;
        if (class_id >= 1u && class_id <= 9u) {
            paint_line(&paint, px, roster_class_y[row],
                       class_names[class_id - 1u], COLOR_TEXT);
        }
        paint_check(roster, slot, (character->flags & 0x800u) != 0u);
    }
}

static void paint_viewer(yt2_roster *roster)
{
    yt2_create paint;
    const yt2_character *character;

    roster_paint_borrow(roster, &paint);
    character = &roster->party->roster[roster->slot];
    blit_pic(&paint, CREATE_BG_GROUP, CREATE_BG_PIC, CREATE_BG_X, CREATE_BG_Y,
             0);
    paint_line(&paint, 107, 6, "CHARACTER CREATION", COLOR_TEXT);
    paint_right_panel(&paint, character, roster->party->active);
    paint_left_clear(&paint);
    paint_line(&paint, 8, 25, "SELECT AN", COLOR_TITLE);
    paint_line(&paint, 8, 31, "OPTION", COLOR_TITLE);
    paint_red(&paint, 8, 42, "DELETE", 1u);
    paint_red(&paint, 8, 185, "RETURN", 1u);
}

static void paint_confirm(yt2_roster *roster)
{
    yt2_create paint;

    roster_paint_borrow(roster, &paint);
    paint_left_clear(&paint);
    paint_line(&paint, 8, 25, "ARE YOU SURE", COLOR_TITLE);
    paint_red(&paint, 8, 42, "YES, DELETE", 1u);
    paint_red(&paint, 8, 60, "NO, KEEP", 1u);
}

static void roster_compact(yt2_roster *roster)
{
    uint16_t *party = roster->party->assigned;

    if (party[0] == 0u) {
        for (unsigned int i = 1; i < 4u; i++) {
            if (party[i] != 0u) {
                party[0] = party[i];
                party[i] = 0u;
                break;
            }
        }
    }
    if (party[1] == 0u) {
        for (unsigned int i = 2; i < 4u; i++) {
            if (party[i] != 0u) {
                party[1] = party[i];
                party[i] = 0u;
                break;
            }
        }
    }
    if (party[2] == 0u && party[3] != 0u) {
        party[2] = party[3];
        party[3] = 0u;
    }
}

static void roster_done(yt2_roster *roster)
{
    unsigned int total = 0u;

    for (unsigned int i = 0; i < YT2_ROSTER_PARTY_MAX; i++) {
        total += roster->party->assigned[i];
    }
    roster->party_ok = 0;
    if (total == 0u) {
        roster->view = YT2_ROSTER_VIEW_DONE;
        return;
    }
    roster->party_ok = 1;
    roster_compact(roster);
    roster->view = YT2_ROSTER_VIEW_DONE;
}

static void roster_view_sheet(yt2_roster *roster, unsigned int slot)
{
    roster->slot = slot;
    roster->view = YT2_ROSTER_VIEW_SHEET;
    paint_viewer(roster);
}

static int roster_toggle(yt2_roster *roster, unsigned int slot)
{
    yt2_character *character = &roster->party->roster[slot];
    uint16_t number = (uint16_t)(slot + 1u);

    if (character->level == 0u) {
        /* Empty slot: buzz instead of failing silent (the click lands
           on a real target, there is just nobody there). */
        roster_log_sfx(roster, 3u);
        return YT2_STATUS_OK;
    }
    if ((character->flags & 0x800u) != 0u) {
        unsigned int found = 0u;

        character->flags = (uint16_t)(character->flags & ~0x800u);
        for (unsigned int i = 0; i < YT2_ROSTER_PARTY_MAX; i++) {
            if (roster->party->assigned[i] == number) {
                roster->party->assigned[i] = 0u;
                found = 1u;
                break;
            }
        }
        if (!found) {
            /* Stale flag (loc_1C477): view the sheet instead. */
            roster_view_sheet(roster, slot);
            return YT2_STATUS_OK;
        }
        for (unsigned int i = 0; i < YT2_PARTY_ACTIVE_MAX; i++) {
            if (roster->party->active[i] == number) {
                roster->party->active[i] = 0u;
            }
        }
        roster_log_sfx(roster, 4u);
        paint_check(roster, slot, 0);
        return YT2_STATUS_OK;
    }
    for (unsigned int i = 0; i < YT2_ROSTER_PARTY_MAX; i++) {
        if (roster->party->assigned[i] == 0u) {
            roster->party->assigned[i] = number;
            character->flags =
                (uint16_t)(character->flags | 0x800u);
            roster_log_sfx(roster, 4u);
            paint_check(roster, slot, 1);
            return YT2_STATUS_OK;
        }
    }
    roster_log_sfx(roster, 3u);
    return YT2_STATUS_OK;
}

/* fn_15A56: true when the item (or nested containers) is flagged 1. */
static int item_locked(const yt2_create *paint, unsigned int id)
{
    const uint8_t *record = item_record(paint, id);

    if (id == 0u || record == NULL) {
        return 0;
    }
    if ((record_u16(record, 12u) & 1u) != 0u) {
        return 1;
    }
    /*
     * Container files never exist for creation flows (nesting buzzes),
     * so a set 0x2000 flag reads an empty file: unlocked.
     */
    return 0;
}

/* loc_140D9 pre-delete sweep: pack + equipment, then effects stride 4. */
static int delete_blocked(yt2_roster *roster)
{
    yt2_create paint;
    const yt2_character *character;

    roster_paint_borrow(roster, &paint);
    character = &roster->party->roster[roster->slot];
    for (unsigned int i = 0; i < YT2_PACK_SLOTS; i++) {
        if (item_locked(&paint, character->pack[i].id)) {
            return 1;
        }
    }
    for (unsigned int i = 0; i < YT2_EQUIPMENT_SLOTS; i++) {
        if (item_locked(&paint, character->equipment[i].id)) {
            return 1;
        }
    }
    for (unsigned int i = 0; i < 5u; i++) {
        unsigned int effect = 0u;

        /* Verbatim stride-4 reads: effects 0/2/4 plus two zero words. */
        if (i < 3u) {
            effect = character->effects[i * 2u];
        }
        if (item_locked(&paint, effect)) {
            return 1;
        }
    }
    return 0;
}

static int viewer_key(yt2_roster *roster, unsigned int ascii)
{
    if (ascii == 'D') {
        if (delete_blocked(roster)) {
            roster_log_sfx(roster, 3u);
            paint_viewer(roster);
            return YT2_STATUS_OK;
        }
        roster->view = YT2_ROSTER_VIEW_CONFIRM;
        paint_confirm(roster);
        return YT2_STATUS_OK;
    }
    if (ascii == 'R') {
        roster->view = YT2_ROSTER_VIEW_GRID;
        paint_roster_grid(roster);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int viewer_click(yt2_roster *roster, int x, int y)
{
    int id = hit_test(option_table,
                      sizeof(option_table) / sizeof(option_table[0]), x, y);

    if (id == 1) {
        return viewer_key(roster, 'D');
    }
    if (id == 17) {
        return viewer_key(roster, 'R');
    }
    return YT2_STATUS_ERROR;
}

static void delete_confirmed(yt2_roster *roster)
{
    yt2_character *character = &roster->party->roster[roster->slot];
    uint16_t number = (uint16_t)(roster->slot + 1u);

    for (unsigned int i = 0; i < YT2_ROSTER_PARTY_MAX; i++) {
        if (roster->party->assigned[i] == number) {
            roster->party->assigned[i] = 0u;
        }
    }
    for (unsigned int i = 0; i < YT2_PARTY_ACTIVE_MAX; i++) {
        if (roster->party->active[i] == number) {
            roster->party->active[i] = 0u;
        }
    }
    memset(character, 0, sizeof(*character));
    roster->dirty = 1;
    roster->view = YT2_ROSTER_VIEW_GRID;
    paint_roster_grid(roster);
    if (roster->flush != NULL) {
        roster->flush(roster->flush_context);
    }
}

static int confirm_key(yt2_roster *roster, unsigned int ascii)
{
    if (ascii == 'Y') {
        delete_confirmed(roster);
        return YT2_STATUS_OK;
    }
    if (ascii == 'N') {
        roster->view = YT2_ROSTER_VIEW_SHEET;
        paint_viewer(roster);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int confirm_click(yt2_roster *roster, int x, int y)
{
    int id = hit_test(option_table,
                      sizeof(option_table) / sizeof(option_table[0]), x, y);

    if (id == 1) {
        delete_confirmed(roster);
        return YT2_STATUS_OK;
    }
    if (id == 3) {
        roster->view = YT2_ROSTER_VIEW_SHEET;
        paint_viewer(roster);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

static int grid_key(yt2_roster *roster, unsigned int ascii)
{
    if (ascii >= '1' && ascii <= '9') {
        return roster_toggle(roster, ascii - '1');
    }
    if (ascii == 'D') {
        roster_done(roster);
        return YT2_STATUS_OK;
    }
    /*
     * F1..F9 share bytes with ASCII ";<=>?@ABC" but arrive as v_50CC == 2;
     * on the ASCII path they are inert (yt2_roster_fkey covers them).
     */
    return YT2_STATUS_ERROR;
}

int yt2_roster_fkey(yt2_roster *roster, unsigned int number)
{
    if (roster == NULL || yt2_roster_done(roster) ||
        roster->view != YT2_ROSTER_VIEW_GRID || number < 1u ||
        number > 9u) {
        return YT2_STATUS_ERROR;
    }
    if (roster->party->roster[number - 1u].level == 0u) {
        return YT2_STATUS_ERROR;
    }
    roster_view_sheet(roster, number - 1u);
    return YT2_STATUS_OK;
}

static int grid_click(yt2_roster *roster, int x, int y)
{
    int id = roster_hit(x, y);

    if (id >= 1 && id <= 9) {
        unsigned int slot = (unsigned int)id - 1u;

        if (roster->party->roster[slot].level == 0u) {
            roster_log_sfx(roster, 3u);
            return YT2_STATUS_OK;
        }
        roster_view_sheet(roster, slot);
        return YT2_STATUS_OK;
    }
    if (id >= 10 && id <= 18) {
        return roster_toggle(roster, (unsigned int)id - 10u);
    }
    if (id == 19) {
        roster_done(roster);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

int yt2_roster_begin(yt2_roster *roster, yt2_party *party,
                     const yt2_items *items, const yt2_pictures *pictures,
                     yt2_surface *surface)
{
    if (roster == NULL || party == NULL || items == NULL ||
        pictures == NULL || surface == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(roster, 0, sizeof(*roster));
    roster->party = party;
    roster->items = items;
    roster->pictures = pictures;
    roster->surface = surface;
    roster->view = YT2_ROSTER_VIEW_GRID;
    /* No seeding: the party owns assigned[]/active[] (fn_1C354 draws
       from the flags and toggles both sides incrementally; v_963B
       persists across roster opens in the original's DGROUP and in
       the save here). */
    yt2_surface_clear(surface, 0u);
    paint_roster_grid(roster);
    return YT2_STATUS_OK;
}

int yt2_roster_key(yt2_roster *roster, unsigned int ascii)
{
    if (roster == NULL || yt2_roster_done(roster)) {
        return YT2_STATUS_ERROR;
    }
    switch (roster->view) {
    case YT2_ROSTER_VIEW_GRID:
        return grid_key(roster, ascii);
    case YT2_ROSTER_VIEW_SHEET:
        return viewer_key(roster, ascii);
    case YT2_ROSTER_VIEW_CONFIRM:
        return confirm_key(roster, ascii);
    default:
        return YT2_STATUS_ERROR;
    }
}

int yt2_roster_click(yt2_roster *roster, int x, int y)
{
    if (roster == NULL || yt2_roster_done(roster)) {
        return YT2_STATUS_ERROR;
    }
    switch (roster->view) {
    case YT2_ROSTER_VIEW_GRID:
        return grid_click(roster, x, y);
    case YT2_ROSTER_VIEW_SHEET:
        return viewer_click(roster, x, y);
    case YT2_ROSTER_VIEW_CONFIRM:
        return confirm_click(roster, x, y);
    default:
        return YT2_STATUS_ERROR;
    }
}

int yt2_roster_done(const yt2_roster *roster)
{
    return roster != NULL && roster->view == YT2_ROSTER_VIEW_DONE;
}

unsigned int yt2_roster_sfx_count(const yt2_roster *roster)
{
    return roster != NULL ? roster->sfx_count : 0u;
}

unsigned int yt2_roster_sfx(const yt2_roster *roster, unsigned int index)
{
    if (roster == NULL || index >= roster->sfx_count) {
        return 0u;
    }
    return roster->sfx[index];
}
