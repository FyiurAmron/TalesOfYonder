#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/monsters.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_monsters(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_monsters monsters;
    yt2_surface surface;
    char name[64];

    CHECK(yt2_monsters_load(NULL, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_monster_name(NULL, 0u, name, sizeof(name)) == YT2_STATUS_ERROR);
    CHECK(yt2_monster_record(NULL, 0u) == NULL);
    CHECK(yt2_monsters_render(NULL, NULL, NULL, 0u) == YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_monsters_load(&monsters, &world) == YT2_STATUS_OK);

    CHECK(yt2_monster_name(&monsters, 0u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "ALLIGATOR") == 0);
    CHECK(yt2_monster_name(&monsters, 5u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "CARNIVOROUS SPIDER") == 0);
    CHECK(yt2_monster_name(&monsters, 7u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "CENTAUR SCOUT") == 0);
    CHECK(yt2_monster_name(&monsters, 10u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "CYRIL") == 0);
    CHECK(yt2_monster_name(&monsters, 60u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "WYVERN") == 0);
    CHECK(yt2_monster_name(&monsters, 61u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_monster_name(&monsters, 0u, name, 2u) == YT2_STATUS_ERROR);
    CHECK(yt2_monster_record(&monsters, 61u) == NULL);

    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_monsters_render(&surface, &pictures, &monsters, 0u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0xD5B4C34273DCD369));
    CHECK(yt2_monsters_render(&surface, &pictures, &monsters, 60u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x6E3F491525EA591E));
    CHECK(yt2_monsters_render(&surface, &pictures, &monsters, 61u) ==
          YT2_STATUS_ERROR);
    yt2_surface_free(&surface);

    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
