#include "test.h"

int test_failures;

int test_memory(void);
int test_cpu(void);
int test_protocol(void);
int test_service(void);
int test_video(void);
int test_video_rle(void);
int test_video_rle_width(void);
int test_video_scroll(void);

int main(void)
{
    test_memory();
    test_cpu();
    test_protocol();
    test_service();
    test_video();
    test_video_rle();
    test_video_rle_width();
    test_video_scroll();
    if (test_failures != 0) {
        fprintf(stderr, "yt2core-tests: %d failure(s)\n", test_failures);
        return 1;
    }
    printf("yt2core-tests: OK\n");
    return 0;
}
