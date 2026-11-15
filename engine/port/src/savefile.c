#include "yt2/savefile.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/file.h"

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
     "cache"}
};

static int chunks_cover_file(void)
{
    size_t covered = 0;

    for (unsigned int index = 0; index < YT2_SAVE_CHUNK_COUNT; index++) {
        const yt2_save_chunk *chunk = &yt2_save_chunks[index];

        if (chunk->offset != covered || chunk->name == NULL ||
            chunk->record_size == 0u ||
            chunk->size != (size_t)chunk->count * chunk->record_size) {
            return 0;
        }
        covered += chunk->size;
    }
    return covered == YT2_SAVE_SIZE;
}

int yt2_save_load(const char *path, yt2_save *save)
{
    yt2_status status;

    if (path == NULL || save == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(save, 0, sizeof(*save));
    if (!chunks_cover_file()) {
        return YT2_STATUS_ERROR;
    }
    status = yt2_file_load(path, &save->bytes, &save->size);
    if (status != YT2_STATUS_OK) {
        return status;
    }
    if (save->size != YT2_SAVE_SIZE) {
        yt2_save_free(save);
        return YT2_STATUS_ERROR;
    }
    status = yt2_party_parse(save->bytes + YT2_SAVE_PARTY_OFFSET,
                             YT2_SAVE_PARTY_SIZE, &save->party);
    if (status != YT2_STATUS_OK) {
        yt2_save_free(save);
        return status;
    }
    return YT2_STATUS_OK;
}

int yt2_save_start(const yt2_world *world, yt2_save *save)
{
    yt2_status status;

    if (world == NULL || world->bytes == NULL || save == NULL ||
        world->size < YT2_SAVE_TEMPLATE_OFFSET + YT2_SAVE_PARTY_SIZE ||
        !chunks_cover_file()) {
        return YT2_STATUS_ERROR;
    }
    memset(save, 0, sizeof(*save));
    save->bytes = calloc(1u, YT2_SAVE_SIZE);
    if (save->bytes == NULL) {
        return YT2_STATUS_ERROR;
    }
    save->size = YT2_SAVE_SIZE;
    memcpy(save->bytes + YT2_SAVE_PARTY_OFFSET,
           world->bytes + YT2_SAVE_TEMPLATE_OFFSET, YT2_SAVE_PARTY_SIZE);
    /* fn_1B9D0 clears the assignment both sides: v_963B and the
       per-slot 0x800 flags.  Nothing is in the party yet. */
    memset(save->bytes + YT2_SAVE_PARTY_OFFSET + 0x1ECu, 0, 8u);
    for (unsigned int slot = 0; slot < YT2_ROSTER_SLOTS; slot++) {
        uint8_t *flags = save->bytes + YT2_SAVE_PARTY_OFFSET + 0x1F4u +
                         slot * 0x1F4u + 0x15Cu;

        flags[1] = (uint8_t)(flags[1] & ~0x08u);
    }
    status = yt2_party_parse(save->bytes + YT2_SAVE_PARTY_OFFSET,
                             YT2_SAVE_PARTY_SIZE, &save->party);
    if (status != YT2_STATUS_OK) {
        yt2_save_free(save);
        return status;
    }
    return YT2_STATUS_OK;
}

int yt2_save_write(const yt2_save *save, const char *path)
{
    uint8_t *image;
    yt2_status status;

    if (save == NULL || save->bytes == NULL || path == NULL ||
        save->size != YT2_SAVE_SIZE || !chunks_cover_file()) {
        return YT2_STATUS_ERROR;
    }
    image = malloc(save->size);
    if (image == NULL) {
        return YT2_STATUS_ERROR;
    }
    memcpy(image, save->bytes, save->size);
    status = yt2_party_write(image + YT2_SAVE_PARTY_OFFSET,
                             YT2_SAVE_PARTY_SIZE, &save->party);
    if (status == YT2_STATUS_OK) {
        status = yt2_file_save(path, image, save->size);
    }
    free(image);
    return status;
}

void yt2_save_free(yt2_save *save)
{
    if (save == NULL) {
        return;
    }
    yt2_file_free(save->bytes);
    memset(save, 0, sizeof(*save));
}

const uint8_t *yt2_save_chunk_bytes(const yt2_save *save,
                                    yt2_save_chunk_id chunk)
{
    if (save == NULL || save->bytes == NULL ||
        (unsigned int)chunk >= YT2_SAVE_CHUNK_COUNT) {
        return NULL;
    }
    return save->bytes + yt2_save_chunks[chunk].offset;
}
