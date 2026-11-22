#include "yt2/item_detail.h"

#include <stdio.h>
#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"
#include "yt2/text.h"

/* ---- static data (all from the verified disassembly) ---- */

static const unsigned int item_detail_icons[YT2_LIST_ICON_COUNT] = {
    32u, 325u, 327u, 339u, 330u, 331u, 333u
};

static const char *const category_names[YT2_ITEMS_CATEGORIES] = {
    "ARMOR/RINGS",
    "ATTRIBUTE ENHANCERS",
    "JEWELS/ARTIFACTS/UNIQUE ITEMS",
    "MAGIC SCROLLS/QUARTZ",
    "POTIONS",
    "SUPPLIES/FOOD",
    "TRANSPORTATIONS",
    "WEAPONS"
};

static const int category_header_x[YT2_ITEMS_CATEGORIES] = {
    249, 201, 141, 195, 273, 237, 225, 273
};

static const char *const protection_names[9] = {
    "DISEASE", "POISON", "SICKNESS", "STONING", "FROZEN", "PARALYZE",
    "CURSING", "HEXING", "JINXING"
};

static const char *const adds_names[27] = {
    "STRENGTH", "DEXTERITY", "STAMINA", "INTELLIGENCE", "WISDOM",
    "CHARISMA", "", "", "", "", "", "HIT POINTS", "MAGIC POINTS", "",
    "SURVIVAL", "PROJECTILE", "SLASHING", "BASHING", "POLEARM",
    "CASTING", "MAPPING", "NAVIGATION", "BARTERING", "REPAIR",
    "THIEVERY", "LINGUISTICS", "CHEMISTRY"
};

static const char *const skill_names[5] = {
    "PROJECTILE", "SLASHING", "BASHING", "POLEARM", "CASTING"
};

#define DETAIL_BG_GROUP 0u
#define DETAIL_BG_PICTURE 13u
#define DETAIL_BG_X 1
#define DETAIL_BG_Y 1

#define DETAIL_TITLE_Y 4
#define DETAIL_TITLE_X 6
#define DETAIL_TITLE_COLOR 0x0Du

#define DETAIL_LABEL_COLOR 0x0Au
#define DETAIL_GOLD_COLOR 0x8Au
#define DETAIL_WORD_COLOR 0x59u
#define DETAIL_FLAG_COLOR 0xA7u
#define DETAIL_SPECIAL_COLOR 0xCAu
#define DETAIL_TEXT_COLOR 0x0Du

#define DETAIL_ICON_GROUP 8u
#define DETAIL_ICON_X 68
#define DETAIL_ICON_Y 41
#define DETAIL_ICON_TRANSPARENT 0xFFu

#define DETAIL_BASE_LABEL_X 91
#define DETAIL_BASE_Y 39
#define DETAIL_WEIGHT_LABEL_X 115
#define DETAIL_WEIGHT_Y 45
#define DETAIL_VALUE_X 157

#define DETAIL_FITS_LABEL_X 110
#define DETAIL_FITS_Y 69
#define DETAIL_FITS_X 158

#define DETAIL_STAT_Y 57
#define DETAIL_PROT_Y 81
#define DETAIL_PROT_LABEL_X 85
#define DETAIL_ADDS_Y 111
#define DETAIL_ADDS_LABEL_X 127
#define DETAIL_LIST_VALUE_X 157
#define DETAIL_LIST_NAME_DX 24
#define DETAIL_LIST_STEP 6

#define DETAIL_PLUS_PIC_BASE 341u
#define DETAIL_PLUS_X0 21
#define DETAIL_PLUS_STEP 26
#define DETAIL_PLUS_Y 141

/* ---- helpers ---- */

static uint16_t read_u16(const uint8_t *at)
{
    return (uint16_t)((uint16_t)at[0] | ((uint16_t)at[1] << 8));
}

/* 4-byte big-endian-digit BCD (bytes in order, high nibble first). */
static unsigned int read_bcd32(const uint8_t *at)
{
    unsigned int value = 0u;

    for (unsigned int i = 0u; i < 4u; i++) {
        value = value * 100u + (unsigned int)(at[i] >> 4) * 10u +
                (unsigned int)(at[i] & 15u);
    }
    return value;
}

static const uint8_t *param_row(const yt2_items *items,
                                const uint8_t *record);

