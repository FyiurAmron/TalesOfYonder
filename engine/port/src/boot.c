#include "yt2/boot.h"

#include "yt2/ingamemenu.h"
#include "yt2/status.h"
#include "yt2/text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOOT_PICTURE_WIDTH 318
#define BOOT_PICTURE_HEIGHT 198

#define STORY_TEXT_SHADOW 0x93u
#define STORY_TEXT_COLOR 0x98u
#define STORY_TEXT_LINE 6

static const char *const story_sleep_lines[] = {
    "NIGHT HAS FALLEN, AND AS YOU SLEEP, YOU DREAM OF",
    "YOUR RECENT ADVENTURE... "
};

static const char *const story_knock_lines[] = {
    "A LOUD KNOCK ON THE DOOR STARTLES YOU FROM",
    "YOUR SLEEP."
};

typedef struct boot_picture_scene {
    unsigned int group;
    unsigned int picture;
    int x;
    int y;
} boot_picture_scene;

static const boot_picture_scene boot_pictures[YT2_BOOT_SCENE_COUNT] = {
    [YT2_BOOT_WEBFOOT] = {0u, 16u, 1, 1},
    [YT2_BOOT_ATTRACT_COMBAT] = {0u, 17u, 1, 0},
    [YT2_BOOT_ATTRACT_MAGIC] = {0u, 18u, 1, 0},
    [YT2_BOOT_ATTRACT_DRAGONS] = {0u, 19u, 1, 0},
    [YT2_BOOT_ATTRACT_TOWER] = {0u, 20u, 1, 0},
    [YT2_BOOT_ATTRACT_SHOP] = {0u, 21u, 1, 0},
    [YT2_BOOT_TITLE] = {0u, 22u, 1, 0},
    [YT2_BOOT_MENU] = {0u, 2u, 1, 1}
};

const char *yt2_boot_scene_name(yt2_boot_scene scene)
{
    switch (scene) {
    case YT2_BOOT_WEBFOOT:
        return "webfoot";
    case YT2_BOOT_STORY_SLEEP:
        return "story-sleep";
    case YT2_BOOT_STORY_KNOCK:
        return "story-knock";
    case YT2_BOOT_ATTRACT_COMBAT:
        return "attract-combat";
    case YT2_BOOT_ATTRACT_MAGIC:
        return "attract-magic";
    case YT2_BOOT_ATTRACT_DRAGONS:
        return "attract-dragons";
    case YT2_BOOT_ATTRACT_TOWER:
        return "attract-tower";
    case YT2_BOOT_ATTRACT_SHOP:
        return "attract-shop";
    case YT2_BOOT_TITLE:
        return "title";
    case YT2_BOOT_MENU:
        return "menu";
    case YT2_BOOT_SCENE_COUNT:
    default:
        return "unknown";
    }
}

static void story_text(yt2_surface *surface, const char *const *lines,
                       size_t line_count, int x, int y)
{
    for (size_t line = 0; line < line_count; line++) {
        yt2_text_draw(surface, x, y + (int)line * STORY_TEXT_LINE,
                      lines[line], STORY_TEXT_SHADOW);
    }
    for (size_t line = 0; line < line_count; line++) {
        yt2_text_draw(surface, x - 1, y - 1 + (int)line * STORY_TEXT_LINE,
                      lines[line], STORY_TEXT_COLOR);
    }
}

int yt2_boot_render(yt2_surface *surface, const yt2_pictures *pictures,
                    yt2_boot_scene scene)
{
    const boot_picture_scene *scene_picture;
    const uint8_t *image;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        scene >= YT2_BOOT_SCENE_COUNT) {
        return YT2_STATUS_ERROR;
    }

    yt2_surface_clear(surface, 0u);
    switch (scene) {
    case YT2_BOOT_STORY_SLEEP:
        story_text(surface, story_sleep_lines,
                   sizeof(story_sleep_lines) / sizeof(story_sleep_lines[0]),
                   10, 174);
        return YT2_STATUS_OK;
    case YT2_BOOT_STORY_KNOCK:
        story_text(surface, story_knock_lines,
                   sizeof(story_knock_lines) / sizeof(story_knock_lines[0]),
                   34, 174);
        return YT2_STATUS_OK;
    case YT2_BOOT_WEBFOOT:
    case YT2_BOOT_ATTRACT_COMBAT:
    case YT2_BOOT_ATTRACT_MAGIC:
    case YT2_BOOT_ATTRACT_DRAGONS:
    case YT2_BOOT_ATTRACT_TOWER:
    case YT2_BOOT_ATTRACT_SHOP:
    case YT2_BOOT_TITLE:
    case YT2_BOOT_MENU:
        break;
    case YT2_BOOT_SCENE_COUNT:
    default:
        return YT2_STATUS_ERROR;
    }

    scene_picture = &boot_pictures[scene];
    image = yt2_picture_data(pictures, scene_picture->group,
                             scene_picture->picture);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    return yt2_surface_blit(surface, scene_picture->x, scene_picture->y,
                            image, BOOT_PICTURE_WIDTH, BOOT_PICTURE_HEIGHT);
}

int yt2_boot_palette(const yt2_world *world, yt2_boot_scene scene,
                     yt2_palette *palette)
{
    const yt2_palette *base;

    if (world == NULL || palette == NULL ||
        scene >= YT2_BOOT_SCENE_COUNT) {
        return YT2_STATUS_ERROR;
    }
    base = yt2_world_palette(world, scene == YT2_BOOT_WEBFOOT ? 8u : 0u);
    if (base == NULL) {
        return YT2_STATUS_ERROR;
    }
    *palette = *base;

    if (scene == YT2_BOOT_STORY_SLEEP || scene == YT2_BOOT_STORY_KNOCK) {
        /* The story screens light only the text range of palette 0. */
        memset(palette->rgb, 0, YT2_PALETTE_SIZE);
        memcpy(palette->rgb + 0x90u * YT2_PALETTE_ENTRY_SIZE,
               base->rgb + 0x90u * YT2_PALETTE_ENTRY_SIZE,
               0x10u * YT2_PALETTE_ENTRY_SIZE);
    }
    return YT2_STATUS_OK;
}

int yt2_boot_enhanced_value(const char *mode)
{
    return mode != NULL && strcmp(mode, "enhanced") == 0;
}

int yt2_boot_enhanced(void)
{
    return yt2_boot_enhanced_value(getenv("YT2_MODE"));
}

static int boot_exists(const char *dir, const char *name)
{
    char path[1088];
    FILE *probe;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    probe = fopen(path, "rb");
    if (probe == NULL) {
        return 0;
    }
    fclose(probe);
    return 1;
}

int yt2_boot_decide(const char *dir, yt2_boot_plan *plan)
{
    char name[16];
    unsigned int i;

    if (dir == NULL || plan == NULL) {
        return YT2_STATUS_ERROR;
    }
    plan->curgame = (unsigned int)boot_exists(dir, "CURGAME");
    plan->slots = 0u;
    for (i = 0u; i < YT2_INGAMEMENU_SLOTS; i++) {
        snprintf(name, sizeof(name), "SAVGAME%u", i + 1u);
        if (boot_exists(dir, name)) {
            plan->slots |= 1u << i;
        }
    }
    if (plan->slots != 0u) {
        plan->kind = YT2_BOOT_PLAN_LOAD;
    } else if (plan->curgame) {
        plan->kind = YT2_BOOT_PLAN_TITLE;
    } else {
        plan->kind = YT2_BOOT_PLAN_FULL;
    }
    return YT2_STATUS_OK;
}
