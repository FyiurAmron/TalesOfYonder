#ifndef YT2_MAPVIEW_H
#define YT2_MAPVIEW_H

#include "yt2/pictures.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The clue-book overhead map (F1, then Enter on a map).
 *
 * The original builds this screen from four layers:
 *
 *   - black background;
 *   - the map title at (0,1) and "SELECT LEGEND OR ESC" at (201,1);
 *   - every tile of the 40x24 map window: a set WORLD.DAT record-1 bit
 *     draws the tile's terrain map icon and then its object icon (8x8
 *     pictures from group 9); a clear bit draws the dark void icon 13h;
 *   - the legend markers of the selected map, icon 73h from the same group.
 *
 * The map is a 320x200 indexed screen; palette 0 is the menu palette.  B2
 * presents the composed screen exactly as the DOS original does (F1, Enter on
 * a map); the B1 pseudo-port shows only the first icons there, a B1-only
 * presentation artifact worth no emulation.
 */

#define YT2_MAPVIEW_PALETTE 0u
#define YT2_MAPVIEW_PICTURE_GROUP 9u
#define YT2_MAPVIEW_ICON_SIZE 8u
#define YT2_MAPVIEW_ICON_TRANSPARENT 0xFFu
#define YT2_MAPVIEW_VOID_ICON 19u
#define YT2_MAPVIEW_LEGEND_ICON 115u

#define YT2_MAPVIEW_MAP_X 0
#define YT2_MAPVIEW_MAP_Y 8
#define YT2_MAPVIEW_TITLE_X 0
#define YT2_MAPVIEW_TITLE_Y 1
#define YT2_MAPVIEW_TITLE_COLOR 0x0Du
#define YT2_MAPVIEW_HELP_X 201
#define YT2_MAPVIEW_HELP_Y 1
#define YT2_MAPVIEW_HELP_COLOR 0x59u

/*
 * Legend selection overlay (sub_03F6B): clicking a legend marker redraws
 * icon 73h at (161,0) and prints the marker's 18 text bytes at (170,1)
 * in color 123 over black.  The text is opaque, so a shorter line erases
 * the previous line's tail; a click that hits no marker changes nothing.
 */
#define YT2_MAPVIEW_LEGEND_ICON_X 161
#define YT2_MAPVIEW_LEGEND_ICON_Y 0
#define YT2_MAPVIEW_LEGEND_TEXT_X 170
#define YT2_MAPVIEW_LEGEND_TEXT_Y 1
#define YT2_MAPVIEW_LEGEND_TEXT_COLOR 123u

/*
 * Compose the overhead map of descriptor `map` (0..119) into an already-sized
 * surface.  Returns YT2_STATUS_ERROR for a filler/unknown map, missing picture
 * data, or a WORLD.DAT image without the map/legend records.
 */
int yt2_map_view_render(yt2_surface *surface, const yt2_world *world,
                        const yt2_pictures *pictures, unsigned int map);

/*
 * Draw one legend line's overlay (marker icon plus text) over an already
 * composed map.  Returns YT2_STATUS_ERROR for missing inputs or an
 * unreadable line.
 */
int yt2_map_view_draw_legend(yt2_surface *surface,
                             const yt2_pictures *pictures,
                             const yt2_world *world, unsigned int line);

/*
 * Legend line whose 9x9 marker box (the 8x8 icon plus its inclusive far
 * edge, as hit-tested by the original) contains (`x`, `y`) on `map`'s
 * overhead view, first legend entry wins.  Returns YT2_STATUS_ERROR when
 * no marker contains the point.
 */
int yt2_map_view_legend_at(const yt2_world *world, unsigned int map, int x,
                           int y, unsigned int *line);

#endif /* YT2_MAPVIEW_H */
