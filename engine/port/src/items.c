#include "yt2/items.h"

#include <stdio.h>
#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"
#include "yt2/text.h"

/* The eight category names, exactly as shipped in SWREG.EXE at 2A6A4h. */
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

/* Right-aligned detail headers share the 315 right edge. */
static const int category_header_x[YT2_ITEMS_CATEGORIES] = {
    249, 201, 141, 195, 273, 237, 225, 273
};

/*
 * The six item categories' members as file indices in observed (display)
 * order: armor, jewels, scrolls, potions, supplies, weapons.
 */
static const uint16_t items_armor[] = {
    289u, 389u, 228u, 339u, 311u, 361u, 167u, 186u, 166u, 164u, 173u, 175u,
    400u, 264u, 239u, 300u, 350u, 172u, 176u, 170u, 211u, 171u, 174u, 367u,
    250u, 222u, 275u, 317u, 373u, 323u, 163u, 381u, 256u, 281u, 331u, 169u,
    200u, 168u, 192u, 165u, 180u
};
static const uint16_t items_jewels[] = {
    592u, 577u, 581u, 601u, 596u, 594u, 162u, 600u, 623u, 585u, 621u, 619u,
    622u, 620u, 152u, 597u, 595u, 624u, 625u, 628u, 626u, 627u, 154u, 151u,
    591u, 630u, 631u, 629u, 633u, 632u, 602u, 49u, 48u, 47u, 50u, 51u, 712u,
    582u, 157u, 598u, 584u, 29u, 578u, 579u, 580u, 599u, 583u, 161u, 153u,
    590u, 159u, 160u
};
static const uint16_t items_scrolls[] = {
    708u, 696u, 700u, 680u, 742u, 692u, 684u, 693u, 682u, 735u, 701u, 685u,
    681u, 706u, 707u, 699u, 679u, 688u, 741u, 690u, 738u, 686u, 732u, 702u,
    695u, 703u, 698u, 736u, 694u, 683u, 704u, 734u, 743u, 689u, 740u, 691u,
    687u, 737u, 697u, 739u, 705u, 709u, 733u
};
static const uint16_t items_potions[] = {
    17u, 26u, 27u, 575u, 24u, 18u, 23u, 21u, 28u, 22u, 20u, 25u, 19u
};
static const uint16_t items_supplies[] = {
    57u, 3u, 5u, 4u, 32u, 39u, 54u, 33u, 40u, 7u, 56u, 34u, 41u, 38u, 45u,
    6u, 35u, 42u, 46u, 31u, 64u, 53u, 37u, 44u, 36u, 43u, 13u
};
static const uint16_t items_weapons[] = {
    504u, 482u, 441u, 417u, 411u, 471u, 515u, 463u, 564u, 447u, 553u, 435u,
    493u, 527u, 730u, 729u, 731u, 547u, 526u, 541u, 429u, 423u, 718u, 455u
};

static const struct {
    const uint16_t *members;
    unsigned int count;
} category_table[YT2_ITEMS_CATEGORIES] = {
    { items_armor, sizeof(items_armor) / sizeof(items_armor[0]) },
    { NULL, 0u },
    { items_jewels, sizeof(items_jewels) / sizeof(items_jewels[0]) },
    { items_scrolls, sizeof(items_scrolls) / sizeof(items_scrolls[0]) },
    { items_potions, sizeof(items_potions) / sizeof(items_potions[0]) },
    { items_supplies, sizeof(items_supplies) / sizeof(items_supplies[0]) },
    { NULL, 0u },
    { items_weapons, sizeof(items_weapons) / sizeof(items_weapons[0]) }
};

static const unsigned int items_icons[YT2_LIST_ICON_COUNT] = {
    32u, 325u, 327u, 339u, 330u, 331u, 333u
};

#define INFO_PICTURE_GROUP 0u
#define INFO_PICTURE_INDEX 13u
#define INFO_PICTURE_X 1
#define INFO_PICTURE_Y 1

#define INFO_TITLE_Y 4
#define INFO_TITLE_COLOR 0x0Du
#define INFO_TITLE_LEFT_X 6
#define INFO_TITLE_RIGHT_X 225

