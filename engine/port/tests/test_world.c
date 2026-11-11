#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/world.h"

static const uint64_t palette_checksums[YT2_WORLD_PALETTES_COUNT] = {
    UINT64_C(0xE3275EE3AE459FB5), UINT64_C(0x4AEEB75435000A7E),
    UINT64_C(0x903FD080512C74E3), UINT64_C(0x099DCAA87208CCCD),
    UINT64_C(0xE83742FE3AE6C2A1), UINT64_C(0x01383C2163524986),
    UINT64_C(0xCBF680A62B81BCF3), UINT64_C(0xFCA5C34E61383CAE),
    UINT64_C(0x089875EBAB4D4C6F)
};

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

static void set_tile(uint8_t *bytes, unsigned int index, uint8_t terrain,
                     uint8_t flags, uint8_t object, uint8_t reserved)
{
    bytes[index * YT2_WORLD_TILE_SIZE] = terrain;
    bytes[index * YT2_WORLD_TILE_SIZE + 1u] = flags;
    bytes[index * YT2_WORLD_TILE_SIZE + 2u] = object;
    bytes[index * YT2_WORLD_TILE_SIZE + 3u] = reserved;
}

static void set_map(uint8_t *bytes, unsigned int index, const char *suffix,
                    uint8_t name_index, uint8_t type)
{
    uint8_t *maps = bytes + YT2_WORLD_TILES_COUNT * YT2_WORLD_TILE_SIZE;
    uint8_t *map = maps + index * YT2_WORLD_MAP_SIZE;

    memcpy(map, suffix, YT2_WORLD_MAP_SUFFIX_LENGTH);
    map[YT2_WORLD_MAP_SUFFIX_LENGTH] = name_index;
    map[YT2_WORLD_MAP_SUFFIX_LENGTH + 1u] = type;
}

static void set_name(uint8_t *bytes, unsigned int index, const char *name)
{
    size_t length = strlen(name);
    uint8_t *names = bytes + YT2_WORLD_TILES_COUNT * YT2_WORLD_TILE_SIZE +
                     YT2_WORLD_MAPS_COUNT * YT2_WORLD_MAP_SIZE;
    uint8_t *slot = names + index * YT2_WORLD_MAP_NAME_LENGTH;

    memset(slot, ' ', YT2_WORLD_MAP_NAME_LENGTH);
    if (length > YT2_WORLD_MAP_NAME_LENGTH) {
        length = YT2_WORLD_MAP_NAME_LENGTH;
    }
    memcpy(slot, name, length);
}

