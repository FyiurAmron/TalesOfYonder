#ifndef YT2_MONSTERS_H
#define YT2_MONSTERS_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The clue-book monster statistics (F2).
 *
 * WORLD.DAT holds 61 fixed 106-byte monster records: two 13-byte name fields
 * (12 characters plus NUL; long names span both) and 80 bytes of statistics
 * for the detail view.  The F2 list shows the names byte-sorted, fourteen
 * rows per page with the same geometry as the map catalog; the gold icon is
 * the person's.
 */

#define YT2_MONSTERS_OFFSET 0x1A7994u
#define YT2_MONSTERS_COUNT 61u
#define YT2_MONSTERS_RECORD_SIZE 106u
#define YT2_MONSTERS_NAME_FIELDS 2u
#define YT2_MONSTERS_NAME_FIELD_SIZE 13u

#define YT2_MONSTERS_HEADER "MONSTER STATISTICS"
#define YT2_MONSTERS_HEADER_X 208

typedef struct yt2_monsters {
    const uint8_t *bytes;
    size_t size;
    uint8_t order[YT2_MONSTERS_COUNT];
} yt2_monsters;

/*
 * Index the monster table out of a loaded world.  The index borrows the
 * world's bytes: the world must outlive the monsters.  Validates every
 * record's name terminators; the shipped file parses cleanly.
 */
int yt2_monsters_load(yt2_monsters *monsters, const yt2_world *world);

/* Ranked name ("CARNIVOROUS SPIDER"), or YT2_STATUS_ERROR out of range. */
int yt2_monster_name(const yt2_monsters *monsters, unsigned int rank,
                     char *buffer, size_t size);

/* File record behind a rank (for the detail view's statistics). */
const uint8_t *yt2_monster_record(const yt2_monsters *monsters,
                                  unsigned int rank);

/*
 * Compose the statistics page with `selected` (a rank) selected.
 * The fourteen-row window starts at the selected rank's page.
 */
int yt2_monsters_render(yt2_surface *surface, const yt2_pictures *pictures,
                        const yt2_monsters *monsters, unsigned int selected);

#endif /* YT2_MONSTERS_H */
