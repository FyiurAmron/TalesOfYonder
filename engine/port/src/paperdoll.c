#include "yt2/paperdoll.h"

#include <string.h>

#include "yt2/item_detail.h"
#include "yt2/savefile.h"

/* Staging occupied flags (+0x15C): 0x1000 cell-11 bag, 0x2000 /
   0x4000 / 0x8000 pack bags (slots 1C8 / 1A2 / 17C). */
#define PAPERDOLL_STAGE_CELL11 0x1000u
#define PAPERDOLL_STAGE_PACK \
    ((unsigned int)(0x2000u | 0x4000u | 0x8000u))

/* DS:60F4h: paperdoll cells relative to the pane origin (v_5056,
   v_505A).  21 entries, byte-identical to the EXE table. */
const yt2_paperdoll_box yt2_paperdoll_cells[21] = {
    {2, 17, 2, 17, 1}, {20, 35, 2, 17, 2}, {38, 53, 2, 17, 3},
    {2, 17, 19, 34, 4}, {20, 35, 19, 34, 5}, {38, 53, 19, 34, 6},
    {2, 17, 36, 51, 7}, {20, 35, 36, 51, 8}, {38, 53, 36, 51, 9},
    {0, 15, 64, 79, 10}, {40, 55, 64, 79, 11},
    {0, 15, 103, 118, 12}, {40, 55, 103, 118, 13},
    {4, 11, 95, 102, 14}, {44, 51, 95, 102, 15},
    {16, 39, 52, 73, 16}, {16, 39, 74, 93, 17},
    {17, 38, 94, 122, 18}, {10, 40, 123, 135, 19},
    {12, 15, 91, 102, 20}, {40, 43, 91, 102, 21},
};

static uint16_t doll_read16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void doll_write16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void doll_sfx(yt2_paperdoll_ctx *ctx, unsigned int id)
{
    if (ctx == NULL || id == 0u || ctx->sfx_log == NULL ||
        ctx->sfx_count == NULL || *ctx->sfx_count >= ctx->sfx_cap) {
        return;
    }
    ctx->sfx_log[(*ctx->sfx_count)++] = id;
}

static int doll_item(yt2_paperdoll_ctx *ctx, unsigned int id,
                     const uint8_t **def, const uint8_t **row)
{
    const uint8_t *record;

    if (def != NULL) {
        *def = NULL;
    }
    if (row != NULL) {
        *row = NULL;
    }
    if (ctx == NULL || id == 0u) {
        return YT2_STATUS_ERROR;
    }
    record = yt2_item_def(ctx->items, id);
    if (record == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (def != NULL) {
        *def = record;
    }
    if (row != NULL) {
        *row = yt2_item_param_row(ctx->items, record);
    }
    return YT2_STATUS_OK;
}

static uint16_t doll_def_flag(const uint8_t *def)
{
    return doll_read16(def + 12);
}

static uint16_t doll_def_icon(const uint8_t *def)
{
    return doll_read16(def + 8);
}

static uint16_t doll_def_weight(const uint8_t *def)
{
    return doll_read16(def + 10);
}

void yt2_paperdoll_init(yt2_paperdoll_ctx *ctx, const yt2_items *items,
                   uint8_t *store_base, size_t store_size,
                   uint16_t *store_bump, uint16_t *store_free,
                   unsigned int *sfx_log, unsigned int *sfx_count,
                   unsigned int sfx_cap, yt2_paperdoll_bonus_fn bonus_apply,
                   void *bonus_ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->items = items;
    ctx->store_base = store_base;
    ctx->store_size = store_size;
    ctx->store_bump = store_bump;
    ctx->store_free = store_free;
    ctx->cursor = 0u;
    ctx->carried = 0u;
    ctx->carried_count = 0u;
    ctx->carried_weight = 0u;
    ctx->sfx_log = sfx_log;
    ctx->sfx_count = sfx_count;
    ctx->sfx_cap = sfx_cap;
    ctx->bonus_apply = bonus_apply;
    ctx->bonus_ctx = bonus_ctx;
}

void yt2_paperdoll_new_party(yt2_paperdoll_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->store_bump != NULL) {
        *ctx->store_bump = 2u;
    }
    if (ctx->store_free != NULL) {
        *ctx->store_free = 0u;
    }
}

unsigned int yt2_paperdoll_hit(int ox, int oy, int x, int y)
{
    unsigned int i;

    for (i = 0u; i < sizeof(yt2_paperdoll_cells) / sizeof(yt2_paperdoll_cells[0]);
         i++) {
        if (ox + yt2_paperdoll_cells[i].x0 <= x &&
            x <= ox + yt2_paperdoll_cells[i].x1 &&
            oy + yt2_paperdoll_cells[i].y0 <= y &&
            y <= oy + yt2_paperdoll_cells[i].y1) {
            return yt2_paperdoll_cells[i].id;
        }
    }
    return 0u;
}

