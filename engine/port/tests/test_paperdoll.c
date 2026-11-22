#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "yt2/create.h"
#include "yt2/paperdoll.h"
#include "yt2/item_detail.h"
#include "yt2/items.h"
#include "yt2/pictures.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

/*
 * The shared paperdoll verbs (paperdoll.c): the 0x60F4 table, creation's
 * container/effect cells through the same core the pack panes run,
 * and the allocator both sides mint from.
 */

static void check_table(void)
{
    /* SWREG.EXE file offset 0x27D94: 21 (x0,x1,y0,y1,id) words,
       then the 0xFFFF end marker. */
    static const int want[21][5] = {
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
    unsigned int i;

    for (i = 0u; i < 21u; i++) {
        CHECK(yt2_paperdoll_cells[i].x0 == want[i][0]);
        CHECK(yt2_paperdoll_cells[i].x1 == want[i][1]);
        CHECK(yt2_paperdoll_cells[i].y0 == want[i][2]);
        CHECK(yt2_paperdoll_cells[i].y1 == want[i][3]);
        CHECK(yt2_paperdoll_cells[i].id == (unsigned int)want[i][4]);
    }
}

static void check_hit(void)
{
    /* Creation paperdoll origin (116, 60); the cells the old creation
       table was missing (15-21) resolve now. */
    CHECK(yt2_paperdoll_hit(116, 60, 125, 69) == 1u);
    CHECK(yt2_paperdoll_hit(116, 60, 163, 158) == 15u);
    CHECK(yt2_paperdoll_hit(116, 60, 140, 120) == 16u);
    CHECK(yt2_paperdoll_hit(116, 60, 140, 140) == 17u);
    CHECK(yt2_paperdoll_hit(116, 60, 140, 160) == 18u);
    CHECK(yt2_paperdoll_hit(116, 60, 130, 190) == 19u);
    CHECK(yt2_paperdoll_hit(116, 60, 129, 155) == 20u);
    CHECK(yt2_paperdoll_hit(116, 60, 157, 155) == 21u);
    CHECK(yt2_paperdoll_hit(116, 60, 161, 103) == 9u);
    CHECK(yt2_paperdoll_hit(116, 60, 0, 0) == 0u);
    /* Pack pane origin (8, 8): same table, other caller. */
    CHECK(yt2_paperdoll_hit(8, 8, 17, 17) == 1u);
    CHECK(yt2_paperdoll_hit(8, 8, 59, 103) == 15u);
}

/* Drive a create session to the items stage (fighter, portrait 1). */
static int begin_fighter(yt2_create *create, yt2_save *save,
                         const yt2_items *items,
                         const yt2_pictures *pictures,
                         yt2_surface *surface)
{
    if (yt2_create_begin(create, save, items, pictures, surface,
                         0x1234u) != YT2_STATUS_OK ||
        yt2_create_open_class(create) != YT2_STATUS_OK ||
        yt2_create_key(create, 'F') != YT2_STATUS_OK ||
        yt2_create_key(create, '1') != YT2_STATUS_OK ||
        yt2_create_key(create, 'I') != YT2_STATUS_OK) {
        return 0;
    }
    return create->stage == YT2_CREATE_STAGE_ITEMS;
}

static void check_creation_container(const yt2_items *items,
                                     const yt2_pictures *pictures,
                                     const yt2_world *world)
{
    yt2_create create;
    yt2_save save;
    yt2_surface surface;
    yt2_character *character;
    uint8_t *store;

    memset(&save, 0, sizeof(save));
    CHECK(yt2_save_start(world, &save) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(begin_fighter(&create, &save, items, pictures, &surface));
    character = &save.party.roster[create.slot];

    /* Take the container (row 7, item 6) into pack cell 1: fresh
       bags mint a store (bump 2, like a new party). */
    CHECK(yt2_create_click(&create, 50, 145) == YT2_STATUS_OK);
    CHECK(create.carried_id == 6u);
    CHECK(yt2_create_click(&create, 125, 69) == YT2_STATUS_OK);
    CHECK(character->pack[0].id == 6u);
    CHECK(character->pack[0].count == 2u);
    CHECK(save.store_bump == 3u);
    CHECK(create.carried_id == 0u);

    /* Drop the dagger onto the bag: inserts into its store. */
    CHECK(yt2_create_click(&create, 50, 62) == YT2_STATUS_OK);
    CHECK(create.carried_id == 412u);
    CHECK(yt2_create_click(&create, 125, 69) == YT2_STATUS_OK);
    CHECK(create.carried_id == 0u);
    CHECK(create.sfx[create.sfx_count - 1u] == 6u);
    store = save.bytes + YT2_SAVE_STORE_OFFSET + 2u * YT2_SAVE_STORE_SIZE;
    CHECK(store[2] == 0x9Cu);
    CHECK(store[3] == 0x01u);
    CHECK(character->pack[1].id == 0u);

    /* Right-click opens the bag: cells redirect to the staging. */
    CHECK(yt2_create_right_click(&create, 125, 69) == YT2_STATUS_OK);
    CHECK(character->stage[2].id == 6u);
    CHECK(character->stage[2].cells[0].id == 412u);
    CHECK((character->flags & 0x2000u) != 0u);

    /* Right-click cell 9 closes: the store keeps the dagger. */
    CHECK(yt2_create_right_click(&create, 161, 103) == YT2_STATUS_OK);
    CHECK(character->stage[2].id == 0u);
    CHECK((character->flags & 0xE000u) == 0u);
    CHECK(store[2] == 0x9Cu);
    CHECK(store[3] == 0x01u);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
}

static void check_creation_effects(const yt2_items *items,
                                   const yt2_pictures *pictures,
                                   const yt2_world *world)
{
    yt2_create create;
    yt2_save save;
    yt2_surface surface;
    yt2_character *character;

    memset(&save, 0, sizeof(save));
    CHECK(yt2_save_start(world, &save) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(begin_fighter(&create, &save, items, pictures, &surface));
    character = &save.party.roster[create.slot];

    /* Item 362 (row 5, flags 0x200, row word 0x4100) fits cell 17
       only; item 312 (row 6, word 0x2100) fits cell 18 only. */
    CHECK(yt2_create_click(&create, 50, 113) == YT2_STATUS_OK);
    CHECK(create.carried_id == 362u);
    CHECK(yt2_create_click(&create, 140, 140) == YT2_STATUS_OK);
    CHECK(character->effects[1] == 362u);
    CHECK(create.carried_id == 0u);
    CHECK(yt2_create_click(&create, 50, 129) == YT2_STATUS_OK);
    CHECK(create.carried_id == 312u);
    CHECK(yt2_create_click(&create, 140, 160) == YT2_STATUS_OK);
    CHECK(character->effects[2] == 312u);

    /* The dagger fits no effect cell: buzz, cursor keeps it. */
    CHECK(yt2_create_click(&create, 50, 62) == YT2_STATUS_OK);
    CHECK(create.carried_id == 412u);
    CHECK(yt2_create_click(&create, 140, 120) == YT2_STATUS_OK);
    CHECK(character->effects[0] == 0u);
    CHECK(create.carried_id == 412u);
    CHECK(create.sfx[create.sfx_count - 1u] == 3u);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
}

static void check_no_bonus_lists(const yt2_items *items)
{
    unsigned int i;

    /* The paperdoll bonus hook no-ops in creation: every fixed take
       row must carry no bonus list. */
    for (i = 0u; i < YT2_CREATE_ITEMS; i++) {
        const uint8_t *def =
            yt2_item_def(items, yt2_create_item_ids[i]);

        CHECK(def != NULL);
        CHECK(yt2_item_adds(items, def) == NULL);
    }
}

static void doll_bonus_count(void *bonus_ctx, unsigned int slot,
                             unsigned int id, int plus)
{
    unsigned int *log = (unsigned int *)bonus_ctx;

    (void)slot;
    (void)id;
    log[0]++;
    log[1] = (unsigned int)plus;
}

static void check_core_recompute(const yt2_items *items)
{
    yt2_paperdoll_ctx ctx;
    yt2_character character;
    uint8_t stores[4u * YT2_SAVE_STORE_SIZE];
    uint16_t bump = 2u;
    uint16_t free_head = 0u;
    unsigned int sfx_log[8];
    unsigned int sfx_count = 0u;
    unsigned int bonus_log[2] = { 0u, 0u };
    int outcome;

    memset(&character, 0, sizeof(character));
    memset(stores, 0, sizeof(stores));
    character.current.skills.slashing = 54u;
    character.maximum.skills.slashing = 54u;
    character.current.combat.carry_capacity = 9999u;
    yt2_paperdoll_init(&ctx, items, stores, sizeof(stores), &bump,
                  &free_head, sfx_log, &sfx_count, 8u,
                  doll_bonus_count, bonus_log);

    /* Drop the dagger (row word0 3) on cell 12: bonuses on plus
       the fn_0B3D2 recompute B1's sub_16BE4 runs. */
    ctx.cursor = 140u;
    ctx.carried = 412u;
    ctx.carried_count = 0u;
    ctx.carried_weight = 11u;
    outcome = yt2_paperdoll_left(&ctx, &character, 1u, 12u);
    CHECK(outcome == YT2_PAPERDOLL_CHANGED);
    CHECK(character.equipment[2].id == 412u);
    CHECK(character.current.combat.battle_accuracy == 54u);
    CHECK(character.current.combat.battle_damage == 3u);
    CHECK(bonus_log[0] == 1u && bonus_log[1] == 1u);

    /* Take it back off: bonuses off, combat reset. */
    outcome = yt2_paperdoll_left(&ctx, &character, 1u, 12u);
    CHECK(outcome == YT2_PAPERDOLL_CHANGED);
    CHECK(character.equipment[2].id == 0u);
    CHECK(character.current.combat.battle_accuracy == 0u);
    CHECK(character.current.combat.battle_damage == 0u);
    CHECK(bonus_log[0] == 2u && bonus_log[1] == 0u);
}

static void check_allocator(const yt2_world *world)
{
    const char *load_path = getenv("YT2_SAVE");
    yt2_save save;

    memset(&save, 0, sizeof(save));
    CHECK(yt2_save_start(world, &save) == YT2_STATUS_OK);
    CHECK(save.store_bump == 2u);
    CHECK(save.store_free == 0u);
    yt2_save_free(&save);

    /* Loads boot the allocator 0/0 like B1 (post-load mints
       collide like vanilla). */
    if (load_path == NULL || load_path[0] == '\0') {
        fprintf(stderr, "test_paperdoll: YT2_SAVE missing, skipped\n");
        return;
    }
    memset(&save, 0, sizeof(save));
    CHECK(yt2_save_load(load_path, &save) == YT2_STATUS_OK);
    CHECK(save.store_bump == 0u);
    CHECK(save.store_free == 0u);
    yt2_save_free(&save);
}

int test_paperdoll(void)
{
    const char *world_path = getenv("YT2_WORLD");
    const char *pictures_path = getenv("YT2_PICTURES");
    yt2_world world;
    yt2_items items;
    yt2_pictures pictures;

    check_table();
    check_hit();
    if (world_path == NULL || pictures_path == NULL) {
        fprintf(stderr, "test_paperdoll: fixture files missing, skipped\n");
        return test_failures == 0;
    }
    memset(&world, 0, sizeof(world));
    memset(&items, 0, sizeof(items));
    memset(&pictures, 0, sizeof(pictures));
    CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
    CHECK(yt2_items_load(&items, &world) == YT2_STATUS_OK);
    CHECK(yt2_pictures_load(pictures_path, &pictures) == YT2_STATUS_OK);
    check_creation_container(&items, &pictures, &world);
    check_creation_effects(&items, &pictures, &world);
    check_no_bonus_lists(&items);
    check_core_recompute(&items);
    check_allocator(&world);
    yt2_pictures_free(&pictures);
    yt2_world_free(&world);
    return test_failures == 0;
}
