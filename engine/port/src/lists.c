#include "yt2/lists.h"

#include "yt2/status.h"
#include "yt2/text.h"

static void draw_more(yt2_surface *surface, unsigned int start,
                      unsigned int count)
{
    char line[12] = "   MORE   ";

    /* A single page shows no MORE line at all (the six-row F4 list). */
    if (count <= YT2_LIST_VISIBLE_ROWS) {
        return;
    }
    if (start > 0u) {
        line[0] = 'a';
    }
    if (start + YT2_LIST_VISIBLE_ROWS < count) {
        line[9] = 'b';
    }
    yt2_text_draw(surface, YT2_LIST_MORE_X, YT2_LIST_MORE_Y, line,
                  YT2_LIST_MORE_COLOR);
}

int yt2_list_render(yt2_surface *surface, const yt2_pictures *pictures,
                    const yt2_list_page *page)
{
    const yt2_picture_group *group;
    const uint8_t *image;
    unsigned int start;
    char text[64];

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        page == NULL || page->header == NULL || page->icons == NULL ||
        page->row_text == NULL || page->selected >= page->count) {
        return YT2_STATUS_ERROR;
    }
    group = yt2_pictures_group(pictures, YT2_LIST_PICTURE_GROUP);
    image = yt2_picture_data(pictures, YT2_LIST_PICTURE_GROUP,
                             YT2_LIST_PICTURE_INDEX);
    if (group == NULL || image == NULL) {
        return YT2_STATUS_ERROR;
    }

    start = page->selected / YT2_LIST_VISIBLE_ROWS * YT2_LIST_VISIBLE_ROWS;

    yt2_surface_clear(surface, 0u);
    if (yt2_surface_blit(surface, YT2_LIST_PICTURE_X, YT2_LIST_PICTURE_Y,
                         image, group->width, group->height) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }

    if (page->title != NULL) {
        yt2_text_draw(surface, page->title_x, YT2_LIST_TITLE_Y, page->title,
                      YT2_LIST_TITLE_COLOR);
    } else {
        yt2_text_draw(surface, YT2_LIST_TITLE_X, YT2_LIST_TITLE_Y,
                      YT2_LIST_TITLE, YT2_LIST_TITLE_COLOR);
    }
    yt2_text_draw(surface, page->header_x, YT2_LIST_HEADER_Y, page->header,
                  YT2_LIST_TITLE_COLOR);

    for (unsigned int row = 0; row < YT2_LIST_VISIBLE_ROWS; row++) {
        unsigned int index = start + row;
        uint8_t color;

        if (index >= page->count) {
            break;
        }
        if (page->row_text(page->ctx, index, text, sizeof(text)) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        color = index == page->selected ? YT2_LIST_SELECTED_COLOR
                                        : YT2_LIST_TEXT_COLOR;
        yt2_text_draw(surface, YT2_LIST_X,
                      YT2_LIST_Y + (int)row * YT2_LIST_STEP, text, color);
    }

    draw_more(surface, start, page->count);

    for (unsigned int index = 0; index < YT2_LIST_ICON_COUNT; index++) {
        const uint8_t *icon = yt2_picture_data(pictures, YT2_LIST_ICON_GROUP,
                                               page->icons[index]);

        if (icon == NULL) {
            return YT2_STATUS_ERROR;
        }
        if (yt2_surface_blit_transparent(
                surface, YT2_LIST_ICON_X + (int)index * YT2_LIST_ICON_STEP,
                YT2_LIST_ICON_Y, icon, YT2_LIST_ICON_SIZE,
                YT2_LIST_ICON_SIZE, YT2_LIST_ICON_TRANSPARENT) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    return YT2_STATUS_OK;
}
