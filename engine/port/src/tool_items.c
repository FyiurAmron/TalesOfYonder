#include "yt2/item_detail.h"
#include "yt2/items.h"
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

static void usage(const char *name)
{
    fprintf(stderr,
            "usage: %s PICTURES.VGA WORLD.DAT list RANK [OUTDIR]\n"
            "       %s PICTURES.VGA WORLD.DAT detail CATEGORY RANK [OUTDIR]\n"
            "       %s PICTURES.VGA WORLD.DAT attribute [OUTDIR]\n"
            "       %s PICTURES.VGA WORLD.DAT transport [OUTDIR]\n"
            "       %s PICTURES.VGA WORLD.DAT page CATEGORY RANK PLUS [OUTDIR]\n",
            name, name, name, name, name);
}

int main(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_surface surface;
    unsigned long first;
    unsigned long second;
    int detail;
    int result = 1;

    if (argc < 4 || argc > 8) {
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
    } else if (strcmp(argv[3], "attribute") == 0 && (argc == 4 || argc == 5)) {
        detail = 2;
        first = 0;
        second = 0;
    } else if (strcmp(argv[3], "transport") == 0 && (argc == 4 || argc == 5)) {
        detail = 3;
        first = 0;
        second = 0;
    } else if (strcmp(argv[3], "page") == 0 && (argc == 7 || argc == 8)) {
        detail = 4;
        first = strtoul(argv[4], NULL, 0);
        second = strtoul(argv[5], NULL, 0);
    } else {
        usage(argv[0]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-items: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-items: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    if (yt2_items_load(&items, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-items: cannot index the items\n");
        goto done;
    }
    if (detail == 1 && first >= YT2_ITEMS_CATEGORIES) {
        fprintf(stderr, "yt2p-items: category %lu is out of range\n", first);
        goto done;
    }
    if (detail == 1 &&
        second >= yt2_items_category_count((unsigned int)first)) {
        fprintf(stderr, "yt2p-items: rank %lu is out of range\n", second);
        goto done;
    }
    if (detail == 4 && first >= YT2_ITEMS_CATEGORIES) {
        fprintf(stderr, "yt2p-items: category %lu is out of range\n", first);
        goto done;
    }
    if (detail == 0 && first >= YT2_ITEMS_CATEGORIES) {
        fprintf(stderr, "yt2p-items: rank %lu is out of range\n", first);
        goto done;
    }
    if (yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-items: cannot allocate the surface\n");
        goto done;
    }
    if (detail == 1) {
        if (yt2_items_render_detail(&surface, &pictures, &items,
                                    (unsigned int)first,
                                    (unsigned int)second) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-items: cannot compose %lu %lu\n", first,
                    second);
            yt2_surface_free(&surface);
            goto done;
        }
        printf("items detail %lu %lu frame %016llX\n", first, second,
               (unsigned long long)checksum(surface.pixels,
                                            surface.stride *
                                                surface.height));
    } else if (detail == 4) {
        unsigned int file;
        unsigned long plus = strtoul(argv[6], NULL, 0);

        if (yt2_items_member_file(&items, (unsigned int)first,
                                  (unsigned int)second, &file) !=
            YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-items: rank %lu is out of range\n", second);
            yt2_surface_free(&surface);
            goto done;
        }
        if (yt2_item_page_render(&surface, &pictures, &items, file,
                                 (unsigned int)first,
                                 (unsigned int)plus) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-items: cannot compose %lu %lu +%lu\n", first,
                    second, plus);
            yt2_surface_free(&surface);
            goto done;
        }
        printf("items page %lu %lu +%lu frame %016llX\n", first, second, plus,
               (unsigned long long)checksum(surface.pixels,
                                            surface.stride *
                                                surface.height));
    } else if (detail == 0) {
        if (yt2_items_render_list(&surface, &pictures,
                                  (unsigned int)first) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-items: cannot compose rank %lu\n", first);
            yt2_surface_free(&surface);
            goto done;
        }
        printf("items list %lu frame %016llX\n", first,
               (unsigned long long)checksum(surface.pixels,
                                            surface.stride *
                                                surface.height));
    } else {
        int ok;

        if (detail == 2) {
            ok = yt2_items_render_attribute(&surface, &pictures);
        } else {
            ok = yt2_items_render_transport(&surface, &pictures);
        }
        if (ok != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-items: cannot compose %s\n", argv[3]);
            yt2_surface_free(&surface);
            goto done;
        }
        printf("items %s frame %016llX\n", argv[3],
               (unsigned long long)checksum(surface.pixels,
                                            surface.stride *
                                                surface.height));
    }
    if ((detail == 1 && argc == 7) || (detail == 0 && argc == 6) ||
        (detail == 4 && argc == 8) ||
        ((detail == 2 || detail == 3) && argc == 5)) {
        char path[1024];

        if (detail == 4) {
            snprintf(path, sizeof(path), "%s/items-page-%lu-%03lu-p%lu.raw",
                     argv[argc - 1], first, second, strtoul(argv[6], NULL, 0));
        } else if (detail == 1) {
            snprintf(path, sizeof(path), "%s/items-detail-%lu-%03lu.raw",
                     argv[argc - 1], first, second);
        } else if (detail == 0) {
            snprintf(path, sizeof(path), "%s/items-list-%03lu.raw",
                     argv[argc - 1], first);
        } else {
            snprintf(path, sizeof(path), "%s/items-%s.raw", argv[argc - 1],
                     argv[3]);
        }
        if (!write_surface(path, &surface)) {
            fprintf(stderr, "yt2p-items: cannot write %s\n", path);
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
