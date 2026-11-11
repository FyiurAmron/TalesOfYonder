#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/classes.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_classes(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_spells spells;
    yt2_classes classes;
    yt2_surface surface;
    char name[64];

    CHECK(yt2_classes_load(NULL, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_class_count(NULL, 0u) == 0u);
    CHECK(yt2_class_row_text(0u, NULL, 0u) == YT2_STATUS_ERROR);
    CHECK(yt2_class_row_text(6u, name, sizeof(name)) == YT2_STATUS_ERROR);
    CHECK(yt2_class_spell_name(NULL, 0u, 0u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_classes_render_list(NULL, NULL, NULL, 0u) == YT2_STATUS_ERROR);
    CHECK(yt2_classes_render_detail(NULL, NULL, NULL, 0u, 0u) ==
          YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_spells_load(&spells, &world) == YT2_STATUS_OK);
    CHECK(yt2_classes_load(&classes, &spells) == YT2_STATUS_OK);

    CHECK(yt2_class_count(&classes, 0u) == 52u);
    CHECK(yt2_class_count(&classes, 1u) == 50u);
    CHECK(yt2_class_count(&classes, 2u) == 32u);
    CHECK(yt2_class_count(&classes, 3u) == 47u);
    CHECK(yt2_class_count(&classes, 4u) == 47u);
    CHECK(yt2_class_count(&classes, 5u) == 31u);
    CHECK(yt2_class_count(&classes, 6u) == 0u);

    CHECK(yt2_class_row_text(0u, name, sizeof(name)) == YT2_STATUS_OK);
    CHECK(strcmp(name, "MONK / CLERIC / PRIEST") == 0);
    CHECK(yt2_class_row_text(5u, name, sizeof(name)) == YT2_STATUS_OK);
    CHECK(strcmp(name, "MARKSMAN / RANGER / KNIGHT") == 0);
    CHECK(yt2_class_row_text(0u, name, 4u) == YT2_STATUS_ERROR);

    CHECK(yt2_class_spell_name(&classes, 3u, 3u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "BEAM OF DEATH") == 0);
    CHECK(yt2_class_spell_name(&classes, 4u, 5u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "BLOCK OF POWER") == 0);
    CHECK(yt2_class_spell_name(&classes, 5u, 0u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "BALL OF ELECTRICITY") == 0);
    CHECK(yt2_class_spell_name(&classes, 0u, 51u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "UNSTONE") == 0);
    CHECK(yt2_class_spell_name(&classes, 0u, 52u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_class_spell_name(&classes, 6u, 0u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);

    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_classes_render_list(&surface, &pictures, &classes, 0u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x635F5999F0C8700A));
    CHECK(yt2_classes_render_list(&surface, &pictures, &classes, 5u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x3E56F34BA5FD770A));
    CHECK(yt2_classes_render_list(&surface, &pictures, &classes, 6u) ==
          YT2_STATUS_ERROR);
    {
        static const unsigned int line[] = {
            0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u
        };
        static const unsigned int rank[] = {
            0u, 51u, 0u, 49u, 0u, 31u, 0u, 46u, 0u, 46u, 0u, 30u
        };
        static const uint64_t frame[] = {
            UINT64_C(0x7D2F8F049CB7FC98), UINT64_C(0xF3BB3DD90AE90F86),
            UINT64_C(0x63D575509257F536), UINT64_C(0x4894CC8AA5C6573B),
            UINT64_C(0x53D69E6B5ECC0E63), UINT64_C(0x22F3C1CCB32DFCAB),
            UINT64_C(0x3E269B1BDE50C254), UINT64_C(0x2997949A89D11BF3),
            UINT64_C(0x9A05102B5253BD5E), UINT64_C(0x481A7B87A5FBB989),
            UINT64_C(0xC2D26B9AA8411307), UINT64_C(0xDFA9A774EC40F77A)
        };
        unsigned int i;

        for (i = 0u; i < 12u; i++) {
            CHECK(yt2_classes_render_detail(&surface, &pictures, &classes,
                                            line[i], rank[i]) ==
                  YT2_STATUS_OK);
            CHECK(checksum(surface.pixels, 320u * 200u) == frame[i]);
        }
    }
    CHECK(yt2_classes_render_detail(&surface, &pictures, &classes, 6u, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_classes_render_detail(&surface, &pictures, &classes, 0u, 52u) ==
          YT2_STATUS_ERROR);
    yt2_surface_free(&surface);

    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
