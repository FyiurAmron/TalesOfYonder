#include "yt2/cluebook.h"

#include "yt2/text.h"

#define CLUEBOOK_PICTURE_X 1
#define CLUEBOOK_PICTURE_Y 1

#define CLUEBOOK_TITLE_X 6
#define CLUEBOOK_TITLE_Y 4
#define CLUEBOOK_TITLE_COLOR 0x0Du
#define CLUEBOOK_HELP_X 249
#define CLUEBOOK_HELP_COLOR 0x0Du

#define CLUEBOOK_TAB_X 21
#define CLUEBOOK_TAB_Y 24
#define CLUEBOOK_TAB_COLOR 0x59u

#define CLUEBOOK_LIST_X 16
#define CLUEBOOK_LIST_Y 60
#define CLUEBOOK_LIST_LINE 6
#define CLUEBOOK_LIST_COLOR 0x08u

#define CLUEBOOK_ICON_X 62
#define CLUEBOOK_ICON_Y 180
#define CLUEBOOK_ICON_STEP 30
#define CLUEBOOK_ICON_SIZE 16u
#define CLUEBOOK_ICON_TRANSPARENT 0xFFu
#define CLUEBOOK_ICON_COUNT 7u

static const char *const cluebook_title = "DARK UNION : THE ON-LINE CLUE BOOK";
static const char *const cluebook_help = "HELP SCREEN";
static const char *const cluebook_tab =
    "** PRESS TAB AT ANY TIME TO SEE THIS SCREEN **";

static const char *const cluebook_lines[] = {
    " F1 MAPS (WORLD, TOWNS, MINES, ETC.)",
    "",
    " F2 MONSTER STATISTICS",
    "",
    " F3 SPELLS (INFORMATION ON ALL SPELLS)",
    "",
    " F4 MAGIC USERS (SPELLS BY CLASS)",
    "",
    " F5 INVENTORY ITEMS (ARMOR, POTIONS, FOOD, ETC.)",
    "",
    " F6 COMPLETE WALK THROUGH OF THE GAME",
    "",
    "ESC RETURN TO GAME"
};

/* fn_04A81's command bar: normal and highlighted variants are adjacent. */
static const unsigned int cluebook_icons[CLUEBOOK_ICON_COUNT] = {
    32u, 325u, 327u, 339u, 329u, 331u, 333u
};

int yt2_cluebook_render(yt2_surface *surface, const yt2_pictures *pictures)
{
    const yt2_picture_group *group;
    const uint8_t *image;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL) {
        return YT2_STATUS_ERROR;
    }
    group = yt2_pictures_group(pictures, YT2_CLUEBOOK_PICTURE_GROUP);
    image = yt2_picture_data(pictures, YT2_CLUEBOOK_PICTURE_GROUP,
                             YT2_CLUEBOOK_PICTURE_INDEX);
    if (group == NULL || image == NULL) {
        return YT2_STATUS_ERROR;
    }

    yt2_surface_clear(surface, 0u);
    if (yt2_surface_blit(surface, CLUEBOOK_PICTURE_X, CLUEBOOK_PICTURE_Y,
                         image, group->width, group->height) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }

    yt2_text_draw(surface, CLUEBOOK_TITLE_X, CLUEBOOK_TITLE_Y, cluebook_title,
                  CLUEBOOK_TITLE_COLOR);
    yt2_text_draw(surface, CLUEBOOK_HELP_X, CLUEBOOK_TITLE_Y, cluebook_help,
                  CLUEBOOK_HELP_COLOR);
    yt2_text_draw(surface, CLUEBOOK_TAB_X, CLUEBOOK_TAB_Y, cluebook_tab,
                  CLUEBOOK_TAB_COLOR);
    for (size_t line = 0;
         line < sizeof(cluebook_lines) / sizeof(cluebook_lines[0]); line++) {
        yt2_text_draw(surface, CLUEBOOK_LIST_X,
                      CLUEBOOK_LIST_Y + (int)line * CLUEBOOK_LIST_LINE,
                      cluebook_lines[line], CLUEBOOK_LIST_COLOR);
    }

    for (unsigned int index = 0; index < CLUEBOOK_ICON_COUNT; index++) {
        const uint8_t *icon =
            yt2_picture_data(pictures, 8u, cluebook_icons[index]);

        if (icon == NULL) {
            return YT2_STATUS_ERROR;
        }
        if (yt2_surface_blit_transparent(
                surface, CLUEBOOK_ICON_X + (int)index * CLUEBOOK_ICON_STEP,
                CLUEBOOK_ICON_Y, icon, CLUEBOOK_ICON_SIZE,
                CLUEBOOK_ICON_SIZE, CLUEBOOK_ICON_TRANSPARENT) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    return YT2_STATUS_OK;
}
