/*
 * yt2p-combat: headless combat/loot matrix runner.
 *
 *   yt2p-combat SAVEGAME PICTURES.VGA WORLD.DAT OUTDIR [--seed N]
 *       [--tick-state DGROUP.BIN] [--render-states Q1:Q2:...]
 *       [--script FILE]
 *
 * Loads B1's spider-staging save, embarks explore, transplants the
 * B1 live tick state (seed plus the staged monster record: the walk
 * spawns it at the same place, this only pins the bytes), replays
 * one action per script line (S/A), and dumps per-action states
 * (video, palette, bytes, SFX) for the B1/B2 quiescent matrix.
 * Vanilla never saves the RNG seed, so matrix runs set it
 * explicitly (B1's live v_F7AC). The render states pin the
 * tick-driven visuals (sky, rotation, origin, flags) per action;
 * combat bytes (seed, HP, objects, XP) always derive from play.
 */
#include "yt2/combat.h"
#include "yt2/cursor.h"
#include "yt2/file.h"
#include "yt2/items.h"
#include "yt2/palette.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/room.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define COMBAT_WIDTH 320u
#define COMBAT_HEIGHT 200u

/* Last scripted mouse position (B1-actual per action; setup seeds it). */
static int tool_mouse_x = 115;
static int tool_mouse_y = 162;

static void tool_mkdir(const char *dir)
{
    char tmp[1024];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (i = 1u; tmp[i] != 0; i++) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0755);
}

static int tool_present(void *context)
{
    unsigned int *frames = (unsigned int *)context;

    if (frames != NULL) {
        (*frames)++;
    }
    return YT2_STATUS_OK;
}

