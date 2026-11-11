#ifndef YT2_CLOCK_H
#define YT2_CLOCK_H

/*
 * Host clocks for the boundary.  Transport timestamps ride the monotonic
 * clock in 8253 PIT units (1,193,182 Hz, the YT2B timebase); game-logic
 * timing stays in explicit tick counts and never reads these.
 */

#include <stdint.h>

/* Monotonic time in PIT units; the origin is arbitrary, only differences go
   on the wire (modular subtraction across the hourly 32-bit wrap). */
uint32_t yt2_pit_now(void);

/* Sleep at least `ms` milliseconds (player pacing). */
void yt2_sleep_ms(unsigned int ms);

#endif /* YT2_CLOCK_H */
