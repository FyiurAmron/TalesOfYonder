/* Exploration slice: dungeon window, room renderer, movement, game clock. */

#include "yt2/room.h"

#include <string.h>

#include "yt2/create.h"
#include "yt2/cursor.h"
#include "yt2/text.h"

/* DGROUP:0E5B3h: floor, ceiling, wall, wall, wall-param, overhead icon. */
static const uint16_t room_terrain[62][6] = {
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0016},
    {0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0017},
    {0x0000, 0x0000, 0x0034, 0x0016, 0x0000, 0x0018},
    {0x0000, 0x0000, 0x0013, 0x0016, 0x000E, 0x001B},
    {0x0000, 0x0000, 0x0019, 0x0016, 0x001C, 0x001E},
    {0x0000, 0x0000, 0x0041, 0x0016, 0x002A, 0x0020},
    {0x0000, 0x0000, 0x003D, 0x0016, 0x0000, 0x0022},
    {0x0000, 0x0000, 0x002E, 0x0016, 0x0000, 0x0018},
    {0x0000, 0x0000, 0x003B, 0x0016, 0x000E, 0x0022},
    {0x0000, 0x0000, 0x002C, 0x0016, 0x000E, 0x001B},
    {0x0000, 0x0000, 0x0009, 0x0016, 0x001C, 0x0050},
    {0x0000, 0x0000, 0x0042, 0x0016, 0x002A, 0x0022},
    {0x0000, 0x0000, 0x0000, 0x0016, 0x0000, 0x0018},
    {0x0000, 0x0000, 0x0000, 0x0016, 0x000E, 0x001B},
    {0x0000, 0x0000, 0x0000, 0x0016, 0x001C, 0x001E},
    {0x0000, 0x0000, 0x0000, 0x0016, 0x002A, 0x0020},
    {0x0004, 0x0000, 0x0000, 0x0000, 0x0000, 0x0025},
    {0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0026},
    {0x0006, 0x0000, 0x0000, 0x0000, 0x0000, 0x0027},
    {0x0007, 0x0000, 0x0000, 0x0000, 0x0000, 0x0028},
    {0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0029},
    {0x0003, 0x0000, 0x0000, 0x0000, 0x0000, 0x0029},
    {0x000C, 0x0000, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x000D, 0x0000, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x000A, 0x0000, 0x0000, 0x0000, 0x0000, 0x002C},
    {0x000B, 0x0000, 0x0000, 0x0000, 0x0000, 0x002D},
    {0x0004, 0x0004, 0x0000, 0x0000, 0x0000, 0x0025},
    {0x0005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0026},
    {0x000C, 0x0004, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x000D, 0x0005, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x0002, 0x0004, 0x0000, 0x0000, 0x0000, 0x0029},
    {0x0003, 0x0005, 0x0000, 0x0000, 0x0000, 0x0029},
    {0x000A, 0x0004, 0x0000, 0x0000, 0x0000, 0x002C},
    {0x000B, 0x0005, 0x0000, 0x0000, 0x0000, 0x002D},
    {0x000C, 0x0002, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x000D, 0x0003, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x0002, 0x0002, 0x0000, 0x0000, 0x0000, 0x0029},
    {0x0003, 0x0003, 0x0000, 0x0000, 0x0000, 0x0029},
    {0x0008, 0x0000, 0x0000, 0x0000, 0x0000, 0x0047},
    {0x0008, 0x0000, 0x0000, 0x0000, 0x0000, 0x0047},
    {0x0008, 0x0004, 0x0000, 0x0000, 0x0000, 0x0047},
    {0x0008, 0x0005, 0x0000, 0x0000, 0x0000, 0x0047},
    {0x0008, 0x0002, 0x0000, 0x0000, 0x0000, 0x0047},
    {0x0008, 0x0003, 0x0000, 0x0000, 0x0000, 0x0047},
    {0x0004, 0x0008, 0x0000, 0x0000, 0x0000, 0x0025},
    {0x0005, 0x0009, 0x0000, 0x0000, 0x0000, 0x0026},
    {0x000C, 0x0008, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x000D, 0x0009, 0x0000, 0x0000, 0x0000, 0x002A},
    {0x000A, 0x0008, 0x0000, 0x0000, 0x0000, 0x002C},
    {0x000B, 0x0009, 0x0000, 0x0000, 0x0000, 0x002D},
    {0x0004, 0x0006, 0x0000, 0x0000, 0x0000, 0x0025},
    {0x0005, 0x0007, 0x0000, 0x0000, 0x0000, 0x0026},
    {0x000E, 0x000A, 0x0000, 0x0000, 0x0000, 0x0024},
    {0x000F, 0x000B, 0x0000, 0x0000, 0x0000, 0x0024},
    {0x0010, 0x0004, 0x0000, 0x0000, 0x0000, 0x000F},
    {0x0011, 0x0005, 0x0000, 0x0000, 0x0000, 0x0010},
    {0x0010, 0x0000, 0x0000, 0x0000, 0x0000, 0x000F},
    {0x0011, 0x0000, 0x0000, 0x0000, 0x0000, 0x0010},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
};

/* DGROUP:0E1D7h: four facing pictures + overhead icon. */
static const uint16_t room_objects[68][5] = {
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x002B, 0x002B, 0x002B, 0x002B, 0x0031},
    {0x001C, 0x001A, 0x0020, 0x0020, 0x0040},
    {0x001D, 0x001B, 0x001F, 0x001E, 0x0040},
    {0x001A, 0x001C, 0x0020, 0x0020, 0x0040},
    {0x001B, 0x001D, 0x001E, 0x001F, 0x0040},
    {0x0020, 0x0020, 0x001C, 0x001A, 0x0041},
    {0x001E, 0x001F, 0x001D, 0x001B, 0x0041},
    {0x0020, 0x0020, 0x001A, 0x001C, 0x0041},
    {0x001F, 0x001E, 0x001B, 0x001D, 0x0041},
    {0x000B, 0x000B, 0x000B, 0x000B, 0x0042},
    {0x000C, 0x000C, 0x000C, 0x000C, 0x0042},
    {0x0037, 0x0037, 0x0037, 0x0037, 0x0039},
    {0x000E, 0x000D, 0x000F, 0x0010, 0x003A},
    {0x000D, 0x000E, 0x0010, 0x000F, 0x003B},
    {0x000F, 0x0010, 0x000D, 0x000E, 0x003C},
    {0x0010, 0x000F, 0x000E, 0x000D, 0x003D},
    {0x0047, 0x0047, 0x0047, 0x0047, 0x0051},
    {0x0048, 0x0048, 0x0048, 0x0048, 0x0052},
    {0x0029, 0x0029, 0x0029, 0x0029, 0x0053},
    {0x002A, 0x002A, 0x002A, 0x002A, 0x0054},
    {0x0045, 0x0045, 0x0045, 0x0045, 0x0021},
    {0x0046, 0x0046, 0x0046, 0x0046, 0x0021},
    {0x0033, 0x0033, 0x0033, 0x0033, 0x0019},
    {0x0032, 0x0032, 0x0032, 0x0032, 0x001A},
    {0x0021, 0x0021, 0x0021, 0x0021, 0x0022},
    {0x0035, 0x0035, 0x0035, 0x0035, 0x002B},
    {0x0012, 0x0012, 0x0012, 0x0012, 0x001C},
    {0x0011, 0x0011, 0x0011, 0x0011, 0x001D},
    {0x0014, 0x0014, 0x0014, 0x0014, 0x002B},
    {0x0018, 0x0018, 0x0018, 0x0018, 0x001F},
    {0x0022, 0x0022, 0x0022, 0x0022, 0x0023},
    {0x0040, 0x0040, 0x0040, 0x0040, 0x002B},
    {0x0044, 0x0044, 0x0044, 0x0044, 0x0055},
    {0x0017, 0x0017, 0x0017, 0x0017, 0x0034},
    {0x0025, 0x0025, 0x0025, 0x0025, 0x0037},
    {0x0039, 0x0039, 0x0039, 0x0039, 0x002E},
    {0x003A, 0x003A, 0x003A, 0x003A, 0x002F},
    {0x0036, 0x0036, 0x0036, 0x0036, 0x0030},
    {0x0015, 0x0015, 0x0015, 0x0015, 0x0032},
    {0x0016, 0x0016, 0x0016, 0x0016, 0x0033},
    {0x0023, 0x0023, 0x0023, 0x0023, 0x0035},
    {0x0024, 0x0024, 0x0024, 0x0024, 0x0036},
    {0x003F, 0x003F, 0x003F, 0x003F, 0x0038},
    {0x0026, 0x0026, 0x0026, 0x0026, 0x003E},
    {0x0030, 0x0030, 0x0030, 0x0030, 0x003F},
    {0x0031, 0x0031, 0x0031, 0x0031, 0x003F},
    {0x0038, 0x0000, 0x0000, 0x0000, 0x0043},
    {0x0000, 0x0038, 0x0000, 0x0000, 0x0044},
    {0x0000, 0x0000, 0x0038, 0x0000, 0x0045},
    {0x0000, 0x0000, 0x0000, 0x0038, 0x0046},
    {0x0053, 0x0053, 0x0053, 0x0053, 0x0056},
    {0x0054, 0x0054, 0x0054, 0x0054, 0x0056},
    {0x0055, 0x0055, 0x0055, 0x0055, 0x0056},
    {0x0056, 0x0056, 0x0056, 0x0056, 0x0056},
    {0x0057, 0x0057, 0x0057, 0x0057, 0x0057},
    {0x0058, 0x0058, 0x0058, 0x0058, 0x0056},
    {0x0059, 0x0059, 0x0059, 0x0059, 0x0056},
    {0x005A, 0x005A, 0x005A, 0x005A, 0x0057},
    {0x005B, 0x005B, 0x005B, 0x005B, 0x0056},
    {0x005C, 0x005C, 0x005C, 0x005C, 0x0056},
    {0x005D, 0x005D, 0x005D, 0x005D, 0x0056},
    {0x005E, 0x005E, 0x005E, 0x005E, 0x0058},
    {0x002A, 0x002A, 0x002A, 0x002A, 0x0054},
    {0x0048, 0x0048, 0x0048, 0x0048, 0x0052},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
};

