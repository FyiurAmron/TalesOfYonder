/* yt2p-play: the B2 boundary demo.  Composes screens with the port engine,
 * presents them over the folder link, and answers input, ticks and the
 * palette rotation on the engine side of protocol v0.
 *
 *   yt2p-play DIR [--scene N] [--cursor X,Y] [--frames N] [--tick-ms N]
 *
 * DIR holds WORLD.DAT and PICTURES.VGA; OUTPUT.BIN is created there and
 * INPUT.BIN is followed there.  Keys 1-6 switch scenes (help, catalog,
 * overhead map, title, menu, monsters); F1-F6, arrows, PgDn/PgUp, Enter
 * and ESC work as in the game (F3 opens the spells, F4 the class list,
 * Enter a line's spells, F5 the categories, Enter a category's items, F6
 * the walkthrough, Enter on the monsters a monster's statistics), the
 * mouse moves the hand cursor, Q quits.  ESC from a list or detail
 * returns to the previous demo scene; the game itself returns to
 * gameplay, which the demo has no scene for yet.
 * Watch with the companion client:
 *
 *   dotnet build/client/bin/Release/net11.0/Yt2Client.dll \
 *       --follow DIR/OUTPUT.BIN --input DIR/INPUT.BIN
 */

#include "yt2/boot.h"
#include "yt2/catalog.h"
#include "yt2/classes.h"
#include "yt2/clock.h"
#include "yt2/cluebook.h"
#include "yt2/cursor.h"
#include "yt2/items.h"
#include "yt2/lists.h"
#include "yt2/mapview.h"
#include "yt2/monster_detail.h"
#include "yt2/monsters.h"
#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/present.h"
#include "yt2/protocol.h"
#include "yt2/item_detail.h"
#include "yt2/spell_detail.h"
#include "yt2/spells.h"
#include "yt2/text.h"
#include "yt2/surface.h"
#include "yt2/transport.h"
#include "yt2/walkthrough.h"
#include "yt2/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAY_WIDTH 320u
#define PLAY_HEIGHT 200u
#define PLAY_TICK_MS 55u
#define PLAY_ROTATION_TICKS 5u
#define PLAY_SCENES 15u

typedef struct play_options {
    unsigned int scene;
    int cursor_x;
    int cursor_y;
    unsigned int frames;
    unsigned int tick_ms;
} play_options;

typedef struct play_state {
    yt2_pictures pictures;
    yt2_world world;
    yt2_monsters monsters;
    yt2_spells spells;
    yt2_classes classes;
    yt2_items items;
    yt2_walkthrough walk;
    yt2_surface surface;
    yt2_palette palette;
    uint8_t anim_base[YT2_WORLD_ANIM_SIZE];
    unsigned int scene;
    unsigned int selected;
    unsigned int monster_selected;
    unsigned int spell_selected;
    unsigned int class_selected;
    unsigned int detail_class;
    unsigned int detail_selected;
    unsigned int spell_detail_index;
    int spell_detail_f4;
    unsigned int item_selected;
    unsigned int item_detail_cat;
    unsigned int item_detail_selected;
    unsigned int item_page_cat;
    unsigned int item_page_rank;
    unsigned int item_page_plus;
    unsigned int walk_selected;
    unsigned int detail_tick;
    /* Overhead-map legend selection (-1: none shown). */
    int legend_line;
    unsigned int legend_map;
    /* Guest ticks since start; the double-click latch below runs on it. */
    unsigned int tick;
    unsigned int last_press_tick;
    int have_last_press;
    /* Stale press the original re-fires on the tick after a double-click. */
    int phantom_pending;
    unsigned int phantom_tick;
    int phantom_x;
    int phantom_y;
    int press_this_tick;
    /* Type-ahead ring: the original drains one key per frame from the BIOS
       ring, so a burst of keys steps visibly instead of coalescing. */
    yt2_input_event key_queue[15];
    unsigned int key_count;
    int cursor_x;
    int cursor_y;
    unsigned int rotation_countdown;
    unsigned int rotation_phase;
    int hello_sent;
} play_state;

/* IBM set-1 scancodes of the clue-book keys. */
#define PLAY_SCAN_ESC 0x01u
#define PLAY_SCAN_ENTER 0x1Cu
#define PLAY_SCAN_F1 0x3Bu
#define PLAY_SCAN_F2 0x3Cu
#define PLAY_SCAN_F3 0x3Du
#define PLAY_SCAN_F4 0x3Eu
#define PLAY_SCAN_F5 0x3Fu
#define PLAY_SCAN_F6 0x40u
#define PLAY_SCAN_PGUP 0x49u
#define PLAY_SCAN_PGDN 0x51u
#define PLAY_SCAN_UP 0x48u
#define PLAY_SCAN_DOWN 0x50u

