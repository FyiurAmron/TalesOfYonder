#include "yt2/party.h"

#include <string.h>

#include "yt2/endian.h"

/* Party block offsets (docs/structures.md). */
#define PARTY_HEADER_GAME_SPEED 0x88u
#define PARTY_HEADER_FACING 0x96u
#define PARTY_HEADER_X 0x98u
#define PARTY_HEADER_Y 0x9Au
#define PARTY_HEADER_SCENE 0xA2u
#define PARTY_HEADER_ACTIVE 0xA4u
#define PARTY_HEADER_ASSIGNED 0x1ECu
#define PARTY_SLOT_BASE 0x1F4u
#define PARTY_SLOT_SIZE 0x1F4u

/* Character slot offsets. */
#define CHARACTER_CLASS 0x0Eu
#define CHARACTER_SEX 0x10u
#define CHARACTER_PORTRAIT_PIC 0x12u
#define CHARACTER_PORTRAIT_INDEX 0x14u
#define CHARACTER_LEVEL 0x16u
#define CHARACTER_EXPERIENCE 0x18u
#define CHARACTER_WEIGHT 0x1Bu
#define CHARACTER_STATUS 0x1Cu
#define CHARACTER_SPELLBOOK 0xCAu
#define CHARACTER_CURRENT 0x3Cu
#define CHARACTER_MAXIMUM 0x7Cu
#define CHARACTER_CARRIED_WEIGHT 0x118u
#define CHARACTER_PACK 0x11Au
#define CHARACTER_EQUIPMENT 0x13Au
#define CHARACTER_EFFECTS 0x152u
#define CHARACTER_FLAGS 0x15Cu

/* Stat block offsets. */
#define STAT_STRENGTH 0x00u
#define STAT_DEXTERITY 0x02u
#define STAT_STAMINA 0x04u
#define STAT_INTELLIGENCE 0x06u
#define STAT_WISDOM 0x08u
#define STAT_CHARISMA 0x0Au
#define STAT_PROJECTILE_ACCURACY 0x0Cu
#define STAT_PROJECTILE_DAMAGE 0x0Eu
#define STAT_BATTLE_ACCURACY 0x10u
#define STAT_BATTLE_DAMAGE 0x12u
#define STAT_ABSORPTION 0x14u
#define STAT_HIT_POINTS 0x16u
#define STAT_MAGIC_POINTS 0x18u
#define STAT_CARRY_CAPACITY 0x1Au
#define STAT_SURVIVAL 0x1Cu
#define STAT_PROJECTILE 0x1Eu
#define STAT_SLASHING 0x20u
#define STAT_BASHING 0x22u
#define STAT_POLEARM 0x24u
#define STAT_CASTING 0x26u
#define STAT_MAPPING 0x28u
#define STAT_NAVIGATION 0x2Au
#define STAT_BARTERING 0x2Cu
#define STAT_REPAIR 0x2Eu
#define STAT_THIEVERY 0x30u
#define STAT_LINGUISTICS 0x32u
#define STAT_CHEMISTRY 0x34u

static const char *const class_names[] = {
    "none",
    "fighter",
    "merchant",
    "rogue",
    "monk",
    "alchemist",
    "paladin",
    "mage",
    "druid",
    "marksman"
};

const char *yt2_class_name(yt2_class class_id)
{
    unsigned int value = (unsigned int)class_id;

    if (value >= sizeof(class_names) / sizeof(class_names[0])) {
        return "?";
    }
    return class_names[value];
}

const char *yt2_sex_name(yt2_sex sex)
{
    switch (sex) {
    case YT2_SEX_MALE:
        return "male";
    case YT2_SEX_FEMALE:
        return "female";
    case YT2_SEX_NONE:
        return "none";
    }
    return "?";
}