/* DGROUP:0x722E: scene [lo, hi] + 7 low shades + 7 high shades. */
static const int16_t room_scenes[][16] = {
    {0x0000, 0x017D, -7, -6, -5, -4, -3, -2, -1, -10, -9, -8, -7, -6, -5, -4},
    {0x017E, 0x0181, -7, -6, -5, -4, -3, -2, -1, -10, -9, -8, -7, -6, -5, -4},
    {0x0182, 0x0187, -7, -6, -5, -4, -3, -2, -1, -10, -9, -8, -7, -6, -5, -4},
    {0x0188, 0x018D, -7, -6, -5, -4, -3, -2, -1, -9, -8, -7, -6, -5, -4, -3},
    {0x018E, 0x0193, -6, -5, -4, -3, -2, -1, 0, -9, -8, -7, -6, -5, -4, -3},
    {0x0194, 0x0199, -6, -5, -4, -3, -2, -1, 0, -8, -7, -6, -5, -4, -3, -2},
    {0x019A, 0x019F, -6, -5, -4, -3, -2, -1, 0, -7, -6, -5, -4, -3, -2, -1},
    {0x01A0, 0x01A5, -6, -5, -4, -3, -2, -1, 0, -7, -6, -5, -4, -3, -2, -1},
    {0x01A6, 0x01AB, -5, -4, -3, -2, -1, 0, 0, -6, -5, -4, -3, -2, -1, 0},
    {0x01AC, 0x01B1, -5, -4, -3, -2, -1, 0, 0, -6, -5, -4, -3, -2, -1, 0},
    {0x01B2, 0x01B7, -5, -4, -3, -2, -1, 0, 0, -5, -4, -3, -2, -1, 0, 0},
    {0x01B8, 0x01BD, -5, -4, -3, -2, -1, 0, 0, -4, -3, -2, -1, 0, 0, 0},
    {0x01BE, 0x01C3, -4, -3, -2, -1, 0, 0, 0, -3, -2, -1, 0, 0, 0, 0},
    {0x01C4, 0x01C9, -4, -3, -2, -1, 0, 0, 0, -2, -1, 0, 0, 0, 0, 0},
    {0x01CA, 0x01CF, -4, -3, -2, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},
    {0x01D0, 0x01D5, -4, -3, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x01D6, 0x01DB, -3, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x01DC, 0x01E1, -3, -2, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
    {0x01E2, 0x044D, -3, -2, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
    {0x044E, 0x0451, -3, -2, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
    {0x0452, 0x0457, -3, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x0458, 0x045D, -3, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0x045E, 0x0463, -4, -3, -2, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},
    {0x0464, 0x0469, -4, -3, -2, -1, 0, 0, 0, -2, -1, 0, 0, 0, 0, 0},
    {0x0469, 0x046F, -4, -3, -2, -1, 0, 0, 0, -3, -2, -1, 0, 0, 0, 0},
    {0x0470, 0x0475, -4, -3, -2, -1, 0, 0, 0, -4, -3, -2, -1, 0, 0, 0},
    {0x0476, 0x047B, -5, -4, -3, -2, -1, 0, 0, -5, -4, -3, -2, -1, 0, 0},
    {0x047C, 0x0481, -5, -4, -3, -2, -1, 0, 0, -6, -5, -4, -3, -2, -1, 0},
    {0x0482, 0x0487, -5, -4, -3, -2, -1, 0, 0, -6, -5, -4, -3, -2, -1, 0},
    {0x0487, 0x048D, -5, -4, -3, -2, -1, 0, 0, -7, -6, -5, -4, -3, -2, -1},
    {0x048E, 0x0493, -6, -5, -4, -3, -2, -1, 0, -7, -6, -5, -4, -3, -2, -1},
    {0x0494, 0x0499, -6, -5, -4, -3, -2, -1, 0, -8, -7, -6, -5, -4, -3, -2},
    {0x049A, 0x049F, -6, -5, -4, -3, -2, -1, 0, -9, -8, -7, -6, -5, -4, -3},
    {0x04A0, 0x04A5, -6, -5, -4, -3, -2, -1, 0, -9, -8, -7, -6, -5, -4, -3},
    {0x04A5, 0x04AB, -7, -6, -5, -4, -3, -2, -1, -10, -9, -8, -7, -6, -5, -4},
    {0x04AC, 0x04B1, -7, -6, -5, -4, -3, -2, -1, -10, -9, -8, -7, -6, -5, -4},
    {0x04B2, 0x05A3, -7, -6, -5, -4, -3, -2, -1, -10, -9, -8, -7, -6, -5, -4},
    {-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/* DGROUP:0x76D0/0x7732/0x7740: flag-selected shade sets. */
static const int16_t room_shade_flag4[7] = {-12, -11, -10, -9, -8, -7, -6};
static const int16_t room_shade_flag2[7] = {-8, -7, -6, -5, -4, -3, -2};
static const int16_t room_shade_default[7] = {-7, -6, -5, -4, -3, -2, -1};

/* DGROUP:0x76DE: brightening caps, 7 shade rows x 4 flag variants. */
static const int16_t room_shade_adjust[7][4] = {
    {10, 8, 5, 4},
    {10, 8, 5, 4},
    {9, 8, 5, 4},
    {9, 7, 5, 4},
    {8, 7, 5, 4},
    {7, 6, 5, 4},
    {6, 6, 5, 4}
};

/*
 * Portrait slot layout (DGROUP:0x61C8, 40 words): rect, three status icon
 * cells, three buff cells, name/bar cell.  Words hold x0, x1, y0, y1, id.
 */
static const uint16_t room_portraits[4][40] = {
    {8, 39, 148, 179, 1, 40, 47, 150, 157, 2, 40, 47, 160, 167, 3, 40, 47,
     170, 177, 4, 50, 57, 150, 157, 5, 50, 57, 160, 167, 6, 50, 57, 170, 177,
     7, 9, 47, 181, 196, 8},
    {66, 96, 148, 179, 11, 98, 105, 150, 157, 12, 98, 105, 160, 167, 13, 98,
     105, 170, 177, 14, 108, 115, 150, 157, 15, 108, 115, 160, 167, 16, 108,
     115, 170, 177, 17, 67, 105, 181, 196, 18},
    {124, 154, 148, 179, 21, 156, 163, 150, 157, 22, 156, 163, 160, 167, 23,
     156, 163, 170, 177, 24, 166, 173, 150, 157, 25, 166, 173, 160, 167, 26,
     166, 173, 170, 177, 27, 125, 163, 181, 196, 28},
    {182, 212, 148, 179, 31, 214, 221, 150, 157, 32, 214, 221, 160, 167, 33,
     214, 221, 170, 177, 34, 224, 231, 150, 157, 35, 224, 231, 160, 167, 36,
     224, 231, 170, 177, 37, 183, 221, 181, 196, 38}
};

/* Arrow feedback cells (DGROUP:0x6420, x0, x1, y0, y1, id). */
static const uint16_t room_arrows[6][5] = {
    {246, 261, 161, 177, 1},
    {268, 284, 161, 177, 2},
    {291, 307, 161, 177, 3},
    {246, 261, 178, 194, 4},
    {268, 284, 178, 194, 5},
    {291, 307, 178, 194, 6}
};

/* Step-cell action table (DGROUP:0xD22B): object, flags, action, data. */
static const uint16_t room_actions[][4] = {
    {17, 0, 2, 0},
    {18, 0, 21, 0},
    {19, 0, 12, 0},
    {20, 0, 11, 0},
    {63, 0, 23, 999},
    {64, 0, 31, 0},
    {65, 0x4000, 334, 99},
    {66, 0x4000, 346, 99},
    {67, 0x0200, 322, 0},
    {0xFFFF, 0, 0, 0}
};

/* Compass labels (DGROUP:0x78D4, 9 bytes each). */
static const char room_compass[4][10] = {
    "   NORTH",
    "   SOUTH",
    "    EAST",
    "    WEST"
};

/* Occlusion mark lists: view-line indices, -1 terminated. */
static const int8_t room_hide_ae[] = {0, 19, 20, 21, 22, 23, 34, 35, 39, 42, -1};
static const int8_t room_hide_c4[] = {16, 27, 28, 29, 30, 31, 37, 38, 41, 44, -1};
static const int8_t room_hide_82[] = {0, 1, 2, 17, 18, 21, 22, 23, 35, 39, -1};
static const int8_t room_hide_98[] = {14, 15, 16, 32, 33, 27, 28, 29, 37, 41, -1};
static const int8_t room_hide_4e[] = {0, 1, 2, 3, 4, 5, 6, 7, 22, 23, 24, 35, -1};
static const int8_t room_hide_68[] = {9, 10, 11, 12, 13, 14, 15, 16, 26, 27, 28, 37, -1};
static const int8_t room_hide_2a[] = {2, 3, 4, 5, 6, 7, 23, 24, -1};
static const int8_t room_hide_3c[] = {9, 10, 11, 12, 13, 14, 26, 27, -1};

/* DGROUP:8 chain: probe view cell, probe cell + 1, then mark the list. */
static const struct {
    uint8_t cell;
    const int8_t *list;
} room_hide_chain[] = {
    {45, room_hide_ae},
    {46, room_hide_c4},
    {42, room_hide_82},
    {43, room_hide_98},
    {39, room_hide_4e},
    {40, room_hide_68},
    {35, room_hide_2a},
    {36, room_hide_3c}
};

/* DGROUP:0xDA: solid cell at this view column marks the list. */
static const int8_t room_hide_20e[] = {0, -1};
static const int8_t room_hide_206[] = {0, 1, 2, -1};
static const int8_t room_hide_202[] = {8, -1};
static const int8_t room_hide_1fa[] = {14, 15, 16, -1};
static const int8_t room_hide_1f6[] = {16, -1};
static const int8_t room_hide_1ee[] = {17, 18, 19, -1};
static const int8_t room_hide_1dc[] = {0, 1, 17, 18, 19, 20, 21, 22, -1};
static const int8_t room_hide_1d6[] = {8, 25, -1};
static const int8_t room_hide_1c4[] = {15, 16, 28, 29, 30, 31, 32, 33, -1};
static const int8_t room_hide_1bc[] = {31, 32, 33, -1};
static const int8_t room_hide_1ae[] = {17, 18, 19, 20, 21, 34, -1};
static const int8_t room_hide_1a6[] = {8, 25, 36, -1};
static const int8_t room_hide_198[] = {29, 30, 31, 32, 33, 38, -1};
static const int8_t room_hide_18c[] = {17, 18, 19, 20, 34, -1};
static const int8_t room_hide_16a[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 24, 25, 26, 36, 40, -1};
static const int8_t room_hide_15e[] = {30, 31, 32, 33, 38, -1};
static const int8_t room_hide_158[] = {17, 18, -1};
static const int8_t room_hide_12c[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 36, 40, 43, -1};
static const int8_t room_hide_126[] = {33, 32, -1};
static const int8_t room_hide_122[] = {17, -1};
static const int8_t room_hide_11e[] = {33, -1};

static const struct {
    uint8_t column;
    const int8_t *list;
} room_hide_columns[] = {
    {0x16, room_hide_20e},
    {0x17, room_hide_206},
    {0x19, room_hide_202},
    {0x1B, room_hide_1fa},
    {0x1C, room_hide_1f6},
    {0x22, room_hide_1ee},
    {0x23, room_hide_1dc},
    {0x24, room_hide_1d6},
    {0x25, room_hide_1c4},
    {0x26, room_hide_1bc},
    {0x27, room_hide_1ae},
    {0x28, room_hide_1a6},
    {0x29, room_hide_198},
    {0x2A, room_hide_18c},
    {0x2B, room_hide_16a},
    {0x2C, room_hide_15e},
    {0x2D, room_hide_158},
    {0x2E, room_hide_12c},
    {0x2F, room_hide_126},
    {0x30, room_hide_122},
    {0x32, room_hide_11e}
};

/* Perspective sub-table bases (v_516C..v_5180, set at boot). */
#define ROOM_PERSP_M0 0x0000u
#define ROOM_PERSP_M1 0x0386u
#define ROOM_PERSP_M2 0x0BF2u
#define ROOM_PERSP_M6 0x13B6u
#define ROOM_PERSP_M7 0x3B76u
#define ROOM_PERSP_M9 0x3CA8u
#define ROOM_PERSP_MA 0x3DF2u
#define ROOM_PERSP_MB 0x41D2u
#define ROOM_PERSP_MC 0x4316u
#define ROOM_PERSP_MD 0x4460u
#define ROOM_PERSP_ME 0x4858u

/* Picture-cache class ids (v_0CCC) and their PICTURES.VGA groups. */
#define ROOM_CLASS_WALLS 0x10u
#define ROOM_CLASS_SPRITE2 0x20u
#define ROOM_CLASS_SPRITE3 0x30u
#define ROOM_CLASS_FLOOR 0x40u
#define ROOM_CLASS_CEIL 0x50u
#define ROOM_CLASS_OVERLAY 0x60u
#define ROOM_CLASS_PORTRAIT 0x70u
#define ROOM_CLASS_ICON 0x80u
#define ROOM_CLASS_BUDGET 0x90u

static unsigned int room_class_group(unsigned int class)
{
    return class >> 4;
}

static uint16_t room_read16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

/* Writable save chunk bytes (visited/object/world-table bits). */
static uint8_t *room_save_bytes(yt2_room *room, size_t offset, size_t size)
{
    if (room->save == NULL || room->save->bytes == NULL ||
        offset + size > room->save->size) {
        return NULL;
    }
    return room->save->bytes + offset;
}

/* One WORLD.DAT record; NULL when the file image is too short. */
static const uint8_t *room_world_record(const yt2_room *room, size_t offset,
                                        size_t size)
{
    if (room->world == NULL || room->world->bytes == NULL ||
        offset + size > room->world->size) {
        return NULL;
    }
    return room->world->bytes + offset;
}

static int room_visited(const yt2_room *room, unsigned int x, unsigned int y)
{
    const uint8_t *bytes;

    if (room->save == NULL || room->save->bytes == NULL ||
        x >= YT2_WORLD_TILES_X || y >= YT2_WORLD_TILES_Y) {
        return 0;
    }
    bytes = room->save->bytes + YT2_SAVE_OBJECTS_OFFSET +
            (size_t)y * YT2_SAVE_OBJECT_SIZE;
    return (bytes[x / 8u] & (uint8_t)(0x80u >> (x % 8u))) != 0;
}

static void room_set_visited(yt2_room *room, unsigned int x, unsigned int y)
{
    uint8_t *bytes = room_save_bytes(room, YT2_SAVE_OBJECTS_OFFSET +
                                     (size_t)y * YT2_SAVE_OBJECT_SIZE,
                                     YT2_SAVE_OBJECT_SIZE);

    if (bytes == NULL || x >= YT2_WORLD_TILES_X ||
        y >= YT2_WORLD_TILES_Y) {
        return;
    }
    bytes[x / 8u] |= (uint8_t)(0x80u >> (x % 8u));
}

/* Object taken/killed bit in the save world-map chunk (sub_12F48). */
static int room_object_bit(const yt2_room *room, unsigned int id)
{
    const uint8_t *bytes;
    unsigned int byte = id / 8u;
    unsigned int bit = id % 8u;

    if (room->save == NULL || room->save->bytes == NULL ||
        YT2_SAVE_WORLD_MAP_OFFSET + byte >= room->save->size) {
        return 0;
    }
    bytes = room->save->bytes + YT2_SAVE_WORLD_MAP_OFFSET;
    return (bytes[byte] & (uint8_t)(0x80u >> bit)) != 0;
}

/* Door record lookup (sub_11A93): (y, flags, data) at world (x, y). */
static int room_door_at(const yt2_room *room, unsigned int x, unsigned int y,
                        uint16_t *flags, uint16_t *data)
{
    const uint8_t *doors;
    uint16_t head;
    size_t at;

    if (x < YT2_ROOM_BOUND_X0 || x > YT2_ROOM_BOUND_X1 ||
        y < YT2_ROOM_BOUND_Y0 || y > YT2_ROOM_BOUND_Y1) {
        return 0;
    }
    doors = room_world_record(room, YT2_WORLD_DOORS_OFFSET,
                              YT2_WORLD_DOORS_SIZE);
    if (doors == NULL) {
        return 0;
    }
    head = room_read16(doors + (x - YT2_ROOM_BOUND_X0) * 2u);
    at = head;
    for (;;) {
        uint16_t ey;
        uint16_t ef;
        uint16_t ed;

        if (at + 6u > YT2_WORLD_DOORS_SIZE) {
            return 0;
        }
        ey = room_read16(doors + at);
        ef = room_read16(doors + at + 2u);
        ed = room_read16(doors + at + 4u);
        if (ey == 0xFFFFu || y < ey) {
            return 0;
        }
        if (y == ey) {
            *flags = ef;
            *data = ed;
            return 1;
        }
        at += 6u;
    }
}

/* fn_07727/fn_077C4: resolve a door data id to its record words. */
static void room_door_data(const yt2_room *room, unsigned int id, int locked,
                           uint16_t *bit, uint16_t *words)
{
    const uint8_t *rec;
    unsigned int bit_index;

    if (locked) {
        bit_index = id + 0x260u;
        rec = room_world_record(room, YT2_WORLD_DOOR4_OFFSET +
                                (size_t)id * YT2_WORLD_DOOR4_SIZE,
                                YT2_WORLD_DOOR4_SIZE);
        if (rec != NULL) {
            words[0] = room_read16(rec);
            words[1] = room_read16(rec + 2u);
        }
    } else {
        bit_index = id;
        rec = room_world_record(room, YT2_WORLD_DOOR26_OFFSET +
                                (size_t)id * YT2_WORLD_DOOR26_SIZE,
                                YT2_WORLD_DOOR26_SIZE);
        if (rec != NULL) {
            unsigned int i;

            for (i = 0u; i < 13u; i++) {
                words[i] = room_read16(rec + i * 2u);
            }
        }
    }
    /* v_5569 test byte from the save world-table chunk. */
    *bit = 0u;
    if (room->save != NULL && room->save->bytes != NULL) {
        size_t at = YT2_SAVE_WORLD_TABLE_OFFSET + bit_index / 8u;

        if (at < room->save->size) {
            uint8_t mask = (uint8_t)(0x80u >> (bit_index % 8u));

            if ((room->save->bytes[at] & mask) != 0) {
                *bit = 1u;
            }
        }
    }
}

/*
 * fn_11914: door/special cell check.  Returns the v_50CC code and, for
 * code 6/7, the replacement terrain/object in `*stamp`.
 */
static unsigned int room_door_check(yt2_room *room, unsigned int x,
                                    unsigned int y, uint16_t *stamp)
{
    uint16_t flags;
    uint16_t data;
    uint16_t words[13];
    uint16_t bit;
    unsigned int i;

    *stamp = 0u;
    if (!room_door_at(room, x, y, &flags, &data)) {
        return 0u;
    }
    if ((flags & 0x8000u) != 0u) {
        for (i = 0u; i < 13u; i++) {
            words[i] = 0u;
        }
        room_door_data(room, data - 1u, 0, &bit, words);
        if (bit != 0u) {
            return 0u;
        }
        if ((words[0] & 0x20u) != 0u) {
            return 3u;
        }
        if ((words[0] & 0x40u) != 0u) {
            return 8u;
        }
        if (words[1] != 0u) {
            return 9u;
        }
        return 0u;
    }
    if ((flags & 0x4000u) != 0u) {
        for (i = 0u; i < 13u; i++) {
            words[i] = 0u;
        }
        room_door_data(room, data - 1u, 1, &bit, words);
        *stamp = words[1];
        if (bit != 0u) {
            return 0u;
        }
        if ((words[0] & 0x10u) != 0u) {
            return 6u;
        }
        if ((words[0] & 0x08u) != 0u) {
            return 7u;
        }
        if ((words[0] & 0x40u) != 0u) {
            return 10u;
        }
        if ((words[0] & 0x20u) != 0u) {
            return 2u;
        }
        return 1u;
    }
    if ((flags & 0x1000u) != 0u) {
        return 4u;
    }
    if ((flags & 0x0800u) == 0u) {
        return 0u;
    }
    room->f5062 |= 0x0080u;
    if (room_object_bit(room, data)) {
        return 0u;
    }
    return 5u;
}

/* Window cell pointer (fn_06FF0): NULL when outside the cached window. */
static yt2_room_cell *room_window_cell(yt2_room *room, int x, int y)
{
    int ix = x - room->origin_x;
    int iy = y - room->origin_y;

    if (ix < 0 || iy < 0 || ix >= (int)YT2_ROOM_WINDOW_SIDE ||
        iy >= (int)YT2_ROOM_WINDOW_SIDE) {
        return NULL;
    }
    return &room->window[iy][ix];
}

/* fn_10BF6: load the 78x78 window around the party. */
static int room_load_window(yt2_room *room)
{
    int x = (int)room->party->x;
    int y = (int)room->party->y;
    int ox = x - 0x27;
    int oy = y - 0x27;
    int wx;
    int wy;

    if (ox < (int)YT2_ROOM_BOUND_X0 - 15) {
        ox = (int)YT2_ROOM_BOUND_X0 - 15;
    }
    if (ox > (int)YT2_ROOM_BOUND_X1 - 15) {
        ox = (int)YT2_ROOM_BOUND_X1 - 15;
    }
    if (oy < (int)YT2_ROOM_BOUND_Y0 - 15) {
        oy = (int)YT2_ROOM_BOUND_Y0 - 15;
    }
    if (oy > (int)YT2_ROOM_BOUND_Y1 - 15) {
        oy = (int)YT2_ROOM_BOUND_Y1 - 15;
    }
    room->origin_x = ox;
    room->origin_y = oy;
    for (wy = 0; wy < (int)YT2_ROOM_WINDOW_SIDE; wy++) {
        for (wx = 0; wx < (int)YT2_ROOM_WINDOW_SIDE; wx++) {
            unsigned int gx = (unsigned int)(ox + wx);
            unsigned int gy = (unsigned int)(oy + wy);
            yt2_room_cell *cell = &room->window[wy][wx];
            const yt2_tile *tile = NULL;

            if (gx < YT2_WORLD_TILES_X && gy < YT2_WORLD_TILES_Y &&
                room->world != NULL && room->world->tiles != NULL) {
                tile = &room->world->tiles[gy * YT2_WORLD_TILES_X + gx];
            }
            cell->terrain = tile != NULL ? tile->terrain : 0u;
            cell->object = tile != NULL ? tile->object : 0u;
            cell->misc = 0u;
            cell->flags = room_visited(room, gx, gy) ?
                          YT2_ROOM_CELL_VISITED : 0u;
        }
    }
    return YT2_STATUS_OK;
}

/* Window post-load: stamp door flags/objects, re-place loaded objects. */
static void room_stamp_window(yt2_room *room)
{
    int wx;
    int wy;
    unsigned int i;

    for (wy = 0; wy < (int)YT2_ROOM_WINDOW_SIDE; wy++) {
        for (wx = 0; wx < (int)YT2_ROOM_WINDOW_SIDE; wx++) {
            unsigned int gx = (unsigned int)(room->origin_x + wx);
            unsigned int gy = (unsigned int)(room->origin_y + wy);
            yt2_room_cell *cell = &room->window[wy][wx];
            uint16_t stamp = 0u;
            uint16_t flags;
            uint16_t data;
            unsigned int code = room_door_check(room, gx, gy, &stamp);

            switch (code) {
            case 1u:
            case 10u:
                cell->flags |= 0x4000u;
                break;
            case 2u:
                cell->flags |= 0x2000u;
                break;
            case 3u:
                cell->flags |= 0x1000u;
                break;
            case 5u:
                if (room_door_at(room, gx, gy, &flags, &data)) {
                    cell->flags |= YT2_ROOM_CELL_OBJECT;
                    cell->misc = data;
                }
                break;
            case 6u:
                cell->object = stamp;
                break;
            case 7u:
                cell->terrain = stamp;
                break;
            default:
                break;
            }
        }
    }
    /* Re-place loaded objects inside the window, free the rest. */
    for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
        yt2_room_object *obj = &room->objects[i];
        yt2_room_cell *cell;

        if (obj->id == 0u) {
            continue;
        }
        cell = room_window_cell(room, (int)obj->x, (int)obj->y);
        if (cell == NULL) {
            memset(obj, 0, sizeof(*obj));
            continue;
        }
        obj->cell = (uint16_t)(((obj->y - (unsigned int)room->origin_y) *
                                YT2_ROOM_WINDOW_ROW_BYTES) +
                               ((obj->x - (unsigned int)room->origin_x) * 8u));
        cell->misc = obj->id;
        cell->flags |= YT2_ROOM_CELL_OBJECT;
    }
}

/* sub_1ABED: add the distance shade, clamped to the 16-entry palette row. */
static uint8_t room_shade_pixel(uint8_t pixel, int shade)
{
    unsigned int lo;
    unsigned int hi;
    unsigned int out;

    if (shade == 0 || pixel >= 0xD0u) {
        return pixel;
    }
    lo = pixel & 0xF0u;
    hi = lo | 0x0Fu;
    out = (unsigned int)((pixel + (unsigned int)(shade & 0xFF)) & 0xFFu);
    if (out < lo) {
        return (uint8_t)lo;
    }
    if (out <= hi) {
        return (uint8_t)out;
    }
    if ((pixel & 0x80u) != 0u) {
        return (uint8_t)hi;
    }
    if ((out & 0x80u) != 0u) {
        return (uint8_t)lo;
    }
    return (uint8_t)hi;
}

/* sub_1AA4A: remap the pixel row through the v_5060 sprite shade table. */
static uint8_t room_shade_map_pixel(uint8_t pixel, const uint16_t *map)
{
    unsigned int row = pixel >> 4;
    unsigned int i;

    for (i = 0u; i < 16u; i++) {
        unsigned int entry = map[i];
        unsigned int brow = (entry >> 8) & 0xFFu;
        unsigned int blow = entry & 0xFFu;

        if (entry == 0u || row < brow) {
            break;
        }
        if (row != brow) {
            continue;
        }
        if (blow == 0x0Fu) {
            return 0xFFu;
        }
        return (uint8_t)((blow << 4) | (pixel & 0x0Fu));
    }
    return pixel;
}

typedef struct room_blit {
    const uint8_t *table;
    const uint8_t *src;
    size_t src_size;
    unsigned int src_width;
    yt2_surface *dst;
    int shade;
    int transparent;
    uint16_t shade_map[16];
    int have_shade_map;
} room_blit;

static uint16_t room_table16(const room_blit *blit, size_t at)
{
    if (at + 2u > YT2_WORLD_PERSP_SIZE) {
        return 0u;
    }
    return room_read16(blit->table + at);
}

static void room_put_pixel(room_blit *blit, size_t src_at, size_t dst_at,
                           int use_map)
{
    uint8_t pixel;
    unsigned int x;
    unsigned int y;

    if (src_at >= blit->src_size) {
        return;
    }
    pixel = blit->src[src_at];
    if (blit->transparent && pixel == 0xFFu) {
        return;
    }
    pixel = room_shade_pixel(pixel, blit->shade);
    if (use_map && blit->have_shade_map) {
        pixel = room_shade_map_pixel(pixel, blit->shade_map);
        if (pixel == 0xFFu) {
            return;
        }
    }
    /* The original overruns the 64 KB buffer past the screen bottom; B2
       clips instead (same visible bytes). */
    y = (unsigned int)(dst_at / 320u);
    x = (unsigned int)(dst_at % 320u);
    if (x >= blit->dst->width || y >= blit->dst->height) {
        return;
    }
    blit->dst->pixels[y * blit->dst->stride + x] = pixel;
}

/*
 * sub_1AB23: horizontal strip runs.  si/di are call-local (the caller
 * resets them from [bp-4]/[bp-8] every row); the entry list always
 * restarts at the head here.
 */
static void room_blit_strips_h(room_blit *blit, size_t entry_at,
                               size_t src_base, size_t dst_base)
{
    size_t src_at = src_base;
    size_t dst_at = dst_base;

    for (;;) {
        uint16_t count;
        uint16_t run;
        uint16_t skip;
        uint16_t i;

        if (entry_at + 6u > YT2_WORLD_PERSP_SIZE) {
            return;
        }
        run = room_table16(blit, entry_at + 2u);
        skip = room_table16(blit, entry_at + 4u);
        count = room_table16(blit, entry_at);
        entry_at += 6u;
        if (count == 0u) {
            return;
        }
        for (i = 0u; i < count; i++) {
            uint16_t p;

            for (p = 0u; p < run; p++) {
                room_put_pixel(blit, src_at, dst_at, 1);
                src_at++;
                dst_at++;
            }
            src_at += skip;
        }
    }
}

/*
 * sub_1AB91: vertical strip runs.  si/di are call-local; the entry
 * pointer outlives the call (the caller derives the next strip from it).
 */
static void room_blit_strips_v(room_blit *blit, size_t *entry_at,
                               size_t src_base, size_t dst_base)
{
    size_t src_at = src_base;
    size_t dst_at = dst_base;

    for (;;) {
        uint16_t count;
        uint16_t run;
        uint16_t skip;
        uint16_t i;

        if (*entry_at + 6u > YT2_WORLD_PERSP_SIZE) {
            return;
        }
        run = room_table16(blit, *entry_at + 2u);
        skip = room_table16(blit, *entry_at + 4u);
        count = room_table16(blit, *entry_at);
        *entry_at += 6u;
        if (count == 0u) {
            return;
        }
        for (i = 0u; i < count; i++) {
            uint16_t p;

            for (p = 0u; p < run; p++) {
                room_put_pixel(blit, src_at, dst_at, 0);
                src_at += blit->src_width;
                dst_at += 320u;
            }
            src_at += (size_t)skip * blit->src_width;
        }
    }
}

/* sub_1AAB5: one horizontal run of `run` pixels from the bases. */
static void room_blit_run(room_blit *blit, uint16_t run, size_t src_base,
                          size_t dst_base)
{
    uint16_t p;
    size_t src_at = src_base;
    size_t dst_at = dst_base;

    for (p = 0u; p < run; p++) {
        uint8_t pixel;

        if (src_at >= blit->src_size) {
            return;
        }
        pixel = blit->src[src_at];
        src_at++;
        if (blit->transparent && pixel == 0xFFu) {
            dst_at++;
            continue;
        }
        pixel = room_shade_pixel(pixel, blit->shade);
        {
            unsigned int x = (unsigned int)(dst_at % 320u);
            unsigned int y = (unsigned int)(dst_at / 320u);

            if (x < blit->dst->width && y < blit->dst->height) {
                blit->dst->pixels[y * blit->dst->stride + x] = pixel;
            }
        }
        dst_at++;
    }
}

/*
 * fn_1A0A9 modes 0/7/8/9..E: front-face column patches.  `base` selects
 * the sub-table; mode 8 reuses the mode-0 table.
 */
static void room_blit_faces(room_blit *blit, size_t base, unsigned int column)
{
    size_t entry = base + (size_t)column * 6u;
    size_t strips;
    size_t desc;
    size_t first;
    uint16_t ex;
    uint16_t ey;
    size_t src_at;
    size_t dst_at;

    if (entry + 6u > YT2_WORLD_PERSP_SIZE) {
        return;
    }
    ex = room_table16(blit, entry);
    ey = room_table16(blit, entry + 2u);
    strips = room_table16(blit, entry + 4u);
    if (ex == 0u) {
        return;
    }
    if (strips + 2u > YT2_WORLD_PERSP_SIZE) {
        return;
    }
    first = room_table16(blit, strips);
    src_at = 0u;
    dst_at = (size_t)ey * 320u + ex;
    desc = first;
    for (;;) {
        uint16_t reps;
        uint16_t rows;
        uint16_t src_skip;
        uint16_t r;

        if (desc + 6u > YT2_WORLD_PERSP_SIZE) {
            return;
        }
        reps = room_table16(blit, desc);
        rows = room_table16(blit, desc + 2u);
        src_skip = room_table16(blit, desc + 4u);
        if (reps == 0u) {
            return;
        }
        for (r = 0u; r < reps; r++) {
            uint16_t i;

            for (i = 0u; i < rows; i++) {
                room_blit_strips_h(blit, strips + 2u, src_at, dst_at);
                dst_at += 320u;
                src_at += blit->src_width;
            }
            src_at += (size_t)src_skip * blit->src_width;
        }
        desc += 6u;
    }
}

/* fn_1A0A9 modes 1/2: floor/ceiling diagonal strips. */
static void room_blit_ground(room_blit *blit, size_t base,
                             unsigned int column, unsigned int y_add)
{
    size_t entry = base + (size_t)column * 6u;
    size_t strips;
    size_t src_at;
    size_t dst_at;
    size_t at;
    uint16_t ex;
    uint16_t ey;

    if (entry + 6u > YT2_WORLD_PERSP_SIZE) {
        return;
    }
    ex = room_table16(blit, entry);
    ey = room_table16(blit, entry + 2u);
    strips = room_table16(blit, entry + 4u);
    if (strips == 0u) {
        return;
    }
    dst_at = ((size_t)ey + y_add) * 320u + ex + 8u;
    src_at = (size_t)ey * blit->src_width + ex;
    at = strips;
    for (;;) {
        uint16_t run;
        int16_t step;

        if (at + 4u > YT2_WORLD_PERSP_SIZE) {
            return;
        }
        run = room_table16(blit, at);
        if (run == 0u) {
            return;
        }
        room_blit_run(blit, run, src_at, dst_at);
        src_at += blit->src_width;
        dst_at += 320u;
        step = (int16_t)room_table16(blit, at + 2u);
        if (step >= 0) {
            src_at += (size_t)step;
            dst_at += (size_t)step;
        } else {
            src_at -= (size_t)-(int)step;
            dst_at -= (size_t)-(int)step;
        }
        at += 6u;
    }
}

/* fn_1A0A9 modes 3/4: side-wall vertical slices. */
static void room_blit_side(room_blit *blit, unsigned int column, int down)
{
    size_t entry = ROOM_PERSP_M6 + (size_t)column * 6u;
    size_t strips;
    size_t src_at = 0u;
    size_t dst_at;
    size_t at;
    uint16_t ex;
    uint16_t ey;

    if (entry + 6u > YT2_WORLD_PERSP_SIZE) {
        return;
    }
    ex = room_table16(blit, entry);
    ey = room_table16(blit, entry + 2u);
    strips = room_table16(blit, entry + 4u);
    if (ex == 0u) {
        return;
    }
    dst_at = (size_t)ey * 320u + ex;
    at = strips;
    for (;;) {
        uint16_t count;
        uint16_t src_skip;
        uint16_t flag;
        uint16_t i;

        if (at + 6u > YT2_WORLD_PERSP_SIZE) {
            return;
        }
        count = room_table16(blit, at);
        src_skip = room_table16(blit, at + 2u);
        flag = room_table16(blit, at + 4u);
        if (flag == 0u) {
            src_at += src_skip;
            at += 6u;
            if (count == 0u) {
                return;
            }
            continue;
        }
        if (count == 0u) {
            return;
        }
        {
            size_t entry_at = 0u;

            for (i = 0u; i < count; i++) {
                /* The entry restarts every iteration ([bp-0x1C] is set
                   from [bp-0x10] inside the count loop). */
                entry_at = at + 4u;
                room_blit_strips_v(blit, &entry_at, src_at, dst_at);
                src_at++;
                dst_at++;
                src_at += src_skip;
            }
            at = entry_at - 4u;
        }
        if (down) {
            dst_at += 320u;
        } else if (dst_at >= 320u) {
            dst_at -= 320u;
        } else {
            return;
        }
    }
}

/* fn_1A0A9 mode 6: far-wall 7x113 strip. */
static void room_blit_far(room_blit *blit, unsigned int column,
                          unsigned int src_offset)
{
    size_t entry = ROOM_PERSP_M6 + (size_t)column * 6u;
    size_t src_at = src_offset;
    size_t dst_at;
    unsigned int row;
    uint16_t ex;
    uint16_t ey;

    if (entry + 6u > YT2_WORLD_PERSP_SIZE) {
        return;
    }
    ex = room_table16(blit, entry);
    ey = room_table16(blit, entry + 2u);
    dst_at = (size_t)ey * 320u + ex;
    for (row = 0u; row < 113u; row++) {
        room_blit_run(blit, 7u, src_at, dst_at);
        src_at += blit->src_width;
        dst_at += 320u;
    }
}

/* fn_1A0A9: dispatch one column draw with the room's blitter parameters. */
static void room_draw_column(yt2_room *room, yt2_surface *dst)
{
    room_blit blit;
    const uint8_t *table;
    const yt2_picture_group *group;
    const uint8_t *pic;
    unsigned int mode = room->blit_mode;
    unsigned int i;

    if (mode == 5u) {
        mode = 7u;
    }
    table = room_world_record(room, YT2_WORLD_PERSP_OFFSET,
                              YT2_WORLD_PERSP_SIZE);
    group = yt2_pictures_group(room->pictures,
                               room_class_group(room->blit_class));
    if (table == NULL || group == NULL) {
        return;
    }
    pic = yt2_picture_data(room->pictures, room_class_group(room->blit_class),
                           room->blit_pic);
    if (pic == NULL) {
        return;
    }
    blit.table = table;
    blit.src = pic;
    blit.src_size = (size_t)group->width * group->height;
    blit.src_width = group->width;
    blit.dst = dst;
    blit.shade = (int)(int8_t)(room->blit_shade & 0xFF);
    blit.transparent = room->blit_transparent != 0u;
    for (i = 0u; i < 16u; i++) {
        blit.shade_map[i] = 0u;
    }
    blit.have_shade_map = 0;
    if ((room->f5060 & 1u) != 0u) {
        for (i = 0u; i < 6u; i++) {
            uint8_t b = room->shade_src[i];

            blit.shade_map[i] =
                (uint16_t)(((uint16_t)(b >> 4) << 8) | (b & 0x0Fu));
        }
        blit.have_shade_map = blit.shade_map[0] != 0u;
    }
    switch (mode) {
    case 0u:
        room_blit_faces(&blit, ROOM_PERSP_M0, room->blit_column);
        break;
    case 1u:
        room_blit_ground(&blit, ROOM_PERSP_M1, room->blit_column, 0x46u);
        break;
    case 2u:
        room_blit_ground(&blit, ROOM_PERSP_M2, room->blit_column, 8u);
        break;
    case 3u:
        room_blit_side(&blit, room->blit_column, 1);
        break;
    case 4u:
        room_blit_side(&blit, room->blit_column, 0);
        break;
    case 6u:
        room_blit_far(&blit, room->blit_column, room->blit_offset);
        break;
    case 7u:
        room_blit_faces(&blit, ROOM_PERSP_M7, room->blit_column);
        break;
    case 8u:
        room_blit_faces(&blit, ROOM_PERSP_M0, room->blit_column);
        break;
    case 9u:
        room_blit_faces(&blit, ROOM_PERSP_M9, room->blit_column);
        break;
    case 10u:
        room_blit_faces(&blit, ROOM_PERSP_MA, room->blit_column);
        break;
    case 11u:
        room_blit_faces(&blit, ROOM_PERSP_MB, room->blit_column);
        break;
    case 12u:
        room_blit_faces(&blit, ROOM_PERSP_MC, room->blit_column);
        break;
    case 13u:
        room_blit_faces(&blit, ROOM_PERSP_MD, room->blit_column);
        break;
    case 14u:
        room_blit_faces(&blit, ROOM_PERSP_ME, room->blit_column);
        break;
    default:
        break;
    }
}

/* fn_19E12: full picture blit (group by blit_class, transparent flag). */
static void room_draw_picture(yt2_room *room, yt2_surface *dst, int x, int y)
{
    const uint8_t *pic = yt2_picture_data(room->pictures,
                                           room_class_group(room->blit_class),
                                           room->blit_pic);
    const yt2_picture_group *group =
        yt2_pictures_group(room->pictures,
                           room_class_group(room->blit_class));

    if (pic == NULL || group == NULL) {
        return;
    }
    if (room->blit_transparent != 0u) {
        yt2_surface_blit_transparent(dst, x, y, pic, group->width,
                                     group->height, 0xFFu);
    } else {
        yt2_surface_blit(dst, x, y, pic, group->width, group->height);
    }
}

/* fn_1A590: shade the ceiling/floor gradient bands in place. */
static void room_shade_gradient(yt2_room *room, yt2_surface *dst, int floor)
{
    static const uint8_t ceil_rows[7] = {10, 11, 10, 10, 9, 9, 3};
    static const uint8_t floor_rows[7] = {4, 9, 9, 10, 10, 11, 21};
    const uint8_t *rows = floor ? floor_rows : ceil_rows;
    unsigned int y = floor ? 70u : 8u;
    unsigned int band;

    for (band = 0u; band < 7u; band++) {
        int shade = room->shades[floor ? band : 6u - band];
        unsigned int row;

        for (row = 0u; row < rows[band]; row++) {
            unsigned int x;

            for (x = 8u; x < 232u; x++) {
                uint8_t *pixel = &dst->pixels[y * dst->stride + x];

                *pixel = room_shade_pixel(*pixel, shade);
            }
            y++;
        }
    }
}

/* fn_17D5A: select the seven distance shades. */
static void room_select_shades(yt2_room *room)
{
    const int16_t *set = NULL;
    unsigned int i;

    room->f505e |= 0x0400u;
    if ((room->r9469 & 4u) != 0u) {
        set = room_shade_flag4;
    } else if ((room->r9469 & 2u) != 0u) {
        set = room_shade_flag2;
    } else if ((room->r9469 & 1u) != 0u) {
        set = room_shade_default;
    } else {
        unsigned int scene = room->party->scene;
        const int16_t *row = NULL;
        unsigned int r;

        for (r = 0u; ; r++) {
            if (room_scenes[r][0] < 0) {
                room->party->scene = 0u;
                row = room_scenes[0];
                break;
            }
            if ((int)scene <= room_scenes[r][1]) {
                row = room_scenes[r];
                break;
            }
        }
        for (i = 0u; i < 7u; i++) {
            room->shades[i] = row[9 + i];
        }
        return;
    }
    for (i = 0u; i < 7u; i++) {
        room->shades[i] = set[i];
    }
    /* The 0x76DE brightening pass needs the 0x6E9E facing scan, whose
       table the original never fills: ax is always 0, and with v_9469
       clear every selector misses, so explore keeps the raw set. */
    if (room->r9469 != 0u) {
        unsigned int variant = 0u;

        if ((room->r9469 & 0x0200u) != 0u || (room->r9469 & 0x0008u) != 0u) {
            variant = 0u;
        } else if ((room->r9469 & 0x0400u) != 0u ||
                   (room->r9469 & 0x0010u) != 0u) {
            variant = 1u;
        } else if ((room->r9469 & 0x0800u) != 0u ||
                   (room->r9469 & 0x0020u) != 0u) {
            variant = 2u;
        } else if ((room->r9469 & 0x1000u) != 0u ||
                   (room->r9469 & 0x0040u) != 0u) {
            variant = 3u;
        } else {
            return;
        }
        for (i = 0u; i < 7u; i++) {
            if (room->shades[i] < 0) {
                int16_t adjusted =
                    (int16_t)(room->shades[i] +
                              room_shade_adjust[i][variant]);

                room->shades[i] = adjusted > 0 ? 0 : adjusted;
            }
        }
    }
}

/*
 * View-column party-relative offset (sub_1152A groups and the
 * 0x7096/0x70FC/0x7162/0x71C8 placement tables agree cell for cell).
 */
static void room_column_offset(unsigned int facing, unsigned int col, int *dx,
                               int *dy)
{
    static const unsigned int groups[7] = {17, 17, 5, 3, 3, 3, 3};
    int lat_dx = 0;
    int lat_dy = 0;
    int dep_dx = 0;
    int dep_dy = 0;
    int sx;
    int sy;
    unsigned int g;

    if (facing == YT2_ROOM_FACING_NORTH) {
        lat_dx = 1;
        dep_dy = 1;
        sx = -8;
        sy = -6;
    } else if (facing == YT2_ROOM_FACING_SOUTH) {
        lat_dx = -1;
        dep_dy = -1;
        sx = 8;
        sy = 6;
    } else if (facing == YT2_ROOM_FACING_EAST) {
        lat_dy = 1;
        dep_dx = -1;
        sx = 6;
        sy = -8;
    } else {
        lat_dy = -1;
        dep_dx = 1;
        sx = -6;
        sy = 8;
    }
    for (g = 0u; g < 7u; g++) {
        if (g == 2u) {
            sx += lat_dx * 6;
            sy += lat_dy * 6;
        } else if (g == 3u) {
            sx += lat_dx;
            sy += lat_dy;
        }
        if (col < groups[g]) {
            *dx = sx + lat_dx * (int)col;
            *dy = sy + lat_dy * (int)col;
            return;
        }
        col -= groups[g];
        sx += dep_dx;
        sy += dep_dy;
    }
    *dx = 0;
    *dy = 0;
}

/* sub_1152A: gather the 51 view columns around the party. */
static void room_gather_view(yt2_room *room)
{
    unsigned int col;

    for (col = 0u; col < YT2_ROOM_VIEW_COLUMNS; col++) {
        int dx;
        int dy;
        yt2_room_cell *cell;

        room_column_offset(room->party->facing, col, &dx, &dy);
        cell = room_window_cell(room, (int)room->party->x + dx,
                                (int)room->party->y + dy);
        if (cell != NULL) {
            room->view[col] = *cell;
        } else {
            room->view[col].terrain = 0u;
            room->view[col].object = 0u;
            room->view[col].misc = 0u;
            room->view[col].flags = 0u;
        }
    }
}

static int room_cell_solid(const yt2_room_cell *cell)
{
    return (cell->flags & YT2_ROOM_CELL_HIDDEN) == 0u &&
           cell->terrain >= YT2_ROOM_SOLID_TERRAIN_LO &&
           cell->terrain <= YT2_ROOM_SOLID_TERRAIN_HI;
}

/* Terrain row: row 60 is mutable (the step animation rewrites it). */
static const uint16_t *room_terrain_row(yt2_room *room, unsigned int terrain)
{
    if (terrain == 60u) {
        return room->terrain60;
    }
    return room_terrain[terrain];
}

static void room_hide_list(yt2_room *room, const int8_t *list)
{
    unsigned int i;

    for (i = 0u; list[i] >= 0; i++) {
        room->view[(unsigned int)list[i]].flags |= YT2_ROOM_CELL_HIDDEN;
    }
}

/* sub_11718: hide columns 0..bp when [col, col+count) is all solid. */
static int room_hide_behind(yt2_room *room, unsigned int col,
                            unsigned int count)
{
    unsigned int i;

    for (i = 0u; i < count; i++) {
        if (!room_cell_solid(&room->view[col + i])) {
            return 0;
        }
    }
    for (i = 0u; i < col; i++) {
        room->view[i].flags |= YT2_ROOM_CELL_HIDDEN;
    }
    return 1;
}

/* sub_11620: occlusion marking on the gathered view line. */
static void room_occlude_view(yt2_room *room)
{
    if (room_hide_behind(room, 0x2Du, 3u) ||
        room_hide_behind(room, 0x2Au, 3u) ||
        room_hide_behind(room, 0x27u, 3u) ||
        room_hide_behind(room, 0x22u, 5u) ||
        room_hide_behind(room, 0x11u, 17u)) {
        /* First solid band wins; the rest still applies below. */
    }
    {
        unsigned int i;

        for (i = 0u; i < 8u; i++) {
            unsigned int cell = room_hide_chain[i].cell;

            if (!room_cell_solid(&room->view[cell]) ||
                !room_cell_solid(&room->view[cell + 1u])) {
                continue;
            }
            room_hide_list(room, room_hide_chain[i].list);
        }
        for (i = 0u; i < 33u; i++) {
            /* Columns 0x32 down to 0x12. */
            unsigned int col = 0x32u - i;
            unsigned int c;

            if (!room_cell_solid(&room->view[col])) {
                continue;
            }
            for (c = 0u; c < 21u; c++) {
                if (room_hide_columns[c].column == col) {
                    room_hide_list(room, room_hide_columns[c].list);
                    break;
                }
            }
        }
    }
}

/* sub_10EA0: neighbour picture equivalence (even/odd pairs match). */
static int room_pic_same(unsigned int base, unsigned int other)
{
    if (base == other) {
        return 1;
    }
    if ((base & 1u) == 0u) {
        return base + 1u == other;
    }
    return base - 1u == other;
}

/* sub_10F10/sub_11036: floor/ceiling detail columns for one group. */
static void room_ground_group(yt2_room *room, yt2_surface *dst,
                              unsigned int start, unsigned int count,
                              unsigned int base_pic, unsigned int base_row,
                              unsigned int mode)
{
    unsigned int i;

    for (i = 0u; i < count; i++) {
        unsigned int col = start + i;
        yt2_room_cell *cell = &room->view[col];
        unsigned int pic;

        room->blit_column = (uint16_t)col;
        if ((cell->flags & YT2_ROOM_CELL_HIDDEN) != 0u) {
            continue;
        }
        if (cell->terrain >= 62u) {
            continue;
        }
        pic = room_terrain_row(room, cell->terrain)[base_row];
        if (room_pic_same(base_pic, pic)) {
            continue;
        }
        room->blit_pic = (uint16_t)pic;
        room->blit_mode = (uint16_t)mode;
        room_draw_column(room, dst);
    }
}

/* sub_1134C: front wall face for one column. */
static void room_front_face(yt2_room *room, yt2_surface *dst,
                            unsigned int col)
{
    yt2_room_cell *cell = &room->view[col];
    unsigned int pic;

    if (cell->terrain == 0u || cell->terrain >= 62u) {
        return;
    }
    pic = room_terrain_row(room, cell->terrain)[2];
    if (pic == 0u) {
        return;
    }
    room->blit_pic = (uint16_t)pic;
    room->blit_mode = 0u;
    room->blit_class = ROOM_CLASS_WALLS;
    room->blit_transparent = 1u;
    room->blit_column = (uint16_t)col;
    room_draw_column(room, dst);
    /* Door cells overdraw front picture 5. */
    if ((cell->flags & 0x2000u) != 0u) {
        room->blit_pic = 5u;
        room_draw_column(room, dst);
    }
}

/* sub_113AB: tile object sprite for one column. */
static void room_tile_object(yt2_room *room, yt2_surface *dst,
                             unsigned int col)
{
    yt2_room_cell *cell = &room->view[col];
    unsigned int facing_pic = 0u;
    unsigned int mode;

    if (cell->object == 0u || cell->object >= 68u) {
        return;
    }
    /* Facing selects the picture word (sub_113AB +0/+2/+4/+6). */
    if (room->party->facing == YT2_ROOM_FACING_NORTH) {
        facing_pic = room_objects[cell->object][0];
    } else if (room->party->facing == YT2_ROOM_FACING_SOUTH) {
        facing_pic = room_objects[cell->object][1];
    } else if (room->party->facing == YT2_ROOM_FACING_EAST) {
        facing_pic = room_objects[cell->object][2];
    } else {
        facing_pic = room_objects[cell->object][3];
    }
    if (facing_pic == 0u) {
        return;
    }
    /* Mode 7 in the v_0CD6..v_0CDC id ranges, else mode 8. */
    mode = 8u;
    if (cell->object <= 0x16u ||
        (cell->object >= 0x33u && cell->object <= 0x43u)) {
        mode = 7u;
    }
    room->blit_pic = (uint16_t)facing_pic;
    room->blit_mode = (uint16_t)mode;
    room->blit_class = ROOM_CLASS_WALLS;
    room->blit_transparent = 1u;
    room->blit_column = (uint16_t)col;
    room_draw_column(room, dst);
    /* An opened container (v_0BA2 = 0x1C) redraws as picture 6. */
    if ((cell->flags & 0x1000u) != 0u && facing_pic == 0x1Cu) {
        room->blit_pic = 6u;
        room->blit_mode = 7u;
        room_draw_column(room, dst);
    }
}

/* sub_136FF: engage a record whose column is near the party. */
static void room_object_engage(yt2_room *room, yt2_room_object *obj,
                               unsigned int col)
{
    (void)room;
    if ((obj->flags & 1u) != 0u) {
        return;
    }
    if (col <= 0x21u || (obj->cull & 0x20u) != 0u) {
        return;
    }
    if ((obj->cull & 0x40u) != 0u) {
        if (col > 0x2Cu) {
            obj->flags |= 1u;
        }
        return;
    }
    if ((obj->cull & 0x80u) != 0u) {
        if (col > 0x29u) {
            obj->flags |= 1u;
        }
        return;
    }
    if ((obj->cull & 0x100u) != 0u) {
        if (col > 0x26u) {
            obj->flags |= 1u;
        }
        return;
    }
    obj->flags |= 1u;
}

/* fn_12E82: find a record by id, engaging it at this column. */
static yt2_room_object *room_object_by_id(yt2_room *room, unsigned int id,
                                          unsigned int col)
{
    unsigned int i;

    for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
        if (room->objects[i].id == id) {
            room_object_engage(room, &room->objects[i], col);
            return &room->objects[i];
        }
    }
    return NULL;
}

static uint16_t room_def16(const uint8_t *def, unsigned int at)
{
    return room_read16(def + at);
}

/* fn_12D72: claim a slot and load the definition for a new object. */
static yt2_room_object *room_object_load(yt2_room *room, unsigned int id,
                                         unsigned int col)
{
    yt2_room_object *obj = NULL;
    const uint8_t *index;
    const uint8_t *def;
    unsigned int def_index;
    unsigned int i;
    int dx;
    int dy;

    for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
        if (room->objects[i].id == 0u) {
            obj = &room->objects[i];
            break;
        }
    }
    if (obj == NULL || id == 0u) {
        return NULL;
    }
    index = room_world_record(room, YT2_WORLD_OBJIDX_OFFSET +
                              (size_t)(id - 1u) * 2u, 2u);
    if (index == NULL) {
        return NULL;
    }
    def_index = room_read16(index);
    def = room_world_record(room, YT2_WORLD_OBJDEF_OFFSET +
                            (size_t)def_index * YT2_WORLD_OBJDEF_SIZE,
                            YT2_WORLD_OBJDEF_SIZE);
    if (def == NULL) {
        return NULL;
    }
    memset(obj, 0, sizeof(*obj));
    obj->id = (uint16_t)id;
    room_column_offset(room->party->facing, col, &dx, &dy);
    obj->x = (uint16_t)((int)room->party->x + dx);
    obj->y = (uint16_t)((int)room->party->y + dy);
    obj->cell = (uint16_t)(((obj->y - (unsigned int)room->origin_y) *
                            YT2_ROOM_WINDOW_ROW_BYTES) +
                           ((obj->x - (unsigned int)room->origin_x) * 8u));
    obj->anim_base = room_def16(def, 0x1Au);
    obj->pic = (uint16_t)(obj->anim_base +
                           yt2_create_rand(&room->rng_seed, room->time_word,
                                           5u));
    obj->group = room_def16(def, 0x60u);
    obj->mode = (obj->group & 1u) != 0u ? 0x0Du : 0x0Au;
    obj->counter = room_def16(def, 0x1Eu);
    obj->def50 = obj->counter;
    obj->range60 = room_def16(def, 0x2Eu);
    obj->over_dx = room_def16(def, 0x36u);
    obj->over_dy = room_def16(def, 0x38u);
    for (i = 0u; i < 6u; i++) {
        obj->shade[i] = def[0x40u + i];
    }
    for (i = 0u; i < 16u; i++) {
        obj->dice[i] = def[0x4Cu + i];
    }
    obj->cull = room_def16(def, 0x62u);
    room_object_engage(room, obj, col);
    return obj;
}

/* fn_1596C: advance one sprite animation frame after its draw. */
static void room_sprite_animate(yt2_room_object *obj)
{
    uint16_t pic = obj->pic;
    uint16_t base = obj->anim_base;

    if ((obj->group & 0x40u) != 0u) {
        return;
    }
    if ((obj->group & 0x20u) != 0u) {
        if ((obj->flags & 4u) != 0u) {
            if (pic != base + 8u) {
                pic++;
            }
        } else if (pic < base + 5u) {
            pic++;
        } else {
            pic = base;
        }
    } else if ((obj->group & 0x10u) != 0u) {
        if ((obj->flags & 4u) != 0u) {
            if (pic != base + 8u) {
                pic++;
            }
        } else if ((obj->group & 8u) != 0u) {
            if (pic < base + 5u) {
                pic++;
            } else {
                pic--;
                obj->group &= (uint16_t)~8u;
            }
        } else if (pic <= base) {
            obj->group |= 8u;
            pic++;
        } else {
            pic--;
        }
    }
    obj->pic = pic;
}

/* sub_11078: draw one runtime object sprite. */
static void room_draw_sprite(yt2_room *room, yt2_surface *dst,
                             yt2_room_object *obj, unsigned int col)
{
    unsigned int i;

    if ((obj->group & 4u) != 0u) {
        room->f5060 |= 1u;
        for (i = 0u; i < 6u; i++) {
            room->shade_src[i] = obj->shade[i];
        }
    }
    room->blit_pic = obj->pic;
    room->blit_class = (obj->group & 1u) != 0u ? ROOM_CLASS_SPRITE3 :
                                                 ROOM_CLASS_SPRITE2;
    room->blit_mode = obj->mode;
    room->blit_transparent = 1u;
    room->blit_column = (uint16_t)col;
    if ((obj->flags & 2u) != 0u) {
        room->blit_pic = (uint16_t)(obj->anim_base + 9u);
    } else if ((obj->flags & 4u) != 0u &&
               obj->pic < obj->anim_base + 6u) {
        room->blit_pic = (uint16_t)(obj->anim_base + 6u);
        obj->pic = room->blit_pic;
    }
    room_draw_column(room, dst);
    room->f5060 &= (uint16_t)~1u;
    if ((obj->flags & 0x10u) != 0u) {
        room->blit_pic = obj->pic2;
        room_draw_column(room, dst);
    }
    if ((obj->flags & 8u) == 0u) {
        if ((obj->flags & 0x3010u) != 0u) {
            obj->pic = (uint16_t)(obj->anim_base + 9u);
            obj->flags |= 2u;
            return;
        }
        room_sprite_animate(obj);
        return;
    }
    obj->flags &= (uint16_t)~8u;
    /* Group-6 overlay at the mode-dependent position. */
    {
        static const int16_t over_x[6] = {-22, 25, 82, -7, 50, 107};
        int mode = (int)room->blit_mode - 9;
        int x;
        int y;

        if (mode < 0 || mode > 5) {
            return;
        }
        x = over_x[mode] + (int)obj->over_dx;
        y = (mode == 3 ? 8 : 34) + (int)obj->over_dy;
        room->blit_pic = 0x20u;
        if ((obj->flags2 & 0x2000u) != 0u) {
            room->blit_pic = 0x13u;
        } else if ((obj->flags2 & 0x4000u) != 0u) {
            room->blit_pic = 0x12u;
        }
        room->blit_class = ROOM_CLASS_OVERLAY;
        room->blit_transparent = 2u;
        room_draw_picture(room, dst, x, y);
        obj->flags2 &= 0x1FFFu;
    }
}

/* sub_114DC: draw the runtime object of one view column, if any. */
static void room_column_object(yt2_room *room, yt2_surface *dst,
                               unsigned int col)
{
    yt2_room_cell *cell = &room->view[col];
    yt2_room_object *obj;

    if (col < 0x11u || (cell->flags & YT2_ROOM_CELL_OBJECT) == 0u) {
        return;
    }
    if (col == 0x31u) {
        return;
    }
    obj = room_object_by_id(room, cell->misc, col);
    if (obj == NULL) {
        obj = room_object_load(room, cell->misc, col);
    }
    if (obj == NULL) {
        return;
    }
    room_draw_sprite(room, dst, obj, col);
}

static int room_terrain_open(yt2_room *room, unsigned int terrain)
{
    return terrain < 62u && room_terrain_row(room, terrain)[2] == 0u &&
           room_terrain_row(room, terrain)[3] == 0u;
}

/* Side wall face (sub_11239 modes 3/4) against an open neighbour. */
static void room_wall_side(yt2_room *room, yt2_surface *dst, unsigned int col,
                           unsigned int other, unsigned int mode)
{
    yt2_room_cell *cell = &room->view[col];
    yt2_room_cell *next = &room->view[other];
    unsigned int pic;

    if ((next->flags & YT2_ROOM_CELL_HIDDEN) != 0u) {
        return;
    }
    if (!room_terrain_open(room, next->terrain)) {
        return;
    }
    if (cell->terrain >= 62u) {
        return;
    }
    pic = room_terrain_row(room, cell->terrain)[2];
    if (pic == 0u) {
        return;
    }
    room->blit_pic = (uint16_t)pic;
    room->blit_mode = (uint16_t)mode;
    room->blit_class = ROOM_CLASS_WALLS;
    room->blit_transparent = 1u;
    room->blit_column = (uint16_t)col;
    room_draw_column(room, dst);
}

/* sub_11239: front/side faces plus objects for one column group. */
static void room_wall_group(yt2_room *room, yt2_surface *dst,
                            unsigned int start, unsigned int count,
                            int16_t shade)
{
    unsigned int half = (count - 1u) / 2u;
    unsigned int i;
    unsigned int col;

    room->blit_shade = shade;
    room->blit_class = ROOM_CLASS_WALLS;
    room->blit_transparent = 1u;
    for (i = 0u; i < half; i++) {
        col = start + i;
        room->blit_column = (uint16_t)col;
        if ((room->view[col].flags & YT2_ROOM_CELL_HIDDEN) != 0u) {
            continue;
        }
        room_front_face(room, dst, col);
        room_wall_side(room, dst, col, col + 1u, 3u);
        room_tile_object(room, dst, col);
        room_column_object(room, dst, col);
    }
    for (i = 0u; i < half; i++) {
        col = start + count - 1u - i;
        room->blit_column = (uint16_t)col;
        if ((room->view[col].flags & YT2_ROOM_CELL_HIDDEN) != 0u) {
            continue;
        }
        room_front_face(room, dst, col);
        room_wall_side(room, dst, col, col - 1u, 4u);
        room_tile_object(room, dst, col);
        room_column_object(room, dst, col);
    }
    col = start + half;
    room->blit_column = (uint16_t)col;
    if ((room->view[col].flags & YT2_ROOM_CELL_HIDDEN) == 0u) {
        room_front_face(room, dst, col);
        room_tile_object(room, dst, col);
        room_column_object(room, dst, col);
    }
}

/* sub_1143B: far group (mode-6 strips plus objects). */
static void room_wall_far(yt2_room *room, yt2_surface *dst,
                          unsigned int start, int16_t shade)
{
    unsigned int col = start;
    unsigned int i;

    room->blit_shade = shade;
    for (i = 0u; i < 2u; i++) {
        yt2_room_cell *cell = &room->view[col];

        room->blit_column = (uint16_t)col;
        if ((cell->flags & YT2_ROOM_CELL_HIDDEN) == 0u &&
            cell->terrain < 62u) {
            unsigned int pic = room_terrain_row(room, cell->terrain)[3];

            if (pic != 0u) {
                room->blit_pic = (uint16_t)pic;
                room->blit_mode = 6u;
                room->blit_offset =
                    (uint16_t)(room_terrain_row(room, cell->terrain)[4] + i * 7u);
                room->blit_class = ROOM_CLASS_OVERLAY;
                room->blit_transparent = 1u;
                room_draw_column(room, dst);
            }
        }
        col += 2u;
        room->blit_column = (uint16_t)(col - 1u);
    }
    col = start + 1u;
    room->blit_column = (uint16_t)col;
    room_tile_object(room, dst, col);
    room_column_object(room, dst, col);
    if ((room->f5064 & 0x1000u) != 0u) {
        for (i = 0u; i < 3u; i++) {
            if (room->near_objects[i].id != 0u) {
                room_draw_sprite(room, dst, &room->near_objects[i], col);
            }
        }
    }
}

/* sub_10F53: ceiling/floor full blits plus the floor detail columns. */
static void room_render_ground(yt2_room *room, yt2_surface *dst)
{
    static const unsigned int counts[7] = {17, 17, 5, 3, 3, 3, 3};
    yt2_room_cell *here;
    unsigned int ceil_pic;
    unsigned int floor_pic;
    unsigned int g;
    unsigned int col;

    here = room_window_cell(room, (int)room->party->x, (int)room->party->y);
    if (here == NULL || here->terrain >= 62u) {
        return;
    }
    ceil_pic = room_terrain_row(room, here->terrain)[1];
    if (ceil_pic == 0u && room->party->facing != YT2_ROOM_FACING_NORTH &&
        room->party->facing != YT2_ROOM_FACING_SOUTH) {
        ceil_pic = 1u;
    }
    floor_pic = room_terrain_row(room, here->terrain)[0];
    room->blit_class = ROOM_CLASS_CEIL;
    room->blit_transparent = 0u;
    room->blit_pic = (uint16_t)ceil_pic;
    room_draw_picture(room, dst, 8, 8);
    room_shade_gradient(room, dst, 0);
    room->blit_class = ROOM_CLASS_FLOOR;
    room->blit_pic = (uint16_t)floor_pic;
    room_draw_picture(room, dst, 8, 70);
    room_shade_gradient(room, dst, 1);
    col = 0u;
    for (g = 0u; g < 7u; g++) {
        room->blit_shade = room->shades[g];
        room->blit_class = ROOM_CLASS_FLOOR;
        room->blit_transparent = 0u;
        room_ground_group(room, dst, col, counts[g], floor_pic, 0u, 1u);
        col += counts[g];
    }
}

/* sub_10EB2: ceiling detail columns. */
static void room_render_ceiling(yt2_room *room, yt2_surface *dst)
{
    static const unsigned int counts[7] = {17, 17, 5, 3, 3, 3, 3};
    yt2_room_cell *here;
    unsigned int ceil_pic;
    unsigned int g;
    unsigned int col;

    here = room_window_cell(room, (int)room->party->x, (int)room->party->y);
    if (here == NULL || here->terrain >= 62u) {
        return;
    }
    ceil_pic = room_terrain_row(room, here->terrain)[1];
    if (ceil_pic == 0u && room->party->facing != YT2_ROOM_FACING_NORTH &&
        room->party->facing != YT2_ROOM_FACING_SOUTH) {
        ceil_pic = 1u;
    }
    col = 0u;
    for (g = 0u; g < 7u; g++) {
        room->blit_shade = room->shades[g];
        room->blit_class = ROOM_CLASS_CEIL;
        room->blit_transparent = 0u;
        room_ground_group(room, dst, col, counts[g], ceil_pic, 1u, 2u);
        col += counts[g];
    }
}

/* sub_111DB: wall faces for all seven groups. */
static void room_render_walls(yt2_room *room, yt2_surface *dst)
{
    static const unsigned int counts[7] = {17, 17, 5, 3, 3, 3, 3};
    unsigned int g;
    unsigned int col = 0u;

    for (g = 0u; g < 6u; g++) {
        room_wall_group(room, dst, col, counts[g], room->shades[g]);
        col += counts[g];
    }
    room_wall_far(room, dst, col, room->shades[6]);
}

/* fn_10E42 without the panel tail: full first-person composition. */
static void room_render_3d(yt2_room *room, yt2_surface *dst)
{
    room_gather_view(room);
    room_occlude_view(room);
    room_select_shades(room);
    room_render_ground(room, dst);
    room_render_ceiling(room, dst);
    room_render_walls(room, dst);
    room->f505e &= (uint16_t)~0x0400u;
}

/* sub_12A06: one 0x26-wide 5-row status bar. */
static void room_draw_bar(yt2_surface *dst, int x, int y, uint16_t cur,
                          uint16_t max, uint8_t fg, uint8_t bg)
{
    unsigned int width;
    unsigned int row;

    if ((int16_t)cur <= 0 || max == 0u) {
        width = 0u;
    } else {
        /* width = 3800 / ((100 * max) / cur), 16-bit long division. */
        uint32_t product = (uint32_t)100u * max;
        uint32_t hi = product >> 16;
        uint32_t lo = product & 0xFFFFu;
        uint32_t denom = cur;
        uint32_t quot;

        if (cur > max) {
            denom = max;
            fg = (uint8_t)(fg + 2u);
        }
        if (hi != 0u && hi >= denom) {
            hi = denom;
            denom = denom + 1u;
        }
        quot = (hi * 65536u + lo) / denom;
        if (quot == 0u) {
            width = 0x26u;
        } else {
            width = 3800u / quot;
            if (width == 0u) {
                width = 1u;
            }
            if (width > 0x26u) {
                width = 0x26u;
            }
        }
    }
    for (row = 0u; row < 5u; row++) {
        unsigned int i;

        for (i = 0u; i < 0x26u; i++) {
            dst->pixels[(size_t)(y + (int)row) * dst->stride +
                        (size_t)(x + (int)i)] =
                i < width ? fg : bg;
        }
    }
}

/* fn_1274F: one portrait slot with bars, icons and name cell. Reads the
   live roster like vanilla reads DGROUP, never the file image. */
static void room_draw_portrait(yt2_room *room, yt2_surface *dst,
                               unsigned int slot)
{
    const uint16_t *layout = room_portraits[slot];
    unsigned int member = room->party->assigned[slot];
    const yt2_character *character;
    const uint8_t *pic;
    const yt2_picture_group *group;
    uint16_t status;
    uint16_t ready;
    uint16_t hp;
    uint16_t hp_max;
    uint16_t mp;
    uint16_t mp_max;
    uint16_t carried;
    uint16_t capacity;
    uint16_t icon_sel;
    uint16_t overlay;
    uint32_t buffs = 0u;
    unsigned int i;

    if (member == 0u || member > YT2_ROSTER_SLOTS) {
        return;
    }
    member--;
    character = &room->party->roster[member];
    group = yt2_pictures_group(room->pictures, 7u);
    pic = yt2_picture_data(room->pictures, 7u, character->portrait_pic);
    if (group == NULL || pic == NULL) {
        return;
    }
    yt2_surface_blit(dst, (int)layout[0], (int)layout[2], pic, group->width,
                     group->height);
    status = character->status;
    overlay = character->overlay;
    if ((status & 0x1C40u) != 0u || (overlay & 0x8000u) != 0u) {
        pic = yt2_picture_data(room->pictures, 7u, 0xE0u);
        if (pic != NULL) {
            yt2_surface_blit_transparent(dst, (int)layout[0], (int)layout[2],
                                         pic, group->width, group->height,
                                         0xFFu);
        }
    }
    hp = character->current.combat.hit_points;
    hp_max = character->maximum.combat.hit_points;
    if ((status & 0x40u) != 0u) {
        hp = 0u;
    }
    room_draw_bar(dst, (int)layout[35] + 0, (int)layout[37] + 0, hp, hp_max,
                  0x59u, 6u);
    mp = character->current.combat.magic_points;
    mp_max = character->maximum.combat.magic_points;
    room_draw_bar(dst, (int)layout[35], (int)layout[37] + 5, mp, mp_max,
                  0xCAu, 6u);
    carried = character->carried_weight;
    capacity = character->current.combat.carry_capacity;
    room_draw_bar(dst, (int)layout[35], (int)layout[37] + 10, carried,
                  capacity, 0x86u, 6u);
    /* Status icons (sub_1291F): three flag-selected group-9 pictures. */
    {
        static const uint16_t masks[3][4] = {
            {0x2000u, 0x4000u, 0x8000u, 0u},
            {0x0400u, 0x0800u, 0x1000u, 0u},
            {0x0080u, 0x0100u, 0x0200u, 0u}
        };
        static const uint8_t pics[3][4] = {
            {7, 6, 5, 4},
            {10, 9, 8, 4},
            {13, 12, 11, 4}
        };
        static const unsigned int rows[3] = {5, 10, 15};

        for (i = 0u; i < 3u; i++) {
            unsigned int p = 3u;
            unsigned int k;

            for (k = 0u; k < 3u; k++) {
                if ((status & masks[i][k]) != 0u) {
                    p = k;
                    break;
                }
            }
            pic = yt2_picture_data(room->pictures, 9u, pics[i][p]);
            if (pic != NULL) {
                const yt2_picture_group *g9 =
                    yt2_pictures_group(room->pictures, 9u);

                yt2_surface_blit(dst, (int)layout[rows[i]],
                                 (int)layout[rows[i] + 2], pic,
                                 g9->width, g9->height);
            }
        }
    }
    /* Buff icon: picture 0xE when any +0x20..0x30 counter is set. */
    for (i = 0u; i < YT2_BUFF_SLOTS; i++) {
        buffs += character->buffs[i];
    }
    pic = yt2_picture_data(room->pictures, 9u, buffs != 0u ? 0x0Eu : 0x15u);
    if (pic != NULL) {
        const yt2_picture_group *g9 =
            yt2_pictures_group(room->pictures, 9u);

        yt2_surface_blit(dst, (int)layout[25], (int)layout[27], pic,
                         g9->width, g9->height);
    }
    /* Ready cell: group-9 picture 0x15, or the fn_1A03C glyph. */
    ready = character->ready;
    icon_sel = character->icon_sel;
    pic = yt2_picture_data(room->pictures, 9u,
                           icon_sel != 0u ? 0x14u : 0x15u);
    if (pic != NULL) {
        const yt2_picture_group *g9 =
            yt2_pictures_group(room->pictures, 9u);

        yt2_surface_blit(dst, (int)layout[30], (int)layout[32], pic,
                         g9->width, g9->height);
    }
    if ((status & 0x40u) == 0u && ready != 0u) {
        char glyph[2] = {(char)0x54u, '\0'};

        yt2_text_draw(dst, (int)layout[20] + 2, (int)layout[22] + 2, glyph,
                      0x0Fu);
    } else {
        pic = yt2_picture_data(room->pictures, 9u, 0x15u);
        if (pic != NULL) {
            const yt2_picture_group *g9 =
                yt2_pictures_group(room->pictures, 9u);

            yt2_surface_blit(dst, (int)layout[20], (int)layout[22], pic,
                             g9->width, g9->height);
        }
    }
    if ((status & 0x40u) != 0u) {
        yt2_text_draw(dst, (int)layout[35] + 8, (int)layout[37], "DEAD",
                      0x0Fu);
    }
}

/* fn_11754: compass label. */
static void room_draw_compass(yt2_room *room, yt2_surface *dst)
{
    unsigned int facing = room->party->facing;
    const char *label = room_compass[3];

    if ((room->r946f & 0x0400u) == 0u) {
        return;
    }
    if (facing == YT2_ROOM_FACING_NORTH) {
        label = room_compass[0];
    } else if (facing == YT2_ROOM_FACING_SOUTH) {
        label = room_compass[1];
    } else if (facing == YT2_ROOM_FACING_EAST) {
        label = room_compass[2];
    }
    yt2_text_draw(dst, 0xF1, 0x57, label, 0x0Fu);
}

int yt2_room_render(yt2_room *room, yt2_surface *surface)
{
    unsigned int i;

    if (room == NULL || surface == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(surface->pixels, 0,
           (size_t)surface->height * surface->stride);
    room->blit_pic = 1u;
    room->blit_class = 0u;
    room->blit_transparent = 0u;
    room_draw_picture(room, surface, 1, 1);
    room_render_3d(room, surface);
    if ((room->r946f & 0x1000u) != 0u) {
        /* Combat panel (fn_1755C): the combat slice draws it. */
    } else if ((room->r946f & 0x2000u) != 0u) {
        room->blit_pic = 0x15u;
        room->blit_class = ROOM_CLASS_OVERLAY;
        room->blit_transparent = 1u;
        room_draw_picture(room, surface, 0xF8, 9);
    }
    for (i = 0u; i < 4u; i++) {
        room_draw_portrait(room, surface, i);
    }
    room_draw_compass(room, surface);
    if ((room->r946f & 0x4000u) != 0u) {
        /* Minimap (fn_11836/fn_117AC): a later slice draws it. */
    }
    return YT2_STATUS_OK;
}

/* fn_00E6C: destination terrain check (0 move, 1 bump, 2 silent bump). */
static unsigned int room_terrain_check(unsigned int terrain)
{
    if (terrain <= 1u) {
        return 2u;
    }
    if (terrain <= 5u) {
        return 1u;
    }
    if (terrain >= YT2_ROOM_ANIM_TERRAIN_LO &&
        terrain <= YT2_ROOM_ANIM_TERRAIN_HI) {
        return 1u;
    }
    if (terrain >= 11u && terrain <= 15u) {
        return 1u;
    }
    if (terrain > 57u) {
        return 1u;
    }
    return 0u;
}

/* fn_00EA6: destination object check (3 blocked, else 0). */
static unsigned int room_object_check(unsigned int object)
{
    if (object >= 0x21u && object <= 0x23u) {
        return 3u;
    }
    if (object == 0x25u) {
        return 3u;
    }
    if (object >= 0x27u && object <= 0x2Au) {
        return 3u;
    }
    return 0u;
}

/* fn_00ECD: monster approach collision on the world grid. */
static unsigned int room_ai_blocked(yt2_room *room, unsigned int x,
                                    unsigned int y)
{
    const yt2_tile *tile;

    if (x >= YT2_WORLD_TILES_X || y >= YT2_WORLD_TILES_Y ||
        room->world == NULL || room->world->tiles == NULL) {
        return 1u;
    }
    tile = &room->world->tiles[y * YT2_WORLD_TILES_X + x];
    if (tile->terrain >= 2u && tile->terrain <= 15u) {
        return 1u;
    }
    if (tile->object == 0u) {
        return 0u;
    }
    if (tile->object >= 0x11u && tile->object <= 0x14u) {
        return 0u;
    }
    if (tile->object >= 0x3Fu && tile->object <= 0x43u) {
        return 0u;
    }
    return 2u;
}

/* fn_1B91E: monster step collision on the window. */
static unsigned int room_ai_step_blocked(yt2_room *room,
                                         yt2_room_object *obj, int dx, int dy)
{
    yt2_room_cell *cell;
    unsigned int code;

    cell = room_window_cell(room, (int)obj->x + dx, (int)obj->y + dy);
    if (cell == NULL) {
        return 1u;
    }
    if ((cell->flags & 0x0C00u) != 0u) {
        return 1u;
    }
    if ((cell->flags & YT2_ROOM_CELL_DOOR) != 0u) {
        return (obj->cull & 0x10u) != 0u ? 2u : 1u;
    }
    if (cell->terrain >= YT2_ROOM_ANIM_TERRAIN_LO &&
        cell->terrain <= YT2_ROOM_ANIM_TERRAIN_HI) {
        return (obj->cull & 0x14u) != 0u ? 2u : 1u;
    }
    if (cell->terrain <= 1u) {
        return (obj->cull & 0x1Au) != 0u ? 0u : 1u;
    }
    if (cell->object >= 0x27u && cell->object <= 0x2Au) {
        return 1u;
    }
    if ((obj->cull & 0x10u) != 0u) {
        return 0u;
    }
    if (cell->object == 0x25u) {
        return (obj->cull & 8u) != 0u ? 0u : 1u;
    }
    code = room_terrain_check(cell->terrain);
    if (code != 0u) {
        return 1u;
    }
    return room_object_check(cell->object);
}

/* sub_1360F: apply a monster step. */
static void room_ai_apply(yt2_room *room, yt2_room_object *obj, int dx,
                          int dy)
{
    yt2_room_cell *from;
    yt2_room_cell *to;

    from = room_window_cell(room, (int)obj->x, (int)obj->y);
    if (from != NULL && from->misc == obj->id) {
        from->flags &= (uint16_t)~YT2_ROOM_CELL_OBJECT;
        from->misc = 0u;
    }
    obj->x = (uint16_t)((int)obj->x + dx);
    obj->y = (uint16_t)((int)obj->y + dy);
    obj->cell = (uint16_t)(((obj->y - (unsigned int)room->origin_y) *
                            YT2_ROOM_WINDOW_ROW_BYTES) +
                           ((obj->x - (unsigned int)room->origin_x) * 8u));
    to = room_window_cell(room, (int)obj->x, (int)obj->y);
    if (to != NULL) {
        to->flags |= YT2_ROOM_CELL_OBJECT;
        to->misc = obj->id;
    }
}

/* fn_12FF7: attack cooldown (0 move, 1 attack, 2 wait). */
static unsigned int room_attack_check(yt2_room_object *obj)
{
    if ((obj->flags & 0xFC10u) == 0u) {
        return 0u;
    }
    obj->counter = (uint16_t)(obj->counter - obj->step);
    if ((int16_t)obj->counter > 0) {
        if ((obj->flags & 0x3010u) == 0u) {
            return 0u;
        }
        obj->counter = (uint16_t)(obj->counter - obj->step);
        if ((int16_t)obj->counter > 0) {
            return 2u;
        }
    }
    if ((obj->flags & 0x3010u) != 0u) {
        obj->cooldown--;
        if ((int16_t)obj->cooldown > 0) {
            return 2u;
        }
        obj->flags &= 0x03EDu;
        obj->pic2 = 0u;
        obj->step = 0u;
        obj->cooldown = 0u;
        obj->pic = obj->anim_base;
        return 0u;
    }
    obj->counter = 0u;
    return 1u;
}

/* fn_13056: monster turn after every party step. */
static void room_monster_step(yt2_room *room, yt2_room_object *obj);
static void room_monster_engage(yt2_room *room, yt2_room_object *obj);

static void room_monster_turn(yt2_room *room)
{
    unsigned int i;

    room->f5062 &= (uint16_t)~0x30u;
    for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
        yt2_room_object *obj = &room->objects[i];
        unsigned int check;

        if (obj->id == 0u || (obj->flags & 1u) == 0u) {
            continue;
        }
        check = room_attack_check(obj);
        if (check == 1u) {
            /* fn_12EA0/fn_13420: the combat slice resolves attacks. */
            continue;
        }
        if (check != 0u) {
            continue;
        }
        if (obj->range60 == 0u || (obj->flags & 0x0800u) != 0u) {
            room_monster_step(room, obj);
            continue;
        }
        /* Same-axis approach probe (line of sight, up to 5 cells). */
        {
            int same_x = obj->x == room->party->x && obj->y != room->party->y;
            int same_y = obj->y == room->party->y && obj->x != room->party->x;
            int dir;
            int at;
            int target;
            int reached = 0;
            int tries;

            if (!same_x && !same_y) {
                room_monster_step(room, obj);
                continue;
            }
            if (same_x) {
                dir = obj->y < room->party->y ? 1 : -1;
                at = (int)obj->y;
                target = (int)room->party->y;
                obj->flags2 |= 0x0800u;
                if (dir < 0) {
                    obj->flags2 |= 0x0400u;
                    obj->flags2 &= (uint16_t)~0x0800u;
                }
            } else {
                dir = obj->x < room->party->x ? 1 : -1;
                at = (int)obj->x;
                target = (int)room->party->x;
                obj->flags2 |= 0x0100u;
                if (dir < 0) {
                    obj->flags2 |= 0x0200u;
                    obj->flags2 &= (uint16_t)~0x0100u;
                }
            }
            for (tries = 0; tries < 5; tries++) {
                unsigned int nx;
                unsigned int ny;

                at += dir;
                if (at == target) {
                    reached = 1;
                    break;
                }
                nx = same_x ? obj->x : (unsigned int)at;
                ny = same_x ? (unsigned int)at : obj->y;
                if (room_ai_blocked(room, nx, ny) != 0u) {
                    break;
                }
            }
            /* Hesitation roll only when the probe reaches the party. */
            if (reached) {
                uint16_t roll =
                    yt2_create_rand(&room->rng_seed, room->time_word, 100u);
                uint16_t threshold = 5u;

                if ((obj->cull & 0x1000u) != 0u) {
                    threshold = 90u;
                } else if ((obj->cull & 0x0800u) != 0u) {
                    threshold = 75u;
                } else if ((obj->cull & 0x0400u) != 0u) {
                    threshold = 50u;
                } else if ((obj->cull & 0x0200u) != 0u) {
                    threshold = 25u;
                }
                if (roll <= threshold) {
                    obj->flags2 |= 0x1000u;
                    room->f5062 |= 0x10u;
                    continue;
                }
            }
        }
        room_monster_step(room, obj);
    }
}

/* fn_13056 tail (loc_131C9): one step toward the party. */
static void room_monster_step(yt2_room *room, yt2_room_object *obj)
{
    int dx = 0;
    int dy = 0;
    int adj_y = 0;
    unsigned int blocked;

    obj->flags2 &= (uint16_t)~0x1F00u;
    room->f5062 &= (uint16_t)~0x40u;
    if ((unsigned int)((int)obj->y + 1) == room->party->y) {
        dy = 1;
        adj_y = 1;
    } else if ((unsigned int)((int)obj->y - 1) == room->party->y) {
        dy = -1;
        adj_y = 1;
    } else if (obj->x == room->party->x && obj->y == room->party->y) {
        return;
    } else if (obj->x != room->party->x) {
        dx = obj->x < room->party->x ? 1 : -1;
    } else {
        dy = obj->y < room->party->y ? 1 : -1;
    }
    if (adj_y) {
        room->f5062 |= 0x40u;
    }
    blocked = room_ai_step_blocked(room, obj, dx, dy);
    if (blocked == 0u) {
        yt2_room_cell *dest =
            room_window_cell(room, (int)obj->x + dx, (int)obj->y + dy);
        yt2_room_cell *here =
            room_window_cell(room, (int)room->party->x, (int)room->party->y);

        if (dest != NULL && dest == here) {
            room_monster_engage(room, obj);
            return;
        }
        room_ai_apply(room, obj, dx, dy);
        return;
    }
    if (blocked == 2u) {
        room_ai_apply(room, obj, dx * 2, dy * 2);
        return;
    }
    /* Retry across the other axis (loc_1328D). */
    if ((room->f5062 & 0x40u) != 0u) {
        room->f5062 &= (uint16_t)~0x40u;
        dx = 0;
        dy = 0;
        if (obj->x != room->party->x) {
            dx = obj->x < room->party->x ? 1 : -1;
        } else if (obj->y != room->party->y) {
            dy = obj->y < room->party->y ? 1 : -1;
        } else {
            return;
        }
        blocked = room_ai_step_blocked(room, obj, dx, dy);
        if (blocked == 0u) {
            room_ai_apply(room, obj, dx, dy);
        } else if (blocked == 2u) {
            room_ai_apply(room, obj, dx * 2, dy * 2);
        }
        return;
    }
    if (dx == 0) {
        return;
    }
    if (room->party->y == obj->y) {
        return;
    }
    dy = obj->y < room->party->y ? 1 : -1;
    blocked = room_ai_step_blocked(room, obj, 0, dy);
    if (blocked == 0u) {
        room_ai_apply(room, obj, 0, dy);
    } else if (blocked == 2u) {
        room_ai_apply(room, obj, 0, dy * 2);
    }
}

/* fn_13056 loc_132D6: a monster steps onto the party cell. */
static void room_monster_engage(yt2_room *room, yt2_room_object *obj)
{
    yt2_room_cell *cell;

    if ((room->f5064 & 0x1000u) != 0u) {
        /* Second engager (v_51BA shuffle): the combat slice owns it. */
        return;
    }
    room->f5064 |= 0x1000u;
    room->near_objects[1] = *obj;
    memset(obj, 0, sizeof(*obj));
    room->f5064 |= (uint16_t)(room->near_objects[1].group & 0xE000u);
    cell = room_window_cell(room, (int)room->party->x, (int)room->party->y);
    if (cell != NULL) {
        cell->flags &= (uint16_t)~YT2_ROOM_CELL_OBJECT;
        cell->misc = 0u;
    }
}

/* sub_013DB: flash one arrow cell, then present. */
static void room_flash_arrow(yt2_room *room, yt2_surface *dst,
                             unsigned int arrow, unsigned int pic,
                             yt2_room_present_fn present, void *context)
{
    room->blit_pic = (uint16_t)pic;
    room->blit_class = ROOM_CLASS_ICON;
    room->blit_transparent = 0u;
    room_draw_picture(room, dst, (int)room_arrows[arrow][0],
                      (int)room_arrows[arrow][2]);
    if (present != NULL) {
        present(context);
    }
}

/* fn_1203A: mark the 3x3 around the party visited (window + save). */
static void room_probe_neighbours(yt2_room *room)
{
    int dx;
    int dy;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int x = (int)room->party->x + dx;
            int y = (int)room->party->y + dy;
            yt2_room_cell *cell = room_window_cell(room, x, y);

            room_set_visited(room, (unsigned int)x, (unsigned int)y);
            if (cell != NULL) {
                cell->flags |= YT2_ROOM_CELL_VISITED;
            }
        }
    }
}

