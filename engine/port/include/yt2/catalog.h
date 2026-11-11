#ifndef YT2_CATALOG_H
#define YT2_CATALOG_H

#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The clue-book map catalog (F1).
 *
 * Picture 14 of group 0 at (1,1) is the stone frame; the overlay is the
 * "DARK UNION" / "MAPS" header, a fourteen-row window of the game's curated
 * map list, the selected row in gold, the MORE arrows, and the seven command
 * icons along the bottom.  The list order is the executable's table at
 * DGROUP:0E95Ch: a count word (0x37) and fifty-five DWORD entries in reading
 * order, not all WORLD.DAT maps and not alphabetical.  Five entries carry
 * flags bit 15 (see catalog.c); every rank opens regardless.
 */

#define YT2_CATALOG_COUNT 55u
#define YT2_CATALOG_HEADER "MAPS"
#define YT2_CATALOG_HEADER_X 291

/* Number of rows in the clue-book list (48h in the original). */
unsigned int yt2_catalog_count(void);

/* Map descriptor index of a list row, or YT2_WORLD_MAPS_COUNT out of range. */
unsigned int yt2_catalog_map(unsigned int rank);

/* Zero-based position of `map` in the catalog, or -1 when it is not listed. */
int yt2_catalog_rank(unsigned int map);

/*
 * Compose the catalog page with `selected` (a map descriptor index) selected.
 * The fourteen-row window starts at the selected row's page.  Returns
 * YT2_STATUS_ERROR for an unlisted map, a broken name or missing picture data.
 */
int yt2_catalog_render(yt2_surface *surface, const yt2_pictures *pictures,
                       const yt2_world *world, unsigned int selected);

#endif /* YT2_CATALOG_H */