typedef struct info_row {
    const char *keyword;
    const char *amount;
    const char *target;
    unsigned int icon;
} info_row;

/*
 * The six enhancer lines: "{keyword} PERMANENTLY ADD {amount} TO
 * A{N} {target}".  Words from SWREG.EXE at 2A799h; amounts and icons as
 * observed (potions/scrolls/wands add 2/4/6 to attributes, vials/
 * parchments/rods 1/3/5 to skills).
 */
static const info_row info_rows[] = {
    { "POTIONS", "2", "ATTRIBUTE", 218u },
    { "SCROLLS", "4", "ATTRIBUTE", 241u },
    { "WANDS", "6", "ATTRIBUTE", 243u },
    { "VIALS", "1", "SKILL", 240u },
    { "PARCHMENTS", "3", "SKILL", 242u },
    { "RODS", "5", "SKILL", 244u }
};

#define INFO_ROW_Y 47
#define INFO_ROW_STEP 18
#define INFO_ICON_X 28
#define INFO_ICON_DY -6
#define INFO_TEXT_X 47
#define INFO_KEYWORD_COLOR 0xCAu
#define INFO_MIDDLE_COLOR 0x0Au
#define INFO_AMOUNT_COLOR 0x59u
#define INFO_TARGET_COLOR 0xA7u

typedef struct transport_entry {
    const char *name;
    const char *value;
    const char *uses;
    int windowed;
} transport_entry;

/*
 * The three rides.  Creature records live in SWREG.EXE at 2946Ch (13-byte
 * name plus 13 data bytes, stride 26): the value is data byte 2 times
 * 10000 printed with a comma, uses is data byte 9, and data byte 11
 * selects ANYTIME (3) or the night window (1).  A fourth record (FLYING
 * RUG) exists but the screen never shows it.
 */
static const transport_entry transport_entries[] = {
    { "PEGASUS", "10,000", "1", 0 },
    { "GIANT EAGLE", "30,000", "2", 0 },
    { "MAGIC DRAGON", "70,000", "4", 1 }
};

#define TRANSPORT_Y 26
#define TRANSPORT_STEP 48
#define TRANSPORT_NAME_X 91
#define TRANSPORT_NAME_COLOR 0x0Du
#define TRANSPORT_LABEL_COLOR 0x0Au
#define TRANSPORT_VALUE_X 127
#define TRANSPORT_VALUE_COLOR 0x8Au
#define TRANSPORT_USES_COLOR 0x59u
#define TRANSPORT_TIME_COLOR 0xCAu
#define TRANSPORT_WRAP_X 175
#define TRANSPORT_WRAP_DY 9

int yt2_items_load(yt2_items *items, const yt2_world *world)
{
    if (items == NULL || world == NULL || world->bytes == NULL ||
        world->size < YT2_ITEMS_OFFSET +
                          YT2_ITEMS_COUNT * YT2_ITEMS_RECORD_SIZE) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int i = 0u; i < YT2_ITEMS_COUNT; i++) {
        const uint8_t *record = world->bytes + YT2_ITEMS_OFFSET +
                                (size_t)i * YT2_ITEMS_RECORD_SIZE;

        for (unsigned int field = 0u; field < YT2_ITEMS_NAME_FIELDS;
             field++) {
            if (record[field * YT2_ITEMS_NAME_FIELD_SIZE +
                       YT2_ITEMS_NAME_FIELD_SIZE - 1u] != 0u) {
                return YT2_STATUS_ERROR;
            }
        }
    }
    items->bytes = world->bytes;
    items->size = world->size;
    return YT2_STATUS_OK;
}

unsigned int yt2_items_category_count(unsigned int category)
{
    if (category >= YT2_ITEMS_CATEGORIES) {
        return 0u;
    }
    return category_table[category].count;
}