/* fn_0ABEC: average mapping/navigation/survival sets the skill flags. */
static void room_update_skill_flags(yt2_room *room)
{
    unsigned int mapping = 0u;
    unsigned int navigation = 0u;
    unsigned int survival = 0u;
    unsigned int count = 0u;
    unsigned int i;

    room->r946f &= 0x70FFu;
    for (i = 0u; i < 4u; i++) {
        unsigned int member = room->party->assigned[i];
        const yt2_character *ch;

        if (member == 0u || member > YT2_ROSTER_SLOTS) {
            continue;
        }
        ch = &room->party->roster[member - 1u];
        if ((ch->status & 0x1C40u) != 0u) {
            continue;
        }
        mapping += ch->current.skills.mapping;
        navigation += ch->current.skills.navigation;
        survival += ch->current.skills.survival;
        count++;
    }
    if (count != 0u) {
        mapping /= count;
        navigation /= count;
        survival /= count;
    }
    if (room->save != NULL && room->save->bytes != NULL &&
        room->save->size >= 0x4Cu) {
        room->save->bytes[0x46u] = (uint8_t)(mapping & 0xFFu);
        room->save->bytes[0x47u] = (uint8_t)(mapping >> 8);
        room->save->bytes[0x48u] = (uint8_t)(navigation & 0xFFu);
        room->save->bytes[0x49u] = (uint8_t)(navigation >> 8);
        room->save->bytes[0x4Au] = (uint8_t)(survival & 0xFFu);
        room->save->bytes[0x4Bu] = (uint8_t)(survival >> 8);
    }
    if (mapping < 45u) {
        return;
    }
    room->r946f |= 0x0400u;
    if (mapping < 50u) {
        return;
    }
    room->r946f |= 0x8000u;
    if (mapping < 60u) {
        return;
    }
    room->r946f |= 0x0200u;
    if (mapping < 70u) {
        return;
    }
    room->r946f |= 0x0800u;
    if (mapping < 80u) {
        return;
    }
    room->r946f |= 0x0100u;
}