/* Container file store address (B1 op 0x0B seek): base + index
   * 0x22, or NULL past the region (B1 would grow DOS files; the
   port's fixed image refuses instead). */
static uint8_t *doll_store_at(yt2_paperdoll_ctx *ctx, unsigned int index)
{
    size_t at;

    if (ctx == NULL || ctx->store_base == NULL) {
        return NULL;
    }
    at = (size_t)index * YT2_SAVE_STORE_SIZE;
    if (at + YT2_SAVE_STORE_SIZE > ctx->store_size) {
        return NULL;
    }
    return ctx->store_base + at;
}

/* sub_16D56: read a store into a staging slot's weight+cells. A
   short read leaves zeros (B1's silent short-read path). */
static void doll_store_read(yt2_paperdoll_ctx *ctx, unsigned int index,
                            yt2_container_stage *stage)
{
    const uint8_t *store = doll_store_at(ctx, index);
    unsigned int i;

    if (store == NULL) {
        stage->weight = 0u;
        for (i = 0u; i < YT2_CONTAINER_CELLS; i++) {
            stage->cells[i].id = 0u;
            stage->cells[i].count = 0u;
        }
        return;
    }
    stage->weight = doll_read16(store);
    for (i = 0u; i < YT2_CONTAINER_CELLS; i++) {
        stage->cells[i].id = doll_read16(store + 2u + i * 4u);
        stage->cells[i].count = doll_read16(store + 4u + i * 4u);
    }
}

void yt2_paperdoll_store_flush(yt2_paperdoll_ctx *ctx,
                          yt2_container_stage *stage)
{
    uint8_t *store;
    unsigned int i;

    if (ctx == NULL || stage == NULL || stage->id == 0u) {
        return;
    }
    store = doll_store_at(ctx, stage->count);
    if (store != NULL) {
        doll_write16(store, stage->weight);
        for (i = 0u; i < YT2_CONTAINER_CELLS; i++) {
            doll_write16(store + 2u + i * 4u, stage->cells[i].id);
            doll_write16(store + 4u + i * 4u, stage->cells[i].count);
        }
    }
    stage->id = 0u;
    stage->count = 0u;
}

/* fn_16532: mint a store index (free-list pop or bump), zeroing
   its file store. Boots 0/0 like B1 (post-load mints collide
   like vanilla). */
static unsigned int doll_store_mint(yt2_paperdoll_ctx *ctx)
{
    uint8_t *store;
    unsigned int index;

    if (ctx->store_free != NULL && *ctx->store_free != 0u) {
        index = *ctx->store_free;
        store = doll_store_at(ctx, index);
        *ctx->store_free = store == NULL ? 0u : doll_read16(store);
    } else if (ctx->store_bump != NULL) {
        index = *ctx->store_bump;
        *ctx->store_bump = (uint16_t)(*ctx->store_bump + 1u);
    } else {
        index = 0u;
    }
    store = doll_store_at(ctx, index);
    if (store != NULL) {
        memset(store, 0, YT2_SAVE_STORE_SIZE);
    }
    return index;
}

/* Take cascade (B1 loc_170DB): subtract weight from the staged
   accumulators (fallthrough by resolve flags, magic 0x11 slots
   exempt) and the carried total. Plain u16 wraparound like B1. */
static void doll_cascade_sub(yt2_character *character, unsigned int flags,
                             unsigned int w)
{
    uint16_t weight = (uint16_t)w;

    if (flags == 0x400u) {
        if (character->stage[0].id != 0x11u) {
            character->stage[0].weight =
                (uint16_t)(character->stage[0].weight - weight);
        }
        if (character->stage[1].id != 0x11u) {
            character->stage[1].weight =
                (uint16_t)(character->stage[1].weight - weight);
        }
        if (character->stage[2].id != 0x11u) {
            character->stage[2].weight =
                (uint16_t)(character->stage[2].weight - weight);
        }
    } else if (flags == 0x200u) {
        if (character->stage[1].id != 0x11u) {
            character->stage[1].weight =
                (uint16_t)(character->stage[1].weight - weight);
        }
        if (character->stage[2].id != 0x11u) {
            character->stage[2].weight =
                (uint16_t)(character->stage[2].weight - weight);
        }
    } else if (flags == 0x100u) {
        if (character->stage[2].id != 0x11u) {
            character->stage[2].weight =
                (uint16_t)(character->stage[2].weight - weight);
        }
    }
    character->carried_weight =
        (uint16_t)(character->carried_weight - weight);
}

