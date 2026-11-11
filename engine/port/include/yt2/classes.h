#ifndef YT2_CLASSES_H
#define YT2_CLASSES_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/pictures.h"
#include "yt2/spells.h"
#include "yt2/surface.h"

/*
 * The clue-book magic users (F4): six class lines, each with its spells.
 *
 * The executable carries three tiers of nine class names (ten bytes padded,
 * NUL at byte 10): tier 1 at 29625h, tier 2 at 2A0D7h, tier 3 at 2A13Ah.
 * The F4 list shows the six magic lines (base classes 3-8, the fighter /
 * merchant / rogue line stays out as non-magic) as "TIER1 / TIER2 / TIER3"
 * triples; Enter opens the line's spells, byte-sorted, fourteen rows per
 * page with a "+ LIST" footer.
 *
 * Which spells each line casts is the original's own fixed assignment (no
 * per-spell mask field yields it; every combination tried fails on at
 * least one spell), captured spell by spell from the B1 detail screens,
 * so the membership below is a frozen table while the alphabetical order
 * is re-derived by sorting at load.  Unlike the F3 screen the details are
 * a true sort: the mage list holds BEAM OF DEATH in its sorted slot and
 * the druid list holds BLOCK OF POWER exactly once.
 */

#define YT2_CLASSES_COUNT 6u
#define YT2_CLASSES_BASE_FIRST 3u
#define YT2_CLASSES_MAX_MEMBERS 52u

#define YT2_CLASSES_HEADER "MAGIC USER INFORMATION"
#define YT2_CLASSES_HEADER_X 183

#define YT2_CLASSES_DETAIL_HEADER "SPELL INFORMATION"
#define YT2_CLASSES_DETAIL_HEADER_X 213
/* 'd' is the font's left arrow, kin to the MORE 'a'/'b' arrows. */
#define YT2_CLASSES_DETAIL_FOOTER "d LIST"
#define YT2_CLASSES_DETAIL_FOOTER_X 11
#define YT2_CLASSES_DETAIL_FOOTER_Y 185
#define YT2_CLASSES_DETAIL_FOOTER_COLOR 0x0Fu

typedef struct yt2_classes {
    const yt2_spells *spells;
    uint8_t members[YT2_CLASSES_COUNT][YT2_CLASSES_MAX_MEMBERS];
    uint8_t counts[YT2_CLASSES_COUNT];
} yt2_classes;

/*
 * Index the six class lines.  Borrows the spells (for the names): the
 * spells must outlive the classes.  Sorts every line's members by name.
 */
int yt2_classes_load(yt2_classes *classes, const yt2_spells *spells);

/* How many spells line `class` casts, or 0 out of range. */
unsigned int yt2_class_count(const yt2_classes *classes, unsigned int class);

/* List row ("MONK / CLERIC / PRIEST"), or YT2_STATUS_ERROR out of range. */
int yt2_class_row_text(unsigned int class, char *buffer, size_t size);

/* Ranked spell name within a line, or YT2_STATUS_ERROR out of range. */
int yt2_class_spell_name(const yt2_classes *classes, unsigned int class,
                         unsigned int rank, char *buffer, size_t size);

/* Compose the six-row line list with `selected` (a line) selected. */
int yt2_classes_render_list(yt2_surface *surface,
                            const yt2_pictures *pictures,
                            const yt2_classes *classes, unsigned int selected);

/*
 * Compose line `class`'s spell page with `selected` (a rank) selected.
 * The fourteen-row window starts at the selected rank's page.
 */
int yt2_classes_render_detail(yt2_surface *surface,
                              const yt2_pictures *pictures,
                              const yt2_classes *classes, unsigned int class,
                              unsigned int selected);

#endif /* YT2_CLASSES_H */
