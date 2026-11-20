#include "yt2/ingamemenu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt2/room.h"

#include "yt2/endian.h"
#include "yt2/file.h"
#include "yt2/text.h"

/*
 * The pre-explore in-game menu (fn_0EBD6) and the save/load slot panel.
 *
 * Function map to the disassembly:
 *   scan/slot records    sub_01D84 (boot scan), 6CC4h records
 *   paint                fn_0EBD6 entry, sub_0F40A (rows), sub_0F58A (titles),
 *                        sub_0F762 (slot list), sub_0F3E7 (checkbox),
 *                        fn_0A834 items (confirms), fn_06F86 (clears)
 *   input                fn_0EBD6 loop (keys/clicks/gates), sub_0F49D (slot
 *                        browser), fn_0D29A (name editor, 24 chars)
 *   handlers             sub_0F356 (speed), sub_0FB19 (music), sub_0FB61
 *                        (sound), sub_0FAEB (quit), sub_0F7C9 (new game)
 *   save/load            save sequel + sub_0F823, load sequel + 0EFAF flow
 *
 * Literal style: countable quantities decimal; flags, masks, palette
 * colors and ASCII codes hex.
 */

/* DS:5CCCh: slot rows 1..6 plus the footer/option buttons. */
typedef struct ingamemenu_region {
    int x0;
    int x1;
    int y0;
    int y1;
    unsigned int id;
} ingamemenu_region;

static const ingamemenu_region ingamemenu_table[] = {
    { 39, 201, 27, 33, 1 }, { 39, 201, 38, 44, 2 },
    { 39, 201, 49, 55, 3 }, { 39, 201, 60, 66, 4 },
    { 39, 201, 71, 77, 5 }, { 39, 201, 82, 88, 6 },
    { 42, 67, 93, 100, 7 }, { 79, 105, 93, 100, 8 },
    { 116, 165, 93, 100, 9 }, { 164, 202, 117, 124, 15 },
    { 31, 74, 105, 113, 11 }, { 80, 123, 105, 113, 12 },
    { 129, 209, 105, 113, 13 }, { 36, 132, 117, 124, 14 },
    { 177, 197, 93, 100, 10 },
};

static int hit_test(int x, int y)
{
    for (size_t i = 0;
         i < sizeof(ingamemenu_table) / sizeof(ingamemenu_table[0]); i++) {
        if (x >= ingamemenu_table[i].x0 && x <= ingamemenu_table[i].x1 &&
            y >= ingamemenu_table[i].y0 && y <= ingamemenu_table[i].y1) {
            return (int)ingamemenu_table[i].id;
        }
    }
    return 0;
}

/* DS:5D64h: confirm rows (Y row id 5, N row id 7). */
static const ingamemenu_region confirm_table[] = {
    { 240, 311, 96, 101, 1 }, { 240, 311, 102, 107, 2 },
    { 240, 311, 108, 113, 3 }, { 240, 311, 114, 119, 4 },
    { 240, 311, 120, 125, 5 }, { 240, 311, 126, 131, 6 },
    { 240, 311, 132, 137, 7 }, { 240, 311, 138, 143, 8 },
};

static int confirm_hit(int x, int y)
{
    for (size_t i = 0;
         i < sizeof(confirm_table) / sizeof(confirm_table[0]); i++) {
        if (x >= confirm_table[i].x0 && x <= confirm_table[i].x1 &&
            y >= confirm_table[i].y0 && y <= confirm_table[i].y1) {
            return (int)confirm_table[i].id;
        }
    }
    return 0;
}

/* fn_0A834 records 1/2/31: three text groups each (prompt + YES + NO). */
typedef struct camp_dialog_item {
    uint8_t color;
    int x;
    int y;
    const char *lines[3];
    unsigned int line_count;
} camp_dialog_item;

typedef struct camp_dialog {
    camp_dialog_item items[3];
} camp_dialog;

static const camp_dialog camp_dialogs[] = {
    /* id 1 (quit): "DO YOU WANT / TO EXIT TO / DOS NOW?" */
    { { { 0x8Au, 240, 96,
          { "DO YOU WANT", "TO EXIT TO", "DOS NOW?" }, 3u },
        { 0xAAu, 240, 120, { "    YES", NULL, NULL }, 1u },
        { 0x5Bu, 240, 132, { "    NO", NULL, NULL }, 1u } } },
    /* id 2 (save): "DO YOU WANT / TO SAVE THIS / GAME NOW?" */
    { { { 0x8Au, 240, 96,
          { "DO YOU WANT", "TO SAVE THIS", "GAME NOW?" }, 3u },
        { 0xAAu, 240, 120, { "    YES", NULL, NULL }, 1u },
        { 0x5Bu, 240, 132, { "    NO", NULL, NULL }, 1u } } },
    /* id 3 (load): "DO YOU WANT / TO LOAD THIS / GAME NOW?" */
    { { { 0x8Au, 240, 96,
          { "DO YOU WANT", "TO LOAD THIS", "GAME NOW?" }, 3u },
        { 0xAAu, 240, 120, { "    YES", NULL, NULL }, 1u },
        { 0x5Bu, 240, 132, { "    NO", NULL, NULL }, 1u } } },
    /* id 31 (new game): "START A NEW / GAME NOW?" */
    { { { 0x8Au, 240, 96,
          { "START A NEW", "GAME NOW?", NULL }, 2u },
        { 0xAAu, 240, 120, { "    YES", NULL, NULL }, 1u },
        { 0x5Bu, 240, 132, { "    NO", NULL, NULL }, 1u } } },
};

static const camp_dialog *dialog_by_id(unsigned int id)
{
    switch (id) {
    case 1u:
        return &camp_dialogs[0];
    case 2u:
        return &camp_dialogs[1];
    case 3u:
        return &camp_dialogs[2];
    case 31u:
        return &camp_dialogs[3];
    default:
        return NULL;
    }
}

/* Cache class to picture group (yt2_picture_class, DS:7834h). */
static unsigned int class_group(unsigned int class)
{
    switch (class) {
    case 0x00u:
        return 0u;
    case 0x10u:
        return 1u;
    case 0x60u:
        return 6u;
    case 0x80u:
        return 8u;
    case 0x90u:
        return 9u;
    default:
        return 0u;
    }
}

