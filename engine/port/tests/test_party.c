#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/endian.h"
#include "yt2/party.h"

#define TEST_SLOT_BASE 0x1F4u
#define TEST_SLOT_SIZE 0x1F4u
#define TEST_CURRENT 0x3Cu
#define TEST_MAXIMUM 0x7Cu

static void set_current(uint8_t *block, unsigned int slot, unsigned int offset,
                        uint16_t value)
{
    yt2_write_u16(block + TEST_SLOT_BASE + slot * TEST_SLOT_SIZE + TEST_CURRENT +
                      offset,
                  value);
}

static void set_maximum(uint8_t *block, unsigned int slot, unsigned int offset,
                        uint16_t value)
{
    yt2_write_u16(block + TEST_SLOT_BASE + slot * TEST_SLOT_SIZE + TEST_MAXIMUM +
                      offset,
                  value);
}

int test_party(void)
{
    uint8_t *block = calloc(YT2_PARTY_BLOCK_SIZE, 1);
    yt2_party party;

    CHECK(block != NULL);
    if (block == NULL) {
        return 0;
    }
    block[0] = 0u;
    memcpy(block + 1, "MITHWARE PARTY", 14);
    block[15] = 0u;
    yt2_write_u16(block + 0x88, 5u);
    yt2_write_u16(block + 0x96, 0x4000u);
    yt2_write_u16(block + 0x98, 161u);
    yt2_write_u16(block + 0x9A, 34u);
    yt2_write_u16(block + 0xA2, 453u);

    memcpy(block + TEST_SLOT_BASE, "CONAN", 5);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x0E, 1u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x10, 1u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x12, 19u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x14, 0u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x16, 1u);
    block[TEST_SLOT_BASE + 0x1B] = 37u;
    yt2_write_u16(block + TEST_SLOT_BASE + 0x1C, 0u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x1E, 1u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x22, 3u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0xB4, 0u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0xB6, 11u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0xB8, 22u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x15E, 0u);

    set_current(block, 0u, 0x00u, 50u);
    set_current(block, 0u, 0x02u, 49u);
    set_current(block, 0u, 0x04u, 50u);
    set_current(block, 0u, 0x06u, 52u);
    set_current(block, 0u, 0x08u, 60u);
    set_current(block, 0u, 0x0Au, 49u);
    set_current(block, 0u, 0x0Cu, 55u);
    set_current(block, 0u, 0x0Eu, 1u);
    set_current(block, 0u, 0x10u, 54u);
    set_current(block, 0u, 0x12u, 3u);
    set_current(block, 0u, 0x16u, 4u);
    set_current(block, 0u, 0x1Au, 500u);
    set_current(block, 0u, 0x1Cu, 55u);
    set_current(block, 0u, 0x1Eu, 55u);
    set_current(block, 0u, 0x20u, 54u);
    set_current(block, 0u, 0x22u, 55u);
    set_current(block, 0u, 0x24u, 55u);
    set_current(block, 0u, 0x28u, 40u);
    set_current(block, 0u, 0x2Au, 49u);
    set_current(block, 0u, 0x2Cu, 40u);
    set_current(block, 0u, 0x2Eu, 52u);
    set_current(block, 0u, 0x30u, 40u);

    set_maximum(block, 0u, 0x00u, 50u);
    set_maximum(block, 0u, 0x02u, 49u);
    set_maximum(block, 0u, 0x04u, 50u);
    set_maximum(block, 0u, 0x06u, 52u);
    set_maximum(block, 0u, 0x08u, 60u);
    set_maximum(block, 0u, 0x0Au, 49u);
    set_maximum(block, 0u, 0x0Cu, 55u);
    set_maximum(block, 0u, 0x0Eu, 1u);
    set_maximum(block, 0u, 0x10u, 54u);
    set_maximum(block, 0u, 0x12u, 3u);
    set_maximum(block, 0u, 0x16u, 13u);
    set_maximum(block, 0u, 0x1Au, 500u);
    set_maximum(block, 0u, 0x1Cu, 55u);
    set_maximum(block, 0u, 0x1Eu, 55u);
    set_maximum(block, 0u, 0x20u, 54u);
    set_maximum(block, 0u, 0x22u, 55u);
    set_maximum(block, 0u, 0x24u, 55u);
    set_maximum(block, 0u, 0x28u, 40u);
    set_maximum(block, 0u, 0x2Au, 49u);
    set_maximum(block, 0u, 0x2Cu, 40u);
    set_maximum(block, 0u, 0x2Eu, 52u);
    set_maximum(block, 0u, 0x30u, 40u);

    yt2_write_u16(block + TEST_SLOT_BASE + 0x118, 19u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x11A, 0x1234u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x11C, 2u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x13A, 542u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x152, 362u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x15C, 0x0880u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x32, 0x5A5Bu);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x36, 0x1112u);
    yt2_write_u16(block + TEST_SLOT_BASE + 0x72, 0x2122u);

    CHECK(yt2_party_parse(block, YT2_PARTY_BLOCK_SIZE, &party) ==
          YT2_STATUS_OK);
    CHECK(party.game_speed == 5u);
    CHECK(party.x == 161u && party.y == 34u);
    CHECK(party.facing == 0x4000u && party.scene == 453u);
    CHECK(party.roster_count == 1u);
    CHECK(strcmp(party.roster[0].name, "CONAN") == 0);
    CHECK(party.roster[0].class_id == YT2_CLASS_FIGHTER);
    CHECK(party.roster[0].sex == YT2_SEX_MALE);
    CHECK(party.roster[0].portrait_pic == 19u);
    CHECK(party.roster[0].portrait_index == 0u);
    CHECK(party.roster[0].level == 1u);
    CHECK(party.roster[0].experience == 0x25000000u);
    CHECK(party.roster[0].current.attributes.strength == 50u);
    CHECK(party.roster[0].current.attributes.charisma == 49u);
    CHECK(party.roster[0].current.combat.hit_points == 4u);
    CHECK(party.roster[0].maximum.combat.hit_points == 13u);
    CHECK(party.roster[0].current.combat.carry_capacity == 500u);
    CHECK(party.roster[0].current.skills.navigation == 49u);
    CHECK(party.roster[0].pack[0].id == 0x1234u);
    CHECK(party.roster[0].pack[0].count == 2u);
    CHECK(party.roster[0].equipment[0].id == 542u);
    CHECK(party.roster[0].effects[0] == 362u);
    CHECK(party.roster[0].flags == 0x0880u);
    CHECK(party.roster[0].ready == 1u);
    CHECK(party.roster[0].buffs[0] == 0u);
    CHECK(party.roster[0].buffs[1] == 3u);
    CHECK(party.roster[0].icon_sel == 0u);
    CHECK(party.roster[0].transport[0] == 11u);
    CHECK(party.roster[0].transport[1] == 22u);
    CHECK(party.roster[0].transport[3] == 0u);
    CHECK(party.roster[0].overlay == 0u);
    CHECK(party.roster[0].base[0] == 0x5A5Bu);
    CHECK(party.roster[0].base[1] == 0x1112u);
    CHECK(party.roster[0].max_base[0] == 0x2122u);
    CHECK(party.roster[1].name[0] == '\0');

    CHECK(strcmp(yt2_class_name(YT2_CLASS_MARKSMAN), "marksman") == 0);
    CHECK(strcmp(yt2_sex_name(YT2_SEX_FEMALE), "female") == 0);

    {
        uint8_t *written = malloc(YT2_PARTY_BLOCK_SIZE);
        yt2_party reread;

        CHECK(written != NULL);
        if (written != NULL) {
            block[0x10] = 0xA5u;
            block[TEST_SLOT_BASE + 0x100u] = 0x5Au;
            block[TEST_SLOT_BASE + 0x17Cu] = 0x3Cu;

            memcpy(written, block, YT2_PARTY_BLOCK_SIZE);
            CHECK(yt2_party_write(written, YT2_PARTY_BLOCK_SIZE, &party) ==
                  YT2_STATUS_OK);
            CHECK(memcmp(written, block, YT2_PARTY_BLOCK_SIZE) == 0);

            party.game_speed = 7u;
            party.x = 0x1111u;
            party.y = 0x2222u;
            party.facing = 0x3333u;
            party.scene = 0x4444u;
            memcpy(party.roster[0].name, "CONAN2", 7);
            party.roster[0].level = 3u;
            party.roster[0].current.combat.hit_points = 9u;
            party.roster[0].pack[1].id = 0xBEEFu;
            party.roster[0].pack[1].count = 5u;
            party.roster[0].ready = 0u;
            party.roster[0].buffs[1] = 7u;
            party.roster[0].icon_sel = 2u;
            party.roster[0].transport[2] = 33u;
            party.roster[0].overlay = 0x8000u;

            CHECK(yt2_party_write(written, YT2_PARTY_BLOCK_SIZE, &party) ==
                  YT2_STATUS_OK);
            CHECK(written[0x10] == 0xA5u);
            CHECK(written[TEST_SLOT_BASE + 0x100u] == 0x5Au);
            CHECK(written[TEST_SLOT_BASE + 0x17Cu] == 0x3Cu);
            CHECK(yt2_read_u16(written + TEST_SLOT_BASE + 0x1E) == 0u);
            CHECK(yt2_read_u16(written + TEST_SLOT_BASE + 0x22) == 7u);
            CHECK(yt2_read_u16(written + TEST_SLOT_BASE + 0xB4) == 2u);
            CHECK(yt2_read_u16(written + TEST_SLOT_BASE + 0xBA) == 33u);
            CHECK(yt2_read_u16(written + TEST_SLOT_BASE + 0x15E) == 0x8000u);

            CHECK(yt2_party_parse(written, YT2_PARTY_BLOCK_SIZE, &reread) ==
                  YT2_STATUS_OK);
            CHECK(reread.game_speed == 7u);
            CHECK(reread.x == 0x1111u && reread.y == 0x2222u);
            CHECK(reread.facing == 0x3333u && reread.scene == 0x4444u);
            CHECK(strcmp(reread.roster[0].name, "CONAN2") == 0);
            CHECK(reread.roster[0].level == 3u);
            CHECK(reread.roster[0].current.combat.hit_points == 9u);
            CHECK(reread.roster[0].pack[1].id == 0xBEEFu);
            CHECK(reread.roster[0].pack[1].count == 5u);
            CHECK(reread.roster[0].pack[0].id == 0x1234u);
            CHECK(reread.roster[0].transport[0] == 11u);
            CHECK(reread.roster[0].transport[2] == 33u);
            free(written);
        }
    }

    CHECK(yt2_party_write(NULL, YT2_PARTY_BLOCK_SIZE, &party) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_party_write(block, YT2_PARTY_BLOCK_SIZE - 1u, &party) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_party_write(block, YT2_PARTY_BLOCK_SIZE, NULL) ==
          YT2_STATUS_ERROR);

    /* No signature check, like vanilla's loader: a typed save name
       overwrites the header bytes and still loads. */
    block[5] = 'X';
    CHECK(yt2_party_parse(block, YT2_PARTY_BLOCK_SIZE, &party) ==
          YT2_STATUS_OK);
    block[5] = 'M';
    CHECK(yt2_party_parse(block, YT2_PARTY_BLOCK_SIZE - 1u, &party) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_party_parse(NULL, YT2_PARTY_BLOCK_SIZE, &party) ==
          YT2_STATUS_ERROR);
    CHECK(yt2_party_parse(block, YT2_PARTY_BLOCK_SIZE, NULL) ==
          YT2_STATUS_ERROR);

    free(block);
    return 0;
}
