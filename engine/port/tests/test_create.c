#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "yt2/create.h"
#include "yt2/cursor.h"
#include "yt2/items.h"
#include "yt2/pictures.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

static void check_rng(void)
{
    uint16_t seed = 0u;

    /* Hand-traced fn_1797C(15) from DOS time 0x1234: STR 45 + 5. */
    CHECK(yt2_create_rand(&seed, 0x1234u, 15u) == 5u);
    CHECK(seed == 0xFC04u);
    CHECK(yt2_create_rand(&seed, 0x1234u, 0u) == 0u);
    {
        uint16_t again = 0xFC04u;

        seed = 0xFC04u;
        CHECK(yt2_create_rand(&seed, 0x1234u, 15u) ==
              yt2_create_rand(&again, 0x1234u, 15u));
        CHECK(seed == again);
    }
}

static void check_percent(void)
{
    CHECK(yt2_create_percent(50u, 25u) == 13u);
    CHECK(yt2_create_percent(49u, 80u) == 39u);
    CHECK(yt2_create_percent(52u, 20u) == 10u);
    CHECK(yt2_create_percent(49u, 50u) == 25u);
    CHECK(yt2_create_percent(45u, 75u) == 34u);
}

static void check_roll(void)
{
    yt2_create create;
    yt2_character character;

    memset(&create, 0, sizeof(create));
    memset(&character, 0, sizeof(character));
    character.class_id = YT2_CLASS_FIGHTER;
    create.time_word = 0x1234u;
    yt2_create_roll(&create, &character);

    /* M5's CONAN roll, pinned by the sheet checkpoint. */
    CHECK(character.current.attributes.strength == 50u);
    CHECK(character.current.attributes.dexterity == 49u);
    CHECK(character.current.attributes.stamina == 50u);
    CHECK(character.current.attributes.intelligence == 52u);
    CHECK(character.current.attributes.wisdom == 60u);
    CHECK(character.current.attributes.charisma == 49u);
    CHECK(character.current.combat.hit_points == 13u);
    CHECK(character.current.combat.magic_points == 0u);
    CHECK(character.current.combat.carry_capacity == 500u);
    CHECK(character.current.skills.survival == 55u);
    CHECK(character.current.skills.projectile == 55u);
    CHECK(character.current.skills.slashing == 54u);
    CHECK(character.current.skills.bashing == 55u);
    CHECK(character.current.skills.polearm == 55u);
    CHECK(character.current.skills.casting == 0u);
    CHECK(character.current.skills.mapping == 40u);
    CHECK(character.current.skills.navigation == 49u);
    CHECK(character.current.skills.bartering == 40u);
    CHECK(character.current.skills.repair == 52u);
    CHECK(character.current.skills.thievery == 40u);
    CHECK(character.current.skills.linguistics == 0u);
    CHECK(character.current.skills.chemistry == 0u);
    CHECK(character.maximum.skills.thievery == 40u);
    character.level = 1u;
    CHECK(strcmp(yt2_create_rating(&character), "AVERAGE") == 0);
}

static void check_rating(void)
{
    yt2_character character;

    memset(&character, 0, sizeof(character));
    character.level = 1u;
    character.current.attributes.strength = 49u;
    character.current.attributes.dexterity = 49u;
    character.current.attributes.stamina = 49u;
    character.current.attributes.intelligence = 49u;
    character.current.attributes.wisdom = 49u;
    character.current.attributes.charisma = 49u;
    CHECK(strcmp(yt2_create_rating(&character), "POOR   ") == 0);
    character.current.attributes.charisma = 50u;
    CHECK(strcmp(yt2_create_rating(&character), "POOR   ") == 0);
    character.current.attributes.charisma = 55u;
    CHECK(strcmp(yt2_create_rating(&character), "AVERAGE") == 0);
    character.current.attributes.wisdom = 60u;
    character.current.attributes.charisma = 60u;
    character.current.attributes.intelligence = 52u;
    CHECK(strcmp(yt2_create_rating(&character), "GOOD   ") == 0);
    character.current.attributes.strength = 60u;
    character.current.attributes.dexterity = 60u;
    character.current.attributes.stamina = 60u;
    character.current.attributes.intelligence = 60u;
    CHECK(strcmp(yt2_create_rating(&character), "GREAT  ") == 0);
    character.level = 2u;
    CHECK(yt2_create_rating(&character) == NULL);
}

