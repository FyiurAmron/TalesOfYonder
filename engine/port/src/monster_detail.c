#include "yt2/monster_detail.h"

#include <stdio.h>
#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"
#include "yt2/text.h"

#define DETAIL_STATS_SIZE 80u
#define DETAIL_NAME_SIZE 26u

/* Portrait picture groups (see the PICTURES.VGA catalog). */
#define DETAIL_GROUP2_WIDTH 140u
#define DETAIL_GROUP2_HEIGHT 155u
#define DETAIL_GROUP2_X 8
#define DETAIL_GROUP2_Y 7
#define DETAIL_GROUP3_WIDTH 190u
#define DETAIL_GROUP3_HEIGHT 110u

typedef struct detail_anim {
    uint8_t group;
    uint8_t pingpong;
    uint16_t period;
    uint8_t lut[16];
    const char *special;
} detail_anim;

/*
 * Per-monster portrait animation, by monster index (file order): the
 * picture group, the table shape over base..base+5 (0: cycle, 1:
 * ping-pong), the excursion period in ticks, the high-nibble remap,
 * and the frozen SPECIAL abilities line.  All fitted at 1.0000
 * LUT-consistency against the B1 oracle; the LUT's low nibble passes
 * through and 0xFF stays transparent.
 */
static const detail_anim detail_anims[YT2_MONSTERS_COUNT] = {
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 0 */
    { 3u, 0u, 360u, { 0, 1, 2, 5, 5, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 1 */
    { 3u, 0u, 360u, { 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 2 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 3 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 4, 7, 8, 9, 1, 11, 12, 13, 14, 15 }, "" }, /* 4 */
    { 3u, 1u, 360u, { 0, 1, 2, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "POISON" }, /* 5 */
    { 2u, 0u, 360u, { 0, 1, 2, 4, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 6 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 1, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "HEXING" }, /* 7 */
    { 3u, 0u, 360u, { 0, 1, 3, 3, 4, 5, 6, 7, 7, 9, 4, 11, 12, 13, 14, 15 }, "" }, /* 8 */
    { 2u, 0u, 360u, { 4, 5, 2, 3, 3, 5, 6, 7, 0, 9, 11, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 9 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 0, 5, 6, 7, 4, 8, 5, 11, 7, 13, 14, 15 }, "" }, /* 10 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 11 */
    { 2u, 1u, 300u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 12 */
    { 3u, 0u, 360u, { 4, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 7, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 13 */
    { 2u, 0u, 360u, { 4, 1, 2, 10, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 14 */
    { 2u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 15 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARALYZE" }, /* 16 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 17 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 18 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 19 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 20 */
    { 2u, 0u, 360u, { 4, 1, 2, 3, 4, 10, 6, 7, 0, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 21 */
    { 2u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 12, 13, 14, 15 }, "" }, /* 22 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 23 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 24 */
    { 2u, 0u, 360u, { 3, 1, 7, 3, 0, 5, 6, 7, 8, 9, 11, 11, 12, 13, 14, 15 }, "" }, /* 25 */
    { 3u, 0u, 360u, { 11, 1, 2, 0, 4, 5, 6, 7, 8, 9, 10, 10, 12, 13, 14, 15 }, "PARTY ATTACK, FROZEN" }, /* 26 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 27 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 28 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 29 */
    { 3u, 0u, 360u, { 0, 1, 2, 2, 4, 5, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 30 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 31 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 32 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "SICK" }, /* 33 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "DISEASE" }, /* 34 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 35 */
    { 3u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 36 */
    { 2u, 0u, 360u, { 0, 1, 2, 4, 0, 5, 6, 7, 0, 9, 10, 11, 5, 13, 14, 15 }, "STONING" }, /* 37 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 38 */
    { 3u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "SICK, POISON, DISEASE" }, /* 39 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK, JINXING" }, /* 40 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 41 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 42 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 43 */
    { 3u, 1u, 360u, { 3, 1, 2, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 44 */
    { 2u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 45 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 10, 12, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 46 */
    { 2u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 5, 12, 13, 14, 15 }, "STEAL GOLD" }, /* 47 */
    { 3u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 48 */
    { 3u, 0u, 360u, { 0, 1, 2, 3, 3, 5, 12, 7, 8, 9, 10, 9, 12, 13, 14, 15 }, "PARTY ATTACK, FROZEN" }, /* 49 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK, PARALYZE" }, /* 50 */
    { 2u, 1u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK, STONING" }, /* 51 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 52 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 53 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 54 */
    { 2u, 0u, 360u, { 0, 1, 2, 1, 0, 5, 6, 7, 12, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 55 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 7, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "" }, /* 56 */
    { 3u, 0u, 360u, { 0, 1, 2, 11, 4, 5, 6, 7, 8, 9, 10, 10, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 57 */
    { 2u, 0u, 360u, { 0, 0, 2, 6, 4, 5, 6, 7, 8, 9, 1, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 58 */
    { 2u, 0u, 360u, { 0, 1, 2, 3, 7, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 59 */
    { 3u, 0u, 360u, { 7, 1, 2, 5, 4, 5, 6, 7, 8, 9, 10, 8, 12, 13, 14, 15 }, "PARTY ATTACK" }, /* 60 */
};

/* The F2 command icons (same bar as the monster list). */
static const unsigned int detail_icons[YT2_LIST_ICON_COUNT] = {
    32u, 326u, 327u, 339u, 329u, 331u, 333u
};

typedef struct dword_row {
    const char *label;
    int x;
    int y;
    unsigned int offset;
} dword_row;

/* BCD big-endian counters from sub_0467C (value at x=251, gold). */
static const dword_row dword_rows[] = {
    { "EXPERIENCE:", 185, 16, 62u },
    { "GOLD:", 221, 22, 50u },
    { "MAGIC ORE:", 191, 28, 58u },
    { "NUORE:", 215, 34, 54u },
};

#define DWORD_VALUE_X 251

/* Plain word statistics from sub_046FB (value at x=275, blank if 0). */
static const dword_row word_rows[] = {
    { "HEALTH-", 233, 46, 4u },
    { "ACCURACY-", 221, 52, 8u },
    { "DEXTERITY-", 215, 58, 10u },
    { "ABSORPTION-", 209, 64, 12u },
    { "DAMAGE-", 233, 70, 14u },
    { "RANGED ACC.-", 203, 76, 24u },
    { "RANGED DAM.-", 203, 82, 26u },
};

#define WORD_VALUE_X 275

typedef struct flag_row {
    const char *label;
    int x;
    int y;
    unsigned int bit;
} flag_row;

/* IMMUNE flags from sub_047B4 (word 37; value at x=263 when set). */
static const flag_row flag_rows[] = {
    { "POISON:", 221, 94, 15u },
    { "DISEASE:", 215, 100, 14u },
    { "PARALYSIS:", 203, 106, 13u },
    { "FREEZING:", 209, 112, 12u },
    { "HEXING:", 221, 118, 11u },
    { "CURSING:", 215, 124, 10u },
    { "FIRE:", 233, 130, 3u },
    { "COLD:", 233, 136, 2u },
    { "ELECTRIC:", 209, 142, 1u },
    { "POWER:", 227, 148, 0u },
};

#define FLAG_VALUE_X 263
#define FLAG_VALUE_TEXT "IMMUNE"
#define RESISTANT_TEXT "RESISTANT"
#define MAGIC_DAMAGE_LABEL "MAGIC DAMAGE:"
#define MAGIC_DAMAGE_X 185
#define MAGIC_DAMAGE_Y 154
#define PHYSICAL_LABEL "PHYSICAL DAMAGE:"
#define PHYSICAL_X 167
#define PHYSICAL_Y 160
#define SPECIAL_LABEL "SPECIAL ATTACK-"
#define SPECIAL_Y 169
#define SPECIAL_BASE_X 171
#define SPECIAL_VALUE_DX 90

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      (uint16_t)((uint16_t)bytes[1] << 8));
}

static uint32_t read_bcd_be(const uint8_t *bytes)
{
    uint32_t value = 0u;

    for (unsigned int i = 0u; i < 4u; i++) {
        value = value * 100u +
                (uint32_t)(bytes[i] >> 4) * 10u +
                (uint32_t)(bytes[i] & 0x0Fu);
    }
    return value;
}

/* Unsigned decimal with thousands separators ("1,000,000"). */
static void format_commas(uint32_t value, char *buffer, size_t size)
{
    char digits[16];
    size_t count = 0u;
    size_t out = 0u;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    for (size_t i = count; i > 0u && out + 1u < size; i--) {
        buffer[out++] = digits[i - 1u];
        if (i > 1u && (i - 1u) % 3u == 0u && out + 1u < size) {
            buffer[out++] = ',';
        }
    }
    buffer[out] = '\0';
}

static void draw_icons(yt2_surface *surface, const yt2_pictures *pictures)
{
    for (unsigned int i = 0u; i < YT2_LIST_ICON_COUNT; i++) {
        const uint8_t *image = yt2_picture_data(pictures,
                                                YT2_LIST_ICON_GROUP,
                                                detail_icons[i]);

        if (image == NULL) {
            continue;
        }
        yt2_surface_blit_transparent(surface,
                                    YT2_LIST_ICON_X +
                                        (int)(i * YT2_LIST_ICON_STEP),
                                    YT2_LIST_ICON_Y, image,
                                    YT2_LIST_ICON_SIZE, YT2_LIST_ICON_SIZE,
                                    YT2_LIST_ICON_TRANSPARENT);
    }
}

static void draw_portrait(yt2_surface *surface, const uint8_t *image,
                          unsigned int width, unsigned int height, int x,
                          int y, const uint8_t *lut)
{
    for (unsigned int row = 0u; row < height; row++) {
        int fy = y + (int)row;

        if (fy < 0 || fy >= (int)surface->height) {
            continue;
        }
        for (unsigned int col = 0u; col < width; col++) {
            int fx = x + (int)col;
            uint8_t pixel = image[row * width + col];

            if (pixel == YT2_DETAIL_PORTRAIT_TRANSPARENT) {
                continue;
            }
            if (fx < 0 || fx >= (int)surface->width) {
                continue;
            }
            surface->pixels[(size_t)fy * surface->stride + (size_t)fx] =
                (uint8_t)(lut[pixel >> 4] << 4 | (pixel & 0x0Fu));
        }
    }
}

int yt2_monster_detail_render(yt2_surface *surface,
                             const yt2_pictures *pictures,
                             const yt2_monsters *monsters, unsigned int rank,
                             unsigned int pic)
{
    const yt2_picture_group *background;
    const yt2_picture_group *portraits;
    const uint8_t *image;
    const uint8_t *record;
    const uint8_t *stats;
    const detail_anim *anim;
    unsigned int index;
    char name[64];
    char value[16];

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        monsters == NULL || rank >= YT2_MONSTERS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    index = monsters->order[rank];
    if (index >= YT2_MONSTERS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    anim = &detail_anims[index];
    portraits = yt2_pictures_group(pictures, anim->group);
    if (portraits == NULL || pic >= portraits->count) {
        return YT2_STATUS_ERROR;
    }
    record = yt2_monster_record(monsters, rank);
    if (record == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (yt2_monster_name(monsters, rank, name, sizeof(name)) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    background = yt2_pictures_group(pictures, YT2_DETAIL_PICTURE_GROUP);
    image = yt2_picture_data(pictures, YT2_DETAIL_PICTURE_GROUP,
                             YT2_DETAIL_PICTURE_INDEX);
    if (background == NULL || image == NULL) {
        return YT2_STATUS_ERROR;
    }
    stats = record + DETAIL_NAME_SIZE;

    yt2_surface_clear(surface, 0u);
    if (yt2_surface_blit(surface, YT2_DETAIL_PICTURE_X,
                         YT2_DETAIL_PICTURE_Y, image, background->width,
                         background->height) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    yt2_text_draw(surface, YT2_DETAIL_NAME_X, YT2_DETAIL_TITLE_Y, name,
                  YT2_DETAIL_TITLE_COLOR);
    yt2_text_draw(surface, YT2_MONSTERS_HEADER_X, YT2_DETAIL_TITLE_Y,
                  YT2_MONSTERS_HEADER, YT2_DETAIL_TITLE_COLOR);
    yt2_text_draw(surface, YT2_DETAIL_FOOTER_X, YT2_DETAIL_FOOTER_Y,
                  YT2_DETAIL_FOOTER_TEXT, YT2_DETAIL_FOOTER_COLOR);
    draw_icons(surface, pictures);

    for (unsigned int i = 0u;
         i < sizeof(dword_rows) / sizeof(dword_rows[0]); i++) {
        uint32_t counter =
            read_bcd_be(stats + dword_rows[i].offset);

        yt2_text_draw(surface, dword_rows[i].x, dword_rows[i].y,
                      dword_rows[i].label, YT2_DETAIL_LABEL_COLOR);
        if (counter != 0u) {
            format_commas(counter, value, sizeof(value));
            yt2_text_draw(surface, DWORD_VALUE_X, dword_rows[i].y, value,
                          YT2_DETAIL_DWORD_COLOR);
        }
    }
    for (unsigned int i = 0u;
         i < sizeof(word_rows) / sizeof(word_rows[0]); i++) {
        uint16_t number = read_u16(stats + word_rows[i].offset);

        yt2_text_draw(surface, word_rows[i].x, word_rows[i].y,
                      word_rows[i].label, YT2_DETAIL_LABEL_COLOR);
        if (number != 0u) {
            snprintf(value, sizeof(value), "%u", number);
            yt2_text_draw(surface, WORD_VALUE_X, word_rows[i].y, value,
                          YT2_DETAIL_WORD_COLOR);
        }
    }
    {
        uint16_t immune = read_u16(stats + 74u);
        uint16_t resist = read_u16(stats + 76u);

        for (unsigned int i = 0u;
             i < sizeof(flag_rows) / sizeof(flag_rows[0]); i++) {
            yt2_text_draw(surface, flag_rows[i].x, flag_rows[i].y,
                          flag_rows[i].label, YT2_DETAIL_LABEL_COLOR);
            if (immune & (uint16_t)(1u << flag_rows[i].bit)) {
                yt2_text_draw(surface, FLAG_VALUE_X, flag_rows[i].y,
                              FLAG_VALUE_TEXT, YT2_DETAIL_FLAG_COLOR);
            }
        }
        yt2_text_draw(surface, MAGIC_DAMAGE_X, MAGIC_DAMAGE_Y,
                      MAGIC_DAMAGE_LABEL, YT2_DETAIL_LABEL_COLOR);
        if ((immune & 0x10u) || (resist & 0x3A00u)) {
            yt2_text_draw(surface, FLAG_VALUE_X, MAGIC_DAMAGE_Y,
                          RESISTANT_TEXT, YT2_DETAIL_FLAG_COLOR);
        }
        yt2_text_draw(surface, PHYSICAL_X, PHYSICAL_Y, PHYSICAL_LABEL,
                      YT2_DETAIL_LABEL_COLOR);
        if (resist & 0xC000u) {
            yt2_text_draw(surface, FLAG_VALUE_X, PHYSICAL_Y,
                          RESISTANT_TEXT, YT2_DETAIL_FLAG_COLOR);
        }
    }
    {
        size_t length = strlen(anim->special);
        int x = SPECIAL_BASE_X;

        if (length > 8u) {
            x = (int)(35u - length) * 6;
        }
        yt2_text_draw(surface, x, SPECIAL_Y, SPECIAL_LABEL,
                      YT2_DETAIL_LABEL_COLOR);
        if (length != 0u) {
            yt2_text_draw(surface, x + SPECIAL_VALUE_DX, SPECIAL_Y,
                          anim->special, YT2_DETAIL_SPECIAL_COLOR);
        }
    }

    image = yt2_picture_data(pictures, anim->group, pic);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (anim->group == 2u) {
        draw_portrait(surface, image, DETAIL_GROUP2_WIDTH,
                      DETAIL_GROUP2_HEIGHT, DETAIL_GROUP2_X,
                      DETAIL_GROUP2_Y, anim->lut);
    } else {
        draw_portrait(surface, image, DETAIL_GROUP3_WIDTH,
                      DETAIL_GROUP3_HEIGHT, YT2_DETAIL_PORTRAIT_X,
                      YT2_DETAIL_PORTRAIT_Y, anim->lut);
    }
    return YT2_STATUS_OK;
}

unsigned int yt2_monster_detail_phase(const yt2_monsters *monsters,
                                     unsigned int rank, unsigned int tick)
{
    static const unsigned int pingpong[10] = {
        0u, 1u, 2u, 3u, 4u, 5u, 4u, 3u, 2u, 1u
    };
    const detail_anim *anim;
    const uint8_t *record;
    unsigned int base;
    unsigned int phase;

    if (monsters == NULL || rank >= YT2_MONSTERS_COUNT) {
        return 0u;
    }
    record = yt2_monster_record(monsters, rank);
    if (record == NULL) {
        return 0u;
    }
    anim = &detail_anims[monsters->order[rank]];
    base = read_u16(record + DETAIL_NAME_SIZE);
    phase = tick % (unsigned int)(anim->period + YT2_DETAIL_EXCURSION_LENGTH);
    if (phase >= anim->period) {
        if (phase - anim->period < YT2_DETAIL_STEP_TICKS) {
            return base + 7u;
        }
        return base + 8u;
    }
    if (anim->pingpong) {
        return base + pingpong[phase / YT2_DETAIL_STEP_TICKS % 10u];
    }
    return base + phase / YT2_DETAIL_STEP_TICKS % 6u;
}
