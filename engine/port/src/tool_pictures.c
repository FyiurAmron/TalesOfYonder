#include "yt2/pictures.h"

#include <stdio.h>

static uint32_t checksum(const uint8_t *bytes, size_t size)
{
    uint32_t value = UINT32_C(2166136261);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT32_C(16777619);
    }
    return value;
}

int main(int argc, char **argv)
{
    yt2_pictures pictures;

    if (argc != 2) {
        fprintf(stderr, "usage: %s PICTURES.VGA\n", argv[0]);
        return 2;
    }
    if (yt2_pictures_load(argv[1], &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-pictures: cannot load %s\n", argv[1]);
        return 1;
    }

    printf("pictures: %zu bytes, %u groups\n", pictures.size,
           (unsigned)YT2_PICTURE_GROUPS);
    for (unsigned int index = 0; index < YT2_PICTURE_GROUPS; index++) {
        const yt2_picture_group *group = &pictures.groups[index];

        printf("group %u: %ux%u x%u offset %08X size %u fnv %08X\n", index,
               (unsigned)group->width, (unsigned)group->height,
               (unsigned)group->count, (unsigned)group->offset,
               (unsigned)group->size,
               (unsigned)checksum(pictures.bytes + group->offset,
                                  group->size));
    }

    yt2_pictures_free(&pictures);
    return 0;
}