static void check_spells(void)
{
    yt2_character character;

    memset(&character, 0, sizeof(character));
    character.class_id = YT2_CLASS_MAGE;
    character.maximum.combat.magic_points = 11u;
    yt2_create_grant_spells(&character);
    CHECK(character.status == 4u);
    CHECK(character.spellbook[0] == 0x6000u);

    memset(&character, 0, sizeof(character));
    character.class_id = YT2_CLASS_MONK;
    character.maximum.combat.magic_points = 11u;
    yt2_create_grant_spells(&character);
    CHECK(character.status == 0x20u);
    CHECK(character.spellbook[0] == 0xA000u);

    memset(&character, 0, sizeof(character));
    character.class_id = YT2_CLASS_FIGHTER;
    character.maximum.combat.magic_points = 11u;
    yt2_create_grant_spells(&character);
    CHECK(character.status == 0u);
    CHECK(character.spellbook[0] == 0u);

    memset(&character, 0, sizeof(character));
    character.class_id = YT2_CLASS_MAGE;
    yt2_create_grant_spells(&character);
    CHECK(character.status == 0u);
}

static void check_alloc(void)
{
    yt2_party party;
    unsigned int i;

    memset(&party, 0, sizeof(party));
    CHECK(yt2_create_alloc_slot(&party) == 0);
    party.roster[0].class_id = YT2_CLASS_FIGHTER;
    CHECK(yt2_create_alloc_slot(&party) == 1);
    for (i = 0; i < YT2_ROSTER_SLOTS; i++) {
        party.roster[i].class_id = YT2_CLASS_FIGHTER;
    }
    CHECK(yt2_create_alloc_slot(&party) == -1);
    CHECK(yt2_create_alloc_slot(NULL) == -1);
}

static void test_flush_count(void *context)
{
    (*(unsigned int *)context)++;
}

static int load_fixtures(yt2_pictures *pictures, yt2_world *world,
                         yt2_items *items)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");

    if (pictures_path == NULL || world_path == NULL) {
        return 0;
    }
    if (yt2_pictures_load(pictures_path, pictures) != YT2_STATUS_OK) {
        return 0;
    }
    if (yt2_world_load(world_path, world) != YT2_STATUS_OK) {
        return 0;
    }
    if (yt2_items_load(items, world) != YT2_STATUS_OK) {
        return 0;
    }
    return 1;
}

