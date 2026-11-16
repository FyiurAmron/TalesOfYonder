#include "test.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "yt2/ingamemenu.h"
#include "yt2/create.h"
#include "yt2/cursor.h"
#include "yt2/items.h"
#include "yt2/party.h"
#include "yt2/pictures.h"
#include "yt2/room.h"
#include "yt2/savefile.h"
#include "yt2/status.h"
#include "yt2/surface.h"
#include "yt2/world.h"

static int load_fixtures(yt2_pictures *pictures, yt2_world *world)
{
    const char *pictures_path = getenv("YT2_PICTURES");
    const char *world_path = getenv("YT2_WORLD");

    if (pictures_path == NULL || world_path == NULL) {
        return 0;
    }
    if (yt2_pictures_load(pictures_path, pictures) != YT2_STATUS_OK) {
        return 0;
    }
    if (yt2_world_load(world_path, world) != YT2_STATUS_OK) {
        yt2_pictures_free(pictures);
        return 0;
    }
    return 1;
}

/* The slot scan: empty dir reads EMPTY, a staged save reads its name. */
static void check_scan(void)
{
    const char *tmp = getenv("YT2_PORT_TMP");
    yt2_ingamemenu_slot slots[YT2_INGAMEMENU_SLOTS];
    char path[1024];
    FILE *stream;

    if (tmp == NULL) {
        fprintf(stderr, "test_ingamemenu: YT2_PORT_TMP missing, skipped\n");
        return;
    }
    snprintf(path, sizeof(path), "%s/ingamemenu-scan-empty", tmp);
    CHECK(yt2_ingamemenu_scan_slots(slots, path) == 0);
    CHECK(strcmp(slots[0].text, "        - EMPTY -       ") == 0);
    CHECK((slots[0].flags & YT2_INGAMEMENU_SLOT_EXISTS) == 0u);

    snprintf(path, sizeof(path), "%s/ingamemenu-scan-full", tmp);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/ingamemenu-scan-full/SAVGAME2", tmp);
    stream = fopen(path, "wb");
    if (stream != NULL) {
        static uint8_t zeros[77509];

        memset(zeros, 0, sizeof(zeros));
        memcpy(zeros, "MY GAME", 7);
        CHECK(fwrite(zeros, 1, sizeof(zeros), stream) == sizeof(zeros));
        fclose(stream);
    }
    snprintf(path, sizeof(path), "%s/ingamemenu-scan-full", tmp);
    CHECK(yt2_ingamemenu_scan_slots(slots, path) != 0);
    CHECK(strcmp(slots[1].text, "MY GAME") == 0);
    CHECK((slots[1].flags & YT2_INGAMEMENU_SLOT_EXISTS) != 0u);
    CHECK((slots[0].flags & YT2_INGAMEMENU_SLOT_EXISTS) == 0u);
}

static void check_gates(void)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_save save;
    yt2_surface surface;
    yt2_ingamemenu ingamemenu;
    yt2_ingamemenu_slot slots[YT2_INGAMEMENU_SLOTS];

    if (!load_fixtures(&pictures, &world)) {
        fprintf(stderr, "test_ingamemenu: fixture files missing, skipped\n");
        return;
    }
    CHECK(yt2_save_start(&world, &save) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    memset(slots, 0, sizeof(slots));

    /* Fresh mask: save/speed/quit/continue work, load/new do not. */
    CHECK(yt2_ingamemenu_begin(&ingamemenu, &save.party, &save, slots, &pictures,
                         &surface, ".", 0x9Du, 0, 0, 0u) ==
          YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'L') == YT2_STATUS_ERROR);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'N') == YT2_STATUS_ERROR);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'S') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'A') == YT2_STATUS_ERROR);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 0x1Bu) == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'R') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_done(&ingamemenu));
    CHECK(yt2_ingamemenu_result(&ingamemenu) == YT2_INGAMEMENU_EXIT_CONTINUE);

    /* Saves-present mask: load/new work, save/speed/quit/continue do not. */
    CHECK(yt2_ingamemenu_begin(&ingamemenu, &save.party, &save, slots, &pictures,
                         &surface, ".", 0x71u, 0, 0, 0u) ==
          YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'R') == YT2_STATUS_ERROR);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'S') == YT2_STATUS_ERROR);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'L') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 0x1Bu) == YT2_STATUS_OK);

    /* Music/sound toggles are dead without the present bits. */
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'M') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'U') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_sfx_count(&ingamemenu) == 0u);

    /* O is always a no-op. */
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'O') == YT2_STATUS_OK);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
}