static uint16_t tool_read16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static int tool_write(const char *path, const void *bytes, size_t size)
{
    FILE *out = fopen(path, "wb");

    if (out == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (size > 0u && fwrite(bytes, 1, size, out) != size) {
        fclose(out);
        return YT2_STATUS_ERROR;
    }
    fclose(out);
    return YT2_STATUS_OK;
}

/*
 * Render-only bytes from a live dgroup: window origin, sky,
 * rotation, flags. Combat bytes (seed, HP, objects, XP) are never
 * touched here; they derive from play.
 */
static void tool_render_bytes(yt2_room *room, const uint8_t *dgroup,
                              size_t size)
{
    if (size < 0x95E6u) {
        return;
    }
    /* The window origin moves every cell pointer, so it lands
       before the relocate. */
    room->origin_x = (int)(int16_t)tool_read16(dgroup + 0x0CF6);
    room->origin_y = (int)(int16_t)tool_read16(dgroup + 0x0CFE);
    yt2_room_relocate(room);
    room->sky_step = (int16_t)tool_read16(dgroup + 0x9583);
    /* B2's window sits one sky step behind v_9581. */
    room->sky = (uint16_t)((int)tool_read16(dgroup + 0x9581) -
                           room->sky_step);
    room->sky_left = tool_read16(dgroup + 0x95E5);
    room->r9469 = tool_read16(dgroup + 0x9469);
    room->shade_scene = tool_read16(dgroup + 0x94F1);
    /* sub_100F3 writes the DAC, then wraps v_506A, so the live
       D0-DF phase lags the counter by one. */
    room->rotation_phase =
        (unsigned int)((tool_read16(dgroup + 0x506A) + 3u) % 4u);
    room->speed_left = tool_read16(dgroup + 0x50E8);
    if (room->party != NULL) {
        room->party->explore_flags = tool_read16(dgroup + 0x946F);
    }
}

static void tool_render_text(yt2_combat *combat, const char *path);

/* One 0x9C object record (DGROUP slot layout) into a B2 object. */
static void tool_record_fill(yt2_room_object *obj, const uint8_t *slot)
{
    unsigned int b;

    obj->id = tool_read16(slot + 0x00);
    obj->x = tool_read16(slot + 0x02);
    obj->y = tool_read16(slot + 0x04);
    obj->cell = tool_read16(slot + 0x06);
    obj->pic = tool_read16(slot + 0x08);
    obj->mode = tool_read16(slot + 0x0A);
    obj->flags = tool_read16(slot + 0x0C);
    obj->flags2 = tool_read16(slot + 0x0E);
    obj->hp = tool_read16(slot + 0x10);
    obj->target = tool_read16(slot + 0x12);
    obj->data14 = tool_read16(slot + 0x14);
    obj->data16 = tool_read16(slot + 0x16);
    obj->clip = tool_read16(slot + 0x18);
    obj->pic2 = tool_read16(slot + 0x1A);
    obj->step = tool_read16(slot + 0x1C);
    obj->cooldown = tool_read16(slot + 0x1E);
    for (b = 0u; b < 14u; b++) {
        obj->name[b] = (char)slot[0x32u + b];
    }
    obj->name[14] = 0;
    obj->anim_base = tool_read16(slot + 0x4C);
    obj->hp_max = tool_read16(slot + 0x50);
    obj->power = tool_read16(slot + 0x52);
    obj->attack_base = tool_read16(slot + 0x54);
    obj->attack_mult = tool_read16(slot + 0x5A);
    obj->sfx_hit = tool_read16(slot + 0x5C);
    obj->sfx_miss = tool_read16(slot + 0x5E);
    obj->initiative = tool_read16(slot + 0x56);
    obj->defense = tool_read16(slot + 0x58);
    obj->fx_base = tool_read16(slot + 0x6C);
    obj->fx_special = tool_read16(slot + 0x6E);
    obj->range60 = tool_read16(slot + 0x60);
    obj->over_dx = tool_read16(slot + 0x68);
    obj->over_dy = tool_read16(slot + 0x6A);
    obj->fx_off = tool_read16(slot + 0x8E);
    obj->fx_cap = tool_read16(slot + 0x90);
    for (b = 0u; b < 6u; b++) {
        obj->shade[b] = slot[0x72u + b];
    }
    for (b = 0u; b < 16u; b++) {
        obj->loot[b] = slot[0x7Eu + b];
    }
    obj->group = tool_read16(slot + 0x92);
    obj->cull = tool_read16(slot + 0x94);
    obj->vuln_status = tool_read16(slot + 0x96);
    obj->vuln_resist = tool_read16(slot + 0x98);
}

/* Stamp the window cell for a transplanted object. */
static void tool_stamp_cell(yt2_room *room, const yt2_room_object *obj)
{
    int wx = (int)obj->x - room->origin_x;
    int wy = (int)obj->y - room->origin_y;

    if (wx >= 0 && wy >= 0 && wx < (int)YT2_ROOM_WINDOW_SIDE &&
        wy < (int)YT2_ROOM_WINDOW_SIDE) {
        room->window[wy][wx].flags |= YT2_ROOM_CELL_OBJECT;
        room->window[wy][wx].misc = obj->id;
    }
}

/*
 * Compact setup (name=value plus slot0=hex): the staged monster
 * record, seed and combat flags. Committed; the dgroup form stays
 * for live captures.
 */
static void tool_setup_text(yt2_combat *combat, const char *path)
{
    yt2_room *room = combat->room;
    FILE *in = fopen(path, "r");
    char line[512];
    uint8_t slot[0x9Cu];
    int have_slot = 0;
    uint16_t seed = 0u;
    uint16_t f5062 = 0u;
    uint16_t f5064 = 0u;
    uint8_t gold[4] = {0u, 0u, 0u, 0u};
    uint8_t ore[4] = {0u, 0u, 0u, 0u};
    uint8_t nuore[4] = {0u, 0u, 0u, 0u};
    uint16_t messages = 0u;
    uint16_t instance_next = 0u;

    if (in == NULL) {
        return;
    }
    memset(slot, 0, sizeof(slot));
    while (fgets(line, sizeof(line), in) != NULL) {
        char *eq = strchr(line, '=');

        if (eq == NULL) {
            continue;
        }
        *eq = 0;
        if (strcmp(line, "slot0") == 0) {
            size_t n = 0u;
            char *hex = eq + 1;

            while (n < sizeof(slot) && hex[0] != 0 && hex[1] != 0) {
                unsigned int byte;

                if (sscanf(hex, "%2x", &byte) != 1) {
                    break;
                }
                slot[n++] = (uint8_t)byte;
                hex += 2;
            }
            have_slot = n == sizeof(slot);
        } else if (strcmp(line, "seed") == 0) {
            seed = (uint16_t)strtoul(eq + 1, NULL, 0);
        } else if (strcmp(line, "f5062") == 0) {
            f5062 = (uint16_t)strtoul(eq + 1, NULL, 0);
        } else if (strcmp(line, "f5064") == 0) {
            f5064 = (uint16_t)strtoul(eq + 1, NULL, 0);
        } else if (strcmp(line, "gold") == 0 || strcmp(line, "ore") == 0 ||
                   strcmp(line, "nuore") == 0) {
            uint8_t *dst = gold;
            char *hex = eq + 1;

            if (strcmp(line, "ore") == 0) {
                dst = ore;
            } else if (strcmp(line, "nuore") == 0) {
                dst = nuore;
            }
            for (size_t n = 0u; n < 4u; n++) {
                unsigned int byte;

                if (sscanf(hex, "%2x", &byte) != 1) {
                    break;
                }
                dst[n] = (uint8_t)byte;
                hex += 2;
            }
        } else if (strcmp(line, "messages") == 0) {
            messages = (uint16_t)strtoul(eq + 1, NULL, 0);
        } else if (strcmp(line, "instance_next") == 0) {
            instance_next = (uint16_t)strtoul(eq + 1, NULL, 0);
        } else if (strcmp(line, "mouse") == 0) {
            int mx = 115;
            int my = 162;

            if (sscanf(eq + 1, "%d,%d", &mx, &my) == 2) {
                tool_mouse_x = mx;
                tool_mouse_y = my;
            }
        }
    }
    fclose(in);
    /* Render lines first: the relocate stamps doors, then the
       record and flags land on top like tool_transplant. */
    tool_render_text(combat, path);
    room->rng_seed = seed;
    room->f5062 = f5062;
    room->f5064 = f5064;
    if (room->party != NULL) {
        memcpy(room->party->gold, gold, sizeof(gold));
        memcpy(room->party->ore, ore, sizeof(ore));
        memcpy(room->party->nuore, nuore, sizeof(nuore));
    }
    combat->chest_messages = messages;
    combat->chest_instance_next = instance_next;
    if (have_slot) {
        yt2_room *dst = combat->room;
        unsigned int id = tool_read16(slot);
        yt2_room_object *obj = NULL;
        unsigned int i;

        for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
            if (dst->objects[i].id == id) {
                obj = &dst->objects[i];
                break;
            }
        }
        if (obj == NULL) {
            for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
                if (dst->objects[i].id == 0u) {
                    obj = &dst->objects[i];
                    break;
                }
            }
        }
        if (obj != NULL && id != 0u) {
            tool_record_fill(obj, slot);
            tool_stamp_cell(dst, obj);
        }
    }
}

