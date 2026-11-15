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
 * The tiles form one 800x144 world grid in row-major order; a map is a 40x24
 * window into it.  The engine loads a whole row of 800 tiles at a time, so the
 * map at (map_x, map_y), tile (tile_x, tile_y) is world row
 * map_y * 24 + tile_y, world column map_x * 40 + tile_x.  A tile is four bytes
 * (terrain, passthrough flag, object index, reserved).  Map names are fixed
 * 20-byte strings padded with spaces; map descriptors are a 4-byte suffix, a
 * name index and a type (0 filler, 1 world, 2 dungeon).
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

/*
 * The clue-book overhead map reads two more WORLD.DAT records that live after
 * the palettes.  Their offsets and sizes are the records the original's file
 * table (DGROUP:D201h / D21Dh) names as records 1 and 4.
 *
 *   record 1: 144 rows of 100 bytes, one bit per world tile, MSB first.  A set
 *             bit marks a tile the overhead map draws (the dungeon floor);
 *             the surrounding zero bits are the void.
 *   record 4: 405 eight-byte legend entries, `map` (1-based descriptor),
 *             global tile `x`, global tile `y`, and the legend line index.
 *             Slots 402..404 hold the digit-run prefix of the following text
 *             record, so their map words never match a descriptor; the game
 *             reads the full 3240-byte record all the same.
 */
#define YT2_WORLD_MAP_BITS_OFFSET 0x155918u
#define YT2_WORLD_MAP_BITS_ROWS 144u
#define YT2_WORLD_MAP_BITS_ROW_BYTES 100u
#define YT2_WORLD_MAP_BITS_SIZE                                          \
    (YT2_WORLD_MAP_BITS_ROWS * YT2_WORLD_MAP_BITS_ROW_BYTES)

#define YT2_WORLD_LEGEND_OFFSET 0x165890u
#define YT2_WORLD_LEGEND_COUNT 405u

/*
 * Legend text lines (record 5 in the executable's table): 26-byte lines
 * of printable text.  Clicking a marker on the overhead map shows its
 * line's first 18 bytes; the record holds 167 lines (referenced 1..166).
 */
#define YT2_WORLD_LEGEND_TEXT_OFFSET 0x166538u
#define YT2_WORLD_LEGEND_TEXT_COUNT 167u
#define YT2_WORLD_LEGEND_TEXT_LINE_SIZE 26u
#define YT2_WORLD_LEGEND_TEXT_SIZE 18u

/*
 * The scene palette record: 768 bytes the game reads into DS:4A56h at
 * startup.  The E0h..FFh sky window (`sub_10208`) slides through it from
 * `v_9581`; the D0h..DFh rotation base (four rotating groups of four,
 * `sub_100F3` every 5 ticks) sits at offset 0x1B0.  The fade engine uses
 * the tail as scratch.
 */
#define YT2_WORLD_SCENE_PAL_OFFSET 0x82D0Au
#define YT2_WORLD_SCENE_PAL_SIZE 0x300u
#define YT2_WORLD_ANIM_OFFSET 0x82EBAu
#define YT2_WORLD_ANIM_SIZE 48u
/*
 * The first-person column geometry: the perspective table `fn_1A0A9`
 * draws through (`fn_18268`).
 */
#define YT2_WORLD_PERSP_OFFSET 0x19C826u
#define YT2_WORLD_PERSP_SIZE 0x499Cu
/*
 * Doors, objects and teleporters: the per-column record lists
 * (`fn_18200`, `v_518E` bytes) plus the 26-byte and 4-byte data records
 * (`fn_07727`/`fn_077C4`) backing `fn_11914`.
 */
#define YT2_WORLD_DOORS_OFFSET 0x1A11C2u
#define YT2_WORLD_DOORS_SIZE 0x6768u
#define YT2_WORLD_DOOR26_OFFSET 0x7E5BAu
#define YT2_WORLD_DOOR26_COUNT 600u
#define YT2_WORLD_DOOR26_SIZE 26u
#define YT2_WORLD_DOOR4_OFFSET 0x822AAu
#define YT2_WORLD_DOOR4_COUNT 280u
#define YT2_WORLD_DOOR4_SIZE 4u
/*
 * Runtime object definitions: the 2-byte id-to-definition index
 * (`fn_1852C`) and the 106-byte definitions (`fn_1850E`) `fn_12D72`
 * loads into the 0xF20 slots.
 */
#define YT2_WORLD_OBJIDX_OFFSET 0x1A92D6u
#define YT2_WORLD_OBJDEF_OFFSET 0x1A792Au
#define YT2_WORLD_OBJDEF_SIZE 106u
/*
 * The embedded LOGO.PCX (fn_15DC2's VBE picture): the EXE's static offset
 * table names 0x1AE0F6 and v_9099 sizes it 0x721E; the PCX header and its
 * 640x480 dimensions plus the 0x0C palette trailer self-check the range.
 */
#define YT2_WORLD_LOGO_OFFSET 0x1AE0F6u
#define YT2_WORLD_LOGO_SIZE 0x721Eu
#define YT2_WORLD_LEGEND_ENTRY_SIZE 8u

typedef struct yt2_legend {
    uint16_t map;               /* 1-based descriptor index */
    uint16_t x;                 /* global tile column, 0..799 */
    uint16_t y;                 /* global tile row, 0..143 */
    uint16_t line;              /* legend text line */
} yt2_legend;

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

/*
 * The full display name used by the clue book and the map title:
 * "BLACKWING MINE LEVEL 2", "LOST ISLES OF YENDOR MAP 1", "ANCIENT RUINS".
 * The suffix comes from the descriptor; "0   " means no suffix.
 */
int yt2_world_map_display_name(const yt2_world *world, unsigned int map,
                               char *buffer, size_t size);

/* The overhead-map visibility bit for a tile of `map`. */
int yt2_world_map_tile_visible(const yt2_world *world, unsigned int map,
                               unsigned int tile_x, unsigned int tile_y);

/* Read one legend entry; entries are little-endian in WORLD.DAT. */
int yt2_world_legend_at(const yt2_world *world, unsigned int index,
                        yt2_legend *legend);

/*
 * Read one legend text line's 18 text bytes into `text` (NUL-terminated;
 * stops at an embedded NUL like the original).  Needs 19 bytes.
 */
int yt2_world_legend_text(const yt2_world *world, unsigned int line,
                          char *text, size_t size);

/* Palette P, or NULL when P is out of range or the file is not loaded. */
const yt2_palette *yt2_world_palette(const yt2_world *world,
                                     unsigned int palette);

/*
 * Copy the 48 animation-base bytes into `base`.  Returns YT2_STATUS_ERROR
 * when the file is not loaded or is too short to hold the record.
 */
int yt2_world_anim_base(const yt2_world *world, uint8_t *base);

/*
 * Point `*bytes`/`*size` at the embedded LOGO.PCX file image (NULL/0 and
 * YT2_STATUS_ERROR when the file is not loaded or is too short).
 */
int yt2_world_logo(const yt2_world *world, const uint8_t **bytes,
                   size_t *size);

#endif /* YT2_WORLD_H */
