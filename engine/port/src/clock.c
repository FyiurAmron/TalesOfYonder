#define _POSIX_C_SOURCE 199309L

#include "yt2/clock.h"

#include <time.h>

#define YT2_PIT_RATE 1193182u
#define YT2_NSEC_PER_SEC 1000000000ull

uint32_t yt2_pit_now(void)
{
    struct timespec now;
    unsigned long long nsec;

    clock_gettime(CLOCK_MONOTONIC, &now);
    nsec = (unsigned long long)now.tv_sec * YT2_NSEC_PER_SEC +
           (unsigned long long)now.tv_nsec;
    return (uint32_t)((nsec * YT2_PIT_RATE) / YT2_NSEC_PER_SEC);
}

void yt2_sleep_ms(unsigned int ms)
{
    struct timespec wait;
    struct timespec rest;

    wait.tv_sec = (time_t)(ms / 1000u);
    wait.tv_nsec = (long)((ms % 1000u) * 1000000u);
    while (nanosleep(&wait, &rest) != 0) {
        wait = rest;
    }
}
