#include "yt2/catalog.h"

#include <stddef.h>
#include <stdint.h>

#include "yt2/lists.h"
#include "yt2/status.h"

/*
 * The clue book's map list is a curated table in the executable, not a scan of
 * the WORLD.DAT descriptors: it is in reading order (not alphabetical - the
 * BLACKWING levels come before BLACKWING APEX) and leaves out the side maps
 * that have no overhead view.  DGROUP:0E95Ch holds the count word (0x37) and
 * then one DWORD per entry: the one-based map descriptor index and a flags
 * word.  Five entries carry flags bit 15 (the PORT HOPE town maps at ranks
 * 30..32 and two Yendor sea maps at ranks 53..54); the bit does not gate
 * the clue book (every rank opens its overhead view), so B2 ignores it.
 */
static const uint32_t catalog_table[YT2_CATALOG_COUNT] = {
    0x00000044u, 0x0000001Fu, 0x00000020u, 0x00000021u, 0x00000022u,
    0x00000049u, 0x00000032u, 0x00000043u, 0x0000005Du, 0x00000046u,
    0x0000005Cu, 0x0000005Eu, 0x0000005Bu, 0x0000003Eu, 0x0000003Fu,
    0x00000040u, 0x00000041u, 0x00000052u, 0x00000053u, 0x00000054u,
    0x00000055u, 0x00000033u, 0x00000034u, 0x00000037u, 0x00000038u,
    0x0000002Fu, 0x00000030u, 0x00000031u, 0x00000023u, 0x00000024u,
    0x80000019u, 0x8000001Au, 0x8000001Bu, 0x0000004Bu, 0x0000004Cu,
    0x00000057u, 0x00000058u, 0x00000059u, 0x0000005Au, 0x00000035u,
    0x00000036u, 0x0000004Eu, 0x00000062u, 0x00000063u, 0x0000001Cu,
    0x0000001Du, 0x00000025u, 0x00000026u, 0x00000027u, 0x0000002Du,
    0x0000002Eu, 0x00000016u, 0x00000017u, 0x8000002Au, 0x8000002Bu
};

static const unsigned int catalog_icons[YT2_LIST_ICON_COUNT] = {
    33u, 325u, 327u, 339u, 329u, 331u, 333u
};

#define CATALOG_MAP_MASK 0x0000FFFFu

unsigned int yt2_catalog_count(void)
{
    return YT2_CATALOG_COUNT;
}

unsigned int yt2_catalog_map(unsigned int rank)
{
    return rank < YT2_CATALOG_COUNT
               ? (unsigned int)(catalog_table[rank] & CATALOG_MAP_MASK) - 1u
               : YT2_WORLD_MAPS_COUNT;
}

int yt2_catalog_rank(unsigned int map)
{
    for (unsigned int rank = 0; rank < YT2_CATALOG_COUNT; rank++) {
        if ((unsigned int)(catalog_table[rank] & CATALOG_MAP_MASK) - 1u ==
            map) {
            return (int)rank;
        }
    }
    return -1;
}

static int catalog_row_text(void *ctx, unsigned int rank, char *buffer,
                            size_t size)
{
    const yt2_world *world = (const yt2_world *)ctx;

    return yt2_world_map_display_name(world, yt2_catalog_map(rank), buffer,
                                      size);
}

int yt2_catalog_render(yt2_surface *surface, const yt2_pictures *pictures,
                       const yt2_world *world, unsigned int selected)
{
    yt2_list_page page;
    int rank;

    if (surface == NULL || surface->pixels == NULL || pictures == NULL ||
        world == NULL) {
        return YT2_STATUS_ERROR;
    }
    rank = yt2_catalog_rank(selected);
    if (rank < 0) {
        return YT2_STATUS_ERROR;
    }
    page.header = YT2_CATALOG_HEADER;
    page.title = NULL;
    page.title_x = 0;
    page.header_x = YT2_CATALOG_HEADER_X;
    page.icons = catalog_icons;
    page.count = YT2_CATALOG_COUNT;
    page.selected = (unsigned int)rank;
    page.row_text = catalog_row_text;
    page.ctx = (void *)world;
    return yt2_list_render(surface, pictures, &page);
}
