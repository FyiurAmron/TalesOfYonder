#include "yt2/combat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt2/boot.h"
#include "yt2/classes.h"
#include "yt2/create.h"
#include "yt2/cursor.h"
#include "yt2/item_detail.h"
#include "yt2/mapview.h"
#include "yt2/text.h"

/* Init-block constants (sub_02064 neighborhood): SFX ids, panel
   colors, and the missile-id window for the portrait tint. */
#define COMBAT_SFX_ROUND 29u
#define COMBAT_SFX_HIT 13u
#define COMBAT_SFX_MISS 30u
#define COMBAT_SFX_GLANCE 6u
#define COMBAT_SFX_VICTORY 7u
#define COMBAT_MISSILE_LO 542u
#define COMBAT_MISSILE_HI 547u

/* Staging occupied flags (+0x15C): 0x1000 cell-11 bag, 0x2000 /
   0x4000 / 0x8000 pack bags (slots 1C8 / 1A2 / 17C). */
#define COMBAT_STAGE_CELL11 0x1000u
#define COMBAT_STAGE_PACK \
    ((unsigned int)(0x2000u | 0x4000u | 0x8000u))

/* fn_0AD9D weapon kinds (equipment slot, wear counter, wear cap). */
#define COMBAT_WEAR_MISSILE 0u
#define COMBAT_WEAR_MELEE 1u
#define COMBAT_WEAR_SPELL 2u
#define COMBAT_WEAR_CAP_MISSILE 0x78u
#define COMBAT_WEAR_CAP_MELEE 0x50u
#define COMBAT_WEAR_CAP_SPELL 0x14u

/*
 * fn_0D620 range bands: the missile is drawn at 0x31 first, then the
 * 0x2E/0x2B/0x28/0x24/0x19 columns are drawn and checked far to near.
 */
static const unsigned int combat_bands[] = {0x2Eu, 0x2Bu, 0x28u, 0x24u,
                                            0x19u};

/* fn_0B4EE level thresholds (DGROUP:0x92C7, BCD dwords). */
static const uint8_t combat_xp_table[][4] = {
    {0x00u, 0x00u, 0x06u, 0x80u}, {0x00u, 0x00u, 0x18u, 0x50u},
    {0x00u, 0x00u, 0x26u, 0x00u}, {0x00u, 0x00u, 0x55u, 0x00u},
    {0x00u, 0x01u, 0x00u, 0x00u}, {0x00u, 0x01u, 0x70u, 0x00u},
    {0x00u, 0x02u, 0x60u, 0x00u}, {0x00u, 0x04u, 0x95u, 0x00u},
    {0x00u, 0x07u, 0x97u, 0x00u}, {0x00u, 0x12u, 0x50u, 0x00u},
};

/* Damage/effect records (DGROUP 0x90AB, 50 x 12 bytes, static init
   data, identical boot to post-game): +0 SFX, +2 portrait pic,
   +4/+6 damage range, +8/+0x0A dispatch flags. Event ids
   (v_555E - 50) address the whole table; monsters and missiles
   use 2/15/24/25/44/26/27. Bytes 45-49 read as-is like B1. */
static const uint8_t combat_effects[50][12] = {
    {0x28u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u},
    {0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x02u},
    {0x0Du, 0x00u, 0x02u, 0x00u, 0x01u, 0x00u, 0x0Au, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Eu, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u},
    {0x09u, 0x00u, 0x03u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x09u, 0x00u, 0x03u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x40u, 0x00u, 0x10u},
    {0x09u, 0x00u, 0x04u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x09u, 0x00u, 0x04u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x20u, 0x00u, 0x10u},
    {0x0Au, 0x00u, 0x07u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Au, 0x00u, 0x07u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x08u, 0x00u, 0x10u},
    {0x0Bu, 0x00u, 0x08u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Bu, 0x00u, 0x08u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x88u, 0x00u, 0x00u, 0x10u},
    {0x08u, 0x00u, 0x05u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Cu, 0x00u, 0x06u, 0x00u, 0x01u, 0x00u, 0x05u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x3Au, 0x00u, 0xE6u, 0x00u, 0x01u, 0x00u, 0x0Fu, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u},
    {0x07u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u},
    {0x00u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u},
    {0x00u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u},
    {0x0Eu, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x41u},
    {0x0Eu, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x40u},
    {0x0Du, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x40u},
    {0x0Du, 0x00u, 0xE1u, 0x00u, 0x01u, 0x00u, 0x09u, 0x00u, 0x10u, 0x40u, 0x00u, 0x10u},
    {0x28u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u},
    {0x3Au, 0x00u, 0xE6u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u},
    {0x0Du, 0x00u, 0xE1u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x80u, 0x00u, 0x10u},
    {0x0Du, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x20u, 0x02u, 0x00u, 0x10u},
    {0x3Eu, 0x00u, 0xE5u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x3Du, 0x00u, 0xE3u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x3Au, 0x00u, 0xE6u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Du, 0x00u, 0xE2u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x20u, 0x00u, 0x10u},
    {0x0Du, 0x00u, 0xE2u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0xE0u, 0x00u, 0x10u},
    {0x0Du, 0x00u, 0xE1u, 0x00u, 0x01u, 0x00u, 0x04u, 0x00u, 0x10u, 0x80u, 0x00u, 0x10u},
    {0x0Cu, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x3Fu, 0x00u, 0xE4u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Au, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Bu, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x3Au, 0x00u, 0xE6u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x10u, 0x00u, 0x10u},
    {0x3Du, 0x00u, 0xE3u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x20u, 0x01u, 0x00u, 0x10u},
    {0x08u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u},
    {0x0Du, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x04u, 0x00u, 0x10u},
    {0x3Fu, 0x00u, 0xE4u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x08u, 0x00u, 0x10u},
    {0x0Bu, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x88u, 0x00u, 0x00u, 0x10u},
    {0x3Eu, 0x00u, 0xE5u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x20u, 0x00u, 0x00u, 0x00u},
    {0x12u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u},
    {0x0Du, 0x00u, 0xE1u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x40u, 0x00u, 0x10u},
    {0x00u, 0x00u, 0x06u, 0x80u, 0x00u, 0x00u, 0x18u, 0x50u, 0x00u, 0x00u, 0x26u, 0x00u},
    {0x00u, 0x00u, 0x55u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x70u, 0x00u},
    {0x00u, 0x02u, 0x60u, 0x00u, 0x00u, 0x04u, 0x95u, 0x00u, 0x00u, 0x07u, 0x97u, 0x00u},
    {0x00u, 0x12u, 0x50u, 0x00u, 0x00u, 0x21u, 0x90u, 0x00u, 0x00u, 0x34u, 0x00u, 0x00u},
    {0x00u, 0x46u, 0x00u, 0x00u, 0x00u, 0x66u, 0x00u, 0x00u, 0x00u, 0x80u, 0x00u, 0x00u},
};

/* Award side panel texts (DGROUP strings at fn_1345B's call sites). */
#define COMBAT_TEXT_GOLD "GOLD COINS:"
#define COMBAT_TEXT_ORE "MAGIC ORE: "
#define COMBAT_TEXT_NUORE "NUORE: "
#define COMBAT_TEXT_XP "EXPERIENCE:"

static void combat_dot_death(void *context, yt2_room_object *obj);
static void combat_monster_turn(yt2_combat *combat, unsigned int engaged,
                                yt2_room_present_fn present, void *context);
static void combat_build_queue(yt2_combat *combat);
static void combat_paint_highlight(yt2_combat *combat);
static void combat_delay(yt2_combat *combat, unsigned int ticks);
static void combat_rederive(yt2_character *member);
static void combat_recompute(yt2_combat *combat, yt2_character *member);
static void combat_write16(uint8_t *bytes, uint16_t value);
static void combat_apply_damage(yt2_combat *combat,
                                yt2_room_present_fn present, void *context);
static void combat_check_wipe(yt2_combat *combat,
                              yt2_room_present_fn present, void *context);
static unsigned int combat_item_die(yt2_combat *combat, unsigned int id,
                                    unsigned int *cls);
static void chest_bcd_sub(uint8_t *dst, const uint8_t *src);
static unsigned int chest_skill_roll(yt2_combat *combat, unsigned int level,
                                     unsigned int difficulty,
                                     unsigned int skill);
static void chest_fill(yt2_combat *combat, int x, int y, int w, int h);
static void chest_paint_loot(yt2_combat *combat);
static void quick_access_paint(yt2_combat *combat);
static void combat_paint_icon(yt2_combat *combat, unsigned int id);
static void combat_paint_swords(yt2_combat *combat);
static void chest_side_panel_clear(yt2_combat *combat, int compass);
static void combat_panel_save_modal(yt2_combat *combat);
static void combat_panel_restore_modal(yt2_combat *combat);
static void combat_panel_save_formal(yt2_combat *combat);
static void combat_panel_restore_formal(yt2_combat *combat);
static void chest_side_panel_paint(yt2_combat *combat);
static void quick_access_click(yt2_combat *combat, unsigned int cell);
static void chest_portrait_drop(yt2_combat *combat, unsigned int position);
static void pack_paint_member(yt2_combat *combat, unsigned int member);
static void combat_paperdoll_bonus(void *bonus_ctx, unsigned int slot,
                              unsigned int id, int plus);
static void pack_backdrop_snap(yt2_combat *combat);
static void pack_backdrop_blit(yt2_combat *combat, int x0, int y0,
                               unsigned int w, unsigned int h);

int yt2_combat_init(yt2_combat *combat, yt2_room *room, yt2_save *save,
                    const yt2_pictures *pictures, const yt2_world *world,
                    const yt2_items *items, yt2_surface *surface)
{
    unsigned int i;
    unsigned int award;

    if (combat == NULL || room == NULL || save == NULL ||
        pictures == NULL || world == NULL || items == NULL ||
        surface == NULL) {
        return YT2_STATUS_ERROR;
    }
    combat->room = room;
    combat->save = save;
    combat->pictures = pictures;
    combat->world = world;
    combat->items = items;
    combat->surface = surface;
    combat->sfx_count = 0u;
    for (i = 0u; i < YT2_COMBAT_SFX_LOG; i++) {
        combat->sfx[i] = 0u;
    }
    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        combat->damage[i] = 0u;
    }
    combat->members_left = 0u;
    combat->status_bits = 0u;
    combat->damage_roll = 0u;
    combat->resist_bits = 0u;
    for (i = 0u; i < YT2_COMBAT_QUEUE_MAX; i++) {
        combat->queue[i].monster = 0;
        combat->queue[i].index = 0u;
        combat->queue[i].initiative = 0u;
        combat->queue[i].flags = 0u;
    }
    combat->queue_count = 0u;
    combat->queue_pos = 0u;
    combat->current = 0u;
    combat->attacker = 0u;
    combat->target = -1;
    combat->panel_saved_modal = 0u;
    combat->panel_saved_modal_valid = 0;
    combat->panel_saved_formal = 0u;
    combat->panel_saved_formal_valid = 0;
    for (award = 0u; award < 4u; award++) {
        combat->award[award][0] = 0u;
        combat->award[award][1] = 0u;
        combat->award[award][2] = 0u;
        combat->award[award][3] = 0u;
        combat->last_award[award][0] = 0u;
        combat->last_award[award][1] = 0u;
        combat->last_award[award][2] = 0u;
        combat->last_award[award][3] = 0u;
    }
    combat->award_pending = 0;
    combat->menu_requested = 0;
    combat->member_page = 0;
    combat->sheet_open = 0u;
    combat->pack_open = 0u;
    combat->pack_current = 0u;
    combat->panel_saved_pack = 0u;
    combat->panel_saved_pack_valid = 0;
    combat->pack_backdrop_valid = 0;
    {
        uint8_t *store_base = NULL;
        size_t store_size = 0u;

        if (save->bytes != NULL && save->size > YT2_SAVE_STORE_OFFSET) {
            store_base = save->bytes + YT2_SAVE_STORE_OFFSET;
            store_size = save->size - YT2_SAVE_STORE_OFFSET;
        }
        yt2_paperdoll_init(&combat->paperdoll, combat->items, store_base,
                      store_size, &save->store_bump, &save->store_free,
                      combat->sfx, &combat->sfx_count,
                      YT2_COMBAT_SFX_LOG, combat_paperdoll_bonus, combat);
    }
    combat->menu_formal = 0;
    combat->chest_open = 0;
    combat->chest_prompt = 0;
    combat->chest_index = 0u;
    for (i = 0u; i < YT2_CHEST_RECORD_SIZE; i++) {
        combat->chest_record[i] = 0u;
    }
    combat->chest_taken = 0u;
    combat->chest_opener = 0u;
    for (i = 0u; i < YT2_CHEST_SLOTS; i++) {
        combat->chest_staging[i].id = 0u;
        combat->chest_staging[i].count = 0u;
    }
    for (i = 0u; i < YT2_QUICK_ACCESS_SLOTS; i++) {
        combat->quick_access[i].id = 0u;
        combat->quick_access[i].count = 0u;
    }
    combat->chest_cursor = 0u;
    combat->chest_carried = 0u;
    combat->chest_carried_weight = 0u;
    combat->chest_carried_count = 0u;
    combat->chest_side_panel_lines = 0u;
    combat->chest_side_panel_compass = 0u;
    combat->chest_compass_fresh = 0u;
    combat->chest_side_panel_sticky = 0u;
    combat->chest_paint_stale = 0u;
    combat->chest_messages = 0u;
    combat->chest_v5560 = 0u;
    combat->button_pressed = 0u;
    for (i = 0u; i < YT2_COMBAT_DAMAGE_SLOTS; i++) {
        combat->damage_slots[i].effect = 0u;
        combat->damage_slots[i].member = -1;
        combat->damage_slots[i].status = 0u;
        combat->damage_slots[i].amount = 0u;
        combat->damage_slots[i].aux = 0u;
        combat->damage_slots[i].staged = 0;
    }
    combat->death_pending = 0;
    combat->save_power = 0u;
    combat->effect_id = 0u;
    combat->status_mask_any = 0u;
    combat->status_mask = 0u;
    for (i = 0u; i < 4u; i++) {
        combat->chest_effect_damage[i] = 0u;
    }
    combat->chest_instance_next = 0u;
    combat->chest_save_index = 0u;
    room->death_cb = combat_dot_death;
    room->death_ctx = combat;
    return YT2_STATUS_OK;
}

/* sub_0DBAA: the gated SFX player. No driver runs here, so the
   SoundWaitIdle gate always passes and every id is logged. */
static void combat_sfx(yt2_combat *combat, unsigned int id)
{
    if (combat == NULL || id == 0u ||
        combat->sfx_count >= YT2_COMBAT_SFX_LOG) {
        return;
    }
    combat->sfx[combat->sfx_count++] = id;
}

/* sub_0DBAA busy arm: a back-to-back SFX finds the driver busy,
   idles 6 ticks and drops (never plays). The S member loop
   resolves with no time passing, and cross-band gaps (~2 ticks)
   stay under the busy window vanilla's held pose proves (>6
   ticks), so one flag per round models it. */
static void combat_sfx_busy(yt2_combat *combat, unsigned int id,
                            int *played)
{
    if (*played) {
        combat_delay(combat, 6u);
        return;
    }
    combat_sfx(combat, id);
    *played = 1;
}

/* fn_09ADC: 4-byte BCD add with DAA per byte (si += di). */
static void combat_bcd_add(uint8_t *dst, const uint8_t *src)
{
    unsigned int carry = 0u;
    int i;

    for (i = 3; i >= 0; i--) {
        unsigned int left = (unsigned int)(dst[i] >> 4) * 10u +
                            (unsigned int)(dst[i] & 15u);
        unsigned int right = (unsigned int)(src[i] >> 4) * 10u +
                             (unsigned int)(src[i] & 15u);
        unsigned int sum = left + right + carry;

        carry = sum > 99u ? 1u : 0u;
        sum %= 100u;
        dst[i] = (uint8_t)(((sum / 10u) << 4) | (sum % 10u));
    }
    (void)carry;
}

/* fn_09B42 == 0 / fn_09B1C order: nibble-wise BCD compare of 4 bytes.
   Returns -1/0/1 (vanilla's flags: je/ja/jb). */
static int combat_bcd_compare(const uint8_t *a, const uint8_t *b)
{
    int i;

    for (i = 0; i < 4; i++) {
        unsigned int ah = (unsigned int)(a[i] >> 4);
        unsigned int bh = (unsigned int)(b[i] >> 4);
        unsigned int al = (unsigned int)(a[i] & 15u);
        unsigned int bl = (unsigned int)(b[i] & 15u);

        if (ah != bh) {
            return ah < bh ? -1 : 1;
        }
        if (al != bl) {
            return al < bl ? -1 : 1;
        }
    }
    return 0;
}

/* fn_09C46 on the small domain: BCD bytes to decimal with thousands
   commas, leading groups suppressed (yt2_item_format_commas matches
   vanilla here; the strip-spaces tail only matters past 9999). */
static void combat_bcd_format(const uint8_t *bcd, char *buffer, size_t size)
{
    unsigned int value = 0u;
    int i;

    for (i = 0; i < 4; i++) {
        value = value * 100u + (unsigned int)(bcd[i] >> 4) * 10u +
                (unsigned int)(bcd[i] & 15u);
    }
    yt2_item_format_commas(buffer, size, value);
}

static uint16_t combat_read16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void combat_write16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
}

/* fn_16024: 1-based roster slot to character (sets v_5070/72). */
static yt2_character *combat_roster(yt2_combat *combat, unsigned int slot)
{
    yt2_party *party;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL || slot == 0u ||
        slot > YT2_ROSTER_SLOTS) {
        return NULL;
    }
    party = combat->room->party;
    combat->attacker = slot;
    return &party->roster[slot - 1u];
}

/* fn_02262 through the items module: definition record (0xB4A) plus
   the parameter row (0xB2E, NULL without a flag-selected table). */
static int combat_item(yt2_combat *combat, unsigned int id,
                       const uint8_t **def, const uint8_t **row)
{
    const uint8_t *record;

    if (def != NULL) {
        *def = NULL;
    }
    if (row != NULL) {
        *row = NULL;
    }
    if (combat == NULL || id == 0u) {
        return YT2_STATUS_ERROR;
    }
    record = yt2_item_def(combat->items, id);
    if (record == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (def != NULL) {
        *def = record;
    }
    if (row != NULL) {
        *row = yt2_item_param_row(combat->items, record);
    }
    return YT2_STATUS_OK;
}

/* sub_0AF63: missile class of an item id (v_50CC 1/2/3, 0 = none). */
static unsigned int combat_missile_class(yt2_combat *combat, unsigned int id)
{
    const uint8_t *def;
    const uint8_t *row;
    uint16_t flags;
    uint16_t kind;

    if (combat_item(combat, id, &def, &row) != YT2_STATUS_OK ||
        row == NULL) {
        return 0u;
    }
    flags = combat_read16(def + 12);
    kind = combat_read16(row + 2);
    if ((flags & 0xC000u) != 0u) {
        if ((kind & 0x100u) != 0u) {
            return 2u;
        }
        /* 0x200 set falls through with v_50CC still 0. */
        return (kind & 0x200u) != 0u ? 0u : 1u;
    }
    if ((flags & 0x800u) == 0u) {
        return 3u;
    }
    if ((kind & 0x40u) != 0u) {
        return 2u;
    }
    return (kind & 0x80u) != 0u ? 0u : 1u;
}

/* fn_15D89: margin roll plus percentile damage into v_0C36. */
static void combat_scale(yt2_combat *combat, uint16_t defense,
                         uint16_t base, uint16_t mult)
{
    yt2_room *room = combat->room;
    uint16_t margin;
    uint16_t roll;
    uint16_t scaled;

    combat->damage_roll = 0u;
    if (mult == 0u || base < defense) {
        return;
    }
    margin = (uint16_t)(base - defense);
    roll = yt2_create_rand(&room->rng_seed, room->time_word, 0x37u);
    if (margin < roll) {
        return;
    }
    scaled = (uint16_t)(((uint32_t)mult * margin + 50u) / 100u);
    combat->damage_roll = scaled == 0u ? 1u : scaled;
}

/*
 * fn_0AD9D: attacker weapon build. Returns the item id and sets
 * `fires`: nonzero equipment, missile-class check, wear++ under the
 * cap, else the rand-1000 breakage roll (break: counter reset, the
 * attack fumbles with v_50CC 0).
 */
/* sub_0AF12: the breakage fumble. Stages effect 0 over the
   member's damage slot (kind offset as status, item id, die)
   and applies it, then rederives. */
static void combat_fumble(yt2_combat *combat, yt2_character *member,
                          int assigned, unsigned int kind, unsigned int id,
                          yt2_room_present_fn present, void *context)
{
    static const unsigned int kinds[] = {0x13Au, 0x142u, 0x146u};
    yt2_combat_damage *slot;

    if (assigned < 0 || assigned >= (int)YT2_COMBAT_DAMAGE_SLOTS ||
        kind > COMBAT_WEAR_SPELL) {
        return;
    }
    slot = &combat->damage_slots[assigned];
    slot->effect = 0u;
    slot->member = assigned;
    slot->status = (uint16_t)kinds[kind];
    slot->amount = (uint16_t)id;
    slot->aux = (uint16_t)combat_item_die(combat, id, NULL);
    slot->staged = 1;
    combat_apply_damage(combat, present, context);
    combat_rederive(member);
}

/*
 * fn_0AD9D: attacker weapon build. Returns B1's v_50CC code and
 * the item id: 4 when the slot is empty (melee still swings
 * fists; S rounds skip), the missile class (1/2/3, no wear)
 * for classed items, 1 on a fresh weapon, 0 on a fumble or
 * break (the break stages the sub_0AF12 fumble effect and
 * resets the counter). Kinds address +0x13A/+0x142/+0x146:
 * equipment slots 0/2/3; callers needing presents (breakage)
 * pass the callback through, others pass NULL for a dry build.
 */
static unsigned int combat_weapon_cc(yt2_combat *combat,
                                     yt2_character *member, unsigned int kind,
                                     int assigned, unsigned int *id,
                                     yt2_room_present_fn present,
                                     void *context)
{
    /* fn_0AD9D kinds address +0x13A/+0x142/+0x146: slots 0/2/3. */
    static const unsigned int slots[] = {0u, 2u, 3u};
    static const unsigned int caps[] = {COMBAT_WEAR_CAP_MISSILE,
                                        COMBAT_WEAR_CAP_MELEE,
                                        COMBAT_WEAR_CAP_SPELL};
    yt2_room *room = combat->room;
    unsigned int item;
    unsigned int cls;
    const uint8_t *def;
    const uint8_t *row;
    uint16_t threshold;
    uint16_t limit;
    uint16_t roll;

    if (id != NULL) {
        *id = 0u;
    }
    if (member == NULL || kind > COMBAT_WEAR_SPELL) {
        return 4u;
    }
    item = member->equipment[slots[kind]].id;
    if (id != NULL) {
        *id = item;
    }
    if (item == 0u) {
        return 4u;
    }
    cls = combat_missile_class(combat, item);
    if (cls != 0u) {
        return cls;
    }
    member->wear[kind]++;
    if (member->wear[kind] <= caps[kind]) {
        return 1u;
    }
    if (combat_item(combat, item, &def, &row) != YT2_STATUS_OK ||
        row == NULL) {
        return 0u;
    }
    if ((combat_read16(def + 12) & 0xC000u) != 0u) {
        limit = combat_read16(def + 6);
        threshold = combat_read16(row + 6);
    } else {
        limit = combat_read16(def + 10);
        threshold = combat_read16(row + 10);
    }
    if (limit == 0u) {
        return 1u;
    }
    roll = yt2_create_rand(&room->rng_seed, room->time_word, 1000u);
    if ((int16_t)roll > (int16_t)threshold) {
        return 1u;
    }
    /* Break: the fumble effect stages, the counter resets. */
    member->wear[kind] = 0u;
    combat_fumble(combat, member, assigned, kind, item, present, context);
    return 0u;
}

/* sub_0DB4D: apply v_0C36 to the target (status, halving, HP, 0x200). */
static void combat_apply(yt2_combat *combat, yt2_room_object *target)
{
    yt2_room *room = combat->room;
    uint16_t resist;
    uint16_t damage;
    unsigned int bit;

    if (combat->damage_roll == 0u || target == NULL) {
        return;
    }
    target->flags |= (uint16_t)(~target->vuln_status & combat->status_bits);
    resist = (uint16_t)(target->vuln_resist & combat->resist_bits);
    damage = combat->damage_roll;
    for (bit = 0u; bit < 16u; bit++) {
        unsigned int carry = (resist & 0x8000u) != 0u;

        resist = (uint16_t)(resist << 1);
        if (carry) {
            damage = (uint16_t)(damage >> 1);
        }
    }
    if (damage >= target->hp) {
        target->hp = 0u;
    } else {
        target->hp = (uint16_t)(target->hp - damage);
    }
    target->flags |= 3u;
    room->f5062 |= 0x200u;
}

/*
 * sub_0DBC8 member path: the first nonzero damage word resolves
 * against the fn_00F42 target. Returns the assigned index, or -1
 * when no word is set.
 */
static int combat_resolve_member(yt2_combat *combat,
                                 yt2_room_object *target)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    unsigned int i;

    room->f5062 &= (uint16_t)~0x200u;
    combat->status_bits = 0u;
    combat->damage_roll = 0u;
    combat->resist_bits = 0u;
    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        yt2_character *member;
        const uint8_t *row;
        unsigned int slot;

        if (combat->damage[i] == 0u) {
            continue;
        }
        slot = party->assigned[i];
        member = combat_roster(combat, slot);
        if (member == NULL) {
            continue;
        }
        if (combat_item(combat, combat->damage[i], NULL, &row) ==
                YT2_STATUS_OK &&
            row != NULL) {
            if ((combat_read16(row + 2) & 0x400u) != 0u) {
                combat->resist_bits |= 0x1000u;
            }
            if (combat_read16(row + 8) != 0u) {
                combat->resist_bits |= 0x800u;
            }
        }
        combat->resist_bits |= 0x8000u;
        combat_scale(combat, target == NULL ? 0u : target->defense,
                     member->current.combat.projectile_accuracy,
                     member->current.combat.projectile_damage);
        if (target != NULL) {
            combat_apply(combat, target);
        }
        return (int)i;
    }
    return -1;
}

/* sub_0DE2E: consume the first nonzero damage word. */
static void combat_consume(yt2_combat *combat)
{
    unsigned int i;

    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        if (combat->damage[i] != 0u) {
            combat->damage[i] = 0u;
            return;
        }
    }
}

/* fn_12EA0: accumulate the kill loot BCDs into the award buffers. */
static void combat_credit(yt2_combat *combat, const yt2_room_object *target)
{
    if (target == NULL) {
        return;
    }
    combat_bcd_add(combat->award[0], target->loot + 0);
    combat_bcd_add(combat->award[1], target->loot + 4);
    combat_bcd_add(combat->award[2], target->loot + 8);
    combat_bcd_add(combat->award[3], target->loot + 12);
}

/* fn_13420: clear the map cell and zero the record. */
static void combat_clear_cell(yt2_combat *combat, yt2_room_object *target)
{
    yt2_room *room = combat->room;

    if (target == NULL) {
        return;
    }
    if (room != NULL) {
        int wx = (int)target->x - room->origin_x;
        int wy = (int)target->y - room->origin_y;

        if (wx >= 0 && wy >= 0 && wx < (int)YT2_ROOM_WINDOW_SIDE &&
            wy < (int)YT2_ROOM_WINDOW_SIDE) {
            yt2_room_cell *cell = &room->window[wy][wx];

            cell->flags &= (uint16_t)~YT2_ROOM_CELL_OBJECT;
            cell->misc = 0u;
        }
    }
    memset(target, 0, sizeof(*target));
}

/* fn_13447: zero the record. */
static void combat_zero(yt2_room_object *target)
{
    if (target != NULL) {
        memset(target, 0, sizeof(*target));
    }
}

/* fn_13056 damage-over-time death through the room callback. */
static void combat_dot_death(void *context, yt2_room_object *obj)
{
    yt2_combat *combat = (yt2_combat *)context;

    if (combat == NULL) {
        return;
    }
    combat_credit(combat, obj);
    combat_clear_cell(combat, obj);
}

/* fn_0B4EE: ready the next level when XP crosses its threshold. */
static void combat_level_up(yt2_combat *combat, yt2_character *member)
{
    uint8_t xp[4];
    unsigned int level;
    unsigned int count =
        (unsigned int)(sizeof(combat_xp_table) / sizeof(combat_xp_table[0]));

    (void)combat;
    if (member == NULL) {
        return;
    }
    member->ready = 0u;
    if ((member->status & 0x1C40u) != 0u) {
        return;
    }
    /* experience holds the raw little-endian save bytes (BCD). */
    xp[0] = (uint8_t)(member->experience & 0xFFu);
    xp[1] = (uint8_t)((member->experience >> 8) & 0xFFu);
    xp[2] = (uint8_t)((member->experience >> 16) & 0xFFu);
    xp[3] = (uint8_t)((member->experience >> 24) & 0xFFu);
    level = member->level;
    if (level == 0u) {
        return;
    }
    for (;;) {
        unsigned int leg = level - 1u;

        if (leg >= count) {
            break;
        }
        if (combat_bcd_compare(xp, combat_xp_table[leg]) < 0) {
            break;
        }
        level++;
    }
    if (level > member->level) {
        member->ready = (uint16_t)level;
    }
}

/* Missile staging (fn_1C211 + fn_1C10F x4 + fn_1C154): a
   210x105 transparent canvas; firing member i contributes the
   53x105 slice at 0/0x36/0x69/0x9D from its missile pic
   (group 1: 81 inside ids 542-547, else 80; v_5062 bit 8
   takes the id raw for monster missiles). Silent members
   leave transparent slots. Caller frees (NULL when group 1
   is missing, and the flight is skipped). */