/* fn_19E12: blit one picture (0B26 nonzero: 0xFF transparent). */
static void camp_blit(yt2_ingamemenu *ingamemenu, unsigned int class, unsigned int pic,
                      int x, int y, int transparent)
{
    const yt2_picture_group *info;
    const uint8_t *pixels;
    unsigned int group = class_group(class);

    info = yt2_pictures_group(ingamemenu->pictures, group);
    pixels = (info == NULL) ? NULL
                            : yt2_picture_data(ingamemenu->pictures, group, pic);
    if (pixels == NULL || info == NULL) {
        return;
    }
    if (transparent) {
        yt2_surface_blit_transparent(ingamemenu->surface, x, y, pixels, info->width,
                                     info->height, 0xFFu);
    } else {
        yt2_surface_blit(ingamemenu->surface, x, y, pixels, info->width,
                         info->height);
    }
}

/* fn_13E74: plain transparent line. */
static void paint_line(yt2_ingamemenu *ingamemenu, int x, int y, const char *text,
                       uint8_t color)
{
    yt2_text_draw(ingamemenu->surface, x, y, text, color);
}

/* fn_13E74 with v_0B26 == 0: opaque line over background 0x04. */
static void paint_line_opaque(yt2_ingamemenu *ingamemenu, int x, int y, const char *text,
                              uint8_t color)
{
    yt2_text_draw_opaque(ingamemenu->surface, x, y, text, color, 0x04u);
}

/* fn_13E8C: cx lines, x reset and +6 y per line. */
static void paint_lines(yt2_ingamemenu *ingamemenu, int x, int y,
                        const char *const *lines, unsigned int count,
                        uint8_t color)
{
    for (unsigned int i = 0u; i < count; i++) {
        paint_line(ingamemenu, x, y + (int)(i * 6u), lines[i], color);
    }
}

/* sub_0F3E7: group-9 checkbox glyph. */
static void paint_check(yt2_ingamemenu *ingamemenu, int x, int y, unsigned int pic)
{
    camp_blit(ingamemenu, 0x90u, pic, x, y, 1);
}

/* fn_06F86: clear the dialog area (small path; the combat path clears the
   wide rect instead). */
static void paint_clear(yt2_ingamemenu *ingamemenu)
{
    if (ingamemenu->combat) {
        for (int y = 86; y < 86 + 109; y++) {
            for (int x = 240; x < 240 + 73; x++) {
                if (x >= 0 && x < (int)ingamemenu->surface->width && y >= 0 &&
                    y < (int)ingamemenu->surface->height) {
                    ingamemenu->surface->pixels[(size_t)y * ingamemenu->surface->stride +
                                          (size_t)x] = 0x04u;
                }
            }
        }
        return;
    }
    for (int y = 96; y < 96 + 60; y++) {
        for (int x = 240; x < 240 + 72; x++) {
            ingamemenu->surface->pixels[(size_t)y * ingamemenu->surface->stride +
                                  (size_t)x] = 0x04u;
        }
    }
    for (int y = 87; y < 87 + 6; y++) {
        for (int x = 241; x < 241 + 72; x++) {
            ingamemenu->surface->pixels[(size_t)y * ingamemenu->surface->stride +
                                  (size_t)x] = 0x04u;
        }
    }
}

void yt2_ingamemenu_wipe_side_panels(yt2_ingamemenu *ingamemenu)
{
    if (ingamemenu == NULL) {
        return;
    }
    paint_clear(ingamemenu);
}

/* Option rows (sub_0F63B/0F684/0F69F/0F6BA/0F6D5/0F6F0/0F70B/0F72C/0F747):
   the background picture carries the enabled labels; these repaint the
   disabled variants in color 6. */
static void paint_row_save(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 44, 95, "SAVE", 6u);
}

static void paint_row_load(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 81, 95, "LOAD", 6u);
}

static void paint_row_dos(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 179, 95, "DOS", 6u);
}

static void paint_row_music(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 33, 107, "MUSIC", 6u);
}

static void paint_row_sound(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 82, 107, "SOUND", 6u);
}

static void paint_row_animation(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 38, 119, "ANIMATION", 6u);
    /* Opaque blanks (v_0B26 == 0): erase the speed value underneath. */
    paint_line_opaque(ingamemenu, 97, 119, "      ", 6u);
}

static void paint_row_newgame(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 118, 95, "NEW GAME", 6u);
}

static void paint_row_return(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 166, 119, "RETURN", 6u);
}

static void paint_row_ordering(yt2_ingamemenu *ingamemenu)
{
    paint_line(ingamemenu, 131, 107, "ORDERING INFO", 6u);
}

/* sub_0FAA8: the game-speed value over the ANIMATION row. */
static void paint_speed_value(yt2_ingamemenu *ingamemenu)
{
    const char *value = "SLOW  ";

    if (ingamemenu->party->game_speed == 1u) {
        value = "FAST  ";
    } else if (ingamemenu->party->game_speed == 5u) {
        value = "MEDIUM";
    }
    paint_line(ingamemenu, 97, 119, value, 0x8Au);
}

/* sub_0F40A: conditional rows plus the music/sound checkboxes. */
static void paint_rows(yt2_ingamemenu *ingamemenu)
{
    if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_SAVE) == 0u) {
        paint_row_save(ingamemenu);
    }
    if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_LOAD) == 0u) {
        paint_row_load(ingamemenu);
    }
    if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_NEWGAME) == 0u) {
        paint_row_newgame(ingamemenu);
    }
    if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_QUIT) == 0u) {
        paint_row_dos(ingamemenu);
    }
    if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_SPEED) == 0u) {
        paint_row_animation(ingamemenu);
    } else {
        paint_speed_value(ingamemenu);
    }
    if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_CONTINUE) == 0u) {
        paint_row_return(ingamemenu);
    }
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_MUSIC_PRESENT) == 0u) {
        paint_row_music(ingamemenu);
    }
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_SFX_PRESENT) == 0u) {
        paint_row_sound(ingamemenu);
    }
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_SFX_ON) != 0u) {
        paint_check(ingamemenu, 115, 106, 0x12u);
    }
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_MUSIC_ON) != 0u) {
        paint_check(ingamemenu, 66, 106, 0x12u);
    }
    /* fn_00000 always returns 1: the ORDERING INFO row always paints. */
    paint_row_ordering(ingamemenu);
}

