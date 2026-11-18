#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/combat.h"
#include "yt2/items.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/room.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

static int test_present_count(void *context)
{
    unsigned int *frames = (unsigned int *)context;

    if (frames != NULL) {
        (*frames)++;
    }
    return YT2_STATUS_OK;
}

/* Mock pacing: counts DelayTicks instead of sleeping. */
static void test_delay_count(unsigned int ticks, void *context)
{
    unsigned int *total = (unsigned int *)context;

    if (total != NULL) {
        *total += ticks;
    }
}

/* Live formal fixture: real data files, the spider-staging save,
   a tanked spider engaged in slot 1. Callers set HP/seed/keys. */
typedef struct test_formal {
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_room room;
    yt2_surface surface;
    yt2_combat combat;
    unsigned int frames;
} test_formal;

static int test_formal_init(test_formal *fix)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");
    const char *save_path = getenv("YT2_COMBAT_SAVE");
    yt2_room_object *spider;

    if (pictures_path == NULL || world_path == NULL ||
        save_path == NULL) {
        fprintf(stderr, "FAIL test_combat: missing fixture env\n");
        test_failures++;
        return 0;
    }
    CHECK(yt2_pictures_load(pictures_path, &fix->pictures) ==
          YT2_STATUS_OK);
    CHECK(yt2_world_load(world_path, &fix->world) == YT2_STATUS_OK);
    CHECK(yt2_items_load(&fix->items, &fix->world) == YT2_STATUS_OK);
    CHECK(yt2_save_load(save_path, &fix->save) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&fix->surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_room_init(&fix->room, &fix->world, &fix->pictures, &fix->save,
                        &fix->save.party, 0x1234u) == YT2_STATUS_OK);
    CHECK(yt2_combat_init(&fix->combat, &fix->room, &fix->save,
                          &fix->pictures, &fix->world, &fix->items,
                          &fix->surface) == YT2_STATUS_OK);
    fix->room.rng_seed = 11312u;
    fix->room.time_word = 0x1234u;
    fix->room.f5064 |= 0x1000u;
    spider = &fix->room.near_objects[1];
    spider->id = 10u;
    spider->hp = 2000u;
    spider->hp_max = 200u;
    spider->flags = 1u;
    spider->initiative = 54u;
    spider->defense = 9u;
    spider->power = 2u;
    spider->attack_base = 67u;
    spider->attack_mult = 5u;
    spider->sfx_hit = 32u;
    spider->sfx_miss = 32u;
    spider->fx_base = 2u;
    spider->fx_special = 0u;
    spider->group = 0xC011u;
    spider->anim_base = 30u;
    spider->pic = 32u;
    spider->target = 0u;
    fix->combat.target = 1;
    fix->frames = 0u;
    return 1;
}

static void test_formal_free(test_formal *fix)
{
    yt2_surface_free(&fix->surface);
    yt2_save_free(&fix->save);
}

