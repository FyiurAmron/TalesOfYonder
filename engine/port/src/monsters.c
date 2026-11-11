#include "yt2/monsters.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"

#define MONSTER_NAME_LENGTH                                               \
    (YT2_MONSTERS_NAME_FIELDS * (YT2_MONSTERS_NAME_FIELD_SIZE - 1u) + 1u)

typedef struct monster_entry {
    char name[MONSTER_NAME_LENGTH + 1u];
    uint8_t index;
} monster_entry;

static const unsigned int monster_icons[YT2_LIST_ICON_COUNT] = {
    32u, 326u, 327u, 339u, 329u, 331u, 333u
};

static int compare_entries(const void *a, const void *b)
{
    const monster_entry *left = (const monster_entry *)a;
    const monster_entry *right = (const monster_entry *)b;
    int order = strcmp(left->name, right->name);

    if (order != 0) {
        return order;
    }
    return (int)left->index - (int)right->index;
}

static int parse_name(const uint8_t *record, char *name, size_t size)
{
    char parts[YT2_MONSTERS_NAME_FIELDS]
               [YT2_MONSTERS_NAME_FIELD_SIZE];
    size_t lengths[YT2_MONSTERS_NAME_FIELDS];
    size_t total = 0u;

    for (unsigned int field = 0u; field < YT2_MONSTERS_NAME_FIELDS; field++) {
        const uint8_t *bytes = record + field * YT2_MONSTERS_NAME_FIELD_SIZE;

        if (bytes[YT2_MONSTERS_NAME_FIELD_SIZE - 1u] != 0u) {
            return YT2_STATUS_ERROR;
        }
        memcpy(parts[field], bytes, YT2_MONSTERS_NAME_FIELD_SIZE);
        lengths[field] = strlen(parts[field]);
        while (lengths[field] > 0u &&
               parts[field][lengths[field] - 1u] == ' ') {
            lengths[field]--;
        }
        parts[field][lengths[field]] = '\0';
        total += lengths[field] + 1u;
    }
    if (total > size) {
        return YT2_STATUS_ERROR;
    }
    {
        int written = snprintf(name, size, "%s %s", parts[0], parts[1]);

        if (written < 0 || (size_t)written >= size) {
            return YT2_STATUS_ERROR;
        }
        /* Collapse a missing half ("NAME " or " NAME") to the bare word. */
        if (lengths[0] == 0u) {
            memmove(name, name + 1u, strlen(name));
        } else if (lengths[1] == 0u) {
            name[lengths[0]] = '\0';
        }
    }
    return YT2_STATUS_OK;
}

int yt2_monsters_load(yt2_monsters *monsters, const yt2_world *world)
{
    monster_entry entries[YT2_MONSTERS_COUNT];

    if (monsters == NULL || world == NULL || world->bytes == NULL ||
        world->size < YT2_MONSTERS_OFFSET +
                          YT2_MONSTERS_COUNT * YT2_MONSTERS_RECORD_SIZE) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int i = 0u; i < YT2_MONSTERS_COUNT; i++) {
        const uint8_t *record = world->bytes + YT2_MONSTERS_OFFSET +
                                (size_t)i * YT2_MONSTERS_RECORD_SIZE;

        if (parse_name(record, entries[i].name, sizeof(entries[i].name)) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        entries[i].index = (uint8_t)i;
    }
    qsort(entries, YT2_MONSTERS_COUNT, sizeof(entries[0]), compare_entries);
    for (unsigned int rank = 0u; rank < YT2_MONSTERS_COUNT; rank++) {
        monsters->order[rank] = entries[rank].index;
    }
    monsters->bytes = world->bytes;
    monsters->size = world->size;
    return YT2_STATUS_OK;
}

int yt2_monster_name(const yt2_monsters *monsters, unsigned int rank,
                     char *buffer, size_t size)
{
    const uint8_t *record;

    if (monsters == NULL || monsters->bytes == NULL || buffer == NULL ||
        size == 0u || rank >= YT2_MONSTERS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    record = monsters->bytes + YT2_MONSTERS_OFFSET +
             (size_t)monsters->order[rank] * YT2_MONSTERS_RECORD_SIZE;
    return parse_name(record, buffer, size);
}

const uint8_t *yt2_monster_record(const yt2_monsters *monsters,
                                  unsigned int rank)
{
    if (monsters == NULL || monsters->bytes == NULL ||
        rank >= YT2_MONSTERS_COUNT) {
        return NULL;
    }
    return monsters->bytes + YT2_MONSTERS_OFFSET +
           (size_t)monsters->order[rank] * YT2_MONSTERS_RECORD_SIZE;
}

static int monster_row_text(void *ctx, unsigned int rank, char *buffer,
                            size_t size)
{
    const yt2_monsters *monsters = (const yt2_monsters *)ctx;

    return yt2_monster_name(monsters, rank, buffer, size);
}

int yt2_monsters_render(yt2_surface *surface, const yt2_pictures *pictures,
                        const yt2_monsters *monsters, unsigned int selected)
{
    yt2_list_page page;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        monsters == NULL || monsters->bytes == NULL ||
        selected >= YT2_MONSTERS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    page.header = YT2_MONSTERS_HEADER;
    page.title = NULL;
    page.title_x = 0;
    page.header_x = YT2_MONSTERS_HEADER_X;
    page.icons = monster_icons;
    page.count = YT2_MONSTERS_COUNT;
    page.selected = selected;
    page.row_text = monster_row_text;
    page.ctx = (void *)monsters;
    return yt2_list_render(surface, pictures, &page);
}