/* Drop cascade (B1 loc_16C38): mirror image (adds). */
static void doll_cascade_add(yt2_character *character, unsigned int flags,
                             unsigned int w)
{
    uint16_t weight = (uint16_t)w;

    if (flags == 0x400u) {
        if (character->stage[0].id != 0x11u) {
            character->stage[0].weight =
                (uint16_t)(character->stage[0].weight + weight);
        }
        if (character->stage[1].id != 0x11u) {
            character->stage[1].weight =
                (uint16_t)(character->stage[1].weight + weight);
        }
        if (character->stage[2].id != 0x11u) {
            character->stage[2].weight =
                (uint16_t)(character->stage[2].weight + weight);
        }
    } else if (flags == 0x200u) {
        if (character->stage[1].id != 0x11u) {
            character->stage[1].weight =
                (uint16_t)(character->stage[1].weight + weight);
        }
        if (character->stage[2].id != 0x11u) {
            character->stage[2].weight =
                (uint16_t)(character->stage[2].weight + weight);
        }
    } else if (flags == 0x100u) {
        if (character->stage[2].id != 0x11u) {
            character->stage[2].weight =
                (uint16_t)(character->stage[2].weight + weight);
        }
    }
    character->carried_weight =
        (uint16_t)(character->carried_weight + weight);
}

/* Bag size mask (B1 loc_1685F ladder): bag 0x2000, box 0x4000,
   backpack 0x8000, magic fits all (0xFFFF), else 0. */
static unsigned int doll_bag_mask(uint16_t flags)
{
    if ((flags & 0x4u) != 0u) {
        return 0x2000u;
    }
    if ((flags & 0x8u) != 0u) {
        return 0x4000u;
    }
    if ((flags & 0x10u) != 0u) {
        return 0x8000u;
    }
    if ((flags & 0x2u) != 0u) {
        return 0xFFFFu;
    }
    return 0u;
}

/* sub_17132: pack-cell drops/swaps into a staged bag require the
   carried item's def+0xE size word to fit the open bag (buzz on
   mismatch). Unstaged and equipment cells pass. */
static int doll_staged_fit(yt2_paperdoll_ctx *ctx, yt2_character *character,
                           unsigned int id)
{
    const uint8_t *def;
    const uint8_t *row;
    const yt2_container_stage *stage;
    unsigned int mask;

    if (id >= 10u) {
        return 1;
    }
    if (character->stage[0].id != 0u &&
        (character->flags & 0x8000u) != 0u) {
        stage = &character->stage[0];
    } else if (character->stage[1].id != 0u &&
               (character->flags & 0x4000u) != 0u) {
        stage = &character->stage[1];
    } else if (character->stage[2].id != 0u &&
               ((character->flags & 0x2000u) != 0u ||
                (character->flags & PAPERDOLL_STAGE_CELL11) != 0u)) {
        stage = &character->stage[2];
    } else {
        return 1;
    }
    if (doll_item(ctx, stage->id, &def, &row) != YT2_STATUS_OK) {
        return 1;
    }
    mask = doll_bag_mask(doll_def_flag(def));
    /* v_50DA: the carried item's def+0xE size word. */
    if (doll_item(ctx, ctx->carried, &def, &row) != YT2_STATUS_OK) {
        return 0;
    }
    if ((doll_read16(def + 14u) & (uint16_t)mask) == 0u) {
        doll_sfx(ctx, 3u);
        return 0;
    }
    return 1;
}

/* sub_16F85: whether the carried item may drop into a member cell. */
static int doll_drop_compat(yt2_paperdoll_ctx *ctx, yt2_character *character,
                            unsigned int cell, unsigned int carried)
{
    const uint8_t *def;
    const uint8_t *row;

    if (doll_item(ctx, carried, &def, &row) != YT2_STATUS_OK) {
        return 0;
    }
    if (cell <= 8u) {
        return 1;
    }
    if (cell == 10u) {
        return (doll_def_flag(def) & 0x8000u) != 0u;
    }
    if (cell == 11u) {
        /* Full swaps anything; empty takes only 0x2000 (bags). */
        if (character->equipment[1].id != 0u) {
            return 1;
        }
        return (doll_def_flag(def) & 0x2000u) != 0u;
    }
    if (cell == 12u) {
        /* Flag 0x4000 plus row word 1 bit 0 clear, or cell 13
           empty when set. Rows always exist here (B1 fn_02262
           materializes 0xC000 rows); a missing row mirrors
           DGROUP:2 bit 0 (clear). */
        uint16_t text;

        if ((doll_def_flag(def) & 0x4000u) == 0u) {
            return 0;
        }
        text = row == NULL ? 0u : doll_read16(row + 2u);
        if ((text & 1u) == 0u) {
            return 1;
        }
        return character->equipment[3].id == 0u;
    }
    if (cell == 13u) {
        return (doll_def_flag(def) & 0x800u) != 0u &&
               (character->flags & 0x20u) == 0u;
    }
    if (cell == 14u || cell == 15u) {
        return (doll_def_flag(def) & 0x400u) != 0u;
    }
    if ((doll_def_flag(def) & 0x200u) == 0u || row == NULL) {
        return 0;
    }
    if (cell == 16u) {
        return (doll_read16(row + 2) & 0x8000u) != 0u;
    }
    if (cell == 17u) {
        return (doll_read16(row + 2) & 0x4000u) != 0u;
    }
    if (cell == 18u) {
        return (doll_read16(row + 2) & 0x2000u) != 0u;
    }
    if (cell == 19u) {
        return (doll_read16(row + 2) & 0x1000u) != 0u;
    }
    return (doll_read16(row + 2) & 0x800u) != 0u;
}