static int parse_unsigned(const char *text, unsigned int *value)
{
    char *end;
    unsigned long parsed;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    parsed = strtoul(text, &end, 0);
    if (*end != '\0' || parsed > 0xFFFFFFFFul) {
        return 0;
    }
    *value = (unsigned int)parsed;
    return 1;
}

static int parse_options(int argc, char **argv, play_options *options)
{
    options->scene = 1u;
    options->cursor_x = 160;
    options->cursor_y = 100;
    options->frames = 0u;
    options->tick_ms = PLAY_TICK_MS;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], &options->scene) ||
                options->scene >= PLAY_SCENES) {
                fprintf(stderr, "yt2p-play: bad --scene (0..14)\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--cursor") == 0 && i + 1 < argc) {
            char *comma = strchr(argv[++i], ',');

            if (comma == NULL) {
                fprintf(stderr, "yt2p-play: --cursor needs X,Y\n");
                return 0;
            }
            *comma = '\0';
            options->cursor_x = atoi(argv[i]);
            options->cursor_y = atoi(comma + 1);
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], &options->frames) ||
                options->frames == 0u) {
                fprintf(stderr, "yt2p-play: bad --frames\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--tick-ms") == 0 && i + 1 < argc) {
            if (!parse_unsigned(argv[++i], &options->tick_ms) ||
                options->tick_ms == 0u) {
                fprintf(stderr, "yt2p-play: bad --tick-ms\n");
                return 0;
            }
        } else {
            fprintf(stderr, "yt2p-play: unknown argument %s\n", argv[i]);
            return 0;
        }
    }
    return 1;
}

