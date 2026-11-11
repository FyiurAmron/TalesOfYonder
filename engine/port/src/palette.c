#include "yt2/palette.h"

#include <string.h>

#include "yt2/status.h"

int yt2_palette_rotate(yt2_palette *palette, const uint8_t *base,
                       unsigned int phase)
{
    unsigned int step;

    if (palette == NULL || base == NULL) {
        return YT2_STATUS_ERROR;
    }
    step = phase % YT2_PALETTE_ANIM_PHASES;
    for (unsigned int group = 0; group < YT2_PALETTE_ANIM_GROUPS; group++) {
        for (unsigned int entry = 0; entry < YT2_PALETTE_ANIM_GROUP_ENTRIES;
             entry++) {
            unsigned int from =
                group * YT2_PALETTE_ANIM_GROUP_ENTRIES +
                (entry + step) % YT2_PALETTE_ANIM_GROUP_ENTRIES;
            unsigned int to = YT2_PALETTE_ANIM_FIRST +
                              group * YT2_PALETTE_ANIM_GROUP_ENTRIES + entry;

            memcpy(&palette->rgb[to * YT2_PALETTE_ENTRY_SIZE],
                   &base[from * YT2_PALETTE_ENTRY_SIZE], YT2_PALETTE_ENTRY_SIZE);
        }
    }
    return YT2_STATUS_OK;
}