static uint8_t *combat_missiles_stage(yt2_combat *combat)
{
    static const unsigned int xs[4] = {0u, 0x36u, 0x69u, 0x9Du};
    yt2_room *room = combat->room;
    const yt2_picture_group *group;
    uint8_t *canvas;
    unsigned int i;

    group = yt2_pictures_group(combat->pictures, 1u);
    if (group == NULL || group->width < 210u || group->height < 105u) {
        return NULL;
    }
    canvas = (uint8_t *)malloc((size_t)210u * 105u);
    if (canvas == NULL) {
        return NULL;
    }
    memset(canvas, 0xFFu, (size_t)210u * 105u);
    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        unsigned int id = combat->damage[i];
        const uint8_t *pic;
        unsigned int picid;
        unsigned int row;

        if (id == 0u) {
            continue;
        }
        if ((room->f5062 & 8u) != 0u) {
            picid = id;
        } else if (id >= COMBAT_MISSILE_LO && id <= COMBAT_MISSILE_HI) {
            picid = 81u;
        } else {
            picid = 80u;
        }
        pic = yt2_picture_data(combat->pictures, 1u, picid);
        if (pic == NULL) {
            continue;
        }
        for (row = 0u; row < 105u; row++) {
            memcpy(canvas + (size_t)row * 210u + xs[i],
                   pic + (size_t)row * group->width + xs[i], 53u);
        }
    }
    return canvas;
}

/* Missile background (fn_1C1D1/fn_1C250): stash and restore the
   viewport rect (8,8,224x136) around every band present so
   stones never smear. 224x136 bytes; caller frees. */
static uint8_t *combat_viewport_stash(yt2_combat *combat)
{
    yt2_surface *surface = combat->surface;
    uint8_t *stash;
    unsigned int row;

    stash = (uint8_t *)malloc((size_t)224u * 136u);
    if (stash == NULL) {
        return NULL;
    }
    for (row = 0u; row < 136u; row++) {
        memcpy(stash + (size_t)row * 224u,
               surface->pixels + (size_t)(8u + row) * surface->stride + 8u,
               224u);
    }
    return stash;
}

static void combat_viewport_restore(yt2_combat *combat, const uint8_t *stash)
{
    yt2_surface *surface = combat->surface;
    unsigned int row;

    if (stash == NULL) {
        return;
    }
    for (row = 0u; row < 136u; row++) {
        memcpy(surface->pixels + (size_t)(8u + row) * surface->stride + 8u,
               stash + (size_t)row * 224u, 224u);
    }
}

/*
 * fn_0D620 main path: the explore S round. Every assigned member
 * with a missile weapon fires; each shot flies the bands until a
 * wall stop (done), a glance (SFX, done), or a monster hit
 * (damage, SFX, death, next member continues at the next band).
 * Stones fly the staged canvas per band (drawn, presented,
 * erased); the hit band presents the erased frame too. The
 * first hit gathers and presents the hit pose (bit 1, one
 * shot) before clearing it; back-to-back member SFX find the
 * driver busy, idle 6 ticks each and drop, which holds the
 * pose (a kill re-gathers the cleared scene instead). The
 * tail runs the monster turn.
 */
static void combat_shoot_round(yt2_combat *combat, yt2_room_present_fn present,
                               void *context)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    unsigned int i;

    combat->members_left = 0u;
    room->f5062 &= (uint16_t)~0x400u;
    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        combat->damage[i] = 0u;
    }
    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        unsigned int slot = party->assigned[i];
        yt2_character *member;
        unsigned int id = 0u;
        unsigned int cc;

        if (slot == 0u) {
            continue;
        }
        member = combat_roster(combat, slot);
        if (member == NULL || (member->status & 0x1C40u) != 0u) {
            continue;
        }
        combat->current = i;
        cc = combat_weapon_cc(combat, member, COMBAT_WEAR_MISSILE,
                              (int)i, &id, present, context);
        /* S fires on v_50CC exactly 1 (fresh or class-1 item). */
        if (cc != 1u) {
            combat->damage[i] = 0u;
            continue;
        }
        combat->damage[i] = (uint16_t)id;
        combat->members_left++;
    }
    if (combat->members_left == 0u) {
        return;
    }
    room->f5062 |= 0x20u;
    /* Pressed bow (pic 2) + present, then the icon restore
       (pic 1) + present. B1 gathers here (fn_10E42), but a B2
       gather would advance sprite animation a second time and
       break quiescence; the icon overpaint reproduces B1's
       observable icon states (pressed for one frame, normal
       through the flight) with B2's proven render count. No
       firers, no flash. */
    combat_paint_icon(combat, 2u);
    if (present != NULL) {
        present(context);
    }
    combat_delay(combat, 2u);
    combat_paint_icon(combat, 1u);
    if (present != NULL) {
        present(context);
    }
    combat_sfx(combat, COMBAT_SFX_ROUND);
    {
        unsigned int band = 0u;
        int sfx_played = 0;
        uint8_t *canvas = combat_missiles_stage(combat);
        uint8_t *bg = combat_viewport_stash(combat);

        /* Band 0x31 draws first, unchecked; then each band
           draws, presents, erases, and checks (fn_1C0DA +
           fn_00F42). The hit band presents the erased frame
           too (loc_0D7D4). B1's 2-tick band delays are pacing. */
        if (canvas != NULL && bg != NULL && present != NULL) {
            yt2_room_draw_missiles(room, combat->surface, canvas, 0x31u);
            present(context);
            combat_viewport_restore(combat, bg);
            combat_delay(combat, 2u);
        }
        while (band < sizeof(combat_bands) / sizeof(combat_bands[0])) {
            yt2_room_object *hit = NULL;
            unsigned int code;

            if (canvas != NULL && bg != NULL && present != NULL) {
                yt2_room_draw_missiles(room, combat->surface, canvas,
                                       combat_bands[band]);
                present(context);
                combat_viewport_restore(combat, bg);
                combat_delay(combat, 2u);
            }
            code = yt2_room_column_hit(room, combat_bands[band], &hit);
            if (code == 0u) {
                band++;
                continue;
            }
            if (present != NULL) {
                present(context);
            }
            if (code == 3u) {
                break;
            }
            if (code != 4u) {
                combat_sfx_busy(combat, COMBAT_SFX_GLANCE, &sfx_played);
                break;
            }
            for (;;) {
                int resolved;

                if (combat->members_left == 0u) {
                    break;
                }
                combat->members_left--;
                resolved = combat_resolve_member(combat, hit);
                if (resolved < 0) {
                    break;
                }
                if ((room->f5062 & 0x200u) == 0u) {
                    combat_sfx_busy(combat, COMBAT_SFX_MISS, &sfx_played);
                    combat_consume(combat);
                    if (combat->members_left == 0u) {
                        break;
                    }
                    continue;
                }
                if ((room->f5062 & 0x400u) == 0u) {
                    room->f5062 |= 0x400u;
                    /* loc_0D814: gather + present with bit 1 set
                       (the hit pose) before the clear below. The
                       gather never paints portraits, so a full
                       repaint here would wipe the formal border
                       with no follow-up to restore it. */
                    yt2_combat_repaint_keep_portraits(combat, present,
                                                      context);
                }
                hit->flags &= (uint16_t)~2u;
                combat_sfx_busy(combat, COMBAT_SFX_HIT, &sfx_played);
                combat_consume(combat);
                if ((int16_t)hit->hp > 0) {
                    if (combat->members_left == 0u) {
                        break;
                    }
                    continue;
                }
                combat_credit(combat, hit);
                combat_clear_cell(combat, hit);
                room->f5062 &= (uint16_t)~0x400u;
                /* loc_0D852: gather + present the cleared scene;
                   later member delays hold it, not the pose.
                   Portraits stay retained (no follow-up). */
                yt2_combat_repaint_keep_portraits(combat, present,
                                                  context);
                /* fn_1C1D1: re-stash the cleared viewport so
                   later bands erase to it (else the kill
                   blinks the stale pre-round spider back). */
                free(bg);
                bg = combat_viewport_stash(combat);
                hit = NULL;
                break;
            }
            if (hit == NULL || combat->members_left == 0u) {
                /*
                 * A kill hands the remaining members to the next
                 * band (loc_0D891: by v_50C8); anything else ends
                 * the round once the words run out.
                 */
                if (hit == NULL && combat->members_left > 0u) {
                    band++;
                    continue;
                }
                break;
            }
            band++;
        }
        free(canvas);
        free(bg);
    }
    yt2_room_monster_turn(room);
    /* Fresh contact settles in fn_06382 at once (queue build, leading
       monsters, first member waiting) instead of back in explore. */
    yt2_combat_formal_entry(combat, present, context);
}

void yt2_combat_formal_entry(yt2_combat *combat,
                             yt2_room_present_fn present, void *context)
{
    yt2_room *room;

    if (combat == NULL || combat->room == NULL) {
        return;
    }
    room = combat->room;
    if ((room->f5064 & 0x1000u) == 0u || combat->queue_count != 0u) {
        return;
    }
    if (combat->chest_open || combat->chest_prompt) {
        return;
    }
    combat_build_queue(combat);
    /* Leading monsters act first (B1's 0690D path); the caller
       repaints the waiting member's border. */
    while (combat->queue_pos < combat->queue_count &&
           combat->queue[combat->queue_pos].monster) {
        combat_monster_turn(combat,
                            combat->queue[combat->queue_pos].index,
                            present, context);
        combat->queue_pos++;
    }
    if ((room->f5064 & 0x1000u) == 0u ||
        combat->queue_pos >= combat->queue_count ||
        combat->queue[combat->queue_pos].monster) {
        return;
    }
    combat->current = combat->queue[combat->queue_pos].index;
}

/* fn_1345B: the award side panel (SFX, texts, numbers, XP, re-init). */
static void combat_award_side_panel(yt2_combat *combat)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    unsigned int i;

    combat_sfx(combat, COMBAT_SFX_VICTORY);
    /* fn_1345B credits the DGROUP purse (0x9503/0x9507/0x950B) from
       the award legs before the XP leg runs: gold += [0], ore +=
       [2] (0x5396), nuore += [1] (0x5392). The purse never reaches
       the save (DGROUP-only, parse zeroes it), but the quick access grid
       and the currency pane read it live. */
    combat_bcd_add(party->gold, combat->award[0]);
    combat_bcd_add(party->ore, combat->award[2]);
    combat_bcd_add(party->nuore, combat->award[1]);
    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        unsigned int slot = party->assigned[i];
        yt2_character *member;

        if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
            continue;
        }
        member = &party->roster[slot - 1u];
        if ((member->status & 0x1C40u) != 0u) {
            continue;
        }
        combat->attacker = slot;
        {
            uint8_t xp[4];

            xp[0] = (uint8_t)(member->experience & 0xFFu);
            xp[1] = (uint8_t)((member->experience >> 8) & 0xFFu);
            xp[2] = (uint8_t)((member->experience >> 16) & 0xFFu);
            xp[3] = (uint8_t)((member->experience >> 24) & 0xFFu);
            combat_bcd_add(xp, combat->award[3]);
            member->experience = (uint32_t)xp[0] |
                                 ((uint32_t)xp[1] << 8) |
                                 ((uint32_t)xp[2] << 16) |
                                 ((uint32_t)xp[3] << 24);
        }
        combat_level_up(combat, member);
    }
    room->save_dirty = 1;
    for (i = 0u; i < 4u; i++) {
        combat->award[i][0] = 0u;
        combat->award[i][1] = 0u;
        combat->award[i][2] = 0u;
        combat->award[i][3] = 0u;
    }
}

/*
 * Snapshot the award buffers for paint, run the side panel when the XP
 * leg is nonzero, and report whether paint has anything to show.
 */
static int combat_finish_award(yt2_combat *combat)
{
    static const uint8_t zero[4] = {0u, 0u, 0u, 0u};
    unsigned int a;

    combat->award_pending = 0;
    for (a = 0u; a < 4u; a++) {
        combat->last_award[a][0] = combat->award[a][0];
        combat->last_award[a][1] = combat->award[a][1];
        combat->last_award[a][2] = combat->award[a][2];
        combat->last_award[a][3] = combat->award[a][3];
    }
    if (combat_bcd_compare(combat->award[3], zero) == 0) {
        return 0;
    }
    /* fn_1345B's fn_06EA4 wipes stale side panel lines (currency pane
       from the fight just ended) before the loot paints. */
    combat->chest_side_panel_lines = 0u;
    combat_award_side_panel(combat);
    combat->award_pending = 1;
    return 1;
}

/* fn_1345B paint half: labels at x=240, numbers 6px below (fn_09C46). */
static void combat_paint_award(yt2_combat *combat, uint8_t award[4][4])
{
    static const uint8_t zero[4] = {0u, 0u, 0u, 0u};
    yt2_surface *surface = combat->surface;
    char number[16];
    int y = 0x60;

    yt2_text_draw(surface, 0xF0, y, COMBAT_TEXT_GOLD, 0x8Au);
    y += 6;
    combat_bcd_format(award[0], number, sizeof(number));
    yt2_text_draw(surface, 0xF0, y, number, 0x0Fu);
    y += 0x0C;
    if (combat_bcd_compare(award[2], zero) != 0) {
        yt2_text_draw(surface, 0xF0, y, COMBAT_TEXT_ORE, 0x8Au);
        y += 6;
        combat_bcd_format(award[2], number, sizeof(number));
        yt2_text_draw(surface, 0xF0, y, number, 0x0Fu);
        y += 6;
    }
    if (combat_bcd_compare(award[1], zero) != 0) {
        yt2_text_draw(surface, 0xF0, y, COMBAT_TEXT_NUORE, 0x8Au);
        y += 6;
        combat_bcd_format(award[1], number, sizeof(number));
        yt2_text_draw(surface, 0xF0, y, number, 0x0Fu);
    }
    y = 0x90;
    yt2_text_draw(surface, 0xF0, y, COMBAT_TEXT_XP, 0x7Bu);
    y += 6;
    combat_bcd_format(award[3], number, sizeof(number));
    yt2_text_draw(surface, 0xF0, y, number, 0x0Fu);
}

/*
 * sub_137E9: one engaged name (two definition lines) at the panel
 * rows (0x57/0x7B/0x9F). The HP bar and status icon need v_9499
 * 0x37+, never observed here, and stay out.
 */
static void combat_paint_engaged(yt2_combat *combat)
{
    static const int rows[YT2_COMBAT_ENGAGED_MAX] = {0x57, 0x7B, 0x9F};
    yt2_room *room = combat->room;
    unsigned int i;

    for (i = 0u; i < YT2_COMBAT_ENGAGED_MAX; i++) {
        yt2_room_object *obj = &room->near_objects[i];
        uint8_t color = 9u;

        if (obj->id == 0u) {
            continue;
        }
        if ((obj->flags & 0x20u) == 0u) {
            color = 9u;
            if (combat->target == (int)i) {
                color = 0x8Au;
            }
        } else {
            color = 0xAAu;
        }
        yt2_text_draw(combat->surface, 0xF1, rows[i], obj->name, color);
    }
    /* The formal panel blanks the compass side panel (flat 4). */
    for (int y = 93; y <= 95; y++) {
        for (int x = 240; x <= 312; x++) {
            combat->surface->pixels[(size_t)y * combat->surface->stride +
                                     (size_t)x] = 4u;
        }
    }
}

/* Attacker fitness (sub_06493 gate): assigned and live. */
static int combat_attacker_fit(yt2_combat *combat, unsigned int index)
{
    yt2_party *party = combat->room->party;
    unsigned int slot;

    if (index >= YT2_COMBAT_ATTACKERS_MAX) {
        return 0;
    }
    slot = party->assigned[index];
    if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
        return 0;
    }
    return (party->roster[slot - 1u].status & 0x1C40u) == 0u;
}

/* sub_06493 next hero: set context, repaint portraits fresh with
   the border on the new current hero, clear the side panel and paint the
   engaged names (B1's fn_135B2 each time), present. Unfit
   heroes are skipped silently (returns 0); callers step past
   them. */
static int combat_next_hero(yt2_combat *combat, unsigned int index,
                                 yt2_room_present_fn present, void *context)
{
    if (!combat_attacker_fit(combat, index)) {
        return 0;
    }
    combat->current = index;
    yt2_room_paint_portraits(combat->room->party, combat->pictures,
                             combat->surface);
    combat_paint_highlight(combat);
    chest_fill(combat, 240, 86, 73, 109);
    combat->room->f5062 &= (uint16_t)~0x1000u;
    combat_paint_engaged(combat);
    combat->chest_side_panel_lines = 0u;
    if (present != NULL) {
        present(context);
    }
    return 1;
}

/* fn_171AE current-member bit: 1px color-219 border on the 32x32 pic. */
static void combat_paint_highlight_at(yt2_combat *combat, unsigned int slot)
{
    yt2_surface *surface = combat->surface;
    int x;
    int y;
    int i;

    if (yt2_room_portrait_origin(slot, &x, &y) != YT2_STATUS_OK) {
        return;
    }
    for (i = 0; i < 32; i++) {
        if ((unsigned int)(x + i) >= surface->width ||
            (unsigned int)(y + i) >= surface->height) {
            continue;
        }
        surface->pixels[(size_t)y * surface->stride + (x + i)] = 219u;
        surface->pixels[(size_t)(y + 31) * surface->stride + (x + i)] = 219u;
        surface->pixels[(size_t)(y + i) * surface->stride + x] = 219u;
        surface->pixels[(size_t)(y + i) * surface->stride + (x + 31)] = 219u;
    }
}

static void combat_paint_highlight(yt2_combat *combat)
{
    combat_paint_highlight_at(combat, combat->current);
}

/* sub_06AC5: build the combat queue, pick monster targets, sort. */
static void combat_build_queue(yt2_combat *combat)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    unsigned int count = 0u;
    unsigned int i;

    /* First build of the fight saves the explore panel bits
       (rebuilds at queue end keep the original save), then the
       formal entry forces quick access exclusive like fn_1755C. */
    if (!combat->panel_saved_formal_valid) {
        combat_panel_save_formal(combat);
        combat->room->party->explore_flags = (uint16_t)(
            (combat->room->party->explore_flags & 0x9FFFu) | 0x1000u);
    }

    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        unsigned int slot = party->assigned[i];
        yt2_character *member;

        if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
            continue;
        }
        member = &party->roster[slot - 1u];
        if ((member->status & 0x1C40u) != 0u) {
            continue;
        }
        combat->queue[count].monster = 0;
        combat->queue[count].index = i;
        combat->queue[count].initiative = member->current.attributes.dexterity;
        combat->queue[count].flags = 0u;
        count++;
    }
    for (i = 0u; i < YT2_COMBAT_ENGAGED_MAX; i++) {
        yt2_room_object *obj = &room->near_objects[i];

        if (obj->id == 0u) {
            continue;
        }
        combat->queue[count].monster = 1;
        combat->queue[count].index = i;
        combat->queue[count].initiative = obj->initiative;
        combat->queue[count].flags = 0x8000u;
        if ((obj->flags & 0x3010u) != 0u) {
            combat->queue[count].flags |= 0x2000u;
        }
        obj->target = 0u;
        if ((combat->queue[count].flags & 0x2000u) == 0u) {
            /* Rejection-sample a live member (B1's rand-per-try
               consumption kept exact); the bound only triggers on
               a TPK, where B1's game-over owns the outcome and
               the target stays 0. */
            unsigned int tries = 0u;

            for (;;) {
                unsigned int pick = yt2_create_rand(
                    &room->rng_seed, room->time_word, 3u);
                unsigned int slot;
                yt2_character *member;

                if (++tries > 64u) {
                    break;
                }
                if (pick >= YT2_COMBAT_ATTACKERS_MAX) {
                    continue;
                }
                slot = party->assigned[pick];
                if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
                    continue;
                }
                member = &party->roster[slot - 1u];
                if ((member->status & 0x1C40u) != 0u) {
                    continue;
                }
                obj->target = (uint16_t)slot;
                break;
            }
        }
        count++;
    }
    /* Bubble sort by initiative descending (stable for ties). */
    for (;;) {
        int swapped = 0;

        for (i = 0u; i + 1u < count; i++) {
            if (combat->queue[i + 1u].initiative >
                combat->queue[i].initiative) {
                yt2_combat_entry tmp = combat->queue[i + 1u];

                combat->queue[i + 1u] = combat->queue[i];
                combat->queue[i] = tmp;
                swapped = 1;
            }
        }
        if (!swapped) {
            break;
        }
    }
    combat->queue_count = count;
    combat->queue_pos = 0u;
    if (combat->target < 0) {
        for (i = 0u; i < count; i++) {
            if (combat->queue[i].monster &&
                (combat->queue[i].flags & 0x4000u) == 0u) {
                combat->target = (int)combat->queue[i].index;
                break;
            }
        }
    }
}

/* sub_06E0B: default target, first live monster in queue order. */
static void combat_pick_target(yt2_combat *combat)
{
    unsigned int i;

    combat->target = -1;
    for (i = 0u; i < combat->queue_count; i++) {
        if (combat->queue[i].monster &&
            (combat->queue[i].flags & 0x4000u) == 0u) {
            combat->target = (int)combat->queue[i].index;
            break;
        }
    }
}

/*
 * sub_06BEF: scan the engaged row for fresh kills (credit, clear
 * v_51BA on match, fn_12EA0/fn_13447). Returns 0 when no monster
 * is left standing, else 2 with the queue advanced past the
 * resolved entries.
 */
static unsigned int combat_scan(yt2_combat *combat)
{
    yt2_room *room = combat->room;
    unsigned int code = 0u;
    unsigned int i;

    for (i = 0u; i < YT2_COMBAT_ENGAGED_MAX; i++) {
        yt2_room_object *obj = &room->near_objects[i];
        unsigned int q;

        if (obj->id == 0u) {
            continue;
        }
        if ((int16_t)obj->hp > 0) {
            code = 2u;
            continue;
        }
        for (q = 0u; q < combat->queue_count; q++) {
            if (!combat->queue[q].monster ||
                combat->queue[q].index != i) {
                continue;
            }
            combat->queue[q].flags |= 0x4000u;
        }
        if (combat->target == (int)i) {
            combat->target = -1;
        }
        combat_credit(combat, obj);
        combat_zero(obj);
    }
    if (code != 2u) {
        return 0u;
    }
    if (combat->target < 0) {
        combat_pick_target(combat);
    }
    while (combat->queue_pos < combat->queue_count &&
           (combat->queue[combat->queue_pos].flags & 0x4000u) != 0u) {
        combat->queue_pos++;
    }
    return combat->queue_pos < combat->queue_count ? 2u : 1u;
}

/*
 * loc_06799: the member melee strike (v_5070 vs v_51BA). Raw HP
 * subtract, per-weapon hit SFX from the parameter row; the hit
 * flash and the v_50F2 spin are visual only and deferred.
 */
/* fn_18C1C: stage the injury flags from the damage share.
   Minor (<= 10% of max HP) sets flags2 0x8000, medium (<= 30%)
   0x4000, major 0x2000; flags 0x0A always (hit pose + overlay,
   both one-shot in the renderer). 16-bit wrap like the mul. */
static void combat_injure(yt2_room_object *target, uint16_t damage)
{
    uint16_t tenth;
    uint16_t third;

    tenth = (uint16_t)((uint16_t)(10u * target->hp_max) + 50u);
    tenth = (uint16_t)(tenth / 100u);
    target->flags2 |= 0x8000u;
    if (damage > tenth) {
        third = (uint16_t)((uint16_t)(30u * target->hp_max) + 50u);
        third = (uint16_t)(third / 100u);
        target->flags2 &= (uint16_t)~0x8000u;
        target->flags2 |= 0x4000u;
        if (damage > third) {
            target->flags2 &= (uint16_t)~0x4000u;
            target->flags2 |= 0x2000u;
        }
    }
    target->flags |= 0x0Au;
}

/*
 * loc_06799: one formal A (melee strike). Flashes the pressed
 * swords (pic 4) and presents, builds the weapon, rolls damage,
 * and on a hit stages the injury, presents the blood frame,
 * plays the weapon SFX and subtracts HP; on a miss or fumble it
 * plays the miss SFX (fumbles stay silent) and skips the blood.
 * The restore overpaints the swords (pic 3) onto the retained
 * frame without re-rendering (B1's blood lingers past it) and
 * presents. B1's trailing v_50F2 wait is pacing only and stays
 * out.
 */
static void combat_melee_restore(yt2_combat *combat,
                                 yt2_room_present_fn present, void *context)
{
    combat->button_pressed = 0u;
    combat_paint_swords(combat);
    if (present != NULL) {
        present(context);
    }
    /* Trailing v_50F2 wait, verbatim 3. */
    combat_delay(combat, 3u);
}

static void combat_melee(yt2_combat *combat, yt2_room_present_fn present,
                         void *context)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    unsigned int slot = party->assigned[combat->current];
    yt2_character *member;
    yt2_room_object *target;
    const uint8_t *row;

    if (slot == 0u || slot > YT2_ROSTER_SLOTS || combat->target < 0) {
        return;
    }
    member = combat_roster(combat, slot);
    target = &room->near_objects[combat->target];
    if (member == NULL || target->id == 0u) {
        return;
    }
    combat->button_pressed = 4u;
    yt2_combat_repaint(combat, present, context);
    /* B1 holds the flash across its render (emu-observed ~13
       ticks); the principled beat is 2. */
    combat_delay(combat, 2u);
    /* Melee swings unless the build fumbles (v_50CC 0); an empty
       slot (4) swings fists, silently (row read misses). */
    if (combat_weapon_cc(combat, member, COMBAT_WEAR_MELEE,
                         combat->current, NULL, present,
                         context) == 0u) {
        combat_melee_restore(combat, present, context);
        return;
    }
    combat_scale(combat, target->defense,
                 member->current.combat.battle_accuracy,
                 member->current.combat.battle_damage);
    if (combat->damage_roll == 0u) {
        combat_sfx(combat, COMBAT_SFX_MISS);
        combat_melee_restore(combat, present, context);
        return;
    }
    combat_injure(target, combat->damage_roll);
    yt2_combat_repaint(combat, present, context);
    combat_delay(combat, 2u);
    if (combat_item(combat, member->equipment[2].id, NULL, &row) ==
            YT2_STATUS_OK &&
        row != NULL && combat_read16(row + 10) != 0u) {
        combat_sfx(combat, combat_read16(row + 10));
    }
    target->hp = (uint16_t)(target->hp - combat->damage_roll);
    combat_melee_restore(combat, present, context);
}

/* Raw member-record u16 access (sub_08386/fn_0AACC address
   fields by offset). Covers the live-decoded words; anything
   else is unreachable in shipped content (all staged offsets
   are 0, all item legs read NULL) and reads 0 / writes nothing.
   Offset 0 is the name's first two bytes (vanilla quirk kept:
   a failed save vs effects 19/20 decrements them). The overlay
   assumes the stat structs pack tightly (verified below). */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(yt2_attributes) == 12u, "attr pack");
_Static_assert(sizeof(yt2_combat_stats) == 16u, "combat pack");
_Static_assert(sizeof(yt2_skills) == 26u, "skills pack");
_Static_assert(sizeof(yt2_stats) == 54u, "stats pack");
#endif

static uint16_t combat_member_at(const yt2_character *member,
                                 unsigned int offset)
{
    const uint16_t *stats;
    unsigned int rel;

    if (member == NULL) {
        return 0u;
    }
    if (offset == 0u) {
        return (uint16_t)((uint8_t)member->name[0] |
                          ((uint16_t)(uint8_t)member->name[1] << 8));
    }
    if (offset == 0x16u) {
        return member->level;
    }
    if (offset == 0x1Cu) {
        return member->status;
    }
    if (offset == 0x118u) {
        return member->carried_weight;
    }
    if (offset >= 0x32u && offset <= 0x3Au && (offset & 1u) == 0u) {
        return member->base[(offset - 0x32u) / 2u];
    }
    if (offset >= 0x72u && offset <= 0x7Au && (offset & 1u) == 0u) {
        return member->max_base[(offset - 0x72u) / 2u];
    }
    stats = NULL;
    if (offset >= 0x3Cu && offset < 0x3Cu + 0x36u &&
        (offset & 1u) == 0u) {
        stats = (const uint16_t *)&member->current;
        rel = offset - 0x3Cu;
    } else if (offset >= 0x7Cu && offset < 0x7Cu + 0x36u &&
               (offset & 1u) == 0u) {
        stats = (const uint16_t *)&member->maximum;
        rel = offset - 0x7Cu;
    }
    if (stats != NULL) {
        return stats[rel / 2u];
    }
    return 0u;
}

static void combat_member_set(yt2_character *member, unsigned int offset,
                              uint16_t value)
{
    uint16_t *stats;
    unsigned int rel;

    if (member == NULL) {
        return;
    }
    if (offset == 0u) {
        member->name[0] = (char)(value & 0xFFu);
        member->name[1] = (char)((value >> 8) & 0xFFu);
        return;
    }
    if (offset == 0x16u) {
        member->level = value;
        return;
    }
    if (offset == 0x1Cu) {
        member->status = value;
        return;
    }
    if (offset == 0x118u) {
        member->carried_weight = value;
        return;
    }
    if (offset >= 0x32u && offset <= 0x3Au && (offset & 1u) == 0u) {
        member->base[(offset - 0x32u) / 2u] = value;
        return;
    }
    if (offset >= 0x72u && offset <= 0x7Au && (offset & 1u) == 0u) {
        member->max_base[(offset - 0x72u) / 2u] = value;
        return;
    }
    stats = NULL;
    if (offset >= 0x3Cu && offset < 0x3Cu + 0x36u &&
        (offset & 1u) == 0u) {
        stats = (uint16_t *)&member->current;
        rel = offset - 0x3Cu;
    } else if (offset >= 0x7Cu && offset < 0x7Cu + 0x36u &&
               (offset & 1u) == 0u) {
        stats = (uint16_t *)&member->maximum;
        rel = offset - 0x7Cu;
    }
    if (stats != NULL) {
        stats[rel / 2u] = value;
    }
}

