#ifndef YT2_BOOT_H
#define YT2_BOOT_H

#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The static scenes of the start-up sequence.  The original fades between
 * them and plays the animated story cutscene in between; this module composes
 * the pictures, palettes and text that the cutscene settles on.
 */

typedef enum yt2_boot_scene {
    YT2_BOOT_WEBFOOT = 0,
    YT2_BOOT_STORY_SLEEP,
    YT2_BOOT_STORY_KNOCK,
    YT2_BOOT_ATTRACT_COMBAT,
    YT2_BOOT_ATTRACT_MAGIC,
    YT2_BOOT_ATTRACT_DRAGONS,
    YT2_BOOT_ATTRACT_TOWER,
    YT2_BOOT_ATTRACT_SHOP,
    YT2_BOOT_TITLE,
    YT2_BOOT_MENU,
    YT2_BOOT_SCENE_COUNT
} yt2_boot_scene;

/* Human-readable scene name for logs and test failures. */
const char *yt2_boot_scene_name(yt2_boot_scene scene);

/* Compose one scene into an already-sized surface. */
int yt2_boot_render(yt2_surface *surface, const yt2_pictures *pictures,
                    yt2_boot_scene scene);

/* The palette the scene settles on, derived from the shipped data. */
int yt2_boot_palette(const yt2_world *world, yt2_boot_scene scene,
                     yt2_palette *palette);

#endif /* YT2_BOOT_H */