/*
 * Transplant the staged monster (B1 slot 0: id, position, picture,
 * flags, HP) over the matching B2 record so the matrix starts from
 * identical bytes. Vanilla spawns from the same defs; the walk may
 * place the variant or the engage bit differently.
 */
static void tool_transplant(yt2_combat *combat, const uint8_t *dgroup,
                            size_t size)
{
    yt2_room *room = combat->room;
    const uint8_t *slot;
    unsigned int id;
    unsigned int i;
    yt2_room_object *obj = NULL;

    if (size < 0xF20u + 0x9Cu) {
        return;
    }
    tool_render_bytes(room, dgroup, size);
    slot = dgroup + 0xF20u;
    id = tool_read16(slot);
    if (id == 0u) {
        return;
    }
    for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
        if (room->objects[i].id == id) {
            obj = &room->objects[i];
            break;
        }
    }
    if (obj == NULL) {
        for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
            if (room->objects[i].id == 0u) {
                obj = &room->objects[i];
                break;
            }
        }
    }
    if (obj == NULL) {
        return;
    }
    tool_record_fill(obj, slot);
    tool_stamp_cell(room, obj);
    room->f5062 = tool_read16(dgroup + 0x5062);
    room->f5064 = tool_read16(dgroup + 0x5064);
    room->rng_seed = tool_read16(dgroup + 0xF7AC);
}