static void parse_stats(const uint8_t *bytes, yt2_stats *stats)
{
    stats->attributes.strength = yt2_read_u16(bytes + STAT_STRENGTH);
    stats->attributes.dexterity = yt2_read_u16(bytes + STAT_DEXTERITY);
    stats->attributes.stamina = yt2_read_u16(bytes + STAT_STAMINA);
    stats->attributes.intelligence = yt2_read_u16(bytes + STAT_INTELLIGENCE);
    stats->attributes.wisdom = yt2_read_u16(bytes + STAT_WISDOM);
    stats->attributes.charisma = yt2_read_u16(bytes + STAT_CHARISMA);

    stats->combat.projectile_accuracy =
        yt2_read_u16(bytes + STAT_PROJECTILE_ACCURACY);
    stats->combat.projectile_damage =
        yt2_read_u16(bytes + STAT_PROJECTILE_DAMAGE);
    stats->combat.battle_accuracy = yt2_read_u16(bytes + STAT_BATTLE_ACCURACY);
    stats->combat.battle_damage = yt2_read_u16(bytes + STAT_BATTLE_DAMAGE);
    stats->combat.absorption = yt2_read_u16(bytes + STAT_ABSORPTION);
    stats->combat.hit_points = yt2_read_u16(bytes + STAT_HIT_POINTS);
    stats->combat.magic_points = yt2_read_u16(bytes + STAT_MAGIC_POINTS);
    stats->combat.carry_capacity = yt2_read_u16(bytes + STAT_CARRY_CAPACITY);

    stats->skills.survival = yt2_read_u16(bytes + STAT_SURVIVAL);
    stats->skills.projectile = yt2_read_u16(bytes + STAT_PROJECTILE);
    stats->skills.slashing = yt2_read_u16(bytes + STAT_SLASHING);
    stats->skills.bashing = yt2_read_u16(bytes + STAT_BASHING);
    stats->skills.polearm = yt2_read_u16(bytes + STAT_POLEARM);
    stats->skills.casting = yt2_read_u16(bytes + STAT_CASTING);
    stats->skills.mapping = yt2_read_u16(bytes + STAT_MAPPING);
    stats->skills.navigation = yt2_read_u16(bytes + STAT_NAVIGATION);
    stats->skills.bartering = yt2_read_u16(bytes + STAT_BARTERING);
    stats->skills.repair = yt2_read_u16(bytes + STAT_REPAIR);
    stats->skills.thievery = yt2_read_u16(bytes + STAT_THIEVERY);
    stats->skills.linguistics = yt2_read_u16(bytes + STAT_LINGUISTICS);
    stats->skills.chemistry = yt2_read_u16(bytes + STAT_CHEMISTRY);
}

static void parse_character(const uint8_t *slot, yt2_character *character)
{
    memcpy(character->name, slot, YT2_CHARACTER_NAME_LENGTH);
    character->name[YT2_CHARACTER_NAME_LENGTH] = '\0';
    character->class_id = (yt2_class)yt2_read_u16(slot + CHARACTER_CLASS);
    character->sex = (yt2_sex)yt2_read_u16(slot + CHARACTER_SEX);
    character->portrait_pic = yt2_read_u16(slot + CHARACTER_PORTRAIT_PIC);
    character->portrait_index = yt2_read_u16(slot + CHARACTER_PORTRAIT_INDEX);
    character->level = yt2_read_u16(slot + CHARACTER_LEVEL);
    character->experience = yt2_read_u32(slot + CHARACTER_EXPERIENCE);
    character->weight = slot[CHARACTER_WEIGHT];
    character->status = yt2_read_u16(slot + CHARACTER_STATUS);
    for (unsigned int leg = 0; leg < YT2_SPELLBOOK_WORDS; leg++) {
        character->spellbook[leg] =
            yt2_read_u16(slot + CHARACTER_SPELLBOOK + leg * 2u);
    }
    parse_stats(slot + CHARACTER_CURRENT, &character->current);
    parse_stats(slot + CHARACTER_MAXIMUM, &character->maximum);
    character->carried_weight = yt2_read_u16(slot + CHARACTER_CARRIED_WEIGHT);

    for (unsigned int index = 0; index < YT2_PACK_SLOTS; index++) {
        const uint8_t *item = slot + CHARACTER_PACK + index * 4u;

        character->pack[index].id = yt2_read_u16(item);
        character->pack[index].count = yt2_read_u16(item + 2u);
    }
    for (unsigned int index = 0; index < YT2_EQUIPMENT_SLOTS; index++) {
        const uint8_t *item = slot + CHARACTER_EQUIPMENT + index * 4u;

        character->equipment[index].id = yt2_read_u16(item);
        character->equipment[index].count = yt2_read_u16(item + 2u);
    }
    for (unsigned int index = 0; index < YT2_EFFECT_SLOTS; index++) {
        character->effects[index] =
            yt2_read_u16(slot + CHARACTER_EFFECTS + index * 2u);
    }
    character->flags = yt2_read_u16(slot + CHARACTER_FLAGS);
}