int yt2_items_category_name(unsigned int category, char *buffer, size_t size)
{
    int written;

    if (buffer == NULL || size == 0u || category >= YT2_ITEMS_CATEGORIES) {
        return YT2_STATUS_ERROR;
    }
    written = snprintf(buffer, size, "%s", category_names[category]);
    if (written < 0 || (size_t)written >= size) {
        return YT2_STATUS_ERROR;
    }
    return YT2_STATUS_OK;
}

static int parse_name(const uint8_t *record, char *name, size_t size)
{
    char parts[YT2_ITEMS_NAME_FIELDS][YT2_ITEMS_NAME_FIELD_SIZE];
    size_t lengths[YT2_ITEMS_NAME_FIELDS];
    size_t total = 0u;
    size_t at = 0u;

    for (unsigned int field = 0u; field < YT2_ITEMS_NAME_FIELDS; field++) {
        const uint8_t *bytes = record + field * YT2_ITEMS_NAME_FIELD_SIZE;

        if (bytes[YT2_ITEMS_NAME_FIELD_SIZE - 1u] != 0u) {
            return YT2_STATUS_ERROR;
        }
        memcpy(parts[field], bytes, YT2_ITEMS_NAME_FIELD_SIZE);
        lengths[field] = strlen(parts[field]);
        while (lengths[field] > 0u &&
               parts[field][lengths[field] - 1u] == ' ') {
            lengths[field]--;
        }
        parts[field][lengths[field]] = '\0';
        if (lengths[field] > 0u) {
            total += lengths[field] + 1u;
        }
    }
    if (total == 0u || total > size) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int field = 0u; field < YT2_ITEMS_NAME_FIELDS; field++) {
        if (lengths[field] == 0u) {
            continue;
        }
        if (at > 0u) {
            name[at++] = ' ';
        }
        memcpy(name + at, parts[field], lengths[field]);
        at += lengths[field];
    }
    name[at] = '\0';
    return YT2_STATUS_OK;
}

int yt2_item_name(const yt2_items *items, unsigned int index, char *buffer,
                  size_t size)
{
    const uint8_t *record;

    if (items == NULL || items->bytes == NULL || buffer == NULL ||
        size == 0u || index >= YT2_ITEMS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    record = items->bytes + YT2_ITEMS_OFFSET +
             (size_t)index * YT2_ITEMS_RECORD_SIZE;
    return parse_name(record, buffer, size);
}

int yt2_items_detail_name(const yt2_items *items, unsigned int category,
                          unsigned int rank, char *buffer, size_t size)
{
    if (items == NULL || items->bytes == NULL ||
        category >= YT2_ITEMS_CATEGORIES ||
        rank >= category_table[category].count) {
        return YT2_STATUS_ERROR;
    }
    return yt2_item_name(items, category_table[category].members[rank],
                         buffer, size);
}

int yt2_items_member_file(const yt2_items *items, unsigned int category,
                          unsigned int rank, unsigned int *file)
{
    if (items == NULL || items->bytes == NULL || file == NULL ||
        category >= YT2_ITEMS_CATEGORIES ||
        rank >= category_table[category].count) {
        return YT2_STATUS_ERROR;
    }
    *file = category_table[category].members[rank];
    return YT2_STATUS_OK;
}

static int items_list_row_text(void *ctx, unsigned int rank, char *buffer,
                               size_t size)
{
    (void)ctx;
    return yt2_items_category_name(rank, buffer, size);
}

int yt2_items_render_list(yt2_surface *surface, const yt2_pictures *pictures,
                          unsigned int selected)
{
    yt2_list_page page;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        selected >= YT2_ITEMS_CATEGORIES) {
        return YT2_STATUS_ERROR;
    }
    page.header = YT2_ITEMS_HEADER;
    page.header_x = YT2_ITEMS_HEADER_X;
    page.title = NULL;
    page.title_x = 0;
    page.icons = items_icons;
    page.count = YT2_ITEMS_CATEGORIES;
    page.selected = selected;
    page.row_text = items_list_row_text;
    page.ctx = NULL;
    return yt2_list_render(surface, pictures, &page);
}

typedef struct detail_ctx {
    const yt2_items *items;
    unsigned int category;
} detail_ctx;

