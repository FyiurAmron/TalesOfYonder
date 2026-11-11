#include "yt2/cursor.h"

int yt2_cursor_draw(yt2_surface *surface, const yt2_pictures *pictures,
                    unsigned int index, int x, int y)
{
    const uint8_t *image;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL) {
        return YT2_STATUS_ERROR;
    }
    image = yt2_picture_data(pictures, YT2_CURSOR_GROUP, index);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    return yt2_surface_blit_transparent(surface, x, y, image, YT2_CURSOR_SIZE,
                                       YT2_CURSOR_SIZE,
                                       YT2_CURSOR_TRANSPARENT);
}