int test_combat(void)
{
    /* Init rejects missing state; behavior lands with the rounds. */
    {
        yt2_combat combat;

        CHECK(yt2_combat_init(NULL, NULL, NULL, NULL, NULL, NULL, NULL) ==
              YT2_STATUS_ERROR);
        (void)combat;
    }
    /* Null combat rejects keys, clicks and repaints. */
    {
        CHECK(yt2_combat_key(NULL, 'S', NULL, NULL) == YT2_STATUS_ERROR);
        CHECK(yt2_combat_click(NULL, 0, 0, 0, NULL, NULL) ==
              YT2_STATUS_ERROR);
        CHECK(yt2_combat_repaint(NULL, NULL, NULL) == YT2_STATUS_ERROR);
        CHECK(yt2_combat_fkey(NULL, 1u, NULL, NULL) == YT2_STATUS_ERROR);
    /* F1-F4 pick the chest opener like portrait clicks (B1 fn_0A834):
       F2 opens with member 2, an unfit member buzzes (sfx 3) and the
       prompt stays, an unassigned slot is consumed silently, and with
       no prompt open (or a bad number) the key is an error. */
    {
        test_formal fix;
        unsigned int frames = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        memset(fix.combat.chest_record, 0, YT2_CHEST_RECORD_SIZE);
        fix.combat.chest_index = 0u;
        fix.save.bytes[YT2_CHEST_UNLOCK_OFFSET] |= 0x80u;
        fix.combat.chest_prompt = 1;
        CHECK(yt2_combat_fkey(&fix.combat, 2u, test_present_count,
                              &frames) == YT2_STATUS_OK);
        CHECK(fix.combat.chest_open == 1);
        CHECK(fix.combat.chest_opener == fix.room.party->assigned[1]);
        CHECK(frames > 0u);
        CHECK(yt2_combat_sfx_count(&fix.combat) == 1u);
        CHECK(yt2_combat_sfx_at(&fix.combat, 0u) == 6u);
        test_formal_free(&fix);
    }
    /* Unfit and unassigned opener picks. */
    {
        test_formal fix;
        unsigned int frames = 0u;
        yt2_party *party;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        party = fix.room.party;
        memset(fix.combat.chest_record, 0, YT2_CHEST_RECORD_SIZE);
        fix.combat.chest_index = 0u;
        fix.save.bytes[YT2_CHEST_UNLOCK_OFFSET] |= 0x80u;
        fix.combat.chest_prompt = 1;
        party->roster[party->assigned[0] - 1u].status |= 0x1C40u;
        CHECK(yt2_combat_fkey(&fix.combat, 1u, test_present_count,
                              &frames) == YT2_STATUS_OK);
        CHECK(fix.combat.chest_open == 0);
        CHECK(fix.combat.chest_prompt == 1);
        CHECK(frames == 0u);
        CHECK(yt2_combat_sfx_count(&fix.combat) == 1u);
        CHECK(yt2_combat_sfx_at(&fix.combat, 0u) == 3u);
        party->assigned[2] = 0u;
        CHECK(yt2_combat_fkey(&fix.combat, 3u, test_present_count,
                              &frames) == YT2_STATUS_OK);
        CHECK(fix.combat.chest_open == 0);
        CHECK(fix.combat.chest_prompt == 1);
        CHECK(frames == 0u);
        CHECK(yt2_combat_sfx_count(&fix.combat) == 1u);
        fix.combat.chest_prompt = 0;
        CHECK(yt2_combat_fkey(&fix.combat, 1u, test_present_count,
                              &frames) == YT2_STATUS_ERROR);
        fix.combat.chest_prompt = 1;
        CHECK(yt2_combat_fkey(&fix.combat, 0u, test_present_count,
                              &frames) == YT2_STATUS_ERROR);
        CHECK(yt2_combat_fkey(&fix.combat, 5u, test_present_count,
                              &frames) == YT2_STATUS_ERROR);
        test_formal_free(&fix);
    }
    /* Portrait clicks map to the same opener slots. */
    {
        test_formal fix;
        unsigned int frames = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        memset(fix.combat.chest_record, 0, YT2_CHEST_RECORD_SIZE);
        fix.combat.chest_index = 0u;
        fix.save.bytes[YT2_CHEST_UNLOCK_OFFSET] |= 0x80u;
        fix.combat.chest_prompt = 1;
        CHECK(yt2_combat_click(&fix.combat, 139, 163, 0,
                               test_present_count,
                               &frames) == YT2_STATUS_OK);
        CHECK(fix.combat.chest_open == 1);
        CHECK(fix.combat.chest_opener == fix.room.party->assigned[2]);
        test_formal_free(&fix);
    }
    }
    /* The SFX log reads empty and clamps out of range. */
    {
        CHECK(yt2_combat_sfx_count(NULL) == 0u);
        CHECK(yt2_combat_sfx_at(NULL, 0u) == 0u);
    }
    /* Monster melee: a tanked spider (HP 2000, single-target)
       acts inside the A loop, poses (4 presents), hits its
       queued target (HP drops, SFX 32 + effect 13), and the
       round completes. Seed-pinned B2-side (formulas are
       B1-verbatim); the matrix spider dies before it can act,
       so the matrix cannot cover this. */
    {
        test_formal fix;
        yt2_room_object *spider;
        unsigned int i;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        spider = &fix.room.near_objects[1];
        for (i = 0u; i < 6u; i++) {
            unsigned int s;

            CHECK(yt2_combat_key(&fix.combat, 'A', test_present_count,
                                 &fix.frames) == YT2_STATUS_OK);
            for (s = 0u; s < yt2_combat_sfx_count(&fix.combat); s++) {
                if (yt2_combat_sfx_at(&fix.combat, s) == 32u) {
                    break;
                }
            }
            if (s < yt2_combat_sfx_count(&fix.combat)) {
                break;
            }
        }
        CHECK(i < 6u);
        /* Seed 11312, first key: member 1 hits (dagger 38, spider
           2000 -> 1998), the spider poses and single-hits its
           queued target member 4 (32 + effect 13, HP 12 -> 9).
           Ten presents: melee 3, pose 4, apply 2, final 1. */
        CHECK(fix.frames == 10u);
        CHECK(yt2_combat_sfx_count(&fix.combat) == 3u);
        CHECK(yt2_combat_sfx_at(&fix.combat, 0u) == 38u);
        CHECK(yt2_combat_sfx_at(&fix.combat, 1u) == 32u);
        CHECK(yt2_combat_sfx_at(&fix.combat, 2u) == 13u);
        CHECK(fix.combat.death_pending == 0);
        CHECK(spider->hp == 1998u);
        {
            static const unsigned int want[4] = {13u, 15u, 12u, 9u};

            for (i = 0u; i < 4u; i++) {
                unsigned int slot = fix.room.party->assigned[i];
                unsigned int hp = fix.save.party.roster[slot - 1u].current
                                      .combat.hit_points;

                CHECK(hp == want[i]);
            }
        }
        test_formal_free(&fix);
    }
    /* Wipe: with every member at 1 HP the spider kills its
       queued target per round and retargets the living; four
       rounds wipe, paint the death screen (SFX 19) and flag the
       caller for menu 0x70 + load (fn_15FBC/fn_1B382). */
    {
        test_formal fix;
        unsigned int i;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.near_objects[1].initiative = 200u;
        for (i = 0u; i < 4u; i++) {
            unsigned int slot = fix.room.party->assigned[i];

            fix.save.party.roster[slot - 1u].current.combat.hit_points = 1u;
        }
        for (i = 0u; i < 25u && fix.combat.death_pending == 0; i++) {
            CHECK(yt2_combat_key(&fix.combat, 'A', test_present_count,
                                 &fix.frames) == YT2_STATUS_OK);
        }
        CHECK(fix.combat.death_pending == 1);
        {
            unsigned int found = 0u;

            for (i = 0u; i < yt2_combat_sfx_count(&fix.combat); i++) {
                if (yt2_combat_sfx_at(&fix.combat, i) == 19u) {
                    found = 1u;
                }
            }
            CHECK(found == 1u);
        }
        test_formal_free(&fix);
    }
    /* Side panel exclusion (B1's 0x100 consume): a pending award never
       paints together with engaged names, search text, or the
       currency pane; the newest writer wins. */
    {
        test_formal fix;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        /* Award plus formal repaint: names win, award clears. */
        fix.combat.award_pending = 1;
        CHECK(yt2_combat_repaint(&fix.combat, test_present_count,
                                 &fix.frames) == YT2_STATUS_OK);
        CHECK(fix.combat.award_pending == 0);
        /* Award plus search: search text wins, award clears. */
        fix.room.f5064 &= (uint16_t)~0x1000u;
        fix.combat.award_pending = 1;
        CHECK(yt2_combat_key(&fix.combat, ' ', test_present_count,
                             &fix.frames) == YT2_STATUS_OK);
        CHECK(fix.combat.award_pending == 0);
        test_formal_free(&fix);
    }
    /* Idle animation: a ranged spider advances one phase per
       stale render (B1's per-poll fn_10E6A); the live session
       re-renders stale on tick-dirty in explore, so spiders
       dance under the palette like B1. */
    {
        test_formal fix;
        yt2_room_object *spider;
        static uint8_t before[320u * 200u];
        unsigned int diffs = 0u;
        unsigned int i;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.f5064 &= (uint16_t)~0x1000u;
        memset(&fix.room.near_objects[1], 0,
               sizeof(fix.room.near_objects[1]));
        spider = &fix.room.objects[0];
        spider->id = 10u;
        spider->hp = 8u;
        spider->hp_max = 8u;
        spider->flags = 1u;
        spider->group = 0xC011u;
        spider->anim_base = 30u;
        spider->pic = 32u;
        spider->mode = 0x0Au;
        spider->x = 161u;
        spider->y = 32u;
        fix.room.view[25].flags |= YT2_ROOM_CELL_OBJECT;
        fix.room.view[25].misc = 10u;
        CHECK(yt2_room_render_stale(&fix.room, &fix.surface) ==
              YT2_STATUS_OK);
        /* Ping-pong down (no bit 3, pic above base). */
        CHECK(spider->pic == 31u);
        memcpy(before, fix.surface.pixels, sizeof(before));
        CHECK(yt2_room_render_stale(&fix.room, &fix.surface) ==
              YT2_STATUS_OK);
        CHECK(spider->pic == 30u);
        for (i = 0u; i < sizeof(before); i++) {
            if (before[i] != fix.surface.pixels[i]) {
                diffs++;
            }
        }
        CHECK(diffs > 0u);
        test_formal_free(&fix);
    }
    /* Pacing (fn_DelayTicks counts): the S flight holds 2 per
       band over the empty view (0x31 plus five checked bands)
       plus the 2-tick flash; melee holds 2 + 2 + 3. Live
       sessions sleep 55 ms per tick; batch NULL-skips. */
    {
        test_formal fix;
        unsigned int slept = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.delay = test_delay_count;
        fix.room.delay_context = &slept;
        /* Explore S over the empty view: flash 2 + six bands
           (0x31 plus the five checked). */
        fix.room.f5064 &= (uint16_t)~0x1000u;
        memset(&fix.room.near_objects[1], 0,
               sizeof(fix.room.near_objects[1]));
        CHECK(yt2_combat_key(&fix.combat, 'S', test_present_count,
                             &fix.frames) == YT2_STATUS_OK);
        CHECK(slept == 2u + 6u * 2u);
        /* Formal A (seed key 0: member 1 plus the spider): melee
           2 + 2 + 3 and pose 3 x 2. */
        slept = 0u;
        fix.room.f5064 |= 0x1000u;
        {
            yt2_room_object *spider = &fix.room.near_objects[1];

            spider->id = 10u;
            spider->hp = 2000u;
            spider->hp_max = 200u;
            spider->flags = 1u;
            spider->initiative = 54u;
            spider->defense = 9u;
            spider->power = 2u;
            spider->attack_base = 67u;
            spider->attack_mult = 5u;
            spider->sfx_hit = 32u;
            spider->sfx_miss = 32u;
            spider->fx_base = 2u;
            spider->fx_special = 0u;
            spider->group = 0xC011u;
            spider->anim_base = 30u;
            spider->pic = 32u;
        }
        fix.combat.target = 1;
        fix.room.rng_seed = 11312u;
        fix.room.time_word = 0x1234u;
        CHECK(yt2_combat_key(&fix.combat, 'A', test_present_count,
                             &fix.frames) == YT2_STATUS_OK);
        CHECK(slept == (2u + 2u + 3u) + 3u * 2u);
        test_formal_free(&fix);
    }
    return test_failures == 0;
}