/* sub_0F58A titles plus the row subset for modes 1..4. */
static void paint_title(yt2_ingamemenu *ingamemenu, unsigned int mode)
{
    switch (mode) {
    case 1u:
        paint_line(ingamemenu, 44, 95, "SAVE", 0x8Au);
        paint_row_load(ingamemenu);
        paint_row_dos(ingamemenu);
        paint_row_newgame(ingamemenu);
        break;
    case 2u:
        paint_line(ingamemenu, 81, 95, "LOAD", 0x8Au);
        paint_row_save(ingamemenu);
        paint_row_dos(ingamemenu);
        paint_row_newgame(ingamemenu);
        break;
    case 3u:
        paint_line(ingamemenu, 179, 95, "DOS", 0x8Au);
        paint_row_save(ingamemenu);
        paint_row_load(ingamemenu);
        paint_row_newgame(ingamemenu);
        break;
    case 4u:
        paint_line(ingamemenu, 118, 95, "NEW GAME", 0x8Au);
        paint_row_save(ingamemenu);
        paint_row_load(ingamemenu);
        paint_row_dos(ingamemenu);
        break;
    default:
        break;
    }
    paint_row_music(ingamemenu);
    paint_row_sound(ingamemenu);
    paint_row_return(ingamemenu);
    paint_row_animation(ingamemenu);
    paint_row_ordering(ingamemenu);
}

/* One slot line: NUL scan like fn_13E74. The EMPTY default has no NUL in
   25 bytes, so vanilla overruns into the next record's file digit (26
   glyphs for slots 1-5); slot 6 stops at the zeroed view line. In the
   explore panel a dirty view line would print garbage instead; B2 always
   stops clean, the only known divergence on this screen. */
static void paint_slot_text(yt2_ingamemenu *ingamemenu, unsigned int index, int x, int y,
                            uint8_t color)
{
    char line[27];
    unsigned int count = 0u;

    while (count < YT2_INGAMEMENU_NAME_TEXT &&
           ingamemenu->slots[index].text[count] != '\0') {
        line[count] = ingamemenu->slots[index].text[count];
        count++;
    }
    if (count == YT2_INGAMEMENU_NAME_TEXT && index + 1u < YT2_INGAMEMENU_SLOTS) {
        line[count++] = ingamemenu->slots[index + 1u].file_digit;
    }
    line[count] = '\0';
    paint_line_opaque(ingamemenu, x, y, line, color);
}

/* sub_0F762: the six slot lines (0x40 selected: color 0x7B, else 0x0F). */
static void paint_slots(yt2_ingamemenu *ingamemenu)
{
    for (unsigned int i = 0u; i < YT2_INGAMEMENU_SLOTS; i++) {
        const ingamemenu_region *region = &ingamemenu_table[i];
        uint8_t color = 0x0Fu;

        if ((ingamemenu->slots[i].flags & YT2_INGAMEMENU_SLOT_SELECTED) != 0u) {
            color = 0x7Bu;
        }
        paint_slot_text(ingamemenu, i, region->x0 + 12, region->y0 + 1, color);
    }
}

/* fn_0A834 paint: the confirm items over the dialog area. */
static void paint_dialog(yt2_ingamemenu *ingamemenu, unsigned int id)
{
    const camp_dialog *dialog = dialog_by_id(id);

    if (dialog == NULL) {
        return;
    }
    for (unsigned int i = 0u; i < 3u; i++) {
        paint_lines(ingamemenu, dialog->items[i].x, dialog->items[i].y,
                    dialog->items[i].lines, dialog->items[i].line_count,
                    dialog->items[i].color);
    }
}

/* The base screen chrome: panel icon, option grid, conditional rows. The
   boot underlay (backdrop + panel) is painted once by begin; repaints
   never clear, so cancelled slot text ghosts like vanilla. */
static void paint_base(yt2_ingamemenu *ingamemenu)
{
    camp_blit(ingamemenu, 0x80u, 0x0Au, 296, 67, 0);
    camp_blit(ingamemenu, 0x10u, 0u, 24, 23, 1);
    paint_rows(ingamemenu);
}

static void camp_log_sfx(yt2_ingamemenu *ingamemenu, unsigned int id)
{
    if (ingamemenu->sfx_count < YT2_INGAMEMENU_SFX_LOG) {
        ingamemenu->sfx[ingamemenu->sfx_count++] = id;
    }
}

/* sub_01D84: scan the six slot files into the records. */
int yt2_ingamemenu_scan_slots(yt2_ingamemenu_slot *slots, const char *dir)
{
    int any = 0;

    if (slots == NULL || dir == NULL) {
        return 0;
    }
    for (unsigned int i = 0u; i < YT2_INGAMEMENU_SLOTS; i++) {
        char path[1088];
        uint8_t name[YT2_INGAMEMENU_NAME_TEXT];
        size_t got = 0u;

        slots[i].file_digit = (char)('1' + i);
        slots[i].flags = 0u;
        memcpy(slots[i].text, "        - EMPTY -       ",
               YT2_INGAMEMENU_NAME_TEXT);
        slots[i].text[YT2_INGAMEMENU_NAME_TEXT] = '\0';
        snprintf(path, sizeof(path), "%s/SAVGAME%u", dir, i + 1u);
        if (yt2_file_load_prefix(path, name, sizeof(name), &got) !=
                YT2_STATUS_OK ||
            got == 0u) {
            continue;
        }
        slots[i].flags |= YT2_INGAMEMENU_SLOT_EXISTS;
        any = 1;
        /* A raw header copy, NULs included: an empty submitted name
           zeroes byte 0 (B1's editor commit), so the row renders
           blank exactly like B1's ASCIIZ copy does. */
        memcpy(slots[i].text, name,
               got < YT2_INGAMEMENU_NAME_TEXT ? got : YT2_INGAMEMENU_NAME_TEXT);
        slots[i].text[YT2_INGAMEMENU_NAME_TEXT] = '\0';
    }
    return any;
}

/* View states: base options, slot lists, overwrite prompt, name editor,
   confirm dialog. */
