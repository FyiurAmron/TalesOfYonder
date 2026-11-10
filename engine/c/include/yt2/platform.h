#ifndef YT2_PLATFORM_H
#define YT2_PLATFORM_H

#include "yt2/core.h"

/*
 * Host resources live here and nowhere else: the monotonic PIT clock used by
 * the folder transport and a sleep for the stub's frame pacing.  The engine
 * core never reads a wall clock.
 */

uint32_t yt2_platform_pit_now(void);
void yt2_platform_sleep_ms(uint32_t milliseconds);

/* Windows turns "\n" into CRLF on the text-mode stdio streams.  The engine's
   logs and diagnostics are part of its reproducible output, so the tools put
   the console streams in binary mode; this is a no-op everywhere else. */
void yt2_platform_console_binary(void);

#endif /* YT2_PLATFORM_H */
