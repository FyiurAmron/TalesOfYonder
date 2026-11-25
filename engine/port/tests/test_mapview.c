#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/mapview.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_mapview(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_surface surface;
    yt2_legend legend;
    const yt2_palette *palette;
    char name[64];

    CHECK(yt2_map_view_render(NULL, NULL, NULL, 67u) == YT2_STATUS_ERROR);
    CHECK(yt2_world_map_tile_visible(NULL, 67u, 0u, 0u) == 0);
    CHECK(yt2_world_legend_at(NULL, 0u, &legend) == YT2_STATUS_ERROR);
    CHECK(yt2_world_map_display_name(NULL, 67u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);

    CHECK(yt2_world_map_display_name(&world, 67u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "ANCIENT RUINS") == 0);
    CHECK(yt2_world_map_display_name(&world, 30u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "BLACKWING MINE LEVEL 1") == 0);
    CHECK(yt2_world_map_display_name(&world, 61u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "LOST ISLES OF YENDOR MAP 1") == 0);
    CHECK(yt2_world_map_display_name(&world, 120u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_world_map_display_name(&world, 67u, NULL, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_world_map_display_name(&world, 67u, name, 2u) ==
          YT2_STATUS_ERROR);

    CHECK(yt2_world_map_tile_visible(&world, 67u, 0u, 0u) == 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 10u, 0u) != 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 30u, 0u) != 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 31u, 0u) == 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 0u, 1u) == 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 9u, 1u) != 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 19u, 23u) != 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 0u, 23u) == 0);
    CHECK(yt2_world_map_tile_visible(&world, 67u, 40u, 0u) == 0);
    CHECK(yt2_world_map_tile_visible(&world, 120u, 0u, 0u) == 0);

    CHECK(yt2_world_legend_at(&world, 247u, &legend) == YT2_STATUS_OK);
    CHECK(legend.map == 68u && legend.x == 299u && legend.y == 95u &&
          legend.line == 30u);
    CHECK(yt2_world_legend_at(&world, 248u, &legend) == YT2_STATUS_OK);
    CHECK(legend.map == 68u && legend.x == 301u && legend.y == 78u &&
          legend.line == 116u);
    /* The record's last three slots are the text record's digit run. */
    CHECK(yt2_world_legend_at(&world, 402u, &legend) == YT2_STATUS_OK);
    CHECK(legend.map == 12849u);
    CHECK(yt2_world_legend_at(&world, YT2_WORLD_LEGEND_COUNT, &legend) ==
          YT2_STATUS_ERROR);

    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_map_view_render(&surface, &world, &pictures, 67u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x725194639EB33B4F));
    palette = yt2_world_palette(&world, YT2_MAPVIEW_PALETTE);
    CHECK(palette != NULL);
    CHECK(checksum(palette->rgb, YT2_PALETTE_SIZE) ==
          UINT64_C(0xE3275EE3AE459FB5));
    /* In-game map shortcuts (M=31, L=47, B1 fixed). U=7 is filler
       in WORLD.DAT (type 0), so B1's U never shows either. */
    CHECK(yt2_map_view_render(&surface, &world, &pictures, 31u) ==
          YT2_STATUS_OK);
    CHECK(yt2_map_view_render(&surface, &world, &pictures, 47u) ==
          YT2_STATUS_OK);
    CHECK(yt2_map_view_render(&surface, &world, &pictures, 7u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_map_view_render(&surface, &world, &pictures, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_map_view_render(&surface, &world, &pictures,
                              YT2_WORLD_MAPS_COUNT) == YT2_STATUS_ERROR);
    CHECK(yt2_map_view_render(&surface, NULL, &pictures, 67u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_map_view_render(&surface, &world, NULL, 67u) ==
          YT2_STATUS_ERROR);

    /* Legend text lines (record 5). */
    {
        char text[YT2_WORLD_LEGEND_TEXT_SIZE + 1u];

        CHECK(yt2_world_legend_text(NULL, 116u, text, sizeof(text)) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_world_legend_text(&world, 116u, NULL, 19u) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_world_legend_text(&world, 116u, text, 18u) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_world_legend_text(
                  &world, YT2_WORLD_LEGEND_TEXT_COUNT, text, sizeof(text)) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_world_legend_text(&world, 116u, text, sizeof(text)) ==
              YT2_STATUS_OK);
        CHECK(strcmp(text, "BONES OF TRAVIS   ") == 0);
        CHECK(yt2_world_legend_text(&world, 30u, text, sizeof(text)) ==
              YT2_STATUS_OK);
        CHECK(strcmp(text, "ENTRANCE          ") == 0);
    }

    /* Legend marker hit-testing (map 67's two markers). */
    {
        unsigned int line = 0u;

        CHECK(yt2_map_view_legend_at(NULL, 67u, 172, 60, &line) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_map_view_legend_at(&world, 67u, 172, 60, NULL) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_map_view_legend_at(&world, 67u, 172, 60, &line) ==
              YT2_STATUS_OK);
        CHECK(line == 116u);
        CHECK(yt2_map_view_legend_at(&world, 67u, 168, 56, &line) ==
              YT2_STATUS_OK);
        CHECK(line == 116u);
        CHECK(yt2_map_view_legend_at(&world, 67u, 176, 64, &line) ==
              YT2_STATUS_OK);
        CHECK(line == 116u);
        CHECK(yt2_map_view_legend_at(&world, 67u, 177, 60, &line) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_map_view_legend_at(&world, 67u, 156, 196, &line) ==
              YT2_STATUS_OK);
        CHECK(line == 30u);
        CHECK(yt2_map_view_legend_at(&world, 67u, 40, 150, &line) ==
              YT2_STATUS_ERROR);
    }

    /* Legend overlay compositions (layout verified against the DOS
       original: icon 73h at (161,0), text at (170,1) in color 123). */
    CHECK(yt2_map_view_draw_legend(NULL, NULL, NULL, 116u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_map_view_render(&surface, &world, &pictures, 67u) ==
          YT2_STATUS_OK);
    CHECK(yt2_map_view_draw_legend(&surface, &pictures, &world, 116u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x40221FD89774C067));
    CHECK(yt2_map_view_render(&surface, &world, &pictures, 67u) ==
          YT2_STATUS_OK);
    CHECK(yt2_map_view_draw_legend(&surface, &pictures, &world, 30u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x025C0FD8B4677669));
    yt2_surface_free(&surface);

    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
