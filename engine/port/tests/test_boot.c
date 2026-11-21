#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include "yt2/boot.h"

typedef struct boot_expectation {
    yt2_boot_scene scene;
    const char *name;
    uint64_t frame;
    uint64_t palette;
} boot_expectation;

static const boot_expectation boot_expected[YT2_BOOT_SCENE_COUNT] = {
    {YT2_BOOT_WEBFOOT, "webfoot", UINT64_C(0xC5B1519EF4C1133A),
     UINT64_C(0x089875EBAB4D4C6F)},
    {YT2_BOOT_STORY_SLEEP, "story-sleep", UINT64_C(0xFB816B027A864AD1),
     UINT64_C(0x16E60732045316F6)},
    {YT2_BOOT_STORY_KNOCK, "story-knock", UINT64_C(0x033C9733AF00EB97),
     UINT64_C(0x16E60732045316F6)},
    {YT2_BOOT_ATTRACT_COMBAT, "attract-combat",
     UINT64_C(0x1B323B732AF8CFD9), UINT64_C(0xE3275EE3AE459FB5)},
    {YT2_BOOT_ATTRACT_MAGIC, "attract-magic", UINT64_C(0xE7BF3FF049508B10),
     UINT64_C(0xE3275EE3AE459FB5)},
    {YT2_BOOT_ATTRACT_DRAGONS, "attract-dragons",
     UINT64_C(0xFD9A4B6D3EA90207), UINT64_C(0xE3275EE3AE459FB5)},
    {YT2_BOOT_ATTRACT_TOWER, "attract-tower", UINT64_C(0x0384E6A33033B700),
     UINT64_C(0xE3275EE3AE459FB5)},
    {YT2_BOOT_ATTRACT_SHOP, "attract-shop", UINT64_C(0xEA4646AEFAF5EADD),
     UINT64_C(0xE3275EE3AE459FB5)},
    {YT2_BOOT_TITLE, "title", UINT64_C(0x09BEC5E9162924EF),
     UINT64_C(0xE3275EE3AE459FB5)},
    {YT2_BOOT_MENU, "menu", UINT64_C(0x6ADE1397DAE95BC0),
     UINT64_C(0xE3275EE3AE459FB5)}
};

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

static void boot_fixture_set(const char *tmp, const char *name, int keep)
{
    char path[2048];
    FILE *out;

    snprintf(path, sizeof(path), "%s/%s", tmp, name);
    if (!keep) {
        remove(path);
        return;
    }
    out = fopen(path, "wb");
    if (out != NULL) {
        fputc('x', out);
        fclose(out);
    }
}

/* Enhanced boot routing, driven through sequential fixture states in
   the shared scratch dir (the plan only probes existence). */
static void test_boot_plan(void)
{
    const char *tmp = getenv("YT2_PORT_TMP");
    yt2_boot_plan plan;

    CHECK(yt2_boot_enhanced_value("enhanced") != 0);
    CHECK(yt2_boot_enhanced_value("vanilla") == 0);
    CHECK(yt2_boot_enhanced_value("") == 0);
    CHECK(yt2_boot_enhanced_value(NULL) == 0);
    CHECK(yt2_boot_enhanced_value("ENHANCED") == 0);
    CHECK(yt2_boot_decide(NULL, &plan) == YT2_STATUS_ERROR);
    CHECK(yt2_boot_decide(".", NULL) == YT2_STATUS_ERROR);

    if (tmp == NULL || tmp[0] == '\0') {
        return;
    }
    boot_fixture_set(tmp, "CURGAME", 0);
    boot_fixture_set(tmp, "SAVGAME1", 0);
    boot_fixture_set(tmp, "SAVGAME2", 0);
    CHECK(yt2_boot_decide(tmp, &plan) == YT2_STATUS_OK);
    CHECK(plan.kind == YT2_BOOT_PLAN_FULL);
    CHECK(plan.curgame == 0u);
    CHECK(plan.slots == 0u);

    boot_fixture_set(tmp, "CURGAME", 1);
    CHECK(yt2_boot_decide(tmp, &plan) == YT2_STATUS_OK);
    CHECK(plan.kind == YT2_BOOT_PLAN_TITLE);
    CHECK(plan.curgame == 1u);
    CHECK(plan.slots == 0u);

    boot_fixture_set(tmp, "SAVGAME2", 1);
    CHECK(yt2_boot_decide(tmp, &plan) == YT2_STATUS_OK);
    CHECK(plan.kind == YT2_BOOT_PLAN_LOAD);
    CHECK(plan.curgame == 1u);
    CHECK(plan.slots == 0x02u);

    boot_fixture_set(tmp, "CURGAME", 0);
    CHECK(yt2_boot_decide(tmp, &plan) == YT2_STATUS_OK);
    CHECK(plan.kind == YT2_BOOT_PLAN_LOAD);
    CHECK(plan.curgame == 0u);
    CHECK(plan.slots == 0x02u);

    boot_fixture_set(tmp, "SAVGAME1", 1);
    CHECK(yt2_boot_decide(tmp, &plan) == YT2_STATUS_OK);
    CHECK(plan.kind == YT2_BOOT_PLAN_LOAD);
    CHECK(plan.slots == 0x03u);

    boot_fixture_set(tmp, "CURGAME", 0);
    boot_fixture_set(tmp, "SAVGAME1", 0);
    boot_fixture_set(tmp, "SAVGAME2", 0);
    CHECK(yt2_boot_decide(tmp, &plan) == YT2_STATUS_OK);
    CHECK(plan.kind == YT2_BOOT_PLAN_FULL);
}

int test_boot(void)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    yt2_pictures pictures;
    yt2_world world;
    yt2_surface surface;

    test_boot_plan();

    for (int scene = 0; scene < (int)YT2_BOOT_SCENE_COUNT; scene++) {
        CHECK(boot_expected[scene].scene == (yt2_boot_scene)scene);
        CHECK(yt2_boot_scene_name((yt2_boot_scene)scene) ==
              boot_expected[scene].name);
    }
    CHECK(yt2_boot_scene_name(YT2_BOOT_SCENE_COUNT) != NULL);

    if (pictures_path == NULL || pictures_path[0] == '\0' ||
        world_path == NULL || world_path[0] == '\0') {
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);

    for (int scene = 0; scene < (int)YT2_BOOT_SCENE_COUNT; scene++) {
        yt2_palette palette;

        CHECK(yt2_boot_palette(&world, (yt2_boot_scene)scene, &palette) ==
              YT2_STATUS_OK);
        CHECK(yt2_boot_render(&surface, &pictures, (yt2_boot_scene)scene) ==
              YT2_STATUS_OK);
        CHECK(checksum(surface.pixels, 320u * 200u) ==
              boot_expected[scene].frame);
        CHECK(checksum(palette.rgb, YT2_PALETTE_SIZE) ==
              boot_expected[scene].palette);
    }

    CHECK(yt2_boot_render(&surface, &pictures, YT2_BOOT_SCENE_COUNT) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_boot_palette(&world, YT2_BOOT_SCENE_COUNT, NULL) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_boot_render(NULL, &pictures, YT2_BOOT_WEBFOOT) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_boot_palette(NULL, YT2_BOOT_WEBFOOT, NULL) ==
          YT2_STATUS_ERROR);

    yt2_surface_free(&surface);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    return 0;
}
