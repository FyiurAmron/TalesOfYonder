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

#define NAME_CAP 64u

typedef struct test_name_capture {
    test_formal *fix;
    unsigned int ink[NAME_CAP];
    unsigned int count;
} test_name_capture;

/* Present hook: non-background pixels on engaged row 2 (y 123-128). */
static int test_present_name(void *context)
{
    test_name_capture *cap = (test_name_capture *)context;
    unsigned int ink = 0u;
    unsigned int x;
    unsigned int y;

    if (cap == NULL || cap->fix == NULL || cap->count >= NAME_CAP) {
        return YT2_STATUS_OK;
    }
    for (x = 241u; x < 313u; x++) {
        for (y = 123u; y < 129u; y++) {
            uint8_t p = cap->fix->surface.pixels[(size_t)y *
                        cap->fix->surface.stride + x];

            if (p != 0u && p != 4u) {
                ink++;
            }
        }
    }
    cap->ink[cap->count++] = ink;
    return YT2_STATUS_OK;
}

#define HIGHLIGHT_CAP 64u

typedef struct highlight_capture {
    test_formal *fix;
    unsigned int borders[HIGHLIGHT_CAP];
    unsigned int currents[HIGHLIGHT_CAP];
    unsigned int allborders[HIGHLIGHT_CAP][4];
    unsigned int count;
    /* Skip probe: victim attacker index + violation latch. */
    unsigned int victim;
    int handed_while_dead;
} highlight_capture;

