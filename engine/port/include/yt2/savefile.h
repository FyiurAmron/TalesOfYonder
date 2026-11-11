#ifndef YT2_SAVEFILE_H
#define YT2_SAVEFILE_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/party.h"
#include "yt2/status.h"

/*
 * CURGAME / SAVEGAMEn container.
 *
 * The save is a fixed sequence of seven blocks.  B2 parses the party block
 * into live state; the other blocks are exposed as typed spans until their
 * own models land.  The container owns the file bytes.
 */

#define YT2_SAVE_PARTY_OFFSET 0x00000u
#define YT2_SAVE_PARTY_SIZE 0x1388u
#define YT2_SAVE_OBJECTS_OFFSET 0x01388u
#define YT2_SAVE_OBJECTS_SIZE 0x3840u
#define YT2_SAVE_OBJECTS_COUNT 144u
#define YT2_SAVE_OBJECT_SIZE 0x64u
#define YT2_SAVE_LEVELS_OFFSET 0x04BC8u
#define YT2_SAVE_LEVELS_SIZE 0xAC20u
#define YT2_SAVE_LEVELS_COUNT 16u
#define YT2_SAVE_LEVEL_SIZE 0xAC2u
#define YT2_SAVE_WORLD_TABLE_OFFSET 0x0F7E8u
#define YT2_SAVE_WORLD_TABLE_SIZE 0x284u
#define YT2_SAVE_WORLD_INDEX_OFFSET 0x0FA6Cu
#define YT2_SAVE_WORLD_INDEX_SIZE 0x260u
#define YT2_SAVE_WORLD_MAP_OFFSET 0x0FCCCu
#define YT2_SAVE_WORLD_MAP_SIZE 0x139u
#define YT2_SAVE_CACHE_OFFSET 0x0FE05u
#define YT2_SAVE_CACHE_SIZE 0x30C0u
#define YT2_SAVE_SIZE 0x12EC5u

typedef enum yt2_save_chunk_id {
    YT2_SAVE_CHUNK_PARTY = 0,
    YT2_SAVE_CHUNK_OBJECTS,
    YT2_SAVE_CHUNK_LEVELS,
    YT2_SAVE_CHUNK_WORLD_TABLE,
    YT2_SAVE_CHUNK_WORLD_INDEX,
    YT2_SAVE_CHUNK_WORLD_MAP,
    YT2_SAVE_CHUNK_CACHE,
    YT2_SAVE_CHUNK_COUNT
} yt2_save_chunk_id;

typedef struct yt2_save_chunk {
    size_t offset;
    size_t size;
    unsigned int count;
    unsigned int record_size;
    const char *name;
} yt2_save_chunk;

extern const yt2_save_chunk yt2_save_chunks[YT2_SAVE_CHUNK_COUNT];

typedef struct yt2_save {
    uint8_t *bytes;             /* owned file image */
    size_t size;
    yt2_party party;
} yt2_save;

int yt2_save_load(const char *path, yt2_save *save);

/*
 * Write the save back: the unmodeled chunks are copied unchanged and the party
 * block is rebuilt from the live state.  Loading and writing an untouched save
 * round-trips byte for byte.
 */
int yt2_save_write(const yt2_save *save, const char *path);
void yt2_save_free(yt2_save *save);

/* The block bytes, or NULL for an invalid id. */
const uint8_t *yt2_save_chunk_bytes(const yt2_save *save,
                                    yt2_save_chunk_id chunk);

#endif /* YT2_SAVEFILE_H */
