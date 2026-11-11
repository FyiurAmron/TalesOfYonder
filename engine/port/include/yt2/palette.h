#ifndef YT2_PALETTE_H
#define YT2_PALETTE_H

#include <stdint.h>

/*
 * VGA palette: 256 entries of 6-bit red/green/blue, exactly as the game writes
 * them to the DAC.  The patched port keeps them 6-bit; the client expands to
 * 8-bit at the display boundary.
 */

#define YT2_PALETTE_ENTRY_COUNT 256u
#define YT2_PALETTE_ENTRY_SIZE 3u
#define YT2_PALETTE_SIZE (YT2_PALETTE_ENTRY_COUNT * YT2_PALETTE_ENTRY_SIZE)

typedef struct yt2_palette {
    uint8_t rgb[YT2_PALETTE_SIZE];
} yt2_palette;

/* 6-bit DAC value to 8-bit display value (the client's rounding). */
static inline uint8_t yt2_palette_expand(uint8_t value)
{
    return (uint8_t)(((value & 0x3Fu) * 255u) / 63u);
}

/*
 * Palette animation: the live entries D0h..DFh are four rotating groups of
 * four (blues, greens, magentas, ambers).  Each rotator step turns every
 * group one entry forward; the sequence repeats every four steps.
 */
#define YT2_PALETTE_ANIM_FIRST 208u
#define YT2_PALETTE_ANIM_GROUPS 4u
#define YT2_PALETTE_ANIM_GROUP_ENTRIES 4u
#define YT2_PALETTE_ANIM_PHASES 4u

/*
 * Write rotation `phase` (modulo 4) of the 48 animation-base bytes over
 * entries D0h..DFh of `palette`.  Phase 0 is the base order.  Returns
 * YT2_STATUS_ERROR for a NULL palette or base.  B2 takes the phase as an
 * input: the 5-tick stepping belongs to the future display layer, not here.
 */
int yt2_palette_rotate(yt2_palette *palette, const uint8_t *base,
                       unsigned int phase);

#endif /* YT2_PALETTE_H */
