#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/item_detail.h"
#include "yt2/items.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_items(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_surface surface;
    char name[64];

    CHECK(yt2_items_load(NULL, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_items_category_count(8u) == 0u);
    CHECK(yt2_items_category_name(0u, NULL, 0u) == YT2_STATUS_ERROR);
    CHECK(yt2_items_category_name(8u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_item_name(NULL, 0u, name, sizeof(name)) == YT2_STATUS_ERROR);
    CHECK(yt2_items_detail_name(NULL, 0u, 0u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_items_render_list(NULL, NULL, 0u) == YT2_STATUS_ERROR);
    CHECK(yt2_items_render_detail(NULL, NULL, NULL, 0u, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_items_render_attribute(NULL, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_items_render_transport(NULL, NULL) == YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_items_load(&items, &world) == YT2_STATUS_OK);

    CHECK(yt2_items_category_count(0u) == 41u);
    CHECK(yt2_items_category_count(1u) == 0u);
    CHECK(yt2_items_category_count(2u) == 52u);
    CHECK(yt2_items_category_count(3u) == 43u);
    CHECK(yt2_items_category_count(4u) == 13u);
    CHECK(yt2_items_category_count(5u) == 27u);
    CHECK(yt2_items_category_count(6u) == 0u);
    CHECK(yt2_items_category_count(7u) == 24u);

    CHECK(yt2_items_category_name(0u, name, sizeof(name)) == YT2_STATUS_OK);
    CHECK(strcmp(name, "ARMOR/RINGS") == 0);
    CHECK(yt2_items_category_name(7u, name, sizeof(name)) == YT2_STATUS_OK);
    CHECK(strcmp(name, "WEAPONS") == 0);

    CHECK(yt2_item_name(&items, 441u, name, sizeof(name)) == YT2_STATUS_OK);
    CHECK(strcmp(name, "BROAD SWORD") == 0);
    CHECK(yt2_item_name(&items, 744u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);

    CHECK(yt2_items_detail_name(&items, 7u, 0u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "2-HANDED SWORD") == 0);
    CHECK(yt2_items_detail_name(&items, 7u, 9u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "LONG SWORD") == 0);
    CHECK(yt2_items_detail_name(&items, 7u, 10u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "LONG BOW") == 0);
    CHECK(yt2_items_detail_name(&items, 1u, 0u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_items_detail_name(&items, 7u, 24u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);

    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_items_render_list(&surface, &pictures, 0u) == YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x52651339B2A27723));
    CHECK(yt2_items_render_list(&surface, &pictures, 7u) == YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x4A2B69FF4D3178A3));
    CHECK(yt2_items_render_list(&surface, &pictures, 8u) ==
          YT2_STATUS_ERROR);
    {
        static const unsigned int category[] = {
            0u, 0u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 7u, 7u
        };
        static const unsigned int rank[] = {
            0u, 40u, 0u, 51u, 0u, 42u, 0u, 12u, 0u, 26u, 0u, 23u
        };
        static const uint64_t frame[] = {
            UINT64_C(0xC87C5DD07A185A62), UINT64_C(0xD92112F791E7D764),
            UINT64_C(0x1E817B5AF150837B), UINT64_C(0xB6584121446C9BE7),
            UINT64_C(0x59C84B0330566BC2), UINT64_C(0xC28B92EDC54E105C),
            UINT64_C(0x058C34726C803E2C), UINT64_C(0x7FF1158C5AE3F7AC),
            UINT64_C(0xA6FCF8E8B1E8075E), UINT64_C(0x93B6F804E0C2FF14),
            UINT64_C(0xE67074CED9AF3D42), UINT64_C(0x381074A149039DDA)
        };
        unsigned int i;

        for (i = 0u; i < 12u; i++) {
            CHECK(yt2_items_render_detail(&surface, &pictures, &items,
                                          category[i], rank[i]) ==
                  YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frame[i]);
        }
    }
    CHECK(yt2_items_render_detail(&surface, &pictures, &items, 1u, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_items_render_detail(&surface, &pictures, &items, 6u, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_items_render_detail(&surface, &pictures, &items, 7u, 24u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_items_render_attribute(&surface, &pictures) == YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x95390B103B037E48));
    CHECK(yt2_items_render_transport(&surface, &pictures) == YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x65F8EE40B9331EF8));
    {
        static const unsigned int category[] = {
            0u, 0u, 2u, 3u, 4u, 4u, 5u, 7u, 0u
        };
        static const unsigned int rank[] = {
            0u, 8u, 0u, 27u, 0u, 3u, 9u, 15u, 0u
        };
        static const unsigned int plus[] = {
            0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 5u
        };
        static const uint64_t frame[] = {
            UINT64_C(0xB3D20242B09338EB), UINT64_C(0x09261DA6156A2B77),
            UINT64_C(0x5D12B0CDD1A500D2), UINT64_C(0xC9335F4118960E4E),
            UINT64_C(0x3E9D8067847AC596), UINT64_C(0x5CB36EB9E65878A7),
            UINT64_C(0xA3FA36F51F77EE9B), UINT64_C(0x46D80760FA64D742),
            UINT64_C(0xB7513842755800DB)
        };
        unsigned int i;

        for (i = 0u; i < 9u; i++) {
            unsigned int file;

            CHECK(yt2_items_member_file(&items, category[i], rank[i],
                                        &file) == YT2_STATUS_OK);
            CHECK(yt2_item_page_render(&surface, &pictures, &items, file,
                                       category[i],
                                       plus[i]) == YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frame[i]);
        }
    }
    {
        unsigned int file;

        CHECK(yt2_items_member_file(NULL, 0u, 0u, &file) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_items_member_file(&items, 0u, 0u, NULL) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_items_member_file(&items, 8u, 0u, &file) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_items_member_file(&items, 0u, 41u, &file) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_item_page_render(NULL, NULL, NULL, 0u, 0u, 0u) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_item_page_render(&surface, &pictures, &items, 744u, 0u,
                                   0u) == YT2_STATUS_ERROR);
        CHECK(yt2_item_page_render(&surface, &pictures, &items, 0u, 1u,
                                   0u) == YT2_STATUS_ERROR);
        CHECK(yt2_item_plus_count(&items, 289u, 0u) == 11u);
        CHECK(yt2_item_plus_count(&items, 592u, 2u) == 0u);
    }
    yt2_surface_free(&surface);

    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
