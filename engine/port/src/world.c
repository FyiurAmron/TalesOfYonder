#include "yt2/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt2/file.h"

static void parse_tiles(const uint8_t *bytes, yt2_tile *tiles)
{
    for (unsigned int index = 0; index < YT2_WORLD_TILES_COUNT; index++) {
        const uint8_t *source = bytes + index * YT2_WORLD_TILE_SIZE;

        tiles[index].terrain = source[0];
        tiles[index].flags = source[1];
        tiles[index].object = source[2];
        tiles[index].reserved = source[3];
    }
}

static void parse_maps(const uint8_t *bytes, yt2_map *maps)
{
    for (unsigned int index = 0; index < YT2_WORLD_MAPS_COUNT; index++) {
        const uint8_t *source = bytes + index * YT2_WORLD_MAP_SIZE;

        memcpy(maps[index].suffix, source, YT2_WORLD_MAP_SUFFIX_LENGTH);
        maps[index].suffix[YT2_WORLD_MAP_SUFFIX_LENGTH] = '\0';
        maps[index].name_index = source[YT2_WORLD_MAP_SUFFIX_LENGTH];
        maps[index].type = source[YT2_WORLD_MAP_SUFFIX_LENGTH + 1u];
    }
}

static void parse_names(const uint8_t *bytes, char *names)
{
    for (unsigned int index = 0; index < YT2_WORLD_MAP_NAMES_COUNT; index++) {
        char *name = names + index * (YT2_WORLD_MAP_NAME_LENGTH + 1u);

        memcpy(name, bytes + index * YT2_WORLD_MAP_NAME_LENGTH,
               YT2_WORLD_MAP_NAME_LENGTH);
        name[YT2_WORLD_MAP_NAME_LENGTH] = '\0';
    }
}

static void parse_palettes(const uint8_t *bytes, yt2_palette *palettes)
{
    for (unsigned int index = 0; index < YT2_WORLD_PALETTES_COUNT; index++) {
        memcpy(palettes[index].rgb,
               bytes + YT2_WORLD_PALETTES_OFFSET + index * YT2_PALETTE_SIZE,
               YT2_PALETTE_SIZE);
    }
}

int yt2_world_parse(const uint8_t *bytes, size_t size, yt2_world *world)
{
    const uint8_t *tiles;
    const uint8_t *maps;
    const uint8_t *names;
    uint8_t *image;

    if (bytes == NULL || world == NULL || size < YT2_WORLD_DATA_SIZE) {
        return YT2_STATUS_ERROR;
    }
    memset(world, 0, sizeof(*world));
    image = malloc(size);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    memcpy(image, bytes, size);
    world->bytes = image;
    world->size = size;

    world->tiles = malloc(YT2_WORLD_TILES_COUNT * sizeof(*world->tiles));
    if (world->tiles == NULL) {
        yt2_world_free(world);
        return YT2_STATUS_ERROR;
    }

    tiles = world->bytes;
    maps = tiles + YT2_WORLD_TILES_COUNT * YT2_WORLD_TILE_SIZE;
    names = maps + YT2_WORLD_MAPS_COUNT * YT2_WORLD_MAP_SIZE;

    parse_tiles(tiles, world->tiles);
    parse_maps(maps, world->maps);
    parse_names(names, world->names[0]);
    parse_palettes(world->bytes, world->palettes);
    return YT2_STATUS_OK;
}

int yt2_world_load(const char *path, yt2_world *world)
{
    uint8_t *bytes;
    size_t size;
    yt2_status status;

    if (path == NULL || world == NULL) {
        return YT2_STATUS_ERROR;
    }
    status = yt2_file_load(path, &bytes, &size);
    if (status != YT2_STATUS_OK) {
        return status;
    }
    status = yt2_world_parse(bytes, size, world);
    yt2_file_free(bytes);
    return status;
}

void yt2_world_free(yt2_world *world)
{
    if (world == NULL) {
        return;
    }
    free(world->tiles);
    yt2_file_free(world->bytes);
    memset(world, 0, sizeof(*world));
}

const yt2_map *yt2_world_map(const yt2_world *world, unsigned int map)
{
    if (world == NULL || world->tiles == NULL || map >= YT2_WORLD_MAPS_COUNT) {
        return NULL;
    }
    return &world->maps[map];
}

const yt2_tile *yt2_world_tile(const yt2_world *world, unsigned int map_x,
                               unsigned int map_y, unsigned int tile_x,
                               unsigned int tile_y)
{
    unsigned int row;
    unsigned int column;

    if (world == NULL || world->tiles == NULL || map_x >= YT2_WORLD_MAPS_X ||
        map_y >= YT2_WORLD_MAPS_Y || tile_x >= YT2_WORLD_MAP_TILES_X ||
        tile_y >= YT2_WORLD_MAP_TILES_Y) {
        return NULL;
    }
    row = map_y * YT2_WORLD_MAP_TILES_Y + tile_y;
    column = map_x * YT2_WORLD_MAP_TILES_X + tile_x;
    return &world->tiles[row * YT2_WORLD_TILES_X + column];
}

const char *yt2_world_map_name(const yt2_world *world, const yt2_map *map)
{
    if (world == NULL || map == NULL ||
        map->name_index >= YT2_WORLD_MAP_NAMES_COUNT) {
        return "";
    }
    return world->names[map->name_index];
}

