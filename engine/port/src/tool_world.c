#include "yt2/world.h"

#include <stdio.h>
#include <string.h>

static void print_trimmed(const char *text)
{
    size_t length = strlen(text);

    while (length > 0u && text[length - 1u] == ' ') {
        length--;
    }
    printf("%.*s", (int)length, text);
}

int main(int argc, char **argv)
{
    yt2_world world;
    unsigned int filler = 0;
    unsigned int world_maps = 0;
    unsigned int dungeon_maps = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s WORLD.DAT\n", argv[0]);
        return 2;
    }
    if (yt2_world_load(argv[1], &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-world: cannot load %s\n", argv[1]);
        return 1;
    }

    for (unsigned int index = 0; index < YT2_WORLD_MAPS_COUNT; index++) {
        switch (world.maps[index].type) {
        case 0:
            filler++;
            break;
        case 1:
            world_maps++;
            break;
        case 2:
            dungeon_maps++;
            break;
        }
    }

    printf("world: %zu bytes\n", world.size);
    printf("tiles: %u (%u x %u)\n", (unsigned)YT2_WORLD_TILES_COUNT,
           (unsigned)YT2_WORLD_TILES_X, (unsigned)YT2_WORLD_TILES_Y);
    printf("maps: %u (filler %u, world %u, dungeon %u)\n",
           (unsigned)YT2_WORLD_MAPS_COUNT, filler, world_maps, dungeon_maps);
    printf("names: %u\n", (unsigned)YT2_WORLD_MAP_NAMES_COUNT);

    for (unsigned int index = 0; index < YT2_WORLD_MAPS_COUNT; index++) {
        const yt2_map *map = &world.maps[index];

        if (map->type == 0u) {
            continue;
        }
        printf("map %u: suffix \"%.4s\" name %u type %u \"",
               index, map->suffix, (unsigned)map->name_index,
               (unsigned)map->type);
        print_trimmed(yt2_world_map_name(&world, map));
        printf("\"\n");
    }

    for (unsigned int index = 0; index < YT2_WORLD_MAP_NAMES_COUNT; index++) {
        printf("name %u: \"", index);
        print_trimmed(world.names[index]);
        printf("\"\n");
    }

    yt2_world_free(&world);
    return 0;
}