static int signature_valid(const uint8_t *block)
{
    /* Byte 00h is 00 in a written save and 'S' in the static template;
       the signature proper starts at 01h. */
    return memcmp(block + 1, "MITHWARE PARTY", 14) == 0 && block[15] == 0u;
}

int yt2_party_parse(const uint8_t *block, size_t size, yt2_party *party)
{
    if (block == NULL || party == NULL || size < YT2_PARTY_BLOCK_SIZE ||
        !signature_valid(block)) {
        return YT2_STATUS_ERROR;
    }

    memset(party, 0, sizeof(*party));
    party->game_speed = yt2_read_u16(block + PARTY_HEADER_GAME_SPEED);
    party->facing = yt2_read_u16(block + PARTY_HEADER_FACING);
    party->x = yt2_read_u16(block + PARTY_HEADER_X);
    party->y = yt2_read_u16(block + PARTY_HEADER_Y);
    party->scene = yt2_read_u16(block + PARTY_HEADER_SCENE);
    for (unsigned int index = 0; index < YT2_PARTY_ACTIVE_MAX; index++) {
        party->active[index] =
            yt2_read_u16(block + PARTY_HEADER_ACTIVE + index * 2u);
    }
    for (unsigned int index = 0; index < YT2_PARTY_ASSIGNED_MAX; index++) {
        party->assigned[index] =
            yt2_read_u16(block + PARTY_HEADER_ASSIGNED + index * 2u);
    }

    for (unsigned int index = 0; index < YT2_ROSTER_SLOTS; index++) {
        const uint8_t *slot = block + PARTY_SLOT_BASE + index * PARTY_SLOT_SIZE;

        parse_character(slot, &party->roster[index]);
        if (party->roster[index].name[0] != '\0') {
            party->roster_count++;
        }
    }
    return YT2_STATUS_OK;
}

static void write_name(uint8_t *slot, const char *name)
{
    size_t length = 0;

    while (length < YT2_CHARACTER_NAME_LENGTH && name[length] != '\0') {
        length++;
    }
    memset(slot, 0, YT2_CHARACTER_NAME_LENGTH);
    memcpy(slot, name, length);
}

static void write_stats(uint8_t *bytes, const yt2_stats *stats)
{
    yt2_write_u16(bytes + STAT_STRENGTH, stats->attributes.strength);
    yt2_write_u16(bytes + STAT_DEXTERITY, stats->attributes.dexterity);
    yt2_write_u16(bytes + STAT_STAMINA, stats->attributes.stamina);
    yt2_write_u16(bytes + STAT_INTELLIGENCE, stats->attributes.intelligence);
    yt2_write_u16(bytes + STAT_WISDOM, stats->attributes.wisdom);
    yt2_write_u16(bytes + STAT_CHARISMA, stats->attributes.charisma);

    yt2_write_u16(bytes + STAT_PROJECTILE_ACCURACY,
                  stats->combat.projectile_accuracy);
    yt2_write_u16(bytes + STAT_PROJECTILE_DAMAGE,
                  stats->combat.projectile_damage);
    yt2_write_u16(bytes + STAT_BATTLE_ACCURACY, stats->combat.battle_accuracy);
    yt2_write_u16(bytes + STAT_BATTLE_DAMAGE, stats->combat.battle_damage);
    yt2_write_u16(bytes + STAT_ABSORPTION, stats->combat.absorption);
    yt2_write_u16(bytes + STAT_HIT_POINTS, stats->combat.hit_points);
    yt2_write_u16(bytes + STAT_MAGIC_POINTS, stats->combat.magic_points);
    yt2_write_u16(bytes + STAT_CARRY_CAPACITY, stats->combat.carry_capacity);

    yt2_write_u16(bytes + STAT_SURVIVAL, stats->skills.survival);
    yt2_write_u16(bytes + STAT_PROJECTILE, stats->skills.projectile);
    yt2_write_u16(bytes + STAT_SLASHING, stats->skills.slashing);
    yt2_write_u16(bytes + STAT_BASHING, stats->skills.bashing);
    yt2_write_u16(bytes + STAT_POLEARM, stats->skills.polearm);
    yt2_write_u16(bytes + STAT_CASTING, stats->skills.casting);
    yt2_write_u16(bytes + STAT_MAPPING, stats->skills.mapping);
    yt2_write_u16(bytes + STAT_NAVIGATION, stats->skills.navigation);
    yt2_write_u16(bytes + STAT_BARTERING, stats->skills.bartering);
    yt2_write_u16(bytes + STAT_REPAIR, stats->skills.repair);
    yt2_write_u16(bytes + STAT_THIEVERY, stats->skills.thievery);
    yt2_write_u16(bytes + STAT_LINGUISTICS, stats->skills.linguistics);
    yt2_write_u16(bytes + STAT_CHEMISTRY, stats->skills.chemistry);
}

