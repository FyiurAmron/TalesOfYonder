#include "test.h"

#include <stdlib.h>

#include "yt2/monster_detail.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_monster_detail(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_monsters monsters;
    yt2_surface surface;

    CHECK(yt2_monster_detail_render(NULL, NULL, NULL, 0u, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_monster_detail_phase(NULL, 0u, 0u) == 0u);
    CHECK(yt2_monster_detail_phase(NULL, 61u, 0u) == 0u);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_monsters_load(&monsters, &world) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);

    CHECK(yt2_monster_detail_render(&surface, &pictures, &monsters, 61u,
                                    0u) == YT2_STATUS_ERROR);
    CHECK(yt2_monster_detail_render(&surface, &pictures, &monsters, 0u,
                                    162u) == YT2_STATUS_ERROR);
    CHECK(yt2_monster_detail_phase(&monsters, 61u, 0u) == 0u);

    /* Nominal phase edges: gator (rank 0) cycles base..base+5. */
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 0u) == 10u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 4u) == 10u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 5u) == 11u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 29u) == 15u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 30u) == 10u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 360u) == 17u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 364u) == 17u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 365u) == 18u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 375u) == 18u);
    CHECK(yt2_monster_detail_phase(&monsters, 0u, 376u) == 10u);
    /* Rank 14 ping-pongs with the shorter period. */
    CHECK(yt2_monster_detail_phase(&monsters, 14u, 0u) == 30u);
    CHECK(yt2_monster_detail_phase(&monsters, 14u, 25u) == 35u);
    CHECK(yt2_monster_detail_phase(&monsters, 14u, 30u) == 34u);
    CHECK(yt2_monster_detail_phase(&monsters, 14u, 300u) == 37u);
    CHECK(yt2_monster_detail_phase(&monsters, 14u, 305u) == 38u);
    CHECK(yt2_monster_detail_phase(&monsters, 14u, 316u) == 30u);

    /* Cursor-free excursion compositions (base+8): g3, h2 ping-pong,
       tinted dragon, title-overdrawing centaur, blank specials. */
    {
        static const unsigned int ranks[] = { 0u, 14u, 40u, 6u, 25u, 60u };
        static const unsigned int pics[] = { 18u, 38u, 118u, 78u, 8u, 118u };
        static const uint64_t frames[] = {
            UINT64_C(0xC54C6B3F0E9A2EF3), UINT64_C(0xFDABA4DA60989103),
            UINT64_C(0x1EC5B24DCDA96801), UINT64_C(0xABF3E203BB21EABD),
            UINT64_C(0x0A51F32CB1CD8B01), UINT64_C(0xDF3E59601825B699)
        };
        unsigned int i;

        for (i = 0u; i < 6u; i++) {
            CHECK(yt2_monster_detail_render(&surface, &pictures, &monsters,
                                            ranks[i], pics[i]) ==
                  YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frames[i]);
        }
    }

    yt2_surface_free(&surface);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
