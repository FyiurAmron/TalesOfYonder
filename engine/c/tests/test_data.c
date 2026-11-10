#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/core.h"
#include "yt2/data.h"

/*
 * The typed save model: the struct layout is compile-time-checked in data.h,
 * and this exercises the byte-level parser/writer and the chunk table against
 * the sizes of a real CURGAME / SAVEGAMEn (77509 bytes).
 */
int test_data(void)
{
    uint8_t *file = calloc(YT2_SAVE_SIZE, 1);
    yt2_party_block party;
    yt2_party_block readback;
    uint32_t covered = 0;

    CHECK(file != NULL);
    if (file == NULL) {
        return 0;
    }
    CHECK(YT2_SAVE_SIZE == 77509u);
    CHECK(YT2_PARTY_BLOCK_SIZE == 5000u);

    memset(&party, 0, sizeof(party));
    party.header.signature[0] = 0u;
    memcpy(party.header.signature + 1, "MITHWARE PARTY", 14);
    party.header.game_speed = 5u;
    party.header.facing = 0x4000u;
    party.header.x = 0x00A1u;
    party.header.y = 0x0022u;
    party.header.scene = 0x01C5u;
    memcpy(party.slots[0].name, "CONAN", 6);
    party.slots[0].class_id = 1u;
    party.slots[0].weight = 37u;
    party.slots[0].current.strength = 50u;
    party.slots[0].current.hit_points = 4u;
    party.slots[0].current.carry_capacity = 500u;
    party.slots[0].maximum.hit_points = 13u;
    party.slots[0].carried_weight = 19u;
    party.slots[0].pack[0].id = 0x1234u;
    party.slots[0].pack[0].count = 2u;
    party.slots[0].equipment[0].id = 0x021Eu;
    party.slots[0].effects[0] = 0x016Au;
    party.slots[0].flags = 0x0880u;

    CHECK(yt2_party_block_write(&party, file, YT2_SAVE_SIZE) == YT2_OK);
    CHECK(yt2_party_block_parse(file, YT2_SAVE_SIZE, &readback) == YT2_OK);
    CHECK(memcmp(&party, &readback, sizeof(party)) == 0);
    CHECK(readback.header.x == 0x00A1u && readback.header.y == 0x0022u);
    CHECK(readback.slots[0].current.carry_capacity == 500u);
    CHECK(readback.slots[0].pack[0].id == 0x1234u);

    /* Both signatures are real: the save routine writes 00h first, the game's
       static DGROUP template has 'S' (so the string reads SMITHWARE PARTY). */
    file[0] = 0x00u;
    CHECK(yt2_party_block_parse(file, YT2_SAVE_SIZE, &readback) == YT2_OK);
    file[0] = 'S';
    CHECK(yt2_party_block_parse(file, YT2_SAVE_SIZE, &readback) == YT2_OK);
    file[0] = 'X';
    CHECK(yt2_party_block_parse(file, YT2_SAVE_SIZE, &readback) == YT2_OK);
    file[5] = 'X';
    CHECK(yt2_party_block_parse(file, YT2_SAVE_SIZE, &readback) == YT2_ERR);
    CHECK(yt2_party_block_write(&party, file, YT2_PARTY_BLOCK_SIZE - 1u) ==
          YT2_ERR);
    CHECK(yt2_party_block_parse(file, YT2_PARTY_BLOCK_SIZE - 1u, &readback) ==
          YT2_ERR);

    /* The seven chunks are contiguous and cover the whole file; each record
       count times its record size is the chunk size. */
    for (unsigned index = 0; index < YT2_SAVE_CHUNK_COUNT; index++) {
        const yt2_save_chunk *chunk = &yt2_save_chunks[index];

        CHECK(chunk->offset == covered);
        CHECK(chunk->size == chunk->count * chunk->record);
        CHECK(chunk->name != NULL && chunk->name[0] != '\0');
        covered += chunk->size;
    }
    CHECK(covered == YT2_SAVE_SIZE);
    CHECK(strcmp(yt2_save_chunks[YT2_SAVE_CHUNK_PARTY].name, "party") == 0);
    CHECK(yt2_save_chunks[YT2_SAVE_CHUNK_OBJECTS].record ==
          YT2_SAVE_OBJECT_SIZE);
    CHECK(yt2_save_chunks[YT2_SAVE_CHUNK_LEVELS].count ==
          YT2_SAVE_LEVELS_COUNT);

    free(file);
    return 0;
}