/* sub_0814D: drop a roster slot (1-based) from the highlight row. */
static void combat_unhighlight(yt2_party *party, unsigned int slot)
{
    unsigned int i;

    if (party == NULL || slot == 0u) {
        return;
    }
    for (i = 0u; i < YT2_PARTY_ACTIVE_MAX; i++) {
        if (party->active[i] == slot) {
            party->active[i] = 0u;
        }
    }
}

/* sub_082E2: HP damage (floor 0 + status 0x40 on death). The
   caller passes the member's 1-based roster slot for the
   highlight clear. */
static void combat_hit_points(yt2_combat *combat, yt2_character *member,
                              unsigned int slot, uint16_t damage)
{
    /* Signed 16-bit wrap (sub + jg): only a positive remainder
       survives, anything else kills. */
    uint16_t raw =
        (uint16_t)(member->current.combat.hit_points - damage);

    if ((int16_t)raw <= 0) {
        member->current.combat.hit_points = 0u;
        member->status |= 0x40u;
        combat_unhighlight(combat->room->party, slot);
        yt2_room_update_skill_flags(combat->room->party);
        return;
    }
    member->current.combat.hit_points = raw;
}

/* sub_08300: SP damage (signed floor 0, same wrap). */
static void combat_spell_points(yt2_character *member, uint16_t damage)
{
    uint16_t raw =
        (uint16_t)(member->current.combat.magic_points - damage);

    member->current.combat.magic_points = (int16_t)raw < 0 ? 0u : raw;
}

/* sub_082BD: purse drain (the take side of a currency hit).
   Drains the leg at (ax) by the slot BCD; an overdraft zeroes
   the leg and repaints the grid (fn_1755C). ax selects the leg
   (gold/ore/nuore); the amount sits in the slot's BCD words. */
static void combat_purse_drain(yt2_combat *combat, unsigned int leg,
                               const uint8_t *amount)
{
    yt2_party *party = combat->room->party;
    uint8_t *purse = party->gold;

    if (leg == 1u) {
        purse = party->ore;
    } else if (leg == 2u) {
        purse = party->nuore;
    }
    if (combat_bcd_compare(purse, amount) > 0) {
        chest_bcd_sub(purse, amount);
        return;
    }
    purse[0] = 0u;
    purse[1] = 0u;
    purse[2] = 0u;
    purse[3] = 0u;
    quick_access_paint(combat);
}

/* sub_0849C: randomize an unstaged damage leg. Staged melee
   damage (nonzero amount, no purse legs) passes through
   untouched; otherwise the amount is the record floor (flat
   under 0x4000, range-rolled unless 0x2000) times member level
   (skipped under 0x4000 with the multiply). */
static void combat_randomize(yt2_combat *combat, yt2_combat_damage *slot,
                             yt2_character *member, const uint8_t *effect)
{
    yt2_room *room = combat->room;
    uint16_t flags = combat_read16(effect + 8);
    uint16_t lo;
    uint16_t span;
    uint16_t roll;

    if ((flags & 7u) != 0u || slot->amount != 0u) {
        return;
    }
    lo = combat_read16(effect + 4);
    if ((combat_read16(effect + 10) & 0x4000u) != 0u) {
        slot->amount = lo;
        return;
    }
    if ((combat_read16(effect + 10) & 0x2000u) == 0u) {
        span = (uint16_t)(combat_read16(effect + 6) - lo);
        roll = yt2_create_rand(&room->rng_seed, room->time_word, span);
        lo = (uint16_t)(lo + roll);
    }
    slot->amount = (uint16_t)(lo * member->level);
}

/* sub_083FA: the status save (buffs resist the effect's status
   legs, fn_17D2C over v_555C). A passed save clears the staged
   status; a failed one (or prestaged status) keeps it. */
static void combat_status_save(yt2_combat *combat, yt2_combat_damage *slot,
                               yt2_character *member, const uint8_t *effect)
{
    uint16_t legs;
    uint16_t save = 0u;
    unsigned int roll;

    if (slot->status != 0u) {
        return;
    }
    legs = (uint16_t)(combat_read16(effect + 8) & 0xFF80u);
    if (legs == 0u || (combat_read16(effect + 10) & 0x1000u) == 0u) {
        slot->status = 0u;
        return;
    }
    if ((combat_read16(effect + 8) & 0x8000u) != 0u) {
        save = (uint16_t)(save + member->buffs[2]);
    }
    if ((combat_read16(effect + 8) & 0x4000u) != 0u) {
        save = (uint16_t)(save + member->buffs[1]);
    }
    if ((combat_read16(effect + 8) & 0x2000u) != 0u) {
        save = (uint16_t)(save + member->buffs[0]);
    }
    if ((combat_read16(effect + 8) & 0x1000u) != 0u) {
        save = (uint16_t)(save + member->buffs[5]);
    }
    if ((combat_read16(effect + 8) & 0x0800u) != 0u) {
        save = (uint16_t)(save + member->buffs[4]);
    }
    if ((combat_read16(effect + 8) & 0x0400u) != 0u) {
        save = (uint16_t)(save + member->buffs[3]);
    }
    if ((combat_read16(effect + 8) & 0x0200u) != 0u) {
        save = (uint16_t)(save + member->buffs[8]);
    }
    if ((combat_read16(effect + 8) & 0x0100u) != 0u) {
        save = (uint16_t)(save + member->buffs[7]);
    }
    if ((combat_read16(effect + 8) & 0x0080u) != 0u) {
        save = (uint16_t)(save + member->buffs[6]);
    }
    /* fn_17D2C over (member level, v_555C, save): d101 over the
       threshold resists. v_555C is the staged power (monster
       +0x52, spell id, or event param). */
    roll = chest_skill_roll(combat, member->level, combat->save_power,
                            save);
    if (roll == 0u) {
        slot->status = 0u;
        return;
    }
    slot->status = legs;
}

/* sub_08386: the 0x180 stat arm (effects 18/19/20, events only;
   monster effects take plain). Under 0x100 the member word at
   the staged offset rises by the staged status (capped at the
   staged cap word), else under 0x80 it falls (floor 0);
   exclusive, 0x100 wins. Then the v_5AA2/v_5A7E-gated status
   mask plus the rederive/skill/level chain. Shipped staging
   leaves both offsets 0. */
static void combat_stat_effect(yt2_combat *combat, yt2_combat_damage *slot,
                               yt2_character *member, const uint8_t *effect)
{
    uint16_t flags = combat_read16(effect + 10);
    uint16_t value;

    if ((flags & 0x180u) == 0u) {
        return;
    }
    if ((flags & 0x100u) != 0u) {
        value = (uint16_t)(combat_member_at(member, slot->amount) +
                           slot->status);
        if (value > combat_member_at(member, slot->aux)) {
            value = combat_member_at(member, slot->aux);
        }
        combat_member_set(member, slot->amount, value);
    } else if ((flags & 0x80u) != 0u) {
        /* Signed floor (sub + jge) like the HP/SP arms. */
        value = (uint16_t)(combat_member_at(member, slot->amount) -
                           slot->status);
        combat_member_set(member, slot->amount,
                          (int16_t)value < 0 ? 0u : value);
    }
    if ((combat->status_mask_any & 0xC0u) == 0u) {
        member->status &= (uint16_t)(combat->status_mask & 0xFFC0u);
    }
    combat_rederive(member);
    yt2_room_update_skill_flags(combat->room->party);
    combat_level_up(combat, member);
}

/* sub_0830F: route HP/SP/purse legs plus the staged status.
   The purse amount is the slot's overlapped BCD (amount high,
   aux low, big-endian bytes); the status OR may kill via
   0x1C40 (highlight clear plus skill flags like a death). */
static void combat_plain_effect(yt2_combat *combat, yt2_combat_damage *slot,
                                yt2_character *member, unsigned int roster,
                                const uint8_t *effect)
{
    uint16_t flags = combat_read16(effect + 8);
    uint8_t bcd[4];

    if ((flags & 0x10u) != 0u) {
        combat_hit_points(combat, member, roster, slot->amount);
    } else if ((flags & 0x08u) != 0u) {
        combat_spell_points(member, slot->amount);
    } else if ((flags & 0x20u) != 0u) {
        combat_hit_points(combat, member, roster, slot->amount);
        combat_spell_points(member, slot->amount);
    } else {
        bcd[0] = (uint8_t)(slot->amount >> 8);
        bcd[1] = (uint8_t)(slot->amount & 0xFFu);
        bcd[2] = (uint8_t)(slot->aux >> 8);
        bcd[3] = (uint8_t)(slot->aux & 0xFFu);
        if ((flags & 0x01u) != 0u) {
            combat_purse_drain(combat, 0u, bcd);
        } else if ((flags & 0x04u) != 0u) {
            combat_purse_drain(combat, 1u, bcd);
        } else if ((flags & 0x02u) != 0u) {
            combat_purse_drain(combat, 2u, bcd);
        }
    }
    if (slot->status == 0u) {
        return;
    }
    member->status |= slot->status;
    if ((member->status & 0x1C40u) == 0u) {
        return;
    }
    combat_unhighlight(combat->room->party, roster);
    yt2_room_update_skill_flags(combat->room->party);
}

/* fn_0AACC: apply an item's bonus legs (4 offset/bonus pairs):
   offsets below 0x32 add unconditionally, the rest add only
   over a nonzero word; everything caps at 999. A legless item
   (def+2 == 0, v_0CE4 NULL) rederives only. B2 reads legs as
   NULL: the def+2 EMS table has no WORLD mapping yet, which is
   B1-identical for the 599 legless items and degrades the 145
   legged ones' 0x600 path to rederive (residual gap, see the
   porting notes). */
static void combat_bonus_apply(yt2_combat *combat, yt2_character *member,
                               unsigned int id)
{
    const uint8_t *def;
    const uint8_t *row;
    const uint8_t *legs = NULL;

    (void)id;
    (void)legs;
    if (combat_item(combat, id, &def, &row) != YT2_STATUS_OK) {
        combat_rederive(member);
        yt2_room_update_skill_flags(combat->room->party);
        return;
    }
    /* Legs resolve here once the def+2 table is mapped. */
    combat_rederive(member);
    yt2_room_update_skill_flags(combat->room->party);
}

/* sub_08253: the 0x600 equipment arm (effects 0/1/22/43,
   events only). Applies the slot amount as an item id through
   the bonus legs (fn_0ACF5), then writes the amount/aux pair
   over the member words at the staged status offset (+2/+0;
   shipped staging leaves status 0, i.e. the name head absurdly
   but verbatim) or, under the record's 0x200 bit, zeroes the
   addressed word and drains +0x118 by the item's row +0x0A.
   Reuses the caller's icon read (def+8) for the portrait blit
   through the out param. */
static void combat_equip_effect(yt2_combat *combat, yt2_combat_damage *slot,
                                yt2_character *member, const uint8_t *effect,
                                uint16_t *icon)
{
    const uint8_t *def;
    const uint8_t *row;
    unsigned int at;

    combat_bonus_apply(combat, member, slot->amount);
    at = slot->status;
    if ((combat_read16(effect + 10) & 0x200u) == 0u) {
        /* Equip staging: the amount/aux pair lands over the
           member words at the status offset (+2/+0); the icon
           comes from the aux item's def+8 (0 when unresolvable,
           like B1's zeroed def). */
        combat_member_set(member, at + 2u, slot->amount);
        combat_member_set(member, at, slot->aux);
        *icon = 0u;
        if (combat_item(combat, slot->aux, &def, &row) ==
            YT2_STATUS_OK) {
            *icon = combat_read16(def + 8);
        }
        combat_bonus_apply(combat, member, slot->aux);
    } else {
        /* Strip staging: the addressed word zeroes and +0x118
           (carried weight) falls by the amount item's def+0x0A;
           the icon is the amount item's def+8. */
        uint16_t have;

        combat_member_set(member, at, 0u);
        *icon = 0u;
        if (combat_item(combat, slot->amount, &def, &row) ==
            YT2_STATUS_OK) {
            *icon = combat_read16(def + 8);
            have = combat_member_at(member, 0x118u);
            /* Plain sub, wraps (no floor in B1). */
            combat_member_set(member, 0x118u,
                              (uint16_t)(have - combat_read16(def + 10)));
        }
    }
    combat_rederive(member);
    yt2_room_update_skill_flags(combat->room->party);
}

/* fn_08172: apply the staged damage slots. Each staged slot
   plays the record SFX once (v_5062 bit 4 guard), routes by
   the record's +0x0A (0x600 equipment, 0x180 stat, else the
   plain randomize/save/route chain), and blits the record's +2
   pic (group 7, transparent) over the member portrait (plain
   origin, or origin+(8,8) for the 0x600 arm). The tail presents,
   idles 12 ticks when no SFX played (pacing, skipped), repaints
   the portraits, and checks for a wipe. */
static void combat_apply_damage(yt2_combat *combat,
                                yt2_room_present_fn present, void *context)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    unsigned int i;

    room->f5062 &= (uint16_t)~0x04u;
    {
        int any = 0;

        for (i = 0u; i < YT2_COMBAT_DAMAGE_SLOTS; i++) {
            if (combat->damage_slots[i].staged) {
                any = 1;
                break;
            }
        }
        /* Silent apply idles 12 (verbatim); staged slots play. */
        if (!any) {
            combat_delay(combat, 12u);
        }
    }
    for (i = 0u; i < YT2_COMBAT_DAMAGE_SLOTS; i++) {
        yt2_combat_damage *slot = &combat->damage_slots[i];
        const uint8_t *effect;
        yt2_character *member;
        unsigned int roster;
        const yt2_picture_group *group;
        const uint8_t *pic;
        int x;
        int y;

        if (!slot->staged || slot->member < 0 ||
            slot->member >= (int)YT2_COMBAT_ATTACKERS_MAX) {
            continue;
        }
        if (slot->effect >= 50u) {
            continue;
        }
        effect = combat_effects[slot->effect];
        roster = party->assigned[slot->member];
        if (roster == 0u || roster > YT2_ROSTER_SLOTS) {
            continue;
        }
        member = &party->roster[roster - 1u];
        if ((room->f5062 & 0x04u) == 0u) {
            unsigned int id = combat_read16(effect + 0);

            room->f5062 |= 0x04u;
            if (id != 0u) {
                combat_sfx(combat, id);
            }
        }
        if ((combat_read16(effect + 10) & 0x600u) != 0u) {
            uint16_t icon = 0u;

            combat_equip_effect(combat, slot, member, effect, &icon);
            if (yt2_room_portrait_origin((unsigned int)slot->member, &x,
                                         &y) != YT2_STATUS_OK) {
                continue;
            }
            group = yt2_pictures_group(combat->pictures, 7u);
            pic = yt2_picture_data(combat->pictures, 7u, icon);
            if (group != NULL && pic != NULL) {
                yt2_surface_blit_transparent(combat->surface, x + 8, y + 8,
                                             pic, group->width,
                                             group->height, 0xFFu);
            }
            continue;
        }
        if ((combat_read16(effect + 10) & 0x180u) != 0u) {
            combat_stat_effect(combat, slot, member, effect);
        } else {
            combat_randomize(combat, slot, member, effect);
            combat_status_save(combat, slot, member, effect);
            combat_plain_effect(combat, slot, member, roster, effect);
        }
        if (yt2_room_portrait_origin((unsigned int)slot->member, &x,
                                     &y) != YT2_STATUS_OK) {
            continue;
        }
        group = yt2_pictures_group(combat->pictures, 7u);
        pic = yt2_picture_data(combat->pictures, 7u,
                               combat_read16(effect + 2));
        if (group != NULL && pic != NULL) {
            yt2_surface_blit_transparent(combat->surface, x, y, pic,
                                         group->width, group->height,
                                         0xFFu);
        }
    }
    if (present != NULL) {
        present(context);
    }
    /* fn_12602: fresh portraits over the retained frame, then
       the wipe check lands in this turn. */
    yt2_room_paint_portraits(combat->room->party, combat->pictures,
                             combat->surface);
    if (present != NULL) {
        present(context);
    }
    combat_check_wipe(combat, present, context);
}

/* fn_15FBC: the wipe check. Returns at the first live member;
   with none alive, paints the death screen (fn_1B382: SFX 19,
   plain chrome, present) and flags the caller for menu 0x70 +
   load. */
static void combat_check_wipe(yt2_combat *combat,
                              yt2_room_present_fn present, void *context)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    unsigned int i;

    for (i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        unsigned int slot = party->assigned[i];
        yt2_character *member;

        if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
            break;
        }
        member = &party->roster[slot - 1u];
        if ((member->status & 0x1C40u) == 0u) {
            return;
        }
    }
    /* sub_1B3B4: flag masks plus the cleared queue; combat
       state otherwise persists under the menu like B1's DGROUP. */
    room->f505e &= 0xDAFFu;
    room->f5062 &= 0x0803u;
    room->f5064 &= 0x0FFFu;
    combat->queue_count = 0u;
    combat->queue_pos = 0u;
    combat->target = -1;
    combat->award_pending = 0;
    /* The abandoned fight never victorious-restores; drop its save. */
    combat->panel_saved_formal_valid = 0;
    combat->chest_side_panel_lines = 0u;
    combat_sfx(combat, 19u);
    yt2_combat_repaint(combat, present, context);
    combat->death_pending = 1;
}

/* fn_DelayTicks: hold the current present (live sessions sleep
   ticks*55ms, batch tools NULL-skip so gates stay instant). */
static void combat_delay(yt2_combat *combat, unsigned int ticks)
{
    if (combat != NULL && combat->room != NULL &&
        combat->room->delay != NULL) {
        combat->room->delay(ticks, combat->room->delay_context);
    }
}

/* fn_12FF7: the formal damage-over-time tick. Returns 0 to
   attack, 1 when the tick killed (HP pinned at 0, the scan
   credits), 2 on the double-tick (flags 0x3010, still alive).
   +0x1C is the damage per turn, +0x1E the turns left; expiry
   clears the effect bits and restores the base picture. */
static unsigned int combat_dot_tick(yt2_room_object *obj)
{
    if ((obj->flags & 0xFC10u) == 0u) {
        return 0u;
    }
    obj->hp = (uint16_t)(obj->hp - obj->step);
    if ((int16_t)obj->hp > 0) {
        if ((obj->flags & 0x3010u) == 0u) {
            obj->cooldown = (uint16_t)(obj->cooldown - 1u);
            if ((int16_t)obj->cooldown > 0) {
                return 0u;
            }
            obj->flags &= 0x03EDu;
            obj->pic2 = 0u;
            obj->step = 0u;
            obj->cooldown = 0u;
            obj->pic = obj->anim_base;
            return 0u;
        }
        obj->hp = (uint16_t)(obj->hp - obj->step);
        if ((int16_t)obj->hp > 0) {
            return 2u;
        }
    }
    obj->hp = 0u;
    return 1u;
}

/* sub_06DD9: resolve a target member record to its assigned
   index (v_50C0 slot scan). Returns -1 when the record is not
   assigned (dead/left). */
static int combat_member_resolve(yt2_combat *combat, yt2_character *member)
{
    yt2_party *party = combat->room->party;

    for (unsigned int i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
        unsigned int slot = party->assigned[i];

        if (slot != 0u && slot <= YT2_ROSTER_SLOTS &&
            &party->roster[slot - 1u] == member) {
            return (int)i;
        }
    }
    return -1;
}

/* sub_06E36: the effect roll. Clears v_5064 0x200, picks the
   base effect (+0x6C) or, when +0x6E is set and unwarded
   (+0x0C 0x400 clear) and d100 lands under 25, the special
   (+0x6E, raising 0x200). Returns the rolled id; fn_08120's
   slot clear is the caller's damage_slots reset. */
static unsigned int combat_effect_roll(yt2_combat *combat,
                                       yt2_room_object *monster)
{
    yt2_room *room = combat->room;
    unsigned int roll;

    room->f5064 &= (uint16_t)~0x0200u;
    if ((monster->flags & 0x0400u) != 0u || monster->fx_special == 0u) {
        return monster->fx_base;
    }
    roll = yt2_create_rand(&room->rng_seed, room->time_word, 100u);
    if (roll >= 0x19u) {
        return monster->fx_base;
    }
    room->f5064 |= 0x0200u;
    return monster->fx_special;
}

/* fn_0AEE9: the item damage die (ax = item id). Classed items
   (missile class nonzero) return 0 with the class kept; the
   rest read the param row (+8, or +4 under def 0xC000). */
static unsigned int combat_item_die(yt2_combat *combat, unsigned int id,
                                    unsigned int *cls)
{
    const uint8_t *def;
    const uint8_t *row;
    unsigned int kind;

    if (cls != NULL) {
        *cls = 0u;
    }
    kind = combat_missile_class(combat, id);
    if (kind != 0u) {
        if (cls != NULL) {
            *cls = kind;
        }
        return 0u;
    }
    if (combat_item(combat, id, &def, &row) != YT2_STATUS_OK ||
        row == NULL) {
        return 0u;
    }
    if ((combat_read16(def + 12) & 0xC000u) != 0u) {
        return combat_read16(row + 4);
    }
    return combat_read16(row + 8);
}

/* sub_06C82: stage one monster-vs-member damage slot (v_50A2).
   The 0xE00 arm (monster +0x92 effect flag) saves vs half the
   member's +0x58 and stages the +0x92-selected fumble kind with
   the fn_0AEE9 die; the 0x200 arm (special rolled) saves vs the
   full +0x58 and stages the +0x8E/+0x90 offset pair; the plain
   arm rolls fn_15D89 (member absorption vs monster +0x54/+0x5A)
   and stages damage. Unstaged (miss/save-made) leaves v_0C36 0.
   v_0C34 (status) stages 0; the save in sub_083FA fills it. */
static void combat_monster_calc(yt2_combat *combat, yt2_room_object *monster,
                                yt2_character *member, int assigned)
{
    yt2_room *room = combat->room;
    yt2_combat_damage *slot;
    unsigned int save;

    if (assigned < 0 || assigned >= (int)YT2_COMBAT_DAMAGE_SLOTS) {
        return;
    }
    slot = &combat->damage_slots[assigned];
    slot->effect = 0u;
    slot->member = -1;
    slot->status = 0u;
    slot->amount = 0u;
    slot->aux = 0u;
    slot->staged = 0;
    if ((room->f5064 & 0x0200u) == 0u) {
        /* Plain arm (loc_06CB0): margin roll plus percentile. */
        combat_scale(combat, member->current.combat.absorption,
                     monster->attack_base, monster->attack_mult);
        if (combat->damage_roll == 0u) {
            return;
        }
        slot->effect = combat->effect_id;
        slot->member = assigned;
        slot->amount = combat->damage_roll;
        slot->staged = 1;
        return;
    }
    if ((monster->group & 0x0E00u) == 0u) {
        /* Special arm (loc_06CEC): save vs survival, then the
           +0x8E/+0x90 offset pair stages (0 shipped). */
        combat->save_power = monster->power;
        save = chest_skill_roll(combat, member->level, monster->power,
                                member->current.skills.survival);
        if (save == 0u) {
            return;
        }
        slot->effect = combat->effect_id;
        slot->member = assigned;
        slot->amount = monster->fx_off;
        slot->aux = monster->fx_cap;
        slot->staged = 1;
        combat->damage_roll = 1u;
        return;
    }
    /* Item-fumble arm (loc_06D4D): save vs halved survival, then
       the member's item at the +0x92-selected slot (0x13A/0x142/
       0x146) feeds the fn_0AEE9 die; a zero die misses (status
       cleared), else the kind/item/die stage. */
    combat->save_power = monster->power;
    save = chest_skill_roll(combat, member->level, monster->power,
                            (uint16_t)(member->current.skills.survival >> 1));
    if (save == 0u) {
        return;
    }
    {
        unsigned int kind;
        unsigned int id;
        unsigned int die;

        if ((monster->group & 0x0800u) != 0u) {
            kind = 0x13Au;
            id = member->equipment[0].id;
        } else if ((monster->group & 0x0400u) != 0u) {
            kind = 0x142u;
            id = member->equipment[2].id;
        } else {
            kind = 0x146u;
            id = member->equipment[3].id;
        }
        die = combat_item_die(combat, id, NULL);
        if (die == 0u) {
            slot->status = 0u;
            return;
        }
        slot->effect = combat->effect_id;
        slot->member = assigned;
        slot->status = (uint16_t)kind;
        slot->amount = (uint16_t)id;
        slot->aux = (uint16_t)die;
        slot->staged = 1;
        combat->damage_roll = 1u;
    }
}

/* Clear the four damage slots (fn_08120's zeroing). */
static void combat_slots_clear(yt2_combat *combat)
{
    for (unsigned int i = 0u; i < YT2_COMBAT_DAMAGE_SLOTS; i++) {
        combat->damage_slots[i].effect = 0u;
        combat->damage_slots[i].member = -1;
        combat->damage_slots[i].status = 0u;
        combat->damage_slots[i].amount = 0u;
        combat->damage_slots[i].aux = 0u;
        combat->damage_slots[i].staged = 0;
    }
}

/*
 * sub_0690D: the formal monster turn. Ticks damage-over-time
 * (a kill aborts into the scan), acks tagged statuses, poses
 * the attack stance over three presents, then resolves: the
 * 0x1000 multi arm swings at every live member, the single arm
 * at the queued target (skipping status-0x1C40 members), each
 * through the effect roll, the damage calc and the shared
 * applier, with the +0x5C hit / +0x5E miss SFX. A miss idles
 * (B1's 12-tick delay is pacing, skipped). The single-arm tail
 * wears the member's spell slot (fn_0AD9D 0x146, result
 * ignored) unless a special rolled. B1's mid-turn key abort
 * stays out (B2 takes input between turns, never mid-turn).
 */
/* The miss leg (loc_069F2): the +0x5E SFX when nonzero and the
   driver idle (always here); silence idles 12 ticks in B1. */
static void combat_monster_miss(yt2_combat *combat, yt2_room_object *obj)
{
    if (obj->sfx_miss != 0u) {
        combat_sfx(combat, obj->sfx_miss);
    }
}

static void combat_monster_turn(yt2_combat *combat, unsigned int engaged,
                                yt2_room_present_fn present, void *context)
{
    yt2_room *room = combat->room;
    yt2_party *party = room->party;
    yt2_room_object *obj = &room->near_objects[engaged];

    if (obj->id == 0u) {
        return;
    }
    if (combat_dot_tick(obj) != 0u) {
        return;
    }
    if ((obj->flags & 0xF010u) != 0u) {
        obj->flags |= 0x20u;
        yt2_combat_repaint(combat, present, context);
        combat_delay(combat, 10u);
    }
    obj->flags |= 0x04u;
    for (unsigned int i = 0u; i < 3u; i++) {
        yt2_combat_repaint(combat, present, context);
        /* B1 paces poses by render time alone; the principled
           beat is 2 per frame (one attack phrase). */
        combat_delay(combat, 2u);
    }
    combat->effect_id = combat_effect_roll(combat, obj);
    combat_slots_clear(combat);
    combat->damage_roll = 0u;
    if ((obj->group & 0x1000u) != 0u) {
        /* Multi arm (loc_06A18): every live member, one SFX. */
        int any = 0;

        for (unsigned int i = 0u; i < YT2_COMBAT_ATTACKERS_MAX; i++) {
            unsigned int slot = party->assigned[i];
            yt2_character *member;

            if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
                continue;
            }
            member = &party->roster[slot - 1u];
            if ((member->status & 0x1C40u) != 0u) {
                continue;
            }
            combat_monster_calc(combat, obj, member, (int)i);
            if (combat->damage_roll == 0u) {
                continue;
            }
            if (!any) {
                any = 1;
                if (obj->sfx_hit != 0u) {
                    combat_sfx(combat, obj->sfx_hit);
                }
            }
        }
        if (!any) {
            combat_monster_miss(combat, obj);
        } else {
            combat->save_power = obj->power;
            combat_apply_damage(combat, present, context);
        }
    } else {
        /* Single arm (loc_06982): the queued target (sub_06DD9
           resolve by roster slot, v_5070 context via the roster
           helper). */
        yt2_character *member = NULL;
        int assigned = -1;

        if (obj->target != 0u) {
            member = combat_roster(combat, obj->target);
            assigned = combat_member_resolve(combat, member);
        }
        if (member == NULL || assigned < 0 ||
            (member->status & 0x1C40u) != 0u) {
            combat_monster_miss(combat, obj);
        } else {
            combat_monster_calc(combat, obj, member, assigned);
            if (combat->damage_roll == 0u) {
                combat_monster_miss(combat, obj);
            } else {
                if (obj->sfx_hit != 0u) {
                    combat_sfx(combat, obj->sfx_hit);
                }
                combat->save_power = obj->power;
                combat_apply_damage(combat, present, context);
                if ((room->f5064 & 0x0200u) == 0u) {
                    /* Spell-slot wear (result ignored). */
                    (void)combat_weapon_cc(combat, member,
                                           COMBAT_WEAR_SPELL, assigned, NULL,
                                           present, context);
                }
            }
        }
    }
    obj->flags &= (uint16_t)~0x04u;
    obj->pic = obj->anim_base;
    yt2_combat_repaint(combat, present, context);
}

/*
 * fn_06382 for one member key: run the round (build the queue on
 * entry), serve the current member turn with the key, scan, and
 * either advance, wait for the next member, or finish victorious.
 * Monster melee damage and the C/P member verbs are deferred;
 * the matrix only verbs A.
 */
