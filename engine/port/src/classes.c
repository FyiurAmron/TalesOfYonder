#include "yt2/classes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"
#include "yt2/text.h"

/* The three nine-name tiers, exactly as shipped in SWREG.EXE. */
static const char *const tier_names[3][9] = {
    {
        "FIGHTER", "MERCHANT", "ROGUE", "MONK", "ALCHEMIST", "PALADIN",
        "MAGE", "DRUID", "MARKSMAN"
    },
    {
        "WARRIOR", "TINKERER", "THIEF", "CLERIC", "TRANSMUTER", "CAVALIER",
        "WIZARD", "ENCHANTER", "RANGER"
    },
    {
        "CHAMPION", "BLACKSMITH", "ASSASSIN", "PRIEST", "HEALER", "HERO",
        "SORCERER", "SAGE", "KNIGHT"
    }
};

/*
 * The six lines' spell memberships as file indices in file order (sorted
 * by name at load): monk, alchemist, paladin, mage, druid, marksman.
 */
static const uint8_t class_monk[] = {
    0u, 2u, 4u, 5u, 6u, 7u, 10u, 13u, 15u, 18u, 19u, 21u, 22u, 24u, 26u,
    28u, 29u, 30u, 33u, 34u, 35u, 37u, 38u, 41u, 44u, 45u, 48u, 51u, 54u,
    57u, 58u, 60u, 62u, 65u, 66u, 67u, 68u, 72u, 74u, 76u, 78u, 79u, 80u,
    81u, 84u, 88u, 89u, 95u, 96u, 98u, 100u, 103u
};
static const uint8_t class_alchemist[] = {
    0u, 1u, 4u, 5u, 6u, 7u, 9u, 10u, 13u, 14u, 15u, 17u, 19u, 20u, 22u,
    23u, 24u, 25u, 28u, 29u, 30u, 32u, 33u, 35u, 38u, 41u, 45u, 46u, 48u,
    51u, 52u, 54u, 57u, 58u, 60u, 62u, 65u, 66u, 67u, 72u, 73u, 78u, 79u,
    80u, 81u, 83u, 91u, 92u, 94u, 100u
};
static const uint8_t class_paladin[] = {
    0u, 1u, 2u, 4u, 5u, 10u, 13u, 15u, 18u, 19u, 21u, 22u, 24u, 25u, 26u,
    29u, 35u, 37u, 41u, 47u, 48u, 54u, 58u, 62u, 65u, 68u, 72u, 76u, 80u,
    81u, 84u, 89u
};
static const uint8_t class_mage[] = {
    1u, 2u, 3u, 5u, 8u, 9u, 11u, 12u, 14u, 16u, 21u, 23u, 27u, 28u, 31u,
    32u, 34u, 36u, 39u, 40u, 42u, 43u, 46u, 49u, 50u, 53u, 55u, 56u, 59u,
    61u, 64u, 65u, 69u, 70u, 71u, 75u, 77u, 82u, 85u, 86u, 90u, 92u, 93u,
    97u, 101u, 102u, 104u
};
static const uint8_t class_druid[] = {
    0u, 1u, 3u, 5u, 6u, 9u, 11u, 13u, 14u, 16u, 17u, 20u, 22u, 23u, 24u,
    25u, 27u, 28u, 31u, 32u, 36u, 40u, 41u, 42u, 46u, 49u, 50u, 52u, 53u,
    55u, 59u, 64u, 65u, 69u, 70u, 71u, 72u, 73u, 75u, 82u, 83u, 85u, 87u,
    92u, 94u, 99u, 104u
};
static const uint8_t class_marksman[] = {
    1u, 2u, 3u, 5u, 8u, 9u, 11u, 12u, 16u, 21u, 23u, 25u, 27u, 31u, 32u,
    34u, 42u, 46u, 50u, 53u, 55u, 59u, 63u, 64u, 65u, 69u, 71u, 75u, 77u,
    82u, 85u
};

static const struct {
    const uint8_t *members;
    uint8_t count;
} class_table[YT2_CLASSES_COUNT] = {
    { class_monk, sizeof(class_monk) },
    { class_alchemist, sizeof(class_alchemist) },
    { class_paladin, sizeof(class_paladin) },
    { class_mage, sizeof(class_mage) },
    { class_druid, sizeof(class_druid) },
    { class_marksman, sizeof(class_marksman) }
};

static const unsigned int class_icons[YT2_LIST_ICON_COUNT] = {
    32u, 325u, 327u, 340u, 329u, 331u, 333u
};

typedef struct class_entry {
    char name[YT2_SPELLS_NAME_SIZE + 1u];
    uint8_t index;
} class_entry;

static int compare_entries(const void *a, const void *b)
{
    const class_entry *left = (const class_entry *)a;
    const class_entry *right = (const class_entry *)b;
    int order = strcmp(left->name, right->name);

    if (order != 0) {
        return order;
    }
    return (int)left->index - (int)right->index;
}

