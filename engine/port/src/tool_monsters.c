#include "yt2/monster_detail.h"
#include "yt2/monsters.h"
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
    yt2_monsters monsters;
    yt2_surface surface;
    unsigned long rank;
    unsigned long pic;
    int result = 1;

    if (argc != 6 && argc != 7) {
        fprintf(stderr,
                "usage: %s PICTURES.VGA WORLD.DAT detail RANK PIC [OUTDIR]\n",
                argv[0]);
        return 2;
    }
    rank = strtoul(argv[4], NULL, 0);
    pic = strtoul(argv[5], NULL, 0);
    if (rank >= YT2_MONSTERS_COUNT) {
        fprintf(stderr, "yt2p-monsters: rank %s is out of range\n", argv[4]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_monsters_load(&monsters, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot index the monsters\n");
        goto done;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot allocate the surface\n");
        goto done;
    }
    if (yt2_monster_detail_render(&surface, &pictures, &monsters,
                                  (unsigned int)rank,
                                  (unsigned int)pic) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot compose rank %lu pic %lu\n",
                rank, pic);
        yt2_surface_free(&surface);
        goto done;
    }
    printf("detail %lu pic %lu frame %016llX\n", rank, pic,
           (unsigned long long)checksum(surface.pixels,
                                        surface.stride * surface.height));
    if (argc == 7) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/detail-%03lu-%03lu.raw", argv[6],
                 rank, pic);
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-monsters: cannot write %s\n", path);
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
    yt2_monsters monsters;
    yt2_surface surface;
    unsigned long rank;
    int result = 1;

    if (argc != 4 && argc != 5 && argc != 6 && argc != 7) {
        fprintf(stderr,
                "usage: %s PICTURES.VGA WORLD.DAT RANK [OUTDIR]\n"
                "       %s PICTURES.VGA WORLD.DAT detail RANK PIC [OUTDIR]\n",
                argv[0], argv[0]);
        return 2;
    }
    if (strcmp(argv[3], "detail") == 0) {
        return render_detail(argc, argv);
    }
    rank = strtoul(argv[3], NULL, 0);
    if (rank >= YT2_MONSTERS_COUNT) {
        fprintf(stderr, "yt2p-monsters: rank %s is out of range\n", argv[3]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_monsters_load(&monsters, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot index the monsters\n");
        goto done;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot allocate the surface\n");
        goto done;
    }
    if (yt2_monsters_render(&surface, &pictures, &monsters,
                            (unsigned int)rank) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-monsters: cannot compose rank %lu\n", rank);
        yt2_surface_free(&surface);
        goto done;
    }
    printf("monsters %lu frame %016llX\n", rank,
           (unsigned long long)checksum(surface.pixels,
                                        surface.stride * surface.height));
    if (argc == 5) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/monsters-%03lu.raw", argv[4],
                 (unsigned long)rank);
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-monsters: cannot write %s\n", path);
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