/* sub_16E64 resolve for pack cells 1-8: the staged slot's cells
   when a bag is open (17C > 1A2 > 1C8 by nonzero id), else the
   pack. Returns the slot plus transient flags (0x80 pack,
   0x100 / 0x200 / 0x400 staged). Equipment/effects never
   redirect (0x80 / 0x40). */
static yt2_item *doll_stage_slot(yt2_character *character, unsigned int id,
                                 unsigned int *flags)
{
    unsigned int s;

    if (flags != NULL) {
        *flags = 0x80u;
    }
    if (id < 1u || id > 8u) {
        return NULL;
    }
    for (s = 0u; s < YT2_CONTAINER_SLOTS; s++) {
        if (character->stage[s].id != 0u) {
            if (flags != NULL) {
                *flags = 0x400u >> s;
            }
            return &character->stage[s].cells[id - 1u];
        }
    }
    return &character->pack[id - 1u];
}

/* fn_1DC1A: whether the def/row pair carries readable text
   (0xC000 rows with text bit 0x100, 0x800 rows with 0x40).
   B1's rows always exist in these arms (fn_02262
   materializes them), so a missing row reads silent. */
static int doll_item_readable(const uint8_t *def, const uint8_t *row)
{
    uint16_t flags = doll_def_flag(def);

    if ((flags & 0xC000u) != 0u) {
        return row != NULL && (doll_read16(row + 2u) & 0x100u) != 0u;
    }
    if ((flags & 0x800u) != 0u) {
        return row != NULL && (doll_read16(row + 2u) & 0x40u) != 0u;
    }
    return 0;
}

void yt2_paperdoll_wear_clear(yt2_character *character, unsigned int cell)
{
    if (cell == 10u) {
        character->wear[0] = 0u;
    } else if (cell == 12u) {
        character->wear[1] = 0u;
    } else if (cell == 13u) {
        character->wear[2] = 0u;
    }
}

void yt2_paperdoll_recompute(yt2_paperdoll_ctx *ctx, yt2_character *character)
{
    const uint8_t *def;
    const uint8_t *row;

    character->current.combat.projectile_accuracy = character->base[0];
    character->current.combat.projectile_damage = character->base[1];
    character->current.combat.battle_accuracy = character->base[2];
    character->current.combat.battle_damage = character->base[3];
    character->current.combat.absorption = character->base[4];
    character->maximum.combat.projectile_accuracy = character->max_base[0];
    character->maximum.combat.projectile_damage = character->max_base[1];
    character->maximum.combat.battle_accuracy = character->max_base[2];
    character->maximum.combat.battle_damage = character->max_base[3];
    character->maximum.combat.absorption = character->max_base[4];
    if (character->equipment[0].id != 0u &&
        doll_item(ctx, character->equipment[0].id, &def,
                  &row) == YT2_STATUS_OK &&
        row != NULL) {
        character->current.combat.projectile_accuracy +=
            character->current.skills.projectile;
        character->maximum.combat.projectile_accuracy +=
            character->maximum.skills.projectile;
        character->current.combat.projectile_damage += doll_read16(row + 0);
        character->maximum.combat.projectile_damage += doll_read16(row + 0);
    }
    character->flags &= (uint16_t)~0x20u;
    if (character->equipment[2].id != 0u &&
        doll_item(ctx, character->equipment[2].id, &def,
                  &row) == YT2_STATUS_OK &&
        row != NULL) {
        uint16_t sub = doll_read16(row + 2);

        if ((sub & 0x4000u) != 0u) {
            character->current.combat.battle_accuracy +=
                character->current.skills.slashing;
            character->maximum.combat.battle_accuracy +=
                character->maximum.skills.slashing;
        } else if ((sub & 0x2000u) != 0u) {
            character->current.combat.battle_accuracy +=
                character->current.skills.bashing;
            character->maximum.combat.battle_accuracy +=
                character->maximum.skills.bashing;
        } else if ((sub & 0x1000u) != 0u) {
            character->current.combat.battle_accuracy +=
                character->current.skills.polearm;
            character->maximum.combat.battle_accuracy +=
                character->maximum.skills.polearm;
        }
        character->current.combat.battle_damage += doll_read16(row + 0);
        character->maximum.combat.battle_damage += doll_read16(row + 0);
        if ((sub & 1u) != 0u) {
            character->flags |= 0x20u;
        }
    }
    for (unsigned int i = 3u; i < YT2_EQUIPMENT_SLOTS; i++) {
        if (character->equipment[i].id != 0u &&
            doll_item(ctx, character->equipment[i].id, &def,
                      &row) == YT2_STATUS_OK &&
            row != NULL) {
            character->current.combat.absorption += doll_read16(row + 0);
            character->maximum.combat.absorption += doll_read16(row + 0);
        }
    }
    for (unsigned int i = 0u; i < YT2_EFFECT_SLOTS; i++) {
        if (character->effects[i] != 0u &&
            doll_item(ctx, character->effects[i], &def,
                      &row) == YT2_STATUS_OK &&
            row != NULL) {
            character->current.combat.absorption += doll_read16(row + 0);
            character->maximum.combat.absorption += doll_read16(row + 0);
        }
    }
}

