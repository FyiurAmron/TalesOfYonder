#include "yt2/data.h"

#include <string.h>

#include "yt2/core.h"

int yt2_party_signature_check(const uint8_t *signature)
{
    return signature != NULL &&
           memcmp(signature + 1, "MITHWARE PARTY", 14) == 0 &&
           signature[15] == 0u;
}

int yt2_party_block_parse(const uint8_t *source, size_t size,
                          yt2_party_block *party)
{
    if (source == NULL || party == NULL || size < YT2_PARTY_BLOCK_SIZE ||
        !yt2_party_signature_check(source)) {
        return YT2_ERR;
    }
    memcpy(party, source, sizeof(*party));
    return YT2_OK;
}

int yt2_party_block_write(const yt2_party_block *party, uint8_t *destination,
                          size_t capacity)
{
    if (party == NULL || destination == NULL ||
        capacity < YT2_PARTY_BLOCK_SIZE) {
        return YT2_ERR;
    }
    memcpy(destination, party, sizeof(*party));
    return YT2_OK;
}

/*
 * The seven blocks of CURGAME / SAVEGAMEn, in file order.  The save routine
 * (swreg.asm sub_0F823) rebuilds the party block from DGROUP:944Fh and copies
 * the other six from the current CURGAME file; see docs/structures.md.
 */
const yt2_save_chunk yt2_save_chunks[YT2_SAVE_CHUNK_COUNT] = {
    {YT2_SAVE_PARTY_OFFSET, YT2_SAVE_PARTY_SIZE, 1u, YT2_SAVE_PARTY_SIZE,
     "party"},
    {YT2_SAVE_OBJECTS_OFFSET, YT2_SAVE_OBJECTS_SIZE, YT2_SAVE_OBJECTS_COUNT,
     YT2_SAVE_OBJECT_SIZE, "objects"},
    {YT2_SAVE_LEVELS_OFFSET, YT2_SAVE_LEVELS_SIZE, YT2_SAVE_LEVELS_COUNT,
     YT2_SAVE_LEVEL_SIZE, "levels"},
    {YT2_SAVE_WORLD_TABLE_OFFSET, YT2_SAVE_WORLD_TABLE_SIZE, 1u,
     YT2_SAVE_WORLD_TABLE_SIZE, "world-table"},
    {YT2_SAVE_WORLD_INDEX_OFFSET, YT2_SAVE_WORLD_INDEX_SIZE, 1u,
     YT2_SAVE_WORLD_INDEX_SIZE, "world-index"},
    {YT2_SAVE_WORLD_MAP_OFFSET, YT2_SAVE_WORLD_MAP_SIZE, 1u,
     YT2_SAVE_WORLD_MAP_SIZE, "world-map"},
    {YT2_SAVE_CACHE_OFFSET, YT2_SAVE_CACHE_SIZE, 1u, YT2_SAVE_CACHE_SIZE,
     "cache"},
};