enum {
    INGAMEMENU_VIEW_BASE = 0,
    INGAMEMENU_VIEW_SAVE_LIST,
    INGAMEMENU_VIEW_LOAD_LIST,
    INGAMEMENU_VIEW_OVERWRITE,
    INGAMEMENU_VIEW_EDITOR,
    INGAMEMENU_VIEW_CONFIRM
};

int yt2_ingamemenu_begin(yt2_ingamemenu *ingamemenu, yt2_party *party, yt2_save *save,
                   yt2_ingamemenu_slot *slots, const yt2_pictures *pictures,
                   yt2_surface *surface, const char *dir,
                   uint16_t enable_mask, int combat, int explore,
                   unsigned int cursor)
{
    size_t dir_length;

    if (ingamemenu == NULL || party == NULL || save == NULL || slots == NULL ||
        pictures == NULL || surface == NULL || dir == NULL) {
        return YT2_STATUS_ERROR;
    }
    dir_length = strlen(dir);
    if (dir_length >= sizeof(ingamemenu->dir)) {
        return YT2_STATUS_ERROR;
    }
    memset(ingamemenu, 0, sizeof(*ingamemenu));
    ingamemenu->party = party;
    ingamemenu->save = save;
    ingamemenu->slots = slots;
    ingamemenu->pictures = pictures;
    ingamemenu->surface = surface;
    memcpy(ingamemenu->dir, dir, dir_length + 1u);
    ingamemenu->enable_mask = enable_mask;
    ingamemenu->combat = combat;
    ingamemenu->view = INGAMEMENU_VIEW_BASE;
    if (cursor != 0u) {
        /* Mid-drag entry skips the screen like vanilla. */
        ingamemenu->exit = YT2_INGAMEMENU_EXIT_CONTINUE;
        return YT2_STATUS_OK;
    }
    if ((enable_mask & 1u) == 0u) {
        camp_log_sfx(ingamemenu, 1u);
    }
    if (!explore) {
        camp_blit(ingamemenu, 0x00u, 1u, 1, 1, 0);
        yt2_room_paint_portraits(ingamemenu->party, ingamemenu->pictures,
                                ingamemenu->surface);
        camp_blit(ingamemenu, 0x60u, 0x15u, 248, 9, 1);
    }
    paint_base(ingamemenu);
    return YT2_STATUS_OK;
}

/* sub_0F356: cycle game speed 5 -> 1 -> 9 -> 5 (MEDIUM/FAST/SLOW). */
static void camp_speed(yt2_ingamemenu *ingamemenu)
{
    if (ingamemenu->party->game_speed == 1u) {
        ingamemenu->party->game_speed = 9u;
    } else if (ingamemenu->party->game_speed == 5u) {
        ingamemenu->party->game_speed = 1u;
    } else {
        ingamemenu->party->game_speed = 5u;
    }
    paint_speed_value(ingamemenu);
}

/* sub_0FB19: toggle music (state-only until the audio engine). */
static void camp_music(yt2_ingamemenu *ingamemenu)
{
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_MUSIC_PRESENT) == 0u) {
        return;
    }
    camp_log_sfx(ingamemenu, 4u);
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_MUSIC_ON) != 0u) {
        ingamemenu->party->sound_state &= (uint16_t)~YT2_INGAMEMENU_SOUND_MUSIC_ON;
        paint_check(ingamemenu, 66, 106, 0x11u);
    } else {
        ingamemenu->party->sound_state |= YT2_INGAMEMENU_SOUND_MUSIC_ON;
        paint_check(ingamemenu, 66, 106, 0x12u);
    }
}

/* sub_0FB61: toggle sound effects (state-only until the audio engine). */
static void camp_sound_toggle(yt2_ingamemenu *ingamemenu)
{
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_SFX_PRESENT) == 0u) {
        return;
    }
    if ((ingamemenu->party->sound_state & YT2_INGAMEMENU_SOUND_SFX_ON) != 0u) {
        ingamemenu->party->sound_state &= (uint16_t)~YT2_INGAMEMENU_SOUND_SFX_ON;
        paint_check(ingamemenu, 115, 106, 0x11u);
    } else {
        ingamemenu->party->sound_state |= YT2_INGAMEMENU_SOUND_SFX_ON;
        paint_check(ingamemenu, 115, 106, 0x12u);
    }
    camp_log_sfx(ingamemenu, 4u);
}

/* Resolve the picked slot like sub_0F3AB (record pointer + line coords). */
static yt2_ingamemenu_slot *camp_resolve(yt2_ingamemenu *ingamemenu, int *x, int *y)
{
    if (ingamemenu->slot < 1u || ingamemenu->slot > YT2_INGAMEMENU_SLOTS) {
        return NULL;
    }
    if (x != NULL) {
        *x = ingamemenu_table[ingamemenu->slot - 1u].x0 + 12;
    }
    if (y != NULL) {
        *y = ingamemenu_table[ingamemenu->slot - 1u].y0 + 1;
    }
    return &ingamemenu->slots[ingamemenu->slot - 1u];
}

/* The slot-list screens (save ax=1 / load ax=2 entry). */
static void camp_list_paint(yt2_ingamemenu *ingamemenu, unsigned int saving)
{
    ingamemenu->saving = saving;
    paint_title(ingamemenu, saving ? 1u : 2u);
    paint_slots(ingamemenu);
}

/* sub_0F49D: the slot browser shared by save and load. Returns 1 when the
   event picks a slot (ingamemenu->slot set, checkbox drawn) or cancels. */
static int camp_browser(yt2_ingamemenu *ingamemenu, int is_click, int x, int y,
                        unsigned int ascii, int *cancelled)
{
    int id;

    *cancelled = 0;
    if (is_click) {
        id = hit_test(x, y);
        if (id < 1 || id > 6) {
            /* Footer cancel: id 7 in save lists, id 8 in load lists. */
            if ((ingamemenu->saving && id == 7) || (!ingamemenu->saving && id == 8)) {
                *cancelled = 1;
                return 1;
            }
            return 0;
        }
    } else {
        if (ascii == 0x1Bu) {
            *cancelled = 1;
            return 1;
        }
        if (ascii < '1' || ascii > '6') {
            /* S cancels save lists, L load lists. */
            if ((ingamemenu->saving && ascii == 'S') ||
                (!ingamemenu->saving && ascii == 'L')) {
                *cancelled = 1;
                return 1;
            }
            return 0;
        }
        id = (int)(ascii - '0');
    }
    ingamemenu->slot = (unsigned int)id;
    paint_check(ingamemenu, ingamemenu_table[id - 1].x0, ingamemenu_table[id - 1].y0, 0x12u);
    return 1;
}