static void combat_formal_key(yt2_combat *combat, unsigned int ascii,
                             yt2_room_present_fn present, void *context)
{
    yt2_room *room = combat->room;

    if ((room->f5064 & 0x1000u) == 0u) {
        return;
    }
    if (combat->queue_count == 0u) {
        /* First formal key builds the queue (consumes the pick
           roll) even when the key itself is not a verb. */
        combat_build_queue(combat);
    }
    if (ascii != 'A' && (ascii < '1' || ascii > '4')) {
        /* Non-verbs re-poll (C/P are deferred slices; D nests the
           menu in the caller and never reaches here). */
        return;
    }
    if (ascii >= '1' && ascii <= '4') {
        /* Member stats paints in the caller (fn_16044); a direct
           call re-polls. */
        return;
    }
    for (;;) {
        yt2_combat_entry *entry;
        unsigned int code;
        unsigned int i;
        int have_member;

        while (combat->queue_pos < combat->queue_count &&
               (combat->queue[combat->queue_pos].flags & 0x4000u) != 0u) {
            combat->queue_pos++;
        }
        if (combat->queue_pos >= combat->queue_count) {
            combat->queue_count = 0u;
            combat_build_queue(combat);
            /* A memberless rebuild (TPK: B1's game-over owns it)
               waits instead of spinning the queue forever. */
            have_member = 0;
            for (i = 0u; i < combat->queue_count; i++) {
                if (!combat->queue[i].monster) {
                    have_member = 1;
                    break;
                }
            }
            if (!have_member) {
                return;
            }
            continue;
        }
        entry = &combat->queue[combat->queue_pos];
        if (entry->monster) {
            /* Monsters never poll: act, then rejoin the member
               wait below (a DoT kill still scans). */
            combat_monster_turn(combat, entry->index, present, context);
            combat->queue_pos++;
            code = combat_scan(combat);
            if (code == 0u) {
                room->f5064 &= 0x0FFFu;
                combat_panel_restore_formal(combat);
                yt2_combat_panel_paint(combat, combat->surface);
                combat_finish_award(combat);
                room->f5062 |= 0x20u;
                combat->queue_count = 0u;
                combat->queue_pos = 0u;
                combat->target = -1;
                yt2_room_monster_turn(room);
                return;
            }
            if (code == 1u) {
                combat->queue_count = 0u;
                combat->queue_pos = 0u;
                return;
            }
            while (combat->queue_pos < combat->queue_count &&
                   (combat->queue[combat->queue_pos].flags & 0x4000u) !=
                       0u) {
                combat->queue_pos++;
            }
            if (combat->queue_pos < combat->queue_count &&
                !combat->queue[combat->queue_pos].monster) {
                if (combat_next_hero(combat,
                        combat->queue[combat->queue_pos].index,
                        present, context)) {
                    return;
                }
                combat->queue_pos++;
            }
            continue;
        }
        if (!combat_next_hero(combat, entry->index, present,
                                   context)) {
            combat->queue_pos++;
            continue;
        }
        combat_melee(combat, present, context);
        combat->queue_pos++;
        code = combat_scan(combat);
        if (code == 0u) {
            room->f5064 &= 0x0FFFu;
            combat_panel_restore_formal(combat);
            yt2_combat_panel_paint(combat, combat->surface);
            combat_finish_award(combat);
            room->f5062 |= 0x20u;
            combat->queue_count = 0u;
            combat->queue_pos = 0u;
            combat->target = -1;
            /* Main-loop 0x20 branch settles before quiescence. */
            yt2_room_monster_turn(room);
            return;
        }
        /*
         * code 1 ends the round (rebuild next key; B1 also runs
         * the explore monster turn and monster missiles between
         * formal rounds, deferred with the missiles slice); code
         * 2 with a member next waits for its key, with a monster
         * next the loop advances at once above.
         */
        if (code == 1u) {
            combat->queue_count = 0u;
            combat->queue_pos = 0u;
            return;
        }
        if (combat->queue_pos < combat->queue_count &&
            !combat->queue[combat->queue_pos].monster) {
            if (combat_next_hero(combat,
                    combat->queue[combat->queue_pos].index,
                    present, context)) {
                return;
            }
            combat->queue_pos++;
            continue;
        }
    }
}

void yt2_combat_menu_open(yt2_combat *combat)
{
    if (combat == NULL) {
        return;
    }
    combat->award_pending = 0;
    combat->chest_side_panel_lines = 0u;
}

static int combat_repaint_impl(yt2_combat *combat,
                               yt2_room_present_fn present, void *context,
                               int portraits)
{
    yt2_room *room;

    if (combat == NULL || combat->room == NULL) {
        return YT2_STATUS_ERROR;
    }
    room = combat->room;
    /* Combat rounds keep the stale row (B1 paints sprites
       incrementally); explore returns repaint it fresh. Under
       the open chest the stale row is the entry compass, which
       the fresh paint reproduces (no moves under the modal). */
    room->no_compass = (room->f5064 & 0x1000u) != 0u ||
                       combat->chest_prompt ||
                       combat->award_pending ||
                       (!combat->chest_open &&
                        !combat->chest_compass_fresh);
    if (combat->chest_open || combat->chest_prompt ||
        combat->chest_paint_stale) {
        /* fn_10E6A: modal repaints reuse the retained view line
           (the open-modal ++ above survives until close). */
        yt2_room_render_stale(room, combat->surface);
    } else if (portraits) {
        yt2_room_render(room, combat->surface);
    } else {
        yt2_room_render_keep_portraits(room, combat->surface);
    }
    room->no_compass = 0;
    if ((room->f5064 & 0x1000u) != 0u) {
        /* fn_135B2's fn_06EA4 consumes the pending side panel (loot
           from a victory that re-engaged in its own tail) before
           the names paint: never loot plus names together. */
        combat->award_pending = 0;
        /* fn_1755C + fn_1261F + the fn_06EA4 formal arm: the pack
           grid, the swords icon, and the extended side panel clear
           (x 240-312, y 86-194, which wipes the arrows). The
           engaged names paint over the clear. */
        quick_access_paint(combat);
        combat_paint_swords(combat);
        chest_fill(combat, 240, 86, 73, 109);
        room->f5062 &= (uint16_t)~0x1000u;
        combat_paint_engaged(combat);
        if (portraits && combat->queue_count > 0u) {
            combat_paint_highlight(combat);
        }
    }
    if (combat->award_pending) {
        combat_paint_award(combat, combat->last_award);
    }
    if (combat->chest_open) {
        /* Rows 96+ wipe every modal paint (B1's side panel clear); the
           compass row wipes only when the action flagged it
           (prompt, instant take, drop). Cursor takes keep it. */
        chest_side_panel_clear(combat, combat->chest_side_panel_compass);
        chest_paint_loot(combat);
        quick_access_paint(combat);
    }
    if (combat->chest_side_panel_lines > 0u) {
        chest_side_panel_paint(combat);
    }
    if (!combat->chest_open && !combat->chest_prompt &&
        !combat->chest_paint_stale) {
        /* B1 keeps no side panel text across gathers (fn_135B2's
           fn_06EA4 wipes it for the names): a gather consumes
           the pending lines instead of resurrecting them over
           the next repaint. Stale/modal repaints retain them. */
        combat->chest_side_panel_lines = 0u;
    }
    combat->chest_side_panel_compass = 0u;
    combat->chest_compass_fresh = 0u;
    combat->chest_paint_stale = 0u;
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

int yt2_combat_repaint(yt2_combat *combat, yt2_room_present_fn present,
                       void *context)
{
    return combat_repaint_impl(combat, present, context, 1);
}

int yt2_combat_repaint_keep_portraits(yt2_combat *combat,
                                      yt2_room_present_fn present,
                                      void *context)
{
    return combat_repaint_impl(combat, present, context, 0);
}

/*
 * Chest UI (SPACE search/open/loot/close). Decoded against the C/D
 * matrix prefixes; table coordinates below are the B1 DGROUP tables
 * (0x5ABC regions, 0x63CE loot cells, 0x6372 quick access cells, 0x61C8
 * portraits), read-only in the binary.
 */

/* fn_0D1DE box: x0 <= x <= x1, y0 <= y <= y1 (inclusive). */
typedef struct combat_box {
    int x0;
    int x1;
    int y0;
    int y1;
    unsigned int id;
} combat_box;

/* Modal regions (0x5ABC): 1 viewport, 2 quick access pane, 3 icon row,
   4 compass/message, 5 loot grid, 6 bottom strip. */
static const combat_box chest_regions[] = {
    {8, 231, 8, 143, 1},
    {240, 311, 8, 63, 2},
    {239, 311, 67, 82, 3},
    {240, 312, 96, 156, 4},
    {240, 312, 160, 194, 5},
    {0, 232, 144, 199, 6},
};

/* Loot cells (0x63CE): 4x2, ids 1..8. */
static const combat_box chest_loot_cells[] = {
    {241, 257, 160, 176, 1},
    {259, 275, 160, 176, 2},
    {277, 293, 160, 176, 3},
    {295, 311, 160, 176, 4},
    {241, 257, 179, 195, 5},
    {259, 275, 179, 195, 6},
    {277, 293, 179, 195, 7},
    {295, 311, 179, 195, 8},
};

/* Arrow buttons (0x6420): turn/step/turn, strafe/back/strafe. */
static const combat_box chest_arrows[] = {
    {246, 261, 161, 177, 1},
    {268, 284, 161, 177, 2},
    {291, 307, 161, 177, 3},
    {246, 261, 178, 194, 4},
    {268, 284, 178, 194, 5},
    {291, 307, 178, 194, 6},
};

/* Formal left-click regions (0x669E): viewport, quick access grid, icon
   row, extended side panel, dead arrows box (region 4 contains it and
   fn_0D1DE takes the first match), bottom strip. */
static const combat_box formal_regions[] = {
    {8, 231, 8, 143, 1},
    {240, 311, 8, 63, 2},
    {239, 311, 67, 82, 3},
    {240, 313, 86, 195, 4},
    {240, 312, 160, 194, 5},
    {0, 232, 144, 199, 6},
};

/* Verb buttons (0x5B2E boxes, shared row): explore bow S, spell
   cast + ZZZ rest deferred, disk menu; formal swords A, spell/ZZZ
   silent, disk menu. */
static const combat_box formal_verbs[] = {
    {239, 254, 67, 82, 1},
    {258, 273, 67, 82, 2},
    {277, 292, 67, 82, 3},
    {296, 311, 67, 82, 4},
};

/* Engaged target rows (0x6606): name cell + four status cells per
   record (ids 1-5, 6-10, 11-15). */
static const combat_box formal_side_panel[] = {
    {240, 313, 86, 98, 1},
    {240, 286, 99, 107, 2},
    {287, 295, 99, 107, 3},
    {296, 304, 99, 107, 4},
    {305, 313, 99, 107, 5},
    {240, 313, 122, 134, 6},
    {240, 286, 135, 143, 7},
    {287, 295, 135, 143, 8},
    {296, 304, 135, 143, 9},
    {305, 313, 135, 143, 10},
    {240, 313, 158, 170, 11},
    {240, 286, 171, 179, 12},
    {287, 295, 171, 179, 13},
    {296, 304, 171, 179, 14},
    {305, 313, 171, 179, 15},
};

/* Quick access cells (0x6372): 3x3, ids 1..9 (row 0 currencies). */
static const combat_box quick_access_cells[] = {
    {249, 264, 10, 26, 1},
    {268, 283, 10, 26, 2},
    {287, 302, 10, 26, 3},
    {249, 264, 28, 43, 4},
    {268, 283, 28, 43, 5},
    {287, 302, 28, 43, 6},
    {249, 264, 46, 61, 7},
    {268, 283, 46, 61, 8},
    {287, 302, 46, 61, 9},
};

/* Portraits (0x61C8): ids 1/11/21/31 pick members 1..4. */
static const combat_box chest_portraits[] = {
    {8, 39, 148, 179, 1},
    {66, 96, 148, 179, 11},
    {124, 154, 148, 179, 21},
    {182, 212, 148, 179, 31},
};

/* Chest texts (DGROUP 0x7Bxx string table). */
#define CHEST_TEXT_NOTHING "NOTHING HERE"
#define CHEST_TEXT_OPEN0 "WHO WILL"
#define CHEST_TEXT_OPEN1 "OPEN?"
#define CHEST_TEXT_SEARCH1 "SEARCH?"
#define CHEST_TEXT_TRAP "TRAP "
#define CHEST_TEXT_DEFEATED "DEFEATED"
#define CHEST_TEXT_LOCKED "LOCKED"
#define CHEST_TEXT_NOT_LOCKED "NOT LOCKED"
#define CHEST_TEXT_BUT_TRAPPED "BUT TRAPPED"
#define CHEST_TEXT_LOCKED_AND "LOCKED AND"
#define CHEST_TEXT_TRAPPED "TRAPPED"
#define CHEST_TEXT_MAGICALLY "MAGICALLY"
#define CHEST_TEXT_REQUIRES "REQUIRES"
#define CHEST_TEXT_SPECIAL_KEY "SPECIAL KEY"

/* Side panel paint position and colors (v_50E6/v_50FE/v_0CC6). */
#define CHEST_SIDE_PANEL_X 240
#define CHEST_SIDE_PANEL_Y 96
#define CHEST_SIDE_PANEL_COLOR 0x8Au
#define CHEST_SIDE_PANEL_VALUE_COLOR 0x0Fu

static unsigned int chest_hit(const combat_box *boxes, unsigned int count,
                              int x, int y)
{
    for (unsigned int i = 0u; i < count; i++) {
        if (x >= boxes[i].x0 && x <= boxes[i].x1 &&
            y >= boxes[i].y0 && y <= boxes[i].y1) {
            return boxes[i].id;
        }
    }
    return 0u;
}

static uint16_t chest_read16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint16_t chest_def_flag(const uint8_t *def)
{
    return chest_read16(def + 12);
}

static uint16_t chest_def_icon(const uint8_t *def)
{
    return chest_read16(def + 8);
}

static uint16_t chest_def_weight(const uint8_t *def)
{
    return chest_read16(def + 10);
}

/* sub_11A93: interactable triple at (x, y), or id 0 for miss. */
static int chest_triple_at(yt2_combat *combat, unsigned int x, unsigned int y,
                           uint16_t *w1, uint16_t *w2)
{
    const uint8_t *bytes;
    size_t size;
    size_t ptr;
    size_t at;

    if (x < 0x28u || x > 0x2F7u || y < 0x18u || y > 0x77u) {
        return 0;
    }
    bytes = combat->world->bytes;
    size = combat->world->size;
    if (bytes == NULL ||
        size < YT2_CHEST_SEARCH_OFFSET + (size_t)(x - 0x28u) * 2u + 2u) {
        return 0;
    }
    ptr = (size_t)chest_read16(bytes + YT2_CHEST_SEARCH_OFFSET +
                               (size_t)(x - 0x28u) * 2u);
    at = YT2_CHEST_SEARCH_OFFSET + ptr;
    for (;;) {
        uint16_t ey;
        uint16_t ew1;
        uint16_t ew2;

        if (at + 6u > size) {
            return 0;
        }
        ey = chest_read16(bytes + at);
        ew1 = chest_read16(bytes + at + 2u);
        ew2 = chest_read16(bytes + at + 4u);
        if (ey == 0xFFFFu || y < ey) {
            return 0;
        }
        if (y == ey) {
            *w1 = ew1;
            *w2 = ew2;
            return 1;
        }
        at += 6u;
    }
}

/* fn_11A11: search the party cell, then the faced cell. Returns
   1/2 for a hit (v_50CC), 0 for miss. */
static unsigned int chest_search(yt2_combat *combat, uint16_t *w1,
                                 uint16_t *w2)
{
    yt2_party *party = combat->room->party;
    unsigned int x = party->x;
    unsigned int y = party->y;

    if (chest_triple_at(combat, x, y, w1, w2)) {
        return 1u;
    }
    if ((party->facing & 0x8000u) != 0u) {
        y--;
    } else if ((party->facing & 0x4000u) != 0u) {
        y++;
    } else if ((party->facing & 0x2000u) != 0u) {
        x--;
    } else {
        x++;
    }
    if (chest_triple_at(combat, x, y, w1, w2)) {
        return 2u;
    }
    return 0u;
}

/* fn_07727: load chest record w2 (1-based) and its taken mask. */
static int chest_load(yt2_combat *combat, uint16_t id)
{
    const uint8_t *bytes;
    size_t size;
    size_t at;
    uint16_t index;

    if (id == 0u) {
        return YT2_STATUS_ERROR;
    }
    index = (uint16_t)(id - 1u);
    bytes = combat->world->bytes;
    size = combat->world->size;
    at = YT2_CHEST_RECORDS_OFFSET + (size_t)index * YT2_CHEST_RECORD_SIZE;
    if (bytes == NULL || at + YT2_CHEST_RECORD_SIZE > size) {
        return YT2_STATUS_ERROR;
    }
    memcpy(combat->chest_record, bytes + at, YT2_CHEST_RECORD_SIZE);
    combat->chest_index = index;
    /* Taken read (index) then unlock read (index/8): last wins. */
    combat->chest_save_index = (uint16_t)(index / 8u);
    combat->chest_taken = 0u;
    if (combat->save != NULL && combat->save->bytes != NULL &&
        YT2_CHEST_TAKEN_OFFSET + (size_t)index < combat->save->size) {
        combat->chest_taken =
            combat->save->bytes[YT2_CHEST_TAKEN_OFFSET + (size_t)index];
    }
    return YT2_STATUS_OK;
}

/* sub_07A66: stage the 8 loot slots (taken-gated, MSB first). */
static void chest_stage(yt2_combat *combat)
{
    uint16_t bits = combat->chest_taken;
    unsigned int slot;

    for (slot = 0u; slot < YT2_CHEST_SLOTS; slot++) {
        uint16_t id;
        const uint8_t *def;
        const uint8_t *row;

        combat->chest_staging[slot].id = 0u;
        combat->chest_staging[slot].count = 0u;
        bits <<= 1;
        if ((bits & 0x100u) != 0u) {
            continue;
        }
        id = chest_read16(combat->chest_record + 4u + slot * 2u);
        if (id == 0u) {
            continue;
        }
        combat->chest_staging[slot].id = id;
        if (id == 1u) {
            combat->chest_staging[slot].count =
                chest_read16(combat->chest_record + 0x14u);
        } else if (id == 2u) {
            combat->chest_staging[slot].count =
                chest_read16(combat->chest_record + 0x16u);
        } else if (id == 3u) {
            combat->chest_staging[slot].count =
                chest_read16(combat->chest_record + 0x18u);
        } else if (combat_item(combat, id, &def, &row) == YT2_STATUS_OK &&
                   row != NULL && (chest_def_flag(def) & 0x100u) != 0u) {
            uint16_t sub = chest_read16(row + 2u);

            /* fn_02262's 0x100-selected row: [bx+2]&1 keeps [bx+4]. */
            if ((sub & 1u) != 0u) {
                combat->chest_staging[slot].count =
                    chest_read16(row + 4u);
            }
        }
    }
}

/* Fill a surface rect (fn_06EA4/sub_07342 0x0404 clears). */
static void chest_fill(yt2_combat *combat, int x, int y, int w, int h)
{
    yt2_surface *surface = combat->surface;

    for (int row = 0u; row < h; row++) {
        for (int col = 0u; col < w; col++) {
            int px = x + col;
            int py = y + row;

            if (px >= 0 && py >= 0 && (unsigned int)px < surface->width &&
                (unsigned int)py < surface->height) {
                surface->pixels[(size_t)py * surface->stride + (size_t)px] =
                    4u;
            }
        }
    }
}

/* Side panel clear (rows 96+) with the optional compass row. B1 splits
   this across fn_06EA4 (v_505E bit 0x100 gate) and fn_06F86 (VGA
   direct); B2's single surface takes the per-action flag. */
static void chest_side_panel_clear(yt2_combat *combat, int compass)
{
    if (compass) {
        chest_fill(combat, 241, 87, 72, 6);
    }
    chest_fill(combat, 240, 96, 72, 60);
    combat->room->f5062 &= (uint16_t)~0x1000u;
}

/* Paint the staged side panel lines (prompt/take/trap/currency). */
static void chest_side_panel_paint(yt2_combat *combat)
{
    yt2_surface *surface = combat->surface;

    chest_side_panel_clear(combat, combat->chest_side_panel_compass);
    for (unsigned int i = 0u; i < combat->chest_side_panel_lines; i++) {
        yt2_text_draw(surface, CHEST_SIDE_PANEL_X, combat->chest_side_panel_row[i],
                      combat->chest_side_panel[i], combat->chest_side_panel_color[i]);
    }
}

/*
 * Member inspect pages (fn_16044 over the bottom strip, keys 1-4,
 * formal F-keys via sub_09A1D). Strip cells 0-2 open afflictions,
 * 3 training, 4 protections, 5 transports, 6 stats; keys always
 * open stats. Pages are painted pixels only: the next repaint wipes
 * them, exactly like vanilla (no page state anywhere).
 */
#define MEMBER_PAGE_X 241
#define MEMBER_PAGE_NAME_Y 87
#define MEMBER_PAGE_TITLE_Y 96
#define MEMBER_PAGE_ROW_Y 102
#define MEMBER_PAGE_TRAIN_VALUE_X 283
#define MEMBER_PAGE_PROT_VALUE_X 295

/* fn_08070 + fn_15A06: thousands comma, then spaces stripped. */
static void member_commas(uint16_t value, char *out)
{
    unsigned int digits[5];
    unsigned int pos = 0u;
    unsigned int i;

    digits[0] = value / 10000u;
    digits[1] = (value / 1000u) % 10u;
    digits[2] = (value / 100u) % 10u;
    digits[3] = (value / 10u) % 10u;
    digits[4] = value % 10u;
    /* Leading-zero suppression with one thousands comma. */
    for (i = 0u; i < 5u; i++) {
        unsigned int place = 1u;

        if (i == 0u) {
            place = 10000u;
        } else if (i == 1u) {
            place = 1000u;
        } else if (i == 2u) {
            place = 100u;
        } else if (i == 3u) {
            place = 10u;
        }
        if (i == 2u && value >= 1000u) {
            out[pos++] = ',';
        }
        if (value >= place || i == 4u) {
            out[pos++] = (char)('0' + digits[i]);
        }
    }
    out[pos] = '\0';
}

/* fn_08070 + fn_15A22: plain digits, no comma. */
static void member_plain(uint16_t value, char *out)
{
    unsigned int divisor = 10000u;
    unsigned int pos = 0u;
    int started = 0;

    while (divisor > 0u) {
        unsigned int digit = (value / divisor) % 10u;

        if (digit != 0u || started || divisor == 1u) {
            out[pos++] = (char)('0' + digit);
            started = 1;
        }
        divisor /= 10u;
    }
    out[pos] = '\0';
}

/* fn_062EE with v_0C46 = 1: decimal point before the last digit
   (tenths; small values keep vanilla's leading dot: ".5"). */
static void member_tenths(uint16_t value, char *out)
{
    char plain[8];
    unsigned int len = 0u;

    member_plain(value, plain);
    while (plain[len] != '\0') {
        len++;
    }
    for (unsigned int i = 0u; i < len; i++) {
        out[i + 1u] = plain[i];
    }
    out[len + 1u] = '\0';
    out[0] = '.';
    /* Shift the dot left past all but the last digit. */
    for (unsigned int i = 0u; i < len - 1u; i++) {
        out[i] = out[i + 1u];
    }
    out[len - 1u] = '.';
}

/* sub_1636E: cur/max pair joined with "/". */
static void member_pair(uint16_t cur, uint16_t max, int decimal, char *out)
{
    char first[8];
    char second[8];
    unsigned int pos = 0u;

    if (decimal) {
        member_tenths(cur, first);
        member_tenths(max, second);
    } else {
        member_commas(cur, first);
        member_commas(max, second);
    }
    for (unsigned int i = 0u; first[i] != '\0'; i++) {
        out[pos++] = first[i];
    }
    out[pos++] = '/';
    for (unsigned int i = 0u; second[i] != '\0'; i++) {
        out[pos++] = second[i];
    }
    out[pos] = '\0';
}

/* sub_163E1: the member name at (241,87). Vanilla also prints a
   12-space blank first; the side panel pre-clear makes it a no-op. */
static void member_header(yt2_combat *combat, const yt2_character *member)
{
    yt2_text_draw(combat->surface, MEMBER_PAGE_X, MEMBER_PAGE_NAME_Y,
                  member->name, 0x8Au);
}

/* sub_16292: status-bit afflictions, display order. */
static void member_afflictions(yt2_combat *combat,
                               const yt2_character *member)
{
    static const struct {
        uint16_t bit;
        const char *label;
    } rows[] = {
        {0x2000u, "DISEASED"}, {0x4000u, "POISONED"}, {0x8000u, "SICK"},
        {0x0400u, "STONED"}, {0x0800u, "FROZEN"}, {0x1000u, "PARALYZED"},
        {0x0080u, "CURSED"}, {0x0100u, "HEXED"}, {0x0200u, "JINXED"},
    };
    yt2_surface *surface = combat->surface;
    int y = MEMBER_PAGE_ROW_Y;

    member_header(combat, member);
    yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_TITLE_Y, "AFFLICTIONS:",
                  0xAAu);
    if ((member->status & 0xFF80u) == 0u) {
        yt2_text_draw(surface, MEMBER_PAGE_X, y, "NONE", 0x0Fu);
        return;
    }
    for (unsigned int i = 0u;
         i < sizeof(rows) / sizeof(rows[0]); i++) {
        if ((member->status & rows[i].bit) != 0u) {
            yt2_text_draw(surface, MEMBER_PAGE_X, y, rows[i].label, 0x0Fu);
            y += 6;
        }
    }
}

/* fn_16171: current and ready-for levels. */
static void member_training(yt2_combat *combat, const yt2_character *member)
{
    yt2_surface *surface = combat->surface;
    char number[8];

    member_header(combat, member);
    yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_TITLE_Y, "TRAINING:",
                  0xAAu);
    yt2_text_draw(surface, MEMBER_PAGE_X, 102, "CURRENTLY", 0x8Au);
    yt2_text_draw(surface, MEMBER_PAGE_X, 108, "LEVEL", 0x8Au);
    member_plain(member->level, number);
    yt2_text_draw(surface, MEMBER_PAGE_TRAIN_VALUE_X, 108, number, 0x8Au);
    if (member->ready != 0u) {
        yt2_text_draw(surface, MEMBER_PAGE_X, 114, "READY FOR", 0x0Fu);
        yt2_text_draw(surface, MEMBER_PAGE_X, 120, "LEVEL", 0x0Fu);
        member_plain(member->ready, number);
        yt2_text_draw(surface, MEMBER_PAGE_TRAIN_VALUE_X, 120, number,
                      0x0Fu);
    }
}

/* sub_164DD: nine buff counters beside their names. */
static void member_protections(yt2_combat *combat,
                               const yt2_character *member)
{
    static const char *names[9] = {
        "DISEASE", "POISON", "SICKNESS", "STONING", "FROZEN", "PARALYZE",
        "CURSING", "HEXING", "JINXING",
    };
    yt2_surface *surface = combat->surface;

    member_header(combat, member);
    yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_TITLE_Y, "PROTECTIONS:",
                  0xAAu);
    for (unsigned int i = 0u; i < 9u; i++) {
        char number[8];

        yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_ROW_Y + (int)i * 6,
                      names[i], 0x0Fu);
        member_plain(member->buffs[i], number);
        yt2_text_draw(surface, MEMBER_PAGE_PROT_VALUE_X,
                      MEMBER_PAGE_ROW_Y + (int)i * 6, number, 0x0Fu);
    }
}

/*
 * fn_1620A: owned rides from the +0xB4 mask (bits 15..12 gate rows
 * 1..4). The 0x77CC table holds the names, uses (+0x16) and time
 * flags (+0x18 bit 0 anytime-at-night, bit 1 anytime-by-day);
 * v_94F1 0x1A4..0x474 is day. An exhausted counter greys the row.
 * Vanilla also sets f5062 bit 0x1000 here (the quick-access-open flag).
 */
static void member_transports(yt2_combat *combat, const yt2_character *member)
{
    static const struct {
        const char *name;
        uint16_t uses;
        uint16_t flags;
    } rides[4] = {
        {"PEGASUS", 1u, 3u},
        {"GIANT EAGLE", 2u, 3u},
        {"@FLYING RUG", 4u, 2u},
        {"MAGIC DRAGON", 4u, 1u},
    };
    yt2_surface *surface = combat->surface;
    yt2_room *room = combat->room;
    uint16_t mask = member->icon_sel;
    int y = MEMBER_PAGE_ROW_Y;

    if (mask == 0u) {
        /* B1 returns before the header on an empty mask: the
           cleared side panel stays fully blank. */
        return;
    }
    member_header(combat, member);
    yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_TITLE_Y, "TRANSPORTS:",
                  0xAAu);
    for (unsigned int i = 0u; i < 4u; i++) {
        uint8_t color = 0x0Fu;

        if (((mask >> (15u - i)) & 1u) == 0u) {
            continue;
        }
        if (member->transport[i] >= rides[i].uses) {
            color = 7u;
        } else if (room->shade_scene < 0x1A4u ||
                   room->shade_scene > 0x474u) {
            if ((rides[i].flags & 1u) == 0u) {
                color = 7u;
            }
        } else if ((rides[i].flags & 2u) == 0u) {
            color = 7u;
        }
        yt2_text_draw(surface, MEMBER_PAGE_X, y, rides[i].name, color);
        y += 6;
    }
    room->f5062 |= 0x1000u;
}

/* sub_16420: health/magic/weight labels with their cur/max pairs
   in the same side panel column (B1 never moves x off 241; 0x0F is the
   ink, not the column). */
