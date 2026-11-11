#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define RENDER_WIDTH 320u
#define RENDER_HEIGHT 200u

static int parse_unsigned(const char *text, unsigned int *value)
{
    char *end;
    unsigned long result;

    errno = 0;
    result = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || result > UINT_MAX) {
        return 0;
    }
    *value = (unsigned int)result;
    return 1;
}

static int parse_signed(const char *text, int *value)
{
    char *end;
    long result;

    errno = 0;
    result = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || result < INT_MIN ||
        result > INT_MAX) {
        return 0;
    }
    *value = (int)result;
    return 1;
}

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
    const yt2_picture_group *group;
    const yt2_palette *palette;
    const uint8_t *image;
    unsigned int group_index;
    unsigned int picture_index;
    unsigned int palette_index;
    int x = 0;
    int y = 0;
    int result = 1;

    if (argc != 7 && argc != 9) {
        fprintf(stderr, "usage: %s PICTURES.VGA WORLD.DAT GROUP INDEX "
                        "PALETTE OUT.ppm [X Y]\n", argv[0]);
        return 2;
    }
    if (!parse_unsigned(argv[3], &group_index) ||
        !parse_unsigned(argv[4], &picture_index) ||
        !parse_unsigned(argv[5], &palette_index) ||
        (argc == 9 && (!parse_signed(argv[7], &x) ||
                       !parse_signed(argv[8], &y)))) {
        fprintf(stderr, "yt2p-render: bad numeric argument\n");
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-render: cannot load %s\n", argv[1]);
        return 1;
    }
    if (yt2_world_load(argv[2], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-render: cannot load %s\n", argv[2]);
        yt2_pictures_free(&pictures);
        return 1;
    }
    group = yt2_pictures_group(&pictures, group_index);
    image = yt2_picture_data(&pictures, group_index, picture_index);
    palette = yt2_world_palette(&world, palette_index);
    if (group == NULL || image == NULL || palette == NULL) {
        fprintf(stderr, "yt2p-render: picture or palette out of range\n");
        goto done;
    }
    if (yt2_surface_init(&surface, RENDER_WIDTH, RENDER_HEIGHT) !=
        YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-render: cannot allocate the surface\n");
        goto done;
    }

    yt2_surface_clear(&surface, 0u);
    if (yt2_surface_blit(&surface, x, y, image, group->width,
                         group->height) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-render: cannot blit the picture\n");
        yt2_surface_free(&surface);
        goto done;
    }
    printf("render: %ux%u group %u picture %u at %d,%d palette %u "
           "frame %016llX palette-fnv %016llX\n",
           surface.width, surface.height, group_index, picture_index, x, y,
           palette_index,
           (unsigned long long)checksum(surface.pixels,
                                        surface.stride * surface.height),
           (unsigned long long)checksum(palette->rgb, YT2_PALETTE_SIZE));
    if (!write_ppm(argv[6], &surface, palette)) {
        fprintf(stderr, "yt2p-render: cannot write %s\n", argv[6]);
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