/* Pick up the record on the party cell, if any (loc_012D4). */
static void room_pickup(yt2_room *room)
{
    yt2_room_cell *here =
        room_window_cell(room, (int)room->party->x, (int)room->party->y);
    unsigned int i;

    if (here == NULL) {
        return;
    }
    for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
        yt2_room_object *obj = &room->objects[i];
        unsigned int off =
            ((obj->y - (unsigned int)room->origin_y) *
             YT2_ROOM_WINDOW_ROW_BYTES) +
            ((obj->x - (unsigned int)room->origin_x) * 8u);
        unsigned int party_off =
            (((unsigned int)room->party->y - (unsigned int)room->origin_y) *
             YT2_ROOM_WINDOW_ROW_BYTES) +
            (((unsigned int)room->party->x - (unsigned int)room->origin_x) *
             8u);

        if (obj->id == 0u || off != party_off) {
            continue;
        }
        room->f5064 |= 0x1000u;
        room->near_objects[1] = *obj;
        memset(obj, 0, sizeof(*obj));
        here->flags &= (uint16_t)~YT2_ROOM_CELL_OBJECT;
        here->misc = 0u;
        room->f5064 |= room->near_objects[1].group;
        return;
    }
}

/* fn_09F1C: step-cell actions (teleporters; the rest is later slices). */
static void room_cell_actions(yt2_room *room, yt2_surface *dst,
                              yt2_room_present_fn present, void *context)
{
    yt2_room_cell *here =
        room_window_cell(room, (int)room->party->x, (int)room->party->y);
    unsigned int a;

    room->keycode = 0u;
    if (here == NULL) {
        return;
    }
    for (a = 0u; room_actions[a][0] != 0xFFFFu; a++) {
        if ((room_actions[a][1] & 0x8000u) != 0u) {
            if (here->terrain != room_actions[a][0]) {
                continue;
            }
        } else if (here->object != room_actions[a][0]) {
            continue;
        }
        if ((room_actions[a][1] & 0x4000u) != 0u) {
            room->party->x = room_actions[a][2];
            room->party->y = room_actions[a][3];
            room->last_sfx = 0x0Au;
            room_load_window(room);
            room_stamp_window(room);
            yt2_room_render(room, dst);
            if (present != NULL) {
                present(context);
            }
            return;
        }
        /* Message/damage actions: the combat slice resolves them. */
        return;
    }
}