static const uint8_t *record_at(const yt2_items *items, unsigned int file)
{
    size_t offset;

    if (items == NULL || items->bytes == NULL ||
        file >= YT2_ITEMS_COUNT) {
        return NULL;
    }
    offset =
        (size_t)YT2_ITEM_STRUCT_OFFSET + (size_t)file * YT2_ITEMS_RECORD_SIZE;
    if (offset + YT2_ITEMS_RECORD_SIZE > items->size) {
        return NULL;
    }
    return items->bytes + offset;
}

static const uint8_t *table_at(const yt2_items *items, unsigned int base,
                               unsigned int index, size_t length)
{
    size_t offset;

    if (items == NULL || items->bytes == NULL) {
        return NULL;
    }
    offset = (size_t)base + (size_t)index;
    if (offset + length > items->size) {
        return NULL;
    }
    return items->bytes + offset;
}

/* Plain %u with thousands commas (equals fn_09C46 on the u32-BCD domain:
 * leading groups suppressed with their commas, at least one digit). */
static void format_commas(char *buffer, size_t size, unsigned int value)
{
    char digits[12];
    size_t count = 0u;
    size_t out = 0u;

    snprintf(digits, sizeof(digits), "%u", value);
    count = strlen(digits);
    for (size_t i = 0u; i < count && out + 1u < size; i++) {
        if (i > 0u && (count - i) % 3u == 0u) {
            buffer[out++] = ',';
        }
        buffer[out++] = digits[i];
    }
    buffer[out] = 0;
}

const uint8_t *yt2_item_record(const yt2_items *items, unsigned int file)
{
    return record_at(items, file);
}

const uint8_t *yt2_item_def(const yt2_items *items, unsigned int id)
{
    size_t offset;

    if (items == NULL || items->bytes == NULL || id == 0u) {
        return NULL;
    }
    if (id <= YT2_ITEM_STRUCT_COUNT_A) {
        offset = (size_t)YT2_ITEM_STRUCT_OFFSET +
                 (size_t)(id - 1u) * YT2_ITEMS_RECORD_SIZE;
    } else if (id <= YT2_ITEM_STRUCT_COUNT_A + YT2_ITEM_STRUCT_COUNT_B) {
        offset = (size_t)YT2_ITEM_STRUCT_OFFSET_B +
                 (size_t)(id - 1u - YT2_ITEM_STRUCT_COUNT_A) *
                     YT2_ITEMS_RECORD_SIZE;
    } else {
        return NULL;
    }
    if (offset + YT2_ITEMS_RECORD_SIZE > items->size) {
        return NULL;
    }
    return items->bytes + offset;
}

const uint8_t *yt2_item_param_row(const yt2_items *items, const uint8_t *record)
{
    if (record == NULL) {
        return NULL;
    }
    return param_row(items, record);
}

const uint8_t *yt2_item_adds(const yt2_items *items, const uint8_t *record)
{
    /* fn_02262's v_0CE4 window: 8 words (4 offset/value pairs)
       at the record+2 byte offset, NULL when absent. */
    if (items == NULL || record == NULL || read_u16(record + 2) == 0u) {
        return NULL;
    }
    return table_at(items, YT2_ITEM_ADDS_OFFSET, read_u16(record + 2), 16u);
}

void yt2_item_format_commas(char *buffer, size_t size, unsigned int value)
{
    if (buffer == NULL || size == 0u) {
        return;
    }
    format_commas(buffer, size, value);
}

/* Plain %u with a decimal point before the last `places` digits and no
 * zero padding (fn_08070 with leading zeros blanked by sub_080F9, then
 * fn_15A22 stripping spaces, then fn_062EE inserting the point). */
static void format_decimal(char *buffer, size_t size, unsigned int value,
                           unsigned int places)
{
    char digits[12];
    size_t count;

    snprintf(digits, sizeof(digits), "%u", value);
    count = strlen(digits);
    if (count > places) {
        size_t point = count - places;

        if (point + 1u + places + 1u <= size) {
            memcpy(buffer, digits, point);
            buffer[point] = '.';
            memcpy(buffer + point + 1u, digits + point, places + 1u);
            return;
        }
    } else if (1u + places + 1u <= size) {
        /* No zero padding: 6 with one place renders ".6". */
        buffer[0] = '.';
        memcpy(buffer + 1u, digits, count + 1u);
        return;
    }
    snprintf(buffer, size, "%u", value);
}

