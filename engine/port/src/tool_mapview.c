#include "yt2/mapview.h"
#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

#include <stdio.h>
#include <stdlib.h>

#define MAPVIEW_WIDTH 320u
#define MAPVIEW_HEIGHT 200u

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

static int write_ppm(const char *path, const yt2_surface *surface,
                     const yt2_palette *palette)
{
    FILE *stream = fopen(path, "wb");

    if (stream == NULL) {
        return 0;
    }
    fprintf(stream, "P6\n%u %u\n255\n", surface->width, surface->height);
    for (unsigned int y = 0; y < surface->height; y++) {
        const uint8_t *row = surface->pixels + y * surface->stride;

        for (unsigned int x = 0; x < surface->width; x++) {
            uint8_t rgb[3];

            rgb[0] = yt2_palette_expand(palette->rgb[row[x] * 3u]);
            rgb[1] = yt2_palette_expand(palette->rgb[row[x] * 3u + 1u]);
            rgb[2] = yt2_palette_expand(palette->rgb[row[x] * 3u + 2u]);
            if (fwrite(rgb, 1, sizeof(rgb), stream) != sizeof(rgb)) {
                fclose(stream);
                return 0;
            }
        }
    }
    return fclose(stream) == 0;
}

int main(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_surface surface;
    const yt2_palette *palette;
    char title[64];
    char path[1024];
    unsigned long map;
    int result = 1;

    if (argc != 5) {
        fprintf(stderr, "usage: %s PICTURES.VGA WORLD.DAT MAP OUTDIR\n",
                argv[0]);
        return 2;
    }
    map = strtoul(argv[3], NULL, 0);
    if (map >= YT2_WORLD_MAPS_COUNT) {
        fprintf(stderr, "yt2p-mapview: map %s is out of range\n", argv[3]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-mapview: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-mapview: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_surface_init(&surface, MAPVIEW_WIDTH, MAPVIEW_HEIGHT) !=
        YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-mapview: cannot allocate the surface\n");
        goto done;
    }
    palette = yt2_world_palette(&world, YT2_MAPVIEW_PALETTE);
    if (palette == NULL ||
        yt2_map_view_render(&surface, &world, &pictures, (unsigned int)map) !=
            YT2_STATUS_OK ||
        yt2_world_map_display_name(&world, (unsigned int)map, title,
                                   sizeof(title)) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-mapview: cannot compose map %lu\n", map);
        yt2_surface_free(&surface);
        goto done;
    }
    printf("map %lu %-24s frame %016llX palette %016llX\n", map, title,
           (unsigned long long)checksum(surface.pixels,
                                        surface.stride * surface.height),
           (unsigned long long)checksum(palette->rgb, YT2_PALETTE_SIZE));
    snprintf(path, sizeof(path), "%s/map-%03lu.ppm", argv[4], map);
    if (!write_ppm(path, &surface, palette)) {
        fprintf(stderr, "yt2p-mapview: cannot write %s\n", path);
        yt2_surface_free(&surface);
        goto done;
    }
    yt2_surface_free(&surface);
    result = 0;

done:
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return result;
}
