#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/pictures.h"

typedef struct test_group {
    uint16_t width;
    uint16_t height;
    uint16_t count;
    size_t offset;
    size_t size;
    uint32_t checksum;
} test_group;

static const test_group expected_groups[YT2_PICTURE_GROUPS] = {
    {318u, 198u, 23u, 0u, 1448172u, UINT32_C(0x70F8CD07)},
    {210u, 105u, 101u, 1448172u, 2227050u, UINT32_C(0x0F255152)},
    {140u, 155u, 215u, 3675222u, 4665500u, UINT32_C(0xE04F9276)},
    {190u, 110u, 162u, 8340722u, 3385800u, UINT32_C(0x21BCB0A6)},
    {224u, 74u, 18u, 11726522u, 298368u, UINT32_C(0x43D8F046)},
    {224u, 62u, 12u, 12024890u, 166656u, UINT32_C(0x157C192B)},
    {56u, 136u, 55u, 12191546u, 418880u, UINT32_C(0x6CCF1848)},
    {32u, 32u, 270u, 12610426u, 276480u, UINT32_C(0x48C4E7FE)},
    {16u, 16u, 510u, 12886906u, 130560u, UINT32_C(0x468AD880)},
    {8u, 8u, 576u, 13017466u, 36864u, UINT32_C(0xDFB96320)}
};

static uint32_t checksum(const uint8_t *bytes, size_t size)
{
    uint32_t value = UINT32_C(2166136261);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT32_C(16777619);
    }
    return value;
}

static int test_synthetic(void)
{
    uint8_t *bytes = calloc(YT2_PICTURES_SIZE, 1);
    yt2_pictures pictures;
    const yt2_picture_group *group;
    const uint8_t *data;

    CHECK(bytes != NULL);
    if (bytes == NULL) {
        return 0;
    }
    CHECK(yt2_pictures_parse(bytes, YT2_PICTURES_SIZE, &pictures) ==
          YT2_STATUS_OK);
    CHECK(pictures.size == YT2_PICTURES_SIZE && pictures.bytes != NULL);

    for (unsigned int index = 0; index < YT2_PICTURE_GROUPS; index++) {
        group = yt2_pictures_group(&pictures, index);
        CHECK(group != NULL);
        if (group == NULL) {
            continue;
        }
        CHECK(group->width == expected_groups[index].width);
        CHECK(group->height == expected_groups[index].height);
        CHECK(group->count == expected_groups[index].count);
        CHECK(group->offset == expected_groups[index].offset);
        CHECK(group->size == expected_groups[index].size);

        data = yt2_picture_data(&pictures, index, 0u);
        CHECK(data == pictures.bytes + group->offset);
        data = yt2_picture_data(&pictures, index, group->count - 1u);
        CHECK(data == pictures.bytes + group->offset + group->size -
                           (size_t)group->width * group->height);
    }

    CHECK(yt2_picture_data(&pictures, 0u, 23u) == NULL);
    CHECK(yt2_picture_data(&pictures, YT2_PICTURE_GROUPS, 0u) == NULL);
    CHECK(yt2_pictures_group(&pictures, YT2_PICTURE_GROUPS) == NULL);
    CHECK(yt2_pictures_group(NULL, 0u) == NULL);
    CHECK(yt2_picture_data(NULL, 0u, 0u) == NULL);

    yt2_pictures_free(&pictures);
    CHECK(pictures.bytes == NULL && pictures.size == 0u);
    yt2_pictures_free(NULL);

    CHECK(yt2_pictures_parse(bytes, YT2_PICTURES_SIZE - 1u, &pictures) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_pictures_parse(NULL, YT2_PICTURES_SIZE, &pictures) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_pictures_parse(bytes, YT2_PICTURES_SIZE, NULL) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_pictures_load("/nonexistent/yt2-pictures", &pictures) ==
          YT2_STATUS_ERROR);

    free(bytes);
    return 0;
}

static int test_real(void)
{
    const char *path = getenv("YT2_PICTURES");
    yt2_pictures pictures;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(path, &pictures) == YT2_STATUS_OK);
    CHECK(pictures.size == YT2_PICTURES_SIZE);

    for (unsigned int index = 0; index < YT2_PICTURE_GROUPS; index++) {
        const yt2_picture_group *group = &pictures.groups[index];

        CHECK(group->offset == expected_groups[index].offset);
        CHECK(group->size == expected_groups[index].size);
        CHECK(checksum(pictures.bytes + group->offset, group->size) ==
              expected_groups[index].checksum);
    }

    yt2_pictures_free(&pictures);
    CHECK(pictures.bytes == NULL && pictures.size == 0u);
    return 0;
}

int test_pictures(void)
{
    test_synthetic();
    test_real();
    return 0;
}