/* The M5 route: CONAN the fighter with sling and dagger, kept. */
static void check_conan_flow(void)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_surface surface;
    yt2_create create;
    const yt2_character *conan;

    if (!load_fixtures(&pictures, &world, &items)) {
        fprintf(stderr, "test_create: fixture files missing, skipped\n");
        return;
    }
    CHECK(yt2_save_start(&world, &save) == YT2_STATUS_OK);
    CHECK(save.party.roster_count == 4u);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);

    CHECK(yt2_create_begin(&create, &save.party, &items, &pictures, &surface,
                           0x1234u) == YT2_STATUS_OK);
    CHECK(create.slot == 0u);
    CHECK(yt2_create_open_class(&create) == YT2_STATUS_OK);
    CHECK(yt2_create_key(&create, 'X') == YT2_STATUS_ERROR);
    CHECK(yt2_create_key(&create, 'F') == YT2_STATUS_OK);
    CHECK(create.stage == YT2_CREATE_STAGE_PORTRAIT);
    CHECK(save.party.roster[0].class_id == YT2_CLASS_FIGHTER);
    CHECK(save.party.roster[0].level == 1u);
    CHECK(save.party.roster[0].sex == YT2_SEX_MALE);
    CHECK(yt2_create_key(&create, '1') == YT2_STATUS_OK);
    CHECK(create.stage == YT2_CREATE_STAGE_SHEET);
    CHECK(save.party.roster[0].portrait_pic == 19u);
    CHECK(save.party.roster[0].portrait_index == 0u);
    CHECK(save.party.roster[0].current.attributes.strength == 50u);
    CHECK(yt2_create_key(&create, 'I') == YT2_STATUS_OK);
    CHECK(create.stage == YT2_CREATE_STAGE_ITEMS);

    /* Sling row 1, dagger row 2; drops on cells 10 and 12. */
    CHECK(yt2_create_click(&create, 50, 50) == YT2_STATUS_OK);
    CHECK(create.carried_id == 542u);
    CHECK(yt2_create_click(&create, 123, 131) == YT2_STATUS_OK);
    CHECK(save.party.roster[0].equipment[0].id == 542u);
    CHECK(yt2_create_click(&create, 50, 62) == YT2_STATUS_OK);
    CHECK(yt2_create_click(&create, 123, 170) == YT2_STATUS_OK);
    CHECK(save.party.roster[0].equipment[2].id == 412u);
    CHECK(save.party.roster[0].current.combat.battle_accuracy == 54u);
    CHECK(save.party.roster[0].current.combat.battle_damage == 3u);

    CHECK(yt2_create_key(&create, 'N') == YT2_STATUS_OK);
    CHECK(create.stage == YT2_CREATE_STAGE_NAME);
    for (const char *name = "CONAN"; *name != '\0'; name++) {
        CHECK(yt2_create_key(&create, (unsigned int)*name) ==
              YT2_STATUS_OK);
    }
    CHECK(yt2_create_key(&create, 0x0Du) == YT2_STATUS_OK);
    CHECK(create.stage == YT2_CREATE_STAGE_PREVIEW);
    CHECK(strcmp(save.party.roster[0].name, "CONAN") == 0);
    CHECK(yt2_create_key(&create, 'K') == YT2_STATUS_OK);
    CHECK(yt2_create_done(&create));
    CHECK(create.stage == YT2_CREATE_STAGE_KEEP);

    conan = &save.party.roster[0];
    CHECK(conan->class_id == YT2_CLASS_FIGHTER);
    CHECK(conan->maximum.combat.hit_points == 13u);
    CHECK(yt2_create_sfx_count(&create) == 2u);
    CHECK(yt2_create_sfx(&create, 0u) == 6u);
    CHECK(yt2_create_sfx(&create, 1u) == 6u);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
}

static uint64_t hash_frame(const yt2_surface *surface,
                           const yt2_pictures *pictures, int cursor_x,
                           int cursor_y)
{
    static uint8_t copy[64000];
    yt2_surface view = *surface;
    uint64_t hash = 0xCBF29CE484222325ull;

    memcpy(copy, surface->pixels, sizeof(copy));
    view.pixels = copy;
    if (cursor_x >= 0) {
        yt2_cursor_draw(&view, pictures, 0, cursor_x, cursor_y);
    }
    for (size_t i = 0; i < sizeof(copy); i++) {
        hash ^= copy[i];
        hash *= 0x100000001B3ull;
    }
    return hash;
}

/*
 * The M5 creation flow, framebuffer against framebuffer: every hash is a
 * tests/c/m5_checkpoints.txt pin (setup2 is B1's frame between preview
 * and keep, pinned here).  Cursor positions are B1's own: (230,180)
 * until the first mouse move, (115,162) after the dagger drop; the
 * name editor and the setup frame composite no cursor.
 */
