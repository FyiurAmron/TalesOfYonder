#ifndef YT2_PRESENT_H
#define YT2_PRESENT_H

/*
 * The engine side of the VIDEO boundary: turns composed surfaces into v0
 * frames on a folder transport.  The presenter owns the per-direction
 * sequence and the shadow of what the client already has (mode, palette).
 *
 * Presents carry set_mode when the size changes (or when forced), the
 * coalesced palette span, one scroll plus residue when the picture pans,
 * one run-coded rect per other changed row and frame_end.  A keyframe
 * sends the mode, the full palette and every row.
 */

#include "yt2/palette.h"
#include "yt2/surface.h"
#include "yt2/transport.h"

typedef struct yt2_presenter {
    uint32_t seq;
    unsigned int width;
    unsigned int height;
    int have_mode;
    uint8_t sent_palette[YT2_PALETTE_SIZE];
    int palette_valid;
    uint8_t *shadow;
    unsigned int shadow_width;
    unsigned int shadow_height;
    uint8_t *scratch;
    size_t scratch_size;
} yt2_presenter;

int yt2_presenter_init(yt2_presenter *presenter);
void yt2_presenter_done(yt2_presenter *presenter);

/* Emit CONTROL hello(role 0 engine, version 0). */
int yt2_present_hello(yt2_presenter *presenter, yt2_folder_out *out);

/* Emit TICK pace(ms): the engine's milliseconds per tick. */
int yt2_present_pace(yt2_presenter *presenter, yt2_folder_out *out,
                     unsigned int ms);

/*
 * Present one frame: set_mode when forced or the size changed, the palette
 * span that differs from what the client has (the full 256 on a keyframe),
 * one scroll plus residue rows when the picture pans (keyframe: every
 * row), one run-coded rect per other row that differs from the shadow,
 * the TICK stamp for engine tick `tick`, and frame_end, then commit.  A
 * wrap takes effect on the next call, which must pass force_keyframe (the
 * reader restarted).
 */
int yt2_present_frame(yt2_presenter *presenter, yt2_folder_out *out,
                      const yt2_surface *surface, const yt2_palette *palette,
                      int force_keyframe, uint32_t tick);

#endif /* YT2_PRESENT_H */
