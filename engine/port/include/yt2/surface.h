#ifndef YT2_SURFACE_H
#define YT2_SURFACE_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/status.h"

/*
 * An 8-bit indexed drawing surface.  The engine draws into one of these and
 * the host boundary later turns it into protocol frames; there is no screen
 * segment or video memory in the model.
 */

typedef struct yt2_surface {
    uint8_t *pixels;
    unsigned int width;
    unsigned int height;
    size_t stride;
} yt2_surface;

int yt2_surface_init(yt2_surface *surface, unsigned int width,
                     unsigned int height);
void yt2_surface_free(yt2_surface *surface);

void yt2_surface_clear(yt2_surface *surface, uint8_t value);

/* Copy a tightly packed image, clipping at the surface edges. */
int yt2_surface_blit(yt2_surface *surface, int x, int y, const uint8_t *source,
                     unsigned int width, unsigned int height);

/* As above, skipping source pixels equal to `transparent`. */
int yt2_surface_blit_transparent(yt2_surface *surface, int x, int y,
                                 const uint8_t *source, unsigned int width,
                                 unsigned int height, uint8_t transparent);

#endif /* YT2_SURFACE_H */
