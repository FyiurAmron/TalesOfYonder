#ifndef YT2_MONSTER_DETAIL_H
#define YT2_MONSTER_DETAIL_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/monsters.h"
#include "yt2/pictures.h"
#include "yt2/surface.h"

/*
 * The F2 monster detail view (MONSTER STATISTICS).
 *
 * Layout (all reverse-engineered from the B1 oracle, geometry confirmed
 * against sub_04022 in the verified assembly): the stone background
 * (group 0, picture 13), the monster name left and the section header
 * right on the title row, the animated portrait on the left, four
 * BCD experience/treasure counters, seven plain statistics, ten
 * IMMUNE flags, the MAGIC DAMAGE and PHYSICAL resistance rows, the
 * SPECIAL abilities line, and the standard F2 footer and icon bar.
 *
 * Portraits animate: six pictures cycle or ping-pong from the record's
 * base picture (word 0), with a two-picture excursion (base+7, held
 * base+8) about every 360 ticks.  Base+6 never shows.  Group-3
 * portraits fill the 190x110 box; group-2 portraits (140x155) blit
 * unclipped and may overdraw the title.  Every portrait passes through
 * a per-monster high-nibble remap table; the low nibble is preserved
 * and 0xFF stays transparent.
 */

#define YT2_DETAIL_PICTURE_GROUP 0u
#define YT2_DETAIL_PICTURE_INDEX 13u
#define YT2_DETAIL_PICTURE_X 1
#define YT2_DETAIL_PICTURE_Y 1

#define YT2_DETAIL_NAME_X 6
#define YT2_DETAIL_TITLE_Y 4
#define YT2_DETAIL_TITLE_COLOR 0x0Du

#define YT2_DETAIL_PORTRAIT_X 6
#define YT2_DETAIL_PORTRAIT_Y 33
#define YT2_DETAIL_PORTRAIT_W 190u
#define YT2_DETAIL_PORTRAIT_H 110u
#define YT2_DETAIL_PORTRAIT_TRANSPARENT 0xFFu

#define YT2_DETAIL_FOOTER_TEXT "d LIST"
#define YT2_DETAIL_FOOTER_X 11
#define YT2_DETAIL_FOOTER_Y 185
#define YT2_DETAIL_FOOTER_COLOR 0x0Fu

#define YT2_DETAIL_LABEL_COLOR 0x0Au
#define YT2_DETAIL_DWORD_COLOR 0x8Au
#define YT2_DETAIL_WORD_COLOR 0x59u
#define YT2_DETAIL_FLAG_COLOR 0xA7u
#define YT2_DETAIL_SPECIAL_COLOR 0xCAu

/* Nominal animation cadence in guest ticks (see the design notes). */
#define YT2_DETAIL_STEP_TICKS 5u
#define YT2_DETAIL_EXCURSION_PERIOD 360u
#define YT2_DETAIL_EXCURSION_LENGTH 16u

/*
 * Compose the detail view for `rank` showing portrait picture `pic`
 * (an absolute picture number in the monster's group).  Returns
 * YT2_STATUS_ERROR for missing inputs, an out-of-range rank or a
 * picture the group does not hold.
 */
int yt2_monster_detail_render(yt2_surface *surface,
                             const yt2_pictures *pictures,
                             const yt2_monsters *monsters, unsigned int rank,
                             unsigned int pic);

/*
 * Nominal portrait picture for `rank` at `tick` guest ticks after the
 * detail opened: the frozen cycle/ping-pong table over base..base+5
 * with the base+7/base+8 excursion every period.  Step edges are
 * nominal (the oracle varies by a tick or two); the tables and the
 * excursion pictures are exact.
 */
unsigned int yt2_monster_detail_phase(const yt2_monsters *monsters,
                                     unsigned int rank, unsigned int tick);

#endif /* YT2_MONSTER_DETAIL_H */