/* fn_0A834 in 0x8000 mode: Y/Enter -> 5, N/ESC -> 7, clicks 5..7 by id. */
static int camp_dialog_event(yt2_ingamemenu *ingamemenu, int is_click, int x, int y,
                             unsigned int ascii, unsigned int *out)
{
    (void)ingamemenu;
    if (is_click) {
        int id = confirm_hit(x, y);

        if (id < 5 || id > 7) {
            return 0;
        }
        *out = (unsigned int)id;
        return 1;
    }
    if (ascii == 'Y' || ascii == 0x0Du) {
        *out = 5u;
        return 1;
    }
    if (ascii == 'N' || ascii == 0x1Bu) {
        *out = 7u;
        return 1;
    }
    return 0;
}

static void camp_show_dialog(yt2_ingamemenu *ingamemenu, unsigned int id)
{
    /* No clear: vanilla paints the dialog text over the current screen. */
    ingamemenu->dialog = id;
    ingamemenu->view = INGAMEMENU_VIEW_CONFIRM;
    paint_dialog(ingamemenu, id);
}

static void camp_repaint_list(yt2_ingamemenu *ingamemenu)
{
    paint_clear(ingamemenu);
    if (ingamemenu->slot >= 1u && ingamemenu->slot <= YT2_INGAMEMENU_SLOTS) {
        paint_check(ingamemenu, ingamemenu_table[ingamemenu->slot - 1u].x0,
                    ingamemenu_table[ingamemenu->slot - 1u].y0, 0x11u);
    }
    paint_slots(ingamemenu);
}

static void slot_path(const yt2_ingamemenu *ingamemenu, unsigned int slot, char *out,
                      size_t size)
{
    snprintf(out, size, "%s/SAVGAME%u", ingamemenu->dir, slot);
}

static void camp_mark_selected(yt2_ingamemenu *ingamemenu);

/* The editor line (sub_0F38A clear + opaque echo with '-' cursor). The
   trailing space erases the stale dash cell like fn_0D29A's redraws. */
static void paint_editor(yt2_ingamemenu *ingamemenu)
{
    char line[YT2_INGAMEMENU_NAME_MAX + 3u];
    int x;
    int y;

    if (camp_resolve(ingamemenu, &x, &y) == NULL) {
        return;
    }
    memcpy(line, ingamemenu->editor, ingamemenu->editor_length);
    line[ingamemenu->editor_length] = '-';
    line[ingamemenu->editor_length + 1u] = ' ';
    line[ingamemenu->editor_length + 2u] = '\0';
    paint_line_opaque(ingamemenu, x, y, line, 0x0Fu);
}

/* sub_0F823 net effect, verified against B1's saves: refresh CURGAME's
   party block from live state, then copy CURGAME over the slot file. The
   name already sits at header bytes [0..N] via strcpy. */
static int camp_write_save(yt2_ingamemenu *ingamemenu)
{
    char path[1088];
    uint8_t *image;

    if (ingamemenu->save->bytes == NULL ||
        ingamemenu->save->size != YT2_SAVE_SIZE) {
        return YT2_STATUS_ERROR;
    }
    image = malloc(ingamemenu->save->size);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    memcpy(image, ingamemenu->save->bytes, ingamemenu->save->size);
    if (yt2_party_write(image + YT2_SAVE_PARTY_OFFSET, YT2_SAVE_PARTY_SIZE,
                        ingamemenu->party) != YT2_STATUS_OK) {
        free(image);
        return YT2_STATUS_ERROR;
    }
    memcpy(ingamemenu->save->bytes, image, ingamemenu->save->size);
    snprintf(path, sizeof(path), "%s/CURGAME", ingamemenu->dir);
    if (yt2_file_save(path, image, ingamemenu->save->size) != YT2_STATUS_OK) {
        free(image);
        return YT2_STATUS_ERROR;
    }
    slot_path(ingamemenu, ingamemenu->slot, path, sizeof(path));
    if (yt2_file_save(path, image, ingamemenu->save->size) != YT2_STATUS_OK) {
        free(image);
        return YT2_STATUS_ERROR;
    }
    free(image);
    /* sub_0F823 marks the written slot selected on its way out. */
    camp_mark_selected(ingamemenu);
    return YT2_STATUS_OK;
}

/* The 0EFAF load flow: slot file over CURGAME plus live state, with the
   sound-state fixups. Short files pad with zeros (vanilla reads blind). */
