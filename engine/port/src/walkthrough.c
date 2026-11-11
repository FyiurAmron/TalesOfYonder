#include "yt2/walkthrough.h"

#include <string.h>

#include "yt2/lists.h"
#include "yt2/status.h"
#include "yt2/text.h"

#define WALK_PICTURE_GROUP 0u
#define WALK_PICTURE_INDEX 13u
#define WALK_PICTURE_X 1
#define WALK_PICTURE_Y 1

#define WALK_TITLE_X 6
#define WALK_TITLE_Y 4
#define WALK_TITLE_COLOR 0x0Du

#define WALK_TEXT_X 10
#define WALK_TEXT_Y 23
#define WALK_TEXT_STEP 6
#define WALK_TEXT_COLOR 0x0Du

#define WALK_MORE_X 129
#define WALK_MORE_Y 167
#define WALK_MORE_COLOR 0x0Du

static const unsigned int walk_icons[YT2_LIST_ICON_COUNT] = {
    32u, 325u, 327u, 339u, 329u, 332u, 333u
};

int yt2_walkthrough_load(yt2_walkthrough *walk, const yt2_world *world)
{
    if (walk == NULL || world == NULL || world->bytes == NULL ||
        world->size < YT2_WALKTHROUGH_OFFSET +
                          YT2_WALKTHROUGH_PAGES * YT2_WALKTHROUGH_LINES *
                              YT2_WALKTHROUGH_RECORD_SIZE) {
        return YT2_STATUS_ERROR;
    }
    for (unsigned int i = 0u;
         i < YT2_WALKTHROUGH_PAGES * YT2_WALKTHROUGH_LINES; i++) {
        const uint8_t *record = world->bytes + YT2_WALKTHROUGH_OFFSET +
                                (size_t)i * YT2_WALKTHROUGH_RECORD_SIZE;

        if (record[YT2_WALKTHROUGH_LINE_SIZE] != 0u) {
            return YT2_STATUS_ERROR;
        }
        for (unsigned int k = 0u; k < YT2_WALKTHROUGH_LINE_SIZE; k++) {
            if (record[k] < 0x20u || record[k] > 0x7Eu) {
                return YT2_STATUS_ERROR;
            }
        }
    }
    walk->bytes = world->bytes;
    walk->size = world->size;
    return YT2_STATUS_OK;
}

int yt2_walkthrough_line(const yt2_walkthrough *walk, unsigned int page,
                         unsigned int line, char *buffer, size_t size)
{
    const uint8_t *record;
    size_t length;

    if (walk == NULL || walk->bytes == NULL || buffer == NULL ||
        size == 0u || page >= YT2_WALKTHROUGH_PAGES ||
        line >= YT2_WALKTHROUGH_LINES) {
        return YT2_STATUS_ERROR;
    }
    record = walk->bytes + YT2_WALKTHROUGH_OFFSET +
             ((size_t)page * YT2_WALKTHROUGH_LINES + line) *
                 YT2_WALKTHROUGH_RECORD_SIZE;
    length = strlen((const char *)record);
    if (length > YT2_WALKTHROUGH_LINE_SIZE || length + 1u > size) {
        return YT2_STATUS_ERROR;
    }
    memcpy(buffer, record, length + 1u);
    return YT2_STATUS_OK;
}

int yt2_walkthrough_render(yt2_surface *surface,
                           const yt2_pictures *pictures,
                           const yt2_walkthrough *walk, unsigned int page)
{
    const yt2_picture_group *group;
    const uint8_t *image;
    char more[12] = "   MORE   ";
    char line[YT2_WALKTHROUGH_LINE_SIZE + 1u];

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        walk == NULL || walk->bytes == NULL ||
        page >= YT2_WALKTHROUGH_PAGES) {
        return YT2_STATUS_ERROR;
    }
    group = yt2_pictures_group(pictures, WALK_PICTURE_GROUP);
    image = yt2_picture_data(pictures, WALK_PICTURE_GROUP,
                             WALK_PICTURE_INDEX);
    if (group == NULL || image == NULL) {
        return YT2_STATUS_ERROR;
    }
    yt2_surface_clear(surface, 0u);
    if (yt2_surface_blit(surface, WALK_PICTURE_X, WALK_PICTURE_Y, image,
                         group->width, group->height) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    yt2_text_draw(surface, WALK_TITLE_X, WALK_TITLE_Y, YT2_LIST_TITLE,
                  WALK_TITLE_COLOR);
    yt2_text_draw(surface, YT2_WALKTHROUGH_HEADER_X, WALK_TITLE_Y,
                  YT2_WALKTHROUGH_HEADER, WALK_TITLE_COLOR);
    for (unsigned int row = 0u; row < YT2_WALKTHROUGH_LINES; row++) {
        if (yt2_walkthrough_line(walk, page, row, line, sizeof(line)) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        yt2_text_draw(surface, WALK_TEXT_X,
                      WALK_TEXT_Y + (int)row * WALK_TEXT_STEP, line,
                      WALK_TEXT_COLOR);
    }
    if (page > 0u) {
        more[0] = 'a';
    }
    if (page + 1u < YT2_WALKTHROUGH_PAGES) {
        more[9] = 'b';
    }
    yt2_text_draw(surface, WALK_MORE_X, WALK_MORE_Y, more, WALK_MORE_COLOR);
    for (unsigned int index = 0u; index < YT2_LIST_ICON_COUNT; index++) {
        const uint8_t *icon =
            yt2_picture_data(pictures, YT2_LIST_ICON_GROUP,
                             walk_icons[index]);

        if (icon == NULL) {
            return YT2_STATUS_ERROR;
        }
        if (yt2_surface_blit_transparent(
                surface,
                YT2_LIST_ICON_X + (int)index * YT2_LIST_ICON_STEP,
                YT2_LIST_ICON_Y, icon, YT2_LIST_ICON_SIZE,
                YT2_LIST_ICON_SIZE,
                YT2_LIST_ICON_TRANSPARENT) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    return YT2_STATUS_OK;
}
