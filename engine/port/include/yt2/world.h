#ifndef YT2_WORLD_H
#define YT2_WORLD_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/palette.h"
#include "yt2/status.h"

/*
 * WORLD.DAT: the map database the engine loads before play.
 *
 * The map prefix is a flat tile grid followed by map descriptors and map
 * names.  The rest of the file holds the sound/music records and is kept in
 * `bytes` until those loaders land.
 *
 * The original walks the grid in row-major order: all tiles of map (0,0),
 * then map (1,0), ...  A tile is four bytes (terrain, passthrough flag,
 * object index, reserved).  Map names are fixed 20-byte strings; map
 * descriptors are a 4-byte suffix, a name index and a type (0 filler, 1 world,
 * 2 dungeon).
 */

#define YT2_WORLD_MAPS_X 20u
#define YT2_WORLD_MAPS_Y 6u
#define YT2_WORLD_MAPS_COUNT (YT2_WORLD_MAPS_X * YT2_WORLD_MAPS_Y)
#define YT2_WORLD_MAP_TILES_X 40u
#define YT2_WORLD_MAP_TILES_Y 24u
#define YT2_WORLD_TILES_X (YT2_WORLD_MAPS_X * YT2_WORLD_MAP_TILES_X)
#define YT2_WORLD_TILES_Y (YT2_WORLD_MAPS_Y * YT2_WORLD_MAP_TILES_Y)
#define YT2_WORLD_TILES_COUNT (YT2_WORLD_TILES_X * YT2_WORLD_TILES_Y)
#define YT2_WORLD_MAP_NAMES_COUNT 70u
#define YT2_WORLD_MAP_NAME_LENGTH 20u
#define YT2_WORLD_MAP_SUFFIX_LENGTH 4u

#define YT2_WORLD_TILE_SIZE 4u
#define YT2_WORLD_MAP_SIZE 6u
#define YT2_WORLD_PREFIX_SIZE                                            \
    (YT2_WORLD_TILES_COUNT * YT2_WORLD_TILE_SIZE +                       \
     YT2_WORLD_MAPS_COUNT * YT2_WORLD_MAP_SIZE +                         \
     YT2_WORLD_MAP_NAMES_COUNT * YT2_WORLD_MAP_NAME_LENGTH)

/* Nine 6-bit palettes follow the SFX records; the game loads one by index. */
#define YT2_WORLD_PALETTES_OFFSET 0x8270Au
#define YT2_WORLD_PALETTES_COUNT 9u
#define YT2_WORLD_DATA_SIZE                                              \
    (YT2_WORLD_PALETTES_OFFSET + YT2_WORLD_PALETTES_COUNT * YT2_PALETTE_SIZE)

typedef struct yt2_tile {
    uint8_t terrain;
    uint8_t flags;
    uint8_t object;
    uint8_t reserved;
} yt2_tile;

typedef struct yt2_map {
    char suffix[YT2_WORLD_MAP_SUFFIX_LENGTH + 1u]; /* always NUL-terminated */
    uint8_t name_index;
    uint8_t type;
} yt2_map;

typedef struct yt2_world {
    uint8_t *bytes;             /* owned file image */
    size_t size;
    yt2_tile *tiles;            /* YT2_WORLD_TILES_COUNT entries */
    yt2_map maps[YT2_WORLD_MAPS_COUNT];
    char names[YT2_WORLD_MAP_NAMES_COUNT][YT2_WORLD_MAP_NAME_LENGTH + 1u];
    yt2_palette palettes[YT2_WORLD_PALETTES_COUNT];
} yt2_world;

int yt2_world_load(const char *path, yt2_world *world);

/* Parse an in-memory WORLD.DAT image; the world keeps its own copy. */
int yt2_world_parse(const uint8_t *bytes, size_t size, yt2_world *world);

void yt2_world_free(yt2_world *world);

/* NULL when the map or tile coordinate is out of range. */
const yt2_map *yt2_world_map(const yt2_world *world, unsigned int map);
const yt2_tile *yt2_world_tile(const yt2_world *world, unsigned int map_x,
                               unsigned int map_y, unsigned int tile_x,
                               unsigned int tile_y);

/* The map's name, or "" when the descriptor has no valid name index. */
const char *yt2_world_map_name(const yt2_world *world, const yt2_map *map);

/* Palette P, or NULL when P is out of range or the file is not loaded. */
const yt2_palette *yt2_world_palette(const yt2_world *world,
                                     unsigned int palette);

#endif /* YT2_WORLD_H */
