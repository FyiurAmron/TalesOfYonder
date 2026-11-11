#include "yt2/classes.h"
#include "yt2/pictures.h"
#include "yt2/spells.h"
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

static void usage(const char *name)
{
    fprintf(stderr,
            "usage: %s PICTURES.VGA WORLD.DAT list RANK [OUTDIR]\n"
            "       %s PICTURES.VGA WORLD.DAT detail CLASS RANK [OUTDIR]\n",
            name, name);
}

int main(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_spells spells;
    yt2_classes classes;
    yt2_surface surface;
    unsigned long first;
    unsigned long second;
    int detail;
    int result = 1;

    if (argc != 5 && argc != 6 && argc != 7) {
        usage(argv[0]);
        return 2;
    }
    if (strcmp(argv[3], "list") == 0 && (argc == 5 || argc == 6)) {
        detail = 0;
        first = strtoul(argv[4], NULL, 0);
        second = 0;
    } else if (strcmp(argv[3], "detail") == 0 && (argc == 6 || argc == 7)) {
        detail = 1;
        first = strtoul(argv[4], NULL, 0);
        second = strtoul(argv[5], NULL, 0);
    } else {
        usage(argv[0]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-classes: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-classes: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_spells_load(&spells, &world) != YT2_STATUS_OK ||
        yt2_classes_load(&classes, &spells) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-classes: cannot index the classes\n");
        goto done;
    }
    if (detail && first >= YT2_CLASSES_COUNT) {
        fprintf(stderr, "yt2p-classes: class %lu is out of range\n", first);
        goto done;
    }
    if (detail && second >= yt2_class_count(&classes, (unsigned int)first)) {
        fprintf(stderr, "yt2p-classes: rank %lu is out of range\n", second);
        goto done;
    }
    if (!detail && first >= YT2_CLASSES_COUNT) {
        fprintf(stderr, "yt2p-classes: rank %lu is out of range\n", first);
        goto done;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-classes: cannot allocate the surface\n");
        goto done;
    }
    if (detail) {
        if (yt2_classes_render_detail(&surface, &pictures, &classes,
                                      (unsigned int)first,
                                      (unsigned int)second) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-classes: cannot compose %lu %lu\n", first,
                    second);
            yt2_surface_free(&surface);
            goto done;
        }
        printf("classes detail %lu %lu frame %016llX\n", first, second,
               (unsigned long long)checksum(surface.pixels,
                                            surface.stride *
                                                surface.height));
    } else {
        if (yt2_classes_render_list(&surface, &pictures, &classes,
                                    (unsigned int)first) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-classes: cannot compose rank %lu\n", first);
            yt2_surface_free(&surface);
            goto done;
        }
        printf("classes list %lu frame %016llX\n", first,
               (unsigned long long)checksum(surface.pixels,
                                            surface.stride *
                                                surface.height));
    }
    if ((detail && argc == 7) || (!detail && argc == 6)) {
        char path[1024];

        if (detail) {
            snprintf(path, sizeof(path), "%s/classes-detail-%lu-%03lu.raw",
                     argv[argc - 1], first, second);
        } else {
            snprintf(path, sizeof(path), "%s/classes-list-%03lu.raw",
                     argv[argc - 1], first);
        }
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-classes: cannot write %s\n", path);
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
