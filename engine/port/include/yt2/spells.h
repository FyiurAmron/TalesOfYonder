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

/*
 * The detail page's EFFECT text: WORLD files 6+7.  File 6 is the 4-byte
 * (start, count) index (slot file+1, slot 0 is the (0,0) header); file 7
 * holds 39-byte NUL-terminated text records.  Vanilla's text area was
 * never authored (script bytes and stray names); the spelltext patch
 * (tools/yt2_spelltext.py) fills it with YT3's descriptions.  The shipped
 * tables peak at 8 lines (DISPEL ILLUSION).
 */
#define YT2_SPELLS_EFFECT_INDEX 0x16762Eu
#define YT2_SPELLS_EFFECT_TEXT 0x167822u
#define YT2_SPELLS_EFFECT_REC 39u
#define YT2_SPELLS_EFFECT_MAX_LINES 8u

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
 * EFFECT text lines for a file index, read live from WORLD files 6+7
 * (slot file+1): at most YT2_SPELLS_EFFECT_MAX_LINES NUL-terminated lines
 * of up to 38 bytes plus the terminator, `count` set to the lines read.
 * Counts above the maximum are clamped; returns YT2_STATUS_ERROR out of
 * range or when the world ends mid-table.  Raw record bytes (high bytes
 * included): the detail renderer canonicalizes them for the text face.
 */
int yt2_spells_effect_lines(const yt2_spells *spells, unsigned int file_index,
                            char lines[YT2_SPELLS_EFFECT_MAX_LINES]
                                       [YT2_SPELLS_EFFECT_REC],
                            unsigned int *count);

/*
 * Compose the information page with `selected` (a rank) selected.
 * The fourteen-row window starts at the selected rank's page.
 */
int yt2_spells_render(yt2_surface *surface, const yt2_pictures *pictures,
                      const yt2_spells *spells, unsigned int selected);

#endif /* YT2_SPELLS_H */