/*
 * Compact render state (name=value lines): origin_x/y, sky,
 * sky_step, sky_left, r9469, rotation_phase, speed_left,
 * explore_flags, shade_scene. Committed per action; the dgroup
 * form stays for live captures.
 */
static void tool_render_text(yt2_combat *combat, const char *path)
{
    yt2_room *room = combat->room;
    FILE *in = fopen(path, "r");
    char line[128];
    int origin_x = 0;
    int origin_y = 0;
    int have_origin = 0;

    if (in == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), in) != NULL) {
        char name[64];
        long value;
        char *eq = strchr(line, '=');

        if (eq == NULL || (size_t)(eq - line) >= sizeof(name)) {
            continue;
        }
        memcpy(name, line, (size_t)(eq - line));
        name[eq - line] = 0;
        value = strtol(eq + 1, NULL, 0);
        if (strcmp(name, "origin_x") == 0) {
            origin_x = (int)value;
            have_origin = 1;
        } else if (strcmp(name, "origin_y") == 0) {
            origin_y = (int)value;
            have_origin = 1;
        } else if (strcmp(name, "sky") == 0) {
            room->sky = (uint16_t)value;
        } else if (strcmp(name, "sky_step") == 0) {
            room->sky_step = (int16_t)value;
        } else if (strcmp(name, "sky_left") == 0) {
            room->sky_left = (uint16_t)value;
        } else if (strcmp(name, "r9469") == 0) {
            room->r9469 = (uint16_t)value;
        } else if (strcmp(name, "rotation_phase") == 0) {
            room->rotation_phase = (unsigned int)value;
        } else if (strcmp(name, "speed_left") == 0) {
            room->speed_left = (uint16_t)value;
        } else if (strcmp(name, "explore_flags") == 0) {
            if (room->party != NULL) {
                room->party->explore_flags = (uint16_t)value;
            }
        } else if (strcmp(name, "shade_scene") == 0) {
            room->shade_scene = (uint16_t)value;
        }
    }
    fclose(in);
    /* B1 never relocates mid-modal (fn_10E6A retains the view);
       a mid-modal rebuild would wipe the open-chest object flip.
       The origin lands with the relocate or not at all: a bare
       transplant would desync it from the built window. */
    if (!combat->chest_open && !combat->chest_prompt) {
        if (have_origin) {
            room->origin_x = origin_x;
            room->origin_y = origin_y;
        }
        yt2_room_relocate(room);
    }
}

static void tool_render_state(yt2_combat *combat, const char *path)
{
    FILE *tick;
    uint8_t *dgroup;
    size_t size;
    size_t len = strlen(path);

    if (len > 4u && strcmp(path + len - 4u, ".txt") == 0) {
        tool_render_text(combat, path);
        return;
    }
    tick = fopen(path, "rb");
    if (tick == NULL) {
        return;
    }
    fseek(tick, 0, SEEK_END);
    size = (size_t)ftell(tick);
    fseek(tick, 0, SEEK_SET);
    dgroup = (uint8_t *)malloc(size > 0u ? size : 1u);
    if (dgroup == NULL || fread(dgroup, 1, size, tick) != size) {
        free(dgroup);
        fclose(tick);
        return;
    }
    fclose(tick);
    tool_render_bytes(combat->room, dgroup, size);
    free(dgroup);
}

