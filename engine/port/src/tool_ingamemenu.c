#include "yt2/ingamemenu.h"
#include "yt2/create.h"
#include "yt2/cursor.h"
#include "yt2/items.h"
#include "yt2/room.h"
#include "yt2/pictures.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
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

static int write_surface(const char *dir, const char *name,
                         const yt2_surface *surface)
{
    char path[1088];
    FILE *stream;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    stream = fopen(path, "wb");
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

static void show(const char *dir, const char *name, yt2_surface *surface)
{
    if (!write_surface(dir, name, surface)) {
        fprintf(stderr, "yt2p-ingamemenu: cannot write %s/%s\n", dir, name);
        return;
    }
    printf("ingamemenu %s frame %016llX\n", name,
           (unsigned long long)checksum(surface->pixels,
                                        (size_t)320u * 200u));
}

static void show_cursor(const char *name, yt2_surface *surface,
                        const yt2_pictures *pictures, int x, int y)
{
    static uint8_t copy[320u * 200u];
    yt2_surface view = *surface;

    memcpy(copy, surface->pixels, sizeof(copy));
    view.pixels = copy;
    yt2_cursor_draw(&view, pictures, 0u, x, y);
    printf("ingamemenu %s frame %016llX\n", name,
           (unsigned long long)checksum(copy, sizeof(copy)));
}

int main(int argc, char **argv)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_surface surface;
    yt2_ingamemenu ingamemenu;
    yt2_roster roster;
    yt2_ingamemenu_slot slots[YT2_INGAMEMENU_SLOTS];
    unsigned long mask = 0x9Du;
    int result = 1;

    if (argc < 5 || argc > 6) {
        fprintf(stderr, "usage: %s PICTURES.VGA WORLD.DAT GAMEDIR OUTDIR "
                        "[MASK]\n",
                argv[0]);
        return 2;
    }
    if (argc == 6) {
        mask = strtoul(argv[5], NULL, 0);
    }
    memset(&pictures, 0, sizeof(pictures));
    memset(&world, 0, sizeof(world));
    memset(&save, 0, sizeof(save));
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK ||
        yt2_world_load(argv[2], &world) != YT2_STATUS_OK ||
        yt2_items_load(&items, &world) != YT2_STATUS_OK ||
        yt2_save_start(&world, &save) != YT2_STATUS_OK ||
        yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-ingamemenu: cannot load inputs\n");
        return 1;
    }
    /* The matrix party: roster 6/D, like the B1 P4 run. */
    if (yt2_roster_begin(&roster, &save.party, &items, &pictures,
                         &surface) != YT2_STATUS_OK ||
        yt2_roster_key(&roster, '6') != YT2_STATUS_OK ||
        yt2_roster_key(&roster, 'D') != YT2_STATUS_OK ||
        !yt2_roster_done(&roster)) {
        fprintf(stderr, "yt2p-ingamemenu: cannot build the party\n");
        return 1;
    }
    /* Menu E runs fn_0ABEC before the in-game menu shows. */
    yt2_room_update_skill_flags(&save.party);
    yt2_ingamemenu_scan_slots(slots, argv[3]);
    yt2_surface_clear(&surface, 0u);
    if (yt2_ingamemenu_begin(&ingamemenu, &save.party, &save, slots, &pictures,
                       &surface, argv[3], (uint16_t)mask, 0, 0, 0u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-ingamemenu: cannot open the in-game menu\n");
        goto done;
    }
    show(argv[4], "base.bin", &surface);
    show_cursor("base+cursor", &surface, &pictures, 230, 180);
    if (yt2_ingamemenu_key(&ingamemenu, 'S') != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-ingamemenu: cannot open save list\n");
        goto done;
    }
    show(argv[4], "save-list.bin", &surface);
    show_cursor("save-list+cursor", &surface, &pictures, 230, 180);
    if (yt2_ingamemenu_key(&ingamemenu, '1') != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-ingamemenu: cannot pick slot\n");
        goto done;
    }
    for (const char *name = "TES"; *name != '\0'; name++) {
        if (yt2_ingamemenu_key(&ingamemenu, (unsigned int)*name) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-ingamemenu: cannot type\n");
            goto done;
        }
    }
    show(argv[4], "save-editor.bin", &surface);
    show_cursor("save-editor+cursor", &surface, &pictures, 230,
                180);
    if (yt2_ingamemenu_key(&ingamemenu, 'T') != YT2_STATUS_OK ||
        yt2_ingamemenu_key(&ingamemenu, 0x0Du) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-ingamemenu: cannot finish the name\n");
        goto done;
    }
    show(argv[4], "save-confirm.bin", &surface);
    show_cursor("save-confirm+cursor", &surface, &pictures, 230,
                180);
    if (yt2_ingamemenu_key(&ingamemenu, 0x0Du) != YT2_STATUS_OK ||
        !yt2_ingamemenu_done(&ingamemenu)) {
        fprintf(stderr, "yt2p-ingamemenu: cannot save\n");
        goto done;
    }
    printf("ingamemenu save means %u %u %u flags %04X\n", save.party.mean_mapping,
           save.party.mean_navigation, save.party.mean_survival,
           save.party.explore_flags);
    {
        yt2_ingamemenu load;
        yt2_room room;

        /* Embark between save and load like the live flow (visited marks
           land before the panel opens). */
        if (yt2_room_init(&room, &world, &pictures, &save, &save.party,
                          0u) != YT2_STATUS_OK ||
            yt2_room_embark(&room, &surface, NULL, NULL) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-ingamemenu: cannot embark\n");
            goto done;
        }
        /* No rescan: one boot session shares the records like DGROUP. */
        if (yt2_ingamemenu_begin(&load, &save.party, &save, slots, &pictures,
                           &surface, argv[3], 0xFEu, 0, 1,
                           0u) != YT2_STATUS_OK ||
            yt2_ingamemenu_key(&load, 'L') != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-ingamemenu: cannot open load list\n");
            goto done;
        }
        show(argv[4], "load-list.bin", &surface);
        show_cursor("load-list+cursor", &surface, &pictures, 230,
                    180);
        if (yt2_ingamemenu_key(&load, '1') != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-ingamemenu: cannot pick load slot\n");
            goto done;
        }
        show(argv[4], "load-confirm.bin", &surface);
        show_cursor("load-confirm+cursor", &surface, &pictures,
                    230, 180);
        if (yt2_ingamemenu_key(&load, 0x0Du) != YT2_STATUS_OK ||
            !yt2_ingamemenu_done(&load)) {
            fprintf(stderr, "yt2p-ingamemenu: cannot load\n");
            goto done;
        }
    }
    result = 0;
done:
    yt2_surface_free(&surface);
    yt2_save_free(&save);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return result;
}
