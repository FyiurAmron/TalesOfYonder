#ifndef YT2_CURSOR_H
#define YT2_CURSOR_H

#include "yt2/pictures.h"
#include "yt2/surface.h"

/*
 * The software mouse cursor.
 *
 * The game keeps one 16x16 cursor image in DS:3FE0h, loaded by
 * fn_SetMouseCursor from PICTURES.VGA class 8 (block size 256 at file offset
 * C4A37A = group 8), so the cursor id is the group-8 picture index.  The
 * everyday pointer is the hand, cursor 0x1D.  The position the game draws at
 * (v_MouseX/v_MouseY) already includes the hotspot: the 8-pixel shift for
 * cursors other than 0/1Dh is applied when the position moves, not when the
 * image is drawn, so drawing is a plain masked blit at the position.
 *
 * B2 takes the index and the position as inputs: pointer tracking belongs to
 * the future display/input layer, not to the screen composers.
 */

#define YT2_CURSOR_GROUP 8u
#define YT2_CURSOR_SIZE 16u
#define YT2_CURSOR_TRANSPARENT 0xFFu
#define YT2_CURSOR_HAND 0x1Du

/*
 * Draw cursor `index` with its top-left corner at (`x`, `y`), skipping
 * transparent pixels.  Returns YT2_STATUS_ERROR for a missing picture or
 * surface; out-of-range positions clip.  The original clips only the right
 * and bottom edges (at x=0 a cursor past the bottom row writes past the
 * 64 KB buffer, off the visible screen); B2 clips all four, which is
 * identical for every visible pixel.
 */
int yt2_cursor_draw(yt2_surface *surface, const yt2_pictures *pictures,
                    unsigned int index, int x, int y);

#endif /* YT2_CURSOR_H */
