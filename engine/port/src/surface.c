#include "yt2/surface.h"

#include <stdlib.h>
#include <string.h>

int yt2_surface_init(yt2_surface *surface, unsigned int width,
                     unsigned int height)
{
    uint8_t *pixels;

    if (surface == NULL || width == 0u || height == 0u) {
        return YT2_STATUS_ERROR;
    }
    memset(surface, 0, sizeof(*surface));
    if (width > (unsigned int)(SIZE_MAX / height)) {
        return YT2_STATUS_ERROR;
    }
    pixels = malloc((size_t)width * height);
    if (pixels == NULL) {
        return YT2_STATUS_ERROR;
    }
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = width;
    return YT2_STATUS_OK;
}

void yt2_surface_free(yt2_surface *surface)
{
    if (surface == NULL) {
        return;
    }
    free(surface->pixels);
    memset(surface, 0, sizeof(*surface));
}

void yt2_surface_clear(yt2_surface *surface, uint8_t value)
{
    if (surface == NULL || surface->pixels == NULL) {
        return;
    }
    memset(surface->pixels, value, surface->stride * surface->height);
}

int yt2_surface_blit(yt2_surface *surface, int x, int y, const uint8_t *source,
                     unsigned int width, unsigned int height)
{
    long long source_x = 0;
    long long source_y = 0;
    long long destination_x = x;
    long long destination_y = y;
    long long copy_width;
    long long copy_height;

    if (surface == NULL || surface->pixels == NULL || source == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (width == 0u || height == 0u) {
        return YT2_STATUS_OK;
    }
    if (destination_x < 0) {
        source_x = -destination_x;
        destination_x = 0;
    }
    if (destination_y < 0) {
        source_y = -destination_y;
        destination_y = 0;
    }
    if ((unsigned long long)source_x >= width ||
        (unsigned long long)source_y >= height ||
        (unsigned long long)destination_x >= surface->width ||
        (unsigned long long)destination_y >= surface->height) {
        return YT2_STATUS_OK;
    }

    copy_width = (long long)width - source_x;
    if (copy_width > (long long)surface->width - destination_x) {
        copy_width = (long long)surface->width - destination_x;
    }
    copy_height = (long long)height - source_y;
    if (copy_height > (long long)surface->height - destination_y) {
        copy_height = (long long)surface->height - destination_y;
    }
    if (copy_width <= 0 || copy_height <= 0) {
        return YT2_STATUS_OK;
    }

    for (long long row = 0; row < copy_height; row++) {
        const uint8_t *source_row =
            source + (size_t)(source_y + row) * width + (size_t)source_x;
        uint8_t *destination_row = surface->pixels +
                                   (size_t)(destination_y + row) *
                                       surface->stride +
                                   (size_t)destination_x;

        memcpy(destination_row, source_row, (size_t)copy_width);
    }
    return YT2_STATUS_OK;
}
