#define _POSIX_C_SOURCE 200809L

#include "yt2/platform.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

uint32_t yt2_platform_pit_now(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER frequency;
    static LARGE_INTEGER started;
    LARGE_INTEGER now;
    LONGLONG elapsed;
    LONGLONG ticks;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&started);
    }
    QueryPerformanceCounter(&now);
    elapsed = now.QuadPart - started.QuadPart;
    ticks = (elapsed * (LONGLONG)YT2_PIT_RATE) / frequency.QuadPart;
    return (uint32_t)((uint64_t)ticks & 0xFFFFFFFFu);
#else
    struct timespec now;
    uint64_t seconds;
    uint64_t nanoseconds;
    uint64_t ticks;

    clock_gettime(CLOCK_MONOTONIC, &now);
    seconds = (uint64_t)now.tv_sec;
    nanoseconds = (uint64_t)now.tv_nsec;
    ticks = seconds * (uint64_t)YT2_PIT_RATE +
            (nanoseconds * (uint64_t)YT2_PIT_RATE) / 1000000000u;
    return (uint32_t)(ticks & 0xFFFFFFFFu);
#endif
}

void yt2_platform_sleep_ms(uint32_t milliseconds)
{
#if defined(_WIN32)
    Sleep((DWORD)milliseconds);
#else
    struct timespec delay;

    delay.tv_sec = (time_t)(milliseconds / 1000u);
    delay.tv_nsec = (long)((milliseconds % 1000u) * 1000000u);
    nanosleep(&delay, NULL);
#endif
}
