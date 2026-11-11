#ifndef YT2_ENDIAN_H
#define YT2_ENDIAN_H

#include <stdint.h>

/*
 * The original's files are little-endian.  B2 reads and writes fields
 * explicitly so host byte order never matters and the code stays portable to
 * the C# rewrite.
 */
static inline uint16_t yt2_read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static inline uint32_t yt2_read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static inline void yt2_write_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static inline void yt2_write_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFu);
}

#endif /* YT2_ENDIAN_H */