static int camp_read_save(yt2_ingamemenu *ingamemenu)
{
    char path[1088];
    uint8_t *data = NULL;
    size_t size = 0u;
    uint16_t kept;

    slot_path(ingamemenu, ingamemenu->slot, path, sizeof(path));
    if (yt2_file_load(path, &data, &size) != YT2_STATUS_OK ||
        ingamemenu->save->bytes == NULL ||
        ingamemenu->save->size != YT2_SAVE_SIZE) {
        yt2_file_free(data);
        return YT2_STATUS_ERROR;
    }
    if (size > ingamemenu->save->size) {
        size = ingamemenu->save->size;
    }
    {
        /* The load copies every chunk except objects (no fn_1838A leg):
           the explored-bits stay as the session left them. */
        static uint8_t objects[YT2_SAVE_OBJECTS_SIZE];

        memcpy(objects, ingamemenu->save->bytes + YT2_SAVE_OBJECTS_OFFSET,
               sizeof(objects));
        memset(ingamemenu->save->bytes, 0, ingamemenu->save->size);
        memcpy(ingamemenu->save->bytes, data, size);
        memcpy(ingamemenu->save->bytes + YT2_SAVE_OBJECTS_OFFSET, objects,
               sizeof(objects));
    }
    yt2_file_free(data);
    if (yt2_party_parse(ingamemenu->save->bytes + YT2_SAVE_PARTY_OFFSET,
                        YT2_SAVE_PARTY_SIZE,
                        ingamemenu->party) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    /* Verbatim fixups (presence gates the on-bits, high byte cleared). */
    kept = (uint16_t)(ingamemenu->party->sound_state & 0x05u);
    ingamemenu->party->sound_state &= (uint16_t)~0x05u;
    if ((kept & 0x01u) != 0u) {
        kept |= 0x02u;
        ingamemenu->party->sound_state |= 0x01u;
    }
    if ((kept & 0x04u) != 0u) {
        kept |= 0x08u;
        ingamemenu->party->sound_state |= 0x04u;
    }
    ingamemenu->party->sound_state &= kept;
    if (yt2_party_write(ingamemenu->save->bytes + YT2_SAVE_PARTY_OFFSET,
                        YT2_SAVE_PARTY_SIZE,
                        ingamemenu->party) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    {
        char path[1088];

        snprintf(path, sizeof(path), "%s/CURGAME", ingamemenu->dir);
        if (yt2_file_save(path, ingamemenu->save->bytes, ingamemenu->save->size) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    return YT2_STATUS_OK;
}

/* sub_0F7B1: mark the picked slot selected. */
static void camp_mark_selected(yt2_ingamemenu *ingamemenu)
{
    for (unsigned int i = 0u; i < YT2_INGAMEMENU_SLOTS; i++) {
        ingamemenu->slots[i].flags &= (uint8_t)~YT2_INGAMEMENU_SLOT_SELECTED;
    }
    if (ingamemenu->slot >= 1u && ingamemenu->slot <= YT2_INGAMEMENU_SLOTS) {
        ingamemenu->slots[ingamemenu->slot - 1u].flags |= YT2_INGAMEMENU_SLOT_SELECTED;
    }
}

/* strcpy the editor buffer over the party header (vanilla kills the
   signature the same way) and into the slot record. */
static void camp_commit_name(yt2_ingamemenu *ingamemenu)
{
    yt2_ingamemenu_slot *slot = camp_resolve(ingamemenu, NULL, NULL);
    uint8_t *header = ingamemenu->save->bytes + YT2_SAVE_PARTY_OFFSET;
    size_t length = ingamemenu->editor_length;

    if (slot == NULL || ingamemenu->save->bytes == NULL) {
        return;
    }
    if (length > YT2_INGAMEMENU_NAME_MAX) {
        length = YT2_INGAMEMENU_NAME_MAX;
    }
    memcpy(slot->text, ingamemenu->editor, length);
    slot->text[length] = '\0';
    memcpy(header, ingamemenu->editor, length);
    header[length] = '\0';
}

/* Overwrite prompt: '-' past the current name, then wait (loc_0EE68). */
static void camp_overwrite_prompt(yt2_ingamemenu *ingamemenu)
{
    int x;
    int y;
    yt2_ingamemenu_slot *slot = camp_resolve(ingamemenu, &x, &y);
    size_t length = 0u;

    ingamemenu->view = INGAMEMENU_VIEW_OVERWRITE;
    if (slot == NULL) {
        return;
    }
    while (length < YT2_INGAMEMENU_NAME_TEXT && slot->text[length] != '\0') {
        length++;
    }
    if (length == YT2_INGAMEMENU_NAME_TEXT) {
        length++;
    }
    {
        char dash[2];

        dash[0] = '-';
        dash[1] = '\0';
        paint_line_opaque(ingamemenu, x + (int)(length * 6u), y, dash, 0x0Fu);
    }
}

static void camp_open_editor(yt2_ingamemenu *ingamemenu, unsigned int initial)
{
    int x;
    int y;
    char blanks[YT2_INGAMEMENU_NAME_TEXT + 1u];

    ingamemenu->view = INGAMEMENU_VIEW_EDITOR;
    ingamemenu->editor_length = 0u;
    ingamemenu->editor[0] = '\0';
    if (camp_resolve(ingamemenu, &x, &y) == NULL) {
        return;
    }
    memset(blanks, ' ', YT2_INGAMEMENU_NAME_TEXT);
    blanks[YT2_INGAMEMENU_NAME_TEXT] = '\0';
    paint_line_opaque(ingamemenu, x, y, blanks, 0x0Fu);
    /* fn_0D29A consumes a pending KeyCode as its first keystroke. */
    if (initial >= 0x20u && initial <= 0x7Eu) {
        ingamemenu->editor[ingamemenu->editor_length++] = (char)initial;
        ingamemenu->editor[ingamemenu->editor_length] = '\0';
    }
    paint_editor(ingamemenu);
}

/* Base-screen keys with their v_505E gates. */
static int camp_base_key(yt2_ingamemenu *ingamemenu, unsigned int ascii)
{
    switch (ascii) {
    case 'A':
        if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_SPEED) == 0u) {
            return YT2_STATUS_ERROR;
        }
        camp_speed(ingamemenu);
        return YT2_STATUS_OK;
    case 'D':
        if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_QUIT) == 0u) {
            return YT2_STATUS_ERROR;
        }
        paint_title(ingamemenu, 3u);
        camp_show_dialog(ingamemenu, 1u);
        return YT2_STATUS_OK;
    case 'U':
        camp_sound_toggle(ingamemenu);
        return YT2_STATUS_OK;
    case 'L':
        if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_LOAD) == 0u) {
            return YT2_STATUS_ERROR;
        }
        ingamemenu->slot = 0u;
        ingamemenu->view = INGAMEMENU_VIEW_LOAD_LIST;
        camp_list_paint(ingamemenu, 0u);
        return YT2_STATUS_OK;
    case 'M':
        camp_music(ingamemenu);
        return YT2_STATUS_OK;
    case 'N':
        if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_NEWGAME) == 0u) {
            return YT2_STATUS_ERROR;
        }
        paint_title(ingamemenu, 4u);
        camp_show_dialog(ingamemenu, 31u);
        return YT2_STATUS_OK;
    case 'O':
        /* fn_00000 always returns 1: dead key. */
        return YT2_STATUS_OK;
    case 'R':
    case 0x1Bu:
        if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_CONTINUE) == 0u) {
            return YT2_STATUS_ERROR;
        }
        camp_log_sfx(ingamemenu, 1u);
        ingamemenu->exit = YT2_INGAMEMENU_EXIT_CONTINUE;
        return YT2_STATUS_OK;
    case 'S':
        if ((ingamemenu->enable_mask & YT2_INGAMEMENU_BIT_SAVE) == 0u) {
            return YT2_STATUS_ERROR;
        }
        ingamemenu->slot = 0u;
        ingamemenu->view = INGAMEMENU_VIEW_SAVE_LIST;
        camp_list_paint(ingamemenu, 1u);
        return YT2_STATUS_OK;
    default:
        return YT2_STATUS_ERROR;
    }
}

