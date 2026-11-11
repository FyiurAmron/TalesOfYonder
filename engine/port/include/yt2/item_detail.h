#ifndef YT2_ITEM_DETAIL_H
#define YT2_ITEM_DETAIL_H

#include <stdint.h>

#include "yt2/items.h"
#include "yt2/pictures.h"
#include "yt2/surface.h"

/*
 * The F5 item page (Enter on an item row).
 *
 * Layout, strings, colors, number formats and behaviors below are read
 * straight from the verified disassembly (item mains fn_02D9E, fn_02E27
 * and fn_02E9B with bodies sub_0342A/034B9/03532/03575/035CE/0360E/037A0/
 * 04733/047DD/04826/048D0, value formatters fn_08070/062EE/09B42/09C46
 * and the +N setup sub_04573); pixel geometry doubles as a cross-check.
 *
 * Records live at WORLD.DAT 0x71138 + file*58: 19 stat bytes followed
 * by three 13-byte names.  The per-type parameter rows come from three
 * WORLD tables selected by the +0C flag word (0x7C81E/0x7DCD2/0x7D502
 * plus the struct+0 byte offset); the PROTECTIONS/ADDS lists from
 * 0x7BD2E plus the struct+2 offset.  All table bases are verified
 * against B1 checkpoints, 2-5 anchors each.
 *
 * On armor and weapons pages the +N row offers the consecutive file+N
 * variant records (up to 11, gated per record on the parameter-row
 * flag bits); clicking +N shows that variant's record with box N
 * selected.  Other categories have neither the row nor the input.
 */

#define YT2_ITEM_STRUCT_OFFSET 0x71138u
#define YT2_ITEM_PARAM_AF0 0x7C81Eu
#define YT2_ITEM_PARAM_FA4 0x7DCD2u
#define YT2_ITEM_PARAM_17D4 0x7D502u
#define YT2_ITEM_ADDS_OFFSET 0x7BD2Eu

#define YT2_ITEM_MAX_PLUS 11u

/* Compose the item page for `file` (a 0..743 record) as opened from
 * `category`, with +N box `plus` selected, or YT2_STATUS_ERROR when
 * out of range (unknown category, bad file, or `plus` past the last
 * enabled box). */
int yt2_item_page_render(yt2_surface *surface, const yt2_pictures *pictures,
                         const yt2_items *items, unsigned int file,
                         unsigned int category, unsigned int plus);

/* How many +N boxes the page offers (0 when the record has none). */
unsigned int yt2_item_plus_count(const yt2_items *items, unsigned int file,
                                unsigned int category);

#endif /* YT2_ITEM_DETAIL_H */
