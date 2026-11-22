#include "yt2/mapview.h"

#include "yt2/text.h"

#define MAPVIEW_TERRAIN_TYPE_COUNT 62u
#define MAPVIEW_OBJECT_TYPE_COUNT 68u
#define MAPVIEW_MAP_TILES_X YT2_WORLD_MAP_TILES_X
#define MAPVIEW_MAP_TILES_Y YT2_WORLD_MAP_TILES_Y
#define MAPVIEW_LEGEND_MAP_BIAS 1u

/*
 * DGROUP:E5B3h / DGROUP:E1D7h: the executable's terrain and object map-icon
 * tables (12 and 10 bytes per entry; the icon is at +0Ah and +8),
 * byte-verbatim from a live DGROUP dump (trailing terrain 58-59/61
 * and empty object types read zero in the binary itself).
 */
static const uint8_t terrain_map_icons[MAPVIEW_TERRAIN_TYPE_COUNT] = {
    22u, 23u, 24u, 27u, 30u, 32u, 34u, 24u, 34u, 27u,
    80u, 34u, 24u, 27u, 30u, 32u, 37u, 38u, 39u, 40u,
    41u, 41u, 42u, 42u, 44u, 45u, 37u, 38u, 42u, 42u,
    41u, 41u, 44u, 45u, 42u, 42u, 41u, 41u, 71u, 71u,
    71u, 71u, 71u, 71u, 37u, 38u, 42u, 42u, 44u, 45u,
    37u, 38u, 36u, 36u, 15u, 16u, 15u, 16u, 0u, 0u,
    24u, 0u
};

static const uint8_t object_map_icons[MAPVIEW_OBJECT_TYPE_COUNT] = {
    0u, 49u, 64u, 64u, 64u, 64u, 65u, 65u, 65u, 65u,
    66u, 66u, 57u, 58u, 59u, 60u, 61u, 81u, 82u, 83u,
    84u, 33u, 33u, 25u, 26u, 34u, 43u, 28u, 29u, 43u,
    31u, 35u, 43u, 85u, 52u, 55u, 46u, 47u, 48u, 50u,
    51u, 53u, 54u, 56u, 62u, 63u, 63u, 67u, 68u, 69u,
    70u, 86u, 86u, 86u, 86u, 87u, 86u, 86u, 87u, 86u,
    86u, 86u, 88u, 84u, 82u, 0u, 0u, 0u
};

static int draw_icon(yt2_surface *surface, const yt2_pictures *pictures,
                     unsigned int icon, int x, int y, int transparent)
{
    const uint8_t *image = yt2_picture_data(pictures, YT2_MAPVIEW_PICTURE_GROUP,
                                            icon);

    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (transparent) {
        return yt2_surface_blit_transparent(
            surface, x, y, image, YT2_MAPVIEW_ICON_SIZE,
            YT2_MAPVIEW_ICON_SIZE, YT2_MAPVIEW_ICON_TRANSPARENT);
    }
    return yt2_surface_blit(surface, x, y, image, YT2_MAPVIEW_ICON_SIZE,
                            YT2_MAPVIEW_ICON_SIZE);
}

