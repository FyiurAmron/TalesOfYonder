#ifndef YT2_LISTS_H
#define YT2_LISTS_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/pictures.h"
#include "yt2/surface.h"

/*
 * The clue-book list pages (F1 maps, F2 monsters, and later sections share
 * it): the stone frame picture, the "DARK UNION" title, a right-aligned
 * section header, a fourteen-row window of names with the selected row in
 * gold, the MORE arrows, and the seven command icons along the bottom.
 * Geometry, colors and paging are identical on every page; the header text
 * and position, the rows and the icon selection differ per section.
 */

#define YT2_LIST_PALETTE 0u
#define YT2_LIST_PICTURE_GROUP 0u
#define YT2_LIST_PICTURE_INDEX 14u
#define YT2_LIST_PICTURE_X 1
#define YT2_LIST_PICTURE_Y 1

#define YT2_LIST_VISIBLE_ROWS 14u
#define YT2_LIST_X 40
#define YT2_LIST_Y 27
#define YT2_LIST_STEP 10
#define YT2_LIST_SELECTED_COLOR 0x8Au
#define YT2_LIST_TEXT_COLOR 0x0Au

#define YT2_LIST_TITLE "DARK UNION"
#define YT2_LIST_TITLE_X 6
#define YT2_LIST_TITLE_Y 4
#define YT2_LIST_TITLE_COLOR 0x0Du
#define YT2_LIST_HEADER_Y 4

#define YT2_LIST_MORE_X 129
#define YT2_LIST_MORE_Y 167
#define YT2_LIST_MORE_COLOR 0x0Du

#define YT2_LIST_ICON_GROUP 8u
#define YT2_LIST_ICON_X 62
#define YT2_LIST_ICON_Y 180
#define YT2_LIST_ICON_STEP 30
#define YT2_LIST_ICON_SIZE 16u
#define YT2_LIST_ICON_TRANSPARENT 0xFFu
#define YT2_LIST_ICON_COUNT 7u

/* Row text callback: write row `rank` NUL-terminated into `buffer`. */
typedef int (*yt2_list_row_text)(void *ctx, unsigned int rank, char *buffer,
                                 size_t size);

typedef struct yt2_list_page {
    const char *header;
    int header_x;
    /* Left title override (the F5 details show the section instead of
       YT2_LIST_TITLE); NULL keeps "DARK UNION" at YT2_LIST_TITLE_X. */
    const char *title;
    int title_x;
    const unsigned int *icons;
    unsigned int count;
    unsigned int selected;
    yt2_list_row_text row_text;
    void *ctx;
} yt2_list_page;

/*
 * Compose the page with `selected` (a rank) selected; the fourteen-row
 * window starts at the selected row's page.  Returns YT2_STATUS_ERROR for
 * missing inputs, an out-of-range selection or missing picture data.
 */
int yt2_list_render(yt2_surface *surface, const yt2_pictures *pictures,
                    const yt2_list_page *page);

#endif /* YT2_LISTS_H */
