#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "yt2/savefile.h"

int test_savefile(void)
{
    size_t covered = 0;
    const char *path = getenv("YT2_SAVE");

    for (unsigned int index = 0; index < YT2_SAVE_CHUNK_COUNT; index++) {
        const yt2_save_chunk *chunk = &yt2_save_chunks[index];

        CHECK(chunk->offset == covered);
        CHECK(chunk->size == (size_t)chunk->count * chunk->record_size);
        CHECK(chunk->name != NULL && chunk->name[0] != '\0');
        covered += chunk->size;
    }
    CHECK(covered == YT2_SAVE_SIZE);
    CHECK(strcmp(yt2_save_chunks[YT2_SAVE_CHUNK_PARTY].name, "party") == 0);
    CHECK(strcmp(yt2_save_chunks[YT2_SAVE_CHUNK_OBJECTS].name, "objects") == 0);
    CHECK(strcmp(yt2_save_chunks[YT2_SAVE_CHUNK_LEVELS].name, "levels") == 0);

    CHECK(yt2_save_load("/nonexistent/yt2-save", NULL) == YT2_STATUS_ERROR);
    CHECK(yt2_save_chunk_bytes(NULL, YT2_SAVE_CHUNK_PARTY) == NULL);

    if (path != NULL && path[0] != '\0') {
        yt2_save save;
        int found = 0;

        CHECK(yt2_save_load(path, &save) == YT2_STATUS_OK);
        CHECK(save.size == YT2_SAVE_SIZE);
        CHECK(save.bytes != NULL);
        CHECK(yt2_save_chunk_bytes(&save, YT2_SAVE_CHUNK_PARTY) == save.bytes);
        CHECK(yt2_save_chunk_bytes(&save, YT2_SAVE_CHUNK_COUNT) == NULL);
        CHECK(save.party.roster_count >= 1u);
        for (unsigned int index = 0; index < YT2_ROSTER_SLOTS; index++) {
            if (strcmp(save.party.roster[index].name, "CONAN") == 0) {
                found = 1;
                CHECK(save.party.roster[index].class_id ==
                      YT2_CLASS_FIGHTER);
                CHECK(save.party.roster[index].current.attributes.strength ==
                      50u);
                CHECK(save.party.roster[index].maximum.combat.hit_points ==
                      13u);
            }
        }
        CHECK(found);
        yt2_save_free(&save);
        CHECK(save.bytes == NULL && save.size == 0u);
    }

    if (path != NULL && path[0] != '\0') {
        const char *roundtrip = getenv("YT2_SAVE_ROUNDTRIP");
        const char *moved = getenv("YT2_SAVE_MOVED");

        if (roundtrip != NULL && roundtrip[0] != '\0' && moved != NULL &&
            moved[0] != '\0') {
            yt2_save save;
            yt2_save reloaded;

            CHECK(yt2_save_load(path, &save) == YT2_STATUS_OK);
            CHECK(yt2_save_write(&save, roundtrip) == YT2_STATUS_OK);
            CHECK(yt2_save_load(roundtrip, &reloaded) == YT2_STATUS_OK);
            CHECK(reloaded.size == save.size);
            CHECK(memcmp(reloaded.bytes, save.bytes, save.size) == 0);
            yt2_save_free(&reloaded);

            save.party.x = 0x1234u;
            save.party.y = 0x5678u;
            CHECK(yt2_save_write(&save, moved) == YT2_STATUS_OK);
            CHECK(yt2_save_load(moved, &reloaded) == YT2_STATUS_OK);
            CHECK(reloaded.party.x == 0x1234u);
            CHECK(reloaded.party.y == 0x5678u);
            yt2_save_free(&reloaded);
            yt2_save_free(&save);
        }
        CHECK(yt2_save_write(NULL, "/nonexistent/yt2-save") ==
              YT2_STATUS_ERROR);
    }

    {
        /* B1's title-entry reset (fn_0D36C/fn_1B9D0): a re-run wipes
           created characters, restores the WORLD template roster
           (SQUIRE/DIANA/YENDOR in 5/6/7), and clears the assignment. */
        const char *world_path = getenv("YT2_WORLD");
        yt2_world world;
        yt2_save fresh;

        if (world_path != NULL && world_path[0] != '\0') {
            CHECK(yt2_world_load(world_path, &world) == YT2_STATUS_OK);
            CHECK(yt2_save_start(&world, &fresh) == YT2_STATUS_OK);
            CHECK(fresh.party.assigned[0] == 0u);
            CHECK(fresh.party.assigned[1] == 0u);
            CHECK(fresh.party.assigned[2] == 0u);
            CHECK(fresh.party.assigned[3] == 0u);
            CHECK(fresh.party.roster[0].name[0] == '\0');
            CHECK(strcmp(fresh.party.roster[5].name, "SQUIRE") == 0);
            CHECK(strcmp(fresh.party.roster[6].name, "DIANA") == 0);
            CHECK(strcmp(fresh.party.roster[7].name, "YENDOR") == 0);
            /* Simulate a played game: CONAN created + assembled. */
            strcpy(fresh.party.roster[0].name, "CONAN");
            fresh.party.assigned[0] = 1u;
            fresh.party.assigned[1] = 6u;
            /* NEWGAME reset: free + start again (play_title_reset). */
            yt2_save_free(&fresh);
            CHECK(yt2_save_start(&world, &fresh) == YT2_STATUS_OK);
            CHECK(fresh.party.roster[0].name[0] == '\0');
            CHECK(fresh.party.assigned[0] == 0u);
            CHECK(fresh.party.assigned[1] == 0u);
            CHECK(strcmp(fresh.party.roster[5].name, "SQUIRE") == 0);
            CHECK(strcmp(fresh.party.roster[6].name, "DIANA") == 0);
            CHECK(strcmp(fresh.party.roster[7].name, "YENDOR") == 0);
            yt2_save_free(&fresh);
            yt2_world_free(&world);
        }
    }
    return 0;
}
