#include "yt2/world.h"

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
    unsigned int map;
    unsigned int index;

    if (world == NULL || world->tiles == NULL || map_x >= YT2_WORLD_MAPS_X ||
        map_y >= YT2_WORLD_MAPS_Y || tile_x >= YT2_WORLD_MAP_TILES_X ||
        tile_y >= YT2_WORLD_MAP_TILES_Y) {
        return NULL;
    }
    map = map_y * YT2_WORLD_MAPS_X + map_x;
    index = map * YT2_WORLD_MAP_TILES_X * YT2_WORLD_MAP_TILES_Y +
            tile_y * YT2_WORLD_MAP_TILES_X + tile_x;
    return &world->tiles[index];
}

const char *yt2_world_map_name(const yt2_world *world, const yt2_map *map)
{
    if (world == NULL || map == NULL ||
        map->name_index >= YT2_WORLD_MAP_NAMES_COUNT) {
        return "";
    }
    return world->names[map->name_index];
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
