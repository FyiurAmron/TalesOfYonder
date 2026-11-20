/* yt2p-play: the B2 boundary demo.  Boots the scripted start-up sequence
 * (Webfoot logo, story cutscene, attract pictures, menu) with the port
 * engine, then runs the menu loop: character creation and the roster are
 * live slices (persisted to CURGAME); embark prints a marker
 * until its slice lands; presents everything over the folder link, and
 * answers input, ticks and the palette rotation on the engine side of
 * protocol v0.
 *
 *   yt2p-play DIR [--scene N] [--cursor X,Y] [--frames N] [--tick-ms N]
 *       [--spelltext] [--no-intro]
 *
 * --spelltext reads the F3 detail EFFECT lines live from WORLD.DAT (files
 * 6+7) instead of the frozen vanilla bytes, so a spelltext-patched world
 * (tools/yt2_spelltext.py) shows its descriptions.  --no-intro skips the
 * start-up sequence and opens the scene demo directly.
 *
 * DIR holds WORLD.DAT and PICTURES.VGA; OUTPUT.BIN is created there and
 * INPUT.BIN is followed there.  During the intro, ESC skips the cutscene
 * and attract to the menu; in the menu the letter keys (C/A/E/R/I) and
 * clicks select, Enter toggles the music, Q quits.  C opens character
 * creation and A the roster; E/R are unported and return to the menu.
 * --no-intro skips the menu loop and opens the scene demo instead.
 * There, keys 1-6
 * switch scenes (help, catalog, overhead map, title, menu, monsters);
 * F1-F6, arrows, PgDn/PgUp, Enter and ESC work as in the game (F3 opens
 * the spells, F4 the class list, Enter a line's spells, F5 the categories,
 * Enter a category's items, F6 the walkthrough, Enter on the monsters a
 * monster's statistics), the mouse moves the hand cursor, Q quits.  ESC
 * from a list or detail returns to the previous demo scene; the game
 * itself returns to gameplay, which the demo has no scene for yet.
 * Watch with the companion client:
 *
 *   dotnet build/client/bin/Release/net11.0/Yt2Client.dll \
 *       --follow DIR/OUTPUT.BIN --input DIR/INPUT.BIN
 */

#include "yt2/boot.h"
#include "yt2/ingamemenu.h"
#include "yt2/catalog.h"
#include "yt2/classes.h"
#include "yt2/clock.h"
#include "yt2/combat.h"
#include "yt2/cluebook.h"
#include "yt2/create.h"
#include "yt2/cursor.h"
#include "yt2/file.h"
#include "yt2/savefile.h"
#include "yt2/intro.h"
#include "yt2/items.h"
#include "yt2/lists.h"
#include "yt2/mapview.h"
#include "yt2/titlemenu.h"
#include "yt2/monster_detail.h"
#include "yt2/monsters.h"
#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/present.h"
#include "yt2/protocol.h"
#include "yt2/item_detail.h"
#include "yt2/room.h"
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
    int spelltext;
    int no_intro;
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
    yt2_surface vbe;
    int have_vbe;
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
    yt2_surface frame; /* session present copy (cursor composites here) */
    int have_frame;
    int spelltext;
    unsigned int rotation_countdown;
    unsigned int rotation_phase;
    int hello_sent;
    /* Creation/roster/explore sessions and the in-game menu (the demo never touches these). */
    int session_mode; /* 0: demo, 1: create, 2: roster, 3: explore, 4: in-game menu */
    yt2_create create;
    yt2_roster roster;
    yt2_room room;
    yt2_combat combat;
    int have_combat;
    yt2_ingamemenu ingamemenu;
    yt2_ingamemenu_slot ingamemenu_slots[YT2_INGAMEMENU_SLOTS];
    int ingamemenu_scanned;
    int ingametitlemenu_requested; /* explore D: run the in-game menu nested */
    int ingamemenu_formal; /* formal D: mask 0x5E + combat wide clear */
    int titlemenu_requested; /* N: unwind to the title menu loop */
    int map_open; /* M/L/U: modal overhead map (fn_0CE82/fn_123E9) */
    unsigned int map_id; /* M=31, L=47, U=7 filler (B1 fixed) */
    yt2_save save;
    int have_save;
    char save_path[1024];
    char game_dir[1024];
    /* No party copy: assigned[] lives in save.party (header 0x1EC)
       and party_ok is derived from its sum at embark (fn_0D36C). */
    /* Live session I/O for the explore present callback. */
    yt2_folder_out *session_out;
    yt2_presenter *session_presenter;
    unsigned int *session_presented;
    unsigned int create_sfx_seen;
    unsigned int roster_sfx_seen;
    unsigned int ingamemenu_sfx_seen;
    unsigned int combat_sfx_seen;
} play_state;