/* Speed cycles 5 -> 1 -> 9 -> 5 like sub_0F356. */
static void check_speed(void)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_save save;
    yt2_surface surface;
    yt2_ingamemenu ingamemenu;
    yt2_ingamemenu_slot slots[YT2_INGAMEMENU_SLOTS];

    if (!load_fixtures(&pictures, &world)) {
        return;
    }
    CHECK(yt2_save_start(&world, &save) == YT2_STATUS_OK);
    CHECK(yt2_surface_init(&surface, 320u, 200u) == YT2_STATUS_OK);
    memset(slots, 0, sizeof(slots));
    CHECK(yt2_ingamemenu_begin(&ingamemenu, &save.party, &save, slots, &pictures,
                         &surface, ".", 0x9Du, 0, 0, 0u) ==
          YT2_STATUS_OK);
    CHECK(save.party.game_speed == 5u);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'A') == YT2_STATUS_OK);
    CHECK(save.party.game_speed == 1u);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'A') == YT2_STATUS_OK);
    CHECK(save.party.game_speed == 9u);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'A') == YT2_STATUS_OK);
    CHECK(save.party.game_speed == 5u);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
}

/* Load restores every chunk except objects and clears the sound high
   byte (B1 post-load bytes). */
static void check_load_bytes(void)
{
    const char *tmp = getenv("YT2_PORT_TMP");
    yt2_pictures pictures;
    yt2_world world;
    yt2_save save;
    yt2_surface surface;
    yt2_ingamemenu ingamemenu;
    yt2_ingamemenu_slot slots[YT2_INGAMEMENU_SLOTS];
    char dir[1024];
    char path[1088];
    FILE *stream;
    uint8_t *image;

    if (tmp == NULL || !load_fixtures(&pictures, &world)) {
        return;
    }
    snprintf(dir, sizeof(dir), "%s/ingamemenu-load", tmp);
    mkdir(dir, 0755);
    if (yt2_save_start(&world, &save) != YT2_STATUS_OK ||
        yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        return;
    }
    /* Fake slot 1: boot bytes with a name and sound high byte set. */
    image = malloc(save.size);
    CHECK(image != NULL);
    memcpy(image, save.bytes, save.size);
    memcpy(image, "X", 1);
    image[0x87u] = 0x80u;
    image[YT2_SAVE_OBJECTS_OFFSET] = 0xFFu;
    snprintf(path, sizeof(path), "%s/SAVGAME1", dir);
    stream = fopen(path, "wb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK(fwrite(image, 1, save.size, stream) == save.size);
        fclose(stream);
    }
    free(image);
    /* Live CURGAME differs in objects only (explored) plus the name. */
    save.bytes[YT2_SAVE_OBJECTS_OFFSET] = 0x06u;
    CHECK(yt2_ingamemenu_scan_slots(slots, dir) != 0);
    CHECK(yt2_ingamemenu_begin(&ingamemenu, &save.party, &save, slots, &pictures,
                         &surface, dir, 0x71u, 0, 0, 0u) ==
          YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'L') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, '1') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 0x0Du) == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_done(&ingamemenu));
    CHECK(save.bytes[0x87u] == 0x00u);
    CHECK(save.bytes[YT2_SAVE_OBJECTS_OFFSET] == 0x06u);
    CHECK(save.bytes[0] == 'X');

    yt2_surface_free(&surface);
    yt2_save_free(&save);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
}

/* fn_0ABEC on the matrix party: SQUIRE alone means 60/55/61. */
static void check_means(void)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_surface surface;
    yt2_roster roster;

    if (!load_fixtures(&pictures, &world)) {
        return;
    }
    if (yt2_items_load(&items, &world) != YT2_STATUS_OK ||
        yt2_save_start(&world, &save) != YT2_STATUS_OK ||
        yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        return;
    }
    CHECK(yt2_roster_begin(&roster, &save.party, &items, &pictures,
                           &surface) == YT2_STATUS_OK);
    CHECK(yt2_roster_key(&roster, '6') == YT2_STATUS_OK);
    CHECK(yt2_roster_key(&roster, 'D') == YT2_STATUS_OK);
    CHECK(save.party.assigned[0] == 6u);
    yt2_room_update_skill_flags(&save.party);
    CHECK(save.party.mean_mapping == 60u);
    CHECK(save.party.mean_navigation == 55u);
    CHECK(save.party.mean_survival == 61u);

    yt2_surface_free(&surface);
    yt2_save_free(&save);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
}