static int render_scene(play_state *state)
{
    const yt2_palette *base;
    int ok = YT2_STATUS_ERROR;

    switch (state->scene) {
    case 0:
        ok = yt2_cluebook_render(&state->surface, &state->pictures);
        break;
    case 1:
        ok = yt2_catalog_render(&state->surface, &state->pictures,
                                &state->world,
                                yt2_catalog_map(state->selected));
        break;
    case 2: {
        unsigned int map = yt2_catalog_map(state->selected);

        ok = yt2_map_view_render(&state->surface, &state->world,
                                 &state->pictures, map);
        if (ok == YT2_STATUS_OK && state->legend_line >= 0 &&
            state->legend_map == map) {
            ok = yt2_map_view_draw_legend(&state->surface, &state->pictures,
                                          &state->world,
                                          (unsigned int)state->legend_line);
        }
        break;
    }
    case 3:
        ok = yt2_boot_render(&state->surface, &state->pictures,
                             YT2_BOOT_TITLE);
        break;
    case 4:
        ok = yt2_boot_render(&state->surface, &state->pictures,
                             YT2_BOOT_MENU);
        break;
    case 5:
        ok = yt2_monsters_render(&state->surface, &state->pictures,
                                 &state->monsters, state->monster_selected);
        break;
    case 6:
        ok = yt2_spells_render(&state->surface, &state->pictures,
                               &state->spells, state->spell_selected);
        break;
    case 7:
        ok = yt2_classes_render_list(&state->surface, &state->pictures,
                                     &state->classes, state->class_selected);
        break;
    case 8:
        ok = yt2_classes_render_detail(&state->surface, &state->pictures,
                                       &state->classes, state->detail_class,
                                       state->detail_selected);
        break;
    case 9:
        ok = yt2_items_render_list(&state->surface, &state->pictures,
                                   state->item_selected);
        break;
    case 10:
        if (state->item_detail_cat == YT2_ITEMS_ATTRIBUTE) {
            ok = yt2_items_render_attribute(&state->surface,
                                            &state->pictures);
        } else if (state->item_detail_cat == YT2_ITEMS_TRANSPORT) {
            ok = yt2_items_render_transport(&state->surface,
                                            &state->pictures);
        } else {
            ok = yt2_items_render_detail(&state->surface, &state->pictures,
                                         &state->items, state->item_detail_cat,
                                         state->item_detail_selected);
        }
        break;
    case 11:
        ok = yt2_walkthrough_render(&state->surface, &state->pictures,
                                    &state->walk, state->walk_selected);
        break;
    case 14: {
        unsigned int file;

        if (yt2_items_member_file(&state->items, state->item_page_cat,
                                  state->item_page_rank,
                                  &file) == YT2_STATUS_OK) {
            ok = yt2_item_page_render(&state->surface, &state->pictures,
                                      &state->items, file,
                                      state->item_page_cat,
                                      state->item_page_plus);
        } else {
            ok = YT2_STATUS_ERROR;
        }
        break;
    }
    case 13:
        ok = yt2_spell_detail_render_index(
            &state->surface, &state->pictures, &state->spells,
            state->spell_detail_index, state->spell_detail_f4);
        break;
    case 12:
        ok = yt2_monster_detail_render(
            &state->surface, &state->pictures, &state->monsters,
            state->monster_selected,
            yt2_monster_detail_phase(&state->monsters,
                                     state->monster_selected,
                                     state->detail_tick));
        break;
    default:
        return YT2_STATUS_ERROR;
    }
    if (ok != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    base = yt2_world_palette(&state->world, 0u);
    if (base == NULL) {
        return YT2_STATUS_ERROR;
    }
    state->palette = *base;
    if (yt2_palette_rotate(&state->palette, state->anim_base,
                           state->rotation_phase) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return yt2_cursor_draw(&state->surface, &state->pictures, YT2_CURSOR_HAND,
                           state->cursor_x, state->cursor_y);
}

static void play_key(play_state *state, const yt2_input_event *event,
                      int *dirty, int *running)
{
    unsigned int list_count;
    unsigned int *selected;
    int in_list;

    if (event->down != 1) {
        return;
    }
    if (state->scene == 5u) {
        selected = &state->monster_selected;
        list_count = YT2_MONSTERS_COUNT;
    } else if (state->scene == 6u) {
        selected = &state->spell_selected;
        list_count = YT2_SPELLS_COUNT;
    } else if (state->scene == 7u) {
        selected = &state->class_selected;
        list_count = YT2_CLASSES_COUNT;
    } else if (state->scene == 8u) {
        selected = &state->detail_selected;
        list_count =
            yt2_class_count(&state->classes, state->detail_class);
    } else if (state->scene == 9u) {
        selected = &state->item_selected;
        list_count = YT2_ITEMS_CATEGORIES;
    } else if (state->scene == 10u) {
        selected = &state->item_detail_selected;
        list_count = yt2_items_category_count(state->item_detail_cat);
    } else {
        selected = &state->selected;
        list_count = yt2_catalog_count();
    }
    in_list = state->scene == 1u || state->scene == 5u ||
              state->scene == 6u || state->scene == 7u ||
              state->scene == 8u || state->scene == 9u ||
              state->scene == 10u;
    if (event->scancode == PLAY_SCAN_DOWN && in_list &&
        *selected + 1u < list_count) {
        (*selected)++;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_UP && in_list && *selected > 0u) {
        (*selected)--;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_PGDN && state->scene == 11u &&
               state->walk_selected + 1u < YT2_WALKTHROUGH_PAGES) {
        state->walk_selected++;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_PGUP && state->scene == 11u &&
               state->walk_selected > 0u) {
        state->walk_selected--;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ENTER && state->scene == 1u) {
        state->legend_line = -1;
        state->scene = 2u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ENTER && state->scene == 6u) {
        state->spell_detail_index =
            state->spells.order[state->spell_selected];
        state->spell_detail_f4 = 0;
        state->scene = 13u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ENTER && state->scene == 8u) {
        state->spell_detail_index = state->classes.members
            [state->detail_class][state->detail_selected];
        state->spell_detail_f4 = 1;
        state->scene = 13u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ENTER && state->scene == 5u) {
        state->detail_tick = 0u;
        state->scene = 12u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ENTER && state->scene == 7u) {
        state->detail_class = state->class_selected;
        state->detail_selected = 0u;
        state->scene = 8u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ENTER && state->scene == 9u) {
        state->item_detail_cat = state->item_selected;
        state->item_detail_selected = 0u;
        state->scene = 10u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ENTER && state->scene == 10u &&
               yt2_items_category_count(state->item_detail_cat) > 0u) {
        state->item_page_cat = state->item_detail_cat;
        state->item_page_rank = state->item_detail_selected;
        state->item_page_plus = 0u;
        state->scene = 14u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_ESC) {
        if (state->scene == 2u) {
            state->scene = 1u;
            *dirty = 1;
        } else if (state->scene == 8u) {
            state->scene = 7u;
            *dirty = 1;
        } else if (state->scene == 10u) {
            state->scene = 9u;
            *dirty = 1;
        } else if (state->scene == 12u) {
            state->scene = 5u;
            *dirty = 1;
        } else if (state->scene == 13u) {
            state->scene = state->spell_detail_f4 ? 8u : 6u;
            *dirty = 1;
        } else if (state->scene == 14u) {
            state->scene = 10u;
            *dirty = 1;
        } else if (state->scene == 1u || state->scene == 5u ||
                   state->scene == 6u || state->scene == 7u ||
                   state->scene == 9u || state->scene == 11u) {
            state->scene = 0u;
            *dirty = 1;
        }
    } else if (event->scancode == PLAY_SCAN_F1) {
        state->scene = 1u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_F2) {
        state->scene = 5u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_F3) {
        state->scene = 6u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_F4) {
        state->scene = 7u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_F5) {
        state->scene = 9u;
        *dirty = 1;
    } else if (event->scancode == PLAY_SCAN_F6) {
        state->scene = 11u;
        *dirty = 1;
    } else if (event->ascii >= '1' && event->ascii <= '6') {
        unsigned int scene = (unsigned int)(event->ascii - '1');

        if (scene == 2u) {
            state->legend_line = -1;
        }
        state->scene = scene;
        *dirty = 1;
    } else if (event->ascii == 'q' || event->ascii == 'Q') {
        *running = 0;
    }
}

/* MORE paging steps (verified on the original: down one window, clamped). */
#define PLAY_MORE_STEP YT2_LIST_VISIBLE_ROWS

/* MORE arrow cells: 'a' at line[0], 'b' at line[9] of the MORE row. */
#define PLAY_MORE_UP_X0 YT2_LIST_MORE_X
#define PLAY_MORE_UP_X1 (YT2_LIST_MORE_X + 6)
#define PLAY_MORE_DOWN_X0 (YT2_LIST_MORE_X + 9 * 6)
#define PLAY_MORE_DOWN_X1 (YT2_LIST_MORE_X + 10 * 6)
#define PLAY_MORE_Y0 YT2_LIST_MORE_Y
#define PLAY_MORE_Y1 (YT2_LIST_MORE_Y + 6)

/* Double-click window in ticks (the original's 6-tick poll delay). */
#define PLAY_DOUBLE_CLICK_TICKS 6u

static unsigned int play_sel(const play_state *state);

static unsigned int play_count(play_state *state)
{
    if (state->scene == 5u) {
        return YT2_MONSTERS_COUNT;
    }
    if (state->scene == 6u) {
        return YT2_SPELLS_COUNT;
    }
    if (state->scene == 7u) {
        return YT2_CLASSES_COUNT;
    }
    if (state->scene == 8u) {
        return yt2_class_count(&state->classes, state->detail_class);
    }
    if (state->scene == 9u) {
        return YT2_ITEMS_CATEGORIES;
    }
    if (state->scene == 10u) {
        return yt2_items_category_count(state->item_detail_cat);
    }
    return yt2_catalog_count();
}

static void play_set_sel(play_state *state, unsigned int value)
{
    if (state->scene == 5u) {
        state->monster_selected = value;
    } else if (state->scene == 6u) {
        state->spell_selected = value;
    } else if (state->scene == 7u) {
        state->class_selected = value;
    } else if (state->scene == 8u) {
        state->detail_selected = value;
    } else if (state->scene == 9u) {
        state->item_selected = value;
    } else if (state->scene == 10u) {
        state->item_detail_selected = value;
    } else {
        state->selected = value;
    }
}

/* -1: MORE-up zone, +1: MORE-down zone, 0: elsewhere.  Mirrors draw_more. */
static int play_more_at(play_state *state, int x, int y)
{
    unsigned int count;
    unsigned int start;

    if (state->scene == 11u) {
        if (y < PLAY_MORE_Y0 || y >= PLAY_MORE_Y1) {
            return 0;
        }
        if (x >= PLAY_MORE_UP_X0 && x < PLAY_MORE_UP_X1 &&
            state->walk_selected > 0u) {
            return -1;
        }
        if (x >= PLAY_MORE_DOWN_X0 && x < PLAY_MORE_DOWN_X1 &&
            state->walk_selected + 1u < YT2_WALKTHROUGH_PAGES) {
            return 1;
        }
        return 0;
    }
    if (state->scene != 1u && state->scene != 5u && state->scene != 6u &&
        state->scene != 8u && state->scene != 10u) {
        return 0;
    }
    count = play_count(state);
    if (count <= YT2_LIST_VISIBLE_ROWS) {
        return 0;
    }
    if (y < PLAY_MORE_Y0 || y >= PLAY_MORE_Y1) {
        return 0;
    }
    start = play_sel(state) / YT2_LIST_VISIBLE_ROWS * YT2_LIST_VISIBLE_ROWS;
    if (x >= PLAY_MORE_UP_X0 && x < PLAY_MORE_UP_X1 && start > 0u) {
        return -1;
    }
    if (x >= PLAY_MORE_DOWN_X0 && x < PLAY_MORE_DOWN_X1 &&
        start + YT2_LIST_VISIBLE_ROWS < count) {
        return 1;
    }
    return 0;
}

/* Open whatever Enter would open on the row under (x, y), if anything. */
static void play_open_at(play_state *state, int x, int y, int *dirty)
{
    unsigned int current;
    unsigned int count;
    unsigned int start;
    unsigned int row;

    if (state->scene != 1u && state->scene != 5u && state->scene != 6u &&
        state->scene != 7u && state->scene != 8u && state->scene != 9u &&
        state->scene != 10u) {
        return;
    }
    if (x < YT2_LIST_X || x > 279 || y < YT2_LIST_Y ||
        (y - YT2_LIST_Y) % YT2_LIST_STEP > 5) {
        return;
    }
    current = play_sel(state);
    count = play_count(state);
    start = current / YT2_LIST_VISIBLE_ROWS * YT2_LIST_VISIBLE_ROWS;
    row = (unsigned int)(y - YT2_LIST_Y) / (unsigned int)YT2_LIST_STEP;
    if (row >= YT2_LIST_VISIBLE_ROWS || start + row >= count) {
        return;
    }
    play_set_sel(state, start + row);
    if (state->scene == 1u) {
        state->legend_line = -1;
        state->scene = 2u;
        *dirty = 1;
    } else if (state->scene == 5u) {
        state->detail_tick = 0u;
        state->scene = 12u;
        *dirty = 1;
    } else if (state->scene == 6u) {
        state->spell_detail_index =
            state->spells.order[state->spell_selected];
        state->spell_detail_f4 = 0;
        state->scene = 13u;
        *dirty = 1;
    } else if (state->scene == 8u) {
        state->spell_detail_index = state->classes.members
            [state->detail_class][state->detail_selected];
        state->spell_detail_f4 = 1;
        state->scene = 13u;
        *dirty = 1;
    } else if (state->scene == 7u) {
        state->detail_class = state->class_selected;
        state->detail_selected = 0u;
        state->scene = 8u;
        *dirty = 1;
    } else if (state->scene == 9u) {
        state->item_detail_cat = state->item_selected;
        state->item_detail_selected = 0u;
        state->scene = 10u;
        *dirty = 1;
    } else if (state->scene == 10u &&
               yt2_items_category_count(state->item_detail_cat) > 0u) {
        state->item_page_cat = state->item_detail_cat;
        state->item_page_rank = state->item_detail_selected;
        state->item_page_plus = 0u;
        state->scene = 14u;
        *dirty = 1;
    }
}

static unsigned int play_sel(const play_state *state)
{
    if (state->scene == 5u) {
        return state->monster_selected;
    }
    if (state->scene == 6u) {
        return state->spell_selected;
    }
    if (state->scene == 7u) {
        return state->class_selected;
    }
    if (state->scene == 8u) {
        return state->detail_selected;
    }
    if (state->scene == 9u) {
        return state->item_selected;
    }
    if (state->scene == 10u) {
        return state->item_detail_selected;
    }
    if (state->scene == 11u) {
        return state->walk_selected;
    }
    if (state->scene == 12u) {
        return state->monster_selected;
    }
    if (state->scene == 13u) {
        return state->spell_detail_index;
    }
    if (state->scene == 14u) {
        unsigned int file;

        if (yt2_items_member_file(&state->items, state->item_page_cat,
                                  state->item_page_rank,
                                  &file) == YT2_STATUS_OK) {
            return file;
        }
        return state->item_page_rank;
    }
    return state->selected;
}

static void clamp_cursor(play_state *state)
{
    if (state->cursor_x < 0) {
        state->cursor_x = 0;
    }
    if (state->cursor_y < 0) {
        state->cursor_y = 0;
    }
    if (state->cursor_x > (int)(PLAY_WIDTH - 1u)) {
        state->cursor_x = (int)(PLAY_WIDTH - 1u);
    }
    if (state->cursor_y > (int)(PLAY_HEIGHT - 1u)) {
        state->cursor_y = (int)(PLAY_HEIGHT - 1u);
    }
}

/* Single left press at (x, y): icons, footers, rows, MORE, legend. */
static void play_press_at(play_state *state, int x, int y, int *dirty,
                          int *running)
{
    /* Icon bar (the list pages share it): map opens the catalog, the
       person the monsters, the scroll the spells, the book the classes,
       the chest the items, the quill the walkthrough, EXIT quits. */
    if ((state->scene == 0u || state->scene == 1u || state->scene == 5u ||
         state->scene == 6u || state->scene == 7u || state->scene == 8u ||
         state->scene == 9u || state->scene == 10u || state->scene == 11u ||
         state->scene == 12u || state->scene == 13u ||
         state->scene == 14u) &&
        y >= YT2_LIST_ICON_Y &&
        y < YT2_LIST_ICON_Y + (int)YT2_LIST_ICON_SIZE) {
        int slot = (x - YT2_LIST_ICON_X) / YT2_LIST_ICON_STEP;

        if (x >= YT2_LIST_ICON_X && slot >= 0 &&
            slot < (int)YT2_LIST_ICON_COUNT &&
            (x - YT2_LIST_ICON_X) % YT2_LIST_ICON_STEP <
                (int)YT2_LIST_ICON_SIZE) {
            if (slot == 0) {
                state->scene = 1u;
                *dirty = 1;
            } else if (slot == 1) {
                state->scene = 5u;
                *dirty = 1;
            } else if (slot == 2) {
                state->scene = 6u;
                *dirty = 1;
            } else if (slot == 3) {
                state->scene = 7u;
                *dirty = 1;
            } else if (slot == 4) {
                state->scene = 9u;
                *dirty = 1;
            } else if (slot == 5) {
                state->scene = 11u;
                *dirty = 1;
            } else if (slot == (int)YT2_LIST_ICON_COUNT - 1) {
                *running = 0;
            }
        }
    }
    /* The detail footers return to their lists. */
    if (state->scene == 14u) {
        unsigned int file;
        unsigned int count;
        unsigned int box;

        /* +N boxes (sub_04573 hit zones). */
        if (yt2_items_member_file(&state->items, state->item_page_cat,
                                  state->item_page_rank,
                                  &file) == YT2_STATUS_OK) {
            count = yt2_item_plus_count(&state->items, file,
                                        state->item_page_cat);
            for (box = 0u; box < count; box++) {
                int x0 = 21 + (int)box * 26;

                if (x >= x0 && x <= x0 + 16 && y >= 141 && y <= 150 &&
                    state->item_page_plus != box) {
                    state->item_page_plus = box;
                    *dirty = 1;
                    return;
                }
            }
        }
    }
    if ((state->scene == 8u || state->scene == 10u || state->scene == 12u ||
         state->scene == 13u || state->scene == 14u) &&
        x >= YT2_CLASSES_DETAIL_FOOTER_X &&
        x < YT2_CLASSES_DETAIL_FOOTER_X +
                (int)(6u * YT2_TEXT_GLYPH_WIDTH) &&
        y >= YT2_CLASSES_DETAIL_FOOTER_Y &&
        y < YT2_CLASSES_DETAIL_FOOTER_Y + (int)YT2_TEXT_GLYPH_HEIGHT) {
        if (state->scene == 8u) {
            state->scene = 7u;
        } else if (state->scene == 10u) {
            state->scene = 9u;
        } else if (state->scene == 13u) {
            state->scene = state->spell_detail_f4 ? 8u : 6u;
        } else if (state->scene == 14u) {
            state->scene = 10u;
        } else {
            state->scene = 5u;
        }
        *dirty = 1;
    }
    /* List rows select by list position (x 40..279, 6-pixel bands). */
    if ((state->scene == 1u || state->scene == 5u || state->scene == 6u ||
         state->scene == 7u || state->scene == 8u || state->scene == 9u ||
         state->scene == 10u) &&
        x >= YT2_LIST_X && x <= 279 && y >= YT2_LIST_Y &&
        (y - YT2_LIST_Y) % YT2_LIST_STEP <= 5) {
        unsigned int current = play_sel(state);
        unsigned int count = play_count(state);
        unsigned int start = current / YT2_LIST_VISIBLE_ROWS *
                             YT2_LIST_VISIBLE_ROWS;
        unsigned int row = (unsigned int)(y - YT2_LIST_Y) /
                           (unsigned int)YT2_LIST_STEP;

        if (row < YT2_LIST_VISIBLE_ROWS && start + row < count) {
            play_set_sel(state, start + row);
            *dirty = 1;
        }
    }
    /* MORE arrows page the window by fourteen rows (one page on the
       walkthrough). */
    {
        int more = play_more_at(state, x, y);

        if (more != 0 && state->scene == 11u) {
            if (more < 0) {
                state->walk_selected--;
            } else {
                state->walk_selected++;
            }
            *dirty = 1;
        } else if (more < 0) {
            unsigned int sel = play_sel(state);

            play_set_sel(state,
                         sel >= PLAY_MORE_STEP ? sel - PLAY_MORE_STEP : 0u);
            *dirty = 1;
        } else if (more > 0) {
            unsigned int sel = play_sel(state);
            unsigned int count = play_count(state);

            play_set_sel(state,
                         sel + PLAY_MORE_STEP < count ? sel + PLAY_MORE_STEP
                                                     : count - 1u);
            *dirty = 1;
        }
    }
    /* Overhead-map legend markers show their text line. */
    if (state->scene == 2u) {
        unsigned int line;

        if (yt2_map_view_legend_at(&state->world,
                                   yt2_catalog_map(play_sel(state)), x, y,
                                   &line) == YT2_STATUS_OK) {
            state->legend_line = (int)line;
            state->legend_map = yt2_catalog_map(play_sel(state));
            *dirty = 1;
        }
    }
}

int main(int argc, char **argv)
{
    play_options options;
    play_state state;
    yt2_folder_out out;
    yt2_folder_in in;
    yt2_presenter presenter;
    char path[1024];
    unsigned int presented = 0u;
    int running = 1;
    int result = 1;

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    if (argc < 2 || !parse_options(argc, argv, &options)) {
        fprintf(stderr,
                "usage: %s DIR [--scene N] [--cursor X,Y] [--frames N] "
                "[--tick-ms N]\n",
                argv[0]);
        return 2;
    }
    memset(&state, 0, sizeof(state));
    state.legend_line = -1;
    state.scene = options.scene;
    state.cursor_x = options.cursor_x;
    state.cursor_y = options.cursor_y;
    state.rotation_countdown = PLAY_ROTATION_TICKS;
    state.rotation_phase = 0u;
    clamp_cursor(&state);

    snprintf(path, sizeof(path), "%s/PICTURES.VGA", argv[1]);
    if (yt2_pictures_load(path, &state.pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-play: cannot load %s\n", path);
        return 1;
    }
    snprintf(path, sizeof(path), "%s/WORLD.DAT", argv[1]);
    if (yt2_world_load(path, &state.world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-play: cannot load %s\n", path);
        yt2_pictures_free(&state.pictures);
        return 1;
    }
    if (yt2_surface_init(&state.surface, PLAY_WIDTH, PLAY_HEIGHT) !=
            YT2_STATUS_OK ||
        yt2_world_anim_base(&state.world, state.anim_base) != YT2_STATUS_OK ||
        yt2_monsters_load(&state.monsters, &state.world) != YT2_STATUS_OK ||
        yt2_spells_load(&state.spells, &state.world) != YT2_STATUS_OK ||
        yt2_classes_load(&state.classes, &state.spells) != YT2_STATUS_OK ||
        yt2_items_load(&state.items, &state.world) != YT2_STATUS_OK ||
        yt2_walkthrough_load(&state.walk, &state.world) != YT2_STATUS_OK ||
        yt2_folder_out_open(&out, argv[1]) != YT2_STATUS_OK ||
        yt2_folder_in_open(&in, argv[1]) != YT2_STATUS_OK ||
        yt2_presenter_init(&presenter) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-play: cannot start the session\n");
        goto done;
    }
    if (render_scene(&state) != YT2_STATUS_OK ||
        yt2_present_hello(&presenter, &out) != YT2_STATUS_OK ||
        yt2_present_frame(&presenter, &out, &state.surface, &state.palette,
                          1) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-play: cannot present the first frame\n");
        goto done;
    }
    presented = 1u;
    printf("play: present 1 scene %u sel %u", state.scene,
           play_sel(&state));
    if (state.scene == 2u) {
        printf(" leg %d", state.legend_line);
    }
    printf("\n");

    while (running && (options.frames == 0u || presented < options.frames)) {
        const uint8_t *frame;
        uint32_t length;
        int dirty = 0;
        int force = 0;
        int keys = 0;
        int poll;

        yt2_sleep_ms(options.tick_ms);
        state.tick++;
        state.press_this_tick = 0;
        if (yt2_folder_in_take_wrapped(&in)) {
            state.hello_sent = 0;
        }
        if (state.key_count > 0u) {
            play_key(&state, &state.key_queue[0], &dirty, &running);
            state.key_count--;
            memmove(&state.key_queue[0], &state.key_queue[1],
                    state.key_count * sizeof(state.key_queue[0]));
            keys = 1;
        }
        for (;;) {
            yt2_input_event event;

            poll = yt2_folder_in_poll(&in, &frame, &length);
            if (poll <= 0) {
                break;
            }
            if (yt2_input_decode_frame(frame, length, &event) != 1) {
                continue;
            }
            if (event.kind == YT2_INPUT_EVENT_KEY) {
                /* One key per tick, like the original's per-frame BIOS
                   ring drain; extras queue in arrival order.  Mouse and
                   control always flow, so a click's abs+button pair lands
                   on the position it names. */
                if (keys > 0) {
                    if (state.key_count <
                        sizeof(state.key_queue) /
                            sizeof(state.key_queue[0])) {
                        state.key_queue[state.key_count++] = event;
                    }
                    continue;
                }
                keys = 1;
            }
            switch (event.kind) {
            case YT2_INPUT_EVENT_HELLO:
                if (event.role == YT2_ROLE_CLIENT && event.version == 0 &&
                    !state.hello_sent) {
                    if (yt2_present_hello(&presenter, &out) !=
                        YT2_STATUS_OK) {
                        running = 0;
                    }
                    state.hello_sent = 1;
                }
                break;
            case YT2_INPUT_EVENT_KEYFRAME_REQUEST:
                dirty = 1;
                force = 1;
                break;
            case YT2_INPUT_EVENT_KEY:
                play_key(&state, &event, &dirty, &running);
                break;
            case YT2_INPUT_EVENT_MOUSE_MOVE:
                state.cursor_x += event.dx;
                state.cursor_y += event.dy;
                clamp_cursor(&state);
                dirty = 1;
                break;
            case YT2_INPUT_EVENT_MOUSE_ABS:
                state.cursor_x = (int)event.x;
                state.cursor_y = (int)event.y;
                clamp_cursor(&state);
                dirty = 1;
                break;
            case YT2_INPUT_EVENT_MOUSE_BUTTON:
                if (event.button == 0u && event.down == 1u) {
                    /* Single press selects; a second press inside the
                       window opens (the original's kind-6 event).  A press
                       landing on the phantom tick consumes it as a single,
                       the way the game merges it into the stale latch. */
                    if (state.phantom_pending) {
                        state.phantom_pending = 0;
                        play_press_at(&state, state.cursor_x, state.cursor_y,
                                      &dirty, &running);
                    } else if (state.have_last_press &&
                               state.tick - state.last_press_tick <
                                   PLAY_DOUBLE_CLICK_TICKS) {
                        play_open_at(&state, state.cursor_x, state.cursor_y,
                                     &dirty);
                        state.phantom_pending = 1;
                        state.phantom_tick = state.tick + 1u;
                        state.phantom_x = state.cursor_x;
                        state.phantom_y = state.cursor_y;
                        state.have_last_press = 0;
                    } else {
                        play_press_at(&state, state.cursor_x, state.cursor_y,
                                      &dirty, &running);
                    }
                    state.have_last_press = 1;
                    state.last_press_tick = state.tick;
                    state.press_this_tick = 1;
                }
                break;
            case YT2_INPUT_EVENT_QUIT:
                running = 0;
                break;
            default:
                break;
            }
        }
        if (poll < 0) {
            fprintf(stderr, "yt2p-play: input failure\n");
            break;
        }
        if (state.phantom_pending && !state.press_this_tick &&
            state.tick >= state.phantom_tick) {
            state.phantom_pending = 0;
            play_press_at(&state, state.phantom_x, state.phantom_y, &dirty,
                          &running);
            state.have_last_press = 1;
            state.last_press_tick = state.tick;
        }
        state.detail_tick++;
        if (state.rotation_countdown > 0u) {
            state.rotation_countdown--;
        }
        if (state.rotation_countdown == 0u) {
            state.rotation_countdown = PLAY_ROTATION_TICKS;
            state.rotation_phase =
                (state.rotation_phase + 1u) % YT2_PALETTE_ANIM_PHASES;
            dirty = 1;
        }
        if (dirty && running) {
            if (yt2_folder_out_take_wrapped(&out)) {
                force = 1;
            }
            if (render_scene(&state) != YT2_STATUS_OK ||
                yt2_present_frame(&presenter, &out, &state.surface,
                                  &state.palette, force) != YT2_STATUS_OK) {
                fprintf(stderr, "yt2p-play: cannot present\n");
                break;
            }
            presented++;
            printf("play: present %u scene %u sel %u", presented, state.scene,
                   play_sel(&state));
            if (state.scene == 2u) {
                printf(" leg %d", state.legend_line);
            }
            printf("\n");
        }
    }
    printf("play: %u frames\n", presented);
    result = 0;

done:
    yt2_presenter_done(&presenter);
    yt2_folder_in_close(&in);
    yt2_folder_out_close(&out);
    yt2_surface_free(&state.surface);
    yt2_world_free(&state.world);
    yt2_pictures_free(&state.pictures);
    return result;
}