static void record_title(const uint8_t *record, char *buffer, size_t size)
{
    /* Names at +19/+32/+45 joined with single spaces (fn_0496E trims
     * between appends, so gaps collapse; no record has a gap anyway). */
    size_t out = 0u;

    buffer[0] = 0;
    for (unsigned int field = 0u; field < 3u; field++) {
        const uint8_t *name = record + 19u + field * 13u;
        size_t length = 0u;

        while (length < 13u && name[length] != 0) {
            length++;
        }
        while (length > 0u && name[length - 1u] == ' ') {
            length--;
        }
        if (length == 0u) {
            continue;
        }
        if (out > 0u && out + 1u < size) {
            buffer[out++] = ' ';
        }
        if (out + length >= size) {
            length = size - out - 1u;
        }
        memcpy(buffer + out, name, length);
        out += length;
    }
    buffer[out] = 0;
}

/* Parameter-row source for a record: table base + byte index + row
 * length, following fn_02262's flag order (0xE00, 0xC000, 0x100).
 * Returns 0 with *length set to 0 when the record selects no table. */
static unsigned int param_source(const uint8_t *record, size_t *length)
{
    uint16_t flags = read_u16(record + 12);

    if ((flags & 0xE00u) != 0u) {
        *length = 12u;
        return YT2_ITEM_PARAM_AF0;
    }
    if ((flags & 0xC000u) != 0u) {
        *length = 12u;
        return YT2_ITEM_PARAM_FA4;
    }
    if ((flags & 0x100u) != 0u) {
        *length = 8u;
        return YT2_ITEM_PARAM_17D4;
    }
    *length = 0u;
    return 0u;
}

static const uint8_t *param_row(const yt2_items *items,
                                const uint8_t *record)
{
    size_t length;
    unsigned int base = param_source(record, &length);
    uint16_t index;

    if (length == 0u) {
        return NULL;
    }
    index = read_u16(record + 0);
    return table_at(items, base, index, length);
}

/* +N box count for a base file (sub_04573 verbatim): no boxes without a
 * parameter row whose continuation bit is set (0x100 in row word 2 for
 * the 0xE00/0x100-selected tables... see below), then two boxes
 * unconditionally plus up to nine consecutive qualifying variants. */
unsigned int yt2_item_plus_count(const yt2_items *items, unsigned int file,
                                unsigned int category)
{
    const uint8_t *record;
    const uint8_t *row;
    uint16_t flags;
    uint16_t need;
    unsigned int count;

    (void)category;
    record = record_at(items, file);
    if (record == NULL) {
        return 0u;
    }
    /* Only armor and weapons mains run the +N setup at all. */
    if (category != YT2_ITEMS_ARMOR && category != YT2_ITEMS_WEAPONS) {
        return 0u;
    }
    row = param_row(items, record);
    if (row == NULL) {
        return 0u;
    }
    flags = read_u16(record + 12);
    if ((flags & 0xC000u) != 0u) {
        need = 0x800u;
    } else if ((flags & 0xE00u) != 0u) {
        need = 0x100u;
    } else {
        return 0u;
    }
    if ((read_u16(row + 2) & need) == 0u) {
        return 0u;
    }
    /*
     * Boxes 1-2 are added unconditionally; each further box N is gated
     * on variant file+N-2 (the loop tests the file below the box it
     * adds, up to nine more iterations).
     */
    count = 2u;
    while (count < YT2_ITEM_MAX_PLUS) {
        const uint8_t *variant;
        const uint8_t *variant_row;

        if (file + count - 1u >= YT2_ITEMS_COUNT) {
            break;
        }
        variant = record_at(items, file + count - 1u);
        if (variant == NULL) {
            break;
        }
        variant_row = param_row(items, variant);
        if (variant_row == NULL ||
            (read_u16(variant_row + 2) & need) == 0u) {
            break;
        }
        count++;
    }
    return count;
}