int yt2_world_map_display_name(const yt2_world *world, unsigned int map,
                               char *buffer, size_t size)
{
    const yt2_map *descriptor;
    const char *name;
    int written;

    if (world == NULL || buffer == NULL || size == 0u ||
        map >= YT2_WORLD_MAPS_COUNT) {
        return YT2_STATUS_ERROR;
    }
    descriptor = &world->maps[map];
    name = yt2_world_map_name(world, descriptor);
    if (name[0] == '\0') {
        return YT2_STATUS_ERROR;
    }

    {
        char trimmed[YT2_WORLD_MAP_NAME_LENGTH + 1u];
        size_t length = strlen(name);

        while (length > 0u && name[length - 1u] == ' ') {
            length--;
        }
        memcpy(trimmed, name, length);
        trimmed[length] = '\0';

        if (descriptor->suffix[0] == '0' && descriptor->suffix[1] == ' ') {
            written = snprintf(buffer, size, "%s", trimmed);
        } else if (descriptor->suffix[0] == '0') {
            written = snprintf(buffer, size, "%s MAP %c", trimmed,
                               descriptor->suffix[1]);
        } else {
            written = snprintf(buffer, size, "%s LEVEL %c", trimmed,
                               descriptor->suffix[0]);
        }
    }
    if (written < 0 || (size_t)written >= size) {
        return YT2_STATUS_ERROR;
    }
    return YT2_STATUS_OK;
}

int yt2_world_map_tile_visible(const yt2_world *world, unsigned int map,
                               unsigned int tile_x, unsigned int tile_y)
{
    unsigned int map_x;
    unsigned int map_y;
    unsigned int row;
    unsigned int byte;
    unsigned int mask;

    if (world == NULL || world->bytes == NULL || map >= YT2_WORLD_MAPS_COUNT ||
        tile_x >= YT2_WORLD_MAP_TILES_X || tile_y >= YT2_WORLD_MAP_TILES_Y ||
        world->size < YT2_WORLD_MAP_BITS_OFFSET + YT2_WORLD_MAP_BITS_SIZE) {
        return 0;
    }
    map_x = map % YT2_WORLD_MAPS_X;
    map_y = map / YT2_WORLD_MAPS_X;
    row = map_y * YT2_WORLD_MAP_TILES_Y + tile_y;
    byte = YT2_WORLD_MAP_BITS_OFFSET +
           row * YT2_WORLD_MAP_BITS_ROW_BYTES +
           map_x * (YT2_WORLD_MAP_TILES_X / 8u) + tile_x / 8u;
    mask = 0x80u >> (tile_x % 8u);
    return (world->bytes[byte] & mask) != 0u;
}

int yt2_world_legend_at(const yt2_world *world, unsigned int index,
                        yt2_legend *legend)
{
    const uint8_t *entry;

    if (world == NULL || world->bytes == NULL || legend == NULL ||
        index >= YT2_WORLD_LEGEND_COUNT ||
        world->size <
            YT2_WORLD_LEGEND_OFFSET +
                YT2_WORLD_LEGEND_COUNT * YT2_WORLD_LEGEND_ENTRY_SIZE) {
        return YT2_STATUS_ERROR;
    }
    entry = world->bytes + YT2_WORLD_LEGEND_OFFSET +
            index * YT2_WORLD_LEGEND_ENTRY_SIZE;
    legend->map = (uint16_t)(entry[0] | (entry[1] << 8));
    legend->x = (uint16_t)(entry[2] | (entry[3] << 8));
    legend->y = (uint16_t)(entry[4] | (entry[5] << 8));
    legend->line = (uint16_t)(entry[6] | (entry[7] << 8));
    return YT2_STATUS_OK;
}

int yt2_world_legend_text(const yt2_world *world, unsigned int line,
                          char *text, size_t size)
{
    const uint8_t *entry;
    size_t offset;
    size_t i = 0u;

    if (world == NULL || world->bytes == NULL || text == NULL ||
        size < YT2_WORLD_LEGEND_TEXT_SIZE + 1u ||
        line >= YT2_WORLD_LEGEND_TEXT_COUNT) {
        return YT2_STATUS_ERROR;
    }
    offset = YT2_WORLD_LEGEND_TEXT_OFFSET +
             (size_t)line * YT2_WORLD_LEGEND_TEXT_LINE_SIZE;
    if (world->size < offset + YT2_WORLD_LEGEND_TEXT_SIZE) {
        return YT2_STATUS_ERROR;
    }
    entry = world->bytes + offset;
    while (i < YT2_WORLD_LEGEND_TEXT_SIZE && entry[i] != 0u) {
        text[i] = (char)entry[i];
        i++;
    }
    while (i < YT2_WORLD_LEGEND_TEXT_SIZE) {
        text[i] = ' ';
        i++;
    }
    text[YT2_WORLD_LEGEND_TEXT_SIZE] = '\0';
    return YT2_STATUS_OK;
}

const yt2_palette *yt2_world_palette(const yt2_world *world,
                                     unsigned int palette)
{
    if (world == NULL || world->tiles == NULL ||
        palette >= YT2_WORLD_PALETTES_COUNT) {
        return NULL;
    }
    return &world->palettes[palette];
}

int yt2_world_anim_base(const yt2_world *world, uint8_t *base)
{
    if (world == NULL || world->bytes == NULL || base == NULL ||
        world->size < YT2_WORLD_ANIM_OFFSET + YT2_WORLD_ANIM_SIZE) {
        return YT2_STATUS_ERROR;
    }
    memcpy(base, world->bytes + YT2_WORLD_ANIM_OFFSET, YT2_WORLD_ANIM_SIZE);
    return YT2_STATUS_OK;
}
