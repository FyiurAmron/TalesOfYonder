#ifndef YT2_SPELL_DETAIL_H
#define YT2_SPELL_DETAIL_H

#include <stdint.h>

#include "yt2/pictures.h"
#include "yt2/spells.h"
#include "yt2/surface.h"

/*
 * The F3 spell detail view (SPELL INFORMATION).
 *
 * Layout (all reverse-engineered from the B1 oracle, 105/105 ranks
 * sampled): the stone background (group 0, picture 13), the spell name
 * left and the section header right on the title row, the class block
 * (up to three member classes with level and SCROLL/TRAINING/CREATION
 * type), the MP/NUORE/ORE costs from the record's words 1-3, the
 * AFFECTS scope/middle/target triple, the WHEN line, up to five EFFECT
 * lines over the record-text face (normal glyphs plus eighteen private
 * symbol glyphs lifted from the executable's second font bank), and the
 * standard footer and icon bar.
 */

#define YT2_SPELL_DETAIL_PICTURE_GROUP 0u
#define YT2_SPELL_DETAIL_PICTURE_INDEX 13u
#define YT2_SPELL_DETAIL_PICTURE_X 1
#define YT2_SPELL_DETAIL_PICTURE_Y 1

#define YT2_SPELL_DETAIL_NAME_X 6
#define YT2_SPELL_DETAIL_HEADER_X 213
#define YT2_SPELL_DETAIL_TITLE_Y 4
#define YT2_SPELL_DETAIL_TITLE_COLOR 0x0Du

#define YT2_SPELL_DETAIL_LABEL_COLOR 0x0Au
#define YT2_SPELL_DETAIL_TEXT_COLOR 0x0Du
#define YT2_SPELL_DETAIL_SCROLL_COLOR 0xCAu
#define YT2_SPELL_DETAIL_TRAINING_COLOR 0x8Au
#define YT2_SPELL_DETAIL_CREATION_COLOR 0xA7u

#define YT2_SPELL_DETAIL_CLASS_LABEL_X 44
#define YT2_SPELL_DETAIL_LEVEL_LABEL_X 104
#define YT2_SPELL_DETAIL_CLASS_LABEL_Y 28
#define YT2_SPELL_DETAIL_ROW_Y 34
#define YT2_SPELL_DETAIL_ROW_STEP 6
#define YT2_SPELL_DETAIL_CLASS_X 44
#define YT2_SPELL_DETAIL_LEVEL_X 104
#define YT2_SPELL_DETAIL_TYPE_X 122

#define YT2_SPELL_DETAIL_MP_LABEL_X 218
#define YT2_SPELL_DETAIL_MP_Y 40
#define YT2_SPELL_DETAIL_NUORE_LABEL_X 200
#define YT2_SPELL_DETAIL_NUORE_Y 46
#define YT2_SPELL_DETAIL_ORE_LABEL_X 212
#define YT2_SPELL_DETAIL_ORE_Y 52
#define YT2_SPELL_DETAIL_VALUE_X 236

#define YT2_SPELL_DETAIL_AFFECTS_LABEL_X 44
#define YT2_SPELL_DETAIL_AFFECTS_Y 76
#define YT2_SPELL_DETAIL_SCOPE_X 92
#define YT2_SPELL_DETAIL_MIDDLE_X 116

#define YT2_SPELL_DETAIL_WHEN_LABEL_X 44
#define YT2_SPELL_DETAIL_WHEN_Y 88
#define YT2_SPELL_DETAIL_WHEN_X 74

#define YT2_SPELL_DETAIL_EFFECT_LABEL_X 44
#define YT2_SPELL_DETAIL_EFFECT_Y 100
#define YT2_SPELL_DETAIL_EFFECT_X 44
#define YT2_SPELL_DETAIL_EFFECT_FIRST_Y 106
#define YT2_SPELL_DETAIL_EFFECT_STEP 6
#define YT2_SPELL_DETAIL_EFFECT_SLOTS 5u

#define YT2_SPELL_DETAIL_FOOTER_TEXT "d LIST"
#define YT2_SPELL_DETAIL_FOOTER_X 11
#define YT2_SPELL_DETAIL_FOOTER_Y 185
#define YT2_SPELL_DETAIL_FOOTER_COLOR 0x0Fu

/* Compose the detail page for `rank`, or YT2_STATUS_ERROR out of range. */
int yt2_spell_detail_render(yt2_surface *surface,
                            const yt2_pictures *pictures,
                            const yt2_spells *spells, unsigned int rank);

/*
 * The same page with EFFECT lines read live from WORLD files 6+7
 * (yt2_spells_effect_lines) instead of the frozen vanilla bytes: the
 * spelltext patch (tools/yt2_spelltext.py) shows through, while a vanilla
 * world renders pixel-identical to the frozen table.  Validates the
 * world bytes like the reader does.
 */
int yt2_spell_detail_render_world(yt2_surface *surface,
                                  const yt2_pictures *pictures,
                                  const yt2_spells *spells,
                                  unsigned int rank);

/*
 * Compose the detail page for a file index (the F4 spell lists address
 * spells this way; file 86, BEAM OF DEATH, has no F3 rank).  Nonzero
 * `f4_icons` selects the F4 icon bar, zero the F3 one.
 */
int yt2_spell_detail_render_index(yt2_surface *surface,
                                  const yt2_pictures *pictures,
                                  const yt2_spells *spells,
                                  unsigned int file_index, int f4_icons);

/* File-index form of yt2_spell_detail_render_world (see above). */
int yt2_spell_detail_render_index_world(yt2_surface *surface,
                                        const yt2_pictures *pictures,
                                        const yt2_spells *spells,
                                        unsigned int file_index,
                                        int f4_icons);

#endif /* YT2_SPELL_DETAIL_H */
