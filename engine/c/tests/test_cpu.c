#include "test.h"

#include "yt2/cpu.h"

static void clear_flags(yt2_registers *registers)
{
    registers->flags = 0;
}

int test_cpu(void)
{
    yt2_registers cpu;
    uint8_t value8;
    uint16_t value16;

    yt2_cpu_reset(&cpu);
    CHECK(cpu.ax == 0 && cpu.flags == 0);
    CHECK(yt2_cpu_lo8(0xABCDu) == 0xCDu);
    CHECK(yt2_cpu_hi8(0xABCDu) == 0xABu);
    CHECK(yt2_cpu_make16(0x12u, 0x34u) == 0x1234u);

    /* add8: sign/zero/carry/overflow/auxiliary/parity edges */
    clear_flags(&cpu);
    value8 = yt2_cpu_add8(&cpu, 0x7Fu, 1u, 0u);
    CHECK(value8 == 0x80u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_AF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_PF));

    clear_flags(&cpu);
    value8 = yt2_cpu_add8(&cpu, 0xFFu, 1u, 0u);
    CHECK(value8 == 0x00u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_PF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_AF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));

    clear_flags(&cpu);
    value8 = yt2_cpu_add8(&cpu, 0x80u, 0x80u, 0u);
    CHECK(value8 == 0x00u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));

    clear_flags(&cpu);
    value8 = yt2_cpu_add8(&cpu, 0x0Fu, 1u, 0u);
    CHECK(value8 == 0x10u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_AF));

    /* add16 carry and overflow */
    clear_flags(&cpu);
    value16 = yt2_cpu_add16(&cpu, 0xFFFFu, 1u, 0u);
    CHECK(value16 == 0u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));

    clear_flags(&cpu);
    value16 = yt2_cpu_add16(&cpu, 0x7FFFu, 1u, 0u);
    CHECK(value16 == 0x8000u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));

    /* subtraction, including borrow-in and signed overflow */
    clear_flags(&cpu);
    value8 = yt2_cpu_sub8(&cpu, 0u, 1u, 0u);
    CHECK(value8 == 0xFFu);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_AF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));

    clear_flags(&cpu);
    value8 = yt2_cpu_sub8(&cpu, 0x80u, 1u, 0u);
    CHECK(value8 == 0x7Fu);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));

    clear_flags(&cpu);
    value8 = yt2_cpu_sub8(&cpu, 0x7Fu, 0xFFu, 0u);
    CHECK(value8 == 0x80u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));

    clear_flags(&cpu);
    value16 = yt2_cpu_sub16(&cpu, 0x0000u, 0x0001u, 0u);
    CHECK(value16 == 0xFFFFu);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));

    clear_flags(&cpu);
    value16 = yt2_cpu_sub16(&cpu, 0x8000u, 1u, 0u);
    CHECK(value16 == 0x7FFFu);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));

    /* INC/DEC preserve CF, unlike ADD/SUB. */
    cpu.flags = YT2_FLAG_CF;
    value8 = yt2_cpu_inc8(&cpu, 0xFFu);
    CHECK(value8 == 0u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));

    cpu.flags = YT2_FLAG_CF;
    value8 = yt2_cpu_dec8(&cpu, 0u);
    CHECK(value8 == 0xFFu);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));

    /* logic clears CF/OF/AF and sets SF/ZF/PF from the result */
    cpu.flags = YT2_FLAG_CF | YT2_FLAG_OF | YT2_FLAG_AF;
    value8 = yt2_cpu_xor8(&cpu, 0xFFu, 0xFFu);
    CHECK(value8 == 0u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_PF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_AF));

    clear_flags(&cpu);
    value8 = yt2_cpu_and8(&cpu, 0xF0u, 0x80u);
    CHECK(value8 == 0x80u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_SF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));

    value16 = yt2_cpu_or16(&cpu, 0x0100u, 0x0001u);
    CHECK(value16 == 0x0101u);

    /* NEG is 0 - value; OF is the 0x80/0x8000 case. */
    clear_flags(&cpu);
    value8 = yt2_cpu_neg8(&cpu, 0u);
    CHECK(value8 == 0u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_ZF));
    CHECK(!yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));

    clear_flags(&cpu);
    value8 = yt2_cpu_neg8(&cpu, 0x80u);
    CHECK(value8 == 0x80u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_CF));
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));

    clear_flags(&cpu);
    value16 = yt2_cpu_neg16(&cpu, 0x8000u);
    CHECK(value16 == 0x8000u);
    CHECK(yt2_cpu_get_flag(&cpu, YT2_FLAG_OF));

    /* Conditions used by the transliterated branches. */
    cpu.flags = YT2_FLAG_ZF;
    CHECK(yt2_cpu_condition(&cpu, 0x4u));    /* E */
    CHECK(!yt2_cpu_condition(&cpu, 0x5u));   /* NE */
    CHECK(yt2_cpu_condition(&cpu, 0x3u));    /* AE */
    CHECK(!yt2_cpu_condition(&cpu, 0x2u));   /* B */
    CHECK(yt2_cpu_condition(&cpu, 0xDu));    /* GE (SF == OF) */
    CHECK(!yt2_cpu_condition(&cpu, 0xFu));   /* G (not ZF and SF == OF) */

    cpu.flags = YT2_FLAG_ZF | YT2_FLAG_CF;
    CHECK(yt2_cpu_condition(&cpu, 0x4u));
    CHECK(yt2_cpu_condition(&cpu, 0x6u));    /* BE */
    CHECK(!yt2_cpu_condition(&cpu, 0x7u));   /* A */
    CHECK(!yt2_cpu_condition(&cpu, 0xFu));

    cpu.flags = YT2_FLAG_SF | YT2_FLAG_OF;
    CHECK(!yt2_cpu_condition(&cpu, 0xCu));   /* L */
    CHECK(yt2_cpu_condition(&cpu, 0xDu));

    return 0;
}
