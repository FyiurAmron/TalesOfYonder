/* yt2p-room: render one exploration frame for debugging and gates.
 *
 *   yt2p-room WORLD PICTURES CURGAME OUT.ppm [--pos X,Y] [--facing F]
 *       [--scene S] [--sky K] [--phase P] [--key SC...]
 *
 * Loads the party block from CURGAME (roster + position), applies the
 * overrides, optionally replays --key scancodes (hex, one frame each),
 * renders, writes the PPM and prints the FNV-1a hashes of the pixels and
 * the live palette.  Defaults reproduce the M5 room checkpoint (tick
 * 4165): pos 166,36 west, scene 0x1A6, sky 0xF0, phase 3.
 */

#include "yt2/palette.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/room.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

#include <stdint.h>
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

static int write_ppm(const char *path, const yt2_surface *surface,
                     const yt2_palette *palette)
{
    FILE *stream = fopen(path, "wb");

    if (stream == NULL) {
        return 0;
    }
    fprintf(stream, "P6\n%u %u\n255\n", surface->width, surface->height);
    for (unsigned int y = 0; y < surface->height; y++) {
        for (unsigned int x = 0; x < surface->width; x++) {
            uint8_t index =
                surface->pixels[(size_t)y * surface->stride + x];
            uint8_t rgb[3];

            rgb[0] = yt2_palette_expand(palette->rgb[(size_t)index * 3u]);
            rgb[1] = yt2_palette_expand(palette->rgb[(size_t)index * 3u + 1u]);
            rgb[2] = yt2_palette_expand(palette->rgb[(size_t)index * 3u + 2u]);
            if (fwrite(rgb, 1, 3, stream) != 3) {
                fclose(stream);
                return 0;
            }
        }
    }
    fclose(stream);
    return 1;
}

static int parse_unsigned(const char *text, unsigned int *value)
{
    char *end;
    unsigned long parsed = strtoul(text, &end, 0);

    if (text[0] == '\0' || *end != '\0' || parsed > 0xFFFFFFFFul) {
        return 0;
    }
    *value = (unsigned int)parsed;
    return 1;
}

static int present_noop(void *context)
{
    (void)context;
    return YT2_STATUS_OK;
}

int main(int argc, char **argv)
{
    yt2_world world;
    yt2_pictures pictures;
    yt2_save save;
    yt2_surface surface;
    yt2_palette palette;
    yt2_room room;
    unsigned int pos_x = 166u;
    unsigned int pos_y = 36u;
    unsigned int facing = YT2_ROOM_FACING_WEST;
    unsigned int scene = 0x1A6u;
    unsigned int sky = 0xF0u;
    unsigned int phase = 3u;
    int arg;

    if (argc < 5) {
        fprintf(stderr, "usage: %s WORLD PICTURES CURGAME OUT.ppm [...]\n",
                argv[0]);
        return 1;
    }
    for (arg = 5; arg < argc; arg++) {
        if (strcmp(argv[arg], "--pos") == 0 && arg + 1 < argc) {
            char *comma = strchr(argv[++arg], ',');

            if (comma == NULL || !parse_unsigned(argv[arg], &pos_x) ||
                !parse_unsigned(comma + 1, &pos_y)) {
                fprintf(stderr, "yt2p-room: bad --pos\n");
                return 1;
            }
        } else if (strcmp(argv[arg], "--facing") == 0 && arg + 1 < argc) {
            if (!parse_unsigned(argv[++arg], &facing)) {
                fprintf(stderr, "yt2p-room: bad --facing\n");
                return 1;
            }
        } else if (strcmp(argv[arg], "--scene") == 0 && arg + 1 < argc) {
            if (!parse_unsigned(argv[++arg], &scene)) {
                fprintf(stderr, "yt2p-room: bad --scene\n");
                return 1;
            }
        } else if (strcmp(argv[arg], "--sky") == 0 && arg + 1 < argc) {
            if (!parse_unsigned(argv[++arg], &sky)) {
                fprintf(stderr, "yt2p-room: bad --sky\n");
                return 1;
            }
        } else if (strcmp(argv[arg], "--phase") == 0 && arg + 1 < argc) {
            if (!parse_unsigned(argv[++arg], &phase)) {
                fprintf(stderr, "yt2p-room: bad --phase\n");
                return 1;
            }
        } else if (strcmp(argv[arg], "--key") == 0 && arg + 1 < argc) {
            arg++;
        } else {
            fprintf(stderr, "yt2p-room: unknown argument %s\n", argv[arg]);
            return 1;
        }
    }
    memset(&world, 0, sizeof(world));
    memset(&pictures, 0, sizeof(pictures));
    memset(&save, 0, sizeof(save));
    if (yt2_world_load(argv[1], &world) != YT2_STATUS_OK ||
        yt2_pictures_load(argv[2], &pictures) != YT2_STATUS_OK ||
        yt2_save_load(argv[3], &save) != YT2_STATUS_OK ||
        yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-room: cannot load inputs\n");
        return 1;
    }
    /* The embark-fresh CURGAME carries a zero tail (like the
       original's); the M5 checkpoint's live DGROUP assignment is
       1/6/7/8, so restore it before rendering. */
    save.party.assigned[0] = 1u;
    save.party.assigned[1] = 6u;
    save.party.assigned[2] = 7u;
    save.party.assigned[3] = 8u;
    if (yt2_room_init(&room, &world, &pictures, &save, &save.party,
                      0u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-room: cannot init room\n");
        return 1;
    }
    /* Fresh-embark volatile state, then the requested overrides. */
    if (yt2_room_embark(&room, &surface, present_noop, NULL) !=
        YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-room: cannot embark\n");
        return 1;
    }
    save.party.x = (uint16_t)pos_x;
    save.party.y = (uint16_t)pos_y;
    save.party.facing = (uint16_t)facing;
    save.party.scene = (uint16_t)scene;
    room.sky = (uint16_t)sky;
    room.rotation_phase = phase;
    if (yt2_room_relocate(&room) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-room: cannot relocate\n");
        return 1;
    }
    for (arg = 5; arg < argc; arg++) {
        if (strcmp(argv[arg], "--key") == 0 && arg + 1 < argc) {
            unsigned int scancode;

            if (!parse_unsigned(argv[++arg], &scancode) ||
                yt2_room_key(&room, &surface, scancode, present_noop,
                             NULL) != YT2_STATUS_OK) {
                fprintf(stderr, "yt2p-room: bad --key\n");
                return 1;
            }
        }
    }
    if (yt2_room_render(&room, &surface) != YT2_STATUS_OK ||
        yt2_room_palette(&room, &palette) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-room: cannot render\n");
        return 1;
    }
    if (!write_ppm(argv[4], &surface, &palette)) {
        fprintf(stderr, "yt2p-room: cannot write %s\n", argv[4]);
        return 1;
    }
    {
        char idx_path[4096];
        FILE *idx;

        snprintf(idx_path, sizeof(idx_path), "%s.idx", argv[4]);
        idx = fopen(idx_path, "wb");
        if (idx != NULL) {
            fwrite(surface.pixels, 1, (size_t)320u * 200u, idx);
            fclose(idx);
        }
    }
    printf("pixels %016llX palette %016llX pos %u,%u facing %04X scene %04X\n",
           (unsigned long long)checksum(
               surface.pixels, (size_t)320u * 200u),
           (unsigned long long)checksum(palette.rgb, YT2_PALETTE_SIZE),
           save.party.x, save.party.y, save.party.facing, save.party.scene);
    return 0;
}
