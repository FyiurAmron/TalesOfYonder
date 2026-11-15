#ifndef YT2_TEXT_H
#define YT2_TEXT_H

#include <stdint.h>

#include "yt2/surface.h"

/*
 * The game's standard 6x6 text face.  The executable carries four font banks
 * selected by v_0CEC (pointer table at DGROUP:214h); this is the normal face
 * at DGROUP:21Ch, used by the menus, the clue book and the status screens.
 *
 * A glyph is six row bytes; bit 7 of a row is the leftmost column.  Glyphs are
 * drawn transparently: unset pixels keep the surface contents.
 */

#define YT2_TEXT_GLYPH_WIDTH 6u
#define YT2_TEXT_GLYPH_HEIGHT 6u
#define YT2_TEXT_FIRST_CHAR 0x20u
#define YT2_TEXT_LAST_CHAR 0x7Fu

/* Draw one zero-terminated line; characters outside the face advance only. */
void yt2_text_draw(yt2_surface *surface, int x, int y, const char *text,
                   uint8_t color);

/*
 * Opaque variant (fn_1A03C with v_0B26 == 0): unset glyph pixels paint
 * `background`, so redraws erase the previous contents.
 */
void yt2_text_draw_opaque(yt2_surface *surface, int x, int y, const char *text,
                          uint8_t color, uint8_t background);

/* Width in pixels of a zero-terminated line, six per byte. */
unsigned int yt2_text_width(const char *text);

#endif /* YT2_TEXT_H */