static int items_detail_row_text(void *ctx, unsigned int rank, char *buffer,
                                 size_t size)
{
    const detail_ctx *detail = (const detail_ctx *)ctx;

    return yt2_items_detail_name(detail->items, detail->category, rank,
                                 buffer, size);
}

int yt2_items_render_detail(yt2_surface *surface,
                            const yt2_pictures *pictures,
                            const yt2_items *items, unsigned int category,
                            unsigned int selected)
{
    yt2_list_page page;
    detail_ctx ctx;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        items == NULL || items->bytes == NULL ||
        category >= YT2_ITEMS_CATEGORIES ||
        category_table[category].members == NULL ||
        selected >= category_table[category].count) {
        return YT2_STATUS_ERROR;
    }
    ctx.items = items;
    ctx.category = category;
    page.header = category_names[category];
    page.header_x = category_header_x[category];
    page.title = YT2_ITEMS_DETAIL_TITLE;
    page.title_x = YT2_ITEMS_DETAIL_TITLE_X;
    page.icons = items_icons;
    page.count = category_table[category].count;
    page.selected = selected;
    page.row_text = items_detail_row_text;
    page.ctx = &ctx;
    if (yt2_list_render(surface, pictures, &page) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    yt2_text_draw(surface, YT2_ITEMS_DETAIL_FOOTER_X,
                  YT2_ITEMS_DETAIL_FOOTER_Y, YT2_ITEMS_DETAIL_FOOTER,
                  YT2_ITEMS_DETAIL_FOOTER_COLOR);
    return YT2_STATUS_OK;
}

