#ifndef YT2_CLUEBOOK_H
#define YT2_CLUEBOOK_H

#include "yt2/pictures.h"
#include "yt2/surface.h"

/*
 * The on-line clue book help page, reached with F8 in play and Tab from the
 * main menu.  The screen is picture 13 of group 0 at (1,1), the standard 6x6
 * face, and the seven 16x16 command icons along the bottom row.  Palette 0 is
 * the menu palette; the command icons' palette entries are animated by the
 * original but the page pixels use none of them.
 */

#define YT2_CLUEBOOK_PALETTE 0u
#define YT2_CLUEBOOK_PICTURE_GROUP 0u
#define YT2_CLUEBOOK_PICTURE_INDEX 13u

/*
 * Compose the page into an already-sized surface.  Returns YT2_STATUS_ERROR
 * when the surface or picture data is missing.
 */
int yt2_cluebook_render(yt2_surface *surface, const yt2_pictures *pictures);

#endif /* YT2_CLUEBOOK_H */