/* One cell of movement plus the neighbour probe (sub_013FF tail). */
static void room_move_once(yt2_room *room)
{
    room->party->x = (uint16_t)((int)room->party->x + room->step_dx);
    room->party->y = (uint16_t)((int)room->party->y + room->step_dy);
    room_probe_neighbours(room);
}

/* Shared post-move tail (loc_012D4..loc_013D4). */
static void room_post_move(yt2_room *room, yt2_surface *dst,
                           yt2_room_present_fn present, void *context)
{
    room_pickup(room);
    yt2_room_render(room, dst);
    if (present != NULL) {
        present(context);
    }
    if ((room->f5064 & 0x1000u) == 0u && (room->f505e & 0x1800u) != 0u) {
        room_monster_turn(room);
        yt2_room_render(room, dst);
        if (present != NULL) {
            present(context);
        }
    }
    room_cell_actions(room, dst, present, context);
    /* Edge reload past the 9-cell inner margin. */
    if ((int)room->party->x <= room->origin_x + 9 ||
        (int)room->party->x >= room->origin_x + 0x45 ||
        (int)room->party->y <= room->origin_y + 9 ||
        (int)room->party->y >= room->origin_y + 0x45) {
        room_load_window(room);
        room_stamp_window(room);
        yt2_room_render(room, dst);
        if (present != NULL) {
            present(context);
        }
    }
    room->f505e &= (uint16_t)~0x1800u;
}