static int info_frame(yt2_surface *surface, const yt2_pictures *pictures,
                      const char *left, const char *right)
{
    const yt2_picture_group *group;
    const uint8_t *image;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL) {
        return YT2_STATUS_ERROR;
    }
    group = yt2_pictures_group(pictures, INFO_PICTURE_GROUP);
    image = yt2_picture_data(pictures, INFO_PICTURE_GROUP,
                             INFO_PICTURE_INDEX);
    if (group == NULL || image == NULL) {
        return YT2_STATUS_ERROR;
    }
    yt2_surface_clear(surface, 0u);
    if (yt2_surface_blit(surface, INFO_PICTURE_X, INFO_PICTURE_Y, image,
                         group->width, group->height) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    yt2_text_draw(surface, INFO_TITLE_LEFT_X, INFO_TITLE_Y, left,
                  INFO_TITLE_COLOR);
    yt2_text_draw(surface, INFO_TITLE_RIGHT_X, INFO_TITLE_Y, right,
                  INFO_TITLE_COLOR);
    for (unsigned int index = 0u; index < YT2_LIST_ICON_COUNT; index++) {
        const uint8_t *icon =
            yt2_picture_data(pictures, YT2_LIST_ICON_GROUP,
                             items_icons[index]);

        if (icon == NULL) {
            return YT2_STATUS_ERROR;
        }
        if (yt2_surface_blit_transparent(
                surface,
                YT2_LIST_ICON_X + (int)index * YT2_LIST_ICON_STEP,
                YT2_LIST_ICON_Y, icon, YT2_LIST_ICON_SIZE,
                YT2_LIST_ICON_SIZE,
                YT2_LIST_ICON_TRANSPARENT) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    yt2_text_draw(surface, YT2_ITEMS_DETAIL_FOOTER_X,
                  YT2_ITEMS_DETAIL_FOOTER_Y, YT2_ITEMS_DETAIL_FOOTER,
                  YT2_ITEMS_DETAIL_FOOTER_COLOR);
    return YT2_STATUS_OK;
}

int yt2_items_render_attribute(yt2_surface *surface,
                               const yt2_pictures *pictures)
{
    if (info_frame(surface, pictures,
                   category_names[YT2_ITEMS_ATTRIBUTE],
                   YT2_ITEMS_HEADER) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int row = 0u;
         row < sizeof(info_rows) / sizeof(info_rows[0]); row++) {
        const uint8_t *icon;
        int y = INFO_ROW_Y + (int)row * INFO_ROW_STEP;
        int x = INFO_TEXT_X;
        const char *joiner;

        icon = yt2_picture_data(pictures, YT2_LIST_ICON_GROUP,
                                info_rows[row].icon);
        if (icon == NULL) {
            return YT2_STATUS_ERROR;
        }
        if (yt2_surface_blit_transparent(
                surface, INFO_ICON_X, y + INFO_ICON_DY, icon,
                YT2_LIST_ICON_SIZE, YT2_LIST_ICON_SIZE,
                YT2_LIST_ICON_TRANSPARENT) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        yt2_text_draw(surface, x, y, info_rows[row].keyword,
                      INFO_KEYWORD_COLOR);
        x += (int)(strlen(info_rows[row].keyword) + 1u) *
             (int)YT2_TEXT_GLYPH_WIDTH;
        yt2_text_draw(surface, x, y, "PERMANENTLY ADD", INFO_MIDDLE_COLOR);
        x += (int)(strlen("PERMANENTLY ADD") + 1u) *
             (int)YT2_TEXT_GLYPH_WIDTH;
        yt2_text_draw(surface, x, y, info_rows[row].amount,
                      INFO_AMOUNT_COLOR);
        x += (int)(strlen(info_rows[row].amount) + 1u) *
             (int)YT2_TEXT_GLYPH_WIDTH;
        joiner = info_rows[row].target[0] == 'A' ? "TO AN" : "TO A";
        yt2_text_draw(surface, x, y, joiner, INFO_MIDDLE_COLOR);
        x += (int)(strlen(joiner) + 1u) * (int)YT2_TEXT_GLYPH_WIDTH;
        yt2_text_draw(surface, x, y, info_rows[row].target,
                      INFO_TARGET_COLOR);
    }
    return YT2_STATUS_OK;
}

int yt2_items_render_transport(yt2_surface *surface,
                               const yt2_pictures *pictures)
{
    if (info_frame(surface, pictures,
                   category_names[YT2_ITEMS_TRANSPORT],
                   category_names[YT2_ITEMS_TRANSPORT]) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int row = 0u;
         row < sizeof(transport_entries) / sizeof(transport_entries[0]);
         row++) {
        int y = TRANSPORT_Y + (int)row * TRANSPORT_STEP;

        yt2_text_draw(surface, TRANSPORT_NAME_X, y,
                      transport_entries[row].name, TRANSPORT_NAME_COLOR);
        yt2_text_draw(surface, TRANSPORT_NAME_X, y + 12, "VALUE:",
                      TRANSPORT_LABEL_COLOR);
        yt2_text_draw(surface, TRANSPORT_VALUE_X, y + 12,
                      transport_entries[row].value, TRANSPORT_VALUE_COLOR);
        yt2_text_draw(surface, TRANSPORT_NAME_X + 6, y + 21, "USES:",
                      TRANSPORT_LABEL_COLOR);
        yt2_text_draw(surface, TRANSPORT_VALUE_X, y + 21,
                      transport_entries[row].uses, TRANSPORT_USES_COLOR);
        yt2_text_draw(surface, TRANSPORT_NAME_X + 6, y + 30, "TIME:",
                      TRANSPORT_LABEL_COLOR);
        if (!transport_entries[row].windowed) {
            yt2_text_draw(surface, TRANSPORT_VALUE_X, y + 30, "ANYTIME",
                          TRANSPORT_TIME_COLOR);
        } else {
            yt2_text_draw(surface, TRANSPORT_VALUE_X, y + 30, "BETWEEN",
                          TRANSPORT_NAME_COLOR);
            yt2_text_draw(surface, TRANSPORT_WRAP_X, y + 30, "7P.M.",
                          TRANSPORT_TIME_COLOR);
            yt2_text_draw(surface, TRANSPORT_WRAP_X + 36, y + 30, "AND",
                          TRANSPORT_NAME_COLOR);
            yt2_text_draw(surface, TRANSPORT_WRAP_X,
                          y + 30 + TRANSPORT_WRAP_DY, "7A.M.",
                          TRANSPORT_TIME_COLOR);
        }
    }
    return YT2_STATUS_OK;
}
