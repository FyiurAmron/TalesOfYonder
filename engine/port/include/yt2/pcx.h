#ifndef YT2_PCX_H
#define YT2_PCX_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/status.h"

/*
 * Single-plane 8-bit PCX decode: the format the VBE logo ships in
 * (fn_15DC2 reads exactly this: the 128-byte header, RLE scanlines with
 * the 0xC0 run marker, and the 0x0C + 768-byte palette trailer).
 */

#define YT2_PCX_HEADER_SIZE 128u
#define YT2_PCX_PALETTE_SIZE 768u

/*
 * Decode `size` bytes at `bytes`: write W*H indices to `indices` and the
 * 768 8-bit palette bytes to `palette`, dimensions to `width`/`height`.
 * Fails unless the header, the run stream and the trailer are all whole.
 */
int yt2_pcx_decode(const uint8_t *bytes, size_t size, uint8_t *indices,
                   size_t indices_size, uint8_t *palette,
                   unsigned int *width, unsigned int *height);

#endif /* YT2_PCX_H */