static void check_conan_hashes(void)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_surface surface;
    yt2_create create;
    yt2_create second;
    yt2_roster roster;

    if (!load_fixtures(&pictures, &world, &items)) {
        return;
    }
    CHECK(yt2_save_start(&world, &save) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    CHECK(yt2_create_begin(&create, &save.party, &items, &pictures, &surface,
                           0x1234u) == YT2_STATUS_OK);
    CHECK(yt2_create_open_class(&create) == YT2_STATUS_OK);
    CHECK(hash_frame(&surface, &pictures, 230, 180) ==
          0x68B68BD0939CE8C2ull);
    yt2_create_key(&create, 'F');
    CHECK(hash_frame(&surface, &pictures, 230, 180) ==
          0x7DF6E7761A531E5Aull);
    yt2_create_key(&create, '1');
    CHECK(hash_frame(&surface, &pictures, 230, 180) ==
          0x33163413D78BF902ull);
    yt2_create_key(&create, 'I');
    CHECK(hash_frame(&surface, &pictures, 230, 180) ==
          0x548A553C24CFEC18ull);
    yt2_create_click(&create, 50, 50);
    yt2_create_click(&create, 123, 131);
    yt2_create_click(&create, 50, 62);
    yt2_create_click(&create, 123, 170);
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0x3D6A9D53CD5E369Eull);
    yt2_create_key(&create, 'N');
    CHECK(hash_frame(&surface, &pictures, -1, -1) ==
          0x4211A4E43662E9BAull);
    yt2_create_key(&create, 'C');
    CHECK(hash_frame(&surface, &pictures, -1, -1) ==
          0x5CD354C7FFF0F656ull);
    yt2_create_key(&create, 'O');
    CHECK(hash_frame(&surface, &pictures, -1, -1) ==
          0x60B9078234ABD696ull);
    yt2_create_key(&create, 'N');
    CHECK(hash_frame(&surface, &pictures, -1, -1) ==
          0x93FD7535F94FB77Aull);
    yt2_create_key(&create, 'A');
    CHECK(hash_frame(&surface, &pictures, -1, -1) ==
          0xE0280273872B3DE2ull);
    yt2_create_key(&create, 'N');
    CHECK(hash_frame(&surface, &pictures, -1, -1) ==
          0x381F10A8F8F6F816ull);
    yt2_create_key(&create, 0x0D);
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0x5B9AAFD86CEB1917ull);
    yt2_create_key(&create, 'K');
    CHECK(yt2_create_begin(&second, &save.party, &items, &pictures, &surface,
                           0x1234u) == YT2_STATUS_OK);
    CHECK(hash_frame(&surface, &pictures, -1, -1) ==
          0x87D84580471BA4B4ull);
    CHECK(yt2_create_open_class(&second) == YT2_STATUS_OK);
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0xAD265445F7701A83ull);
    yt2_create_key(&second, 'Q');
    CHECK(yt2_roster_begin(&roster, &save.party, &items, &pictures,
                           &surface) == YT2_STATUS_OK);
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0x93C47E1DE31E28FBull);
    yt2_roster_key(&roster, '1');
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0x720E882684FA343Aull);
    yt2_roster_key(&roster, '6');
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0x119C488A71398663ull);
    yt2_roster_key(&roster, '7');
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0x4325566BA01D17AAull);
    yt2_roster_key(&roster, '8');
    CHECK(hash_frame(&surface, &pictures, 115, 162) ==
          0x617F49FFF490240Full);
    yt2_roster_key(&roster, 'D');
    CHECK(yt2_roster_done(&roster));
    CHECK(roster.party_ok);
    CHECK(save.party.assigned[0] == 1u && save.party.assigned[1] == 6u &&
          save.party.assigned[2] == 7u && save.party.assigned[3] == 8u);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
}

typedef struct create_fixture {
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_surface surface;
} create_fixture;

static int fixture_begin(create_fixture *fix, yt2_create *create)
{
    if (!load_fixtures(&fix->pictures, &fix->world, &fix->items)) {
        return 0;
    }
    if (yt2_save_start(&fix->world, &fix->save) != YT2_STATUS_OK ||
        yt2_surface_init(&fix->surface, 320u, 200u) != YT2_STATUS_OK) {
        return 0;
    }
    if (yt2_create_begin(create, &fix->save.party, &fix->items,
                         &fix->pictures, &fix->surface,
                         0x1234u) != YT2_STATUS_OK) {
        return 0;
    }
    return yt2_create_open_class(create) == YT2_STATUS_OK;
}

static void fixture_free(create_fixture *fix)
{
    yt2_surface_free(&fix->surface);
    yt2_save_free(&fix->save);
}