/* Base-screen clicks (5CCCh ids). */
static int camp_base_click(yt2_ingamemenu *ingamemenu, int x, int y)
{
    switch (hit_test(x, y)) {
    case 14:
        return camp_base_key(ingamemenu, 'A');
    case 10:
        return camp_base_key(ingamemenu, 'D');
    case 12:
        return camp_base_key(ingamemenu, 'U');
    case 8:
        return camp_base_key(ingamemenu, 'L');
    case 11:
        return camp_base_key(ingamemenu, 'M');
    case 9:
        return camp_base_key(ingamemenu, 'N');
    case 15:
        return camp_base_key(ingamemenu, 'R');
    case 7:
        return camp_base_key(ingamemenu, 'S');
    case 13:
        return camp_base_key(ingamemenu, 'O');
    default:
        return YT2_STATUS_ERROR;
    }
}

/* Slot-list keys and clicks (sub_0F49D + the pick sequel). */
static int camp_list_event(yt2_ingamemenu *ingamemenu, int is_click, int x, int y,
                           unsigned int ascii)
{
    int cancelled = 0;

    if (!camp_browser(ingamemenu, is_click, x, y, ascii, &cancelled)) {
        return YT2_STATUS_ERROR;
    }
    if (cancelled) {
        ingamemenu->slot = 0u;
        ingamemenu->view = INGAMEMENU_VIEW_BASE;
        paint_base(ingamemenu);
        return YT2_STATUS_OK;
    }
    if (ingamemenu->saving) {
        yt2_ingamemenu_slot *slot = camp_resolve(ingamemenu, NULL, NULL);

        camp_log_sfx(ingamemenu, 4u);
        if (slot != NULL &&
            (slot->flags & YT2_INGAMEMENU_SLOT_EXISTS) != 0u) {
            camp_overwrite_prompt(ingamemenu);
        } else {
            camp_open_editor(ingamemenu, 0u);
        }
        return YT2_STATUS_OK;
    }
    {
        yt2_ingamemenu_slot *slot = camp_resolve(ingamemenu, NULL, NULL);

        if (slot == NULL ||
            (slot->flags & YT2_INGAMEMENU_SLOT_EXISTS) == 0u) {
            camp_log_sfx(ingamemenu, 3u);
            camp_repaint_list(ingamemenu);
            return YT2_STATUS_OK;
        }
        camp_log_sfx(ingamemenu, 4u);
        camp_show_dialog(ingamemenu, 3u);
        return YT2_STATUS_OK;
    }
}

/* Overwrite-prompt keys: ESC cancels, Enter keeps the name, anything else
   opens the editor with the keystroke as its first char. */
static int camp_overwrite_key(yt2_ingamemenu *ingamemenu, unsigned int ascii)
{
    if (ascii == 0x1Bu) {
        camp_repaint_list(ingamemenu);
        ingamemenu->slot = 0u;
        ingamemenu->view = INGAMEMENU_VIEW_SAVE_LIST;
        paint_title(ingamemenu, 1u);
        paint_slots(ingamemenu);
        return YT2_STATUS_OK;
    }
    if (ascii == 0x0Du) {
        ingamemenu->dialog_named = 0;
        camp_show_dialog(ingamemenu, 2u);
        return YT2_STATUS_OK;
    }
    if (ascii < 0x20u || ascii > 0x7Eu) {
        return YT2_STATUS_ERROR;
    }
    camp_open_editor(ingamemenu, ascii);
    return YT2_STATUS_OK;
}

/* Name editor keys (fn_0D29A, 23 chars). Enter accepts (dash erased with
   a space), ESC cancels, BackSpace deletes. Extended keys never arrive
   (the editor's poll loop swallows them). */
static int camp_editor_key(yt2_ingamemenu *ingamemenu, unsigned int ascii)
{
    if (ascii == 0x1Bu) {
        camp_repaint_list(ingamemenu);
        ingamemenu->slot = 0u;
        ingamemenu->view = INGAMEMENU_VIEW_SAVE_LIST;
        paint_title(ingamemenu, 1u);
        paint_slots(ingamemenu);
        return YT2_STATUS_OK;
    }
    if (ascii == 0x0Du) {
        int x;
        int y;

        ingamemenu->dialog_named = 1;
        /* The editor erases its '-' with a space on accept. */
        if (camp_resolve(ingamemenu, &x, &y) != NULL) {
            paint_line_opaque(ingamemenu,
                              x + (int)(ingamemenu->editor_length * 6u), y, " ",
                              0x0Fu);
        }
        camp_show_dialog(ingamemenu, 2u);
        return YT2_STATUS_OK;
    }
    if (ascii == 0x08u) {
        if (ingamemenu->editor_length == 0u) {
            camp_log_sfx(ingamemenu, 3u);
            return YT2_STATUS_OK;
        }
        ingamemenu->editor_length--;
        ingamemenu->editor[ingamemenu->editor_length] = '\0';
        paint_editor(ingamemenu);
        return YT2_STATUS_OK;
    }
    if (ascii < 0x20u || ascii > 0x7Eu) {
        return YT2_STATUS_ERROR;
    }
    if (ingamemenu->editor_length == YT2_INGAMEMENU_NAME_MAX) {
        camp_log_sfx(ingamemenu, 3u);
        return YT2_STATUS_OK;
    }
    ingamemenu->editor[ingamemenu->editor_length++] = (char)ascii;
    ingamemenu->editor[ingamemenu->editor_length] = '\0';
    paint_editor(ingamemenu);
    return YT2_STATUS_OK;
}

