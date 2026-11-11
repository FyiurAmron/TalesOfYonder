#include "yt2/catalog.h"
#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

#include <stdio.h>
#include <stdlib.h>

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

static int write_surface(const char *path, const yt2_surface *surface)
{
    FILE *stream = fopen(path, "wb");

    if (stream == NULL) {
        return 0;
    }
    if (fwrite(surface->pixels, 1, surface->stride * surface->height, stream) !=
        surface->stride * surface->height) {
        fclose(stream);
        return 0;
    }
    return fclose(stream) == 0;
}

int main(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_surface surface;
    unsigned long map;
    int result = 1;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s PICTURES.VGA WORLD.DAT MAP [OUTDIR]\n",
                argv[0]);
        return 2;
    }
    map = strtoul(argv[3], NULL, 0);
    if (map >= YT2_WORLD_MAPS_COUNT) {
        fprintf(stderr, "yt2p-catalog: map %s is out of range\n", argv[3]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-catalog: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-catalog: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-catalog: cannot allocate the surface\n");
        goto done;
    }
    if (yt2_catalog_render(&surface, &pictures, &world, (unsigned int)map) !=
        YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-catalog: cannot compose map %lu\n", map);
        yt2_surface_free(&surface);
        goto done;
    }
    {
        int rank = yt2_catalog_rank((unsigned int)map);

        printf("catalog %lu rank %d frame %016llX\n", map, rank,
               (unsigned long long)checksum(surface.pixels,
                                            surface.stride *
                                                surface.height));
    }
    if (argc == 5) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/catalog-%03lu.raw", argv[4],
                 (unsigned long)map);
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-catalog: cannot write %s\n", path);
            yt2_surface_free(&surface);
            goto done;
        }
    }
    yt2_surface_free(&surface);
    result = 0;

done:
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return result;
}
