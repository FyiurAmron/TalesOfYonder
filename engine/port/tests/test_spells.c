#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/spell_detail.h"
#include "yt2/spells.h"

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

int test_spells(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_spells spells;
    yt2_surface surface;
    char name[64];

    CHECK(yt2_spells_load(NULL, NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_spell_name(NULL, 0u, name, sizeof(name)) == YT2_STATUS_ERROR);
    CHECK(yt2_spell_record(NULL, 0u) == NULL);
    CHECK(yt2_spells_render(NULL, NULL, NULL, 0u) == YT2_STATUS_ERROR);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_spells_load(&spells, &world) == YT2_STATUS_OK);

    CHECK(yt2_spell_name(&spells, 0u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "ACID RAIN") == 0);
    CHECK(yt2_spell_name(&spells, 5u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "BLOCK OF POWER") == 0);
    CHECK(yt2_spell_name(&spells, 11u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "BLOCK OF POWER") == 0);
    CHECK(yt2_spell_name(&spells, 13u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "COLD SLASH") == 0);
    CHECK(yt2_spell_name(&spells, 44u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "HEAL") == 0);
    CHECK(yt2_spell_name(&spells, 104u, name, sizeof(name)) ==
          YT2_STATUS_OK);
    CHECK(strcmp(name, "VOLCANIC ERUPTION") == 0);
    CHECK(yt2_spell_name(&spells, 105u, name, sizeof(name)) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_spell_name(&spells, 0u, name, 2u) == YT2_STATUS_ERROR);
    CHECK(yt2_spell_record(&spells, 105u) == NULL);

    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_spells_render(&surface, &pictures, &spells, 0u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x336AED2DF4355F25));
    CHECK(yt2_spells_render(&surface, &pictures, &spells, 14u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x4E0021E2C76A7281));
    CHECK(yt2_spells_render(&surface, &pictures, &spells, 98u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x14A80E5E78EF674E));
    CHECK(yt2_spells_render(&surface, &pictures, &spells, 104u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0xEB4FF720BE73034E));
    CHECK(yt2_spells_render(&surface, &pictures, &spells, 105u) ==
          YT2_STATUS_ERROR);

    CHECK(yt2_spell_detail_render(NULL, NULL, NULL, 0u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 0u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x0FB79595E489737C));
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 3u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x23A5CEF0412FAD86));
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 31u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0xAC8DE7BF1FEF84C6));
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 60u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x0A7260957E4324FD));
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 85u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0xDE7BEED456D5549F));
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 101u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x891A56D3D6EAEF4F));
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 104u) ==
          YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x3B7482191D934F83));
    CHECK(yt2_spell_detail_render(&surface, &pictures, &spells, 105u) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_spell_detail_render_index(NULL, NULL, NULL, 0u, 0) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_spell_detail_render_index(&surface, &pictures, &spells, 86u,
                                        1) == YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x6E6CF078435509A7));
    CHECK(yt2_spell_detail_render_index(&surface, &pictures, &spells, 87u,
                                        0) == YT2_STATUS_OK);
    CHECK(checksum(surface.pixels, 320u * 200u) ==
          UINT64_C(0x08EAA2E4290A0C7D));
    CHECK(yt2_spell_detail_render_index(&surface, &pictures, &spells, 105u,
                                        0) == YT2_STATUS_ERROR);
    yt2_surface_free(&surface);

    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
