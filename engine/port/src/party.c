#include "yt2/party.h"

#include <string.h>

#include "yt2/endian.h"

/* Party block offsets (docs/structures.md). */
#define PARTY_HEADER_GAME_SPEED 0x88u
#define PARTY_HEADER_FACING 0x96u
#define PARTY_HEADER_X 0x98u
#define PARTY_HEADER_Y 0x9Au
#define PARTY_HEADER_DAY 0x9Cu
#define PARTY_HEADER_MONTH 0x9Eu
#define PARTY_HEADER_YEAR 0xA0u
#define PARTY_HEADER_SCENE 0xA2u
#define PARTY_HEADER_ACTIVE 0xA4u
#define PARTY_HEADER_POOL 0x11Au
#define PARTY_HEADER_ASSIGNED 0x1ECu
#define PARTY_HEADER_EXPLORE_FLAGS 0x20u
#define PARTY_HEADER_MUSIC_ID 0x52u
#define PARTY_HEADER_SOUND_STATE 0x86u
#define PARTY_HEADER_MEAN_MAPPING 0x46u
#define PARTY_HEADER_MEAN_NAVIGATION 0x48u
#define PARTY_HEADER_MEAN_SURVIVAL 0x4Au
#define PARTY_SLOT_BASE 0x1F4u
#define PARTY_SLOT_SIZE 0x1F4u

/* Character slot offsets. */
#define CHARACTER_CLASS 0x0Eu
#define CHARACTER_SEX 0x10u
#define CHARACTER_PORTRAIT_PIC 0x12u
#define CHARACTER_PORTRAIT_INDEX 0x14u
#define CHARACTER_LEVEL 0x16u
#define CHARACTER_EXPERIENCE 0x18u
#define CHARACTER_STATUS 0x1Cu
#define CHARACTER_READY 0x1Eu
#define CHARACTER_BUFFS 0x20u
#define CHARACTER_ICON_SEL 0xB4u
#define CHARACTER_TRANSPORT 0xB6u
#define CHARACTER_OVERLAY 0x15Eu
#define CHARACTER_SPELLBOOK 0xCAu
#define CHARACTER_CURRENT 0x3Cu
#define CHARACTER_MAXIMUM 0x7Cu
#define CHARACTER_BASE_OFFS_0 0x32u
#define CHARACTER_BASE_OFFS_1 0x36u
#define CHARACTER_BASE_OFFS_2 0x34u
#define CHARACTER_BASE_OFFS_3 0x38u
#define CHARACTER_BASE_OFFS_4 0x3Au
#define CHARACTER_MAX_BASE_OFFS_0 0x72u
#define CHARACTER_MAX_BASE_OFFS_1 0x76u
#define CHARACTER_MAX_BASE_OFFS_2 0x74u
#define CHARACTER_MAX_BASE_OFFS_3 0x78u
#define CHARACTER_MAX_BASE_OFFS_4 0x7Au
#define CHARACTER_CARRIED_WEIGHT 0x118u
#define CHARACTER_PACK 0x11Au
#define CHARACTER_EQUIPMENT 0x13Au
#define CHARACTER_EFFECTS 0x152u
#define CHARACTER_FLAGS 0x15Cu
#define CHARACTER_STAGE_0 0x17Cu
#define CHARACTER_STAGE_1 0x1A2u
#define CHARACTER_STAGE_2 0x1C8u
#define CHARACTER_WEAR 0xBEu

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

static void parse_character(const uint8_t *slot, yt2_character *character);

