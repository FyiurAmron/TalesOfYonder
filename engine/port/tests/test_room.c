#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/palette.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/room.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/* M5 room checkpoint (B1 frame 1743, tick 4165): the oracle frame carries
   the mouse cursor at (115,162); the pinned hash zeroes that 9x16 box
   first, because the cursor is client-side in B2. */
#define ROOM_CURSOR_X 115u
#define ROOM_CURSOR_Y 162u
#define ROOM_CURSOR_W 9u
#define ROOM_CURSOR_H 16u
#define ROOM_FRAME_FNV UINT64_C(0x166202CD198B9900)
#define ROOM_PALETTE_FNV UINT64_C(0xEA991CB49988BA23)

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

static int present_noop(void *context)
{
    (void)context;
    return YT2_STATUS_OK;
}

/* A synthetic 800x144 world with room floor around the M5 start. */
static void synthetic_world(yt2_world *world, uint8_t *bytes)
{
    unsigned int x;
    unsigned int y;

    memset(bytes, 0, YT2_WORLD_TILES_COUNT * YT2_WORLD_TILE_SIZE);
    /* Indoor floor (terrain 37) with a wall band north and a doorway. */
    for (y = 28u; y < 42u; y++) {
        for (x = 158u; x < 172u; x++) {
            size_t at = ((size_t)y * YT2_WORLD_TILES_X + x) *
                        YT2_WORLD_TILE_SIZE;

            bytes[at] = 37u;
        }
    }
    for (x = 158u; x < 172u; x++) {
        bytes[((size_t)30u * YT2_WORLD_TILES_X + x) *
              YT2_WORLD_TILE_SIZE] = 2u;
    }
    bytes[((size_t)31u * YT2_WORLD_TILES_X + 162u) *
          YT2_WORLD_TILE_SIZE] = 7u;
    memset(world, 0, sizeof(*world));
    world->bytes = bytes;
    world->size = YT2_WORLD_TILES_COUNT * YT2_WORLD_TILE_SIZE;
    world->tiles = calloc(YT2_WORLD_TILES_COUNT, sizeof(yt2_tile));
    for (y = 0u; y < YT2_WORLD_TILES_Y; y++) {
        for (x = 0u; x < YT2_WORLD_TILES_X; x++) {
            size_t at = ((size_t)y * YT2_WORLD_TILES_X + x) *
                        YT2_WORLD_TILE_SIZE;
            yt2_tile *tile =
                &world->tiles[(size_t)y * YT2_WORLD_TILES_X + x];

            tile->terrain = bytes[at];
            tile->flags = bytes[at + 1u];
            tile->object = bytes[at + 2u];
            tile->reserved = bytes[at + 3u];
        }
    }
}

static void synthetic_save(yt2_save *save, uint8_t *bytes)
{
    memset(bytes, 0, YT2_SAVE_SIZE);
    memcpy(bytes + 1, "MITHWARE PARTY", 14);
    memset(save, 0, sizeof(*save));
    save->bytes = bytes;
    save->size = YT2_SAVE_SIZE;
    CHECK(yt2_party_parse(bytes, YT2_SAVE_SIZE, &save->party) ==
          YT2_STATUS_OK);
}

static void test_movement(void)
{
    static uint8_t world_bytes[YT2_WORLD_TILES_COUNT * YT2_WORLD_TILE_SIZE];
    static uint8_t save_bytes[YT2_SAVE_SIZE];
    yt2_world world;
    yt2_pictures pictures;
    yt2_save save;
    yt2_surface surface;
    yt2_room room;

    synthetic_world(&world, world_bytes);
    synthetic_save(&save, save_bytes);
    save.party.assigned[0] = 1u;
    memset(&pictures, 0, sizeof(pictures));
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_room_init(&room, &world, &pictures, &save, &save.party,
                        0u) == YT2_STATUS_OK);
    CHECK(yt2_room_embark(&room, &surface, present_noop, NULL) ==
          YT2_STATUS_OK);
    CHECK(save.party.x == 166u && save.party.y == 36u);
    CHECK(save.party.facing == YT2_ROOM_FACING_WEST);
    CHECK(room.origin_x == 127 && room.origin_y == 9);

    /* Three steps west. */
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_UP, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_UP, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_UP, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(save.party.x == 163u && save.party.y == 36u);
    CHECK(room.last_sfx == 5u);

    /* Turns cycle back after four. */
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_LEFT, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(save.party.facing == YT2_ROOM_FACING_SOUTH);
    CHECK(save.party.x == 163u && save.party.y == 36u);
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_LEFT, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_LEFT, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_LEFT, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(save.party.facing == YT2_ROOM_FACING_WEST);

    /* Bump north into the wall band (silent for void, SFX for walls). */
    save.party.facing = YT2_ROOM_FACING_NORTH;
    save.party.x = 163u;
    save.party.y = 31u;
    CHECK(yt2_room_relocate(&room) == YT2_STATUS_OK);
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_UP, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(save.party.x == 163u && save.party.y == 31u);
    CHECK(room.last_sfx == 6u);

    /* The terrain-7 doorway double-steps to (161,31). */
    save.party.facing = YT2_ROOM_FACING_WEST;
    save.party.x = 163u;
    save.party.y = 31u;
    CHECK(yt2_room_relocate(&room) == YT2_STATUS_OK);
    CHECK(yt2_room_key(&room, &surface, YT2_ROOM_SCAN_UP, present_noop,
                       NULL) == YT2_STATUS_OK);
    CHECK(save.party.x == 161u && save.party.y == 31u);

    free(world.tiles);
    yt2_surface_free(&surface);
}