static uint64_t frame_hash(yt2_surface *surface)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);
    size_t size = (size_t)320u * 200u;

    for (size_t i = 0u; i < size; i++) {
        value ^= surface->pixels[i];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

static uint64_t frame_hash_cursor(yt2_surface *surface,
                                  const yt2_pictures *pictures, int x, int y)
{
    static uint8_t copy[320u * 200u];
    yt2_surface view = *surface;
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    memcpy(copy, surface->pixels, sizeof(copy));
    view.pixels = copy;
    CHECK(yt2_cursor_draw(&view, pictures, 0u, x, y) == YT2_STATUS_OK);
    for (size_t i = 0u; i < sizeof(copy); i++) {
        value ^= copy[i];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

/* Camp screens against B1's frames (cursor at its B1 position; the editor
   hides it). B1 provenance: no-sound link runs, script
   ESC/A/6/D/E/S/1/T/E/S/T/Enter. */
static void check_pixels(void)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_items items;
    yt2_save save;
    yt2_surface surface;
    yt2_ingamemenu ingamemenu;
    yt2_ingamemenu_slot slots[YT2_INGAMEMENU_SLOTS];
    yt2_roster roster;

    if (!load_fixtures(&pictures, &world)) {
        return;
    }
    if (yt2_items_load(&items, &world) != YT2_STATUS_OK ||
        yt2_save_start(&world, &save) != YT2_STATUS_OK ||
        yt2_surface_init(&surface, 320u, 200u) != YT2_STATUS_OK) {
        return;
    }
    CHECK(yt2_roster_begin(&roster, &save.party, &items, &pictures,
                           &surface) == YT2_STATUS_OK);
    CHECK(yt2_roster_key(&roster, '6') == YT2_STATUS_OK);
    CHECK(yt2_roster_key(&roster, 'D') == YT2_STATUS_OK);
    memset(slots, 0, sizeof(slots));
    for (unsigned int i = 0u; i < YT2_INGAMEMENU_SLOTS; i++) {
        slots[i].file_digit = (char)('1' + i);
        memcpy(slots[i].text, "        - EMPTY -       ",
               YT2_INGAMEMENU_NAME_TEXT);
        slots[i].text[YT2_INGAMEMENU_NAME_TEXT] = '\0';
    }
    yt2_surface_clear(&surface, 0u);
    CHECK(yt2_ingamemenu_begin(&ingamemenu, &save.party, &save, slots, &pictures,
                         &surface, ".", 0x9Du, 0, 0, 0u) ==
          YT2_STATUS_OK);
    CHECK(frame_hash_cursor(&surface, &pictures, 230, 180) ==
          UINT64_C(0xC8BBE9E19A39BC70));
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'S') == YT2_STATUS_OK);
    CHECK(frame_hash_cursor(&surface, &pictures, 230, 180) ==
          UINT64_C(0x6E67E19FEF3688FD));
    CHECK(yt2_ingamemenu_key(&ingamemenu, '1') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'T') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'E') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'S') == YT2_STATUS_OK);
    CHECK(frame_hash(&surface) == UINT64_C(0x23A66EC70832733E));
    CHECK(yt2_ingamemenu_key(&ingamemenu, 'T') == YT2_STATUS_OK);
    CHECK(yt2_ingamemenu_key(&ingamemenu, 0x0Du) == YT2_STATUS_OK);
    CHECK(frame_hash_cursor(&surface, &pictures, 230, 180) ==
          UINT64_C(0xF63CA19B33529A4D));

    yt2_surface_free(&surface);
    yt2_save_free(&save);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
}

/* Boot is byte-exact vs B1: template plus the nine fn_1B9D0 bytes. */
static void check_boot_bytes(void)
{
    yt2_pictures pictures;
    yt2_world world;
    yt2_save save;
    const uint8_t *tpl;

    if (!load_fixtures(&pictures, &world)) {
        return;
    }
    if (yt2_save_start(&world, &save) != YT2_STATUS_OK) {
        return;
    }
    tpl = world.bytes + YT2_SAVE_TEMPLATE_OFFSET;
    CHECK(save.bytes[0x21u] == 0x20u);
    CHECK(save.bytes[0x52u] == 0x0Cu);
    CHECK(save.bytes[0x82u] == 0x03u);
    CHECK(save.bytes[0x84u] == 0x03u);
    CHECK(save.bytes[0x87u] == 0x80u);
    for (unsigned int slot = 0; slot < YT2_ROSTER_SLOTS; slot++) {
        CHECK((save.party.roster[slot].flags & 0x800u) == 0u);
    }
    for (unsigned int i = 0u; i < 4u; i++) {
        CHECK(save.party.assigned[i] == 0u);
    }
    for (size_t i = 0u; i < YT2_PARTY_BLOCK_SIZE; i++) {
        int fixed = (i == 0x21u || i == 0x52u || i == 0x82u || i == 0x84u ||
                     i == 0x87u);

        for (unsigned int slot = 0; slot < YT2_ROSTER_SLOTS && !fixed;
             slot++) {
            size_t flag = 0x1F4u + slot * 0x1F4u + 0x15Cu + 1u;

            if (i == flag) {
                fixed = 1;
            }
        }
        if (i >= 0x1ECu && i < 0x1F4u) {
            fixed = 1;
        }
        if (!fixed) {
            CHECK(save.bytes[i] == tpl[i]);
        }
    }

    yt2_save_free(&save);
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
}

int test_ingamemenu(void)
{
    check_scan();
    check_gates();
    check_speed();
    check_means();
    check_load_bytes();
    check_pixels();
    check_boot_bytes();
    return test_failures;
}