/* Present hook: color-219 pixels on the current member's 32x32 border. */
static int test_present_highlight(void *context)
{
    highlight_capture *cap = (highlight_capture *)context;
    int x = 0;
    int y = 0;
    unsigned int b = 0u;
    unsigned int i;

    if (cap == NULL || cap->fix == NULL || cap->count >= HIGHLIGHT_CAP) {
        return YT2_STATUS_OK;
    }
    if (yt2_room_portrait_origin(cap->fix->combat.current, &x, &y) ==
        YT2_STATUS_OK) {
        for (i = 0u; i < 32u; i++) {
            if ((unsigned int)(x + i) >= cap->fix->surface.width ||
                (unsigned int)(y + i) >= cap->fix->surface.height) {
                continue;
            }
            if (cap->fix->surface.pixels[(size_t)y *
                        cap->fix->surface.stride + (x + i)] == 219u) {
                b++;
            }
            if (cap->fix->surface.pixels[(size_t)(y + 31) *
                        cap->fix->surface.stride + (x + i)] == 219u) {
                b++;
            }
            if (cap->fix->surface.pixels[(size_t)(y + i) *
                        cap->fix->surface.stride + x] == 219u) {
                b++;
            }
            if (cap->fix->surface.pixels[(size_t)(y + i) *
                        cap->fix->surface.stride + (x + 31)] == 219u) {
                b++;
            }
        }
    }
    cap->borders[cap->count] = b;
    cap->currents[cap->count] = cap->fix->combat.current;
    for (unsigned int s = 0u; s < 4u; s++) {
        int sx = 0;
        int sy = 0;
        unsigned int sb = 0u;
        unsigned int j;

        if (yt2_room_portrait_origin(s, &sx, &sy) != YT2_STATUS_OK) {
            cap->allborders[cap->count][s] = 0u;
            continue;
        }
        for (j = 0u; j < 32u; j++) {
            if ((unsigned int)(sx + j) >= cap->fix->surface.width ||
                (unsigned int)(sy + j) >= cap->fix->surface.height) {
                continue;
            }
            if (cap->fix->surface.pixels[(size_t)sy *
                        cap->fix->surface.stride + (sx + j)] == 219u) {
                sb++;
            }
            if (cap->fix->surface.pixels[(size_t)(sy + 31) *
                        cap->fix->surface.stride + (sx + j)] == 219u) {
                sb++;
            }
            if (cap->fix->surface.pixels[(size_t)(sy + j) *
                        cap->fix->surface.stride + sx] == 219u) {
                sb++;
            }
            if (cap->fix->surface.pixels[(size_t)(sy + j) *
                        cap->fix->surface.stride + (sx + 31)] == 219u) {
                sb++;
            }
        }
        cap->allborders[cap->count][s] = sb;
    }
    cap->count++;
    if (cap->fix->combat.current == cap->victim) {
        yt2_party *party = cap->fix->room.party;
        unsigned int slot = party->assigned[cap->victim];

        if (slot != 0u && slot <= YT2_ROSTER_SLOTS &&
            (party->roster[slot - 1u].status & 0x1C40u) != 0u) {
            cap->handed_while_dead = 1;
        }
    }
    return YT2_STATUS_OK;
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
    }
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
    /* Formal rounds wait out the prompt/modal (B1's fn_073E3 loop
       replaces the formal loop): A builds no queue and presents
       nothing while either is open. */
    {
        test_formal fix;
        unsigned int frames = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.f5064 |= 0x1000u;
        fix.combat.chest_prompt = 1;
        CHECK(yt2_combat_key(&fix.combat, 'A', test_present_count,
                             &frames) == YT2_STATUS_OK);
        CHECK(fix.combat.queue_count == 0u);
        CHECK(frames == 0u);
        fix.combat.chest_prompt = 0;
        fix.combat.chest_open = 1;
        CHECK(yt2_combat_key(&fix.combat, 'A', test_present_count,
                             &frames) == YT2_STATUS_OK);
        CHECK(fix.combat.queue_count == 0u);
        CHECK(frames == 0u);
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
           Twelve presents: next 1, melee 3, pose 4, apply 2,
           next 1, final 1. */
        CHECK(fix.frames == 12u);
        /* The next-hero call repaints the current-member border: the
           last two presents (next, final) both carry the full 128px
           border. Without the restore the damage applier's fresh
           portraits wipe it and only the final has it. */
        {
            test_formal probe;
            highlight_capture cap;

            if (!test_formal_init(&probe)) {
                return 0;
            }
            memset(&cap, 0, sizeof(cap));
            cap.fix = &probe;
            CHECK(yt2_combat_key(&probe.combat, 'A',
                                 test_present_highlight,
                                 &cap) == YT2_STATUS_OK);
            CHECK(cap.count >= 2u);
            CHECK(cap.borders[cap.count - 1u] == 128u);
            CHECK(cap.borders[cap.count - 2u] == 128u);
            /* Round end (formal, queue exhausted): B1 waits on the
               last next-hero frame, so the border survives; a bare
               tail repaint would wipe it. */
            for (unsigned int round = 0u; round < 8u; round++) {
                if ((probe.room.f5064 & 0x1000u) != 0u &&
                    probe.combat.queue_count == 0u) {
                    break;
                }
                CHECK(yt2_combat_key(&probe.combat, 'A',
                                     test_present_highlight,
                                     &cap) == YT2_STATUS_OK);
            }
            CHECK((probe.room.f5064 & 0x1000u) != 0u);
            CHECK(probe.combat.queue_count == 0u);
            CHECK(cap.borders[cap.count - 1u] == 128u);
            test_formal_free(&probe);
        }
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
    /* Dead members never take turns (sub_06493 skips them): the
       spider (initiative first) kills attacker 3, and the present
       hook latches if attacker 3 is ever current while dead. */
    {
        test_formal fix;
        highlight_capture cap;
        yt2_character *victim;
        unsigned int slot;
        unsigned int i;
        uint16_t spider_hp;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.near_objects[1].initiative = 200u;
        slot = fix.room.party->assigned[3];
        CHECK(slot != 0u && slot <= YT2_ROSTER_SLOTS);
        victim = &fix.room.party->roster[slot - 1u];
        victim->current.combat.hit_points = 1u;
        spider_hp = fix.room.near_objects[1].hp;
        memset(&cap, 0, sizeof(cap));
        cap.fix = &fix;
        cap.victim = 3u;
        for (i = 0u; i < 6u; i++) {
            CHECK(yt2_combat_key(&fix.combat, 'A',
                                 test_present_highlight,
                                 &cap) == YT2_STATUS_OK);
            if (cap.count >= HIGHLIGHT_CAP) {
                break;
            }
        }
        CHECK((victim->status & 0x1C40u) != 0u);
        CHECK(fix.room.near_objects[1].hp < spider_hp);
        CHECK(cap.handed_while_dead == 0);
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
    /* Explore campfire click (246,75): the S round fires (29 first). */
    {
        test_formal fix;

        if (!test_formal_init(&fix)) {
            return 0;
        }
        fix.room.f5064 &= (uint16_t)~0x1000u;
        CHECK(yt2_combat_click(&fix.combat, 246, 75, 0, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK(yt2_combat_sfx_count(&fix.combat) >= 1u);
        CHECK(yt2_combat_sfx_at(&fix.combat, 0u) == 29u);
        test_formal_free(&fix);
    }
    /* Explore left-arrow click (253,169): turns (facing moves). */
    {
        test_formal fix;
        uint16_t f0;

        if (!test_formal_init(&fix)) {
            return 0;
        }
        fix.room.f5064 &= (uint16_t)~0x1000u;
        f0 = fix.save.party.facing;
        CHECK(yt2_combat_click(&fix.combat, 253, 169, 0, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK(fix.save.party.facing != f0);
        test_formal_free(&fix);
    }
    /* Explore disk click: menu requested (formal 0). Spell cast is
       deferred (silent). */
    {
        test_formal fix;

        if (!test_formal_init(&fix)) {
            return 0;
        }
        fix.room.f5064 &= (uint16_t)~0x1000u;
        CHECK(yt2_combat_click(&fix.combat, 303, 75, 0, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK(fix.combat.menu_requested == 1);
        CHECK(fix.combat.menu_formal == 0);
        fix.combat.menu_requested = 0;
        CHECK(yt2_combat_click(&fix.combat, 265, 75, 0, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK(fix.combat.menu_requested == 0);
        test_formal_free(&fix);
    }
    /* Formal disk click: menu requested (formal 1). Swords act. */
    {
        test_formal fix;

        if (!test_formal_init(&fix)) {
            return 0;
        }
        CHECK(yt2_combat_click(&fix.combat, 303, 75, 0, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK(fix.combat.menu_requested == 1);
        CHECK(fix.combat.menu_formal == 1);
        fix.combat.menu_requested = 0;
        fix.combat.menu_formal = 0;
        CHECK(yt2_combat_click(&fix.combat, 246, 75, 0, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK(yt2_combat_sfx_count(&fix.combat) >= 1u);
        test_formal_free(&fix);
    }
    /* Nested menu open consumes the side panels (fn_06EA4): a pending
       award or message never paints under the menu. */
    {
        test_formal fix;

        if (!test_formal_init(&fix)) {
            return 0;
        }
        yt2_combat_menu_open(NULL);
        fix.combat.award_pending = 1;
        fix.combat.chest_side_panel_lines = 3u;
        yt2_combat_menu_open(&fix.combat);
        CHECK(fix.combat.award_pending == 0);
        CHECK(fix.combat.chest_side_panel_lines == 0u);
        test_formal_free(&fix);
    }
    /* Formal consumes a stuck side panel (B1 fn_135B2/fn_06EA4): a
       search side panel left over from explore is gone after the first
       formal key, and the engaged name shows on every present of
       the turn (next and melee repaints) instead of being
       buried under it. */
    {
        test_formal fix;
        yt2_room_object *spider;
        test_name_capture cap;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        spider = &fix.room.near_objects[1];
        memcpy(spider->name, "SPIDER", 7u);
        fix.room.f5064 |= 0x1000u;
        memset(fix.combat.chest_side_panel, 0, sizeof(fix.combat.chest_side_panel));
        memcpy(fix.combat.chest_side_panel[0], "NOTHING HERE", 13u);
        fix.combat.chest_side_panel_color[0] = 0x0Fu;
        fix.combat.chest_side_panel_row[0] = 96;
        fix.combat.chest_side_panel_lines = 1u;
        fix.combat.chest_side_panel_compass = 1u;
        memset(&cap, 0, sizeof(cap));
        cap.fix = &fix;
        yt2_combat_key(&fix.combat, 'A', test_present_name, &cap);
        CHECK(fix.combat.chest_side_panel_lines == 0u);
        CHECK(cap.count > 0u);
        for (unsigned int i = 0u; i < cap.count; i++) {
            CHECK(cap.ink[i] > 20u);
        }
        test_formal_free(&fix);
    }
    /* Fresh formal contact settles at once (B1 fn_06382 entry):
       the queue builds and the first member waits without another
       key. Member-first: no presents yet (the caller repaints the
       border); the spider leading acts first instead. */
    {
        test_formal fix;
        highlight_capture cap;
        unsigned int frames = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.f5064 |= 0x1000u;
        CHECK(fix.combat.queue_count == 0u);
        yt2_combat_formal_entry(&fix.combat, test_present_count,
                                &frames);
        CHECK(fix.combat.queue_count > 0u);
        CHECK(frames == 0u);
        CHECK(fix.combat.current == 3u);
        memset(&cap, 0, sizeof(cap));
        cap.fix = &fix;
        cap.victim = 99u;
        CHECK(yt2_combat_repaint(&fix.combat, test_present_highlight,
                                 &cap) == YT2_STATUS_OK);
        CHECK(cap.count == 1u);
        CHECK(cap.allborders[0][3] == 128u);
        test_formal_free(&fix);
    }
    /* Spider-first entry: the monster prefix acts (presents) before
       the next-hero state. Explore and modal states stay out. */
    {
        test_formal fix;
        unsigned int frames = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.f5064 |= 0x1000u;
        fix.room.near_objects[1].initiative = 99u;
        yt2_combat_formal_entry(&fix.combat, test_present_count,
                                &frames);
        CHECK(fix.combat.queue_count > 0u);
        CHECK(frames > 0u);
        CHECK(fix.combat.current == 3u);
        test_formal_free(&fix);
    }
    /* Entry guards: explore and modal states never build. */
    {
        test_formal fix;
        unsigned int frames = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.room.f5064 &= (uint16_t)~0x1000u;
        yt2_combat_formal_entry(&fix.combat, test_present_count,
                                &frames);
        CHECK(fix.combat.queue_count == 0u);
        CHECK(frames == 0u);
        fix.room.f5064 |= 0x1000u;
        fix.combat.chest_prompt = 1;
        yt2_combat_formal_entry(&fix.combat, test_present_count,
                                &frames);
        CHECK(fix.combat.queue_count == 0u);
        CHECK(frames == 0u);
        test_formal_free(&fix);
    }
    /* Take receipts paint per name field (B1 fn_11F83), never joined:
       the two-field +10 splits across rows 96/102, the one-field +1
       takes row 96 with blank rows after. */
    {
        test_formal fix;
        unsigned int frames = 0u;

        if (!test_formal_init(&fix)) {
            return test_failures == 0;
        }
        fix.combat.chest_open = 1;
        fix.combat.chest_staging[0].id = 361u;
        fix.combat.chest_staging[0].count = 0u;
        CHECK(yt2_combat_click(&fix.combat, 249, 168, 0,
                               test_present_count,
                               &frames) == YT2_STATUS_OK);
        CHECK(fix.combat.chest_side_panel_lines == 3u);
        CHECK(strcmp(fix.combat.chest_side_panel[0], "FIELD PLATE") == 0);
        CHECK(fix.combat.chest_side_panel_row[0] == 96);
        CHECK(strcmp(fix.combat.chest_side_panel[1], "LEGGINGS +10") == 0);
        CHECK(fix.combat.chest_side_panel_row[1] == 102);
        CHECK(fix.combat.chest_side_panel[2][0] == '\0');
        CHECK(fix.combat.chest_side_panel_row[2] == 108);
        fix.combat.chest_cursor = 0u;
        fix.combat.chest_carried = 0u;
        fix.combat.chest_staging[0].id = 413u;
        fix.combat.chest_staging[0].count = 0u;
        CHECK(yt2_combat_click(&fix.combat, 249, 168, 0,
                               test_present_count,
                               &frames) == YT2_STATUS_OK);
        CHECK(strcmp(fix.combat.chest_side_panel[0], "DAGGER +1") == 0);
        CHECK(fix.combat.chest_side_panel_row[0] == 96);
        CHECK(fix.combat.chest_side_panel[1][0] == '\0');
        test_formal_free(&fix);
    }
    /* F5 toggle (B1 path B): QUICK ACCESS<->MAP with the skill,
       QUICK ACCESS<->CHAPTER else; silent ERROR when formal. */
    {
        test_formal fix;

        if (!test_formal_init(&fix)) {
            return 0;
        }
        fix.room.f5064 &= (uint16_t)~0x1000u;
        fix.room.party->explore_flags = 0xA400u;
        CHECK(yt2_combat_panel_toggle(&fix.combat, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK((fix.room.party->explore_flags & 0x7000u) == 0x4000u);
        CHECK(yt2_combat_sfx_count(&fix.combat) == 1u);
        CHECK(yt2_combat_sfx_at(&fix.combat, 0u) == 2u);
        CHECK(yt2_combat_panel_toggle(&fix.combat, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK((fix.room.party->explore_flags & 0x7000u) == 0x1000u);
        fix.room.party->explore_flags = 0x0400u;
        CHECK(yt2_combat_panel_toggle(&fix.combat, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK((fix.room.party->explore_flags & 0x7000u) == 0x2000u);
        CHECK(yt2_combat_panel_toggle(&fix.combat, NULL, NULL) ==
              YT2_STATUS_OK);
        CHECK((fix.room.party->explore_flags & 0x7000u) == 0x1000u);
        fix.room.f5064 |= 0x1000u;
        CHECK(yt2_combat_panel_toggle(&fix.combat, NULL, NULL) ==
              YT2_STATUS_ERROR);
        test_formal_free(&fix);
    }
    /* Minimap marker follows facing (sub_118C0: N->0, S->2, E->1,
       else 3); the gather path paints it through the callback. */
    {
        static const unsigned int pics[4] = {0u, 2u, 1u, 3u};
        static const uint16_t facings[4] = {
            YT2_ROOM_FACING_NORTH, YT2_ROOM_FACING_SOUTH,
            YT2_ROOM_FACING_EAST, YT2_ROOM_FACING_WEST
        };
        test_formal fix;

        if (!test_formal_init(&fix)) {
            return 0;
        }
        fix.room.f5064 &= (uint16_t)~0x1000u;
        fix.room.party->explore_flags = 0xC400u;
        fix.room.panel_paint = yt2_combat_panel_paint;
        fix.room.panel_combat = &fix.combat;
        for (unsigned int f = 0u; f < 4u; f++) {
            const uint8_t *pic;
            unsigned int same = 1u;

            fix.room.party->facing = facings[f];
            yt2_combat_panel_paint(&fix.combat, &fix.surface);
            pic = yt2_picture_data(&fix.pictures, 9u, pics[f]);
            CHECK(pic != NULL);
            for (unsigned int row = 0u; row < 8u && pic != NULL; row++) {
                for (unsigned int col = 0u; col < 8u; col++) {
                    uint8_t want = pic[row * 8u + col];

                    if (want == 0xFFu) {
                        continue;
                    }
                    if (fix.surface.pixels[(size_t)(32 + row) *
                                               fix.surface.stride +
                                           (size_t)(272 + col)] !=
                        want) {
                        same = 0u;
                    }
                }
            }
            CHECK(same);
        }
        test_formal_free(&fix);
    }
    return test_failures == 0;
}