static void test_time(void)
{
    yt2_world world;
    yt2_pictures pictures;
    yt2_save save;
    yt2_party party;
    yt2_room room;
    unsigned int i;

    memset(&world, 0, sizeof(world));
    memset(&pictures, 0, sizeof(pictures));
    memset(&save, 0, sizeof(save));
    memset(&party, 0, sizeof(party));
    party.scene = YT2_ROOM_EMBARK_SCENE;
    party.day = 4u;
    party.month = 11u;
    party.year = 0x222u;
    CHECK(yt2_room_init(&room, &world, &pictures, &save, &party,
                        0u) == YT2_STATUS_OK);
    room.gates =
        YT2_ROOM_GATE_CLOCK | YT2_ROOM_GATE_SKY | YT2_ROOM_GATE_ROTATION;
    room.sky = YT2_ROOM_EMBARK_SKY;
    room.sky_step = 3;
    room.sky_left = YT2_ROOM_EMBARK_SKY_LEFT;
    room.cd_clock = YT2_ROOM_CLOCK_RELOAD;
    room.cd_sky = YT2_ROOM_CLOCK_RELOAD;
    room.cd_rotation = YT2_ROOM_ROTATION_RELOAD;

    /* 91 ticks: scene and sky step once, rotation 18 times + 1 left. */
    for (i = 0u; i < 91u; i++) {
        CHECK(yt2_room_tick(&room) == YT2_STATUS_OK);
    }
    CHECK(party.scene == YT2_ROOM_EMBARK_SCENE + 1u);
    CHECK(room.sky == YT2_ROOM_EMBARK_SKY + 3u);
    CHECK(room.sky_left == YT2_ROOM_EMBARK_SKY_LEFT - 1u);
    CHECK(room.rotation_phase == 18u);
    CHECK(room.cd_rotation == 4u);

    /* Pausing the clock gate freezes the scene but not the sky. */
    room.gates &= (uint16_t)~YT2_ROOM_GATE_CLOCK;
    for (i = 0u; i < 91u; i++) {
        CHECK(yt2_room_tick(&room) == YT2_STATUS_OK);
    }
    CHECK(party.scene == YT2_ROOM_EMBARK_SCENE + 1u);
    CHECK(room.sky == YT2_ROOM_EMBARK_SKY + 6u);

    /* Date rollover at the 0x5A1 wrap. */
    party.scene = YT2_ROOM_SCENE_WRAP - 1u;
    party.day = 30u;
    party.month = 12u;
    room.gates |= YT2_ROOM_GATE_CLOCK;
    room.cd_clock = 1u;
    CHECK(yt2_room_tick(&room) == YT2_STATUS_OK);
    CHECK(party.scene == 1u);
    CHECK(party.day == 1u && party.month == 1u && party.year == 0x223u);
}

static void test_room_gate(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    const char *save_path = getenv("YT2_ROOM_SAVE");
    yt2_pictures pictures;
    yt2_world world;
    yt2_save save;
    yt2_surface surface;
    yt2_palette palette;
    yt2_room room;
    unsigned int x;
    unsigned int y;

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0' ||
        save_path == NULL || save_path[0] == '\0') {
        return;
    }
    memset(&pictures, 0, sizeof(pictures));
    memset(&world, 0, sizeof(world));
    memset(&save, 0, sizeof(save));
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_save_load(save_path, &save) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    /* The embark-fresh CURGAME carries a zero tail (like the
       original's); the M5 checkpoint's live DGROUP assignment is
       1/6/7/8, so restore it before rendering. */
    save.party.assigned[0] = 1u;
    save.party.assigned[1] = 6u;
    save.party.assigned[2] = 7u;
    save.party.assigned[3] = 8u;
    CHECK(yt2_room_init(&room, &world, &pictures, &save, &save.party,
                        0u) == YT2_STATUS_OK);
    CHECK(yt2_room_embark(&room, &surface, present_noop, NULL) ==
          YT2_STATUS_OK);

    /* B1 state at the room checkpoint (frame 1743, tick 4165). */
    save.party.scene = 0x1A6u;
    room.sky = 0xF0u;
    room.rotation_phase = 3u;
    CHECK(yt2_room_relocate(&room) == YT2_STATUS_OK);
    CHECK(yt2_room_render(&room, &surface) == YT2_STATUS_OK);
    CHECK(yt2_room_palette(&room, &palette) == YT2_STATUS_OK);
    CHECK(checksum(palette.rgb, YT2_PALETTE_SIZE) == ROOM_PALETTE_FNV);

    /* Cursor-free by design: mask B1's pointer before hashing pixels. */
    for (y = 0u; y < ROOM_CURSOR_H; y++) {
        for (x = 0u; x < ROOM_CURSOR_W; x++) {
            surface.pixels[(size_t)(ROOM_CURSOR_Y + y) * surface.stride +
                           ROOM_CURSOR_X + x] = 0u;
        }
    }
    CHECK(checksum(surface.pixels, (size_t)320u * 200u) == ROOM_FRAME_FNV);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
    yt2_pictures_free(&pictures);
    yt2_world_free(&world);
}

int test_room(void)
{
    test_movement();
    test_time();
    test_room_gate();
    return 0;
}
