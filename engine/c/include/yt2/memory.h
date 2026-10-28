#ifndef YT2_MEMORY_H
#define YT2_MEMORY_H

#include "yt2/core.h"

/*
 * One flat byte array stands in for the whole 16-bit address space:
 * seg:off becomes seg*16+off, so VGA RAM (0xA0000), the BIOS data area and
 * the EMS window all keep their original addresses.  Accesses are unchecked
 * on purpose: the original code is the authority on what it touches, and a
 * C port that silently clamps would hide translation bugs.
 */

typedef struct yt2_memory {
    uint8_t bytes[YT2_MEM_SIZE];
} yt2_memory;

uint32_t yt2_mem_address(uint16_t segment, uint16_t offset);
void yt2_mem_clear(yt2_memory *memory);

uint8_t yt2_mem_read8(const yt2_memory *memory, uint16_t segment, uint16_t offset);
uint16_t yt2_mem_read16(const yt2_memory *memory, uint16_t segment, uint16_t offset);
uint32_t yt2_mem_read32(const yt2_memory *memory, uint16_t segment, uint16_t offset);

void yt2_mem_write8(yt2_memory *memory, uint16_t segment, uint16_t offset, uint8_t value);
void yt2_mem_write16(yt2_memory *memory, uint16_t segment, uint16_t offset, uint16_t value);
void yt2_mem_write32(yt2_memory *memory, uint16_t segment, uint16_t offset, uint32_t value);

void yt2_mem_copy_from(const yt2_memory *memory, uint32_t address,
                       uint8_t *destination, uint32_t count);
void yt2_mem_copy_to(yt2_memory *memory, uint32_t address,
                     const uint8_t *source, uint32_t count);
void yt2_mem_fill(yt2_memory *memory, uint32_t address, uint8_t value, uint32_t count);

#endif /* YT2_MEMORY_H */