/*
 * fn_1672C: left-press on a member paperdoll. Empty cursor over a full
 * slot picks up (silent); a full cursor drops into empty slots,
 * swaps with full ones and equips with stat updates (SFX 6);
 * refusals buzz (SFX 3). Container nests ride with containers.
 * Returns YT2_PAPERDOLL_CHANGED (B1 v_50CC 0) or YT2_PAPERDOLL_MISS
 * (v_50CC 1). The caller repaints its pane (B1 fn_1658F).
 */
int yt2_paperdoll_left(yt2_paperdoll_ctx *ctx, yt2_character *character,
                  unsigned int slot, unsigned int id)
{
    uint16_t item;
    uint16_t count;
    const uint8_t *def;
    const uint8_t *row;

    if (ctx == NULL || character == NULL || id == 0u || id == 9u ||
        id > 21u) {
        return YT2_PAPERDOLL_MISS;
    }
    if (ctx->cursor == 0u) {
        /* Empty cursor: take full slots (B1 sub_1705F). */
        unsigned int rflags = 0x80u;
        yt2_item *cellslot = NULL;
        uint16_t carried_weight;

        if (id == 11u &&
            (character->flags & PAPERDOLL_STAGE_CELL11) != 0u) {
            /* The open cell-11 bag stays put (B1 loc_1677E). */
            doll_sfx(ctx, 3u);
            return YT2_PAPERDOLL_MISS;
        }
        if (id <= 8u) {
            cellslot = doll_stage_slot(character, id, &rflags);
            item = cellslot == NULL ? 0u : cellslot->id;
            count = cellslot == NULL ? 0u : cellslot->count;
            if (item == 0u) {
                return YT2_PAPERDOLL_MISS;
            }
            cellslot->id = 0u;
            cellslot->count = 0u;
        } else if (id <= 15u) {
            item = character->equipment[id - 10u].id;
            count = character->equipment[id - 10u].count;
            if (item == 0u) {
                return YT2_PAPERDOLL_MISS;
            }
            character->equipment[id - 10u].id = 0u;
            character->equipment[id - 10u].count = 0u;
        } else {
            /* Cell 21 aliases cell 20's word (B1 loc_08983). */
            unsigned int effect = id > 20u ? 4u : id - 16u;

            item = character->effects[effect];
            count = ctx->carried_count;
            if (item == 0u) {
                return YT2_PAPERDOLL_MISS;
            }
            character->effects[effect] = 0u;
            rflags = 0x40u;
        }
        if (doll_item(ctx, item, &def, &row) != YT2_STATUS_OK) {
            return YT2_PAPERDOLL_MISS;
        }
        ctx->cursor = doll_def_icon(def);
        ctx->carried = item;
        ctx->carried_count = count;
        carried_weight = doll_def_weight(def);
        if ((doll_def_flag(def) & 0x2000u) != 0u) {
            /* Bags carry their contents weight along (the
               file store's head word). */
            const uint8_t *store = doll_store_at(ctx, count);

            if (store != NULL) {
                carried_weight = (uint16_t)(carried_weight +
                                            doll_read16(store));
            }
        }
        ctx->carried_weight = carried_weight;
        if ((id >= 10u && id <= 15u && id != 11u) || id >= 16u) {
            /* Unequip: weight off, bonuses off, combat reset. */
            character->carried_weight = (uint16_t)(
                character->carried_weight - carried_weight);
            ctx->bonus_apply(ctx->bonus_ctx, slot, item, 0);
            yt2_paperdoll_recompute(ctx, character);
        } else if (rflags == 0x80u && item == 0x11u) {
            /* Magic takes weigh nothing (B1 loc_170DB). */
        } else {
            doll_cascade_sub(character, rflags, carried_weight);
        }
        yt2_paperdoll_wear_clear(character, id);
        return YT2_PAPERDOLL_CHANGED;
    }
    {
        uint16_t target;
        uint16_t target_count;
        uint16_t carried = ctx->carried;
        uint16_t carried_count = ctx->carried_count;
        uint16_t carried_weight = ctx->carried_weight;
        const uint8_t *carried_def;
        const uint8_t *carried_row;
        unsigned int rflags = 0x80u;
        yt2_item *cellslot = NULL;

        /* Full cursor: compat, then weight, then drop or swap. */
        if (!doll_drop_compat(ctx, character, id, carried)) {
            doll_sfx(ctx, 3u);
            return YT2_PAPERDOLL_MISS;
        }
        if (doll_item(ctx, carried, &carried_def,
                      &carried_row) != YT2_STATUS_OK) {
            return YT2_PAPERDOLL_MISS;
        }
        if ((doll_def_flag(carried_def) & 0x2000u) != 0u &&
            carried_count == 0u) {
            /* Fresh bags mint a store on drop (B1 loc_167C2). */
            carried_count = (uint16_t)doll_store_mint(ctx);
            ctx->carried_count = carried_count;
        }
        if (id <= 8u) {
            cellslot = doll_stage_slot(character, id, &rflags);
            target = cellslot == NULL ? 0u : cellslot->id;
            target_count = cellslot == NULL ? 0u : cellslot->count;
        } else if (id <= 15u) {
            cellslot = &character->equipment[id - 10u];
            target = cellslot->id;
            target_count = cellslot->count;
        } else {
            target = character->effects[id > 20u ? 4u : id - 16u];
            target_count = 0u;
            rflags = 0x40u;
        }
        if (target == 0x2Fu) {
            /* Keys 0x21-0x2E consume into item 47's ring
               (B1 fn_1C09C, v_9471 bits, SFX 4); anything else
               is silent. The consume needs the keyring word
               (unmapped); the silent miss is exact. */
            return YT2_PAPERDOLL_MISS;
        }
        {
            const uint8_t *target_def;
            const uint8_t *target_row;

            if (target != 0u &&
                doll_item(ctx, target, &target_def,
                          &target_row) == YT2_STATUS_OK &&
                (doll_def_flag(target_def) & 0x2000u) != 0u) {
                /* Drop onto a bag (B1 loc_1683B): insert into
                   its file store (first empty cell), name-silent.
                   Reads the file (stale when the bag is open with
                   unsaved RAM edits: B1 clobbers on close, kept). */
                uint8_t store[YT2_SAVE_STORE_SIZE];
                const uint8_t *file;
                unsigned int c;

                if ((uint16_t)(character->carried_weight +
                               carried_weight) >
                    character->current.combat.carry_capacity) {
                    doll_sfx(ctx, 3u);
                    return YT2_PAPERDOLL_MISS;
                }
                if (id == 11u &&
                    (character->flags & PAPERDOLL_STAGE_CELL11) != 0u) {
                    return YT2_PAPERDOLL_MISS;
                }
                if ((doll_read16(carried_def + 14u) &
                     (uint16_t)doll_bag_mask(
                         doll_def_flag(target_def))) == 0u) {
                    doll_sfx(ctx, 3u);
                    return YT2_PAPERDOLL_MISS;
                }
                file = doll_store_at(ctx, target_count);
                if (file == NULL) {
                    memset(store, 0, sizeof(store));
                } else {
                    memcpy(store, file, sizeof(store));
                }
                c = 0u;
                while (c < YT2_CONTAINER_CELLS &&
                       doll_read16(store + 2u + c * 4u) != 0u) {
                    c++;
                }
                if (c >= YT2_CONTAINER_CELLS) {
                    doll_sfx(ctx, 3u);
                    return YT2_PAPERDOLL_MISS;
                }
                if (target == 0x11u) {
                    /* Magic absorbs weightlessly (B1 loc_168B9). */
                    character->carried_weight = (uint16_t)(
                        character->carried_weight - carried_weight);
                    doll_write16(
                        store, (uint16_t)(doll_read16(store) -
                                          carried_weight));
                }
                doll_write16(store + 2u + c * 4u, carried);
                doll_write16(store + 4u + c * 4u, carried_count);
                doll_write16(
                    store, (uint16_t)(doll_read16(store) +
                                      carried_weight));
                {
                    uint8_t *w = doll_store_at(ctx, target_count);

                    if (w != NULL) {
                        memcpy(w, store, sizeof(store));
                    }
                }
                if ((id >= 10u && id <= 15u && id != 11u) ||
                    id >= 16u) {
                    /* Equipped-bag inserts re-apply the placed
                       item's bonuses (B1 sub_16BE4 verbatim);
                       the cascade is skipped here. */
                    character->carried_weight = (uint16_t)(
                        character->carried_weight + carried_weight);
                    ctx->bonus_apply(ctx->bonus_ctx, slot, carried, 1);
                    yt2_paperdoll_recompute(ctx, character);
                } else if (rflags == 0x80u && carried == 0x11u) {
                    /* Magic into pack weighs nothing. */
                } else {
                    doll_cascade_add(character, rflags,
                                     carried_weight);
                }
                ctx->cursor = 0u;
                ctx->carried = 0u;
                ctx->carried_count = 0u;
                ctx->carried_weight = 0u;
                doll_sfx(ctx, 6u);
                return YT2_PAPERDOLL_CHANGED;
            }
        }
        if (target == 0u) {
            if ((uint16_t)(character->carried_weight +
                           carried_weight) >
                character->current.combat.carry_capacity) {
                doll_sfx(ctx, 3u);
                return YT2_PAPERDOLL_MISS;
            }
            if (!doll_staged_fit(ctx, character, id)) {
                return YT2_PAPERDOLL_MISS;
            }
            if (id <= 8u && cellslot != NULL) {
                cellslot->id = carried;
                cellslot->count = carried_count;
            } else if (id <= 15u) {
                character->equipment[id - 10u].id = carried;
                character->equipment[id - 10u].count = carried_count;
            } else {
                character->effects[id > 20u ? 4u : id - 16u] = carried;
            }
            if ((id >= 10u && id <= 15u && id != 11u) || id >= 16u) {
                character->carried_weight = (uint16_t)(
                    character->carried_weight + carried_weight);
                ctx->bonus_apply(ctx->bonus_ctx, slot, carried, 1);
                yt2_paperdoll_recompute(ctx, character);
            } else if (rflags == 0x80u && carried == 0x11u) {
                /* Magic drops weigh nothing (B1 loc_16C38). */
            } else {
                doll_cascade_add(character, rflags, carried_weight);
            }
            ctx->cursor = 0u;
            ctx->carried = 0u;
            ctx->carried_count = 0u;
            ctx->carried_weight = 0u;
            doll_sfx(ctx, 6u);
        } else {
            const uint8_t *target_def;
            const uint8_t *target_row;
            uint16_t target_weight;

            if (doll_item(ctx, target, &target_def, &target_row) !=
                YT2_STATUS_OK) {
                return YT2_PAPERDOLL_MISS;
            }
            /* Swap targets are never bags (those insert above),
               so the target weight is def-only. */
            target_weight = doll_def_weight(target_def);
            /* Equal weights skip the capacity check (B1 loc_1680C). */
            if (carried_weight != target_weight) {
                /* B1 wraps u16 here (no floor): light swaps onto
                   heavy targets can buzz at zero carried weight. */
                unsigned int total = (unsigned int)(uint16_t)(
                    (unsigned int)character->carried_weight +
                    (unsigned int)carried_weight -
                    (unsigned int)target_weight);

                if (total >
                    (unsigned int)character->current.combat.carry_capacity) {
                    doll_sfx(ctx, 3u);
                    return YT2_PAPERDOLL_MISS;
                }
            }
            if (!doll_staged_fit(ctx, character, id)) {
                return YT2_PAPERDOLL_MISS;
            }
            /* Swap = take target + drop carried (B1 sub_16DB0):
               weight cascades both ways, bonuses around. */
            if ((id >= 10u && id <= 15u && id != 11u) || id >= 16u) {
                character->carried_weight = (uint16_t)(
                    character->carried_weight - target_weight);
                ctx->bonus_apply(ctx->bonus_ctx, slot, target, 0);
                yt2_paperdoll_recompute(ctx, character);
            } else if (rflags == 0x80u && target == 0x11u) {
                /* Magic takes weigh nothing. */
            } else {
                doll_cascade_sub(character, rflags, target_weight);
            }
            if (id <= 8u && cellslot != NULL) {
                cellslot->id = carried;
                cellslot->count = carried_count;
            } else if (id <= 15u) {
                character->equipment[id - 10u].id = carried;
                character->equipment[id - 10u].count = carried_count;
            } else {
                character->effects[id > 20u ? 4u : id - 16u] = carried;
            }
            ctx->cursor = doll_def_icon(target_def);
            ctx->carried = target;
            ctx->carried_count = target_count;
            ctx->carried_weight = target_weight;
            if ((id >= 10u && id <= 15u && id != 11u) || id >= 16u) {
                character->carried_weight = (uint16_t)(
                    character->carried_weight + carried_weight);
                ctx->bonus_apply(ctx->bonus_ctx, slot, carried, 1);
                yt2_paperdoll_recompute(ctx, character);
            } else if (rflags == 0x80u && carried == 0x11u) {
                /* Magic drops weigh nothing. */
            } else {
                doll_cascade_add(character, rflags, carried_weight);
            }
            doll_sfx(ctx, 6u);
        }
        return YT2_PAPERDOLL_CHANGED;
    }
}