/* sub_013FF step animation through terrain 6..11 (KeyCode 0x48 only). */
static void room_step_animation(yt2_room *room, yt2_surface *dst,
                                yt2_room_cell *dest,
                                yt2_room_present_fn present, void *context)
{
    const uint16_t *drow;
    const uint16_t *vrow;

    if (room->keycode != YT2_ROOM_SCAN_UP) {
        room->last_sfx = 5u;
        return;
    }
    if (dest->terrain >= 62u) {
        return;
    }
    drow = room_terrain_row(room, dest->terrain);
    /* View column 42 lends the floor/ceiling; column 44 shows row 60. */
    vrow = room_terrain_row(room, room->view[42].terrain < 62u ?
                            room->view[42].terrain : 0u);
    room->terrain60[0] = vrow[0];
    room->terrain60[1] = vrow[1];
    room->terrain60[2] = (uint16_t)(drow[2] + 1u);
    room->terrain60[3] = drow[3];
    room->terrain60[4] = drow[4];
    room->terrain60[5] = drow[5];
    room->view[44].terrain = 60u;
    /* fn_10E6A redraws the viewport over the current frame (UI stays). */
    room_select_shades(room);
    room_render_ground(room, dst);
    room_render_ceiling(room, dst);
    room_render_walls(room, dst);
    if (present != NULL) {
        present(context);
    }
    room->last_sfx = 5u;
}