/* IBM set-1 scancodes of the clue-book keys. */
#define PLAY_SCAN_ESC 0x01u
#define PLAY_SCAN_ENTER 0x1Cu
#define PLAY_SCAN_BS 0x0Eu
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
#define PLAY_SCAN_LEFT 0x4Bu
#define PLAY_SCAN_RIGHT 0x4Du

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
    options->spelltext = 0;
    options->no_intro = 0;
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
                options->tick_ms == 0u || options->tick_ms > 0xFFFFu) {
                fprintf(stderr, "yt2p-play: bad --tick-ms\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--spelltext") == 0) {
            options->spelltext = 1;
        } else if (strcmp(argv[i], "--no-intro") == 0) {
            options->no_intro = 1;
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
        if (state->spelltext) {
            ok = yt2_spell_detail_render_index_world(
                &state->surface, &state->pictures, &state->spells,
                state->spell_detail_index, state->spell_detail_f4);
        } else {
            ok = yt2_spell_detail_render_index(
                &state->surface, &state->pictures, &state->spells,
                state->spell_detail_index, state->spell_detail_f4);
        }
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

/* IBM set-1 scancode of Q (the menu quit key; the wire carries no Ctrl). */
#define PLAY_SCAN_Q 0x10u

/* Game key codes queued for the intro (the original's BIOS typeahead ring
   holds 15). */
#define PLAY_INTRO_KEYS 15u

typedef struct play_intro_bridge {
    play_state *state;
    yt2_folder_out *out;
    yt2_folder_in *in;
    yt2_presenter *presenter;
    yt2_intro *intro;
    unsigned int tick_ms;
    unsigned int *presented;
    unsigned int frames_cap;
    /* Full wire events: the game-code translation happens at consumption,
       and whatever the intro leaves queued carries into the demo loop. */
    yt2_input_event keys[PLAY_INTRO_KEYS];
    unsigned int key_count;
    int click_pending;
    int click_x;
    int click_y;
    int force;
    int hello_sent;
    int quit;
    int failed;
    int have_paced;
    unsigned long paced_tick;
    unsigned int audio_sent;
} play_intro_bridge;

/*
 * Pace every script tick in wall time, whoever observes it first: holds
 * and menu spins burn ticks without emitting, so emit-side pacing alone
 * lets polls race ahead of wall-clock input.  Each tick sleeps exactly
 * once; quiet stretches render as holds of the current frame.
 */
static void play_intro_pace(play_intro_bridge *bridge, unsigned long tick)
{
    if (!bridge->have_paced) {
        bridge->paced_tick = tick;
        bridge->have_paced = 1;
    } else if (tick > bridge->paced_tick) {
        yt2_sleep_ms((unsigned int)((tick - bridge->paced_tick) *
                                      bridge->tick_ms));
        bridge->paced_tick = tick;
    }
}

/*
 * Forward-sync the intro clock after a session: the stream tick jumps
 * ahead for monotonicity, but wall pacing rebases to the jump instead
 * of sleeping through the session's ticks.
 */
static void play_intro_resync(play_intro_bridge *bridge, yt2_intro *intro,
                              unsigned long tick)
{
    intro->tick = tick;
    bridge->paced_tick = tick;
    bridge->have_paced = 1;
}

static const char *play_intro_audio_name(yt2_intro_audio_op op)
{
    switch (op) {
    case YT2_INTRO_AUDIO_MUSIC:
        return "music";
    case YT2_INTRO_AUDIO_SFX:
        return "sfx";
    case YT2_INTRO_AUDIO_MUSIC_STOP:
        return "music-stop";
    case YT2_INTRO_AUDIO_MUSIC_SERVICE:
        return "music-service";
    default:
        return "unknown";
    }
}

static const char *play_intro_transition_name(yt2_intro_transition transition)
{
    switch (transition) {
    case YT2_INTRO_TRANSITION_QUIT:
        return "quit";
    case YT2_INTRO_TRANSITION_CREATE:
        return "create";
    case YT2_INTRO_TRANSITION_LOAD:
        return "load";
    case YT2_INTRO_TRANSITION_EMBARK:
        return "embark";
    case YT2_INTRO_TRANSITION_RULES:
        return "rules";
    case YT2_INTRO_TRANSITION_NONE:
    default:
        return "none";
    }
}

static void play_intro_poll(yt2_intro_input *input, void *user)
{
    play_intro_bridge *bridge = user;
    play_state *state = bridge->state;

    play_intro_pace(bridge, bridge->intro->tick);
    for (;;) {
        const uint8_t *frame;
        uint32_t length;
        yt2_input_event event;
        int poll = yt2_folder_in_poll(bridge->in, &frame, &length);

        if (poll <= 0) {
            if (poll < 0) {
                bridge->failed = 1;
            }
            break;
        }
        if (yt2_input_decode_frame(frame, length, &event) != 1) {
            continue;
        }
        switch (event.kind) {
        case YT2_INPUT_EVENT_HELLO:
            if (event.role == YT2_ROLE_CLIENT && event.version == 0 &&
                !bridge->hello_sent) {
                if (yt2_present_hello(bridge->presenter, bridge->out) !=
                        YT2_STATUS_OK ||
                    yt2_present_pace(bridge->presenter, bridge->out,
                                     bridge->tick_ms) != YT2_STATUS_OK) {
                    bridge->failed = 1;
                }
                bridge->hello_sent = 1;
            }
            break;
        case YT2_INPUT_EVENT_KEYFRAME_REQUEST:
            bridge->force = 1;
            bridge->intro->reshow = 1;
            break;
        case YT2_INPUT_EVENT_KEY:
            if (event.down == 0) {
                break;
            }
            if (bridge->key_count < PLAY_INTRO_KEYS) {
                bridge->keys[bridge->key_count++] = event;
            }
            break;
        case YT2_INPUT_EVENT_MOUSE_MOVE:
            state->cursor_x += event.dx;
            state->cursor_y += event.dy;
            clamp_cursor(state);
            break;
        case YT2_INPUT_EVENT_MOUSE_ABS:
            state->cursor_x = (int)event.x;
            state->cursor_y = (int)event.y;
            clamp_cursor(state);
            break;
        case YT2_INPUT_EVENT_MOUSE_BUTTON:
            if (event.button == 0u && event.down == 1u) {
                bridge->click_pending = 1;
                bridge->click_x = state->cursor_x;
                bridge->click_y = state->cursor_y;
            }
            break;
        case YT2_INPUT_EVENT_QUIT:
            bridge->quit = 1;
            bridge->intro->stop = 1;
            break;
        default:
            break;
        }
    }
    /* A checkpoint poll only asks after ESC: report the first one queued
       and leave everything else (other keys, clicks) for the menu. */
    if (bridge->intro->peek_only) {
        unsigned int found = bridge->key_count;
        unsigned int qi;

        for (qi = 0; qi < bridge->key_count; qi++) {
            if (bridge->keys[qi].scancode == PLAY_SCAN_ESC) {
                found = qi;
                break;
            }
        }
        if (found < bridge->key_count) {
            input->have_key = 1;
            input->key = 0x1Bu;
            bridge->key_count--;
            memmove(&bridge->keys[found], &bridge->keys[found + 1],
                    (bridge->key_count - found) *
                        sizeof(bridge->keys[0]));
        }
        return;
    }
    /* One queued key per poll, like the original's per-frame ring drain.
       Untranslatable keys (arrows, function keys) are consumed but not
       reported: the intro has no binding for them. */
    if (bridge->key_count > 0u) {
        yt2_input_event *queued = &bridge->keys[0];
        uint8_t code = queued->ascii;

        /* The wire carries no modifiers, so plain Q is the DOS ^Q the
           menu quits on; ESC arrives with its own code. */
        if (queued->scancode == PLAY_SCAN_ESC) {
            code = 0x1Bu;
        } else if (queued->scancode == PLAY_SCAN_Q) {
            code = 0x11u;
        }
        if (getenv("YT2_PLAY_KEYTRACE") != NULL) {
            fprintf(stderr, "keytrace scan=%02X ascii=%02X code=%02X\n",
                    queued->scancode, queued->ascii, code);
        }
        if (code != 0) {
            input->have_key = 1;
            input->key = code;
        }
        bridge->key_count--;
        memmove(&bridge->keys[0], &bridge->keys[1],
                bridge->key_count * sizeof(bridge->keys[0]));
    }
    if (bridge->click_pending) {
        bridge->click_pending = 0;
        input->left_pressed = 1;
        input->mouse_x = bridge->click_x;
        input->mouse_y = bridge->click_y;
    }
}

static int play_intro_emit(const uint8_t *frame, const uint8_t *palette,
                           unsigned long tick, void *user)
{
    play_intro_bridge *bridge = user;
    play_state *state = bridge->state;

    /* A BIOS tick is ~55 ms: pace emissions by the stamp they carry, so
       same-tick flushes (full-speed fades) present back to back. */
    play_intro_pace(bridge, tick);
    while (bridge->audio_sent < bridge->intro->audio_count) {
        const yt2_intro_audio_event *event =
            &bridge->intro->audio[bridge->audio_sent++];

        printf("play: audio %lu %s %u\n", event->tick,
               play_intro_audio_name(event->op), event->id);
    }
    if (yt2_surface_blit(&state->surface, 0, 0, frame, PLAY_WIDTH,
                         PLAY_HEIGHT) != YT2_STATUS_OK) {
        bridge->failed = 1;
        return 1;
    }
    memcpy(state->palette.rgb, palette, YT2_PALETTE_SIZE);
    if (yt2_folder_out_take_wrapped(bridge->out)) {
        bridge->force = 1;
    }
    if (yt2_present_frame(bridge->presenter, bridge->out, &state->surface,
                          &state->palette, bridge->force,
                          (uint32_t)tick) != YT2_STATUS_OK) {
        bridge->failed = 1;
        return 1;
    }
    bridge->force = 0;
    (*bridge->presented)++;
    if (bridge->quit || bridge->failed) {
        return 1;
    }
    if (bridge->frames_cap != 0u &&
        *bridge->presented >= bridge->frames_cap) {
        return 1;
    }
    return 0;
}

static int play_intro_vbe_show(const uint8_t *indices, const uint8_t *palette,
                               unsigned long tick, void *user)
{
    play_intro_bridge *bridge = user;
    play_state *state = bridge->state;

    play_intro_pace(bridge, tick);
    printf("play: intro vbe 640x480\n");
    if (yt2_surface_blit(&state->vbe, 0, 0, indices, YT2_INTRO_VBE_WIDTH,
                         YT2_INTRO_VBE_HEIGHT) != YT2_STATUS_OK) {
        bridge->failed = 1;
        return 1;
    }
    memcpy(state->palette.rgb, palette, YT2_PALETTE_SIZE);
    if (yt2_folder_out_take_wrapped(bridge->out)) {
        bridge->force = 1;
    }
    if (yt2_present_frame(bridge->presenter, bridge->out, &state->vbe,
                          &state->palette, bridge->force,
                          (uint32_t)tick) != YT2_STATUS_OK) {
        bridge->failed = 1;
        return 1;
    }
    bridge->force = 0;
    (*bridge->presented)++;
    if (bridge->quit || bridge->failed) {
        return 1;
    }
    if (bridge->frames_cap != 0u &&
        *bridge->presented >= bridge->frames_cap) {
        return 1;
    }
    return 0;
}

/* Creation/roster sessions: the menu loop behind the ported slices. */

static unsigned int session_ascii(const yt2_input_event *event)
{
    if (event->scancode == PLAY_SCAN_ESC) {
        return 0x1Bu;
    }
    if (event->scancode == PLAY_SCAN_ENTER) {
        return 0x0Du;
    }
    if (event->scancode == PLAY_SCAN_BS) {
        return 0x08u;
    }
    if (event->ascii >= 'a' && event->ascii <= 'z') {
        return (unsigned int)event->ascii - 32u;
    }
    return event->ascii;
}

static int session_present(play_state *state, yt2_folder_out *out,
                           yt2_presenter *presenter, int force)
{
    const yt2_palette *base = yt2_world_palette(&state->world, 0u);
    unsigned int cursor = YT2_CREATE_CURSOR_ARROW;
    int shown = 1;

    if (base == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (state->session_mode == 1) {
        cursor = yt2_create_cursor(&state->create);
        shown = yt2_create_cursor_shown(&state->create);
    }
    if (state->session_mode == 3 && state->have_combat &&
        state->combat.chest_open && state->combat.chest_cursor != 0u) {
        /* Carried item icon (fn_SetMouseCursor). */
        cursor = state->combat.chest_cursor;
    }
    if (!state->have_frame) {
        return YT2_STATUS_ERROR;
    }
    memcpy(state->frame.pixels, state->surface.pixels,
           (size_t)PLAY_WIDTH * PLAY_HEIGHT);
    if (shown &&
        yt2_cursor_draw(&state->frame, &state->pictures, cursor,
                        state->cursor_x, state->cursor_y) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    if (state->session_mode == 3 && !state->map_open) {
        /* Explore owns rotation phase and the E0-FF sky window. */
        if (yt2_room_palette(&state->room, &state->palette) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    } else {
        state->palette = *base;
        if (yt2_palette_rotate(&state->palette, state->anim_base,
                               state->rotation_phase) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    return yt2_present_frame(presenter, out, &state->frame, &state->palette,
                             force, state->tick);
}

/* yt2_room_present_fn: the room presents every frame it composes. */
static int explore_present(void *context)
{
    play_state *state = context;

    if (session_present(state, state->session_out, state->session_presenter,
                        0) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    (*state->session_presented)++;
    return YT2_STATUS_OK;
}

/* Live animation pacing: BIOS ticks at 55 ms (B1's DelayTicks),
   independent of the presenter tick. The presenter holds the
   last frame across the sleep. */
static void explore_delay(unsigned int ticks, void *context)
{
    if (context == NULL || ticks == 0u) {
        return;
    }
    yt2_sleep_ms(ticks * PLAY_TICK_MS);
}

static void session_drain_sfx(play_state *state)
{
    if (state->session_mode == 1) {
        unsigned int count = yt2_create_sfx_count(&state->create);

        while (state->create_sfx_seen < count) {
            printf("play: audio %u sfx %u\n", state->tick,
                   yt2_create_sfx(&state->create, state->create_sfx_seen));
            state->create_sfx_seen++;
        }
    } else if (state->session_mode == 2) {
        unsigned int count = yt2_roster_sfx_count(&state->roster);

        while (state->roster_sfx_seen < count) {
            printf("play: audio %u sfx %u\n", state->tick,
                   yt2_roster_sfx(&state->roster, state->roster_sfx_seen));
            state->roster_sfx_seen++;
        }
    } else if (state->session_mode == 4) {
        unsigned int count = yt2_ingamemenu_sfx_count(&state->ingamemenu);

        while (state->ingamemenu_sfx_seen < count) {
            printf("play: audio %u sfx %u\n", state->tick,
                   yt2_ingamemenu_sfx(&state->ingamemenu, state->ingamemenu_sfx_seen));
            state->ingamemenu_sfx_seen++;
        }
    } else if (state->session_mode == 3 && state->have_combat) {
        unsigned int count = yt2_combat_sfx_count(&state->combat);

        while (state->combat_sfx_seen < count) {
            printf("play: audio %u sfx %u\n", state->tick,
                   yt2_combat_sfx_at(&state->combat, state->combat_sfx_seen));
            state->combat_sfx_seen++;
        }
    }
}

/* Roster delete flush (vanilla writes the block at confirm time). */
static void roster_flush_save(void *context)
{
    play_state *state = context;

    yt2_save_write(&state->save, state->save_path);
}

/* Explore flush (vanilla persists visited bits per newly seen cell). Raw
   image write: the party block on disk keeps its boot bytes until a save
   refreshes it, like vanilla's row-sized fn_11FCC writes. */
static void explore_flush_save(play_state *state)
{
    if (yt2_room_take_save_dirty(&state->room)) {
        yt2_file_save(state->save_path, state->save.bytes,
                      state->save.size);
    }
}

/* Overhead map modal (M/L/U: fn_0CE82 + fn_123E9 WaitKey). Renders the
   fixed map (M=31, L=47, U=7 filler) plus the selected legend line;
   any key closes back to explore, marker clicks select legend lines
   (sub_03F6B) and stay open. Palette is the world base. */
static int play_map_render(play_state *state)
{
    memset(state->surface.pixels, 0,
           (size_t)state->surface.width * state->surface.height);
    if (yt2_map_view_render(&state->surface, &state->world,
                            &state->pictures, state->map_id) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    if (state->legend_line >= 0 &&
        state->legend_map == state->map_id &&
        yt2_map_view_draw_legend(&state->surface, &state->pictures,
                                 &state->world,
                                 (unsigned int)state->legend_line) !=
            YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    return YT2_STATUS_OK;
}

static void play_map_open(play_state *state, unsigned int map)
{
    unsigned int saved_id = state->map_id;
    int saved_line = state->legend_line;
    unsigned int saved_map = state->legend_map;

    /* B1 shows nothing when the map lookup fails (v_5110 == 0);
       render first and open only on success. B1 also gates on the
       0x9569 map-UI state (empty on a fresh game, so M no-ops
       there); B2 shows ungated so the shortcut works. */
    state->map_id = map;
    state->legend_line = -1;
    state->legend_map = map;
    if (play_map_render(state) != YT2_STATUS_OK) {
        state->map_id = saved_id;
        state->legend_line = saved_line;
        state->legend_map = saved_map;
        if (state->have_combat) {
            yt2_combat_repaint_keep_portraits(&state->combat,
                                              explore_present, state);
        }
        return;
    }
    state->map_open = 1;
    explore_present(state);
    printf("play: map %u\n", map);
}

static void play_map_close(play_state *state)
{
    state->map_open = 0;
    state->legend_line = -1;
    if (state->have_combat) {
        yt2_combat_repaint_keep_portraits(&state->combat,
                                          explore_present, state);
    }
    printf("play: map done\n");
}

static void session_key(play_state *state, const yt2_input_event *event,
                        int *dirty)
{
    unsigned int ascii;

    if (event->down != 1) {
        return;
    }
    if (state->session_mode == 4) {
        ascii = session_ascii(event);
        if (ascii == 0u) {
            if (yt2_ingamemenu_fkey(&state->ingamemenu, event->scancode) ==
                YT2_STATUS_OK) {
                *dirty = 1;
            }
            session_drain_sfx(state);
            return;
        }
        if (yt2_ingamemenu_key(&state->ingamemenu, ascii) == YT2_STATUS_OK) {
            *dirty = 1;
        }
        session_drain_sfx(state);
        return;
    }
    if (state->session_mode == 3) {
        /* Main-loop dispatch (tbl_KeyDispatch/tbl_ScanDispatch): arrows
           move, D opens the in-game menu, R opens the game menu (unported),
           everything else is silent (loc_00817 ignores ESC/Q/junk). */
        if (state->map_open) {
            /* fn_123E9 WaitKey: any key closes the map (consumed). */
            play_map_close(state);
            session_drain_sfx(state);
            return;
        }
        if (event->scancode == PLAY_SCAN_UP ||
            event->scancode == PLAY_SCAN_DOWN ||
            event->scancode == PLAY_SCAN_LEFT ||
            event->scancode == PLAY_SCAN_RIGHT) {
            /* Formal re-polls arrows (B1 sub_06493: F-keys select,
               the rest is not movement). */
            if (state->have_combat &&
                (state->room.f5064 & 0x1000u) != 0u) {
                return;
            }
            /* Presents happen inside (flash, step); no trailing dup. */
            state->room.last_sfx = 0u;
            if (yt2_room_key(&state->room, &state->surface, event->scancode,
                             explore_present, state) == YT2_STATUS_OK &&
                state->room.last_sfx != 0u) {
                printf("play: audio %u sfx %u\n", state->tick,
                       state->room.last_sfx);
            }
            /* Steps wipe VRAM ink (mirrors the F/B/L/R key path
               and click steps). */
            if (state->have_combat) {
                state->combat.award_pending = 0;
                state->combat.chest_side_panel_lines = 0u;
            }
            if (state->have_combat &&
                (state->room.f5064 & 0x1000u) != 0u) {
                /* Fresh contact settles in fn_06382 at once. */
                yt2_combat_formal_entry(&state->combat, explore_present,
                                        state);
                yt2_combat_repaint(&state->combat, explore_present,
                                   state);
            }
            explore_flush_save(state);
            return;
        }
        ascii = session_ascii(event);
        if (ascii == 'D') {
            /* B1's formal D (loc_06885) nests the menu with mask
               0x5E; explore uses 0xFE. */
            state->ingametitlemenu_requested = 1;
            state->ingamemenu_formal =
                (state->have_combat &&
                 (state->room.f5064 & 0x1000u) != 0u);
            return;
        }
        if ((ascii == 'S' || ascii == 'A') && state->have_combat) {
            if (yt2_combat_key(&state->combat, ascii, explore_present,
                               state) == YT2_STATUS_OK) {
                explore_flush_save(state);
            }
            session_drain_sfx(state);
            return;
        }
        if (ascii == ' ' && state->have_combat) {
            /* SPACE search/prompt/close (fn_0795E/fn_073E3). */
            if (yt2_combat_key(&state->combat, ascii, explore_present,
                               state) == YT2_STATUS_OK) {
                explore_flush_save(state);
            }
            session_drain_sfx(state);
            return;
        }
        if ((ascii == 0x1Bu || ascii == 'N') && state->have_combat &&
            state->combat.chest_prompt) {
            /* Prompt cancel (fn_0795E ax 0); silent elsewhere. */
            if (yt2_combat_key(&state->combat, ascii, explore_present,
                               state) == YT2_STATUS_OK) {
                explore_flush_save(state);
            }
            session_drain_sfx(state);
            return;
        }
        if ((ascii == 'M' || ascii == 'L' || ascii == 'U') &&
            state->have_combat && !state->combat.chest_open &&
            !state->combat.chest_prompt &&
            (state->room.f5064 & 0x1000u) == 0u) {
            /* Overhead map (M=31, L=47, U=7); the modal owns input
               while open, like vanilla's fn_073E3 loop. */
            play_map_open(state, ascii == 'M' ? 31u : ascii == 'L' ? 47u :
                                                               7u);
            session_drain_sfx(state);
            return;
        }
        if (ascii == 'R' ||
            ascii == 'P' || ascii == 'S' ||
            ascii == 'T' || ascii == 'W' || ascii == 'C' ||
            ascii == 'A' || ascii == 'K' || ascii == 'V' ||
            ascii == ' ' || (ascii >= '1' && ascii <= '4')) {
            fprintf(stderr, "play: explore key %02X unported\n", ascii);
            return;
        }
        if (event->ascii == 0u && event->scancode >= PLAY_SCAN_F1 &&
            event->scancode <= PLAY_SCAN_F4 && state->have_combat) {
            /* F1-F4 member pick (B1 fn_0A834); the chest opener
               prompt answers, everything else stays unported. */
            if (yt2_combat_fkey(&state->combat,
                                event->scancode - PLAY_SCAN_F1 + 1u,
                                explore_present, state) ==
                YT2_STATUS_OK) {
                explore_flush_save(state);
                session_drain_sfx(state);
                return;
            }
        }
        if (event->ascii == 0u &&
            ((event->scancode >= PLAY_SCAN_F1 &&
              event->scancode <= PLAY_SCAN_F1 + 4u) ||
             event->scancode == PLAY_SCAN_F1 + 6u)) {
            fprintf(stderr, "play: explore key scan %02X unported\n",
                    event->scancode);
        }
        return;
    }
    if (state->session_mode == 2 && event->scancode >= PLAY_SCAN_F1 &&
        event->scancode <= PLAY_SCAN_F1 + 8u) {
        if (yt2_roster_fkey(&state->roster,
                            event->scancode - PLAY_SCAN_F1 + 1u) ==
            YT2_STATUS_OK) {
            *dirty = 1;
        }
        return;
    }
    ascii = session_ascii(event);
    if (state->session_mode == 1) {
        if (yt2_create_key(&state->create, ascii) == YT2_STATUS_OK) {
            *dirty = 1;
        }
    } else if (yt2_roster_key(&state->roster, ascii) == YT2_STATUS_OK) {
        *dirty = 1;
    }
    session_drain_sfx(state);
}

static void session_click(play_state *state, int x, int y, int right,
                          int *dirty)
{
    if (state->session_mode == 3) {
        /* Modal/prompt clicks (loot, pack, portraits, opener pick).
           Explore clicks (arrows, portraits, viewport) land in
           later slices; the combat layer ignores them. */
        if (state->map_open && !right) {
            unsigned int line;

            /* Marker legend (sub_03F6B); misses change nothing. */
            if (yt2_map_view_legend_at(&state->world, state->map_id, x,
                                       y, &line) == YT2_STATUS_OK) {
                state->legend_line = (int)line;
                state->legend_map = state->map_id;
                if (play_map_render(state) == YT2_STATUS_OK) {
                    explore_present(state);
                }
                *dirty = 1;
            }
            session_drain_sfx(state);
            return;
        }
        if (state->have_combat) {
            /* Presents happen inside (arrows flash/step); the room
               SFX logs like the key path. */
            state->room.last_sfx = 0u;
            if (yt2_combat_click(&state->combat, x, y, right,
                                 explore_present, state) !=
                YT2_STATUS_OK) {
                session_drain_sfx(state);
                return;
            }
            if (state->room.last_sfx != 0u) {
                printf("play: audio %u sfx %u\n", state->tick,
                       state->room.last_sfx);
            }
            if (state->combat.menu_requested) {
                /* Disk verb: nest the menu like D (no repaint). */
                state->combat.menu_requested = 0;
                state->ingametitlemenu_requested = 1;
                state->ingamemenu_formal = state->combat.menu_formal;
                state->combat.menu_formal = 0;
                session_drain_sfx(state);
                return;
            }
            /* Member packs change under portrait drops; sync the
               block like the key path before the file flush. */
            yt2_party_write(state->save.bytes + YT2_SAVE_PARTY_OFFSET,
                            YT2_SAVE_PARTY_SIZE, state->room.party);
            if (yt2_combat_repaint(&state->combat, explore_present,
                                   state) == YT2_STATUS_OK) {
                explore_flush_save(state);
            }
            *dirty = 1;
        }
        session_drain_sfx(state);
        return;
    }
    if (state->session_mode == 4) {
        if (yt2_ingamemenu_click(&state->ingamemenu, x, y, right) == YT2_STATUS_OK) {
            *dirty = 1;
        }
        session_drain_sfx(state);
        return;
    }
    if (state->session_mode == 1) {
        /* fn_MouseMoveBy hotspot: carried icons click 8 pixels in. */
        if (right) {
            if (yt2_create_right_click(&state->create, x, y) ==
                YT2_STATUS_OK) {
                *dirty = 1;
            }
            session_drain_sfx(state);
            return;
        }
        if (yt2_create_cursor(&state->create) !=
            YT2_CREATE_CURSOR_ARROW) {
            x += 8;
            y += 8;
        }
        if (yt2_create_click(&state->create, x, y) == YT2_STATUS_OK) {
            *dirty = 1;
        }
        session_drain_sfx(state);
        return;
    }
    if (!right &&
        yt2_roster_click(&state->roster, x, y) == YT2_STATUS_OK) {
        *dirty = 1;
    }
    session_drain_sfx(state);
}

static int session_done(const play_state *state)
{
    if (state->session_mode == 1) {
        return yt2_create_done(&state->create);
    }
    if (state->session_mode == 3) {
        return state->titlemenu_requested;
    }
    if (state->session_mode == 4) {
        return yt2_ingamemenu_done(&state->ingamemenu);
    }
    return yt2_roster_done(&state->roster);
}

static const char *session_stage_name(const play_state *state)
{
    if (state->session_mode == 3) {
        return "explore";
    }
    if (state->session_mode == 4) {
        return "ingamemenu";
    }
    if (state->session_mode == 1) {
        switch (state->create.stage) {
        case YT2_CREATE_STAGE_CLASS:
            return "class";
        case YT2_CREATE_STAGE_PORTRAIT:
            return "portrait";
        case YT2_CREATE_STAGE_SHEET:
            return "sheet";
        case YT2_CREATE_STAGE_ITEMS:
            return "items";
        case YT2_CREATE_STAGE_NAME:
            return "name";
        case YT2_CREATE_STAGE_PREVIEW:
            return "preview";
        case YT2_CREATE_STAGE_KEEP:
            return "keep";
        case YT2_CREATE_STAGE_QUIT:
            return "quit";
        default:
            return "?";
        }
    }
    switch (state->roster.view) {
    case YT2_ROSTER_VIEW_GRID:
        return "grid";
    case YT2_ROSTER_VIEW_SHEET:
        return "sheet";
    case YT2_ROSTER_VIEW_CONFIRM:
        return "confirm";
    case YT2_ROSTER_VIEW_DONE:
        return "done";
    default:
        return "?";
    }
}

/* Forward: the explore/formal D key nests the in-game menu mid-session. */
static int run_ingamemenu(play_state *state, yt2_folder_out *out,
                    yt2_folder_in *in, yt2_presenter *presenter,
                    play_options *options, unsigned int *presented,
                    int *running, uint16_t mask, int combat, int explore);

/* B1's title-entry reset (fn_0D36C/fn_1B9D0) on NEWGAME: reload the
   WORLD roster template (created characters wiped, SQUIRE/DIANA/YENDOR
   restored), clear the assignment, and rewrite CURGAME fresh. B1 runs it
   only when v_5060/v_505E guards are clear (NEWGAME-confirm clears them;
   create/roster quit-to-title keeps the roster); B2's call sites encode
   that: menu EXIT_MENU (NEWGAME-confirmed) resets, session ends don't. */
static int play_title_reset(play_state *state)
{
    if (state == NULL || !state->have_save) {
        return YT2_STATUS_ERROR;
    }
    yt2_save_free(&state->save);
    if (yt2_save_start(&state->world, &state->save) != YT2_STATUS_OK ||
        yt2_save_write(&state->save, state->save_path) != YT2_STATUS_OK) {
        state->have_save = 0;
        return YT2_STATUS_ERROR;
    }
    printf("play: new party\n");
    return YT2_STATUS_OK;
}

/*
 * One modal session: poll the link, apply every input with its own
 * present (the original presents once per input poll), and return when
 * the session ends, quits, or hits the frame cap.
 */
static int run_session(play_state *state, yt2_folder_out *out,
                       yt2_folder_in *in, yt2_presenter *presenter,
                       play_options *options, unsigned int *presented,
                       int *running)
{
    state->session_out = out;
    state->session_presenter = presenter;
    state->session_presented = presented;
    if (session_present(state, out, presenter, 1) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-play: cannot present the session\n");
        return YT2_STATUS_ERROR;
    }
    (*presented)++;
    while (*running && !session_done(state) &&
           (options->frames == 0u || *presented < options->frames)) {
        const uint8_t *frame;
        uint32_t length;
        int moved = 0;
        int poll;

        yt2_sleep_ms(options->tick_ms);
        state->tick++;
        if (yt2_folder_in_take_wrapped(in)) {
            state->hello_sent = 0;
        }
        for (;;) {
            yt2_input_event event;

            poll = yt2_folder_in_poll(in, &frame, &length);
            if (poll <= 0) {
                break;
            }
            if (yt2_input_decode_frame(frame, length, &event) != 1) {
                continue;
            }
            switch (event.kind) {
            case YT2_INPUT_EVENT_HELLO:
                if (event.role == YT2_ROLE_CLIENT && event.version == 0 &&
                    !state->hello_sent) {
                    if (yt2_present_hello(presenter, out) != YT2_STATUS_OK ||
                        yt2_present_pace(presenter, out, options->tick_ms) !=
                            YT2_STATUS_OK) {
                        *running = 0;
                    }
                    state->hello_sent = 1;
                }
                break;
            case YT2_INPUT_EVENT_KEYFRAME_REQUEST:
                if (session_present(state, out, presenter, 1) ==
                    YT2_STATUS_OK) {
                    (*presented)++;
                }
                break;
            case YT2_INPUT_EVENT_KEY:
                if (event.down == 1) {
                    const char *before = session_stage_name(state);
                    int dirty = 0;

                    session_key(state, &event, &dirty);
                    if (dirty &&
                        session_present(state, out, presenter, 0) ==
                            YT2_STATUS_OK) {
                        (*presented)++;
                    }
                    if (session_stage_name(state) != before) {
                        printf("play: %s %s\n",
                               state->session_mode == 1 ? "create" :
                               state->session_mode == 4 ? "ingamemenu" : "roster",
                               session_stage_name(state));
                    }
                }
                break;
            case YT2_INPUT_EVENT_MOUSE_MOVE:
                state->cursor_x += event.dx;
                state->cursor_y += event.dy;
                clamp_cursor(state);
                /* Coalesced below: a burst of moves presents once. */
                moved = 1;
                break;
            case YT2_INPUT_EVENT_MOUSE_ABS:
                state->cursor_x = (int)event.x;
                state->cursor_y = (int)event.y;
                clamp_cursor(state);
                moved = 1;
                break;
            case YT2_INPUT_EVENT_MOUSE_BUTTON:
                if (event.down == 1u &&
                    (event.button == 0u || event.button == 2u)) {
                    const char *before = session_stage_name(state);
                    int dirty = 0;

                    session_click(state, state->cursor_x, state->cursor_y,
                                  event.button == 2u, &dirty);
                    if (dirty &&
                        session_present(state, out, presenter, 0) ==
                            YT2_STATUS_OK) {
                        (*presented)++;
                    }
                    if (session_stage_name(state) != before) {
                        printf("play: %s %s\n",
                               state->session_mode == 1 ? "create" :
                               state->session_mode == 4 ? "ingamemenu" : "roster",
                               session_stage_name(state));
                    }
                }
                break;
            case YT2_INPUT_EVENT_QUIT:
                *running = 0;
                break;
            default:
                break;
            }
            if (session_done(state)) {
                break;
            }
        }
        if (poll < 0) {
            fprintf(stderr, "yt2p-play: input failure\n");
            return YT2_STATUS_ERROR;
        }
        if (moved && session_present(state, out, presenter, 0) ==
            YT2_STATUS_OK) {
            (*presented)++;
        }
        if (state->ingametitlemenu_requested && state->session_mode == 3) {
            int mode = state->session_mode;
            yt2_ingamemenu_exit exit;
            uint16_t mask = state->ingamemenu_formal ? 0x5Eu : 0xFEu;
            int formal = state->ingamemenu_formal;

            state->ingametitlemenu_requested = 0;
            state->ingamemenu_formal = 0;
            if (run_ingamemenu(state, out, in, presenter, options, presented,
                         running, mask, formal, 1) != YT2_STATUS_OK) {
                return YT2_STATUS_ERROR;
            }
            state->session_mode = mode;
            exit = yt2_ingamemenu_result(&state->ingamemenu);
            if (exit == YT2_INGAMEMENU_EXIT_QUIT) {
                *running = 0;
            } else if (exit == YT2_INGAMEMENU_EXIT_MENU) {
                state->titlemenu_requested = 1;
            } else if (state->have_combat) {
                /* CONTINUE: B1 re-renders on menu close (loc_00614
                   path); without it the menu base keeps group-8 pic
                   10 over the disk cell and stale ticks never
                   restore the icon row. Portraits stay retained
                   (B1's gather never repaints them). */
                yt2_combat_repaint_keep_portraits(&state->combat,
                                                  explore_present,
                                                  state);
            }
        }
        if (state->have_combat && state->combat.death_pending &&
            state->session_mode == 3) {
            /* Wipe (fn_15FBC): the death screen is painted; the
               menu (0x70: load/newgame/quit) runs, quit ends the
               game, menu unwinds to title, anything else
               re-embarks explore from the (possibly loaded) save
               like fn_0F2C0. */
            yt2_ingamemenu_exit exit;

            state->combat.death_pending = 0;
            printf("play: wipe\n");
            if (run_ingamemenu(state, out, in, presenter, options, presented,
                         running, 0x70u, 0, 1) != YT2_STATUS_OK) {
                return YT2_STATUS_ERROR;
            }
            state->session_mode = 3;
            exit = yt2_ingamemenu_result(&state->ingamemenu);
            if (exit == YT2_INGAMEMENU_EXIT_QUIT) {
                *running = 0;
            } else if (exit == YT2_INGAMEMENU_EXIT_MENU) {
                state->titlemenu_requested = 1;
            } else {
                uint16_t seed = (uint16_t)yt2_pit_now();

                if (yt2_room_init(&state->room, &state->world,
                                  &state->pictures, &state->save,
                                  &state->save.party,
                                  seed) != YT2_STATUS_OK ||
                    yt2_room_embark(&state->room, &state->surface,
                                    explore_present,
                                    state) != YT2_STATUS_OK) {
                    fprintf(stderr, "yt2p-play: cannot re-embark\n");
                    return YT2_STATUS_ERROR;
                }
                explore_flush_save(state);
                state->room.delay = explore_delay;
                state->room.delay_context = state;
                if (yt2_combat_init(&state->combat, &state->room,
                                    &state->save, &state->pictures,
                                    &state->world, &state->items,
                                    &state->surface) == YT2_STATUS_OK) {
                    state->have_combat = 1;
                    state->combat_sfx_seen = 0u;
                }
                printf("play: explore start\n");
            }
        }
        if (state->session_mode == 3) {
            int idle_explore;
            int idle_formal;

            /* Map modal (fn_123E9 WaitKey): frozen (no ticks,
               repaints, or presents); input still polls above and
               presents on open/legend/close. */
            if (state->map_open) {
                continue;
            }
            /* The room ticks its own clock/sky/rotation countdowns.
               Idle explore re-renders stale (B1's per-poll fn_10E6A:
               sprites advance under the palette); formal and modal
               idle present the palette over the settled frame. */
            if (yt2_room_tick(&state->room) != YT2_STATUS_OK) {
                fprintf(stderr, "yt2p-play: room tick failed\n");
                return YT2_STATUS_ERROR;
            }
            idle_explore = (state->room.f5064 & 0x1000u) == 0u &&
                           (!state->have_combat ||
                            (!state->combat.chest_open &&
                             !state->combat.chest_prompt));
            idle_formal =
                state->have_combat &&
                (state->room.f5064 & 0x1000u) != 0u &&
                !state->combat.chest_open && !state->combat.chest_prompt &&
                state->room.rotation_phase % 3u == 0u;
            if (yt2_room_tick_dirty(&state->room)) {
                /* B1 animates ranged ~5 ticks, engaged ~15 (idle
                   observation); the /3 divider matches formal. */
                if (idle_explore || idle_formal) {
                    int no_compass = state->room.no_compass;

                    /* Formal idle keeps panel and side panel (no plaque
                       repaint, no compass over the names); portraits
                       stay retained too (B1's fn_10E6A never paints
                       them), so the member border survives ticks. */
                    if (idle_formal) {
                        state->room.no_compass = 1;
                        yt2_room_render_stale_keep_portraits(
                            &state->room, &state->surface);
                    } else {
                        yt2_room_render_stale(&state->room,
                                              &state->surface);
                    }
                    state->room.no_compass = no_compass;
                }
                if (session_present(state, out, presenter, 0) ==
                    YT2_STATUS_OK) {
                    (*presented)++;
                }
            }
            continue;
        }
        if (state->rotation_countdown > 0u) {
            state->rotation_countdown--;
        }
        if (state->rotation_countdown == 0u) {
            state->rotation_countdown = PLAY_ROTATION_TICKS;
            state->rotation_phase =
                (state->rotation_phase + 1u) % YT2_PALETTE_ANIM_PHASES;
            if (session_present(state, out, presenter, 0) == YT2_STATUS_OK) {
                (*presented)++;
            }
        }
    }
    return YT2_STATUS_OK;
}

/* One in-game menu to an exit (boot mask, explore 0xFE, formal 0x5E). */
static int run_ingamemenu(play_state *state, yt2_folder_out *out,
                    yt2_folder_in *in, yt2_presenter *presenter,
                    play_options *options, unsigned int *presented,
                    int *running, uint16_t mask, int combat, int explore)
{
    int result;

    printf("play: ingamemenu start\n");
    if (yt2_ingamemenu_begin(&state->ingamemenu, &state->save.party, &state->save,
                       state->ingamemenu_slots, &state->pictures, &state->surface,
                       state->game_dir, mask, combat, explore,
                       0u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-play: cannot open the in-game menu\n");
        return YT2_STATUS_ERROR;
    }
    if (explore && state->have_combat) {
        /* fn_0EBD6 opens with fn_06EA4: the side panels wipe, so a pending
           award or message never paints under the menu. Title menus
           (fn_0D36C) do not wipe. */
        yt2_combat_menu_open(&state->combat);
        yt2_ingamemenu_wipe_side_panels(&state->ingamemenu);
    }
    state->session_mode = 4;
    state->ingamemenu_sfx_seen = 0u;
    result = run_session(state, out, in, presenter, options, presented,
                         running);
    state->session_mode = 0;
    if (result == YT2_STATUS_OK) {
        static const char *const names[] = {
            "none", "continue", "quit", "menu",
        };
        yt2_ingamemenu_exit exit = yt2_ingamemenu_result(&state->ingamemenu);

        printf("play: ingamemenu done %s\n",
               exit <= YT2_INGAMEMENU_EXIT_MENU ? names[exit] : "?");
    }
    return result;
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
                "[--tick-ms N] [--spelltext] [--no-intro]\n",
                argv[0]);
        return 2;
    }
    memset(&state, 0, sizeof(state));
    state.legend_line = -1;
    state.scene = options.scene;
    state.cursor_x = options.cursor_x;
    state.cursor_y = options.cursor_y;
    state.spelltext = options.spelltext;
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
            YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-play: cannot start the session\n");
        goto done;
    }
    /* A short VBE surface only loses the publisher logo (black beat). */
    state.have_vbe =
        yt2_surface_init(&state.vbe, YT2_INTRO_VBE_WIDTH,
                         YT2_INTRO_VBE_HEIGHT) == YT2_STATUS_OK;
    state.have_frame =
        yt2_surface_init(&state.frame, PLAY_WIDTH, PLAY_HEIGHT) ==
        YT2_STATUS_OK;
    if (yt2_world_anim_base(&state.world, state.anim_base) != YT2_STATUS_OK ||
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
    if (!options.no_intro) {
        play_intro_bridge bridge;
        yt2_intro intro;
        yt2_titlemenu titlemenu;
        yt2_intro_transition transition;

        memset(&bridge, 0, sizeof(bridge));
        bridge.state = &state;
        bridge.out = &out;
        bridge.in = &in;
        bridge.presenter = &presenter;
        bridge.tick_ms = options.tick_ms;
        bridge.presented = &presented;
        bridge.frames_cap = options.frames;
        bridge.force = 1;
        snprintf(state.save_path, sizeof(state.save_path), "%s/CURGAME",
                 argv[1]);
        snprintf(state.game_dir, sizeof(state.game_dir), "%s", argv[1]);
        /* Vanilla boot (fn_1B9D0): fresh party from the template, written
           straight back before anything shows; CURGAME is never read. */
        if (yt2_save_start(&state.world, &state.save) != YT2_STATUS_OK ||
            yt2_save_write(&state.save, state.save_path) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-play: cannot start the party\n");
            goto done;
        }
        state.have_save = 1;
        state.ingamemenu_scanned = yt2_ingamemenu_scan_slots(state.ingamemenu_slots, argv[1]);
        printf("play: new party\n");
        printf("play: intro start\n");
        if (yt2_present_hello(&presenter, &out) != YT2_STATUS_OK ||
            yt2_present_pace(&presenter, &out, options.tick_ms) !=
                YT2_STATUS_OK ||
            yt2_intro_init(&intro, &state.pictures, &state.world,
                           play_intro_emit, play_intro_poll,
                           &bridge) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-play: cannot start the intro\n");
            goto done;
        }
        bridge.intro = &intro;
        /* Live play lets the client draw the pointer. */
        intro.engine_cursor = 0;
        if (state.have_vbe) {
            intro.vbe_show = play_intro_vbe_show;
        }
        if (yt2_titlemenu_init(&titlemenu, &intro, options.tick_ms) != YT2_STATUS_OK) {
            fprintf(stderr, "yt2p-play: cannot start the menu\n");
            yt2_intro_free(&intro);
            goto done;
        }
        transition = yt2_titlemenu_boot(&titlemenu);
        printf("play: intro transition %s frames %u\n",
               play_intro_transition_name(transition), presented);
        state.tick = (unsigned int)intro.tick;
        if (bridge.failed) {
            fprintf(stderr, "yt2p-play: intro failure\n");
            yt2_intro_free(&intro);
            goto done;
        }
        if (bridge.quit ||
            transition == YT2_INTRO_TRANSITION_QUIT ||
            (options.frames != 0u && presented >= options.frames)) {
            printf("play: %u frames\n", presented);
            result = 0;
            yt2_intro_free(&intro);
            goto done;
        }
        while (running && (transition == YT2_INTRO_TRANSITION_CREATE ||
                            transition == YT2_INTRO_TRANSITION_LOAD ||
                            transition == YT2_INTRO_TRANSITION_EMBARK) &&
               (options.frames == 0u || presented < options.frames)) {
            if (transition == YT2_INTRO_TRANSITION_CREATE) {
                uint16_t seed = (uint16_t)yt2_pit_now();

                printf("play: create start\n");
                if (yt2_create_begin(&state.create, &state.save.party,
                                     &state.items, &state.pictures,
                                     &state.surface,
                                     seed) != YT2_STATUS_OK) {
                    printf("play: roster full\n");
                } else {
                    unsigned int bi;

                    yt2_create_open_class(&state.create);
                    state.session_mode = 1;
                    state.create_sfx_seen = 0u;
                    /* The BIOS ring survives the menu: drain leftovers. */
                    for (bi = 0; bi < bridge.key_count; bi++) {
                        int dirty = 0;

                        session_key(&state, &bridge.keys[bi], &dirty);
                    }
                    bridge.key_count = 0;
                    if (run_session(&state, &out, &in, &presenter, &options,
                                    &presented, &running) != YT2_STATUS_OK) {
                        break;
                    }
                    state.session_mode = 0;
                    if (state.create.dirty &&
                        yt2_save_write(&state.save, state.save_path) ==
                            YT2_STATUS_OK) {
                        printf("play: save written\n");
                    }
                    printf("play: create done\n");
                }
            } else if (transition == YT2_INTRO_TRANSITION_LOAD) {
                printf("play: roster start\n");
                if (yt2_roster_begin(&state.roster, &state.save.party,
                                     &state.items, &state.pictures,
                                     &state.surface) != YT2_STATUS_OK) {
                    fprintf(stderr, "yt2p-play: cannot open roster\n");
                    break;
                }
                /* Deletions flush at confirm time; toggles/DONE stay in
                   memory like vanilla. */
                state.roster.flush = roster_flush_save;
                state.roster.flush_context = &state;
                state.session_mode = 2;
                state.roster_sfx_seen = 0u;
                if (run_session(&state, &out, &in, &presenter, &options,
                                &presented, &running) != YT2_STATUS_OK) {
                    break;
                }
                state.session_mode = 0;
                printf("play: roster done party %u %u %u %u\n",
                       state.save.party.assigned[0],
                       state.save.party.assigned[1],
                       state.save.party.assigned[2],
                       state.save.party.assigned[3]);
            } else if (transition == YT2_INTRO_TRANSITION_EMBARK) {
                unsigned int assigned =
                    (unsigned int)state.save.party.assigned[0] +
                    (unsigned int)state.save.party.assigned[1] +
                    (unsigned int)state.save.party.assigned[2] +
                    (unsigned int)state.save.party.assigned[3];
                yt2_ingamemenu_exit ingamemenu_exit;

                /* fn_0D36C derives v_505E bit 0x200 from this sum on
                   the menu path; an empty party buzzes instead of
                   embarking. */
                if (assigned == 0u) {
                    printf("play: audio %u sfx 3\n", state.tick);
                } else {
                    uint16_t seed = (uint16_t)yt2_pit_now();

                    yt2_room_update_skill_flags(&state.save.party);
                    if (run_ingamemenu(&state, &out, &in, &presenter, &options,
                                 &presented, &running,
                                 state.ingamemenu_scanned ? 0x71u : 0x9Du,
                                 0, 0) != YT2_STATUS_OK) {
                        break;
                    }
                    ingamemenu_exit = yt2_ingamemenu_result(&state.ingamemenu);
                    if (ingamemenu_exit == YT2_INGAMEMENU_EXIT_QUIT) {
                        running = 0;
                        break;
                    }
                    if (ingamemenu_exit != YT2_INGAMEMENU_EXIT_CONTINUE) {
                        /* NEWGAME-confirmed: B1's title-entry reset
                           (fn_0D36C) before the title shows. */
                        if (play_title_reset(&state) != YT2_STATUS_OK) {
                            fprintf(stderr,
                                    "yt2p-play: cannot reset the party\n");
                            break;
                        }
                        /* The stream tick jumps ahead (never backwards),
                           pacing rebases, and the fade-out darkens the
                           in-game menu, not a stale intro frame. */
                        play_intro_resync(&bridge, &intro, state.tick);
                        yt2_intro_adopt_screen(&intro, state.surface.pixels,
                                               state.palette.rgb);
                        transition = yt2_titlemenu_run(&titlemenu);
                        printf("play: intro transition %s frames %u\n",
                               play_intro_transition_name(transition),
                               presented);
                        if (bridge.failed) {
                            fprintf(stderr, "yt2p-play: intro failure\n");
                            break;
                        }
                        state.tick = (unsigned int)intro.tick;
                        continue;
                    }
                    printf("play: explore start\n");
                    state.session_out = &out;
                    state.session_presenter = &presenter;
                    state.session_presented = &presented;
                    if (yt2_room_init(&state.room, &state.world,
                                      &state.pictures, &state.save,
                                      &state.save.party,
                                      seed) != YT2_STATUS_OK ||
                        yt2_room_embark(&state.room, &state.surface,
                                        explore_present,
                                        &state) != YT2_STATUS_OK) {
                        fprintf(stderr, "yt2p-play: cannot embark\n");
                        break;
                    }
                    explore_flush_save(&state);
                    state.room.delay = explore_delay;
                    state.room.delay_context = &state;
                    if (yt2_combat_init(&state.combat, &state.room,
                                        &state.save, &state.pictures,
                                        &state.world, &state.items,
                                        &state.surface) == YT2_STATUS_OK) {
                        state.have_combat = 1;
                        state.combat_sfx_seen = 0u;
                    }
                    state.session_mode = 3;
                    state.titlemenu_requested = 0;
                    if (run_session(&state, &out, &in, &presenter, &options,
                                    &presented, &running) != YT2_STATUS_OK) {
                        break;
                    }
                    state.session_mode = 0;
                    printf("play: explore done\n");
                }
            }
            if (!running ||
                (options.frames != 0u && presented >= options.frames)) {
                break;
            }
            if (state.titlemenu_requested) {
                /* Explore/wipe-menu NEWGAME (fn_0D36C): reset the
                   party before the title shows, then clear. */
                state.titlemenu_requested = 0;
                if (play_title_reset(&state) != YT2_STATUS_OK) {
                    fprintf(stderr,
                            "yt2p-play: cannot reset the party\n");
                    break;
                }
            }
            play_intro_resync(&bridge, &intro, state.tick);
            yt2_intro_adopt_screen(&intro, state.surface.pixels,
                                   state.palette.rgb);
            transition = yt2_titlemenu_run(&titlemenu);
            printf("play: intro transition %s frames %u\n",
                   play_intro_transition_name(transition), presented);
            if (bridge.failed) {
                fprintf(stderr, "yt2p-play: intro failure\n");
                break;
            }
            state.tick = (unsigned int)intro.tick;
        }
        yt2_intro_free(&intro);
        if (state.have_save) {
            yt2_save_free(&state.save);
            state.have_save = 0;
        }
        printf("play: %u frames\n", presented);
        result = 0;
        goto done;
    }
    if (render_scene(&state) != YT2_STATUS_OK ||
        yt2_present_hello(&presenter, &out) != YT2_STATUS_OK ||
        yt2_present_pace(&presenter, &out, options.tick_ms) !=
            YT2_STATUS_OK ||
        yt2_present_frame(&presenter, &out, &state.surface, &state.palette,
                          1, state.tick) != YT2_STATUS_OK) {
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
                            YT2_STATUS_OK ||
                        yt2_present_pace(&presenter, &out, options.tick_ms) !=
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
                                  &state.palette, force,
                                  state.tick) != YT2_STATUS_OK) {
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
    if (state.have_save) {
        yt2_save_free(&state.save);
    }
    if (state.have_frame) {
        yt2_surface_free(&state.frame);
    }
    yt2_surface_free(&state.vbe);
    yt2_surface_free(&state.surface);
    yt2_world_free(&state.world);
    yt2_pictures_free(&state.pictures);
    return result;
}
