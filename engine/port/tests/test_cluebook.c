#include "test.h"

#include <stdlib.h>

#include "yt2/cluebook.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_cluebook(void)
{
    const char *path = getenv("YT2_PICTURES");
    yt2_pictures pictures;
    yt2_surface surface;

    CHECK(yt2_cluebook_render(NULL, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_cluebook_render(&surface, NULL) == YT2_STATUS_ERROR);

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_cluebook_render(&surface, &pictures) == YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x07D368FE31D7447F));

    /* The page is not the bare picture: text and the command bar are drawn. */
    CHECK(surface.pixels[4u * 320u + 6u] != 0u);
    CHECK(surface.pixels[181u * 320u + 63u] != 0u);

    yt2_surface_free(&surface);
    yt2_pictures_free(&pictures);
    return 0;
}
