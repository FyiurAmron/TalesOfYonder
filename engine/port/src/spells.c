#include "yt2/spells.h"

#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"

/*
 * Rank to file-record table, template-matched rank by rank against the B1
 * spell-list frames (105 Down steps from the F3 screen).  Record 87 (BLOCK
 * OF POWER) sits at ranks 5 and 11; record 86 (BEAM OF DEATH) is absent.
 */
static const uint8_t spell_order[YT2_SPELLS_COUNT] = {
    48u, 43u, 50u, 55u, 67u, 87u, 32u, 35u, 80u, 71u, 59u, 87u, 92u, 3u,
    6u, 37u, 38u, 7u, 81u, 88u, 26u, 44u, 16u, 29u, 45u, 102u, 11u, 103u,
    60u, 12u, 8u, 9u, 64u, 46u, 93u, 90u, 39u, 65u, 76u, 61u, 68u, 72u,
    97u, 98u, 0u, 17u, 30u, 51u, 27u, 13u, 56u, 10u, 14u, 49u, 52u, 73u,
    83u, 94u, 74u, 53u, 36u, 1u, 28u, 99u, 5u, 20u, 4u, 100u, 84u, 47u,
    62u, 24u, 54u, 89u, 15u, 40u, 82u, 69u, 63u, 95u, 57u, 18u, 101u, 41u,
    96u, 31u, 21u, 70u, 22u, 66u, 23u, 33u, 2u, 58u, 42u, 77u, 79u, 25u,
    75u, 85u, 104u, 19u, 34u, 78u, 91u
};

static const unsigned int spell_icons[YT2_LIST_ICON_COUNT] = {
    32u, 325u, 328u, 339u, 329u, 331u, 333u
};

static int parse_name(const uint8_t *record, char *name, size_t size)
{
    size_t length = YT2_SPELLS_NAME_SIZE;

    if (record[YT2_SPELLS_NAME_SIZE] != 0u) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int i = 0u; i < YT2_SPELLS_NAME_SIZE; i++) {
        if (record[i] < 0x20u || record[i] > 0x7Eu) {
            return YT2_STATUS_ERROR;
        }
    }
    while (length > 0u && record[length - 1u] == ' ') {
        length--;
    }
    if (length + 1u > size) {
        return YT2_STATUS_ERROR;
    }
    memcpy(name, record, length);
    name[length] = '\0';
    return YT2_STATUS_OK;
}

int yt2_spells_load(yt2_spells *spells, const yt2_world *world)
{
    char name[YT2_SPELLS_NAME_SIZE + 1u];

    if (spells == NULL || world == NULL || world->bytes == NULL ||
        world->size < YT2_SPELLS_OFFSET +
                          YT2_SPELLS_COUNT * YT2_SPELLS_RECORD_SIZE) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int i = 0u; i < YT2_SPELLS_COUNT; i++) {
        const uint8_t *record = world->bytes + YT2_SPELLS_OFFSET +
                                (size_t)i * YT2_SPELLS_RECORD_SIZE;

        if (parse_name(record, name, sizeof(name)) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    memcpy(spells->order, spell_order, sizeof(spells->order));
    spells->bytes = world->bytes;
    spells->size = world->size;
    return YT2_STATUS_OK;
}

int yt2_spell_name(const yt2_spells *spells, unsigned int rank, char *buffer,
                   size_t size)
{
    const uint8_t *record;

    if (spells == NULL || spells->bytes == NULL || buffer == NULL ||
        size == 0u || rank >= YT2_SPELLS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    record = spells->bytes + YT2_SPELLS_OFFSET +
             (size_t)spells->order[rank] * YT2_SPELLS_RECORD_SIZE;
    return parse_name(record, buffer, size);
}

int yt2_spell_name_by_index(const yt2_spells *spells, unsigned int index,
                            char *buffer, size_t size)
{
    const uint8_t *record;

    if (spells == NULL || spells->bytes == NULL || buffer == NULL ||
        size == 0u || index >= YT2_SPELLS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    record = spells->bytes + YT2_SPELLS_OFFSET +
             (size_t)index * YT2_SPELLS_RECORD_SIZE;
    return parse_name(record, buffer, size);
}

const uint8_t *yt2_spell_record(const yt2_spells *spells, unsigned int rank)
{
    if (spells == NULL || spells->bytes == NULL ||
        rank >= YT2_SPELLS_COUNT) {
        return NULL;
    }
    return spells->bytes + YT2_SPELLS_OFFSET +
           (size_t)spells->order[rank] * YT2_SPELLS_RECORD_SIZE;
}

static uint16_t read_u16(const uint8_t *at)
{
    return (uint16_t)((uint16_t)at[0] | ((uint16_t)at[1] << 8));
}

int yt2_spells_effect_lines(const yt2_spells *spells, unsigned int file_index,
                            char lines[YT2_SPELLS_EFFECT_MAX_LINES]
                                       [YT2_SPELLS_EFFECT_REC],
                            unsigned int *count)
{
    size_t slot;
    unsigned int start;
    unsigned int total;

    if (spells == NULL || spells->bytes == NULL || lines == NULL ||
        count == NULL || file_index >= YT2_SPELLS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    slot = (size_t)YT2_SPELLS_EFFECT_INDEX +
           (size_t)(file_index + 1u) * 4u;
    if (slot + 4u > spells->size) {
        return YT2_STATUS_ERROR;
    }
    start = read_u16(spells->bytes + slot);
    total = read_u16(spells->bytes + slot + 2u);
    if (total > YT2_SPELLS_EFFECT_MAX_LINES) {
        total = YT2_SPELLS_EFFECT_MAX_LINES;
    }
    for (unsigned int i = 0u; i < total; i++) {
        size_t record = (size_t)YT2_SPELLS_EFFECT_TEXT +
                        (size_t)(start + i) * YT2_SPELLS_EFFECT_REC;
        size_t length = 0u;

        if (record + YT2_SPELLS_EFFECT_REC > spells->size) {
            return YT2_STATUS_ERROR;
        }
        while (length + 1u < YT2_SPELLS_EFFECT_REC &&
               spells->bytes[record + length] != 0u) {
            lines[i][length] = (char)spells->bytes[record + length];
            length++;
        }
        lines[i][length] = '\0';
    }
    *count = total;
    return YT2_STATUS_OK;
}

static int spell_row_text(void *ctx, unsigned int rank, char *buffer,
                          size_t size)
{
    const yt2_spells *spells = (const yt2_spells *)ctx;

    return yt2_spell_name(spells, rank, buffer, size);
}

int yt2_spells_render(yt2_surface *surface, const yt2_pictures *pictures,
                      const yt2_spells *spells, unsigned int selected)
{
    yt2_list_page page;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        spells == NULL || spells->bytes == NULL ||
        selected >= YT2_SPELLS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    page.header = YT2_SPELLS_HEADER;
    page.title = NULL;
    page.title_x = 0;
    page.header_x = YT2_SPELLS_HEADER_X;
    page.icons = spell_icons;
    page.count = YT2_SPELLS_COUNT;
    page.selected = selected;
    page.row_text = spell_row_text;
    page.ctx = (void *)spells;
    return yt2_list_render(surface, pictures, &page);
}
