#include "test.h"

int test_failures;

int test_boot(void);
int test_combat(void);
int test_ingamemenu(void);
int test_catalog(void);
int test_classes(void);
int test_cursor(void);
int test_intro(void);
int test_items(void);
int test_mapview(void);
int test_monster_detail(void);
int test_monsters(void);
int test_palette(void);
int test_party(void);
int test_pictures(void);
int test_present(void);
int test_protocol(void);
int test_transport(void);
int test_savefile(void);
int test_spells(void);
int test_surface(void);
int test_world(void);
int test_walkthrough(void);
int test_text(void);
int test_cluebook(void);
int test_create(void);
int test_paperdoll(void);
int test_room(void);

int main(void)
{
    test_boot();
    test_combat();
    test_ingamemenu();
    test_catalog();
    test_classes();
    test_cursor();
    test_intro();
    test_items();
    test_mapview();
    test_monster_detail();
    test_monsters();
    test_palette();
    test_party();
    test_pictures();
    test_present();
    test_protocol();
    test_transport();
    test_savefile();
    test_spells();
    test_surface();
    test_world();
    test_walkthrough();
    test_text();
    test_cluebook();
    test_create();
    test_paperdoll();
    test_room();
    if (test_failures != 0) {
        fprintf(stderr, "yt2p-tests: %d failure(s)\n", test_failures);
        return 1;
    }
    printf("yt2p-tests: OK\n");
    return 0;
}