static int test_synthetic(void)
{
    uint8_t *bytes = malloc(YT2_WORLD_DATA_SIZE);
    yt2_world world;
    const yt2_tile *tile;
    const yt2_map *map;
    const yt2_palette *palette;
    yt2_map invalid;

    CHECK(bytes != NULL);
    if (bytes == NULL) {
        return 0;
    }
    memset(bytes, 0, YT2_WORLD_DATA_SIZE);

    set_tile(bytes, 0u, 1u, 2u, 3u, 4u);
    set_tile(bytes, 1u, 5u, 6u, 7u, 8u);
    set_tile(bytes, 21u * 960u, 9u, 10u, 11u, 12u);
    set_tile(bytes, YT2_WORLD_TILES_COUNT - 1u, 13u, 14u, 15u, 16u);
    set_map(bytes, 0u, "0   ", 0u, 0u);
    set_map(bytes, 21u, "01  ", 1u, 1u);
    set_map(bytes, YT2_WORLD_MAPS_COUNT - 1u, "09  ", 69u, 2u);
    set_name(bytes, 0u, "PORT HOPE");
    set_name(bytes, 1u, "YENDOR-PORT HOPE");
    set_name(bytes, 69u, "LAST MAP DESCRIPTION");
    bytes[YT2_WORLD_PALETTES_OFFSET] = 1u;
    bytes[YT2_WORLD_PALETTES_OFFSET + 1u] = 2u;
    bytes[YT2_WORLD_PALETTES_OFFSET + 2u] = 3u;
    bytes[YT2_WORLD_PALETTES_OFFSET + YT2_PALETTE_SIZE] = 4u;

    CHECK(yt2_world_parse(bytes, YT2_WORLD_DATA_SIZE, &world) ==
          YT2_STATUS_OK);
    CHECK(world.size == YT2_WORLD_DATA_SIZE);
    CHECK(world.bytes != NULL && world.tiles != NULL);

    tile = yt2_world_tile(&world, 0u, 0u, 0u, 0u);
    CHECK(tile != NULL);
    if (tile != NULL) {
        CHECK(tile->terrain == 1u && tile->flags == 2u);
        CHECK(tile->object == 3u && tile->reserved == 4u);
    }
    tile = yt2_world_tile(&world, 0u, 0u, 1u, 0u);
    CHECK(tile != NULL);
    if (tile != NULL) {
        CHECK(tile->terrain == 5u && tile->object == 7u);
    }
    tile = yt2_world_tile(&world, 1u, 1u, 0u, 0u);
    CHECK(tile != NULL);
    if (tile != NULL) {
        CHECK(tile->terrain == 9u && tile->reserved == 12u);
    }
    tile = yt2_world_tile(&world, YT2_WORLD_MAPS_X - 1u,
                          YT2_WORLD_MAPS_Y - 1u, YT2_WORLD_MAP_TILES_X - 1u,
                          YT2_WORLD_MAP_TILES_Y - 1u);
    CHECK(tile != NULL);
    if (tile != NULL) {
        CHECK(tile->terrain == 13u && tile->object == 15u);
    }

    CHECK(yt2_world_tile(&world, YT2_WORLD_MAPS_X, 0u, 0u, 0u) == NULL);
    CHECK(yt2_world_tile(&world, 0u, YT2_WORLD_MAPS_Y, 0u, 0u) == NULL);
    CHECK(yt2_world_tile(&world, 0u, 0u, YT2_WORLD_MAP_TILES_X, 0u) == NULL);
    CHECK(yt2_world_tile(&world, 0u, 0u, 0u, YT2_WORLD_MAP_TILES_Y) == NULL);
    CHECK(yt2_world_tile(NULL, 0u, 0u, 0u, 0u) == NULL);

    map = yt2_world_map(&world, 21u);
    CHECK(map != NULL);
    if (map != NULL) {
        CHECK(strcmp(map->suffix, "01  ") == 0);
        CHECK(map->name_index == 1u && map->type == 1u);
        CHECK(strcmp(yt2_world_map_name(&world, map),
                     "YENDOR-PORT HOPE    ") == 0);
    }
    map = yt2_world_map(&world, 0u);
    CHECK(map != NULL);
    if (map != NULL) {
        CHECK(strcmp(map->suffix, "0   ") == 0 && map->type == 0u);
    }
    CHECK(yt2_world_map(&world, YT2_WORLD_MAPS_COUNT) == NULL);
    CHECK(yt2_world_map(NULL, 0u) == NULL);

    invalid = world.maps[21];
    invalid.name_index = YT2_WORLD_MAP_NAMES_COUNT;
    CHECK(strcmp(yt2_world_map_name(&world, &invalid), "") == 0);
    CHECK(strcmp(yt2_world_map_name(&world, NULL), "") == 0);
    CHECK(strcmp(yt2_world_map_name(NULL, &invalid), "") == 0);

    palette = yt2_world_palette(&world, 0u);
    CHECK(palette != NULL);
    if (palette != NULL) {
        CHECK(palette->rgb[0] == 1u && palette->rgb[1] == 2u);
        CHECK(palette->rgb[2] == 3u);
    }
    palette = yt2_world_palette(&world, YT2_WORLD_PALETTES_COUNT - 1u);
    CHECK(palette != NULL);
    CHECK(yt2_world_palette(&world, YT2_WORLD_PALETTES_COUNT) == NULL);
    CHECK(yt2_world_palette(NULL, 0u) == NULL);

    yt2_world_free(&world);
    CHECK(world.bytes == NULL && world.tiles == NULL && world.size == 0u);
    yt2_world_free(NULL);

    CHECK(yt2_world_parse(bytes, YT2_WORLD_DATA_SIZE - 1u, &world) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_world_parse(NULL, YT2_WORLD_DATA_SIZE, &world) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_world_parse(bytes, YT2_WORLD_DATA_SIZE, NULL) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_world_load("/nonexistent/yt2-world", &world) ==
          YT2_STATUS_ERROR);

    free(bytes);
    return 0;
}