int yt2_room_key(yt2_room *room, yt2_surface *surface, unsigned int scancode,
                 yt2_room_present_fn present, void *context)
{
    unsigned int facing = room->party->facing;
    unsigned int arrow = 1u;
    unsigned int flash = 0x14u;
    unsigned int restore = 0x13u;
    int dx = 0;
    int dy = 0;
    int dcell = 0;
    unsigned int turn = 0u;

    if (room == NULL || surface == NULL) {
        return YT2_STATUS_ERROR;
    }
    room->f505e &= (uint16_t)~0x1800u;
    room->keycode = (uint8_t)scancode;
    if (scancode == YT2_ROOM_SCAN_UP) {
        if (facing == YT2_ROOM_FACING_NORTH) {
            dy = -1;
            dcell = -0x270;
        } else if (facing == YT2_ROOM_FACING_SOUTH) {
            dy = 1;
            dcell = 0x270;
        } else if (facing == YT2_ROOM_FACING_EAST) {
            dx = 1;
            dcell = 8;
        } else {
            dx = -1;
            dcell = -8;
        }
    } else if (scancode == YT2_ROOM_SCAN_DOWN) {
        arrow = 4u;
        flash = 0x1Au;
        restore = 0x19u;
        if (facing == YT2_ROOM_FACING_NORTH) {
            dy = 1;
            dcell = 0x270;
        } else if (facing == YT2_ROOM_FACING_SOUTH) {
            dy = -1;
            dcell = -0x270;
        } else if (facing == YT2_ROOM_FACING_EAST) {
            dx = -1;
            dcell = -8;
        } else {
            dx = 1;
            dcell = 8;
        }
    } else if (scancode == YT2_ROOM_SCAN_LEFT) {
        arrow = 0u;
        flash = 0x12u;
        restore = 0x11u;
        turn = 1u;
        if (facing == YT2_ROOM_FACING_NORTH) {
            facing = YT2_ROOM_FACING_WEST;
        } else if (facing == YT2_ROOM_FACING_WEST) {
            facing = YT2_ROOM_FACING_SOUTH;
        } else if (facing == YT2_ROOM_FACING_SOUTH) {
            facing = YT2_ROOM_FACING_EAST;
        } else {
            facing = YT2_ROOM_FACING_NORTH;
        }
    } else if (scancode == YT2_ROOM_SCAN_RIGHT) {
        arrow = 2u;
        flash = 0x16u;
        restore = 0x15u;
        turn = 1u;
        if (facing == YT2_ROOM_FACING_NORTH) {
            facing = YT2_ROOM_FACING_EAST;
        } else if (facing == YT2_ROOM_FACING_EAST) {
            facing = YT2_ROOM_FACING_SOUTH;
        } else if (facing == YT2_ROOM_FACING_SOUTH) {
            facing = YT2_ROOM_FACING_WEST;
        } else {
            facing = YT2_ROOM_FACING_NORTH;
        }
    } else {
        return YT2_STATUS_OK;
    }
    if ((room->f5064 & 0x1000u) != 0u) {
        room_flash_arrow(room, surface, arrow, restore, present, context);
        return YT2_STATUS_OK;
    }
    room_flash_arrow(room, surface, arrow, flash, present, context);
    room->step_dx = (int16_t)dx;
    room->step_dy = (int16_t)dy;
    room->step_cell = (int16_t)dcell;
    if (!turn) {
        int nx = (int)room->party->x + dx;
        int ny = (int)room->party->y + dy;

        if (nx < (int)YT2_ROOM_BOUND_X0 || nx > (int)YT2_ROOM_BOUND_X1 ||
            ny < (int)YT2_ROOM_BOUND_Y0 || ny > (int)YT2_ROOM_BOUND_Y1) {
            room_flash_arrow(room, surface, arrow, restore, present,
                             context);
            return YT2_STATUS_OK;
        }
    }
    room->party->facing = (uint16_t)facing;
    if (turn) {
        room->last_sfx = 5u;
        room_move_once(room);
        room_post_move(room, surface, present, context);
        return YT2_STATUS_OK;
    }
    {
        yt2_room_cell *dest =
            room_window_cell(room, (int)room->party->x + dx,
                             (int)room->party->y + dy);

        if (dest == NULL) {
            room_flash_arrow(room, surface, arrow, restore, present,
                             context);
            return YT2_STATUS_OK;
        }
        if (dy != 0) {
            room->f505e |= 0x0800u;
        } else {
            room->f505e |= 0x1000u;
        }
        if ((dest->flags & YT2_ROOM_CELL_DOOR) != 0u) {
            /* Door message (fn_0784D): a later slice prints it. */
            room->last_sfx = 6u;
            room_flash_arrow(room, surface, arrow, restore, present,
                             context);
            return YT2_STATUS_OK;
        }
        if (dest->terrain >= YT2_ROOM_ANIM_TERRAIN_LO &&
            dest->terrain <= YT2_ROOM_ANIM_TERRAIN_HI) {
            room_gather_view(room);
            room_occlude_view(room);
            room_step_animation(room, surface, dest, present, context);
            room_move_once(room);
            room_move_once(room);
            room_post_move(room, surface, present, context);
            return YT2_STATUS_OK;
        }
        {
            unsigned int code = room_terrain_check(dest->terrain);

            if (code == 0u) {
                code = room_object_check(dest->object);
            }
            if (code != 0u) {
                if (code != 2u) {
                    room->last_sfx = 6u;
                }
                room_flash_arrow(room, surface, arrow, restore, present,
                                 context);
                return YT2_STATUS_OK;
            }
        }
        room->last_sfx = 5u;
        room_move_once(room);
        room_post_move(room, surface, present, context);
        return YT2_STATUS_OK;
    }
}