static void member_stats(yt2_combat *combat, const yt2_character *member)
{
    yt2_surface *surface = combat->surface;
    char pair[24];

    member_header(combat, member);
    yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_TITLE_Y, "HEALTH:",
                  member->current.combat.hit_points >
                          member->maximum.combat.hit_points
                      ? 0x5Bu
                      : 0x59u);
    yt2_text_draw(surface, MEMBER_PAGE_X, 114, "MAGIC:",
                  member->current.combat.magic_points >
                          member->maximum.combat.magic_points
                      ? 0xCCu
                      : 0xCAu);
    yt2_text_draw(surface, MEMBER_PAGE_X, 132, "WEIGHT:",
                  member->current.combat.carry_capacity >
                          member->maximum.combat.carry_capacity
                      ? 0x88u
                      : 0x86u);
    if ((member->status & 0x40u) != 0u) {
        yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_ROW_Y, "DEAD",
                      0x0Fu);
    } else {
        member_pair(member->current.combat.hit_points,
                    member->maximum.combat.hit_points, 0, pair);
        yt2_text_draw(surface, MEMBER_PAGE_X, MEMBER_PAGE_ROW_Y, pair,
                      0x0Fu);
    }
    member_pair(member->current.combat.magic_points,
                member->maximum.combat.magic_points, 0, pair);
    yt2_text_draw(surface, MEMBER_PAGE_X, 120, pair, 0x0Fu);
    member_pair(member->carried_weight,
                member->current.combat.carry_capacity, 1, pair);
    yt2_text_draw(surface, MEMBER_PAGE_X, 138, pair, 0x0Fu);
}

/*
 * fn_16044 page dispatch (0 afflictions, 1 training, 2
 * protections, 3 transports, 4 stats) for assigned member 1..4.
 * Unassigned members are a silent no-op; anything else paints and
 * presents with no repaint (the next repaint wipes the page).
 */
static int combat_member_page(yt2_combat *combat, unsigned int member,
                              unsigned int page,
                              yt2_room_present_fn present, void *context,
                              int key)
{
    yt2_room *room;
    yt2_character *character;
    unsigned int slot;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL) {
        return YT2_STATUS_ERROR;
    }
    room = combat->room;
    if (member == 0u || member > 4u || page > 4u) {
        return YT2_STATUS_OK;
    }
    slot = room->party->assigned[member - 1u];
    if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
        return YT2_STATUS_OK;
    }
    character = &room->party->roster[slot - 1u];
    /* fn_06EA4 side panel prep. The key path pre-sets v_505E bit
       0x100, so it always wipes; the mouse path consumes the
       bit first and skips the wipe when clear (painting over
       the stale row like B1). Either way the bit ends set.
       The explore arm clears the name row and rows 96+, the
       formal arm clears x 240-312, y 86-194. */
    if (key || (room->f505e & 0x100u) != 0u) {
        if ((room->f5064 & 0x1000u) != 0u) {
            chest_fill(combat, 240, 86, 73, 109);
            room->f5062 &= (uint16_t)~0x1000u;
        } else {
            chest_side_panel_clear(combat, 1);
        }
        room->f505e &= (uint16_t)~0x100u;
    }
    room->f505e |= 0x100u;
    if (page == 0u) {
        member_afflictions(combat, character);
    } else if (page == 1u) {
        member_training(combat, character);
    } else if (page == 2u) {
        member_protections(combat, character);
    } else if (page == 3u) {
        member_transports(combat, character);
    } else {
        member_stats(combat, character);
    }
    /* Pages are silent (B1's SFX 9 is a countdown blip gated
       on v_5064 bit 0x40, never set by the page paints). */
    combat->member_page = 1;
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

/* sub_09855: current value, tan when buffed past max. */
static void sheet_pair(yt2_combat *combat, int x, int y, uint16_t cur,
                       uint16_t max, uint8_t plain, uint8_t buffed)
{
    char number[8];

    member_plain(cur, number);
    yt2_text_draw(combat->surface, x, y, number,
                  cur > max ? buffed : plain);
}

/*
 * sub_0987F: the stat sheet, bare current values over the scene (no
 * labels, no backdrop): 6 attributes, battle acc/dam/absorption,
 * 8+5 skills in three columns. Rows past max print tan; the last
 * five skill rows print the highlight pair when this member holds
 * the party highlight slot.
 */
static void combat_paint_sheet(yt2_combat *combat,
                               const yt2_character *member,
                               unsigned int slot)
{
    static const unsigned int skill_x[2] = {116, 203};
    const uint16_t *cur = (const uint16_t *)&member->current;
    const uint16_t *max = (const uint16_t *)&member->maximum;
    yt2_party *party = combat->room->party;
    unsigned int i;

    /* Attributes: strength..charisma are the first six words. */
    for (i = 0u; i < 6u; i++) {
        sheet_pair(combat, 42, 37 + (int)i * 10, cur[i], max[i], 0x0Fu,
                   0x8Au);
    }
    /* Battle accuracy/damage/absorption (+0x4C words 8..10). */
    for (i = 0u; i < 3u; i++) {
        sheet_pair(combat, 42, 99 + (int)i * 10, cur[8u + i],
                   max[8u + i], 0x0Fu, 0x8Au);
    }
    /* Skills 0..7 at x 116, 8..12 at x 203 (+0x58/+0x68 words). */
    for (i = 0u; i < 13u; i++) {
        unsigned int col = i < 8u ? 0u : 1u;
        int y = 43 + (int)(i < 8u ? i : i - 8u) * 10;
        uint8_t plain = 0x0Fu;
        uint8_t buffed = 0x8Au;

        if (col == 1u && party->active[i - 8u] == (uint16_t)slot) {
            plain = 0xCBu;
            buffed = 0x9Bu;
        }
        sheet_pair(combat, (int)skill_x[col], y, cur[14u + i],
                   max[14u + i], plain, buffed);
    }
}

/*
 * sub_09A1D panel: the opaque working-set picture at (15,23)
 * plus the title row (class name, level, experience, all
 * 0x0F). Labels (LEVEL/DM/CLOSE/stats) ride in the art.
 */
static void combat_paint_sheet_panel(yt2_combat *combat,
                                     const yt2_character *member)
{
    uint8_t xp[4];
    char number[24];

    yt2_surface_blit(combat->surface, YT2_SHEET_ART_X, YT2_SHEET_ART_Y,
                     &yt2_sheet_art[0][0], YT2_SHEET_ART_W,
                     YT2_SHEET_ART_H);
    yt2_text_draw(combat->surface, 20, 27,
                  yt2_class_sheet_name((unsigned int)member->class_id),
                  0x0Fu);
    sheet_pair(combat, 122, 27, member->level, member->level, 0x0Fu, 0x8Au);
    /* fn_09C46 reads the BCD bytes in file order (most
       significant first). */
    xp[0] = (uint8_t)(member->experience & 0xFFu);
    xp[1] = (uint8_t)((member->experience >> 8) & 0xFFu);
    xp[2] = (uint8_t)((member->experience >> 16) & 0xFFu);
    xp[3] = (uint8_t)((member->experience >> 24) & 0xFFu);
    combat_bcd_format(xp, number, sizeof(number));
    yt2_text_draw(combat->surface, 160, 27, number, 0x0Fu);
}

/*
 * fn_09619 open/switch (loc_0969F): side panel stats plus the sheet
 * numbers plus the member border. Unassigned members are silent.
 */
static int combat_sheet_open(yt2_combat *combat, unsigned int member,
                             yt2_room_present_fn present, void *context)
{
    yt2_room *room;
    unsigned int slot;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL) {
        return YT2_STATUS_ERROR;
    }
    room = combat->room;
    if (member == 0u || member > 4u) {
        return YT2_STATUS_OK;
    }
    slot = room->party->assigned[member - 1u];
    if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
        return YT2_STATUS_OK;
    }
    combat_sfx(combat, 2u);
    if (combat_member_page(combat, member, 4u, NULL, NULL, 1) !=
        YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    combat_paint_sheet_panel(combat, &room->party->roster[slot - 1u]);
    combat_paint_sheet(combat, &room->party->roster[slot - 1u], slot);
    /* fn_171AE repaints the portraits (erasing the old border)
       before the new one. */
    yt2_room_paint_portraits(room->party, combat->pictures,
                             combat->surface);
    combat_paint_highlight_at(combat, member - 1u);
    combat->sheet_open = member;
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

/* loc_097D8: close the sheet (numbers stay stale, like vanilla). */
static int combat_sheet_close(yt2_combat *combat,
                              yt2_room_present_fn present, void *context)
{
    yt2_room *room;

    if (combat == NULL || combat->room == NULL) {
        return YT2_STATUS_ERROR;
    }
    room = combat->room;
    combat_sfx(combat, 2u);
    yt2_room_paint_portraits(room->party, combat->pictures,
                             combat->surface);
    /* fn_076AC: "$" plus the gold purse, gated on f5060 bit 0x80. */
    if ((room->f5060 & 0x80u) != 0u && room->party != NULL) {
        char purse[24];

        combat_bcd_format(room->party->gold, purse, sizeof(purse));
        yt2_text_draw(combat->surface, 241, 87, "$", 0x8Au);
        yt2_text_draw(combat->surface, 247, 87, purse, 0x8Au);
    }
    combat->sheet_open = 0u;
    if (combat->pack_open != 0u) {
        unsigned int i;

        /* Nested sheet: restore the stashed viewport (B1
           sub_092C2), then repaint the held panes. */
        pack_backdrop_blit(combat, 8, 8, 224u, 136u);
        for (i = 1u; i <= 4u; i++) {
            if ((combat->pack_open & (0x4000u >> (i - 1u))) != 0u) {
                pack_paint_member(combat, i);
            }
        }
    } else {
        /* The caller erases the panel with a stale viewport
           repaint (B1's fn_10E6A tail); the side panel keeps the
           stale page, the compass row stays untouched. */
        int no_compass = room->no_compass;

        room->no_compass = 1;
        yt2_room_render_stale(room, combat->surface);
        room->no_compass = no_compass;
    }
    combat->member_page = 1;
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

/* loc_097FA: toggle a party highlight for this member, if fit. */
static int combat_sheet_toggle(yt2_combat *combat, unsigned int hl,
                               yt2_room_present_fn present, void *context)
{
    yt2_room *room;
    yt2_character *member;
    unsigned int slot;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL) {
        return YT2_STATUS_ERROR;
    }
    room = combat->room;
    if (combat->sheet_open == 0u || combat->sheet_open > 4u || hl > 4u) {
        return YT2_STATUS_OK;
    }
    slot = room->party->assigned[combat->sheet_open - 1u];
    if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
        return YT2_STATUS_OK;
    }
    member = &room->party->roster[slot - 1u];
    if ((member->status & 0x1C40u) != 0u) {
        /* fn_0A692 buzz (the cursor flash is presentation). */
        combat_sfx(combat, 3u);
        return YT2_STATUS_OK;
    }
    room->party->active[hl] =
        room->party->active[hl] == (uint16_t)slot ? 0u : (uint16_t)slot;
    combat_paint_sheet(combat, member, slot);
    combat->member_page = 1;
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

/* Sheet click boxes (DGROUP:0x6334): 1 close, 2..6 highlights. */
static const combat_box sheet_boxes[] = {
    {164, 197, 112, 121, 1},
    {138, 220, 42, 48, 2},
    {138, 220, 52, 58, 3},
    {138, 220, 62, 68, 4},
    {138, 220, 72, 78, 5},
    {138, 220, 82, 88, 6},
};

static int combat_sheet_click(yt2_combat *combat, int x, int y, int right,
                              yt2_room_present_fn present, void *context)
{
    unsigned int hit;
    unsigned int member;
    unsigned int cell;

    if (right) {
        return combat_sheet_close(combat, present, context);
    }
    hit = chest_hit(sheet_boxes, 6u, x, y);
    if (hit == 1u) {
        return combat_sheet_close(combat, present, context);
    }
    if (hit >= 2u && hit <= 6u) {
        return combat_sheet_toggle(combat, hit - 2u, present, context);
    }
    /* Portrait clicks switch the sheet member (cells re-poll). */
    {
        static const combat_box portraits[] = {
            {8, 39, 148, 179, 1},
            {66, 96, 148, 179, 2},
            {124, 154, 148, 179, 3},
            {182, 212, 148, 179, 4},
        };

        (void)cell;
        member = chest_hit(portraits, 4u, x, y);
    }
    if (member >= 1u && member <= 4u) {
        return combat_sheet_open(combat, member, present, context);
    }
    return YT2_STATUS_OK;
}

/* Pack member panes (DGROUP:0x630A, viewport quarters). */
static const combat_box pack_panes[] = {
    {8, 63, 8, 143, 1},
    {64, 119, 8, 143, 2},
    {120, 175, 8, 143, 3},
    {176, 231, 8, 143, 4},
};

/* Pack icon cells (DGROUP:0x60F4, pane-relative): 1-8 pack, 9 dead,
   10-15 equipment, 16-21 effects. */
/* Pane origins (v_5056/505A per picked member). */
static const int pack_origins[4][2] = {
    {8, 8}, {64, 8}, {120, 8}, {176, 8}
};

/* One group-8 item icon (sub_16CB7, transparent 0xFF). */
static void pack_icon(yt2_combat *combat, int x, int y, unsigned int pic)
{
    const yt2_picture_group *group;
    const uint8_t *data;

    group = yt2_pictures_group(combat->pictures, 8u);
    data = yt2_picture_data(combat->pictures, 8u, pic);
    if (group == NULL || data == NULL) {
        return;
    }
    yt2_surface_blit_transparent(combat->surface, x, y, data,
                                 group->width, group->height, 0xFFu);
}

/* One group-N icon for effects/portraits. */
static void pack_pic(yt2_combat *combat, int x, int y, unsigned int group_id,
                     unsigned int pic, int transparent)
{
    const yt2_picture_group *group;
    const uint8_t *data;

    group = yt2_pictures_group(combat->pictures, group_id);
    data = yt2_picture_data(combat->pictures, group_id, pic);
    if (group == NULL || data == NULL) {
        return;
    }
    if (transparent) {
        yt2_surface_blit_transparent(combat->surface, x, y, data,
                                     group->width, group->height, 0xFFu);
    } else {
        yt2_surface_blit(combat->surface, x, y, data, group->width,
                         group->height);
    }
}

/*
 * fn_1658F: one member pane (portrait, worn equipment, pack, effect
 * icons) at the pane origin. Container branches (0x17C/0x1A2/0x1C8)
 * and the +0x14A icons ride with unported containers; equipment
 * 4-5 paint unconditionally (B1 gates them on the container bit,
 * same pixels when empty).
 */
static void combat_paint_pane(yt2_combat *combat,
                              const yt2_character *member, int ox, int oy)
{
    static const unsigned int equip_xy[6][2] = {
        {0, 64}, {40, 64}, {0, 103}, {40, 103}, {4, 95}, {44, 95}
    };
    static const unsigned int effect_xy[5][2] = {
        {12, 49}, {12, 72}, {12, 93}, {10, 104}, {12, 86}
    };
    const uint8_t *def;
    const uint8_t *row;
    unsigned int i;

    /* Portrait (group 6, index pic, opaque). */
    pack_pic(combat, ox, oy, 6u, member->portrait_index, 0);
    /* Worn equipment (group 8). A staged cell-11 bag paints its
       open variant (icon+1) over cell 11 instead (B1 sub_16D00);
       the bag itself stays hidden by the grid redirect. */
    for (i = 0u; i < YT2_EQUIPMENT_SLOTS; i++) {
        uint16_t icon;

        if (i == 1u && (member->flags & COMBAT_STAGE_CELL11) != 0u) {
            if (member->stage[2].id == 0u ||
                combat_item(combat, member->stage[2].id, &def,
                            &row) != YT2_STATUS_OK) {
                continue;
            }
            pack_icon(combat, ox + (int)equip_xy[i][0],
                      oy + (int)equip_xy[i][1],
                      (unsigned int)(chest_def_icon(def) + 1u));
            continue;
        }
        if (member->equipment[i].id == 0u) {
            continue;
        }
        if (combat_item(combat, member->equipment[i].id, &def, &row) !=
            YT2_STATUS_OK) {
            continue;
        }
        /* Cells 14/15 paint with v_5060 bit 0x8000 set, so flag
           0x400 selects the def+4 variant icon (B1 sub_16CB7). */
        icon = chest_def_icon(def);
        if (i >= 4u && (chest_def_flag(def) & 0x400u) != 0u) {
            icon = combat_read16(def + 4);
        }
        pack_icon(combat, ox + (int)equip_xy[i][0],
                  oy + (int)equip_xy[i][1], icon);
    }
    /* Sex icon over slot 3 when flagged. */
    if ((member->flags & 0x20u) != 0u) {
        pack_icon(combat, ox + 40, oy + 103,
                  member->sex == YT2_SEX_MALE ? 0x0Cu : 0x0Du);
    }
    /* Grid (group 8): staged container cells when a bag is open
       (17C > 1A2 > 1C8 by nonzero id), else the pack. Cell 9
       shows the open pack bag's variant icon (B1 sub_16D2B). */
    {
        const yt2_item *grid = member->pack;
        unsigned int s;

        for (s = 0u; s < YT2_CONTAINER_SLOTS; s++) {
            if (member->stage[s].id == 0u) {
                continue;
            }
            grid = member->stage[s].cells;
            /* Cell-11 bags indicate at cell 11, not cell 9. */
            if (s == 2u &&
                (member->flags & COMBAT_STAGE_CELL11) != 0u) {
                break;
            }
            if (combat_item(combat, member->stage[s].id, &def,
                            &row) == YT2_STATUS_OK) {
                pack_icon(combat, ox + 38, oy + 36,
                          (unsigned int)(chest_def_icon(def) + 1u));
            }
            break;
        }
        for (i = 0u; i < YT2_PACK_SLOTS; i++) {
            if (grid[i].id == 0u) {
                continue;
            }
            if (combat_item(combat, grid[i].id, &def, &row) !=
                YT2_STATUS_OK) {
                continue;
            }
            pack_icon(combat, ox + yt2_paperdoll_cells[i].x0,
                      oy + yt2_paperdoll_cells[i].y0,
                      chest_def_icon(def));
        }
    }
    /* Effects (group 7): B1 resolves through fn_02262's row gate
       (flags 0xE00/0xC000/0x100 pick the row, anything else paints
       nothing); the pic is row+4, +1 unless male. */
    for (i = 0u; i < YT2_EFFECT_SLOTS; i++) {
        const yt2_picture_group *group;
        const uint8_t *data;
        unsigned int pic;

        if (member->effects[i] == 0u) {
            continue;
        }
        if (combat_item(combat, member->effects[i], &def, &row) !=
            YT2_STATUS_OK) {
            continue;
        }
        if (row == NULL || (chest_def_flag(def) & 0xE700u) == 0u) {
            continue;
        }
        pic = chest_read16(row + 4u);
        if (member->sex != YT2_SEX_MALE) {
            pic++;
        }
        group = yt2_pictures_group(combat->pictures, 7u);
        data = yt2_picture_data(combat->pictures, 7u, pic);
        if (group == NULL || data == NULL) {
            continue;
        }
        yt2_surface_blit_transparent(combat->surface,
                                     ox + (int)effect_xy[i][0],
                                     oy + (int)effect_xy[i][1], data,
                                     group->width, group->height, 0xFFu);
    }
}

/* sub_091F9: paint one picked member's pane. */
static void pack_paint_member(yt2_combat *combat, unsigned int member)
{
    yt2_party *party = combat->room->party;
    unsigned int slot;

    if (member == 0u || member > 4u) {
        return;
    }
    slot = party->assigned[member - 1u];
    if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
        return;
    }
    combat_paint_pane(combat, &party->roster[slot - 1u],
                      pack_origins[member - 1u][0],
                      pack_origins[member - 1u][1]);
}

/* sub_092F2: stash the pack-entry viewport (x8-231/y8-143). */
static void pack_backdrop_snap(yt2_combat *combat)
{
    unsigned int y;

    for (y = 0u; y < 136u; y++) {
        memcpy(combat->pack_backdrop[y],
               combat->surface->pixels + (8u + y) * combat->surface->stride +
                   8u,
               224u);
    }
    combat->pack_backdrop_valid = 1;
}

/* sub_091AF/sub_092C2: restore a viewport rect from the stash. */
static void pack_backdrop_blit(yt2_combat *combat, int x0, int y0,
                               unsigned int w, unsigned int h)
{
    unsigned int y;

    if (!combat->pack_backdrop_valid) {
        return;
    }
    for (y = 0u; y < h; y++) {
        memcpy(combat->surface->pixels +
                   (unsigned int)(y0 + (int)y) * combat->surface->stride +
                   (unsigned int)x0,
               combat->pack_backdrop[y0 - 8 + (int)y] + (x0 - 8), w);
    }
}

/*
 * Pack UI open (fn_08668/fn_085CA + fn_08763 entry): the pool stages
 * into the shared grid, panes paint per picked member, the pack
 * pane paints. Single-member opens (right-press) paint just them.
 */
static int combat_pack_open(yt2_combat *combat, unsigned int member,
                            yt2_room_present_fn present, void *context)
{
    yt2_room *room;
    unsigned int i;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL) {
        return YT2_STATUS_ERROR;
    }
    room = combat->room;
    if (combat->pack_open != 0u) {
        return YT2_STATUS_OK;
    }
    if (member != 0u) {
        unsigned int slot;

        if (member > 4u) {
            return YT2_STATUS_OK;
        }
        slot = room->party->assigned[member - 1u];
        if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
            return YT2_STATUS_OK;
        }
        combat_sfx(combat, 1u);
        combat->pack_open = 0x4000u >> (member - 1u);
        combat->pack_current = member;
    } else {
        combat->pack_open = 0u;
        combat->pack_current = 0u;
        for (i = 1u; i <= 4u; i++) {
            unsigned int slot = room->party->assigned[i - 1u];

            if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
                continue;
            }
            combat->pack_open |= 0x4000u >> (i - 1u);
        }
        if (combat->pack_open == 0u) {
            return YT2_STATUS_OK;
        }
        /* fn_08668 plays SFX 1 for the all-panes open. */
        combat_sfx(combat, 1u);
    }
    for (i = 0u; i < YT2_POOL_SLOTS; i++) {
        combat->quick_access[i].id = room->party->pool[i].id;
        combat->quick_access[i].count = room->party->pool[i].count;
    }
    /* fn_1755C forces quick access exclusive (stash the panel bits). */
    combat->panel_saved_pack =
        (uint16_t)(room->party->explore_flags & 0x7000u);
    combat->panel_saved_pack_valid = 1;
    room->party->explore_flags =
        (uint16_t)((room->party->explore_flags & 0x9FFFu) | 0x1000u);
    /* fn_06EA4 side panel prep, gated on v_505E bit 0x100 like B1
       (explore P wipes like B1 s11, formal P like loc_068C3);
       the chest P (B1 loc_07475) never wipes, loot stays. */
    if (!combat->chest_open && !combat->chest_prompt &&
        (room->f505e & 0x100u) != 0u) {
        if ((room->f5064 & 0x1000u) != 0u) {
            chest_fill(combat, 240, 86, 73, 109);
            room->f5062 &= (uint16_t)~0x1000u;
        } else {
            chest_side_panel_clear(combat, 1);
        }
        room->f505e &= (uint16_t)~0x100u;
    }
    quick_access_paint(combat);
    /* First pane from zero stashes the viewport (B1 sub_092F2);
       toggle-off and nested sheet close restore from it. */
    pack_backdrop_snap(combat);
    for (i = 1u; i <= 4u; i++) {
        if ((combat->pack_open & (0x4000u >> (i - 1u))) != 0u) {
            pack_paint_member(combat, i);
        }
    }
    /* B1's pack never paints the member border (v_50C0 is the
       sheet's); s11's borderless strip proves it. */
    combat->member_page = 1;
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

/* loc_08BE2: close the pack UI (pool stages back, full repaint). */
static void pack_flush_member(yt2_combat *combat, unsigned int member)
{
    yt2_party *party = combat->room->party;
    unsigned int assigned = party->assigned[member - 1u];
    yt2_character *staged;

    if (assigned == 0u || assigned > YT2_ROSTER_SLOTS) {
        return;
    }
    staged = &party->roster[assigned - 1u];
    for (unsigned int s = 0u; s < YT2_CONTAINER_SLOTS; s++) {
        yt2_paperdoll_store_flush(&combat->paperdoll, &staged->stage[s]);
    }
    staged->flags = (uint16_t)(staged->flags & ~0xF000u);
}

static int combat_pack_close(yt2_combat *combat, unsigned int member,
                             yt2_room_present_fn present, void *context)
{
    unsigned int i;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (combat->pack_open == 0u) {
        return YT2_STATUS_OK;
    }
    combat_sfx(combat, 1u);
    for (i = 0u; i < YT2_POOL_SLOTS; i++) {
        combat->room->party->pool[i].id = combat->quick_access[i].id;
        combat->room->party->pool[i].count = combat->quick_access[i].count;
    }
    /* The carried item stays carried (B1 never drops the cursor
       here); explore drops land with the ground-spawn slice. */
    combat->pack_open = 0u;
    combat->pack_current = 0u;
    combat->chest_side_panel_lines = 0u;
    /* P/rhold close flushes every member (B1 sub_09032);
       last-pane toggle flushes just it (B1 sub_091AF). */
    if (member == 0u) {
        for (i = 1u; i <= 4u; i++) {
            pack_flush_member(combat, i);
        }
    } else {
        pack_flush_member(combat, member);
    }
    if (combat->panel_saved_pack_valid) {
        combat->room->party->explore_flags = (uint16_t)(
            (combat->room->party->explore_flags & 0x8FFFu) |
            combat->panel_saved_pack);
        combat->panel_saved_pack_valid = 0;
    }
    /* sub_09032's fn_06EA4 consumes the pending side panel, then the
       explore tail repaints with the compass (WEST over the
       blank row, like B1 s12). */
    chest_side_panel_clear(combat, 1);
    combat->room->f505e &= (uint16_t)~0x100u;
    combat->chest_compass_fresh = 1u;
    /* The close presents itself; a trailing repaint would wipe
       the compass row (its fresh bit is spent). */
    combat->member_page = 1;
    return yt2_combat_repaint(combat, present, context);
}

/* sub_093D4: member pane hit (sets the current member). */
static int pack_pane_hit(yt2_combat *combat, int x, int y)
{
    unsigned int hit = chest_hit(pack_panes, 4u, x, y);

    if (hit == 0u) {
        return 0;
    }
    if ((combat->pack_open & (0x4000u >> (hit - 1u))) == 0u) {
        return 0;
    }
    if (combat->room->party->assigned[hit - 1u] == 0u) {
        return 0;
    }
    combat->pack_current = hit;
    return 1;
}

/*
 * fn_0AACC/fn_0ACF5: apply or remove an item's bonus list (four
 * stat-offset/value pairs at the def+2 list, 0-terminated; adds
 * cap at 999, removes floor at 0), then rederive capacity/bases
 * and recompute the party means. Writes raw save bytes like B1
 * (flushed first) and reparses the slot.
 */
static void combat_item_bonus_apply(yt2_combat *combat, unsigned int slot,
                                    unsigned int id, int plus)
{
    yt2_party *party = combat->room->party;
    const uint8_t *def;
    const uint8_t *row;
    const uint8_t *adds;
    uint8_t *block;
    uint8_t *slot_bytes;
    unsigned int i;

    if (slot == 0u || slot > YT2_ROSTER_SLOTS ||
        combat_item(combat, id, &def, &row) != YT2_STATUS_OK) {
        return;
    }
    adds = yt2_item_adds(combat->items, def);

    if (adds == NULL) {
        return;
    }
    if (combat->save == NULL || combat->save->bytes == NULL) {
        return;
    }
    block = combat->save->bytes + YT2_SAVE_PARTY_OFFSET;
    if (yt2_party_write(block, YT2_SAVE_PARTY_SIZE, party) !=
        YT2_STATUS_OK) {
        return;
    }
    slot_bytes = block + 0x1F4u + (slot - 1u) * 0x1F4u;
    for (i = 0u; i < 4u; i++) {
        uint16_t off = combat_read16(adds + i * 4u);
        uint16_t value = combat_read16(adds + i * 4u + 2u);
        uint16_t cur;

        if (off == 0u) {
            break;
        }
        if (off + 1u >= 0x1F4u) {
            continue;
        }
        cur = combat_read16(slot_bytes + off);
        if (plus) {
            if (off >= 0x32u && cur == 0u) {
                continue;
            }
            cur = (uint16_t)(cur + value);
            if (cur > 999u) {
                cur = 999u;
            }
        } else if (cur > value) {
            cur = (uint16_t)(cur - value);
        } else {
            cur = 0u;
        }
        combat_write16(slot_bytes + off, cur);
    }
    yt2_party_parse_character(slot_bytes, &party->roster[slot - 1u]);
    combat_rederive(&party->roster[slot - 1u]);
    /* fn_0ABEC: party means plus the map/quick access panel flip. */
    yt2_room_update_skill_flags(party);
    if ((party->explore_flags & 0x4000u) != 0u &&
        (party->explore_flags & 0x8000u) == 0u) {
        party->explore_flags &= (uint16_t)~0x8000u;
        party->explore_flags |= 0x1000u;
        combat->room->f505e |= 0x0400u;
    }
}

/* Paperdoll bonus hook: the shared verbs apply roster-slot bonuses here. */
static void combat_paperdoll_bonus(void *bonus_ctx, unsigned int slot,
                              unsigned int id, int plus)
{
    combat_item_bonus_apply((yt2_combat *)bonus_ctx, slot, id, plus);
}

