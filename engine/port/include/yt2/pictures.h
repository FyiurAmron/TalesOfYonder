#ifndef YT2_PICTURES_H
#define YT2_PICTURES_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/status.h"

/*
 * PICTURES.VGA: ten groups of raw 8-bit pictures, packed back to back.
 *
 * Every picture is width * height bytes, rows tightly packed.  The groups are
 * the picture-cache classes; the game picks a group and a picture index and
 * copies one image into the cache.  Palettes are separate (the first group
 * uses the logo/Webfoot palettes, the rest the main game palette).
 */

#define YT2_PICTURE_GROUPS 10u

#define YT2_PICTURES_SIZE 13054330u

typedef struct yt2_picture_group {
    uint16_t width;
    uint16_t height;
    uint16_t count;
    size_t offset;              /* byte offset in PICTURES.VGA */
    size_t size;                /* width * height * count */
} yt2_picture_group;

typedef struct yt2_pictures {
    uint8_t *bytes;             /* owned file image */
    size_t size;
    yt2_picture_group groups[YT2_PICTURE_GROUPS];
} yt2_pictures;

int yt2_pictures_load(const char *path, yt2_pictures *pictures);
int yt2_pictures_parse(const uint8_t *bytes, size_t size,
                       yt2_pictures *pictures);
void yt2_pictures_free(yt2_pictures *pictures);

/* NULL when the group is out of range or the file is not loaded. */
const yt2_picture_group *yt2_pictures_group(const yt2_pictures *pictures,
                                            unsigned int group);

/* The raw width * height bytes of one picture, or NULL. */
const uint8_t *yt2_picture_data(const yt2_pictures *pictures,
                                unsigned int group, unsigned int index);

#endif /* YT2_PICTURES_H */
