#include "yt2/memory.h"

uint32_t yt2_mem_address(uint16_t segment, uint16_t offset)
{
    return ((uint32_t)segment << 4) + (uint32_t)offset;
}

void yt2_mem_clear(yt2_memory *memory)
{
    uint32_t index;

    for (index = 0; index < YT2_MEM_SIZE; index++) {
        memory->bytes[index] = 0;
    }
}

uint8_t yt2_mem_read8(const yt2_memory *memory, uint16_t segment, uint16_t offset)
{
    return memory->bytes[yt2_mem_address(segment, offset)];
}

uint16_t yt2_mem_read16(const yt2_memory *memory, uint16_t segment, uint16_t offset)
{
    uint32_t address = yt2_mem_address(segment, offset);

    return (uint16_t)((uint16_t)memory->bytes[address] |
                      ((uint16_t)memory->bytes[address + 1u] << 8));
}

uint32_t yt2_mem_read32(const yt2_memory *memory, uint16_t segment, uint16_t offset)
{
    uint32_t address = yt2_mem_address(segment, offset);

    return (uint32_t)memory->bytes[address] |
           ((uint32_t)memory->bytes[address + 1u] << 8) |
           ((uint32_t)memory->bytes[address + 2u] << 16) |
           ((uint32_t)memory->bytes[address + 3u] << 24);
}

void yt2_mem_write8(yt2_memory *memory, uint16_t segment, uint16_t offset, uint8_t value)
{
    memory->bytes[yt2_mem_address(segment, offset)] = value;
}

void yt2_mem_write16(yt2_memory *memory, uint16_t segment, uint16_t offset, uint16_t value)
{
    uint32_t address = yt2_mem_address(segment, offset);

    memory->bytes[address] = (uint8_t)(value & 0xFFu);
    memory->bytes[address + 1u] = (uint8_t)((value >> 8) & 0xFFu);
}

void yt2_mem_write32(yt2_memory *memory, uint16_t segment, uint16_t offset, uint32_t value)
{
    uint32_t address = yt2_mem_address(segment, offset);

    memory->bytes[address] = (uint8_t)(value & 0xFFu);
    memory->bytes[address + 1u] = (uint8_t)((value >> 8) & 0xFFu);
    memory->bytes[address + 2u] = (uint8_t)((value >> 16) & 0xFFu);
    memory->bytes[address + 3u] = (uint8_t)((value >> 24) & 0xFFu);
}

void yt2_mem_copy_from(const yt2_memory *memory, uint32_t address,
                       uint8_t *destination, uint32_t count)
{
    uint32_t index;

    for (index = 0; index < count; index++) {
        destination[index] = memory->bytes[address + index];
    }
}

void yt2_mem_copy_to(yt2_memory *memory, uint32_t address,
                     const uint8_t *source, uint32_t count)
{
    uint32_t index;

    for (index = 0; index < count; index++) {
        memory->bytes[address + index] = source[index];
    }
}

void yt2_mem_fill(yt2_memory *memory, uint32_t address, uint8_t value, uint32_t count)
{
    uint32_t index;

    for (index = 0; index < count; index++) {
        memory->bytes[address + index] = value;
    }
}