/* fn_11F83: the carried/taken item name paints 3 white fields. */
static void combat_item_name_side_panel(yt2_combat *combat, unsigned int id)
{
    char fields[3][14];
    unsigned int i;

    /* Gated wipe first, then the bit stays set like B1. */
    if ((combat->room->f505e & 0x100u) != 0u) {
        chest_side_panel_clear(combat, 1);
        combat->room->f505e &= (uint16_t)~0x100u;
    }
    combat->room->f505e |= 0x100u;
    if (id != 0u) {
        for (i = 0u; i < 3u; i++) {
            if (yt2_item_name_field(combat->items, id - 1u, i, fields[i],
                                    sizeof(fields[i])) != YT2_STATUS_OK) {
                return;
            }
            yt2_text_draw(combat->surface, 240, 96 + (int)i * 6, fields[i],
                          0x0Fu);
        }
    }
    /* fn_076AC's purse paints only on f5060 bit 0x80 (dormant). */
}

/* New parties reset the allocator (B1 v_95FD 2 / v_95FF 0). */
void yt2_combat_new_party(yt2_combat *combat)
{
    if (combat == NULL) {
        return;
    }
    yt2_paperdoll_new_party(&combat->paperdoll);
}

/*
 * fn_16925: right-press on a member paperdoll. The shared verb stages
 * containers and reports examined hits; vanilla stays silent on
 * those (not even a name), enhanced paints the name+stats receipt.
 */
static void pack_examine(yt2_combat *combat, unsigned int member, int x,
                         int y, yt2_room_present_fn present, void *context)
{
    yt2_party *party = combat->room->party;
    unsigned int slot;
    yt2_character *character;
    unsigned int id;
    unsigned int examined = 0u;
    const uint8_t *def;
    const uint8_t *row;
    char stats[24];
    char weight[24];
    int outcome;

    if (member == 0u || member > 4u) {
        return;
    }
    slot = party->assigned[member - 1u];
    if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
        return;
    }
    character = &party->roster[slot - 1u];
    id = yt2_paperdoll_hit(pack_origins[member - 1u][0],
                      pack_origins[member - 1u][1], x, y);
    combat->paperdoll.cursor = combat->chest_cursor;
    combat->paperdoll.carried = combat->chest_carried;
    combat->paperdoll.carried_count = combat->chest_carried_count;
    combat->paperdoll.carried_weight = combat->chest_carried_weight;
    outcome = yt2_paperdoll_right(&combat->paperdoll, character, slot, id,
                             &examined);
    combat->chest_cursor = combat->paperdoll.cursor;
    combat->chest_carried = combat->paperdoll.carried;
    combat->chest_carried_count = combat->paperdoll.carried_count;
    combat->chest_carried_weight = combat->paperdoll.carried_weight;
    if (outcome == YT2_PAPERDOLL_CHANGED) {
        pack_paint_member(combat, member);
        combat->member_page = 1;
        if (present != NULL) {
            present(context);
        }
        return;
    }
    if (outcome != YT2_PAPERDOLL_EXAMINED || examined == 0u) {
        return;
    }
    /* fn_19B42 use modal (potions, scrolls, lore gear),
       deferred; silent in vanilla. */
    if (!yt2_boot_enhanced()) {
        return;
    }
    /* Enhanced receipt: fn_11F83 paints the 3-field name first. */
    if (combat_item(combat, examined, &def, &row) != YT2_STATUS_OK) {
        return;
    }
    combat_item_name_side_panel(combat, examined);
    if (row != NULL) {
        snprintf(stats, sizeof(stats), "DMG %u",
                 (unsigned int)combat_read16(row));
    } else {
        snprintf(stats, sizeof(stats), "DMG -");
    }
    yt2_text_draw(combat->surface, 240, 114, stats, 0x8Au);
    snprintf(weight, sizeof(weight), "WT %u",
             (unsigned int)chest_def_weight(def));
    yt2_text_draw(combat->surface, 240, 120, weight, 0x8Au);
    combat->member_page = 1;
    if (present != NULL) {
        present(context);
    }
}

/*
 * fn_1672C: left-press on a member paperdoll. The shared verb moves the
 * item; the pane repaints (B1 fn_1658F) and the side panel takes the
 * carried name (B1 fn_11F83 tail). Returns 1 on success (B1
 * v_50CC 0), 0 on a miss (v_50CC 1).
 */
static int pack_pane_click(yt2_combat *combat, unsigned int member, int x,
                           int y)
{
    yt2_party *party = combat->room->party;
    unsigned int slot;
    yt2_character *character;
    unsigned int id;
    int outcome;

    if (member == 0u || member > 4u) {
        return 0;
    }
    slot = party->assigned[member - 1u];
    if (slot == 0u || slot > YT2_ROSTER_SLOTS) {
        return 0;
    }
    character = &party->roster[slot - 1u];
    id = yt2_paperdoll_hit(pack_origins[member - 1u][0],
                      pack_origins[member - 1u][1], x, y);
    if (id == 0u || id == 9u) {
        return 0;
    }
    combat->paperdoll.cursor = combat->chest_cursor;
    combat->paperdoll.carried = combat->chest_carried;
    combat->paperdoll.carried_count = combat->chest_carried_count;
    combat->paperdoll.carried_weight = combat->chest_carried_weight;
    outcome = yt2_paperdoll_left(&combat->paperdoll, character, slot, id);
    combat->chest_cursor = combat->paperdoll.cursor;
    combat->chest_carried = combat->paperdoll.carried;
    combat->chest_carried_count = combat->paperdoll.carried_count;
    combat->chest_carried_weight = combat->paperdoll.carried_weight;
    if (outcome != YT2_PAPERDOLL_CHANGED) {
        return 0;
    }
    /* Every placement repaints the pane (B1 fn_1658F runs inside
       sub_16D74/16D8B/16DB0 and the loc_168B9 insert arm); the
       strip and the carried name follow. */
    pack_paint_member(combat, member);
    combat_item_name_side_panel(combat, combat->chest_carried);
    return 1;
}