void yt2_party_parse_character(const uint8_t *slot, yt2_character *character)
{
    if (slot == NULL || character == NULL) {
        return;
    }
    parse_character(slot, character);
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
    character->status = yt2_read_u16(slot + CHARACTER_STATUS);
    character->ready = yt2_read_u16(slot + CHARACTER_READY);
    for (unsigned int buff = 0; buff < YT2_BUFF_SLOTS; buff++) {
        character->buffs[buff] =
            yt2_read_u16(slot + CHARACTER_BUFFS + buff * 2u);
    }
    for (unsigned int leg = 0; leg < YT2_SPELLBOOK_WORDS; leg++) {
        character->spellbook[leg] =
            yt2_read_u16(slot + CHARACTER_SPELLBOOK + leg * 2u);
    }
    parse_stats(slot + CHARACTER_CURRENT, &character->current);
    parse_stats(slot + CHARACTER_MAXIMUM, &character->maximum);
    {
        static const unsigned int offs[5] = {
            CHARACTER_BASE_OFFS_0, CHARACTER_BASE_OFFS_1,
            CHARACTER_BASE_OFFS_2, CHARACTER_BASE_OFFS_3,
            CHARACTER_BASE_OFFS_4,
        };
        static const unsigned int max_offs[5] = {
            CHARACTER_MAX_BASE_OFFS_0, CHARACTER_MAX_BASE_OFFS_1,
            CHARACTER_MAX_BASE_OFFS_2, CHARACTER_MAX_BASE_OFFS_3,
            CHARACTER_MAX_BASE_OFFS_4,
        };

        for (unsigned int i = 0u; i < 5u; i++) {
            character->base[i] = yt2_read_u16(slot + offs[i]);
            character->max_base[i] = yt2_read_u16(slot + max_offs[i]);
        }
    }
    character->icon_sel = yt2_read_u16(slot + CHARACTER_ICON_SEL);
    for (unsigned int ride = 0; ride < YT2_TRANSPORT_SLOTS; ride++) {
        character->transport[ride] =
            yt2_read_u16(slot + CHARACTER_TRANSPORT + ride * 2u);
    }
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
    {
        static const unsigned int offs[YT2_CONTAINER_SLOTS] = {
            CHARACTER_STAGE_0, CHARACTER_STAGE_1, CHARACTER_STAGE_2,
        };

        for (unsigned int s = 0u; s < YT2_CONTAINER_SLOTS; s++) {
            const uint8_t *stage = slot + offs[s];

            character->stage[s].id = yt2_read_u16(stage);
            character->stage[s].count = yt2_read_u16(stage + 2u);
            character->stage[s].weight = yt2_read_u16(stage + 4u);
            for (unsigned int c = 0u; c < YT2_CONTAINER_CELLS; c++) {
                character->stage[s].cells[c].id =
                    yt2_read_u16(stage + 6u + c * 4u);
                character->stage[s].cells[c].count =
                    yt2_read_u16(stage + 8u + c * 4u);
            }
        }
    }
    character->overlay = yt2_read_u16(slot + CHARACTER_OVERLAY);
    for (unsigned int wear = 0; wear < 3u; wear++) {
        character->wear[wear] =
            yt2_read_u16(slot + CHARACTER_WEAR + wear * 2u);
    }
}