/*
 * fn_16925: right-press on a member paperdoll. B1's tree, in order:
 * cell 9 closes the staged pack bag (LIFO flush); empty slots
 * are silent; flag 0x2000 (bag) presses stage (pack) or toggle
 * (cell 11), even with a full cursor; other full-cursor presses
 * are silent; flag-0x100 and fn_1DC1A-readable items report
 * YT2_PAPERDOLL_EXAMINED (B1 sets the v_5110 context, the caller
 * presents); plain gear too (50CC 2, not even a name in
 * vanilla). The 19B42 use-modal arms are a deferred slice,
 * silent in vanilla; enhanced callers paint their own receipt
 * for examined hits.
 */
int yt2_paperdoll_right(yt2_paperdoll_ctx *ctx, yt2_character *character,
                   unsigned int slot, unsigned int id,
                   unsigned int *examined_id)
{
    uint16_t item;
    const uint8_t *def;
    const uint8_t *row;

    (void)slot;
    if (examined_id != NULL) {
        *examined_id = 0u;
    }
    if (ctx == NULL || character == NULL || id == 0u || id > 21u) {
        return YT2_PAPERDOLL_MISS;
    }
    if (id == 9u) {
        /* Container close (B1 loc_169C3): flush the topmost
           pack-staged bag (LIFO by nonzero id), clear its
           occupied bit. Silent with none staged (cell-11 bags
           close by re-press). */
        unsigned int s = 0u;

        if ((character->flags & PAPERDOLL_STAGE_PACK) == 0u) {
            return YT2_PAPERDOLL_MISS;
        }
        while (s < YT2_CONTAINER_SLOTS &&
               character->stage[s].id == 0u) {
            s++;
        }
        if (s >= YT2_CONTAINER_SLOTS) {
            /* Desync fallback (B1 loc_169FA clears 0x2000). */
            character->flags =
                (uint16_t)(character->flags & ~0x2000u);
        } else {
            yt2_paperdoll_store_flush(ctx, &character->stage[s]);
            character->flags = (uint16_t)(
                character->flags & ~(0x8000u >> s));
        }
        return YT2_PAPERDOLL_CHANGED;
    }
    {
        yt2_item *cell = NULL;

        if (id >= 1u && id <= 8u) {
            /* Staged redirect (B1 sub_16E64): nested bags
               stage deeper. */
            cell = doll_stage_slot(character, id, NULL);
            item = cell == NULL ? 0u : cell->id;
        } else if (id >= 10u && id <= 15u) {
            cell = &character->equipment[id - 10u];
            item = cell->id;
        } else if (id >= 16u) {
            item = character->effects[id > 20u ? 4u : id - 16u];
        } else {
            return YT2_PAPERDOLL_MISS;
        }
        if (item == 0u ||
            doll_item(ctx, item, &def, &row) != YT2_STATUS_OK) {
            return YT2_PAPERDOLL_MISS;
        }
        if ((doll_def_flag(def) & 0x2000u) != 0u) {
            /* Container open (B1 loc_16996): cell 11 toggles
               (close-all then stage, or close-all), pack bags
               stage into the last free slot (buzz when three
               are open). The bag stays in its cell (hidden by
               the redirect); contents load from its store. */
            if (id == 11u) {
                for (unsigned int s = 0u; s < YT2_CONTAINER_SLOTS;
                     s++) {
                    yt2_paperdoll_store_flush(ctx, &character->stage[s]);
                }
                if ((character->flags & PAPERDOLL_STAGE_CELL11) != 0u) {
                    character->flags = (uint16_t)(
                        character->flags & ~0xF000u);
                } else {
                    yt2_container_stage *stage =
                        &character->stage[2];
                    uint16_t count =
                        character->equipment[1].count;

                    character->flags = (uint16_t)(
                        (character->flags & ~0xE000u) |
                        PAPERDOLL_STAGE_CELL11);
                    stage->id = item;
                    stage->count = count;
                    doll_store_read(ctx, count, stage);
                }
            } else {
                static const unsigned int bit[YT2_CONTAINER_SLOTS] = {
                    0x8000u, 0x4000u, 0x2000u,
                };
                unsigned int s = YT2_CONTAINER_SLOTS;

                if (character->stage[0].id != 0u) {
                    doll_sfx(ctx, 3u);
                    return YT2_PAPERDOLL_MISS;
                }
                if (character->stage[1].id != 0u) {
                    s = 0u;
                } else if (character->stage[2].id != 0u) {
                    s = 1u;
                } else {
                    s = 2u;
                }
                character->stage[s].id = item;
                character->stage[s].count =
                    cell == NULL ? 0u : cell->count;
                doll_store_read(ctx, character->stage[s].count,
                                &character->stage[s]);
                character->flags =
                    (uint16_t)(character->flags | bit[s]);
            }
            return YT2_PAPERDOLL_CHANGED;
        }
    }
    if (ctx->cursor != 0u) {
        return YT2_PAPERDOLL_MISS;
    }
    if ((doll_def_flag(def) & 0x100u) != 0u ||
        doll_item_readable(def, row)) {
        /* fn_19B42 use modal (potions, scrolls, lore gear):
           B1 sets the examine context (50CC 2); the caller
           paints nothing in vanilla. */
        if (examined_id != NULL) {
            *examined_id = item;
        }
        return YT2_PAPERDOLL_EXAMINED;
    }
    /* Plain gear: B1 sets the context too (50CC 2), not even a
       name in vanilla. */
    if (examined_id != NULL) {
        *examined_id = item;
    }
    return YT2_PAPERDOLL_EXAMINED;
}