int yt2_item_page_render(yt2_surface *surface, const yt2_pictures *pictures,
                         const yt2_items *items, unsigned int file,
                         unsigned int category, unsigned int plus)
{
    const yt2_picture_group *group;
    const uint8_t *image;
    const uint8_t *record;
    const uint8_t *params;
    const uint8_t *adds;
    uint16_t flags;
    uint16_t file_one;
    unsigned int shown;
    char title[64];
    char text[32];

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        items == NULL || items->bytes == NULL ||
        file >= YT2_ITEMS_COUNT || category >= YT2_ITEMS_CATEGORIES ||
        category == YT2_ITEMS_ATTRIBUTE ||
        category == YT2_ITEMS_TRANSPORT) {
        return YT2_STATUS_ERROR;
    }
    if (plus >= yt2_item_plus_count(items, file, category) &&
        yt2_item_plus_count(items, file, category) > 0u) {
        return YT2_STATUS_ERROR;
    }
    if (file + plus >= YT2_ITEMS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    shown = file + plus;
    record = record_at(items, shown);
    if (record == NULL) {
        return YT2_STATUS_ERROR;
    }
    group = yt2_pictures_group(pictures, DETAIL_BG_GROUP);
    image = yt2_picture_data(pictures, DETAIL_BG_GROUP, DETAIL_BG_PICTURE);
    if (group == NULL || image == NULL) {
        return YT2_STATUS_ERROR;
    }
    flags = read_u16(record + 12);
    file_one = (uint16_t)(shown + 1u);
    params = param_row(items, record);
    adds = NULL;
    if (read_u16(record + 2) != 0u) {
        adds = table_at(items, YT2_ITEM_ADDS_OFFSET, read_u16(record + 2),
                        16u);
        if (adds == NULL) {
            return YT2_STATUS_ERROR;
        }
    }

    /* Chrome: background, title, category header (fn_04F7C). */
    yt2_surface_clear(surface, 0u);
    if (yt2_surface_blit(surface, DETAIL_BG_X, DETAIL_BG_Y, image,
                         group->width,
                         group->height) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    record_title(record, title, sizeof(title));
    yt2_text_draw(surface, DETAIL_TITLE_X, DETAIL_TITLE_Y, title,
                  DETAIL_TITLE_COLOR);
    yt2_text_draw(surface, category_header_x[category], DETAIL_TITLE_Y,
                  category_names[category], DETAIL_TITLE_COLOR);

    /* Icon (sub_0342A): group 8, struct+8 (+1 with flag 0x1000). */
    {
        const uint8_t *icon;
        unsigned int index = read_u16(record + 8);

        if ((flags & 0x1000u) != 0u) {
            index++;
        }
        icon = yt2_picture_data(pictures, DETAIL_ICON_GROUP, index);
        if (icon == NULL) {
            return YT2_STATUS_ERROR;
        }
        group = yt2_pictures_group(pictures, DETAIL_ICON_GROUP);
        if (group == NULL) {
            return YT2_STATUS_ERROR;
        }
        if (yt2_surface_blit_transparent(surface, DETAIL_ICON_X,
                                         DETAIL_ICON_Y, icon, group->width,
                                         group->height,
                                         DETAIL_ICON_TRANSPARENT) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }

    /* BASE VALUE (sub_04648): label plus BCD value unless zero. */
    yt2_text_draw(surface, DETAIL_BASE_LABEL_X, DETAIL_BASE_Y, "BASE VALUE:",
                  DETAIL_LABEL_COLOR);
    if (read_bcd32(record + 4) != 0u) {
        format_commas(text, sizeof(text), read_bcd32(record + 4));
        yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_BASE_Y, text,
                      DETAIL_GOLD_COLOR);
    }

    /* WEIGHT (sub_046BF): label plus value/10 unless zero. */
    yt2_text_draw(surface, DETAIL_WEIGHT_LABEL_X, DETAIL_WEIGHT_Y, "WEIGHT:",
                  DETAIL_LABEL_COLOR);
    if (read_u16(record + 10) != 0u) {
        format_decimal(text, sizeof(text), read_u16(record + 10), 1u);
        yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_WEIGHT_Y, text,
                      DETAIL_GOLD_COLOR);
    }

    /* FITS IN (sub_034B9). */
    yt2_text_draw(surface, DETAIL_FITS_LABEL_X, DETAIL_FITS_Y, "FITS IN-",
                  DETAIL_LABEL_COLOR);
    {
        uint16_t fits = read_u16(record + 14);
        char fits_text[48] = "";

        if ((fits & 0xE000u) != 0u) {
            /* The original prints pieces with fixed advances (+54/+24
             * pixels); the concatenation matches pixel for pixel
             * because the pieces carry their trailing spaces. */
            if ((fits & 0x8000u) != 0u) {
                strcat(fits_text, "BACKPACK ");
            }
            if ((fits & 0x4000u) != 0u) {
                strcat(fits_text, "BOX ");
            }
            if ((fits & 0x2000u) != 0u) {
                strcat(fits_text, "BAG");
            }
            {
                size_t end = strlen(fits_text);

                while (end > 0u && fits_text[end - 1u] == ' ') {
                    fits_text[--end] = 0;
                }
            }
        } else if ((flags & 0x2000u) != 0u) {
            strcpy(fits_text, "CHARACTER PANEL");
        } else {
            strcpy(fits_text, "ANY PANEL");
        }
        yt2_text_draw(surface, DETAIL_FITS_X, DETAIL_FITS_Y, fits_text,
                      DETAIL_SPECIAL_COLOR);
    }

    /* Per-category bodies. */
    if (category == YT2_ITEMS_ARMOR) {
        /* ABSORPTION (sub_03532). */
        yt2_text_draw(surface, DETAIL_BASE_LABEL_X, DETAIL_STAT_Y,
                      "ABSORPTION-", DETAIL_LABEL_COLOR);
        if (params != NULL && read_u16(params + 0) != 0u) {
            snprintf(text, sizeof(text), "%u", read_u16(params + 0));
            yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y, text,
                          DETAIL_WORD_COLOR);
        }
    } else if (category == YT2_ITEMS_WEAPONS) {
        /* DAMAGE / SKILL / 2-HANDED (sub_0360E/sub_048D0). */
        yt2_text_draw(surface, 115, DETAIL_STAT_Y, "DAMAGE:",
                      DETAIL_LABEL_COLOR);
        if (params != NULL && read_u16(params + 0) != 0u) {
            snprintf(text, sizeof(text), "%u", read_u16(params + 0));
            yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y, text,
                          DETAIL_WORD_COLOR);
        }
        yt2_text_draw(surface, 121, 90, "SKILL:", DETAIL_LABEL_COLOR);
        if (params != NULL) {
            static const uint16_t bits[5] = {
                0x8000u, 0x4000u, 0x2000u, 0x1000u, 0x0000u
            };
            uint16_t mode = read_u16(params + 2);
            const char *skill = skill_names[0];
            unsigned int i;

            for (i = 0u; i < 4u; i++) {
                if ((mode & bits[i]) != 0u) {
                    skill = skill_names[i];
                    break;
                }
            }
            /* No bit set falls through to the first entry. */
            yt2_text_draw(surface, DETAIL_VALUE_X, 90, skill,
                          DETAIL_FLAG_COLOR);
        }
        yt2_text_draw(surface, 103, 120, "2-HANDED:", DETAIL_LABEL_COLOR);
        if (params != NULL) {
            yt2_text_draw(surface, DETAIL_VALUE_X, 120,
                          (read_u16(params + 2) & 1u) != 0u ? "YES" : "NO",
                          DETAIL_FLAG_COLOR);
        }
    } else {
        /* fn_02E27 conditional stat row. */
        if ((flags & 0x1000u) != 0u) {
            /* DURATION (sub_035CE): value*10 MINUTES. */
            yt2_text_draw(surface, 103, DETAIL_STAT_Y, "DURATION-",
                          DETAIL_LABEL_COLOR);
            if (params != NULL && read_u16(params + 4) != 0u) {
                snprintf(text, sizeof(text), "%u",
                         (unsigned int)read_u16(params + 4) * 10u);
                yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y, text,
                              DETAIL_WORD_COLOR);
            }
            yt2_text_draw(surface, 181, DETAIL_STAT_Y, "MINUTES",
                          DETAIL_TEXT_COLOR);
        } else if (file_one >= 54u && file_one <= 70u) {
            /* HEALTH-/MAGIC- (sub_03575). */
            int label_x = 115;
            const char *label = "HEALTH-";

            if (params != NULL && (read_u16(params + 2) & 0x8000u) != 0u) {
                label_x = 121;
                label = "MAGIC-";
            }
            yt2_text_draw(surface, label_x, DETAIL_STAT_Y, label,
                          DETAIL_LABEL_COLOR);
            if (params != NULL && read_u16(params + 4) != 0u) {
                unsigned int value = read_u16(params + 4);

                snprintf(text, sizeof(text), "%u", value);
                yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y, text,
                              DETAIL_WORD_COLOR);
            }
            yt2_text_draw(surface,
                          params != NULL && read_u16(params + 4) < 10u ? 169
                                                                      : 175,
                          DETAIL_STAT_Y, "PERCENT", DETAIL_TEXT_COLOR);
        } else if (file_one == 576u ||
                   (file_one >= 18u && file_one <= 29u)) {
            /* Quirks (sub_037A0). */
            if (adds != NULL) {
                uint16_t id = read_u16(adds + 0);

                yt2_text_draw(surface,
                              id == 62u ? 97 : 103, DETAIL_STAT_Y,
                              id == 62u ? "DEXTERITY-" : "STRENGTH-",
                              DETAIL_LABEL_COLOR);
                if (read_u16(adds + 2) != 0u) {
                    snprintf(text, sizeof(text), "%u", read_u16(adds + 2));
                    yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y,
                                  text, DETAIL_WORD_COLOR);
                }
            } else if (params != NULL) {
                /* Named-file rows: DAMAGE block with bit 0 of the
                 * parameter word set, RESTORES/CURES/CONVERTS
                 * block without it. */
                if ((read_u16(params + 0) & 1u) != 0u) {
                    /* DAMAGE block. */
                    if (file_one == 27u || file_one == 25u ||
                        file_one == 26u || file_one == 576u) {
                        unsigned int value;

                        if (file_one == 27u) {
                            value = 50u;
                        } else if (file_one == 25u) {
                            value = 60u;
                        } else if (file_one == 26u) {
                            value = 35u;
                        } else {
                            value = 40u;
                        }
                        yt2_text_draw(surface, 115, DETAIL_STAT_Y, "DAMAGE-",
                                      DETAIL_LABEL_COLOR);
                        snprintf(text, sizeof(text), "%u", value);
                        yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y,
                                      text, DETAIL_WORD_COLOR);
                        if (file_one == 26u) {
                            yt2_text_draw(surface, 175, DETAIL_STAT_Y,
                                          "POISON", DETAIL_FLAG_COLOR);
                        } else if (file_one == 27u) {
                            yt2_text_draw(surface, 175, DETAIL_STAT_Y,
                                          "UNDEAD", DETAIL_FLAG_COLOR);
                        } else if (file_one == 576u) {
                            yt2_text_draw(surface, 175, DETAIL_STAT_Y, "3 X 3",
                                          DETAIL_FLAG_COLOR);
                        }
                    }
                } else if (file_one == 29u || file_one == 23u ||
                           file_one == 18u || file_one == 19u ||
                           file_one == 20u) {
                    unsigned int value = 0u;
                    const char *suffix = "PERCENT OF MAGIC";

                    if (file_one == 29u) {
                        value = 50u;
                    } else if (file_one == 23u) {
                        value = 100u;
                    } else if (file_one == 18u) {
                        value = 25u;
                        suffix = "PERCENT OF HEALTH";
                    } else if (file_one == 19u) {
                        value = 50u;
                        suffix = "PERCENT OF HEALTH";
                    } else {
                        value = 100u;
                        suffix = "PERCENT OF HEALTH";
                    }
                    yt2_text_draw(surface, 103, DETAIL_STAT_Y, "RESTORES-",
                                  DETAIL_LABEL_COLOR);
                    snprintf(text, sizeof(text), "%u", value);
                    yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y,
                                  text, DETAIL_WORD_COLOR);
                    yt2_text_draw(surface, value >= 100u ? 181 : 175,
                                  DETAIL_STAT_Y, suffix, DETAIL_FLAG_COLOR);
                } else if (file_one == 24u) {
                    yt2_text_draw(surface, 121, DETAIL_STAT_Y, "CURES-",
                                  DETAIL_LABEL_COLOR);
                    yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y,
                                  "SICK, POISON, DISEASE", DETAIL_WORD_COLOR);
                } else if (file_one == 28u) {
                    yt2_text_draw(surface, 103, DETAIL_STAT_Y, "CONVERTS-",
                                  DETAIL_LABEL_COLOR);
                    yt2_text_draw(surface, DETAIL_VALUE_X, DETAIL_STAT_Y,
                                  "MAGIC ORE AND NUORE", DETAIL_WORD_COLOR);
                }
            }
        }
    }

    /* PROTECTIONS (sub_04826) and ADDS (sub_04733): armor only. */
    if (category == YT2_ITEMS_ARMOR) {
        unsigned int row;

        yt2_text_draw(surface, DETAIL_PROT_LABEL_X, DETAIL_PROT_Y,
                      "PROTECTIONS:", DETAIL_LABEL_COLOR);
        if (adds != NULL) {
            row = 0u;
            for (unsigned int i = 0u; i < 4u; i++) {
                uint16_t id = read_u16(adds + i * 4u);

                if (id == 0u) {
                    break;
                }
                if (id > 48u) {
                    continue;
                }
                snprintf(text, sizeof(text), "%u",
                         read_u16(adds + i * 4u + 2u));
                yt2_text_draw(surface, DETAIL_LIST_VALUE_X,
                              DETAIL_PROT_Y + (int)row * DETAIL_LIST_STEP,
                              text, DETAIL_FLAG_COLOR);
                yt2_text_draw(surface,
                              DETAIL_LIST_VALUE_X + DETAIL_LIST_NAME_DX,
                              DETAIL_PROT_Y + (int)row * DETAIL_LIST_STEP,
                              protection_names[(id - 32u) / 2u],
                              DETAIL_FLAG_COLOR);
                row++;
            }
        }
        yt2_text_draw(surface, DETAIL_ADDS_LABEL_X, DETAIL_ADDS_Y, "ADDS:",
                      DETAIL_LABEL_COLOR);
        if (adds != NULL) {
            row = 0u;
            for (unsigned int i = 0u; i < 4u; i++) {
                uint16_t id = read_u16(adds + i * 4u);

                if (id == 0u) {
                    break;
                }
                if (id < 124u) {
                    continue;
                }
                snprintf(text, sizeof(text), "%u",
                         read_u16(adds + i * 4u + 2u));
                yt2_text_draw(surface, DETAIL_LIST_VALUE_X,
                              DETAIL_ADDS_Y + (int)row * DETAIL_LIST_STEP,
                              text, DETAIL_FLAG_COLOR);
                yt2_text_draw(surface,
                              DETAIL_LIST_VALUE_X + DETAIL_LIST_NAME_DX,
                              DETAIL_ADDS_Y + (int)row * DETAIL_LIST_STEP,
                              adds_names[(id - 124u) / 2u],
                              DETAIL_FLAG_COLOR);
                row++;
            }
        }
    }

    /* +N boxes (sub_047DD): armor and weapons with boxes. */
    {
        unsigned int count = yt2_item_plus_count(items, file, category);

        for (unsigned int box = 0u; box < count; box++) {
            const uint8_t *box_image = yt2_picture_data(
                pictures, DETAIL_ICON_GROUP,
                DETAIL_PLUS_PIC_BASE + box * 2u + (box == plus ? 1u : 0u));

            if (box_image == NULL) {
                return YT2_STATUS_ERROR;
            }
            group = yt2_pictures_group(pictures, DETAIL_ICON_GROUP);
            if (group == NULL) {
                return YT2_STATUS_ERROR;
            }
            if (yt2_surface_blit_transparent(
                    surface, DETAIL_PLUS_X0 + (int)box * DETAIL_PLUS_STEP,
                    DETAIL_PLUS_Y, box_image, group->width, group->height,
                    DETAIL_ICON_TRANSPARENT) != YT2_STATUS_OK) {
                return YT2_STATUS_ERROR;
            }
        }
    }

    /* Footer (d LIST plus the F5 icon bar). */
    yt2_text_draw(surface, YT2_ITEMS_DETAIL_FOOTER_X,
                  YT2_ITEMS_DETAIL_FOOTER_Y, YT2_ITEMS_DETAIL_FOOTER,
                  YT2_ITEMS_DETAIL_FOOTER_COLOR);
    for (unsigned int index = 0; index < YT2_LIST_ICON_COUNT; index++) {
        const uint8_t *icon = yt2_picture_data(pictures, YT2_LIST_ICON_GROUP,
                                               item_detail_icons[index]);

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
