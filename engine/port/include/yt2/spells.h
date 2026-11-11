#ifndef YT2_SPELLS_H
#define YT2_SPELLS_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The clue-book spell information (F3).
 *
 * WORLD.DAT holds 105 fixed 80-byte spell records: a 21-byte name field
 * (space-padded, NUL at byte 21) and 58 bytes of spell data.  The F3 list
 * shows the names fourteen rows per page with the same geometry as the
 * other clue-book lists; the gold icon is the scroll's.
 *
 * The list order is the original's own, quirks included: it is alphabetical
 * except that BLOCK OF POWER appears twice (ranks 5 and 11) and BEAM OF
 * DEATH never appears, so 105 ranks cover 104 distinct spells.  Both the
 * DOS screen and the B1 engine show the doubled rank (page one matches on
 * both), so the order below is a frozen table, verified rank by rank
 * against the B1 frames.
 */

#define YT2_SPELLS_OFFSET 0x1AA65Eu
#define YT2_SPELLS_COUNT 105u
#define YT2_SPELLS_RECORD_SIZE 80u
#define YT2_SPELLS_NAME_SIZE 21u

#define YT2_SPELLS_HEADER "SPELL INFORMATION"
#define YT2_SPELLS_HEADER_X 213

typedef struct yt2_spells {
    const uint8_t *bytes;
    size_t size;
    uint8_t order[YT2_SPELLS_COUNT];
} yt2_spells;

/*
 * Index the spell table out of a loaded world.  The index borrows the
 * world's bytes: the world must outlive the spells.  Validates every
 * record's name terminator; the shipped file parses cleanly.
 */
int yt2_spells_load(yt2_spells *spells, const yt2_world *world);

/* Ranked name ("ACID RAIN"), or YT2_STATUS_ERROR out of range. */
int yt2_spell_name(const yt2_spells *spells, unsigned int rank, char *buffer,
                   size_t size);

/* File-index name (for the per-class details), or YT2_STATUS_ERROR. */
int yt2_spell_name_by_index(const yt2_spells *spells, unsigned int index,
                            char *buffer, size_t size);

/* File record behind a rank (for the detail view's statistics). */
const uint8_t *yt2_spell_record(const yt2_spells *spells, unsigned int rank);

/*
 * Compose the information page with `selected` (a rank) selected.
 * The fourteen-row window starts at the selected rank's page.
 */
int yt2_spells_render(yt2_surface *surface, const yt2_pictures *pictures,
                      const yt2_spells *spells, unsigned int selected);

#endif /* YT2_SPELLS_H */
