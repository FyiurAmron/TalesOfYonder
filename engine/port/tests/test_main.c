#include "test.h"

int test_failures;

int test_party(void);
int test_pictures(void);
int test_savefile(void);
int test_surface(void);
int test_world(void);

int main(void)
{
    test_party();
    test_pictures();
    test_savefile();
    test_surface();
    test_world();
    if (test_failures != 0) {
        fprintf(stderr, "yt2p-tests: %d failure(s)\n", test_failures);
        return 1;
    }
    printf("yt2p-tests: OK\n");
    return 0;
}