int yt2_party_parse(const uint8_t *block, size_t size, yt2_party *party)
{
    /* No signature check, like vanilla's loader: a typed save name
       overwrites the header bytes via strcpy, and the game loads such
       saves blindly. */
    if (block == NULL || party == NULL || size < YT2_PARTY_BLOCK_SIZE) {
        return YT2_STATUS_ERROR;
    }

    memset(party, 0, sizeof(*party));
    party->game_speed = yt2_read_u16(block + PARTY_HEADER_GAME_SPEED);
    party->facing = yt2_read_u16(block + PARTY_HEADER_FACING);
    party->x = yt2_read_u16(block + PARTY_HEADER_X);
    party->y = yt2_read_u16(block + PARTY_HEADER_Y);
    party->day = yt2_read_u16(block + PARTY_HEADER_DAY);
    party->month = yt2_read_u16(block + PARTY_HEADER_MONTH);
    party->year = yt2_read_u16(block + PARTY_HEADER_YEAR);
    party->scene = yt2_read_u16(block + PARTY_HEADER_SCENE);
    party->explore_flags = yt2_read_u16(block + PARTY_HEADER_EXPLORE_FLAGS);
    party->music_id = yt2_read_u16(block + PARTY_HEADER_MUSIC_ID);
    party->sound_state = yt2_read_u16(block + PARTY_HEADER_SOUND_STATE);
    party->mean_mapping = yt2_read_u16(block + PARTY_HEADER_MEAN_MAPPING);
    party->mean_navigation =
        yt2_read_u16(block + PARTY_HEADER_MEAN_NAVIGATION);
    party->mean_survival = yt2_read_u16(block + PARTY_HEADER_MEAN_SURVIVAL);
    for (unsigned int index = 0; index < YT2_PARTY_ACTIVE_MAX; index++) {
        party->active[index] =
            yt2_read_u16(block + PARTY_HEADER_ACTIVE + index * 2u);
    }
    for (unsigned int index = 0; index < YT2_POOL_SLOTS; index++) {
        const uint8_t *item = block + PARTY_HEADER_POOL + index * 4u;

        party->pool[index].id = yt2_read_u16(item);
        party->pool[index].count = yt2_read_u16(item + 2u);
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
    yt2_write_u16(slot + CHARACTER_STATUS, character->status);
    yt2_write_u16(slot + CHARACTER_READY, character->ready);
    for (unsigned int buff = 0; buff < YT2_BUFF_SLOTS; buff++) {
        yt2_write_u16(slot + CHARACTER_BUFFS + buff * 2u,
                      character->buffs[buff]);
    }
    for (unsigned int leg = 0; leg < YT2_SPELLBOOK_WORDS; leg++) {
        yt2_write_u16(slot + CHARACTER_SPELLBOOK + leg * 2u,
                      character->spellbook[leg]);
    }
    write_stats(slot + CHARACTER_CURRENT, &character->current);
    write_stats(slot + CHARACTER_MAXIMUM, &character->maximum);
    {
        static const unsigned int offs[5] = {
            CHARACTER_BASE_OFFS_0, CHARACTER_BASE_OFFS_1,
            CHARACTER_BASE_OFFS_2, CHARACTER_BASE_OFFS_3,
            CHARACTER_BASE_OFFS_4,
        };
        static const unsigned int max_offs[5] = {
            CHARACTER_MAX_BASE_OFFS_0, CHARACTER_MAX_BASE_OFFS_1,
            CHARACTER_MAX_BASE_OFFS_2, CHARACTER_MAX_BASE_OFFS_3,
            CHARACTER_MAX_BASE_OFFS_4,
        };

        for (unsigned int i = 0u; i < 5u; i++) {
            yt2_write_u16(slot + offs[i], character->base[i]);
            yt2_write_u16(slot + max_offs[i], character->max_base[i]);
        }
    }
    yt2_write_u16(slot + CHARACTER_ICON_SEL, character->icon_sel);
    for (unsigned int ride = 0; ride < YT2_TRANSPORT_SLOTS; ride++) {
        yt2_write_u16(slot + CHARACTER_TRANSPORT + ride * 2u,
                      character->transport[ride]);
    }
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
    {
        static const unsigned int offs[YT2_CONTAINER_SLOTS] = {
            CHARACTER_STAGE_0, CHARACTER_STAGE_1, CHARACTER_STAGE_2,
        };

        for (unsigned int s = 0u; s < YT2_CONTAINER_SLOTS; s++) {
            uint8_t *stage = slot + offs[s];

            yt2_write_u16(stage, character->stage[s].id);
            yt2_write_u16(stage + 2u, character->stage[s].count);
            yt2_write_u16(stage + 4u, character->stage[s].weight);
            for (unsigned int c = 0u; c < YT2_CONTAINER_CELLS; c++) {
                yt2_write_u16(stage + 6u + c * 4u,
                              character->stage[s].cells[c].id);
                yt2_write_u16(stage + 8u + c * 4u,
                              character->stage[s].cells[c].count);
            }
        }
    }
    yt2_write_u16(slot + CHARACTER_OVERLAY, character->overlay);
    for (unsigned int wear = 0; wear < 3u; wear++) {
        yt2_write_u16(slot + CHARACTER_WEAR + wear * 2u,
                      character->wear[wear]);
    }
}

int yt2_party_write(uint8_t *block, size_t size, const yt2_party *party)
{
    if (block == NULL || party == NULL || size < YT2_PARTY_BLOCK_SIZE) {
        return YT2_STATUS_ERROR;
    }

    yt2_write_u16(block + PARTY_HEADER_GAME_SPEED, party->game_speed);
    yt2_write_u16(block + PARTY_HEADER_FACING, party->facing);
    yt2_write_u16(block + PARTY_HEADER_X, party->x);
    yt2_write_u16(block + PARTY_HEADER_Y, party->y);
    yt2_write_u16(block + PARTY_HEADER_DAY, party->day);
    yt2_write_u16(block + PARTY_HEADER_MONTH, party->month);
    yt2_write_u16(block + PARTY_HEADER_YEAR, party->year);
    yt2_write_u16(block + PARTY_HEADER_SCENE, party->scene);
    yt2_write_u16(block + PARTY_HEADER_EXPLORE_FLAGS, party->explore_flags);
    yt2_write_u16(block + PARTY_HEADER_MUSIC_ID, party->music_id);
    yt2_write_u16(block + PARTY_HEADER_SOUND_STATE, party->sound_state);
    yt2_write_u16(block + PARTY_HEADER_MEAN_MAPPING, party->mean_mapping);
    yt2_write_u16(block + PARTY_HEADER_MEAN_NAVIGATION,
                  party->mean_navigation);
    yt2_write_u16(block + PARTY_HEADER_MEAN_SURVIVAL, party->mean_survival);
    for (unsigned int index = 0; index < YT2_PARTY_ACTIVE_MAX; index++) {
        yt2_write_u16(block + PARTY_HEADER_ACTIVE + index * 2u,
                      party->active[index]);
    }
    for (unsigned int index = 0; index < YT2_POOL_SLOTS; index++) {
        uint8_t *item = block + PARTY_HEADER_POOL + index * 4u;

        yt2_write_u16(item, party->pool[index].id);
        yt2_write_u16(item + 2u, party->pool[index].count);
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
