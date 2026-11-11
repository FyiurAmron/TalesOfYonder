#ifndef YT2_WALKTHROUGH_H
#define YT2_WALKTHROUGH_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The clue-book complete walk through (F6): 31 pages of fifty-character
 * text lines.
 *
 * WORLD.DAT at 159158h holds 31 pages of 25 lines of 51 bytes (fifty
 * characters plus NUL, space-padded).  The last line of every page is
 * its "N OF 31" footer, typos included (pages 20-31 read "N 0F 31").
 * Percent and tilde render literally: the font's % glyph just looks like
 * a quote mark.  Another nine-page document (ordering info) follows the
 * walkthrough in the same format but no clue-book screen shows it.
 */

#define YT2_WALKTHROUGH_OFFSET 0x159158u
#define YT2_WALKTHROUGH_PAGES 31u
#define YT2_WALKTHROUGH_LINES 25u
#define YT2_WALKTHROUGH_LINE_SIZE 50u
#define YT2_WALKTHROUGH_RECORD_SIZE 51u

#define YT2_WALKTHROUGH_HEADER "COMPLETE WALK THROUGH"
#define YT2_WALKTHROUGH_HEADER_X 188

typedef struct yt2_walkthrough {
    const uint8_t *bytes;
    size_t size;
} yt2_walkthrough;

/*
 * Index the walkthrough out of a loaded world.  The index borrows the
 * world's bytes: the world must outlive the walkthrough.  Validates every
 * line's terminator; the shipped file parses cleanly.
 */
int yt2_walkthrough_load(yt2_walkthrough *walk, const yt2_world *world);

/* One page line (verbatim, right-trimmed), or YT2_STATUS_ERROR. */
int yt2_walkthrough_line(const yt2_walkthrough *walk, unsigned int page,
                         unsigned int line, char *buffer, size_t size);

/* Compose page `page` (a 0-based rank). */
int yt2_walkthrough_render(yt2_surface *surface,
                           const yt2_pictures *pictures,
                           const yt2_walkthrough *walk, unsigned int page);

#endif /* YT2_WALKTHROUGH_H */
