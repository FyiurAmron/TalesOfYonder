#include "yt2/savefile.h"

#include <stdio.h>

static void print_stats(const char *label, const yt2_stats *stats)
{
    printf("  %s: str %u dex %u sta %u int %u wis %u cha %u\n", label,
           (unsigned)stats->attributes.strength,
           (unsigned)stats->attributes.dexterity,
           (unsigned)stats->attributes.stamina,
           (unsigned)stats->attributes.intelligence,
           (unsigned)stats->attributes.wisdom,
           (unsigned)stats->attributes.charisma);
    printf("    proj %u/%u battle %u/%u absorb %u hp %u mp %u carry %u\n",
           (unsigned)stats->combat.projectile_accuracy,
           (unsigned)stats->combat.projectile_damage,
           (unsigned)stats->combat.battle_accuracy,
           (unsigned)stats->combat.battle_damage,
           (unsigned)stats->combat.absorption,
           (unsigned)stats->combat.hit_points,
           (unsigned)stats->combat.magic_points,
           (unsigned)stats->combat.carry_capacity);
    printf("    skills survival %u projectile %u slashing %u bashing %u "
           "polearm %u\n",
           (unsigned)stats->skills.survival,
           (unsigned)stats->skills.projectile,
           (unsigned)stats->skills.slashing,
           (unsigned)stats->skills.bashing,
           (unsigned)stats->skills.polearm);
    printf("    casting %u mapping %u navigation %u bartering %u repair %u "
           "thievery %u linguistics %u chemistry %u\n",
           (unsigned)stats->skills.casting,
           (unsigned)stats->skills.mapping,
           (unsigned)stats->skills.navigation,
           (unsigned)stats->skills.bartering,
           (unsigned)stats->skills.repair,
           (unsigned)stats->skills.thievery,
           (unsigned)stats->skills.linguistics,
           (unsigned)stats->skills.chemistry);
}

static void print_character(const yt2_character *character)
{
    printf("  gear: carried %u flags %04X\n",
           (unsigned)character->carried_weight,
           (unsigned)character->flags);
    for (unsigned int index = 0; index < YT2_PACK_SLOTS; index++) {
        if (character->pack[index].id != 0u) {
            printf("    pack %u: %04X x %04X\n", index,
                   (unsigned)character->pack[index].id,
                   (unsigned)character->pack[index].count);
        }
    }
    for (unsigned int index = 0; index < YT2_EQUIPMENT_SLOTS; index++) {
        if (character->equipment[index].id != 0u) {
            printf("    equipment %u: %04X x %04X\n", index,
                   (unsigned)character->equipment[index].id,
                   (unsigned)character->equipment[index].count);
        }
    }
    for (unsigned int index = 0; index < YT2_EFFECT_SLOTS; index++) {
        if (character->effects[index] != 0u) {
            printf("    effect %u: %u\n", index,
                   (unsigned)character->effects[index]);
        }
    }
}

int main(int argc, char **argv)
{
    yt2_save save;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s SAVEGAME\n", argv[0]);
        return 2;
    }
    if (yt2_save_load(argv[1], &save) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-info: cannot load %s\n", argv[1]);
        return 1;
    }

    printf("save: %u bytes, %u chunks\n", (unsigned)save.size,
           (unsigned)YT2_SAVE_CHUNK_COUNT);
    for (unsigned int index = 0; index < YT2_SAVE_CHUNK_COUNT; index++) {
        const yt2_save_chunk *chunk = &yt2_save_chunks[index];

        printf("chunk %u %-12s %05X..%05X (%u bytes, %u x %u)\n", index,
               chunk->name, (unsigned)chunk->offset,
               (unsigned)(chunk->offset + chunk->size),
               (unsigned)chunk->size, chunk->count, chunk->record_size);
    }
    printf("party: position %04X:%04X facing %04X speed %u scene %04X\n",
           (unsigned)save.party.x, (unsigned)save.party.y,
           (unsigned)save.party.facing, (unsigned)save.party.game_speed,
           (unsigned)save.party.scene);
    for (unsigned int index = 0; index < YT2_ROSTER_SLOTS; index++) {
        const yt2_character *character = &save.party.roster[index];

        if (character->name[0] == '\0') {
            continue;
        }
        printf("slot %u: %.14s class %u sex %u pic %u idx %u level %u "
               "weight %u status %04X\n",
               index, character->name, (unsigned)character->class_id,
               (unsigned)character->sex, (unsigned)character->portrait_pic,
               (unsigned)character->portrait_index,
               (unsigned)character->level, (unsigned)character->weight,
               (unsigned)character->status);
        print_stats("current", &character->current);
        print_stats("maximum", &character->maximum);
        print_character(character);
    }

    yt2_save_free(&save);
    result = 0;
    return result;
}
