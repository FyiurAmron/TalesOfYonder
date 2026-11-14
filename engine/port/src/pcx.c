#include "yt2/pcx.h"

static unsigned int read_le16(const uint8_t *bytes)
{
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

int yt2_pcx_decode(const uint8_t *bytes, size_t size, uint8_t *indices,
                   size_t indices_size, uint8_t *palette,
                   unsigned int *width, unsigned int *height)
{
    unsigned int w;
    unsigned int h;
    unsigned int stride;
    size_t need;
    size_t pos;
    size_t row;

    if (bytes == NULL || indices == NULL || palette == NULL ||
        width == NULL || height == NULL ||
        size < YT2_PCX_HEADER_SIZE + 1u + YT2_PCX_PALETTE_SIZE) {
        return YT2_STATUS_ERROR;
    }
    /* Manufacturer 0x0A, RLE encoding, 8 bits per pixel, one plane. */
    if (bytes[0] != 0x0Au || bytes[2] != 1u || bytes[3] != 8u ||
        bytes[65] != 1u) {
        return YT2_STATUS_ERROR;
    }
    if (read_le16(bytes + 4) != 0u || read_le16(bytes + 6) != 0u) {
        return YT2_STATUS_ERROR;
    }
    w = read_le16(bytes + 8) + 1u;
    h = read_le16(bytes + 10) + 1u;
    stride = read_le16(bytes + 66);
    if (w == 0u || h == 0u || stride < w ||
        (size_t)w > SIZE_MAX / h || (size_t)w * h > indices_size) {
        return YT2_STATUS_ERROR;
    }
    /* The 0x0C palette marker + 768 bytes close the file. */
    if (bytes[size - YT2_PCX_PALETTE_SIZE - 1u] != 0x0Cu) {
        return YT2_STATUS_ERROR;
    }
    need = (size_t)h * stride;
    pos = YT2_PCX_HEADER_SIZE;
    row = 0u;
    while (row < need) {
        unsigned int run = 1u;
        uint8_t value;

        if (pos >= size - YT2_PCX_PALETTE_SIZE - 1u) {
            return YT2_STATUS_ERROR;
        }
        value = bytes[pos++];
        if (value >= 0xC0u) {
            run = (unsigned int)(value & 0x3Fu);
            if (run == 0u || pos >= size - YT2_PCX_PALETTE_SIZE - 1u) {
                return YT2_STATUS_ERROR;
            }
            value = bytes[pos++];
        }
        while (run-- > 0u) {
            unsigned int y;
            unsigned int x;

            if (row >= need) {
                return YT2_STATUS_ERROR;
            }
            y = (unsigned int)(row / stride);
            x = (unsigned int)(row % stride);
            /* Stray padding past the row width never reaches the image. */
            if (x < w) {
                indices[(size_t)y * w + x] = value;
            }
            row++;
        }
    }
    if (row != need) {
        return YT2_STATUS_ERROR;
    }
    for (size_t i = 0; i < YT2_PCX_PALETTE_SIZE; i++) {
        palette[i] = bytes[size - YT2_PCX_PALETTE_SIZE + i];
    }
    *width = w;
    *height = h;
    return YT2_STATUS_OK;
}