/* Sex toggle, female portraits, reroll, re-picks. */
static void check_branches(void)
{
    create_fixture fix;
    yt2_create create;
    uint16_t first_strength;

    if (!fixture_begin(&fix, &create)) {
        return;
    }
    /* M before anything: inert on the class screen. */
    CHECK(yt2_create_key(&create, 'K') == YT2_STATUS_OK);
    CHECK(fix.save.party.roster[0].class_id == YT2_CLASS_MARKSMAN);
    /* M toggles to male (already male: inert), F toggles to female. */
    CHECK(yt2_create_key(&create, 'M') == YT2_STATUS_ERROR);
    CHECK(yt2_create_key(&create, 'F') == YT2_STATUS_OK);
    CHECK(fix.save.party.roster[0].sex == YT2_SEX_FEMALE);
    CHECK(yt2_create_key(&create, 'F') == YT2_STATUS_ERROR);
    CHECK(yt2_create_click(&create, 60, 152) == YT2_STATUS_OK);
    CHECK(fix.save.party.roster[0].sex == YT2_SEX_MALE);
    CHECK(yt2_create_key(&create, '9') == YT2_STATUS_OK);
    CHECK(fix.save.party.roster[0].portrait_index == 16u);
    CHECK(fix.save.party.roster[0].portrait_pic == 35u);
    first_strength =
        fix.save.party.roster[0].current.attributes.strength;
    CHECK(yt2_create_key(&create, 'R') == YT2_STATUS_OK);
    CHECK(fix.save.party.roster[0].current.attributes.strength !=
          first_strength);
    CHECK(yt2_create_key(&create, 'I') == YT2_STATUS_OK);
    /* Four takes, then the fifth buzzes. */
    yt2_create_click(&create, 50, 50);
    yt2_create_click(&create, 123, 131);
    yt2_create_click(&create, 50, 62);
    yt2_create_click(&create, 123, 170);
    yt2_create_click(&create, 50, 82);
    yt2_create_click(&create, 120, 64);
    yt2_create_click(&create, 50, 98);
    yt2_create_click(&create, 140, 64);
    CHECK(create.taken_count == 4u);
    CHECK(fix.save.party.roster[0].pack[0].id == 418u);
    CHECK(fix.save.party.roster[0].pack[1].id == 424u);
    CHECK(yt2_create_click(&create, 50, 114) == YT2_STATUS_OK);
    CHECK(create.taken_count == 4u);
    CHECK(create.carried_id == 0u);
    CHECK(yt2_create_sfx(&create, yt2_create_sfx_count(&create) - 1u) == 3u);
    /* Wrong slot buzzes: dagger onto the sling cell. */
    yt2_create_click(&create, 123, 170);
    CHECK(create.carried_id == 412u);
    CHECK(yt2_create_click(&create, 123, 131) == YT2_STATUS_OK);
    CHECK(create.carried_id == 412u);
    CHECK(fix.save.party.roster[0].equipment[0].id == 542u);
    /* Put the dagger back on its cell. */
    yt2_create_click(&create, 123, 170);
    CHECK(create.carried_id == 0u);
    /* Carry it again, click its own row: putback, and the same-click
     * pickup is skipped (last-returned guard), leaving the verbatim
     * ghost id behind an arrow cursor. */
    yt2_create_click(&create, 123, 170);
    yt2_create_click(&create, 50, 62);
    CHECK(create.taken_count == 3u);
    CHECK(create.carried_id == 412u);
    CHECK(create.cursor == YT2_CREATE_CURSOR_ARROW);
    /* Name editor edges. */
    yt2_create_key(&create, 'N');
    CHECK(yt2_create_key(&create, 0x08u) == YT2_STATUS_OK);
    CHECK(yt2_create_key(&create, 0x0Du) == YT2_STATUS_OK);
    CHECK(create.stage == YT2_CREATE_STAGE_NAME);
    for (unsigned int i = 0; i < 14u; i++) {
        yt2_create_key(&create, 'X');
    }
    CHECK(create.name_length == 12u);
    yt2_create_key(&create, 0x1Bu);
    CHECK(create.name_length == 0u);
    yt2_create_key(&create, 'B');
    yt2_create_key(&create, 'O');
    yt2_create_key(&create, 0x08u);
    yt2_create_key(&create, 'B');
    yt2_create_key(&create, 0x0Du);
    CHECK(strcmp(fix.save.party.roster[0].name, "BB") == 0);
    /* Preview re-picks: roll, rename, class change recomputes. */
    yt2_create_key(&create, 'R');
    CHECK(create.stage == YT2_CREATE_STAGE_PREVIEW);
    yt2_create_key(&create, 'N');
    CHECK(create.stage == YT2_CREATE_STAGE_NAME);
    yt2_create_key(&create, 0x1Bu);
    CHECK(strcmp(fix.save.party.roster[0].name, "BB") == 0);
    yt2_create_key(&create, 'C');
    CHECK(create.stage == YT2_CREATE_STAGE_CLASS);
    yt2_create_key(&create, 'M');
    CHECK(fix.save.party.roster[0].class_id == YT2_CLASS_MERCHANT);
    CHECK(create.stage == YT2_CREATE_STAGE_PREVIEW);
    yt2_create_key(&create, 'K');
    CHECK(yt2_create_done(&create));
    fixture_free(&fix);
}