static int tool_dump(yt2_combat *combat, const char *dir, unsigned int frames,
                     int cursor)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    char path[1024];
    char state[8192];
    char sfx[2048];
    size_t at = 0u;
    unsigned int i;

    if (cursor) {
        /* fn_1A6B5: the cursor lands in the presented frame only;
           the work buffer keeps its background for the next step. */
        uint8_t saved[16][16];
        int sx;
        int sy;

        for (sy = 0; sy < 16; sy++) {
            for (sx = 0; sx < 16; sx++) {
                int px = tool_mouse_x + sx;
                int py = tool_mouse_y + sy;

                saved[sy][sx] = 0u;
                if (px >= 0 && py >= 0 &&
                    (unsigned int)px < combat->surface->width &&
                    (unsigned int)py < combat->surface->height) {
                    saved[sy][sx] = combat->surface->pixels[
                        (size_t)py * combat->surface->stride + (size_t)px];
                }
            }
        }
        yt2_cursor_draw(combat->surface, combat->pictures,
                        combat->chest_cursor, tool_mouse_x, tool_mouse_y);
        at += (size_t)snprintf(state + at, sizeof(state) - at,
                               "cursor=%d,%d shape=%04X\n", tool_mouse_x,
                               tool_mouse_y, combat->chest_cursor);
        snprintf(path, sizeof(path), "%s/video.bin", dir);
        if (tool_write(path, combat->surface->pixels,
                       (size_t)COMBAT_WIDTH * COMBAT_HEIGHT) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        for (sy = 0; sy < 16; sy++) {
            for (sx = 0; sx < 16; sx++) {
                int px = tool_mouse_x + sx;
                int py = tool_mouse_y + sy;

                if (px >= 0 && py >= 0 &&
                    (unsigned int)px < combat->surface->width &&
                    (unsigned int)py < combat->surface->height) {
                    combat->surface->pixels[
                        (size_t)py * combat->surface->stride + (size_t)px] =
                        saved[sy][sx];
                }
            }
        }
    } else {
        snprintf(path, sizeof(path), "%s/video.bin", dir);
        if (tool_write(path, combat->surface->pixels,
                       (size_t)COMBAT_WIDTH * COMBAT_HEIGHT) !=
            YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
    }
    {
        yt2_palette palette;

        if (yt2_room_palette(combat->room, &palette) == YT2_STATUS_OK) {
            snprintf(path, sizeof(path), "%s/palette.bin", dir);
            if (tool_write(path, palette.rgb,
                           sizeof(palette.rgb)) != YT2_STATUS_OK) {
                return YT2_STATUS_ERROR;
            }
        }
    }
    at += (size_t)snprintf(state + at, sizeof(state) - at,
                           "frames=%u seed=%04X f5060=%04X f5062=%04X "
                           "f5064=%04X xflags=%04X sky=%04X phase=%u "
                           "shade=%04X\n",
                           frames, room->rng_seed, room->f5060, room->f5062,
                           room->f5064, party->explore_flags, room->sky,
                           room->rotation_phase, room->shade_scene);
    for (i = 0u; i < YT2_ROOM_OBJECT_SLOTS; i++) {
        const yt2_room_object *obj = &room->objects[i];

        if (obj->id == 0u) {
            continue;
        }
        at += (size_t)snprintf(
            state + at, sizeof(state) - at,
            "obj slot=%u id=%u x=%u y=%u cell=%04X pic=%u mode=%u "
            "flags=%04X hp=%d/%u\n",
            i, obj->id, obj->x, obj->y, obj->cell, obj->pic, obj->mode,
            obj->flags, (int16_t)obj->hp, obj->hp_max);
    }
    for (i = 0u; i < YT2_COMBAT_ENGAGED_MAX; i++) {
        const yt2_room_object *obj = &room->near_objects[i];

        if (obj->id == 0u) {
            continue;
        }
        at += (size_t)snprintf(
            state + at, sizeof(state) - at,
            "eng%d id=%u hp=%d flags=%04X\n", i, obj->id,
            (int16_t)obj->hp, obj->flags);
    }
    for (i = 0u; i < YT2_PARTY_ASSIGNED_MAX; i++) {
        unsigned int slot = party->assigned[i];

        if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
            continue;
        }
        at += (size_t)snprintf(
            state + at, sizeof(state) - at, "member%d slot=%u xp=%08X\n",
            i, slot, party->roster[slot - 1u].experience);
    }
    at += (size_t)snprintf(
        state + at, sizeof(state) - at, "pos=%u,%u facing=%04X\n", party->x,
        party->y, party->facing);
    at += (size_t)snprintf(
        state + at, sizeof(state) - at,
        "purse=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
        party->gold[0], party->gold[1], party->gold[2], party->gold[3],
        party->ore[0], party->ore[1], party->ore[2], party->ore[3],
        party->nuore[0], party->nuore[1], party->nuore[2],
        party->nuore[3]);
    at += (size_t)snprintf(
        state + at, sizeof(state) - at,
        "chest open=%d prompt=%d idx=%u taken=%02X opener=%u "
        "cursor=%04X carry=%u,%u,%u messages=%u saveidx=%u inst=%u\n",
        combat->chest_open, combat->chest_prompt, combat->chest_index,
        combat->chest_taken, combat->chest_opener, combat->chest_cursor,
        combat->chest_carried, combat->chest_carried_weight,
        combat->chest_carried_count, combat->chest_messages,
        combat->chest_save_index, combat->chest_instance_next);
    for (i = 0u; i < YT2_CHEST_SLOTS; i++) {
        at += (size_t)snprintf(
            state + at, sizeof(state) - at, "loot%u id=%u count=%u\n", i,
            combat->chest_staging[i].id, combat->chest_staging[i].count);
    }
    for (i = 0u; i < YT2_QUICK_ACCESS_SLOTS; i++) {
        at += (size_t)snprintf(
            state + at, sizeof(state) - at, "stash%u id=%u count=%u\n", i,
            combat->quick_access[i].id, combat->quick_access[i].count);
    }
    for (i = 0u; i < YT2_PARTY_ASSIGNED_MAX; i++) {
        unsigned int slot = party->assigned[i];
        const yt2_character *ch;

        if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
            continue;
        }
        ch = &party->roster[slot - 1u];
        at += (size_t)snprintf(
            state + at, sizeof(state) - at,
            "gear%u hp=%d/%d mp=%d status=%04X carried=%u\n", i,
            ch->current.combat.hit_points, ch->maximum.combat.hit_points,
            ch->current.combat.magic_points, ch->status,
            ch->carried_weight);
        for (unsigned int p = 0u; p < YT2_PACK_SLOTS; p++) {
            at += (size_t)snprintf(
                state + at, sizeof(state) - at, "pack%u_%u id=%u count=%u\n",
                i, p, ch->pack[p].id, ch->pack[p].count);
        }
        for (unsigned int e = 0u; e < YT2_EQUIPMENT_SLOTS; e++) {
            at += (size_t)snprintf(
                state + at, sizeof(state) - at, "equip%u_%u id=%u count=%u\n",
                i, e, ch->equipment[e].id, ch->equipment[e].count);
        }
    }
    if (combat->save != NULL && combat->save->bytes != NULL) {
        size_t taken_at =
            (size_t)YT2_CHEST_TAKEN_OFFSET + combat->chest_index;
        size_t unlock_at = (size_t)YT2_CHEST_UNLOCK_OFFSET +
                           combat->chest_index / 8u;

        if (taken_at < combat->save->size &&
            unlock_at < combat->save->size) {
            at += (size_t)snprintf(
                state + at, sizeof(state) - at, "chestfile taken=%02X unlock=%02X\n",
                combat->save->bytes[taken_at],
                combat->save->bytes[unlock_at]);
        }
    }
    snprintf(path, sizeof(path), "%s/state.txt", dir);
    if (tool_write(path, state, strlen(state)) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    at = 0u;
    for (i = 0u; i < yt2_combat_sfx_count(combat); i++) {
        at += (size_t)snprintf(sfx + at, sizeof(sfx) - at, "%u\n",
                               yt2_combat_sfx_at(combat, i));
    }
    snprintf(path, sizeof(path), "%s/sfx.txt", dir);
    return tool_write(path, sfx, strlen(sfx));
}

