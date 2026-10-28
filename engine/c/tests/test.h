#ifndef YT2_TEST_H
#define YT2_TEST_H

#include <stdio.h>

extern int test_failures;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,          \
                    #condition);                                             \
            test_failures++;                                                 \
        }                                                                    \
    } while (0)

#endif /* YT2_TEST_H */