/* Quit paths: items-Q leaks the slot, the rest wipe it. */
static void check_quits(void)
{
    create_fixture fix;
    yt2_create create;

    if (!fixture_begin(&fix, &create)) {
        return;
    }
    yt2_create_key(&create, 'F');
    yt2_create_key(&create, 'Q');
    CHECK(yt2_create_done(&create));
    CHECK(fix.save.party.roster[0].class_id == YT2_CLASS_NONE);

    CHECK(yt2_create_begin(&create, &fix.save.party, &fix.items,
                           &fix.pictures, &fix.surface,
                           0x1234u) == YT2_STATUS_OK);
    yt2_create_open_class(&create);
    yt2_create_key(&create, 'F');
    yt2_create_key(&create, '1');
    yt2_create_key(&create, 'I');
    yt2_create_key(&create, 'Q');
    CHECK(yt2_create_done(&create));
    CHECK(fix.save.party.roster[0].class_id == YT2_CLASS_FIGHTER);
    CHECK(fix.save.party.roster[0].name[0] == '\0');

    CHECK(yt2_create_begin(&create, &fix.save.party, &fix.items,
                           &fix.pictures, &fix.surface,
                           0x1234u) == YT2_STATUS_OK);
    CHECK(create.slot == 1u);
    fixture_free(&fix);
}