static int combat_pack_click(yt2_combat *combat, int x, int y, int right,
                             yt2_room_present_fn present, void *context)
{
    unsigned int hit;

    if (right) {
        /* loc_08AED: a right-press on a strip portrait toggles
           that member's pane (SFX 1); unpicking the last pane
           closes. Paperdolls examine (below); chapter examines ride
           with fn_1789B. */
        unsigned int member;
        unsigned int bit;

        hit = chest_hit(chest_regions, 6u, x, y);
        if (hit == 1u) {
            /* fn_16925: plain gear silent, readables/sockets
               deferred-silent in vanilla; enhanced shows the
               name+stats receipt instead. */
            if (pack_pane_hit(combat, x, y)) {
                pack_examine(combat, combat->pack_current, x, y,
                             present, context);
            }
            return YT2_STATUS_OK;
        }
        if (hit != 6u ||
            !yt2_room_portrait_hit(x, y, &member)) {
            return YT2_STATUS_OK;
        }
        if (combat->room->party->assigned[member - 1u] == 0u) {
            return YT2_STATUS_OK;
        }
        bit = 0x4000u >> (member - 1u);
        if ((combat->pack_open & bit) == 0u) {
            combat->pack_open |= bit;
            combat_sfx(combat, 1u);
            pack_paint_member(combat, member);
            combat->member_page = 1;
            if (present != NULL) {
                present(context);
            }
            return YT2_STATUS_OK;
        }
        if ((combat->pack_open & (unsigned int)~bit) == 0u) {
            /* Unpicking the last pane closes (pack_close
               clears the bits itself). */
            return combat_pack_close(combat, member, present,
                                     context);
        }
        combat->pack_open &= (unsigned int)~bit;
        combat_sfx(combat, 1u);
        /* Toggle-off flushes that member's staging, then the
           pane restores from the stash (B1 sub_091AF). */
        pack_flush_member(combat, member);
        pack_backdrop_blit(combat, pack_origins[member - 1u][0],
                           pack_origins[member - 1u][1], 56u, 136u);
        combat->member_page = 1;
        if (present != NULL) {
            present(context);
        }
        return YT2_STATUS_OK;
    }
    hit = chest_hit(chest_regions, 6u, x, y);
    if (hit == 2u) {
        unsigned int cell = chest_hit(quick_access_cells, 9u, x, y);

        /* Quick access grid (fn_176EC over the staged pool). */
        if (cell != 0u) {
            quick_access_click(combat, cell);
            quick_access_paint(combat);
            combat->member_page = 1;
            if (present != NULL) {
                present(context);
            }
        }
        return YT2_STATUS_OK;
    }
    if (hit == 6u) {
        unsigned int member;
        unsigned int cell;

        /* Carried clicks run fn_08D3F (portrait drops, cells buzz). */
        if (combat->chest_cursor != 0u) {
            if (yt2_room_portrait_hit(x, y, &member)) {
                chest_portrait_drop(combat, member);
                pack_paint_member(combat, member);
                combat->member_page = 1;
                if (present != NULL) {
                    present(context);
                }
            } else if (yt2_room_strip_hit(x, y, &member, &cell)) {
                combat_sfx(combat, 3u);
            }
            return YT2_STATUS_OK;
        }
        /* Status cells inspect (fn_16044). */
        if (yt2_room_strip_hit(x, y, &member, &cell)) {
            return combat_member_page(combat, member,
                                      cell <= 2u ? 0u : cell - 2u,
                                      present, context, 0);
        }
        return YT2_STATUS_OK;
    }
    if (hit == 1u) {
        /* Member pane: pickup/drop/swap/equip, then the strip
           repaint and the item name (B1 loc_087F3 tail). */
        if (pack_pane_hit(combat, x, y) &&
            pack_pane_click(combat, combat->pack_current, x, y)) {
            yt2_room_paint_portraits(combat->room->party,
                                     combat->pictures, combat->surface);
            combat->member_page = 1;
            if (present != NULL) {
                present(context);
            }
        }
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_OK;
}

/* One item icon (fn_19E12 class 0x80, transparent 0xFF). */
static void chest_icon(yt2_combat *combat, int x, int y, unsigned int id)
{
    const uint8_t *def;
    const uint8_t *row;
    const uint8_t *pic;
    const yt2_picture_group *group;

    if (combat_item(combat, id, &def, &row) != YT2_STATUS_OK) {
        return;
    }
    group = yt2_pictures_group(combat->pictures, 8u);
    if (group == NULL) {
        return;
    }
    pic = yt2_picture_data(combat->pictures, 8u, chest_def_icon(def));
    if (pic == NULL) {
        return;
    }
    yt2_surface_blit_transparent(combat->surface, x, y, pic, group->width,
                                 group->height, 0xFFu);
}

/* fn_0B540 + sub_07342: clear the loot grid (union x240-312,
   35 rows, not 36) and blit icons. */
static void chest_paint_loot(yt2_combat *combat)
{
    chest_fill(combat, 240, 160, 73, 35);
    for (unsigned int i = 0u; i < YT2_CHEST_SLOTS; i++) {
        if (combat->chest_staging[i].id != 0u) {
            chest_icon(combat, chest_loot_cells[i].x0,
                       chest_loot_cells[i].y0,
                       combat->chest_staging[i].id);
        }
    }
}

/* The (240,8) 74x58 backdrop rect both quick access and chapter restore. */
static void chest_panel_backdrop(yt2_combat *combat, yt2_surface *surface)
{
    const yt2_picture_group *back;
    const uint8_t *back_pic;

    back = yt2_pictures_group(combat->pictures, 0u);
    back_pic = yt2_picture_data(combat->pictures, 0u, 1u);
    if (back == NULL || back_pic == NULL) {
        return;
    }
    /* The backdrop blits at (1,1): file (239,7) lands on (240,8). */
    for (int row = 0u; row < 58; row++) {
        for (int col = 0u; col < 74; col++) {
            size_t src = (size_t)(7 + row) * back->width + (239u + col);
            size_t dst =
                (size_t)(8 + row) * surface->stride + (240u + col);

            surface->pixels[dst] = back_pic[src];
        }
    }
}

/* fn_1755C: restore the panel backdrop, blit the grid frame
   (group 6 pic 0x14 at (248,9), transparent), then the 9 icons
   (sub_17951). The restore wipes explore's 0x15 plaque. The
   currency row shows an icon only when that purse leg is nonzero
   (fn_09B42 over 0x9503/0x9507/0x950B); the six item cells read
   the shared 0x9569 staging (chest drops land there, and formal
   combat shows the same residue). The chest modal and the formal
   panel call this same painter. */
static void quick_access_paint(yt2_combat *combat)
{
    static const unsigned int currencies[3] = {1u, 2u, 3u};
    static const uint8_t zero[4] = {0u, 0u, 0u, 0u};
    const yt2_picture_group *over;
    const uint8_t *quick_access_frame;
    const uint8_t *purses[3];
    yt2_surface *surface = combat->surface;

    chest_panel_backdrop(combat, surface);
    over = yt2_pictures_group(combat->pictures, 6u);
    quick_access_frame = yt2_picture_data(combat->pictures, 6u, 0x14u);
    if (over != NULL && quick_access_frame != NULL) {
        yt2_surface_blit_transparent(surface, 248, 9, quick_access_frame, over->width,
                                     over->height, 0xFFu);
    }
    purses[0] = combat->room->party->gold;
    purses[1] = combat->room->party->ore;
    purses[2] = combat->room->party->nuore;
    for (unsigned int i = 0u; i < 3u; i++) {
        if (combat_bcd_compare(purses[i], zero) != 0) {
            chest_icon(combat, quick_access_cells[i].x0,
                       quick_access_cells[i].y0, currencies[i]);
        }
    }
    for (unsigned int i = 0u; i < YT2_QUICK_ACCESS_SLOTS; i++) {
        if (combat->quick_access[i].id != 0u) {
            chest_icon(combat, quick_access_cells[3u + i].x0,
                       quick_access_cells[3u + i].y0, combat->quick_access[i].id);
        }
    }
}

/* fn_117AC + fn_11836 fill (the compass stays in the room):
   the explore top-right panel by state. Unvisited and
   out-of-window cells read the void icon; the facing chain
   (sub_118C0: N->0, S->2, E->1, else 3) picks the marker. */
void yt2_combat_panel_paint(yt2_combat *combat, yt2_surface *dst)
{
    yt2_room *room;
    uint16_t flags;

    if (combat == NULL || combat->room == NULL || dst == NULL ||
        combat->pictures == NULL) {
        return;
    }
    room = combat->room;
    flags = room->party->explore_flags;
    if ((flags & 0x2000u) != 0u) {
        /* fn_1270C: backdrop rect restore plus the chapter logo
           (the restore is a no-op on fresh gathers and required
           on stale toggles). */
        const yt2_picture_group *group =
            yt2_pictures_group(combat->pictures, 6u);
        const uint8_t *pic =
            yt2_picture_data(combat->pictures, 6u, 0x15u);

        chest_panel_backdrop(combat, dst);
        if (group != NULL && pic != NULL) {
            yt2_surface_blit_transparent(dst, 248, 9, pic, group->width,
                                         group->height, 0xFFu);
        }
        return;
    }
    if ((flags & 0x1000u) != 0u) {
        quick_access_paint(combat);
        return;
    }
    for (unsigned int row = 0u; row < 7u; row++) {
        for (unsigned int col = 0u; col < 9u; col++) {
            yt2_room_cell *cell = yt2_room_window_cell(
                room, (int)room->party->x - 4 + (int)col,
                (int)room->party->y - 3 + (int)row);
            unsigned int base = YT2_MAPVIEW_VOID_ICON;
            unsigned int over = 0u;
            const uint8_t *pic;

            if (cell != NULL &&
                (cell->flags & YT2_ROOM_CELL_VISITED) != 0u) {
                base = yt2_mapview_terrain_icon(cell->terrain);
                over = yt2_mapview_object_icon(cell->object);
            }
            pic = yt2_picture_data(combat->pictures,
                                   YT2_MAPVIEW_PICTURE_GROUP, base);
            if (pic != NULL) {
                yt2_surface_blit(dst, 240 + (int)col * 8,
                                 8 + (int)row * 8, pic,
                                 YT2_MAPVIEW_ICON_SIZE,
                                 YT2_MAPVIEW_ICON_SIZE);
            }
            if (over != 0u) {
                pic = yt2_picture_data(combat->pictures,
                                       YT2_MAPVIEW_PICTURE_GROUP, over);
                if (pic != NULL) {
                    yt2_surface_blit_transparent(
                        dst, 240 + (int)col * 8, 8 + (int)row * 8, pic,
                        YT2_MAPVIEW_ICON_SIZE, YT2_MAPVIEW_ICON_SIZE,
                        YT2_MAPVIEW_ICON_TRANSPARENT);
                }
            }
        }
    }
    {
        unsigned int facing = room->party->facing;
        unsigned int id = 3u;
        const uint8_t *pic;

        if ((facing & YT2_ROOM_FACING_NORTH) != 0u) {
            id = 0u;
        } else if ((facing & YT2_ROOM_FACING_SOUTH) != 0u) {
            id = 2u;
        } else if ((facing & YT2_ROOM_FACING_EAST) != 0u) {
            id = 1u;
        }
        pic = yt2_picture_data(combat->pictures, YT2_MAPVIEW_PICTURE_GROUP,
                               id);
        if (pic != NULL) {
            yt2_surface_blit_transparent(dst, 272, 32, pic,
                                         YT2_MAPVIEW_ICON_SIZE,
                                         YT2_MAPVIEW_ICON_SIZE,
                                         YT2_MAPVIEW_ICON_TRANSPARENT);
        }
    }
}

/* One icon cell (fn_19E12 mode 0): group-8 pic, opaque 16x16,
   over the first icon cell at (239,67). */
static void combat_paint_icon(yt2_combat *combat, unsigned int id)
{
    const yt2_picture_group *group =
        yt2_pictures_group(combat->pictures, 8u);
    const uint8_t *pic = yt2_picture_data(combat->pictures, 8u, id);

    if (group == NULL || pic == NULL) {
        return;
    }
    yt2_surface_blit(combat->surface, 239, 67, pic, group->width,
                     group->height);
}

/* fn_1261F in formal combat: the crossed-swords icon (pic 3)
   over the first icon cell. Explore keeps the bow from the
   chrome (pic 1, same blit). While a verb resolves, the pressed
   pic shows instead (4 swords). */
static void combat_paint_swords(yt2_combat *combat)
{
    combat_paint_icon(combat, combat->button_pressed != 0u ?
                      combat->button_pressed : 3u);
}

/* fn_17D2C: d101 over the threshold succeeds. Returns 1/0. */
static unsigned int chest_skill_roll(yt2_combat *combat, unsigned int level,
                                     unsigned int difficulty,
                                     unsigned int skill)
{
    yt2_room *room = combat->room;
    int threshold;
    unsigned int roll;

    threshold = 5 * ((int)level - (int)difficulty) + (int)skill;
    if (threshold < 5) {
        threshold = 5;
    }
    roll = yt2_create_rand(&room->rng_seed, room->time_word, 100u);
    return roll > (unsigned int)threshold ? 1u : 0u;
}

/* fn_09D41: 4-byte BCD subtract with DAS per byte (si -= di). */
static void chest_bcd_sub(uint8_t *dst, const uint8_t *src)
{
    unsigned int borrow = 0u;

    for (int i = 3; i >= 0; i--) {
        unsigned int d = (unsigned int)dst[i];
        unsigned int s = (unsigned int)src[i] + borrow;
        unsigned int lo = (d & 15u) - (s & 15u);
        unsigned int hi = (d >> 4) - (s >> 4);

        borrow = 0u;
        if ((lo & 16u) != 0u) {
            lo = (lo - 6u) & 15u;
            hi--;
        }
        if ((hi & 16u) != 0u) {
            hi = (hi - 6u) & 15u;
            borrow = 1u;
        }
        dst[i] = (uint8_t)((hi << 4) | lo);
    }
}

/* Trap effect defs (DGROUP 0x90AB, 12 bytes each, B1-verified;
   v_5138 boot patch already applied to [0] and [0x108]).
   Only ids below 64 are real defs; the rest is static binary data
   vanilla reads as defs (replicated for fidelity). */
static const uint8_t chest_effects[100][12] = {
    {0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02},
    {0x0D,0x00,0x02,0x00,0x01,0x00,0x0A,0x00,0x10,0x00,0x00,0x00},
    {0x0E,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x09,0x00,0x03,0x00,0x01,0x00,0x05,0x00,0x10,0x00,0x00,0x00},
    {0x09,0x00,0x03,0x00,0x01,0x00,0x05,0x00,0x10,0x40,0x00,0x10},
    {0x09,0x00,0x04,0x00,0x01,0x00,0x05,0x00,0x10,0x00,0x00,0x00},
    {0x09,0x00,0x04,0x00,0x01,0x00,0x05,0x00,0x10,0x20,0x00,0x10},
    {0x0A,0x00,0x07,0x00,0x01,0x00,0x05,0x00,0x10,0x00,0x00,0x00},
    {0x0A,0x00,0x07,0x00,0x01,0x00,0x05,0x00,0x10,0x08,0x00,0x10},
    {0x0B,0x00,0x08,0x00,0x01,0x00,0x05,0x00,0x10,0x00,0x00,0x00},
    {0x0B,0x00,0x08,0x00,0x01,0x00,0x05,0x00,0x88,0x00,0x00,0x10},
    {0x08,0x00,0x05,0x00,0x01,0x00,0x05,0x00,0x10,0x00,0x00,0x00},
    {0x0C,0x00,0x06,0x00,0x01,0x00,0x05,0x00,0x10,0x00,0x00,0x00},
    {0x3A,0x00,0xE6,0x00,0x01,0x00,0x0F,0x00,0x08,0x00,0x00,0x00},
    {0x07,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00},
    {0x00,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00},
    {0x00,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00},
    {0x0E,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x41},
    {0x0E,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x40},
    {0x0D,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x40},
    {0x0D,0x00,0xE1,0x00,0x01,0x00,0x09,0x00,0x10,0x40,0x00,0x10},
    {0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04},
    {0x3A,0x00,0xE6,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00},
    {0x0D,0x00,0xE1,0x00,0x00,0x00,0x00,0x00,0x10,0x80,0x00,0x10},
    {0x0D,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x20,0x02,0x00,0x10},
    {0x3E,0x00,0xE5,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x3D,0x00,0xE3,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x3A,0x00,0xE6,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x0D,0x00,0xE2,0x00,0x00,0x00,0x00,0x00,0x10,0x20,0x00,0x10},
    {0x0D,0x00,0xE2,0x00,0x00,0x00,0x00,0x00,0x10,0xE0,0x00,0x10},
    {0x0D,0x00,0xE1,0x00,0x01,0x00,0x04,0x00,0x10,0x80,0x00,0x10},
    {0x0C,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x3F,0x00,0xE4,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x0A,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x0B,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x3A,0x00,0xE6,0x00,0x00,0x00,0x00,0x00,0x10,0x10,0x00,0x10},
    {0x3D,0x00,0xE3,0x00,0x00,0x00,0x00,0x00,0x20,0x01,0x00,0x10},
    {0x08,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
    {0x0D,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x10,0x04,0x00,0x10},
    {0x3F,0x00,0xE4,0x00,0x00,0x00,0x00,0x00,0x10,0x08,0x00,0x10},
    {0x0B,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x88,0x00,0x00,0x10},
    {0x3E,0x00,0xE5,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00},
    {0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04},
    {0x0D,0x00,0xE1,0x00,0x00,0x00,0x00,0x00,0x10,0x40,0x00,0x10},
    {0x00,0x00,0x06,0x80,0x00,0x00,0x18,0x50,0x00,0x00,0x26,0x00},
    {0x00,0x00,0x55,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x70,0x00},
    {0x00,0x02,0x60,0x00,0x00,0x04,0x95,0x00,0x00,0x07,0x97,0x00},
    {0x00,0x12,0x50,0x00,0x00,0x21,0x90,0x00,0x00,0x34,0x00,0x00},
    {0x00,0x46,0x00,0x00,0x00,0x66,0x00,0x00,0x00,0x80,0x00,0x00},
    {0x00,0x92,0x00,0x00,0x01,0x10,0x00,0x00,0x01,0x21,0x00,0x00},
    {0x01,0x35,0x00,0x00,0x01,0x64,0x00,0x00,0x01,0x82,0x00,0x00},
    {0x01,0x96,0x00,0x00,0x02,0x09,0x00,0x00,0x02,0x35,0x00,0x00},
    {0x02,0x62,0x00,0x00,0x03,0x05,0x00,0x00,0x03,0x42,0x00,0x00},
    {0x03,0x67,0x00,0x00,0x03,0x85,0x00,0x00,0x04,0x27,0x00,0x00},
    {0x04,0x49,0x00,0x00,0x04,0x75,0x00,0x00,0x05,0x17,0x00,0x00},
    {0x05,0x60,0x00,0x00,0x05,0x92,0x00,0x00,0x06,0x24,0x00,0x00},
    {0x06,0x52,0x00,0x00,0x07,0x64,0x00,0x00,0x37,0x55,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00},
    {0x90,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x53,0x4D,0x49,0x54},
    {0x48,0x57,0x41,0x52,0x45,0x20,0x50,0x41,0x52,0x54,0x59,0x00},
    {0x57,0x41,0x52,0x45,0x00,0x20,0x20,0x20,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x94,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x37,0x00,0x34,0x00,0x36,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x03,0x00,0x00,0x80},
    {0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x40,0xA1,0x00,0x22,0x00,0x04,0x00,0x0B,0x00},
    {0x22,0x02,0xD3,0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x35},
    {0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* sub_082E2/sub_08300: HP/MP subtract with the death rule. */
static void chest_hurt_member(yt2_combat *combat, yt2_character *member,
                              unsigned int slot, unsigned int amount, int mana)
{
    uint16_t *pool;
    uint16_t have;

    if (mana) {
        pool = &member->current.combat.magic_points;
    } else {
        pool = &member->current.combat.hit_points;
    }
    have = *pool;
    if (amount >= have) {
        *pool = 0u;
    } else {
        *pool = (uint16_t)(have - amount);
    }
    if (!mana && *pool == 0u) {
        /* Dead: status bit, drop from v_94F3 active, skill means. */
        member->status |= 0x40u;
        if (combat->room->party != NULL) {
            yt2_party *party = combat->room->party;

            for (unsigned int i = 0u; i < YT2_PARTY_ACTIVE_MAX; i++) {
                if (party->active[i] == slot) {
                    party->active[i] = 0u;
                }
            }
        }
        yt2_room_update_skill_flags(combat->room->party);
    }
}

/* fn_08172 for one staged trap effect (fn_1916C's 0x0C4A slot). */
static void chest_apply_effect(yt2_combat *combat, yt2_character *member,
                               unsigned int slot, unsigned int staging,
                               const uint8_t *def, unsigned int difficulty)
{
    uint16_t flags;
    uint16_t mode;
    unsigned int damage;
    unsigned int resist = 0u;

    flags = chest_read16(def + 8);
    mode = chest_read16(def + 10);
    damage = combat->chest_effect_damage[staging];
    if ((flags & 7u) == 0u && damage == 0u) {
        if ((mode & 0x6000u) == 0u) {
            unsigned int lo = chest_read16(def + 4);
            unsigned int hi = chest_read16(def + 6);

            damage = yt2_create_rand(&combat->room->rng_seed,
                                     combat->room->time_word,
                                     (uint16_t)(hi - lo)) +
                     lo;
        } else {
            damage = chest_read16(def + 4);
        }
        damage *= member->level;
        combat->chest_effect_damage[staging] = (uint16_t)damage;
    }
    /* Status gate (sub_083FA): the roll keeps the 0xFF80 status bits. */
    if ((flags & 0xFF80u) != 0u && (mode & 0x1000u) != 0u) {
        static const uint16_t bits[9] = {0x8000u, 0x4000u, 0x2000u,
                                         0x1000u, 0x800u,  0x400u,
                                         0x200u,  0x100u,  0x80u};
        static const unsigned int buffs[9] = {2u, 1u, 0u, 5u, 4u,
                                              3u, 8u, 7u, 6u};

        for (unsigned int i = 0u; i < 9u; i++) {
            if ((flags & bits[i]) != 0u) {
                resist += member->buffs[buffs[i]];
            }
        }
        if (chest_skill_roll(combat, member->level, difficulty, resist) !=
            0u) {
            member->status |= (uint16_t)(flags & 0xFF80u);
            if ((member->status & 0x1C40u) != 0u &&
                combat->room->party != NULL) {
                yt2_party *party = combat->room->party;

                for (unsigned int i = 0u; i < YT2_PARTY_ACTIVE_MAX; i++) {
                    if (party->active[i] == slot) {
                        party->active[i] = 0u;
                    }
                }
                yt2_room_update_skill_flags(party);
            }
        }
    }
    if ((flags & 0x10u) != 0u) {
        chest_hurt_member(combat, member, slot, damage, 0);
    } else if ((flags & 0x8u) != 0u) {
        chest_hurt_member(combat, member, slot, damage, 1);
    } else if ((flags & 0x20u) != 0u) {
        chest_hurt_member(combat, member, slot, damage, 0);
        chest_hurt_member(combat, member, slot, damage, 1);
    } else if ((flags & 7u) != 0u) {
        /* Currency drain (sub_082BD): clamped BCD subtract of the
           binary damage word plus two always-zero staging bytes. */
        uint8_t *purse = combat->room->party->gold;
        uint8_t blob[4];

        if ((flags & 1u) == 0u) {
            if ((flags & 4u) != 0u) {
                purse = combat->room->party->ore;
            } else if ((flags & 2u) != 0u) {
                purse = combat->room->party->nuore;
            } else {
                return;
            }
        }
        blob[0] = (uint8_t)(damage & 0xFFu);
        blob[1] = (uint8_t)(damage >> 8);
        blob[2] = 0u;
        blob[3] = 0u;
        if (combat_bcd_compare(purse, blob) > 0) {
            chest_bcd_sub(purse, blob);
        } else {
            purse[0] = 0u;
            purse[1] = 0u;
            purse[2] = 0u;
            purse[3] = 0u;
        }
    }
}

/* fn_1916C: trap roll for the opener. 1 = fired, 0 = defeated. */
static unsigned int chest_trap_roll(yt2_combat *combat, unsigned int slot)
{
    yt2_party *party = combat->room->party;
    yt2_character *member = &party->roster[slot - 1u];
    uint16_t word = chest_read16(combat->chest_record + 2);
    unsigned int difficulty = word / 100u;
    unsigned int effect = word % 100u;

    if (word == 0u) {
        return 0u;
    }
    if (chest_skill_roll(combat, member->level, difficulty,
                         member->current.skills.thievery) == 0u) {
        return 0u;
    }
    combat_sfx(combat, chest_read16(chest_effects[effect] + 0));
    combat->room->f5062 |= 4u;
    if (effect < 50u) {
        unsigned int staging = 0u;

        for (unsigned int i = 0u; i < YT2_PARTY_ASSIGNED_MAX; i++) {
            if (party->assigned[i] == slot) {
                staging = i;
                break;
            }
        }
        chest_apply_effect(combat, member, slot, staging,
                           chest_effects[effect], difficulty);
    } else {
        for (unsigned int i = 0u; i < YT2_PARTY_ASSIGNED_MAX; i++) {
            yt2_character *victim;

            if (party->assigned[i] == 0u) {
                break;
            }
            victim = &party->roster[party->assigned[i] - 1u];
            if ((victim->status & 0x1C40u) == 0u) {
                chest_apply_effect(combat, victim, party->assigned[i], i,
                                   chest_effects[effect], difficulty);
            }
        }
    }
    /* fn_08172 tail (fn_15FBC menu sync): flip explore mode bits. */
    party->explore_flags =
        (uint16_t)((party->explore_flags & ~0x2000u) | 0x1000u);
    return 1u;
}

/* Stage one side panel line. */
static void chest_side_panel_line(yt2_combat *combat, unsigned int index,
                            const char *text, uint8_t color, int row)
{
    size_t n;

    for (n = 0u; n < sizeof(combat->chest_side_panel[index]) - 1u &&
                  text[n] != 0;
         n++) {
        combat->chest_side_panel[index][n] = text[n];
    }
    combat->chest_side_panel[index][n] = 0;
    combat->chest_side_panel_color[index] = color;
    combat->chest_side_panel_row[index] = row;
}

/* Copy a side panel message (prompt/take/trap), packed from row 96. */
static void chest_set_side_panel(yt2_combat *combat, const char *l0, uint8_t c0,
                           const char *l1, uint8_t c1, const char *l2,
                           uint8_t c2)
{
    const char *lines[3] = {l0, l1, l2};
    uint8_t colors[3] = {c0, c1, c2};

    combat->chest_side_panel_lines = 0u;
    for (unsigned int i = 0u; i < 3u; i++) {
        if (lines[i] == NULL) {
            break;
        }
        chest_side_panel_line(combat, i, lines[i], colors[i],
                        CHEST_SIDE_PANEL_Y + (int)i * 6);
        combat->chest_side_panel_lines++;
    }
}

/* fn_11F83: the take name paints per field (rows 96/102/108),
   never joined; empty fields stage blank rows like B1. */
static void chest_stage_name(yt2_combat *combat, unsigned int id)
{
    char fields[3][14];

    if (combat->items == NULL || id == 0u) {
        return;
    }
    for (unsigned int i = 0u; i < 3u; i++) {
        if (yt2_item_name_field(combat->items, id - 1u, i, fields[i],
                                sizeof(fields[i])) != YT2_STATUS_OK) {
            return;
        }
    }
    if (combat->chest_side_panel_sticky) {
        combat->chest_side_panel_compass = 1u;
    }
    combat->chest_side_panel_sticky = 1u;
    chest_set_side_panel(combat, fields[0], CHEST_SIDE_PANEL_VALUE_COLOR, fields[1],
                   CHEST_SIDE_PANEL_VALUE_COLOR, fields[2],
                   CHEST_SIDE_PANEL_VALUE_COLOR);
}

/* fn_0795E head: prompt for the opener (prompt 5/6). */
static void chest_prompt_opener(yt2_combat *combat)
{
    uint16_t flags = chest_read16(combat->chest_record + 0);
    const char *second = CHEST_TEXT_OPEN1;

    if ((flags & 2u) != 0u) {
        second = CHEST_TEXT_SEARCH1;
    }
    combat->chest_prompt = 1;
    /* fn_06F86: the prompt wipes the side panel and the compass (VGA). */
    combat->chest_side_panel_compass = 1u;
    chest_set_side_panel(combat, CHEST_TEXT_OPEN0, CHEST_SIDE_PANEL_COLOR, second,
                   CHEST_SIDE_PANEL_COLOR, NULL, 0u);
}

/* fn_0784D: locked-chest info (never opens; live-ungrounded). */
static void chest_lock_info(yt2_combat *combat, yt2_character *member)
{
    uint16_t flags = chest_read16(combat->chest_record + 0);
    uint16_t word = chest_read16(combat->chest_record + 2);
    uint16_t thievery = member->current.skills.thievery;

    /* Prompt-phase text (fn_06F86 already wiped VGA). */
    combat->chest_side_panel_compass = 1u;
    if ((flags & 0x20u) != 0u) {
        chest_set_side_panel(combat, CHEST_TEXT_MAGICALLY, CHEST_SIDE_PANEL_COLOR,
                       CHEST_TEXT_LOCKED, CHEST_SIDE_PANEL_COLOR, NULL, 0u);
        return;
    }
    if ((flags & 0x80u) != 0u) {
        if (thievery < 0x37u || word == 0u) {
            chest_set_side_panel(combat, CHEST_TEXT_NOT_LOCKED, CHEST_SIDE_PANEL_COLOR,
                           NULL, 0u, NULL, 0u);
        } else {
            chest_set_side_panel(combat, CHEST_TEXT_NOT_LOCKED, CHEST_SIDE_PANEL_COLOR,
                           CHEST_TEXT_BUT_TRAPPED, CHEST_SIDE_PANEL_COLOR, NULL,
                           0u);
        }
        return;
    }
    if (thievery < 0x37u) {
        chest_set_side_panel(combat, CHEST_TEXT_LOCKED, CHEST_SIDE_PANEL_COLOR, NULL,
                       0u, NULL, 0u);
        return;
    }
    if (word != 0u && thievery >= 0x41u) {
        chest_set_side_panel(combat, CHEST_TEXT_LOCKED_AND, CHEST_SIDE_PANEL_COLOR,
                       CHEST_TEXT_TRAPPED, CHEST_SIDE_PANEL_COLOR, NULL, 0u);
        return;
    }
    if ((flags & 0xFE00u) == 0u || thievery < 0x50u) {
        chest_set_side_panel(combat, CHEST_TEXT_LOCKED, CHEST_SIDE_PANEL_COLOR, NULL,
                       0u, NULL, 0u);
        return;
    }
    /* Key identification names the required key (static-only). */
    chest_set_side_panel(combat, CHEST_TEXT_REQUIRES, CHEST_SIDE_PANEL_COLOR,
                   CHEST_TEXT_SPECIAL_KEY, CHEST_SIDE_PANEL_COLOR, NULL, 0u);
}

/* fn_073E3 head: enter the modal (stage + paint). */
/* [v_5560]+2 aliases view column 46/49's object word (0x6ED6/0x6EEE
   base + 2): the message counter ++/-- flips the pictured object
   (chest 4 -> 5) while the modal is up. */
static void chest_bump_view(yt2_combat *combat, int down)
{
    yt2_room *room = combat->room;
    unsigned int col = (combat->chest_v5560 == 0x6EEEu) ? 49u : 46u;

    if (col >= YT2_ROOM_VIEW_COLUMNS) {
        return;
    }
    if (down) {
        room->view[col].object--;
    } else {
        room->view[col].object++;
    }
}

static void chest_enter_modal(yt2_combat *combat)
{
    yt2_room *room = combat->room;

    combat_panel_save_modal(combat);
    /* fn_1755C head: the modal forces quick access exclusive. */
    combat->room->party->explore_flags = (uint16_t)(
        (combat->room->party->explore_flags & 0x9FFFu) | 0x1000u);
    chest_stage(combat);
    combat->chest_open = 1;
    combat->chest_prompt = 0;
    /* Entry never touches carried state (D5/D9: close keeps it). */
    room->f5060 |= 0x200u;
    combat->chest_messages++;
    chest_bump_view(combat, 0);
    combat->chest_side_panel_lines = 0u;
    /* fn_073E3 head: the entry clear consumes the sticky. */
    if (combat->chest_side_panel_sticky) {
        combat->chest_side_panel_compass = 1u;
    }
    combat->chest_side_panel_sticky = 0u;
}

/* fn_0795E past the pick: unlock test, trap roll, modal entry. */
static void chest_open_modal(yt2_combat *combat)
{
    yt2_party *party = combat->room->party;
    yt2_character *member = &party->roster[combat->chest_opener - 1u];
    uint16_t flags = chest_read16(combat->chest_record + 0);
    size_t at =
        YT2_CHEST_UNLOCK_OFFSET + (size_t)combat->chest_index / 8u;
    uint8_t mask = (uint8_t)(0x80u >> (combat->chest_index % 8u));

    if (combat->save != NULL && combat->save->bytes != NULL &&
        at < combat->save->size &&
        (combat->save->bytes[at] & mask) != 0u) {
        /* sub_07B1D runs on both paths (SFX + message count). */
        if ((flags & 2u) == 0u) {
            combat_sfx(combat, 6u);
        }
        chest_enter_modal(combat);
        return;
    }
    if ((flags & 0x80u) == 0u) {
        chest_lock_info(combat, member);
        combat->chest_prompt = 0;
        return;
    }
    if (chest_trap_roll(combat, combat->chest_opener) == 0u) {
        /* Defeated: the transient paint is covered by modal entry. */
        combat->chest_side_panel_compass = 1u;
        chest_set_side_panel(combat, CHEST_TEXT_TRAP, CHEST_SIDE_PANEL_COLOR,
                       CHEST_TEXT_DEFEATED, CHEST_SIDE_PANEL_COLOR, NULL, 0u);
    }
    if (combat->save != NULL && combat->save->bytes != NULL &&
        at < combat->save->size) {
        combat->save->bytes[at] |= mask;
        combat->chest_save_index = (uint16_t)(combat->chest_index / 8u);
        combat->room->save_dirty = 1;
    }
    /* sub_07B1D: SFX unless record bit 0x2. */
    if ((flags & 2u) == 0u) {
        combat_sfx(combat, 6u);
    }
    chest_enter_modal(combat);
}

/* The portrait click answers the opener prompt. */
static void chest_pick_opener(yt2_combat *combat, unsigned int slot)
{
    yt2_party *party = combat->room->party;
    yt2_character *member;

    if (slot == 0u || slot > YT2_PARTY_ASSIGNED_MAX) {
        return;
    }
    if (party->assigned[slot - 1u] == 0u) {
        return;
    }
    member = &party->roster[party->assigned[slot - 1u] - 1u];
    if ((member->status & 0x1C40u) != 0u) {
        combat_sfx(combat, 3u);
        return;
    }
    combat->chest_opener = party->assigned[slot - 1u];
    chest_open_modal(combat);
}

/* SPACE on a chest: stale opener skips the prompt (v_94F7). */
static void chest_search_hit(yt2_combat *combat)
{
    yt2_party *party = combat->room->party;

    if (combat->chest_opener != 0u &&
        combat->chest_opener <= YT2_ROSTER_SLOTS) {
        yt2_character *member = &party->roster[combat->chest_opener - 1u];

        if ((member->status & 0x1C40u) == 0u) {
            chest_open_modal(combat);
            return;
        }
        combat_sfx(combat, 3u);
        combat->chest_opener = 0u;
    }
    chest_prompt_opener(combat);
}

/* fn_070EA on a loot cell: instant currency or cursor pickup. */
static void chest_take(yt2_combat *combat, unsigned int cell)
{
    yt2_chest_slot *slot;
    const uint8_t *def;
    const uint8_t *row;
    uint16_t flags;

    if (cell == 0u || cell > YT2_CHEST_SLOTS) {
        return;
    }
    slot = &combat->chest_staging[cell - 1u];
    if (slot->id == 0u) {
        return;
    }
    if (combat_item(combat, slot->id, &def, &row) != YT2_STATUS_OK) {
        return;
    }
    flags = chest_def_flag(def);
    if ((flags & 0x80u) != 0u || (flags & 0x40u) != 0u ||
        (flags & 0x20u) != 0u) {
        /* Instant credit (SFX 7) with the side panel receipt. */
        uint8_t *purse = combat->room->party->gold;
        const char *label = COMBAT_TEXT_GOLD;
        uint8_t amount[4];
        char number[16];
        unsigned int value = slot->count;

        if ((flags & 0x40u) != 0u) {
            purse = combat->room->party->ore;
            label = COMBAT_TEXT_ORE;
        } else if ((flags & 0x20u) != 0u) {
            purse = combat->room->party->nuore;
            label = COMBAT_TEXT_NUORE;
        }
        combat_sfx(combat, 7u);
        combat->chest_carried = slot->id;
        amount[0] = 0u;
        amount[1] = (uint8_t)((value / 10000u) % 10u);
        amount[2] =
            (uint8_t)(((value / 1000u) % 10u) << 4 | ((value / 100u) % 10u));
        amount[3] =
            (uint8_t)(((value / 10u) % 10u) << 4 | (value % 10u));
        combat_bcd_add(purse, amount);
        combat_bcd_format(amount, number, sizeof(number));
        /* fn_070EA currency arm: present, VGA clear, receipt. */
        combat->chest_side_panel_compass = 1u;
        chest_set_side_panel(combat, label, CHEST_SIDE_PANEL_COLOR, number,
                       CHEST_SIDE_PANEL_VALUE_COLOR, NULL, 0u);
        slot->id = 0u;
        slot->count = 0u;
        combat->chest_taken |= (uint8_t)(0x80u >> (cell - 1u));
        return;
    }
    /* Plain pickup to the cursor with the name paint. */
    combat->chest_cursor = chest_def_icon(def);
    combat->chest_carried = slot->id;
    combat->chest_carried_weight = chest_def_weight(def);
    combat->chest_carried_count = slot->count;
    slot->id = 0u;
    slot->count = 0u;
    combat->chest_taken |= (uint8_t)(0x80u >> (cell - 1u));
    chest_stage_name(combat, combat->chest_carried);
}

/* fn_070EA drop-back: the cursor's item returns to its record slot. */
static void chest_drop_back(yt2_combat *combat)
{
    unsigned int id = combat->chest_carried;

    if (combat->chest_cursor == 0u || id == 0u) {
        return;
    }
    for (unsigned int i = 0u; i < YT2_CHEST_SLOTS; i++) {
        if (chest_read16(combat->chest_record + 4u + i * 2u) == id) {
            const uint8_t *def;
            const uint8_t *row;

            if (combat_item(combat, id, &def, &row) != YT2_STATUS_OK) {
                return;
            }
            if ((chest_def_flag(def) & 0x2000u) != 0u &&
                chest_def_weight(def) != combat->chest_carried_weight) {
                combat_sfx(combat, 3u);
                return;
            }
            if ((chest_def_flag(def) & 0x100u) != 0u && row != NULL) {
                uint16_t sub = chest_read16(row + 2u);

                if ((sub & 1u) != 0u &&
                    chest_read16(row + 4u) != combat->chest_carried_count) {
                    combat_sfx(combat, 3u);
                    return;
                }
            }
            combat->chest_staging[i].id = (uint16_t)id;
            combat->chest_staging[i].count = combat->chest_carried_count;
            combat->chest_taken &= (uint8_t)~(0x80u >> i);
            combat->chest_cursor = 0u;
            combat->chest_carried = 0u;
            combat->chest_carried_weight = 0u;
            combat->chest_carried_count = 0u;
            /* loc_072E7 tail: the loot repaint plus the name. */
            chest_stage_name(combat, id);
            return;
        }
    }
    combat_sfx(combat, 3u);
}

/* Currency pane (fn_1765A): three labeled pairs at rows
   96/102/114/120/132/138, then the sticky set. Shared by the
   modal cells and the explore quick access pane (loc_0023B). */
/* fn_1765A proper (silent; the toggle tail owns SFX 2). */
static void chest_currency_stage(yt2_combat *combat)
{
    static const int rows[6] = {96, 102, 114, 120, 132, 138};
    char numbers[3][16];
    const char *labels[3] = {COMBAT_TEXT_GOLD, COMBAT_TEXT_ORE,
                             COMBAT_TEXT_NUORE};
    const uint8_t *purses[3];
    unsigned int i;

    /* fn_1765A's fn_06EA4 consumes a pending award first. */
    combat->award_pending = 0;
    purses[0] = combat->room->party->gold;
    purses[1] = combat->room->party->ore;
    purses[2] = combat->room->party->nuore;
    /* Gated clear first (usually a no-op), then the set. */
    if (combat->chest_side_panel_sticky) {
        combat->chest_side_panel_compass = 1u;
    }
    combat->chest_side_panel_sticky = 1u;
    combat->chest_paint_stale = 1u;
    combat->chest_side_panel_lines = 0u;
    for (i = 0u; i < 3u; i++) {
        combat_bcd_format(purses[i], numbers[i], sizeof(numbers[i]));
        chest_side_panel_line(combat, 2u * i, labels[i], CHEST_SIDE_PANEL_COLOR,
                        rows[2u * i]);
        chest_side_panel_line(combat, 2u * i + 1u, numbers[i],
                        CHEST_SIDE_PANEL_VALUE_COLOR, rows[2u * i + 1u]);
    }
    combat->chest_side_panel_lines = 6u;
}

static void chest_currency_pane(yt2_combat *combat)
{
    /* Modal/cell entry: the click itself blips (stale frame). */
    chest_currency_stage(combat);
    combat_sfx(combat, 2u);
}

/* fn_176EC on a quick access cell: drop, swap, pickup, or the currency pane. */
static void quick_access_click(yt2_combat *combat, unsigned int cell)
{
    if (cell >= 1u && cell <= 3u) {
        chest_currency_pane(combat);
        return;
    }
    if (cell < 4u || cell > 9u) {
        return;
    }
    {
        yt2_chest_slot *slot = &combat->quick_access[cell - 4u];

        if (combat->chest_cursor == 0u) {
            const uint8_t *def;
            const uint8_t *row;

            if (slot->id == 0u) {
                return;
            }
            if (combat_item(combat, slot->id, &def, &row) !=
                YT2_STATUS_OK) {
                return;
            }
            combat->chest_carried = slot->id;
            combat->chest_carried_count = slot->count;
            slot->id = 0u;
            slot->count = 0u;
            /* B1 sub_1782C takes the item's own icon and weight. */
            combat->chest_cursor = chest_def_icon(def);
            combat->chest_carried_weight = chest_def_weight(def);
            /* loc_17858 stages the name WorkSeg-side (quiescently
               invisible: the next action clears or overwrites it),
               but the sticky set is observable at the next clear. */
            combat->chest_side_panel_sticky = 1u;
            return;
        }
        if (slot->id == 0x2Fu && combat->chest_carried >= 0x21u &&
            combat->chest_carried <= 0x2Eu) {
            /* Gem socket (fn_1C09C): live-ungrounded, keep counting. */
            combat->chest_cursor = 0u;
            combat->chest_carried = 0u;
            combat->chest_carried_weight = 0u;
            combat->chest_carried_count = 0u;
            return;
        }
        {
            const uint8_t *def;
            const uint8_t *row;

            if (combat_item(combat, combat->chest_carried, &def,
                            &row) != YT2_STATUS_OK ||
                (chest_def_flag(def) & 0x2000u) != 0u) {
                combat_sfx(combat, 3u);
                return;
            }
        }
        if (slot->id == 0u) {
            combat_sfx(combat, 6u);
            slot->id = combat->chest_carried;
            slot->count = combat->chest_carried_count;
            combat->chest_cursor = 0u;
            combat->chest_carried = 0u;
            combat->chest_carried_weight = 0u;
            combat->chest_carried_count = 0u;
            /* fn_176EC drop: gated clear (the take set the
               sticky), silent paint. */
            if (combat->chest_side_panel_sticky) {
                combat->chest_side_panel_compass = 1u;
            }
            combat->chest_side_panel_sticky = 0u;
            chest_set_side_panel(combat, NULL, 0u, NULL, 0u, NULL, 0u);
            return;
        }
        {
            /* Swap carried and staged. */
            uint16_t id = slot->id;
            uint16_t count = slot->count;
            const uint8_t *def;
            const uint8_t *row;

            combat_sfx(combat, 6u);
            slot->id = combat->chest_carried;
            slot->count = combat->chest_carried_count;
            combat->chest_carried = id;
            combat->chest_carried_count = count;
            combat->chest_carried_weight = 0u;
            if (combat_item(combat, id, &def, &row) == YT2_STATUS_OK) {
                combat->chest_cursor = chest_def_icon(def);
                combat->chest_carried_weight = chest_def_weight(def);
            }
            /* Same WorkSeg staging as pickup (see above). */
            combat->chest_side_panel_sticky = 1u;
        }
    }
}

/* sub_0908B: fresh 0x2000 items mint an instance count. */
static uint16_t chest_mint(yt2_combat *combat, const uint8_t *def,
                           uint16_t count)
{
    if ((chest_def_flag(def) & 0x2000u) == 0u || count != 0u) {
        return count;
    }
    combat->chest_instance_next++;
    return combat->chest_save_index;
}

/* fn_0AACC: apply an equipped item's sub-row bonuses (static-only).
   Offsets below 0x32 apply directly; the rest skip zero cells.
   Out-of-roster offsets read as zero (vanilla reads stale DGROUP;
   machine/state-dependent, treated as absent). */
static void chest_equip_bonuses(yt2_combat *combat, yt2_character *member,
                                unsigned int slot, unsigned int id)
{
    const uint8_t *def;
    const uint8_t *row;
    const uint8_t *sub;
    unsigned int subid;
    uint8_t *base;
    unsigned int touched = 0u;

    if (combat_item(combat, id, &def, &row) != YT2_STATUS_OK) {
        return;
    }
    (void)row;
    subid = chest_read16(def + 2);
    if (subid == 0u || combat->world->bytes == NULL ||
        combat->save == NULL || combat->save->bytes == NULL) {
        return;
    }
    if ((size_t)YT2_ITEM_STRUCT_OFFSET + (size_t)subid + 16u >
        combat->world->size) {
        return;
    }
    base = combat->save->bytes + YT2_SAVE_PARTY_OFFSET + 0x1F4u +
           (size_t)(slot - 1u) * 0x1F4u;
    sub = combat->world->bytes + YT2_ITEM_STRUCT_OFFSET + subid;
    for (unsigned int i = 0u; i < 4u; i++) {
        unsigned int off = chest_read16(sub + i * 4u);
        unsigned int value = chest_read16(sub + i * 4u + 2u);
        unsigned int have;

        if (off == 0u) {
            break;
        }
        if (off >= 0x1F4u) {
            continue;
        }
        have = chest_read16(base + off);
        if (off >= 0x32u && have == 0u) {
            continue;
        }
        have += value;
        if (have > 999u) {
            have = 999u;
        }
        base[off] = (uint8_t)(have & 0xFFu);
        base[off + 1u] = (uint8_t)(have >> 8);
        touched = 1u;
    }
    if (touched) {
        yt2_party_parse_character(base, member);
    }
}

/* fn_0AB61: rederive capacity and base combat bonuses. */
static void combat_rederive(yt2_character *member)
{
    unsigned int str = member->current.attributes.strength;
    unsigned int dex = member->current.attributes.dexterity;
    unsigned int max_str = member->maximum.attributes.strength;
    unsigned int max_dex = member->maximum.attributes.dexterity;

    member->current.combat.carry_capacity = (uint16_t)(str * 10u);
    member->maximum.combat.carry_capacity = (uint16_t)(max_str * 10u);
    member->base[3] = 0u;
    member->base[4] = 0u;
    member->max_base[3] = 0u;
    member->max_base[4] = 0u;
    if (str > 0x48u) {
        member->base[3] = (uint16_t)(((str - 0x48u) * 20u + 50u) / 100u);
    }
    if (dex > 0x48u) {
        member->base[4] = (uint16_t)(((dex - 0x48u) * 20u + 50u) / 100u);
    }
    if (max_str > 0x48u) {
        member->max_base[3] =
            (uint16_t)(((max_str - 0x48u) * 20u + 50u) / 100u);
    }
    if (max_dex > 0x48u) {
        member->max_base[4] =
            (uint16_t)(((max_dex - 0x48u) * 20u + 50u) / 100u);
    }
}

/* fn_0B3D2: recompute current/max combat stats from bases+equipment. */
static void combat_recompute(yt2_combat *combat, yt2_character *member)
{
    yt2_paperdoll_recompute(&combat->paperdoll, member);
}

/* fn_08D3F: portrait drop (equip slots, weight check, pack insert). */
static void chest_portrait_drop(yt2_combat *combat, unsigned int position)
{
    yt2_party *party = combat->room->party;
    yt2_character *member;
    const uint8_t *def;
    const uint8_t *row;
    uint16_t flags;
    unsigned int id;
    unsigned int equipped = 0u;

    if (combat->chest_cursor == 0u || position == 0u ||
        position > YT2_PARTY_ASSIGNED_MAX ||
        party->assigned[position - 1u] == 0u) {
        return;
    }
    id = combat->chest_carried;
    member = &party->roster[party->assigned[position - 1u] - 1u];
    if (id != 0x11u &&
        member->carried_weight + combat->chest_carried_weight >
            member->current.combat.carry_capacity) {
        combat_sfx(combat, 3u);
        return;
    }
    if (combat_item(combat, id, &def, &row) != YT2_STATUS_OK) {
        combat_sfx(combat, 3u);
        return;
    }
    flags = chest_def_flag(def);
    /* Each affinity tries its slot, then falls through to pack. */
    if ((flags & 0x8000u) != 0u) {
        if (member->equipment[0].id == 0u) {
            member->equipment[0].id = (uint16_t)id;
            member->equipment[0].count = combat->chest_carried_count;
            member->wear[0] = 0u;
            equipped = 1u;
        }
    } else if ((flags & 0x2000u) != 0u) {
        if (member->equipment[1].id == 0u) {
            member->equipment[1].id = (uint16_t)id;
            member->equipment[1].count =
                chest_mint(combat, def, combat->chest_carried_count);
            equipped = 1u;
        }
    } else if ((flags & 0x4000u) != 0u) {
        if (member->equipment[2].id == 0u &&
            ((chest_read16(def + 2) & 1u) == 0u ||
             member->equipment[3].id == 0u)) {
            member->equipment[2].id = (uint16_t)id;
            member->equipment[2].count = combat->chest_carried_count;
            member->wear[1] = 0u;
            equipped = 1u;
        }
    } else if ((flags & 0x800u) != 0u) {
        if (member->equipment[3].id == 0u &&
            (member->flags & 0x20u) == 0u) {
            member->equipment[3].id = (uint16_t)id;
            member->equipment[3].count = combat->chest_carried_count;
            member->wear[2] = 0u;
            equipped = 1u;
        }
    } else if ((flags & 0x400u) != 0u) {
        if (member->equipment[4].id == 0u) {
            member->equipment[4].id = (uint16_t)id;
            member->equipment[4].count = combat->chest_carried_count;
            equipped = 1u;
        } else if (member->equipment[5].id == 0u) {
            member->equipment[5].id = (uint16_t)id;
            member->equipment[5].count = combat->chest_carried_count;
            equipped = 1u;
        }
    } else if ((flags & 0x200u) != 0u) {
        static const uint16_t bits[5] = {0x8000u, 0x4000u, 0x2000u, 0x1000u,
                                         0x800u};
        uint16_t sub = chest_read16(def + 2);

        for (unsigned int i = 0u; i < YT2_EFFECT_SLOTS; i++) {
            if ((sub & bits[i]) != 0u) {
                if (member->effects[i] == 0u) {
                    member->effects[i] = (uint16_t)id;
                    equipped = 1u;
                }
                break;
            }
        }
    }
    if (equipped) {
        chest_equip_bonuses(combat, member, party->assigned[position - 1u],
                            id);
        combat_rederive(member);
    } else {
        unsigned int placed = 0u;

        for (unsigned int i = 0u; i < YT2_PACK_SLOTS && !placed; i++) {
            if (member->pack[i].id == 0u) {
                member->pack[i].id = (uint16_t)id;
                member->pack[i].count =
                    chest_mint(combat, def, combat->chest_carried_count);
                placed = 1u;
            }
        }
        if (!placed) {
            combat_sfx(combat, 3u);
            return;
        }
    }
    if (id != 0x11u) {
        member->carried_weight += combat->chest_carried_weight;
    }
    combat_recompute(combat, member);
    yt2_room_update_skill_flags(party);
    combat_sfx(combat, 6u);
    combat->chest_cursor = 0u;
    combat->chest_carried = 0u;
    combat->chest_carried_weight = 0u;
    combat->chest_carried_count = 0u;
    /* loc_07569: gated clear (the take set the sticky),
       silent paint. */
    if (combat->chest_side_panel_sticky) {
        combat->chest_side_panel_compass = 1u;
    }
    combat->chest_side_panel_sticky = 0u;
    chest_set_side_panel(combat, NULL, 0u, NULL, 0u, NULL, 0u);
}

/* loc_075B8: SPACE closes (taken flush + explore render). */
static void chest_close(yt2_combat *combat)
{
    size_t at =
        YT2_CHEST_TAKEN_OFFSET + (size_t)combat->chest_index;
    uint16_t flags = chest_read16(combat->chest_record + 0);

    if (combat->save != NULL && combat->save->bytes != NULL &&
        at < combat->save->size) {
        combat->save->bytes[at] = combat->chest_taken;
        combat->chest_save_index = combat->chest_index;
        /* sub_07AD9 writes through (service 0x0A). */
        combat->room->save_dirty = 1;
    }
    if ((combat->room->f5060 & 0x80u) == 0u && (flags & 2u) == 0u) {
        combat_sfx(combat, 6u);
        if (combat->chest_messages > 0u) {
            combat->chest_messages--;
            chest_bump_view(combat, 1);
        }
    }
    combat->chest_open = 0;
    combat->chest_prompt = 0;
    combat->chest_side_panel_lines = 0u;
    /* Back to explore: the compass repaints fresh. */
    combat->chest_compass_fresh = 1u;
    combat->room->f5060 &= (uint16_t)~0x200u;
    combat_panel_restore_modal(combat);
    /* The close presents stale: repaint the restored panel now
       (B1's fn_11836 + fn_117AC at loc_00465). */
    yt2_combat_panel_paint(combat, combat->surface);
}

/* SPACE search without the present (shared by the key and the
   viewport right-click, which share loc_003F3). Sets *paint when
   the screen changed. */
static int chest_search_action(yt2_combat *combat, int *paint)
{
    uint16_t w1 = 0u;
    uint16_t w2 = 0u;
    unsigned int found = chest_search(combat, &w1, &w2);

    /* Every search paints the side panel, consuming a pending award
       (B1's 0x100 clear inside the search/chest paints). */
    combat->award_pending = 0;
    *paint = 0;
    if (found == 0u) {
        combat->chest_side_panel_compass = 1u;
        chest_set_side_panel(combat, CHEST_TEXT_NOTHING, CHEST_SIDE_PANEL_COLOR,
                       NULL, 0u, NULL, 0u);
        *paint = 1;
        return YT2_STATUS_OK;
    }
    if ((w1 & 0x8000u) == 0u) {
        /* Transfers/talk/stairs (0x2000/0x1000/0x400):
           deferred, silent like a door search. */
        return YT2_STATUS_OK;
    }
    if (chest_load(combat, w2) != YT2_STATUS_OK) {
        return YT2_STATUS_ERROR;
    }
    /* fn_0795E entry: v_5560 follows v_50CC (search hit
       cell: 1 = party, 2 = faced). */
    combat->chest_v5560 = (found == 1u) ? 0x6EEEu : 0x6ED6u;
    chest_search_hit(combat);
    *paint = 1;
    return YT2_STATUS_OK;
}

int yt2_combat_key(yt2_combat *combat, unsigned int ascii,
                   yt2_room_present_fn present, void *context)
{
    yt2_room *room;
    unsigned int a;
    int status;

    if (combat == NULL || combat->room == NULL) {
        return YT2_STATUS_ERROR;
    }
    room = combat->room;
    combat->member_page = 0;
    if (combat->sheet_open != 0u) {
        /* The sheet owns input (B1's loc_096C0 loop); ESC and C
           close, everything else re-polls. */
        if (ascii == 0x1Bu || ascii == 'C') {
            return combat_sheet_close(combat, present, context);
        }
        return YT2_STATUS_OK;
    }
    if (combat->pack_open != 0u) {
        /* The pack UI owns input (B1 fn_08763); P closes, ESC
           buzzes a carried cursor, the verb keys re-poll with
           the deferred slices. */
        if (ascii == 'P') {
            return combat_pack_close(combat, 0u, present, context);
        }
        if (ascii >= '1' && ascii <= '4') {
            /* Member stats over the pack (B1 loc_08C81). */
            return combat_member_page(combat,
                                      (unsigned int)(ascii - '1') + 1u, 4u,
                                      present, context, 1);
        }
        if (ascii == 0x1Bu && combat->chest_cursor != 0u) {
            combat_sfx(combat, 3u);
        }
        return YT2_STATUS_OK;
    }
    if ((room->f5064 & 0x1000u) != 0u) {
        /* The modal owns input (B1's fn_073E3 loop replaces the
           formal loop); rounds wait. Packs and stats paint
           through (B1 loc_07475/loc_07489). */
        if (combat->chest_prompt) {
            return YT2_STATUS_OK;
        }
        if (combat->chest_open) {
            if (ascii == 'P') {
                return combat_pack_open(combat, 0u, present, context);
            }
            if (ascii >= '1' && ascii <= '4') {
                return combat_member_page(
                    combat, (unsigned int)(ascii - '1') + 1u, 4u,
                    present, context, 1);
            }
            return YT2_STATUS_OK;
        }
        if (ascii >= '1' && ascii <= '4') {
            /* Member stats (fn_16044 key path): pure inspect, the
               queue and its pick roll are untouched, no repaint. */
            return combat_member_page(combat,
                                      (unsigned int)(ascii - '1') + 1u, 4u,
                                      present, context, 1);
        }
        if (ascii == 'P') {
            /* Pack UI (loc_068C3). */
            return combat_pack_open(combat, 0u, present, context);
        }
        combat_formal_key(combat, ascii, present, context);
        if (combat->queue_count == 0u &&
            (room->f5064 & 0x1000u) != 0u) {
            /* Round end: wait on the last next-hero frame (B1 does
               not repaint here); a bare-portrait repaint would
               wipe the current-member border. Victory disengaged
               above and still repaints below. */
            status = YT2_STATUS_OK;
        } else {
            status = yt2_combat_repaint(combat, present, context);
        }
    } else if (ascii == 'S') {
        /* The modal owns input (B1's fn_073E3 loop); rounds wait. */
        if (combat->chest_open || combat->chest_prompt) {
            return YT2_STATUS_OK;
        }
        for (a = 0u; a < 4u; a++) {
            combat->award[a][0] = 0u;
            combat->award[a][1] = 0u;
            combat->award[a][2] = 0u;
            combat->award[a][3] = 0u;
        }
        room->f5062 |= 0x100u;
        combat_shoot_round(combat, present, context);
        room->f5062 &= (uint16_t)~0x100u;
        combat_finish_award(combat);
        status = yt2_combat_repaint(combat, present, context);
    } else if (ascii == 'A') {
        return YT2_STATUS_OK;
    } else if (ascii == 'F' || ascii == 'B' || ascii == 'L' ||
               ascii == 'R') {
        unsigned int scancode = YT2_ROOM_SCAN_UP;

        if (combat->chest_open || combat->chest_prompt) {
            /* Vanilla repaints explore here (wiping the modal UI
               while the modal stays open); B2 keeps the modal
               painted instead. Unpinned either way. */
            return YT2_STATUS_OK;
        }
        if (ascii == 'B') {
            scancode = YT2_ROOM_SCAN_DOWN;
        } else if (ascii == 'L') {
            scancode = YT2_ROOM_SCAN_LEFT;
        } else if (ascii == 'R') {
            scancode = YT2_ROOM_SCAN_RIGHT;
        }
        status = yt2_room_key(room, combat->surface, scancode, present,
                              context);
        /* Steps wipe VRAM ink (award/prompt side panels do not repaint). */
        combat->award_pending = 0;
        combat->chest_side_panel_lines = 0u;
        if ((room->f5064 & 0x1000u) != 0u) {
            /* Fresh contact settles in fn_06382 at once (B1 enters
               the formal loop on the engaging step, not the next
               key). */
            yt2_combat_formal_entry(combat, present, context);
            status = yt2_combat_repaint(combat, present, context);
        }
    } else if (ascii == ' ') {
        if (combat->chest_open) {
            chest_close(combat);
            status = yt2_combat_repaint(combat, present, context);
        } else if (!combat->chest_prompt) {
            int paint = 0;

            status = chest_search_action(combat, &paint);
            if (status == YT2_STATUS_OK && paint) {
                status = yt2_combat_repaint(combat, present, context);
            }
        } else {
            return YT2_STATUS_OK;
        }
    } else if (ascii == 0x1Bu || ascii == 'N') {
        if (combat->chest_prompt) {
            /* fn_0795E cancel (ax 0): VGA wipe, back to explore. */
            combat->chest_prompt = 0;
            combat->chest_side_panel_lines = 0u;
            combat->chest_compass_fresh = 1u;
            status = yt2_combat_repaint(combat, present, context);
        } else {
            return YT2_STATUS_OK;
        }
    } else if (ascii >= '1' && ascii <= '4') {
        /* Member stats (fn_16044 key path, B1 loc_07489 over
           the open chest; the prompt ignores them). */
        if (combat->chest_prompt) {
            return YT2_STATUS_OK;
        }
        status = combat_member_page(combat,
                                    (unsigned int)(ascii - '1') + 1u, 4u,
                                    present, context, 1);
    } else if (ascii == 'P') {
        /* Pack UI (loc_0061C, B1 loc_07475 over the open
           chest; the prompt ignores it). */
        if (combat->chest_prompt) {
            return YT2_STATUS_OK;
        }
        status = combat_pack_open(combat, 0u, present, context);
    } else {
        return YT2_STATUS_ERROR;
    }
    if (status == YT2_STATUS_OK && combat->save != NULL &&
        combat->save->bytes != NULL) {
        yt2_party_write(combat->save->bytes + YT2_SAVE_PARTY_OFFSET,
                        YT2_SAVE_PARTY_SIZE, room->party);
    }
    return status;
}

int yt2_combat_fkey(yt2_combat *combat, unsigned int number,
                    yt2_room_present_fn present, void *context)
{
    int was_open;
    int was_prompt;

    if (combat == NULL || combat->room == NULL) {
        return YT2_STATUS_ERROR;
    }
    combat->member_page = 0;
    if (number == 0u || number > 4u) {
        return YT2_STATUS_ERROR;
    }
    if (combat->sheet_open != 0u) {
        /* The sheet switches members (B1 loc_0978D); empty slots
           re-poll inside the open. */
        return combat_sheet_open(combat, number, present, context);
    }
    if (!combat->chest_prompt || combat->chest_open) {
        /* Stat sheet (fn_09619): explore, formal and the open
           modal; the prompt answers F-keys itself below. */
        return combat_sheet_open(combat, number, present, context);
    }
    was_open = combat->chest_open;
    was_prompt = combat->chest_prompt;
    chest_pick_opener(combat, number);
    if (combat->chest_open == was_open &&
        combat->chest_prompt == was_prompt) {
        /* Unassigned (B1 repolls silently) or unfit (the buzz
           drains at the caller): consumed, nothing painted. */
        return YT2_STATUS_OK;
    }
    if (combat->save != NULL && combat->save->bytes != NULL) {
        yt2_party_write(combat->save->bytes + YT2_SAVE_PARTY_OFFSET,
                        YT2_SAVE_PARTY_SIZE, combat->room->party);
    }
    return yt2_combat_repaint(combat, present, context);
}

/* loc_06706: formal side panel click (target select + status tags).
   The first non-empty engaged record whose id range contains the
   hit wins (1-5 record 1, 6-10 record 2 with -5, 11-15 record 3
   with -10); sub 1 selects the target (v_51BA), subs 2-5 set
   flags 0x40/0x200/0x100/0x80 (the monster turn reads 0x100 and
   0x200 via its 0xF010 test). The v_50FE writes have no reader
   on this path and stay out. The caller repaints. */
static void combat_side_panel_click(yt2_combat *combat, int x, int y)
{
    yt2_room *room = combat->room;
    unsigned int id = chest_hit(formal_side_panel, 15u, x, y);
    int rec = -1;
    unsigned int sub = 0u;

    if (id == 0u) {
        return;
    }
    if (room->near_objects[0].id != 0u && id <= 5u) {
        rec = 0;
        sub = id;
    } else if (room->near_objects[1].id != 0u && id <= 10u) {
        rec = 1;
        sub = id - 5u;
    } else if (room->near_objects[2].id != 0u && id <= 15u) {
        rec = 2;
        sub = id - 10u;
    } else {
        return;
    }
    if (sub == 1u) {
        combat->target = rec;
    } else if (sub == 2u) {
        room->near_objects[rec].flags |= 0x40u;
    } else if (sub == 3u) {
        room->near_objects[rec].flags |= 0x200u;
    } else if (sub == 4u) {
        room->near_objects[rec].flags |= 0x100u;
    } else if (sub == 5u) {
        room->near_objects[rec].flags |= 0x80u;
    }
}

/* Formal clicks (sub_06493 regions, 0x669E). Viewport clicks are
   silent with the cursor empty (fn_0A444 returns at once); the
   quick access grid runs the quick access handler; the icon row verbs A (swords)
   and D (disk, nested menu 0x5E) while C (spell) is a deferred
   slice (cast UI) and ZZZ re-polls; the side panel selects targets; the
   bottom strip opens the member inspect pages (fn_16044). Right
   clicks re-poll (their P-verb/inspect arms belong to the
   deferred sheet slice). Verbs present through the caller. */
static int combat_formal_click(yt2_combat *combat, int x, int y, int right,
                               yt2_room_present_fn present, void *context)
{
    unsigned int hit;

    if (right) {
        return YT2_STATUS_OK;
    }
    hit = chest_hit(formal_regions, 6u, x, y);
    if (hit == 1u) {
        return YT2_STATUS_OK;
    }
    if (hit == 2u) {
        unsigned int cell = chest_hit(quick_access_cells, 9u, x, y);

        if (cell == 0u) {
            return YT2_STATUS_OK;
        }
        quick_access_click(combat, cell);
        return YT2_STATUS_OK;
    }
    if (hit == 3u) {
        unsigned int verb = chest_hit(formal_verbs, 4u, x, y);

        if (verb == 1u) {
            combat_formal_key(combat, 'A', present, context);
        } else if (verb == 4u) {
            /* Disk: the in-game menu (mask 0x5E formal). Spell/ZZZ
               match B1's formal verbs (unread, kept silent). */
            combat->menu_requested = 1;
            combat->menu_formal = 1;
        }
        return YT2_STATUS_OK;
    }
    if (hit == 4u) {
        combat_side_panel_click(combat, x, y);
        return YT2_STATUS_OK;
    }
    if (hit == 6u) {
        unsigned int member;
        unsigned int cell;

        /* Bottom-strip inspect (fn_16044 via sub_06493). */
        if (yt2_room_strip_hit(x, y, &member, &cell)) {
            combat_member_page(combat, member,
                               cell <= 2u ? 0u : cell - 2u,
                               present, context, 0);
        }
        return YT2_STATUS_OK;
    }
    return YT2_STATUS_OK;
}

/* Explore clicks (main-loop regions, loc_0016B/loc_00368). Arrows
   move (same KeyCode injection as keys, flash included), viewport
   right-click searches, the quick access pane repaints the currency, bow
   fires S and disk nests the menu; the rest are vanilla silence
   (viewport left, empty cursor) or deferred slices (spell cast, ZZZ
   rest, message link, portraits/equip, member/item inspect). Formal
   combat routes through the 0x669E table above. */
/* Quick access open (fn_1755C + fn_1765A): exclusive flags, grid paint,
   silent currency stage; the caller presents stale. */
static void combat_panel_open_quick_access(yt2_combat *combat)
{
    combat->room->party->explore_flags = (uint16_t)(
        (combat->room->party->explore_flags & 0x8FFFu) | 0x1000u);
    quick_access_paint(combat);
    chest_currency_stage(combat);
}

/* Panel bits across the modal / the formal fight (B1 pushes
   explore_flags & 0x7000 on the stack at open/entry and restores
   with and-0xEFFF/or-saved on close/victory). */
static void combat_panel_save_modal(yt2_combat *combat)
{
    combat->panel_saved_modal =
        (uint16_t)(combat->room->party->explore_flags & 0x7000u);
    combat->panel_saved_modal_valid = 1;
}

static void combat_panel_restore_modal(yt2_combat *combat)
{
    if (!combat->panel_saved_modal_valid) {
        return;
    }
    combat->room->party->explore_flags = (uint16_t)(
        (combat->room->party->explore_flags & 0xEFFFu) |
        combat->panel_saved_modal);
    combat->panel_saved_modal_valid = 0;
}

static void combat_panel_save_formal(yt2_combat *combat)
{
    combat->panel_saved_formal =
        (uint16_t)(combat->room->party->explore_flags & 0x7000u);
    combat->panel_saved_formal_valid = 1;
}

static void combat_panel_restore_formal(yt2_combat *combat)
{
    if (!combat->panel_saved_formal_valid) {
        return;
    }
    combat->room->party->explore_flags = (uint16_t)(
        (combat->room->party->explore_flags & 0xEFFFu) |
        combat->panel_saved_formal);
    combat->panel_saved_formal_valid = 0;
}

/* Quick access close (v_50CC=1): map when available, else chapter. */
static void combat_panel_close_quick_access(yt2_combat *combat)
{
    uint16_t flags = (uint16_t)(combat->room->party->explore_flags &
                                0xEFFFu);

    if ((flags & 0x8000u) != 0u) {
        flags |= 0x4000u;
    } else {
        flags |= 0x2000u;
    }
    combat->room->party->explore_flags = flags;
    yt2_combat_panel_paint(combat, combat->surface);
}

/* F5 (B1 path B): QUICK ACCESS<->MAP with the skill, QUICK ACCESS<->CHAPTER else.
   Stale present, never gather. */
int yt2_combat_panel_toggle(yt2_combat *combat,
                            yt2_room_present_fn present, void *context)
{
    uint16_t flags;

    if (combat == NULL || combat->room == NULL) {
        return YT2_STATUS_ERROR;
    }
    if ((combat->room->f5064 & 0x1000u) != 0u) {
        return YT2_STATUS_ERROR;
    }
    flags = combat->room->party->explore_flags;
    if ((flags & 0x8000u) != 0u) {
        if ((flags & 0x4000u) != 0u) {
            combat_panel_open_quick_access(combat);
            chest_side_panel_paint(combat);
        } else {
            combat->room->party->explore_flags =
                (uint16_t)((flags & 0x8FFFu) | 0x4000u);
            yt2_combat_panel_paint(combat, combat->surface);
        }
    } else if ((flags & 0x2000u) != 0u) {
        combat_panel_open_quick_access(combat);
        chest_side_panel_paint(combat);
    } else {
        combat->room->party->explore_flags =
            (uint16_t)((flags & 0x8FFFu) | 0x2000u);
        yt2_combat_panel_paint(combat, combat->surface);
    }
    combat->chest_paint_stale = 1u;
    combat_sfx(combat, 2u);
    if (present != NULL) {
        present(context);
    }
    return YT2_STATUS_OK;
}

static int chest_explore_click(yt2_combat *combat, int x, int y, int right,
                               yt2_room_present_fn present, void *context)
{
    yt2_room *room = combat->room;
    unsigned int hit = chest_hit(chest_regions, 6u, x, y);

    if (right) {
        int paint = 0;
        unsigned int member;

        /* Right-press portrait opens that member's pack (fn_085CA). */
        if (hit == 6u && yt2_room_portrait_hit(x, y, &member)) {
            return combat_pack_open(combat, member, present, context);
        }
        if (hit != 1u) {
            return YT2_STATUS_ERROR;
        }
        if (chest_search_action(combat, &paint) != YT2_STATUS_OK ||
            !paint) {
            return YT2_STATUS_ERROR;
        }
        return YT2_STATUS_OK;
    }
    if (hit == 5u) {
        static const unsigned int scans[6] = {
            YT2_ROOM_SCAN_LEFT, YT2_ROOM_SCAN_UP, YT2_ROOM_SCAN_RIGHT,
            YT2_ROOM_SCAN_STRAFE_LEFT, YT2_ROOM_SCAN_DOWN,
            YT2_ROOM_SCAN_STRAFE_RIGHT
        };
        unsigned int cell = chest_hit(chest_arrows, 6u, x, y);

        if (cell == 0u) {
            return YT2_STATUS_ERROR;
        }
        /* B1 injects the arrow KeyCode (shared flash+step path). */
        if (yt2_room_key(room, combat->surface, scans[cell - 1u],
                         present, context) != YT2_STATUS_OK) {
            return YT2_STATUS_ERROR;
        }
        /* Steps wipe VRAM ink (mirrors the key path). */
        combat->award_pending = 0;
        combat->chest_side_panel_lines = 0u;
        if ((room->f5064 & 0x1000u) != 0u) {
            /* Fresh contact settles in fn_06382 at once. */
            yt2_combat_formal_entry(combat, present, context);
            return yt2_combat_repaint(combat, present, context);
        }
        return YT2_STATUS_OK;
    }
    if (hit == 3u) {
        unsigned int verb = chest_hit(formal_verbs, 4u, x, y);
        unsigned int a;

        if (verb == 1u) {
            /* Bow (loc_0073C): the S round, shared flash. */
            for (a = 0u; a < 4u; a++) {
                combat->award[a][0] = 0u;
                combat->award[a][1] = 0u;
                combat->award[a][2] = 0u;
                combat->award[a][3] = 0u;
            }
            room->f5062 |= 0x100u;
            combat_shoot_round(combat, present, context);
            room->f5062 &= (uint16_t)~0x100u;
            combat_finish_award(combat);
            return YT2_STATUS_OK;
        }
        if (verb == 4u) {
            /* Disk: the in-game menu (loc_005E4, mask 0xFE). */
            combat->menu_requested = 1;
            combat->menu_formal = 0;
            return YT2_STATUS_OK;
        }
        /* Sun cast (fn_0DE48) and ZZZ rest (fn_0E7B2) are deferred
           slices (B1 explore D is cast too; B2's D-menu is kept as
           a shipped convenience until cast ships). */
        return YT2_STATUS_OK;
    }
    if (hit == 2u) {
        /* B1 loc_0021F: side-panel-clear head, then quick access open/select/close.
           Everything presents stale (loc_00272 never gathers). */
        uint16_t flags = room->party->explore_flags;
        unsigned int cell;

        chest_fill(combat, 240, 96, 72, 60);
        combat->chest_side_panel_lines = 0u;
        combat->chest_paint_stale = 1u;
        if ((flags & 0x1000u) == 0u) {
            combat_panel_open_quick_access(combat);
            combat_sfx(combat, 2u);
            return YT2_STATUS_OK;
        }
        cell = chest_hit(quick_access_cells, 9u, x, y);
        if (cell >= 1u && cell <= 3u) {
            chest_currency_stage(combat);
            return YT2_STATUS_OK;
        }
        if (cell >= 4u && cell <= 9u &&
            (combat->chest_cursor != 0u ||
             combat->quick_access[cell - 4u].id != 0u)) {
            quick_access_click(combat, cell);
            return YT2_STATUS_OK;
        }
        combat_panel_close_quick_access(combat);
        combat_sfx(combat, 2u);
        return YT2_STATUS_OK;
    }
    if (hit == 6u) {
        unsigned int member;
        unsigned int cell;

        /* Bottom-strip inspect (fn_16044 mouse path). */
        if (!yt2_room_strip_hit(x, y, &member, &cell)) {
            return YT2_STATUS_ERROR;
        }
        return combat_member_page(combat, member,
                                  cell <= 2u ? 0u : cell - 2u,
                                  present, context, 0);
    }
    return YT2_STATUS_ERROR;
}

int yt2_combat_click(yt2_combat *combat, int x, int y, int right,
                     yt2_room_present_fn present, void *context)
{
    unsigned int hit;

    if (combat == NULL || combat->room == NULL) {
        return YT2_STATUS_ERROR;
    }
    combat->member_page = 0;
    if (combat->sheet_open != 0u) {
        return combat_sheet_click(combat, x, y, right, present, context);
    }
    if (combat->pack_open != 0u) {
        return combat_pack_click(combat, x, y, right, present, context);
    }
    if ((combat->room->f5064 & 0x1000u) != 0u && !combat->chest_open &&
        !combat->chest_prompt) {
        return combat_formal_click(combat, x, y, right, present, context);
    }
    if (combat->chest_prompt && !combat->chest_open) {
        static const unsigned int ids[4] = {1u, 11u, 21u, 31u};

        /* The prompt answers left clicks and Y/N/ESC only. */
        if (right) {
            return YT2_STATUS_ERROR;
        }
        hit = chest_hit(chest_portraits, 4u, x, y);
        for (unsigned int i = 0u; i < 4u; i++) {
            if (hit == ids[i]) {
                chest_pick_opener(combat, i + 1u);
                return YT2_STATUS_OK;
            }
        }
        return YT2_STATUS_OK;
    }
    if (!combat->chest_open) {
        return chest_explore_click(combat, x, y, right, present,
                                   context);
    }
    hit = chest_hit(chest_regions, 6u, x, y);
    if (right) {
        /* B1 loc_07586: viewport closes like SPACE, strip
           portraits pick single pack panes (fn_085CA);
           chapter examines (deferred), the rest spins. */
        unsigned int member;

        if (hit == 1u) {
            chest_close(combat);
            return yt2_combat_repaint(combat, present, context);
        }
        if (hit == 6u && yt2_room_portrait_hit(x, y, &member)) {
            return combat_pack_open(combat, member, present, context);
        }
        return YT2_STATUS_OK;
    }
    if (hit == 5u) {
        unsigned int cell = chest_hit(chest_loot_cells, 8u, x, y);

        if (cell == 0u) {
            return YT2_STATUS_OK;
        }
        if (combat->chest_cursor != 0u) {
            chest_drop_back(combat);
        } else {
            chest_take(combat, cell);
        }
        return YT2_STATUS_OK;
    }
    if (hit == 2u) {
        unsigned int cell = chest_hit(quick_access_cells, 9u, x, y);

        if (cell != 0u) {
            quick_access_click(combat, cell);
        }
        return YT2_STATUS_OK;
    }
    if (hit == 6u) {
        static const unsigned int ids[4] = {1u, 11u, 21u, 31u};

        hit = chest_hit(chest_portraits, 4u, x, y);
        for (unsigned int i = 0u; i < 4u; i++) {
            if (hit == ids[i]) {
                if (combat->chest_cursor != 0u) {
                    chest_portrait_drop(combat, i + 1u);
                }
                /* Empty-cursor portrait presses are vanilla
                   silence (fn_16044 ignores the rect); holds open
                   the sheet via yt2_combat_hold. */
                return YT2_STATUS_OK;
            }
        }
        if (combat->chest_cursor == 0u) {
            unsigned int member;
            unsigned int cell;

            /* Status-cell inspect (fn_16044 via fn_073E3). */
            if (yt2_room_strip_hit(x, y, &member, &cell)) {
                return combat_member_page(combat, member,
                                          cell <= 2u ? 0u : cell - 2u,
                                          present, context, 0);
            }
        }
        return YT2_STATUS_OK;
    }
    /* Regions 1/3/4: viewport drops need prompt 4 (deferred with the
       ground spawn); the rest ignore clicks. */
    return YT2_STATUS_OK;
}

void yt2_combat_sheet_retain(yt2_combat *combat)
{
    yt2_room *room;
    unsigned int slot;
    unsigned int i;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL) {
        return;
    }
    room = combat->room;
    if (combat->sheet_open != 0u && combat->sheet_open <= 4u) {
        slot = room->party->assigned[combat->sheet_open - 1u];
        if (slot != 0u && slot <= YT2_ROSTER_SLOTS) {
            combat_paint_sheet(combat, &room->party->roster[slot - 1u],
                               slot);
            combat_paint_highlight_at(combat, combat->sheet_open - 1u);
        }
    }
    if (combat->pack_open != 0u) {
        for (i = 1u; i <= 4u; i++) {
            if ((combat->pack_open & (0x4000u >> (i - 1u))) != 0u) {
                pack_paint_member(combat, i);
            }
        }
    }
}

int yt2_combat_hold(yt2_combat *combat, int x, int y,
                    yt2_room_present_fn present, void *context)
{
    unsigned int member;

    if (combat == NULL || combat->room == NULL ||
        combat->room->party == NULL) {
        return YT2_STATUS_ERROR;
    }
    combat->member_page = 0;
    if (combat->sheet_open != 0u) {
        /* Holds re-poll under the sheet. */
        return YT2_STATUS_OK;
    }
    if (combat->chest_open) {
        /* Stat sheet (fn_09619 via fn_073E3's hold arm). */
        if (combat->chest_cursor == 0u &&
            yt2_room_portrait_hit(x, y, &member)) {
            return combat_sheet_open(combat, member, present, context);
        }
        return YT2_STATUS_OK;
    }
    if (combat->chest_prompt ||
        (combat->room->f5064 & 0x1000u) != 0u) {
        /* Prompt and formal holds re-poll (formal quick access holds ride
           with the pack UI). */
        return YT2_STATUS_OK;
    }
    if ((combat->room->party->explore_flags & 0x1000u) != 0u) {
        /* Pack-open holds over portraits open the sheet (B1
           loc_0889D via fn_09619); item holds ride with the
           deferred slices. */
        if (yt2_room_portrait_hit(x, y, &member)) {
            return combat_sheet_open(combat, member, present, context);
        }
        return YT2_STATUS_OK;
    }
    /* Stat sheet (fn_09619 via loc_0033F). */
    if (yt2_room_portrait_hit(x, y, &member)) {
        return combat_sheet_open(combat, member, present, context);
    }
    return YT2_STATUS_OK;
}

int yt2_combat_rhold(yt2_combat *combat, int x, int y,
                     yt2_room_present_fn present, void *context)
{
    if (combat == NULL || combat->room == NULL) {
        return YT2_STATUS_ERROR;
    }
    combat->member_page = 0;
    if (x < 0 || x > 232 || y < 144 || y > 199) {
        return YT2_STATUS_OK;
    }
    if (combat->sheet_open != 0u || combat->chest_prompt) {
        /* The sheet and the opener prompt re-poll holds. */
        return YT2_STATUS_OK;
    }
    if (combat->pack_open != 0u) {
        return combat_pack_close(combat, 0u, present, context);
    }
    /* Strip holds open all panes (B1 loc_0061C in explore,
       loc_07682 over the chest). */
    return combat_pack_open(combat, 0u, present, context);
}

unsigned int yt2_combat_sfx_count(const yt2_combat *combat)
{
    return combat == NULL ? 0u : combat->sfx_count;
}

unsigned int yt2_combat_sfx_at(const yt2_combat *combat, unsigned int index)
{
    if (combat == NULL || index >= combat->sfx_count) {
        return 0u;
    }
    return combat->sfx[index];
}
