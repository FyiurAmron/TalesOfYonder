#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/walkthrough.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_walkthrough(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_walkthrough walk;
    yt2_surface surface;
    char line[64];

    CHECK(yt2_walkthrough_load(NULL, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_walkthrough_line(NULL, 0u, 0u, line, sizeof(line)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_walkthrough_render(NULL, NULL, NULL, 0u) == YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_walkthrough_load(&walk, &world) == YT2_STATUS_OK);

    CHECK(yt2_walkthrough_line(&walk, 0u, 0u, line, sizeof(line)) ==
          YT2_STATUS_OK);
    CHECK(strncmp(line, "1. PORT HOPE", 12) == 0);
    CHECK(yt2_walkthrough_line(&walk, 0u, 24u, line, sizeof(line)) ==
          YT2_STATUS_OK);
    CHECK(strncmp(line, " 1 OF 31", 8) == 0);
    CHECK(yt2_walkthrough_line(&walk, 19u, 24u, line, sizeof(line)) ==
          YT2_STATUS_OK);
    CHECK(strncmp(line, " 20 0F 31", 9) == 0);
    CHECK(yt2_walkthrough_line(&walk, 31u, 0u, line, sizeof(line)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_walkthrough_line(&walk, 0u, 25u, line, sizeof(line)) ==
          YT2_STATUS_ERROR);

    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    {
        static const unsigned int pages[] = { 0u, 1u, 14u, 19u, 30u };
        static const uint64_t frame[] = {
            UINT64_C(0x2006DFD600464B52), UINT64_C(0x22F421C5C49220A1),
            UINT64_C(0xA2B9C0771EAF4A5D), UINT64_C(0x964B04246F019CFC),
            UINT64_C(0x8A6765000C222B95)
        };
        unsigned int i;

        for (i = 0u; i < 5u; i++) {
            CHECK(yt2_walkthrough_render(&surface, &pictures, &walk,
                                         pages[i]) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frame[i]);
        }
    }
    CHECK(yt2_walkthrough_render(&surface, &pictures, &walk, 31u) ==
          YT2_STATUS_ERROR);
    yt2_surface_free(&surface);

    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
