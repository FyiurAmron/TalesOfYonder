#ifndef YT2_ITEMS_H
#define YT2_ITEMS_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The clue-book inventory items (F5): eight categories, six of them item
 * lists, two of them (attribute enhancers, transportations) fixed info
 * screens.
 *
 * WORLD.DAT at 7114Bh holds 744 fixed 58-byte item records: three 13-byte
 * name fields (12 characters plus NUL each; the shown name joins the
 * non-empty fields with single spaces) and 19 bytes of item data.  Which
 * of the 744 records each category shows is the original's fixed
 * assignment (no stat byte, pair or triple of bytes yields it), captured
 * item by item from the B1 detail screens; the orders are captured too,
 * quirks included (a swapped neighbor pair in armor, weapons and scrolls,
 * SAPHIRE trailing the jewels).
 */

#define YT2_ITEMS_OFFSET 0x7114Bu
#define YT2_ITEMS_COUNT 744u
#define YT2_ITEMS_RECORD_SIZE 58u
#define YT2_ITEMS_NAME_FIELDS 3u
#define YT2_ITEMS_NAME_FIELD_SIZE 13u

#define YT2_ITEMS_CATEGORIES 8u
#define YT2_ITEMS_ARMOR 0u
#define YT2_ITEMS_ATTRIBUTE 1u
#define YT2_ITEMS_JEWELS 2u
#define YT2_ITEMS_SCROLLS 3u
#define YT2_ITEMS_POTIONS 4u
#define YT2_ITEMS_SUPPLIES 5u
#define YT2_ITEMS_TRANSPORT 6u
#define YT2_ITEMS_WEAPONS 7u

#define YT2_ITEMS_HEADER "INVENTORY ITEMS"
#define YT2_ITEMS_HEADER_X 225
#define YT2_ITEMS_DETAIL_TITLE "INVENTORY ITEMS"
#define YT2_ITEMS_DETAIL_TITLE_X 6
#define YT2_ITEMS_DETAIL_FOOTER "d LIST"
#define YT2_ITEMS_DETAIL_FOOTER_X 11
#define YT2_ITEMS_DETAIL_FOOTER_Y 185
#define YT2_ITEMS_DETAIL_FOOTER_COLOR 0x0Fu

typedef struct yt2_items {
    const uint8_t *bytes;
    size_t size;
} yt2_items;

/*
 * Index the item table out of a loaded world.  The index borrows the
 * world's bytes: the world must outlive the items.  Validates every
 * record's name terminators; the shipped file parses cleanly.
 */
int yt2_items_load(yt2_items *items, const yt2_world *world);

/* How many items category `category` lists (0 for the info screens). */
unsigned int yt2_items_category_count(unsigned int category);

/* Category row ("ARMOR/RINGS"), or YT2_STATUS_ERROR out of range. */
int yt2_items_category_name(unsigned int category, char *buffer, size_t size);

/* File-index name ("BROAD SWORD"), or YT2_STATUS_ERROR out of range. */
int yt2_item_name(const yt2_items *items, unsigned int index, char *buffer,
                  size_t size);

/* Ranked item name within a category, or YT2_STATUS_ERROR out of range. */
int yt2_items_detail_name(const yt2_items *items, unsigned int category,
                          unsigned int rank, char *buffer, size_t size);

/* File index behind (category, rank), or YT2_STATUS_ERROR out of range. */
int yt2_items_member_file(const yt2_items *items, unsigned int category,
                          unsigned int rank, unsigned int *file);

/* Compose the eight-row category list with `selected` selected. */
int yt2_items_render_list(yt2_surface *surface, const yt2_pictures *pictures,
                          unsigned int selected);

/*
 * Compose category `category`'s item page with `selected` (a rank)
 * selected.  The fourteen-row window starts at the selected rank's page.
 */
int yt2_items_render_detail(yt2_surface *surface,
                            const yt2_pictures *pictures,
                            const yt2_items *items, unsigned int category,
                            unsigned int selected);

/* Compose the attribute-enhancers info screen (category 1). */
int yt2_items_render_attribute(yt2_surface *surface,
                               const yt2_pictures *pictures);

/* Compose the transportations info screen (category 6). */
int yt2_items_render_transport(yt2_surface *surface,
                               const yt2_pictures *pictures);

#endif /* YT2_ITEMS_H */