int main(int argc, char **argv)
{
    const char *seed_opt = NULL;
    const char *tick_opt = NULL;
    const char *script_opt = NULL;
    const char *render_opt = NULL;
    const char *setup_opt = NULL;
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_party *party;
    yt2_room room;
    yt2_surface surface;
    yt2_combat combat;
    uint8_t *dgroup = NULL;
    size_t dgroup_size = 0u;
    FILE *script = NULL;
    char line[64];
    unsigned int step = 0u;
    unsigned int frames = 0u;
    char dir[1024];
    int arg;

    if (argc < 5) {
        fprintf(stderr,
                "usage: %s SAVEGAME PICTURES.VGA WORLD.DAT OUTDIR "
                "[--seed N] [--tick-state DGROUP.BIN] "
                "[--render-states Q1:...] [--setup-text FILE] "
                "[--script FILE]\n",
                argv[0]);
        return 2;
    }
    for (arg = 5; arg < argc; arg++) {
        if (strcmp(argv[arg], "--seed") == 0 && arg + 1 < argc) {
            seed_opt = argv[++arg];
        } else if (strcmp(argv[arg], "--tick-state") == 0 && arg + 1 < argc) {
            tick_opt = argv[++arg];
        } else if (strcmp(argv[arg], "--script") == 0 && arg + 1 < argc) {
            script_opt = argv[++arg];
        } else if (strcmp(argv[arg], "--render-states") == 0 &&
                   arg + 1 < argc) {
            render_opt = argv[++arg];
        } else if (strcmp(argv[arg], "--setup-text") == 0 && arg + 1 < argc) {
            setup_opt = argv[++arg];
        } else {
            fprintf(stderr, "yt2p-combat: unknown option %s\n", argv[arg]);
            return 2;
        }
    }
    memset(&pictures, 0, sizeof(pictures));
    memset(&world, 0, sizeof(world));
    memset(&items, 0, sizeof(items));
    memset(&save, 0, sizeof(save));
    memset(&room, 0, sizeof(room));
    memset(&surface, 0, sizeof(surface));
    memset(&combat, 0, sizeof(combat));
    if (yt2_pictures_load(argv[2], &pictures) != YT2_STATUS_OK ||
        yt2_world_load(argv[3], &world) != YT2_STATUS_OK ||
        yt2_items_load(&items, &world) != YT2_STATUS_OK ||
        yt2_save_load(argv[1], &save) != YT2_STATUS_OK ||
        yt2_surface_init(&surface, COMBAT_WIDTH,
                         COMBAT_HEIGHT) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-combat: cannot load inputs\n");
        return 1;
    }
    party = &save.party;
    if (yt2_room_init(&room, &world, &pictures, &save, party,
                      0x1234u) != YT2_STATUS_OK ||
        yt2_room_embark(&room, &surface, tool_present,
                        &frames) != YT2_STATUS_OK ||
        yt2_combat_init(&combat, &room, &save, &pictures, &world,
                        &items, &surface) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-combat: cannot embark\n");
        return 1;
    }
    if (tick_opt != NULL) {
        FILE *tick = fopen(tick_opt, "rb");

        if (tick == NULL) {
            fprintf(stderr, "yt2p-combat: cannot open %s\n", tick_opt);
            return 1;
        }
        fseek(tick, 0, SEEK_END);
        dgroup_size = (size_t)ftell(tick);
        fseek(tick, 0, SEEK_SET);
        dgroup = (uint8_t *)malloc(dgroup_size > 0u ? dgroup_size : 1u);
        if (dgroup == NULL ||
            fread(dgroup, 1, dgroup_size, tick) != dgroup_size) {
            fprintf(stderr, "yt2p-combat: cannot read %s\n", tick_opt);
            fclose(tick);
            return 1;
        }
        fclose(tick);
        tool_transplant(&combat, dgroup, dgroup_size);
        free(dgroup);
    }
    if (setup_opt != NULL) {
        tool_setup_text(&combat, setup_opt);
    }
    if (seed_opt != NULL) {
        room.rng_seed = (uint16_t)strtoul(seed_opt, NULL, 0);
    }
    yt2_room_render(&room, &surface);
    snprintf(dir, sizeof(dir), "%s/A00-setup", argv[4]);
    tool_mkdir(dir);
    if (tool_dump(&combat, dir, frames, 0) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-combat: cannot dump %s\n", dir);
        return 1;
    }
    if (script_opt != NULL) {
        script = fopen(script_opt, "r");
        if (script == NULL) {
            fprintf(stderr, "yt2p-combat: cannot open %s\n", script_opt);
            return 1;
        }
    }
    if (script != NULL) {
        char render_paths[4096];
        const char *render_list[64];
        unsigned int render_count = 0u;

        if (render_opt != NULL) {
            size_t at = 0u;
            size_t start = 0u;
            size_t len = strlen(render_opt);

            if (len >= sizeof(render_paths)) {
                len = sizeof(render_paths) - 1u;
            }
            memcpy(render_paths, render_opt, len);
            render_paths[len] = 0;
            while (at <= len && render_count < 64u) {
                if (render_paths[at] == ':' || render_paths[at] == 0) {
                    render_paths[at] = 0;
                    if (at > start) {
                        render_list[render_count++] =
                            &render_paths[start];
                    }
                    start = at + 1u;
                }
                at++;
            }
        }
        while (fgets(line, sizeof(line), script) != NULL) {
            int click_x = 0;
            int click_y = 0;
            int is_click = 0;
            int is_right = 0;
            char key = line[0];

            if ((key == 'C' || key == 'r') &&
                sscanf(line + 1, "%d %d", &click_x, &click_y) == 2) {
                is_click = 1;
                is_right = key == 'r';
            } else if (key != 'S' && key != 'A' && key != 'F' &&
                       key != 'B' && key != 'L' && key != 'R' &&
                       key != 'N' && key != ' ') {
                continue;
            }
            step++;
            if (step <= render_count) {
                tool_render_state(&combat, render_list[step - 1u]);
            }
            if (is_click) {
                tool_mouse_x = click_x;
                tool_mouse_y = click_y;
                if (yt2_combat_click(&combat, click_x, click_y, is_right,
                                     tool_present, &frames) !=
                        YT2_STATUS_OK ||
                    yt2_combat_repaint(&combat, tool_present, &frames) !=
                        YT2_STATUS_OK) {
                    fprintf(stderr, "yt2p-combat: step %u (click) failed\n",
                            step);
                    fclose(script);
                    return 1;
                }
            } else if (yt2_combat_key(&combat, (unsigned int)key,
                                      tool_present,
                                      &frames) != YT2_STATUS_OK) {
                fprintf(stderr, "yt2p-combat: step %u (%c) failed\n", step,
                        key);
                fclose(script);
                return 1;
            }
            snprintf(dir, sizeof(dir), "%s/A%02u-%c%u", argv[4], step,
                     is_click ? (is_right ? 'r' : 'C') :
                                (key == ' ' ? '_' : key),
                     step);
            tool_mkdir(dir);
            if (tool_dump(&combat, dir, frames, step > 5u) != YT2_STATUS_OK) {
                fprintf(stderr, "yt2p-combat: cannot dump %s\n", dir);
                fclose(script);
                return 1;
            }
        }
        fclose(script);
    }
    printf("combat: %u steps %u frames %u sfx\n", step, frames,
           yt2_combat_sfx_count(&combat));
    return 0;
}