/* Confirm dialog events (Y/Enter yes, N/ESC no, clicks by row id). */
static int camp_confirm_event(yt2_ingamemenu *ingamemenu, int is_click, int x, int y,
                              unsigned int ascii)
{
    unsigned int out = 0u;

    if (!camp_dialog_event(ingamemenu, is_click, x, y, ascii, &out)) {
        return YT2_STATUS_ERROR;
    }
    if (ingamemenu->dialog == 1u) {
        if (out == 5u) {
            ingamemenu->exit = YT2_INGAMEMENU_EXIT_QUIT;
        } else {
            ingamemenu->view = INGAMEMENU_VIEW_BASE;
            paint_base(ingamemenu);
        }
        return YT2_STATUS_OK;
    }
    if (ingamemenu->dialog == 31u) {
        if (out == 5u) {
            ingamemenu->exit = YT2_INGAMEMENU_EXIT_MENU;
        } else {
            ingamemenu->view = INGAMEMENU_VIEW_BASE;
            paint_base(ingamemenu);
        }
        return YT2_STATUS_OK;
    }
    if (out != 5u) {
        camp_repaint_list(ingamemenu);
        ingamemenu->view = ingamemenu->saving ? INGAMEMENU_VIEW_SAVE_LIST : INGAMEMENU_VIEW_LOAD_LIST;
        paint_title(ingamemenu, ingamemenu->saving ? 1u : 2u);
        paint_slots(ingamemenu);
        return YT2_STATUS_OK;
    }
    if (ingamemenu->saving) {
        yt2_ingamemenu_slot *slot = camp_resolve(ingamemenu, NULL, NULL);

        if (ingamemenu->dialog_named) {
            camp_commit_name(ingamemenu);
        } else if (slot != NULL) {
            uint8_t *header = ingamemenu->save->bytes + YT2_SAVE_PARTY_OFFSET;
            size_t length = 0u;

            while (length < YT2_INGAMEMENU_NAME_TEXT &&
                   slot->text[length] != '\0') {
                length++;
            }
            memcpy(header, slot->text, length);
            header[length] = '\0';
        }
        if (slot != NULL) {
            slot->flags |= YT2_INGAMEMENU_SLOT_EXISTS;
        }
        camp_write_save(ingamemenu);
        ingamemenu->exit = YT2_INGAMEMENU_EXIT_CONTINUE;
        return YT2_STATUS_OK;
    }
    if (camp_read_save(ingamemenu) != YT2_STATUS_OK) {
        camp_repaint_list(ingamemenu);
        ingamemenu->view = INGAMEMENU_VIEW_LOAD_LIST;
        paint_title(ingamemenu, 2u);
        paint_slots(ingamemenu);
        return YT2_STATUS_OK;
    }
    camp_mark_selected(ingamemenu);
    ingamemenu->exit = YT2_INGAMEMENU_EXIT_CONTINUE;
    return YT2_STATUS_OK;
}

int yt2_ingamemenu_key(yt2_ingamemenu *ingamemenu, unsigned int ascii)
{
    if (ingamemenu == NULL || ingamemenu->exit != YT2_INGAMEMENU_EXIT_NONE) {
        return YT2_STATUS_ERROR;
    }
    switch (ingamemenu->view) {
    case INGAMEMENU_VIEW_BASE:
        return camp_base_key(ingamemenu, ascii);
    case INGAMEMENU_VIEW_SAVE_LIST:
    case INGAMEMENU_VIEW_LOAD_LIST:
        return camp_list_event(ingamemenu, 0, 0, 0, ascii);
    case INGAMEMENU_VIEW_OVERWRITE:
        return camp_overwrite_key(ingamemenu, ascii);
    case INGAMEMENU_VIEW_EDITOR:
        return camp_editor_key(ingamemenu, ascii);
    case INGAMEMENU_VIEW_CONFIRM:
        return camp_confirm_event(ingamemenu, 0, 0, 0, ascii);
    default:
        return YT2_STATUS_ERROR;
    }
}

int yt2_ingamemenu_click(yt2_ingamemenu *ingamemenu, int x, int y, int right)
{
    if (ingamemenu == NULL || ingamemenu->exit != YT2_INGAMEMENU_EXIT_NONE) {
        return YT2_STATUS_ERROR;
    }
    if (ingamemenu->view == INGAMEMENU_VIEW_BASE) {
        if (right) {
            return camp_base_key(ingamemenu, 'R');
        }
        return camp_base_click(ingamemenu, x, y);
    }
    if (ingamemenu->view == INGAMEMENU_VIEW_SAVE_LIST ||
        ingamemenu->view == INGAMEMENU_VIEW_LOAD_LIST) {
        if (right) {
            return YT2_STATUS_ERROR;
        }
        return camp_list_event(ingamemenu, 1, x, y, 0u);
    }
    if (ingamemenu->view == INGAMEMENU_VIEW_CONFIRM) {
        if (right) {
            return YT2_STATUS_ERROR;
        }
        return camp_confirm_event(ingamemenu, 1, x, y, 0u);
    }
    return YT2_STATUS_ERROR;
}

int yt2_ingamemenu_fkey(yt2_ingamemenu *ingamemenu, unsigned int scancode)
{
    if (ingamemenu == NULL || ingamemenu->exit != YT2_INGAMEMENU_EXIT_NONE) {
        return YT2_STATUS_ERROR;
    }
    if (ingamemenu->view == INGAMEMENU_VIEW_OVERWRITE) {
        camp_open_editor(ingamemenu, scancode);
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_ERROR;
}

int yt2_ingamemenu_done(const yt2_ingamemenu *ingamemenu)
{
    return ingamemenu != NULL && ingamemenu->exit != YT2_INGAMEMENU_EXIT_NONE;
}

yt2_ingamemenu_exit yt2_ingamemenu_result(const yt2_ingamemenu *ingamemenu)
{
    return ingamemenu != NULL ? ingamemenu->exit : YT2_INGAMEMENU_EXIT_NONE;
}

unsigned int yt2_ingamemenu_sfx_count(const yt2_ingamemenu *ingamemenu)
{
    return ingamemenu != NULL ? ingamemenu->sfx_count : 0u;
}

unsigned int yt2_ingamemenu_sfx(const yt2_ingamemenu *ingamemenu, unsigned int index)
{
    if (ingamemenu == NULL || index >= ingamemenu->sfx_count) {
        return 0u;
    }
    return ingamemenu->sfx[index];
}
