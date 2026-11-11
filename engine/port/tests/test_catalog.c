#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/catalog.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_catalog(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_surface surface;
    char name[64];

    CHECK(yt2_catalog_count() == 55u);
    CHECK(yt2_catalog_map(0u) == 67u);
    CHECK(yt2_catalog_map(1u) == 30u);
    CHECK(yt2_catalog_map(4u) == 33u);
    CHECK(yt2_catalog_map(13u) == 61u);
    CHECK(yt2_catalog_map(55u) == YT2_WORLD_MAPS_COUNT);
    CHECK(yt2_catalog_rank(67u) == 0);
    CHECK(yt2_catalog_rank(33u) == 4);
    CHECK(yt2_catalog_rank(61u) == 13);
    CHECK(yt2_catalog_rank(68u) == -1);
    CHECK(yt2_catalog_rank(29u) == -1);
    CHECK(yt2_catalog_rank(24u) == 30);
    CHECK(yt2_catalog_render(NULL, NULL, NULL, 67u) == YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);

    CHECK(yt2_world_map_display_name(&world, 61u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "LOST ISLES OF YENDOR MAP 1") == 0);

    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_catalog_render(&surface, &pictures, &world, 67u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x064C1427810F47E3));
    CHECK(yt2_catalog_render(&surface, &pictures, &world, 24u) ==
          YT2_STATUS_OK);
    CHECK(yt2_catalog_render(&surface, &pictures, &world, 68u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_catalog_render(&surface, &pictures, &world, 29u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_catalog_render(&surface, NULL, &world, 67u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_catalog_render(&surface, &pictures, NULL, 67u) ==
          YT2_STATUS_ERROR);
    yt2_surface_free(&surface);

    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