/* fn_10073: clock tick (scene +1, date rollover at the wrap). */
static void room_clock_tick(yt2_room *room)
{
    room->party->scene++;
    if (room->party->scene == YT2_ROOM_SCENE_WRAP) {
        room->party->scene = 1u;
        room->party->day++;
        if (room->party->day == 31u) {
            room->party->day = 1u;
            room->party->month++;
            if (room->party->month == 13u) {
                room->party->month = 1u;
                room->party->year++;
            }
        }
    }
    room->cd_clock = YT2_ROOM_CLOCK_RELOAD;
}

/* sub_10208: sky window step plus a shade refresh. */
static void room_sky_tick(yt2_room *room)
{
    room->sky = (uint16_t)(room->sky + room->sky_step);
    if (room->sky_left > 0u) {
        room->sky_left--;
        if (room->sky_left == 0u) {
            room->gates &= (uint16_t)~YT2_ROOM_GATE_SKY;
        }
    }
    room_select_shades(room);
    room->cd_sky = YT2_ROOM_CLOCK_RELOAD;
}

int yt2_room_tick(yt2_room *room)
{
    int dirty = 0;

    if (room == NULL) {
        return YT2_STATUS_ERROR;
    }
    room->tick++;
    if ((room->gates & YT2_ROOM_GATE_CLOCK) != 0u) {
        if (room->cd_clock > 0u) {
            room->cd_clock--;
        }
        if (room->cd_clock == 0u) {
            room_clock_tick(room);
            dirty = 1;
        }
    }
    if ((room->gates & YT2_ROOM_GATE_SKY) != 0u) {
        if (room->cd_sky > 0u) {
            room->cd_sky--;
        }
        if (room->cd_sky == 0u) {
            room_sky_tick(room);
            dirty = 1;
        }
    }
    if ((room->gates & YT2_ROOM_GATE_ROTATION) != 0u) {
        if (room->cd_rotation > 0u) {
            room->cd_rotation--;
        }
        if (room->cd_rotation == 0u) {
            room->cd_rotation = YT2_ROOM_ROTATION_RELOAD;
            room->rotation_phase++;
            dirty = 1;
        }
    }
    room->tick_dirty = dirty;
    return YT2_STATUS_OK;
}

int yt2_room_tick_dirty(const yt2_room *room)
{
    if (room == NULL) {
        return 0;
    }
    return room->tick_dirty;
}

int yt2_room_palette(const yt2_room *room, yt2_palette *palette)
{
    const yt2_palette *base;
    const uint8_t *sky;
    unsigned int i;

    if (room == NULL || palette == NULL) {
        return YT2_STATUS_ERROR;
    }
    base = yt2_world_palette(room->world, 0u);
    if (base == NULL) {
        return YT2_STATUS_ERROR;
    }
    *palette = *base;
    {
        uint8_t anim[YT2_WORLD_ANIM_SIZE];
        const uint8_t *rec = room_world_record(
            room, YT2_WORLD_ANIM_OFFSET, YT2_WORLD_ANIM_SIZE);

        if (rec == NULL) {
            return YT2_STATUS_ERROR;
        }
        for (i = 0u; i < YT2_WORLD_ANIM_SIZE; i++) {
            anim[i] = rec[i];
        }
        if (yt2_palette_rotate(palette, anim,
                               room->rotation_phase) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    sky = room_world_record(room, YT2_WORLD_SCENE_PAL_OFFSET + room->sky, 96u);
    if (sky == NULL) {
        return YT2_STATUS_ERROR;
    }
    for (i = 0u; i < 96u; i++) {
        palette->rgb[0xE0u * 3u + i] = sky[i];
    }
    return YT2_STATUS_OK;
}

int yt2_room_init(yt2_room *room, const yt2_world *world,
                  const yt2_pictures *pictures, yt2_save *save,
                  yt2_party *party, uint16_t time_word)
{
    unsigned int i;

    if (room == NULL || world == NULL || pictures == NULL || save == NULL ||
        party == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(room, 0, sizeof(*room));
    room->world = world;
    room->pictures = pictures;
    room->save = save;
    room->party = party;
    room->time_word = time_word;
    for (i = 0u; i < 6u; i++) {
        room->terrain60[i] = 0u;
    }
    room->r946f = 0x2000u;
    return YT2_STATUS_OK;
}

/* Menu E: fresh CURGAME chunks, party placed at Port Hope.
   The world chunks are zeroed like the original's fresh CURGAME,
   but membership (flags, assigned[], active[]) is kept: the original
   can drop it because it never loads CURGAME back (boot truncates it
   unread and the session continues from DGROUP), while B2 loads
   CURGAME at boot so the file is a live save and must resume. */
static void room_embark_fresh(yt2_room *room)
{
    uint8_t *bytes = room->save->bytes;
    size_t size = room->save->size;

    if (bytes == NULL) {
        return;
    }
    if (YT2_SAVE_OBJECTS_OFFSET + YT2_SAVE_OBJECTS_SIZE <= size) {
        memset(bytes + YT2_SAVE_OBJECTS_OFFSET, 0, YT2_SAVE_OBJECTS_SIZE);
    }
    if (YT2_SAVE_LEVELS_OFFSET + YT2_SAVE_LEVELS_SIZE <= size) {
        memset(bytes + YT2_SAVE_LEVELS_OFFSET, 0, YT2_SAVE_LEVELS_SIZE);
    }
    if (YT2_SAVE_WORLD_TABLE_OFFSET + YT2_SAVE_WORLD_TABLE_SIZE <= size) {
        memset(bytes + YT2_SAVE_WORLD_TABLE_OFFSET, 0,
               YT2_SAVE_WORLD_TABLE_SIZE);
    }
    if (YT2_SAVE_WORLD_INDEX_OFFSET + YT2_SAVE_WORLD_INDEX_SIZE <= size) {
        memset(bytes + YT2_SAVE_WORLD_INDEX_OFFSET, 0,
               YT2_SAVE_WORLD_INDEX_SIZE);
    }
    if (YT2_SAVE_WORLD_MAP_OFFSET + YT2_SAVE_WORLD_MAP_SIZE <= size) {
        memset(bytes + YT2_SAVE_WORLD_MAP_OFFSET, 0, YT2_SAVE_WORLD_MAP_SIZE);
    }
    if (YT2_SAVE_CACHE_OFFSET + YT2_SAVE_CACHE_SIZE <= size) {
        memset(bytes + YT2_SAVE_CACHE_OFFSET, 0, YT2_SAVE_CACHE_SIZE);
    }
    room->party->x = 0xA6u;
    room->party->y = 0x24u;
    room->party->facing = YT2_ROOM_FACING_WEST;
    room->party->day = 4u;
    room->party->month = 11u;
    room->party->year = 0x222u;
    room->party->scene = YT2_ROOM_EMBARK_SCENE;
    yt2_party_write(room->save->bytes, room->save->size, room->party);
    /* Level-record cursor (v_95FD/v_95FF, header 0xAE/0xB0). */
    if (size >= 0xB2u) {
        bytes[0xAEu] = 2u;
        bytes[0xAFu] = 0u;
        bytes[0xB0u] = 0u;
        bytes[0xB1u] = 0u;
    }
}

int yt2_room_embark(yt2_room *room, yt2_surface *surface,
                    yt2_room_present_fn present, void *context)
{
    if (room == NULL || surface == NULL) {
        return YT2_STATUS_ERROR;
    }
    room_embark_fresh(room);
    room->f5064 &= 0x0FFFu;
    if (room_load_window(room) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    room_stamp_window(room);
    room_probe_neighbours(room);
    /* fn_0FF27: enable the clock gates with full countdowns. */
    room->gates = YT2_ROOM_GATE_CLOCK | YT2_ROOM_GATE_ROTATION;
    room->sky = YT2_ROOM_EMBARK_SKY;
    room->sky_step = 3;
    room->sky_left = YT2_ROOM_EMBARK_SKY_LEFT;
    if (room->sky_left != 0u) {
        room->gates |= YT2_ROOM_GATE_SKY;
    }
    room->cd_clock = YT2_ROOM_CLOCK_RELOAD;
    room->cd_sky = YT2_ROOM_CLOCK_RELOAD;
    room->cd_rotation = YT2_ROOM_ROTATION_RELOAD;
    room->speed_left = room->party->game_speed;
    room->r9469 = 0u;
    room->r946f = 0x2000u;
    room_update_skill_flags(room);
    yt2_room_render(room, surface);
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

int yt2_room_relocate(yt2_room *room)
{
    if (room == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (room_load_window(room) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    room_stamp_window(room);
    return YT2_STATUS_OK;
}
