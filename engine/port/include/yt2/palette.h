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


#endif /* YT2_PALETTE_H */
