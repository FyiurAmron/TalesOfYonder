#include "yt2/data.h"
#include "yt2/platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Read a CURGAME / SAVEGAMEn file through the typed data model and print the
 * party state.  This is the executable check that the structures in
 * yt2/data.h match the game's serialization: tests/c/saveload.sh runs it on
 * the save the game wrote through its disk panel.
 */

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s SAVEGAME\n", program);
}

static int read_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    long length;
    uint8_t *buffer;

    if (stream == NULL) {
        fprintf(stderr, "yt2saveinfo: cannot open %s: %s\n", path,
                strerror(errno));
        return 0;
    }
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) < 0 ||
        fseek(stream, 0, SEEK_SET) != 0) {
        fprintf(stderr, "yt2saveinfo: cannot size %s\n", path);
        fclose(stream);
        return 0;
    }
    if ((unsigned long)length != YT2_SAVE_SIZE) {
        fprintf(stderr, "yt2saveinfo: %s is %ld bytes, expected %u\n", path,
                length, (unsigned)YT2_SAVE_SIZE);
        fclose(stream);
        return 0;
    }
    buffer = malloc((size_t)length);
    if (buffer == NULL) {
        fprintf(stderr, "yt2saveinfo: out of memory\n");
        fclose(stream);
        return 0;
    }
    if (fread(buffer, 1, (size_t)length, stream) != (size_t)length) {
        fprintf(stderr, "yt2saveinfo: cannot read %s\n", path);
        free(buffer);
        fclose(stream);
        return 0;
    }
    fclose(stream);
    *data = buffer;
    *size = (size_t)length;
    return 1;
}

static void print_stats(const char *label, const yt2_party_stats *stats)
{
    printf("  %s: str %u dex %u sta %u int %u wis %u cha %u\n", label,
           (unsigned)stats->strength, (unsigned)stats->dexterity,
           (unsigned)stats->stamina, (unsigned)stats->intelligence,
           (unsigned)stats->wisdom, (unsigned)stats->charisma);
    printf("    proj %u/%u battle %u/%u absorb %u hp %u mp %u carry %u\n",
           (unsigned)stats->projectile_accuracy,
           (unsigned)stats->projectile_damage,
           (unsigned)stats->battle_accuracy, (unsigned)stats->battle_damage,
           (unsigned)stats->absorption, (unsigned)stats->hit_points,
           (unsigned)stats->magic_points, (unsigned)stats->carry_capacity);
    printf("    skills survival %u projectile %u slashing %u bashing %u "
           "polearm %u\n",
           (unsigned)stats->survival, (unsigned)stats->projectile_skill,
           (unsigned)stats->slashing, (unsigned)stats->bashing,
           (unsigned)stats->polearm);
    printf("    casting %u mapping %u navigation %u bartering %u repair %u "
           "thievery %u linguistics %u chemistry %u\n",
           (unsigned)stats->casting, (unsigned)stats->mapping,
           (unsigned)stats->navigation, (unsigned)stats->bartering,
           (unsigned)stats->repair, (unsigned)stats->thievery,
           (unsigned)stats->linguistics, (unsigned)stats->chemistry);
}

static void print_slot(const yt2_party_slot *slot)
{
    printf("  gear: carried %u flags %04X\n", (unsigned)slot->carried_weight,
           (unsigned)slot->flags);
    for (unsigned index = 0; index < YT2_PARTY_PACK_ITEMS; index++) {
        if (slot->pack[index].id != 0u) {
            printf("    pack %u: %04X x %04X\n", index,
                   (unsigned)slot->pack[index].id,
                   (unsigned)slot->pack[index].count);
        }
    }
    for (unsigned index = 0; index < YT2_PARTY_EQUIPMENT_ITEMS; index++) {
        if (slot->equipment[index].id != 0u) {
            printf("    equipment %u: %04X x %04X\n", index,
                   (unsigned)slot->equipment[index].id,
                   (unsigned)slot->equipment[index].count);
        }
    }
    for (unsigned index = 0; index < YT2_PARTY_EFFECTS; index++) {
        if (slot->effects[index] != 0u) {
            printf("    effect %u: %u\n", index,
                   (unsigned)slot->effects[index]);
        }
    }
}

int main(int argc, char **argv)
{
    uint8_t *data = NULL;
    size_t size = 0;
    yt2_party_block party;
    const yt2_party_header *header = &party.header;

    yt2_platform_console_binary();
    if (argc != 2) {
        usage(argv[0]);
        return 2;
    }
    if (!read_file(argv[1], &data, &size)) {
        return 1;
    }

    printf("save: %u bytes, %u chunks\n", (unsigned)size,
           (unsigned)YT2_SAVE_CHUNK_COUNT);
    {
        uint32_t covered = 0;

        for (unsigned index = 0; index < YT2_SAVE_CHUNK_COUNT; index++) {
            const yt2_save_chunk *chunk = &yt2_save_chunks[index];

            if (chunk->offset != covered) {
                fprintf(stderr, "yt2saveinfo: chunk %u starts at %X, "
                        "expected %X\n", index, (unsigned)chunk->offset,
                        (unsigned)covered);
                free(data);
                return 1;
            }
            printf("chunk %u %-12s %05X..%05X (%u bytes, %u x %u)\n",
                   index, chunk->name, (unsigned)chunk->offset,
                   (unsigned)(chunk->offset + chunk->size),
                   (unsigned)chunk->size, (unsigned)chunk->count,
                   (unsigned)chunk->record);
            covered += chunk->size;
        }
        if (covered != YT2_SAVE_SIZE) {
            fprintf(stderr, "yt2saveinfo: chunks cover %X bytes, "
                    "expected %X\n", (unsigned)covered,
                    (unsigned)YT2_SAVE_SIZE);
            free(data);
            return 1;
        }
    }

    if (yt2_party_block_parse(data + YT2_SAVE_PARTY_OFFSET,
                              size - YT2_SAVE_PARTY_OFFSET, &party) != YT2_OK) {
        fprintf(stderr, "yt2saveinfo: no MITHWARE PARTY signature\n");
        free(data);
        return 1;
    }
    printf("party: position %04X:%04X facing %04X speed %u scene %04X\n",
           (unsigned)header->x, (unsigned)header->y,
           (unsigned)header->facing, (unsigned)header->game_speed,
           (unsigned)header->scene);
    for (unsigned index = 0; index < YT2_PARTY_SLOTS; index++) {
        const yt2_party_slot *slot = &party.slots[index];

        if (slot->name[0] == '\0') {
            continue;
        }
        printf("slot %u: %.14s class %u sex %u pic %u idx %u level %u "
               "weight %u status %04X\n",
               index, slot->name, (unsigned)slot->class_id,
               (unsigned)slot->sex, (unsigned)slot->portrait_pic,
               (unsigned)slot->portrait_index,
               (unsigned)slot->level, (unsigned)slot->weight,
               (unsigned)slot->status);
        print_stats("current", &slot->current);
        print_stats("maximum", &slot->maximum);
        print_slot(slot);
    }

    free(data);
    return 0;
}