int yt2_classes_load(yt2_classes *classes, const yt2_spells *spells)
{
    class_entry entries[YT2_CLASSES_MAX_MEMBERS];

    if (classes == NULL || spells == NULL || spells->bytes == NULL) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int class = 0u; class < YT2_CLASSES_COUNT; class++) {
        uint8_t count = class_table[class].count;

        if (count == 0u || count > YT2_CLASSES_MAX_MEMBERS) {
            return YT2_STATUS_ERROR;
        }
        for (unsigned int i = 0u; i < count; i++) {
            uint8_t index = class_table[class].members[i];

            if (yt2_spell_name_by_index(spells, index, entries[i].name,
                                        sizeof(entries[i].name)) !=
                YT2_STATUS_OK) {
                return YT2_STATUS_ERROR;
            }
            entries[i].index = index;
        }
        qsort(entries, count, sizeof(entries[0]), compare_entries);
        for (unsigned int rank = 0u; rank < count; rank++) {
            classes->members[class][rank] = entries[rank].index;
        }
        classes->counts[class] = count;
    }
    classes->spells = spells;
    return YT2_STATUS_OK;
}

unsigned int yt2_class_count(const yt2_classes *classes, unsigned int class)
{
    if (classes == NULL || classes->spells == NULL ||
        class >= YT2_CLASSES_COUNT) {
        return 0u;
    }
    return classes->counts[class];
}

int yt2_class_row_text(unsigned int class, char *buffer, size_t size)
{
    int written;

    if (buffer == NULL || size == 0u || class >= YT2_CLASSES_COUNT) {
        return YT2_STATUS_ERROR;
    }
    written = snprintf(buffer, size, "%s / %s / %s",
                       tier_names[0][YT2_CLASSES_BASE_FIRST + class],
                       tier_names[1][YT2_CLASSES_BASE_FIRST + class],
                       tier_names[2][YT2_CLASSES_BASE_FIRST + class]);
    if (written < 0 || (size_t)written >= size) {
        return YT2_STATUS_ERROR;
    }
    return YT2_STATUS_OK;
}

int yt2_class_spell_name(const yt2_classes *classes, unsigned int class,
                         unsigned int rank, char *buffer, size_t size)
{
    if (classes == NULL || classes->spells == NULL ||
        class >= YT2_CLASSES_COUNT || rank >= classes->counts[class]) {
        return YT2_STATUS_ERROR;
    }
    return yt2_spell_name_by_index(classes->spells,
                                   classes->members[class][rank], buffer,
                                   size);
}

static int class_list_row_text(void *ctx, unsigned int rank, char *buffer,
                               size_t size)
{
    (void)ctx;
    return yt2_class_row_text(rank, buffer, size);
}

int yt2_classes_render_list(yt2_surface *surface,
                            const yt2_pictures *pictures,
                            const yt2_classes *classes, unsigned int selected)
{
    yt2_list_page page;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        classes == NULL || classes->spells == NULL ||
        selected >= YT2_CLASSES_COUNT) {
        return YT2_STATUS_ERROR;
    }
    page.header = YT2_CLASSES_HEADER;
    page.title = NULL;
    page.title_x = 0;
    page.header_x = YT2_CLASSES_HEADER_X;
    page.icons = class_icons;
    page.count = YT2_CLASSES_COUNT;
    page.selected = selected;
    page.row_text = class_list_row_text;
    page.ctx = NULL;
    return yt2_list_render(surface, pictures, &page);
}

typedef struct detail_ctx {
    const yt2_classes *classes;
    unsigned int class;
} detail_ctx;

static int class_detail_row_text(void *ctx, unsigned int rank, char *buffer,
                                 size_t size)
{
    const detail_ctx *detail = (const detail_ctx *)ctx;

    return yt2_class_spell_name(detail->classes, detail->class, rank, buffer,
                                size);
}

int yt2_classes_render_detail(yt2_surface *surface,
                              const yt2_pictures *pictures,
                              const yt2_classes *classes, unsigned int class,
                              unsigned int selected)
{
    yt2_list_page page;
    detail_ctx ctx;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        classes == NULL || classes->spells == NULL ||
        class >= YT2_CLASSES_COUNT ||
        selected >= classes->counts[class]) {
        return YT2_STATUS_ERROR;
    }
    ctx.classes = classes;
    ctx.class = class;
    page.header = YT2_CLASSES_DETAIL_HEADER;
    page.title = NULL;
    page.title_x = 0;
    page.header_x = YT2_CLASSES_DETAIL_HEADER_X;
    page.icons = class_icons;
    page.count = classes->counts[class];
    page.selected = selected;
    page.row_text = class_detail_row_text;
    page.ctx = &ctx;
    if (yt2_list_render(surface, pictures, &page) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    yt2_text_draw(surface, YT2_CLASSES_DETAIL_FOOTER_X,
                  YT2_CLASSES_DETAIL_FOOTER_Y, YT2_CLASSES_DETAIL_FOOTER,
                  YT2_CLASSES_DETAIL_FOOTER_COLOR);
    return YT2_STATUS_OK;
}
