#include "test.h"

#include "yt2/memory.h"

int test_memory(void)
{
    static yt2_memory memory;
    uint8_t block[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    yt2_mem_clear(&memory);
    CHECK(yt2_mem_address(0x0000u, 0x0000u) == 0x00000u);
    CHECK(yt2_mem_address(0x0040u, 0x001Au) == 0x0041Au);
    CHECK(yt2_mem_address(0xFFFFu, 0xFFFFu) == 0x10FFEFu);

    yt2_mem_write8(&memory, 0x0040u, 0x001Au, 0x5Au);
    CHECK(yt2_mem_read8(&memory, 0x0040u, 0x001Au) == 0x5Au);

    yt2_mem_write16(&memory, 0x1234u, 0x5678u, 0xBEEFu);
    CHECK(yt2_mem_read8(&memory, 0x1234u, 0x5678u) == 0xEFu);
    CHECK(yt2_mem_read8(&memory, 0x1234u, 0x5679u) == 0xBEu);
    CHECK(yt2_mem_read16(&memory, 0x1234u, 0x5678u) == 0xBEEFu);

    yt2_mem_write32(&memory, 0x0000u, 0x0010u, 0x12345678u);
    CHECK(yt2_mem_read8(&memory, 0x0000u, 0x0010u) == 0x78u);
    CHECK(yt2_mem_read8(&memory, 0x0000u, 0x0011u) == 0x56u);
    CHECK(yt2_mem_read8(&memory, 0x0000u, 0x0012u) == 0x34u);
    CHECK(yt2_mem_read8(&memory, 0x0000u, 0x0013u) == 0x12u);
    CHECK(yt2_mem_read32(&memory, 0x0000u, 0x0010u) == 0x12345678u);

    yt2_mem_copy_to(&memory, 0x100u, block, 8);
    CHECK(yt2_mem_read8(&memory, 0x0010u, 0x0000u) == 1u);
    CHECK(yt2_mem_read8(&memory, 0x0010u, 0x0007u) == 8u);
    yt2_mem_fill(&memory, 0x100u, 0xFFu, 4);
    CHECK(yt2_mem_read32(&memory, 0x0010u, 0x0000u) == 0xFFFFFFFFu);
    yt2_mem_copy_from(&memory, 0x100u, block, 4);
    CHECK(block[0] == 0xFFu && block[3] == 0xFFu);

    /* The 64 KiB above 1 MiB is addressable; the flat array covers it. */
    yt2_mem_write8(&memory, 0xFFFFu, 0xFFFFu, 0xA5u);
    CHECK(memory.bytes[0x10FFEFu] == 0xA5u);
    return 0;
}