int yt2_map_view_render(yt2_surface *surface, const yt2_world *world,
                        const yt2_pictures *pictures, unsigned int map)
{
    char title[64];
    unsigned int map_x;
    unsigned int map_y;

    if (surface == NULL || surface->pixels == NULL || world == NULL ||
        pictures == NULL || map >= YT2_WORLD_MAPS_COUNT ||
        world->maps[map].type == 0 ||
        world->size <
            YT2_WORLD_MAP_BITS_OFFSET + YT2_WORLD_MAP_BITS_SIZE ||
        world->size <
            YT2_WORLD_LEGEND_OFFSET +
                YT2_WORLD_LEGEND_COUNT * YT2_WORLD_LEGEND_ENTRY_SIZE) {
        return YT2_STATUS_ERROR;
    }
    if (yt2_world_map_display_name(world, map, title, sizeof(title)) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }

    yt2_surface_clear(surface, 0u);
    yt2_text_draw(surface, YT2_MAPVIEW_TITLE_X, YT2_MAPVIEW_TITLE_Y, title,
                  YT2_MAPVIEW_TITLE_COLOR);
    yt2_text_draw(surface, YT2_MAPVIEW_HELP_X, YT2_MAPVIEW_HELP_Y,
                  "SELECT LEGEND OR ESC", YT2_MAPVIEW_HELP_COLOR);

    map_x = map % YT2_WORLD_MAPS_X;
    map_y = map / YT2_WORLD_MAPS_X;
    for (unsigned int tile_y = 0; tile_y < MAPVIEW_MAP_TILES_Y; tile_y++) {
        for (unsigned int tile_x = 0; tile_x < MAPVIEW_MAP_TILES_X; tile_x++) {
            const yt2_tile *tile;
            unsigned int icon;

            if (!yt2_world_map_tile_visible(world, map, tile_x, tile_y)) {
                if (draw_icon(surface, pictures, YT2_MAPVIEW_VOID_ICON,
                              YT2_MAPVIEW_MAP_X +
                                  (int)tile_x * (int)YT2_MAPVIEW_ICON_SIZE,
                              YT2_MAPVIEW_MAP_Y +
                                  (int)tile_y * (int)YT2_MAPVIEW_ICON_SIZE,
                              0) != YT2_STATUS_OK) {
                    return YT2_STATUS_ERROR;
                }
                continue;
            }
            tile = yt2_world_tile(world, map_x, map_y, tile_x, tile_y);
            if (tile == NULL) {
                return YT2_STATUS_ERROR;
            }
            if (tile->terrain < MAPVIEW_TERRAIN_TYPE_COUNT) {
                icon = terrain_map_icons[tile->terrain];
                if (icon != 0u &&
                    draw_icon(surface, pictures, icon,
                              YT2_MAPVIEW_MAP_X +
                                  (int)tile_x * (int)YT2_MAPVIEW_ICON_SIZE,
                              YT2_MAPVIEW_MAP_Y +
                                  (int)tile_y * (int)YT2_MAPVIEW_ICON_SIZE,
                              0) != YT2_STATUS_OK) {
                    return YT2_STATUS_ERROR;
                }
            }
            if (tile->object < MAPVIEW_OBJECT_TYPE_COUNT) {
                icon = object_map_icons[tile->object];
                if (icon != 0u &&
                    draw_icon(surface, pictures, icon,
                              YT2_MAPVIEW_MAP_X +
                                  (int)tile_x * (int)YT2_MAPVIEW_ICON_SIZE,
                              YT2_MAPVIEW_MAP_Y +
                                  (int)tile_y * (int)YT2_MAPVIEW_ICON_SIZE,
                              1) != YT2_STATUS_OK) {
                    return YT2_STATUS_ERROR;
                }
            }
        }
    }

    for (unsigned int index = 0; index < YT2_WORLD_LEGEND_COUNT; index++) {
        yt2_legend legend;
        int x;
        int y;

        if (yt2_world_legend_at(world, index, &legend) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        if (legend.map != map + MAPVIEW_LEGEND_MAP_BIAS) {
            continue;
        }
        x = YT2_MAPVIEW_MAP_X +
            (int)(legend.x % MAPVIEW_MAP_TILES_X) *
                (int)YT2_MAPVIEW_ICON_SIZE;
        y = YT2_MAPVIEW_MAP_Y +
            (int)(legend.y % MAPVIEW_MAP_TILES_Y) *
                (int)YT2_MAPVIEW_ICON_SIZE;
        if (draw_icon(surface, pictures, YT2_MAPVIEW_LEGEND_ICON, x, y, 1) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    return YT2_STATUS_OK;
}

static void fill_rect(yt2_surface *surface, int x, int y, unsigned int width,
                      unsigned int height, uint8_t value)
{
    for (unsigned int row = 0u; row < height; row++) {
        int fy = y + (int)row;

        if (fy < 0 || fy >= (int)surface->height) {
            continue;
        }
        for (unsigned int col = 0u; col < width; col++) {
            int fx = x + (int)col;

            if (fx < 0 || fx >= (int)surface->width) {
                continue;
            }
            surface->pixels[(size_t)fy * surface->stride + (size_t)fx] = value;
        }
    }
}

int yt2_map_view_draw_legend(yt2_surface *surface,
                             const yt2_pictures *pictures,
                             const yt2_world *world, unsigned int line)
{
    char text[YT2_WORLD_LEGEND_TEXT_SIZE + 1u];
    size_t length = 0u;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        world == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (yt2_world_legend_text(world, line, text, sizeof(text)) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    if (draw_icon(surface, pictures, YT2_MAPVIEW_LEGEND_ICON,
                  YT2_MAPVIEW_LEGEND_ICON_X, YT2_MAPVIEW_LEGEND_ICON_Y,
                  0) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    while (length < YT2_WORLD_LEGEND_TEXT_SIZE && text[length] != '\0') {
        length++;
    }
    /* The original prints opaque cells over black; a fill plus the
       transparent text is the same pixels (spaces stay black). */
    fill_rect(surface, YT2_MAPVIEW_LEGEND_TEXT_X, YT2_MAPVIEW_LEGEND_TEXT_Y,
              (unsigned int)length * 6u, 6u, 0u);
    yt2_text_draw(surface, YT2_MAPVIEW_LEGEND_TEXT_X,
                  YT2_MAPVIEW_LEGEND_TEXT_Y, text,
                  YT2_MAPVIEW_LEGEND_TEXT_COLOR);
    return YT2_STATUS_OK;
}

int yt2_map_view_legend_at(const yt2_world *world, unsigned int map, int x,
                           int y, unsigned int *line)
{
    if (world == NULL || line == NULL) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int index = 0u; index < YT2_WORLD_LEGEND_COUNT; index++) {
        yt2_legend legend;
        int x0;
        int y0;

        if (yt2_world_legend_at(world, index, &legend) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        if (legend.map != map + MAPVIEW_LEGEND_MAP_BIAS) {
            continue;
        }
        x0 = YT2_MAPVIEW_MAP_X +
             (int)(legend.x % MAPVIEW_MAP_TILES_X) *
                 (int)YT2_MAPVIEW_ICON_SIZE;
        y0 = YT2_MAPVIEW_MAP_Y +
             (int)(legend.y % MAPVIEW_MAP_TILES_Y) *
                 (int)YT2_MAPVIEW_ICON_SIZE;
        if (x >= x0 && x <= x0 + (int)YT2_MAPVIEW_ICON_SIZE && y >= y0 &&
            y <= y0 + (int)YT2_MAPVIEW_ICON_SIZE) {
            *line = legend.line;
            return YT2_STATUS_OK;
        }
    }
    return YT2_STATUS_ERROR;
}

unsigned int yt2_mapview_terrain_icon(unsigned int type)
{
    if (type >= MAPVIEW_TERRAIN_TYPE_COUNT) {
        return 0u;
    }
    return terrain_map_icons[type];
}

unsigned int yt2_mapview_object_icon(unsigned int type)
{
    if (type >= MAPVIEW_OBJECT_TYPE_COUNT) {
        return 0u;
    }
    return object_map_icons[type];
}
