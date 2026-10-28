#ifndef YT2_CORE_H
#define YT2_CORE_H

#include <stddef.h>
#include <stdint.h>

/*
 * Small helpers shared by the C core.  Everything is explicit-width and
 * little-endian, matching the 16-bit target and the v0 protocol.
 */

#define YT2_OK 0
#define YT2_ERR (-1)

/* Protocol clock: 8253 PIT input frequency, the folder link timebase. */
#define YT2_PIT_RATE 1193182u

/* The emulated 16-bit address space, including the 64 KiB above 1 MiB. */
#define YT2_MEM_SIZE 0x110000u

static inline uint16_t yt2_le16(const uint8_t *source)
{
    return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8));
}

static inline uint32_t yt2_le32(const uint8_t *source)
{
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

static inline void yt2_put_le16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFu);
    destination[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static inline void yt2_put_le32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFu);
    destination[1] = (uint8_t)((value >> 8) & 0xFFu);
    destination[2] = (uint8_t)((value >> 16) & 0xFFu);
    destination[3] = (uint8_t)((value >> 24) & 0xFFu);
}

#endif /* YT2_CORE_H */