/* Roster: full party, unassign, empty DONE, view, delete. */
static void check_roster_branches(void)
{
    create_fixture fix;
    yt2_create create;
    yt2_roster roster;

    if (!fixture_begin(&fix, &create)) {
        return;
    }
    yt2_create_key(&create, 'F');
    yt2_create_key(&create, '1');
    yt2_create_key(&create, 'I');
    yt2_create_key(&create, 'N');
    yt2_create_key(&create, 'X');
    yt2_create_key(&create, 0x0D);
    yt2_create_key(&create, 'K');
    CHECK(yt2_roster_begin(&roster, &fix.save.party, &fix.items,
                           &fix.pictures, &fix.surface) ==
          YT2_STATUS_OK);
    CHECK(yt2_roster_key(&roster, 'A') == YT2_STATUS_ERROR);
    yt2_roster_key(&roster, '1');
    yt2_roster_key(&roster, '6');
    yt2_roster_key(&roster, '7');
    yt2_roster_key(&roster, '8');
    /* Fifth toggle buzzes (party full). */
    CHECK(yt2_roster_key(&roster, '9') == YT2_STATUS_OK);
    CHECK(fix.save.party.assigned[0] == 1u &&
          fix.save.party.assigned[3] == 8u);
    CHECK(yt2_roster_sfx(&roster, yt2_roster_sfx_count(&roster) - 1u) == 3u);
    /* Unassign middle, DONE compacts. */
    yt2_roster_key(&roster, '6');
    CHECK(fix.save.party.assigned[1] == 0u);
    yt2_roster_key(&roster, 'D');
    CHECK(yt2_roster_done(&roster) && roster.party_ok);
    /* Toggles and DONE are memory-only; only a deletion dirties. */
    CHECK(!roster.dirty);
    CHECK(fix.save.party.assigned[0] == 1u &&
          fix.save.party.assigned[1] == 7u &&
          fix.save.party.assigned[2] == 8u &&
          fix.save.party.assigned[3] == 0u);

    /* The kept party survives a save round trip: flags and the
       header tail persist together, so DONE on the reloaded party
       keeps it (the out-of-box repro: roster, quit, roster, DONE). */
    {
        static uint8_t block[YT2_PARTY_BLOCK_SIZE];
        yt2_party kept;
        yt2_roster again;

        memcpy(block, fix.save.bytes, sizeof(block));
        CHECK(yt2_party_write(block, sizeof(block), &fix.save.party) ==
              YT2_STATUS_OK);
        CHECK(block[0x1ECu] == 1u && block[0x1EEu] == 7u &&
              block[0x1F0u] == 8u && block[0x1F2u] == 0u);
        CHECK((block[0x1F4u + 0x15Cu + 1u] & 0x08u) != 0u);
        CHECK(yt2_party_parse(block, sizeof(block), &kept) ==
              YT2_STATUS_OK);
        CHECK(kept.assigned[0] == 1u && kept.assigned[1] == 7u &&
              kept.assigned[2] == 8u && kept.assigned[3] == 0u);
        CHECK(yt2_roster_begin(&again, &kept, &fix.items, &fix.pictures,
                               &fix.surface) == YT2_STATUS_OK);
        CHECK(yt2_roster_key(&again, 'D') == YT2_STATUS_OK);
        CHECK(yt2_roster_done(&again) && again.party_ok);
        CHECK(kept.assigned[0] == 1u && kept.assigned[1] == 7u &&
              kept.assigned[2] == 8u && kept.assigned[3] == 0u);
    }

    /* Empty DONE on a pristine party leaves party_ok clear. */
    {
        yt2_save fresh;
        yt2_roster empty;

        CHECK(yt2_save_start(&fix.world, &fresh) == YT2_STATUS_OK);
        CHECK(yt2_roster_begin(&empty, &fresh.party, &fix.items,
                               &fix.pictures, &fix.surface) ==
              YT2_STATUS_OK);
        CHECK(!empty.dirty);
        yt2_roster_key(&empty, 'D');
        CHECK(yt2_roster_done(&empty) && !empty.party_ok);
        yt2_save_free(&fresh);
    }

    /* View by click and F-key, back by R. */
    CHECK(yt2_roster_begin(&roster, &fix.save.party, &fix.items,
                           &fix.pictures, &fix.surface) ==
          YT2_STATUS_OK);
    CHECK(yt2_roster_click(&roster, 30, 20) == YT2_STATUS_OK);
    CHECK(roster.view == YT2_ROSTER_VIEW_SHEET);
    CHECK(yt2_roster_key(&roster, 'R') == YT2_STATUS_OK);
    CHECK(yt2_roster_fkey(&roster, 6u) == YT2_STATUS_OK);
    CHECK(roster.slot == 5u);
    CHECK(yt2_roster_click(&roster, 30, 187) == YT2_STATUS_OK);
    CHECK(roster.view == YT2_ROSTER_VIEW_GRID);
    CHECK(yt2_roster_fkey(&roster, 2u) == YT2_STATUS_ERROR);

    /* Delete needs Y; N returns to the sheet. The flush fires exactly
       on confirm (vanilla writes the block mid-roster, not at DONE). */
    {
        unsigned int flushed = 0u;

        roster.flush = test_flush_count;
        roster.flush_context = &flushed;
        yt2_roster_fkey(&roster, 1u);
        CHECK(yt2_roster_key(&roster, 'D') == YT2_STATUS_OK);
        CHECK(roster.view == YT2_ROSTER_VIEW_CONFIRM);
        CHECK(flushed == 0u);
        CHECK(yt2_roster_key(&roster, 'N') == YT2_STATUS_OK);
        CHECK(roster.view == YT2_ROSTER_VIEW_SHEET);
        CHECK(flushed == 0u);
        CHECK(yt2_roster_key(&roster, 'D') == YT2_STATUS_OK);
        CHECK(yt2_roster_key(&roster, 'Y') == YT2_STATUS_OK);
        CHECK(roster.view == YT2_ROSTER_VIEW_GRID);
        CHECK(fix.save.party.roster[0].class_id == YT2_CLASS_NONE);
        CHECK(roster.dirty);
        CHECK(flushed == 1u);
        roster.flush = NULL;
        roster.flush_context = NULL;
    }

    /* A quest item blocks the delete. */
    fix.save.party.roster[5].pack[0].id = 30u;
    yt2_roster_fkey(&roster, 6u);
    CHECK(yt2_roster_key(&roster, 'D') == YT2_STATUS_OK);
    CHECK(roster.view == YT2_ROSTER_VIEW_SHEET);
    fixture_free(&fix);
}

int test_create(void)
{
    check_rng();
    check_percent();
    check_roll();
    check_rating();
    check_spells();
    check_alloc();
    check_conan_flow();
    check_conan_hashes();
    check_branches();
    check_quits();
    check_roster_branches();
    return test_failures;
}