static void write_character(uint8_t *slot, const yt2_character *character)
{
    write_name(slot, character->name);
    yt2_write_u16(slot + CHARACTER_CLASS, (uint16_t)character->class_id);
    yt2_write_u16(slot + CHARACTER_SEX, (uint16_t)character->sex);
    yt2_write_u16(slot + CHARACTER_PORTRAIT_PIC, character->portrait_pic);
    yt2_write_u16(slot + CHARACTER_PORTRAIT_INDEX, character->portrait_index);
    yt2_write_u16(slot + CHARACTER_LEVEL, character->level);
    yt2_write_u32(slot + CHARACTER_EXPERIENCE, character->experience);
    slot[CHARACTER_WEIGHT] = character->weight;
    yt2_write_u16(slot + CHARACTER_STATUS, character->status);
    for (unsigned int leg = 0; leg < YT2_SPELLBOOK_WORDS; leg++) {
        yt2_write_u16(slot + CHARACTER_SPELLBOOK + leg * 2u,
                      character->spellbook[leg]);
    }
    write_stats(slot + CHARACTER_CURRENT, &character->current);
    write_stats(slot + CHARACTER_MAXIMUM, &character->maximum);
    yt2_write_u16(slot + CHARACTER_CARRIED_WEIGHT, character->carried_weight);

    for (unsigned int index = 0; index < YT2_PACK_SLOTS; index++) {
        uint8_t *item = slot + CHARACTER_PACK + index * 4u;

        yt2_write_u16(item, character->pack[index].id);
        yt2_write_u16(item + 2u, character->pack[index].count);
    }
    for (unsigned int index = 0; index < YT2_EQUIPMENT_SLOTS; index++) {
        uint8_t *item = slot + CHARACTER_EQUIPMENT + index * 4u;

        yt2_write_u16(item, character->equipment[index].id);
        yt2_write_u16(item + 2u, character->equipment[index].count);
    }
    for (unsigned int index = 0; index < YT2_EFFECT_SLOTS; index++) {
        yt2_write_u16(slot + CHARACTER_EFFECTS + index * 2u,
                      character->effects[index]);
    }
    yt2_write_u16(slot + CHARACTER_FLAGS, character->flags);
}

int yt2_party_write(uint8_t *block, size_t size, const yt2_party *party)
{
    if (block == NULL || party == NULL || size < YT2_PARTY_BLOCK_SIZE ||
        !signature_valid(block)) {
        return YT2_STATUS_ERROR;
    }

    yt2_write_u16(block + PARTY_HEADER_GAME_SPEED, party->game_speed);
    yt2_write_u16(block + PARTY_HEADER_FACING, party->facing);
    yt2_write_u16(block + PARTY_HEADER_X, party->x);
    yt2_write_u16(block + PARTY_HEADER_Y, party->y);
    yt2_write_u16(block + PARTY_HEADER_SCENE, party->scene);
    for (unsigned int index = 0; index < YT2_PARTY_ACTIVE_MAX; index++) {
        yt2_write_u16(block + PARTY_HEADER_ACTIVE + index * 2u,
                      party->active[index]);
    }
    for (unsigned int index = 0; index < YT2_PARTY_ASSIGNED_MAX; index++) {
        yt2_write_u16(block + PARTY_HEADER_ASSIGNED + index * 2u,
                      party->assigned[index]);
    }

    for (unsigned int index = 0; index < YT2_ROSTER_SLOTS; index++) {
        write_character(block + PARTY_SLOT_BASE + index * PARTY_SLOT_SIZE,
                        &party->roster[index]);
    }
    return YT2_STATUS_OK;
}
