#include "yt2/pictures.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/file.h"

typedef struct picture_group_shape {
    uint16_t width;
    uint16_t height;
    uint16_t count;
} picture_group_shape;

static const picture_group_shape group_shapes[YT2_PICTURE_GROUPS] = {
    {318u, 198u, 23u},
    {210u, 105u, 101u},
    {140u, 155u, 215u},
    {190u, 110u, 162u},
    {224u, 74u, 18u},
    {224u, 62u, 12u},
    {56u, 136u, 55u},
    {32u, 32u, 270u},
    {16u, 16u, 510u},
    {8u, 8u, 576u}
};

int yt2_pictures_parse(const uint8_t *bytes, size_t size,
                       yt2_pictures *pictures)
{
    size_t offset = 0;
    uint8_t *image;

    if (bytes == NULL || pictures == NULL || size < YT2_PICTURES_SIZE) {
        return YT2_STATUS_ERROR;
    }
    memset(pictures, 0, sizeof(*pictures));
    image = malloc(size);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    memcpy(image, bytes, size);
    pictures->bytes = image;
    pictures->size = size;

    for (unsigned int index = 0; index < YT2_PICTURE_GROUPS; index++) {
        yt2_picture_group *group = &pictures->groups[index];
        size_t picture_size = (size_t)group_shapes[index].width *
                              group_shapes[index].height;

        group->width = group_shapes[index].width;
        group->height = group_shapes[index].height;
        group->count = group_shapes[index].count;
        group->offset = offset;
        group->size = picture_size * group->count;
        offset += group->size;
    }
    return YT2_STATUS_OK;
}

int yt2_pictures_load(const char *path, yt2_pictures *pictures)
{
    uint8_t *bytes;
    size_t size;
    yt2_status status;

    if (path == NULL || pictures == NULL) {
        return YT2_STATUS_ERROR;
    }
    status = yt2_file_load(path, &bytes, &size);
    if (status != YT2_STATUS_OK) {
        return status;
    }
    status = yt2_pictures_parse(bytes, size, pictures);
    yt2_file_free(bytes);
    return status;
}

void yt2_pictures_free(yt2_pictures *pictures)
{
    if (pictures == NULL) {
        return;
    }
    yt2_file_free(pictures->bytes);
    memset(pictures, 0, sizeof(*pictures));
}

const yt2_picture_group *yt2_pictures_group(const yt2_pictures *pictures,
                                            unsigned int group)
{
    if (pictures == NULL || pictures->bytes == NULL ||
        group >= YT2_PICTURE_GROUPS) {
        return NULL;
    }
    return &pictures->groups[group];
}

const uint8_t *yt2_picture_data(const yt2_pictures *pictures,
                                unsigned int group, unsigned int index)
{
    const yt2_picture_group *entry = yt2_pictures_group(pictures, group);
    size_t picture_size;

    if (entry == NULL || index >= entry->count) {
        return NULL;
    }
    picture_size = (size_t)entry->width * entry->height;
    return pictures->bytes + entry->offset + picture_size * index;
}
