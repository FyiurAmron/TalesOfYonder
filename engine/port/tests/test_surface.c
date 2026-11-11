#include "test.h"

#include <string.h>

#include "yt2/surface.h"

int test_surface(void)
{
    yt2_surface surface;
    static const uint8_t pattern[9] = {
        1u, 2u, 3u,
        4u, 5u, 6u,
        7u, 8u, 9u
    };
    const uint8_t *row;

    CHECK(yt2_surface_init(&surface, 16u, 10u) == YT2_STATUS_OK);
    CHECK(surface.pixels != NULL);
    CHECK(surface.width == 16u && surface.height == 10u);
    CHECK(surface.stride == 16u);

    yt2_surface_clear(&surface, 0xAAu);
    row = surface.pixels;
    CHECK(row[0] == 0xAAu && row[15] == 0xAAu);
    CHECK(surface.pixels[9u * 16u + 15u] == 0xAAu);

    CHECK(yt2_surface_blit(&surface, 2, 1, pattern, 3u, 3u) ==
          YT2_STATUS_OK);
    CHECK(surface.pixels[1u * 16u + 2u] == 1u);
    CHECK(surface.pixels[1u * 16u + 4u] == 3u);
    CHECK(surface.pixels[2u * 16u + 2u] == 4u);
    CHECK(surface.pixels[3u * 16u + 4u] == 9u);
    CHECK(surface.pixels[1u * 16u + 1u] == 0xAAu);
    CHECK(surface.pixels[1u * 16u + 5u] == 0xAAu);

    yt2_surface_clear(&surface, 0u);
    CHECK(yt2_surface_blit(&surface, -1, -1, pattern, 3u, 3u) ==
          YT2_STATUS_OK);
    CHECK(surface.pixels[0] == 5u);
    CHECK(surface.pixels[1] == 6u);
    CHECK(surface.pixels[16] == 8u);
    CHECK(surface.pixels[17] == 9u);
    CHECK(surface.pixels[18] == 0u);

    yt2_surface_clear(&surface, 0u);
    CHECK(yt2_surface_blit(&surface, 15, 9, pattern, 3u, 3u) ==
          YT2_STATUS_OK);
    CHECK(surface.pixels[9u * 16u + 15u] == 1u);
    CHECK(surface.pixels[9u * 16u + 14u] == 0u);

    yt2_surface_clear(&surface, 0u);
    CHECK(yt2_surface_blit(&surface, 16, 10, pattern, 3u, 3u) ==
          YT2_STATUS_OK);
    CHECK(surface.pixels[0] == 0u);
    CHECK(yt2_surface_blit(&surface, -3, 0, pattern, 3u, 3u) ==
          YT2_STATUS_OK);
    CHECK(surface.pixels[0] == 0u);

    yt2_surface_clear(&surface, 0xAAu);
    CHECK(yt2_surface_blit_transparent(&surface, 2, 1, pattern, 3u, 3u,
                                       5u) == YT2_STATUS_OK);
    CHECK(surface.pixels[1u * 16u + 2u] == 1u);
    CHECK(surface.pixels[1u * 16u + 3u] == 2u);
    CHECK(surface.pixels[1u * 16u + 4u] == 3u);
    CHECK(surface.pixels[2u * 16u + 2u] == 4u);
    CHECK(surface.pixels[2u * 16u + 3u] == 0xAAu);
    CHECK(surface.pixels[2u * 16u + 4u] == 6u);
    CHECK(surface.pixels[3u * 16u + 4u] == 9u);
    CHECK(surface.pixels[1u * 16u + 1u] == 0xAAu);

    yt2_surface_clear(&surface, 0xAAu);
    CHECK(yt2_surface_blit_transparent(&surface, -1, -1, pattern, 3u, 3u,
                                       1u) == YT2_STATUS_OK);
    CHECK(surface.pixels[0] == 5u);
    CHECK(surface.pixels[1] == 6u);
    CHECK(surface.pixels[16] == 8u);
    CHECK(surface.pixels[17] == 9u);

    CHECK(yt2_surface_blit_transparent(NULL, 0, 0, pattern, 3u, 3u, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_surface_blit_transparent(&surface, 0, 0, NULL, 3u, 3u, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_surface_blit_transparent(&surface, 0, 0, pattern, 0u, 3u, 0u) ==
          YT2_STATUS_OK);

    CHECK(yt2_surface_blit(NULL, 0, 0, pattern, 3u, 3u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_surface_blit(&surface, 0, 0, NULL, 3u, 3u) ==
          YT2_STATUS_ERROR);

    yt2_surface_free(&surface);
    CHECK(surface.pixels == NULL && surface.width == 0u &&
          surface.height == 0u);
    yt2_surface_free(NULL);
    yt2_surface_clear(NULL, 0u);

    CHECK(yt2_surface_init(NULL, 1u, 1u) == YT2_STATUS_ERROR);
    CHECK(yt2_surface_init(&surface, 0u, 1u) == YT2_STATUS_ERROR);
    CHECK(yt2_surface_init(&surface, 1u, 0u) == YT2_STATUS_ERROR);
    return 0;
}