static int test_real(void)
{
    const char *path = getenv("YT2_WORLD");
    yt2_world world;
    const yt2_map *map;
    const yt2_tile *tile;
    unsigned int nonzero = 0;
    unsigned int filler = 0;
    unsigned int world_maps = 0;
    unsigned int dungeon_maps = 0;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_world_load(path, &world) == YT2_STATUS_OK);
    CHECK(world.size > YT2_WORLD_PREFIX_SIZE);
    CHECK(world.bytes != NULL && world.tiles != NULL);
    CHECK(world.size == 1796340u);

    for (unsigned int index = 0; index < YT2_WORLD_TILES_COUNT; index++) {
        if (world.tiles[index].terrain != 0u ||
            world.tiles[index].flags != 0u ||
            world.tiles[index].object != 0u ||
            world.tiles[index].reserved != 0u) {
            nonzero++;
        }
    }
    CHECK(nonzero == 90126u);

    for (unsigned int index = 0; index < YT2_WORLD_MAPS_COUNT; index++) {
        switch (world.maps[index].type) {
        case 0:
            filler++;
            break;
        case 1:
            world_maps++;
            break;
        case 2:
            dungeon_maps++;
            break;
        }
    }
    CHECK(filler == 48u && world_maps == 25u && dungeon_maps == 47u);

    map = yt2_world_map(&world, 21u);
    CHECK(map != NULL);
    if (map != NULL) {
        CHECK(strcmp(map->suffix, "01  ") == 0);
        CHECK(map->name_index == 1u && map->type == 1u);
        CHECK(strcmp(yt2_world_map_name(&world, map),
                     "YENDOR-PORT HOPE    ") == 0);
    }
    map = yt2_world_map(&world, 24u);
    CHECK(map != NULL);
    if (map != NULL) {
        CHECK(strcmp(map->suffix, "0   ") == 0);
        CHECK(map->name_index == 0u && map->type == 2u);
        CHECK(strcmp(yt2_world_map_name(&world, map),
                     "PORT HOPE           ") == 0);
    }
    tile = yt2_world_tile(&world, 1u, 1u, 0u, 0u);
    CHECK(tile != NULL);
    if (tile != NULL) {
        CHECK(tile->terrain == 2u && tile->flags == 0u);
        CHECK(tile->object == 25u && tile->reserved == 0u);
    }
    tile = yt2_world_tile(&world, 0u, 1u, 28u, 1u);
    CHECK(tile != NULL);
    if (tile != NULL) {
        CHECK(tile->terrain == 16u && tile->object == 39u);
    }
    CHECK(strcmp(world.names[0], "PORT HOPE           ") == 0);
    CHECK(strcmp(world.names[1], "YENDOR-PORT HOPE    ") == 0);
    for (unsigned int index = 0; index < YT2_WORLD_PALETTES_COUNT; index++) {
        CHECK(checksum(world.palettes[index].rgb, YT2_PALETTE_SIZE) ==
              palette_checksums[index]);
    }

    yt2_world_free(&world);
    CHECK(world.bytes == NULL && world.tiles == NULL && world.size == 0u);
    return 0;
}

int test_world(void)
{
    test_synthetic();
    test_real();
    return 0;
}
