#include "yt2/spell_detail.h"
#include "yt2/spells.h"
#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int render_detail(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_spells spells;
    yt2_surface surface;
    unsigned long rank;
    int result = 1;

    if (argc != 5 && argc != 6) {
        fprintf(stderr,
                "usage: %s PICTURES.VGA WORLD.DAT detail RANK [OUTDIR]\n",
                argv[0]);
        return 2;
    }
    rank = strtoul(argv[4], NULL, 0);
    if (rank >= YT2_SPELLS_COUNT) {
        fprintf(stderr, "yt2p-spells: rank %s is out of range\n", argv[4]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_spells_load(&spells, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot index the spells\n");
        goto done;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot allocate the surface\n");
        goto done;
    }
    if (yt2_spell_detail_render(&surface, &pictures, &spells,
                                (unsigned int)rank) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot compose detail %lu\n", rank);
        yt2_surface_free(&surface);
        goto done;
    }
    printf("spells detail %lu frame %016llX\n", rank,
           (unsigned long long)checksum(surface.pixels,
                                        surface.stride * surface.height));
    if (argc == 6) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/spells-detail-%03lu.raw", argv[5],
                 (unsigned long)rank);
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-spells: cannot write %s\n", path);
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

static int render_detail_index(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_spells spells;
    yt2_surface surface;
    unsigned long file;
    int result = 1;

    if (argc != 5 && argc != 6) {
        fprintf(stderr,
                "usage: %s PICTURES.VGA WORLD.DAT detail-index FILE [OUTDIR]\n",
                argv[0]);
        return 2;
    }
    file = strtoul(argv[4], NULL, 0);
    if (file >= YT2_SPELLS_COUNT) {
        fprintf(stderr, "yt2p-spells: file %s is out of range\n", argv[4]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_spells_load(&spells, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot index the spells\n");
        goto done;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot allocate the surface\n");
        goto done;
    }
    if (yt2_spell_detail_render_index(&surface, &pictures, &spells,
                                      (unsigned int)file,
                                      1) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot compose index %lu\n", file);
        yt2_surface_free(&surface);
        goto done;
    }
    printf("spells detail-index %lu frame %016llX\n", file,
           (unsigned long long)checksum(surface.pixels,
                                        surface.stride * surface.height));
    if (argc == 6) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/spells-detail-index-%03lu.raw",
                 argv[5], (unsigned long)file);
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-spells: cannot write %s\n", path);
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

int main(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_spells spells;
    yt2_surface surface;
    unsigned long rank;
    int result = 1;

    if (argc >= 4 && strcmp(argv[3], "detail") == 0) {
        return render_detail(argc, argv);
    }
    if (argc >= 4 && strcmp(argv[3], "detail-index") == 0) {
        return render_detail_index(argc, argv);
    }
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s PICTURES.VGA WORLD.DAT RANK [OUTDIR]\n"
                        "       %s PICTURES.VGA WORLD.DAT detail RANK [OUTDIR]\n"
                        "       %s PICTURES.VGA WORLD.DAT detail-index FILE [OUTDIR]\n",
                argv[0], argv[0], argv[0]);
        return 2;
    }
    rank = strtoul(argv[3], NULL, 0);
    if (rank >= YT2_SPELLS_COUNT) {
        fprintf(stderr, "yt2p-spells: rank %s is out of range\n", argv[3]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_spells_load(&spells, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot index the spells\n");
        goto done;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot allocate the surface\n");
        goto done;
    }
    if (yt2_spells_render(&surface, &pictures, &spells,
                          (unsigned int)rank) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-spells: cannot compose rank %lu\n", rank);
        yt2_surface_free(&surface);
        goto done;
    }
    printf("spells %lu frame %016llX\n", rank,
           (unsigned long long)checksum(surface.pixels,
                                        surface.stride * surface.height));
    if (argc == 5) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/spells-%03lu.raw", argv[4],
                 (unsigned long)rank);
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-spells: cannot write %s\n", path);
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
