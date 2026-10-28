#include "yt2/cpu.h"

static unsigned yt2_cpu_parity(uint8_t value)
{
    unsigned bits = 0;
    unsigned index;

    for (index = 0; index < 8; index++) {
        bits += (unsigned)((value >> index) & 1u);
    }
    return (bits & 1u) == 0u;
}

static void yt2_cpu_set_logic_flags(yt2_registers *registers, uint16_t result,
                                    unsigned width)
{
    uint16_t mask = (width == 8u) ? 0x00FFu : 0xFFFFu;
    uint16_t sign = (width == 8u) ? 0x0080u : 0x8000u;
    uint16_t value = (uint16_t)(result & mask);

    registers->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_PF | YT2_FLAG_AF |
                                    YT2_FLAG_ZF | YT2_FLAG_SF | YT2_FLAG_OF);
    if (value == 0u) {
        registers->flags |= YT2_FLAG_ZF;
    }
    if ((value & sign) != 0u) {
        registers->flags |= YT2_FLAG_SF;
    }
    if (yt2_cpu_parity((uint8_t)(value & 0xFFu)) != 0u) {
        registers->flags |= YT2_FLAG_PF;
    }
}

static void yt2_cpu_keep_cf(yt2_registers *registers, uint16_t saved_cf)
{
    registers->flags = (uint16_t)((registers->flags & (uint16_t)~YT2_FLAG_CF) |
                                  (saved_cf & YT2_FLAG_CF));
}

void yt2_cpu_reset(yt2_registers *registers)
{
    registers->ax = 0;
    registers->bx = 0;
    registers->cx = 0;
    registers->dx = 0;
    registers->si = 0;
    registers->di = 0;
    registers->bp = 0;
    registers->sp = 0;
    registers->cs = 0;
    registers->ds = 0;
    registers->es = 0;
    registers->ss = 0;
    registers->ip = 0;
    registers->flags = 0;
}

uint8_t yt2_cpu_lo8(uint16_t value)
{
    return (uint8_t)(value & 0xFFu);
}

uint8_t yt2_cpu_hi8(uint16_t value)
{
    return (uint8_t)((value >> 8) & 0xFFu);
}

uint16_t yt2_cpu_make16(uint8_t high, uint8_t low)
{
    return (uint16_t)(((uint16_t)high << 8) | (uint16_t)low);
}

int yt2_cpu_get_flag(const yt2_registers *registers, uint16_t flag)
{
    return (registers->flags & flag) != 0u;
}

void yt2_cpu_set_flag(yt2_registers *registers, uint16_t flag, int value)
{
    if (value != 0) {
        registers->flags |= flag;
    } else {
        registers->flags &= (uint16_t)~flag;
    }
}

uint8_t yt2_cpu_add8(yt2_registers *registers, uint8_t left, uint8_t right,
                     unsigned carry)
{
    unsigned sum = (unsigned)left + (unsigned)right + (carry != 0u ? 1u : 0u);
    uint8_t result = (uint8_t)(sum & 0xFFu);

    registers->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_PF | YT2_FLAG_AF |
                                    YT2_FLAG_ZF | YT2_FLAG_SF | YT2_FLAG_OF);
    if (sum > 0xFFu) {
        registers->flags |= YT2_FLAG_CF;
    }
    if (((left ^ right ^ result) & 0x10u) != 0u) {
        registers->flags |= YT2_FLAG_AF;
    }
    if (((left ^ result) & (right ^ result) & 0x80u) != 0u) {
        registers->flags |= YT2_FLAG_OF;
    }
    if (result == 0u) {
        registers->flags |= YT2_FLAG_ZF;
    }
    if ((result & 0x80u) != 0u) {
        registers->flags |= YT2_FLAG_SF;
    }
    if (yt2_cpu_parity(result) != 0u) {
        registers->flags |= YT2_FLAG_PF;
    }
    return result;
}

uint16_t yt2_cpu_add16(yt2_registers *registers, uint16_t left, uint16_t right,
                       unsigned carry)
{
    uint32_t sum = (uint32_t)left + (uint32_t)right +
                   (carry != 0u ? 1u : 0u);
    uint16_t result = (uint16_t)(sum & 0xFFFFu);

    registers->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_PF | YT2_FLAG_AF |
                                    YT2_FLAG_ZF | YT2_FLAG_SF | YT2_FLAG_OF);
    if (sum > 0xFFFFu) {
        registers->flags |= YT2_FLAG_CF;
    }
    if (((left ^ right ^ result) & 0x10u) != 0u) {
        registers->flags |= YT2_FLAG_AF;
    }
    if (((left ^ result) & (right ^ result) & 0x8000u) != 0u) {
        registers->flags |= YT2_FLAG_OF;
    }
    if (result == 0u) {
        registers->flags |= YT2_FLAG_ZF;
    }
    if ((result & 0x8000u) != 0u) {
        registers->flags |= YT2_FLAG_SF;
    }
    if (yt2_cpu_parity((uint8_t)(result & 0xFFu)) != 0u) {
        registers->flags |= YT2_FLAG_PF;
    }
    return result;
}

uint8_t yt2_cpu_sub8(yt2_registers *registers, uint8_t left, uint8_t right,
                     unsigned borrow)
{
    unsigned amount = (unsigned)right + (borrow != 0u ? 1u : 0u);
    unsigned difference = (unsigned)left - amount;
    uint8_t result = (uint8_t)(difference & 0xFFu);

    registers->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_PF | YT2_FLAG_AF |
                                    YT2_FLAG_ZF | YT2_FLAG_SF | YT2_FLAG_OF);
    if ((unsigned)left < amount) {
        registers->flags |= YT2_FLAG_CF;
    }
    if (((left ^ right ^ result) & 0x10u) != 0u) {
        registers->flags |= YT2_FLAG_AF;
    }
    if (((left ^ right) & (left ^ result) & 0x80u) != 0u) {
        registers->flags |= YT2_FLAG_OF;
    }
    if (result == 0u) {
        registers->flags |= YT2_FLAG_ZF;
    }
    if ((result & 0x80u) != 0u) {
        registers->flags |= YT2_FLAG_SF;
    }
    if (yt2_cpu_parity(result) != 0u) {
        registers->flags |= YT2_FLAG_PF;
    }
    return result;
}

uint16_t yt2_cpu_sub16(yt2_registers *registers, uint16_t left, uint16_t right,
                       unsigned borrow)
{
    uint32_t amount = (uint32_t)right + (borrow != 0u ? 1u : 0u);
    uint32_t difference = (uint32_t)left - amount;
    uint16_t result = (uint16_t)(difference & 0xFFFFu);

    registers->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_PF | YT2_FLAG_AF |
                                    YT2_FLAG_ZF | YT2_FLAG_SF | YT2_FLAG_OF);
    if ((uint32_t)left < amount) {
        registers->flags |= YT2_FLAG_CF;
    }
    if (((left ^ right ^ result) & 0x10u) != 0u) {
        registers->flags |= YT2_FLAG_AF;
    }
    if (((left ^ right) & (left ^ result) & 0x8000u) != 0u) {
        registers->flags |= YT2_FLAG_OF;
    }
    if (result == 0u) {
        registers->flags |= YT2_FLAG_ZF;
    }
    if ((result & 0x8000u) != 0u) {
        registers->flags |= YT2_FLAG_SF;
    }
    if (yt2_cpu_parity((uint8_t)(result & 0xFFu)) != 0u) {
        registers->flags |= YT2_FLAG_PF;
    }
    return result;
}

uint8_t yt2_cpu_inc8(yt2_registers *registers, uint8_t value)
{
    uint16_t saved_cf = registers->flags & YT2_FLAG_CF;
    uint8_t result = yt2_cpu_add8(registers, value, 1u, 0u);

    yt2_cpu_keep_cf(registers, saved_cf);
    return result;
}

uint16_t yt2_cpu_inc16(yt2_registers *registers, uint16_t value)
{
    uint16_t saved_cf = registers->flags & YT2_FLAG_CF;
    uint16_t result = yt2_cpu_add16(registers, value, 1u, 0u);

    yt2_cpu_keep_cf(registers, saved_cf);
    return result;
}

uint8_t yt2_cpu_dec8(yt2_registers *registers, uint8_t value)
{
    uint16_t saved_cf = registers->flags & YT2_FLAG_CF;
    uint8_t result = yt2_cpu_sub8(registers, value, 1u, 0u);

    yt2_cpu_keep_cf(registers, saved_cf);
    return result;
}

uint16_t yt2_cpu_dec16(yt2_registers *registers, uint16_t value)
{
    uint16_t saved_cf = registers->flags & YT2_FLAG_CF;
    uint16_t result = yt2_cpu_sub16(registers, value, 1u, 0u);

    yt2_cpu_keep_cf(registers, saved_cf);
    return result;
}

uint8_t yt2_cpu_and8(yt2_registers *registers, uint8_t left, uint8_t right)
{
    uint8_t result = (uint8_t)(left & right);

    yt2_cpu_set_logic_flags(registers, result, 8u);
    return result;
}

uint16_t yt2_cpu_and16(yt2_registers *registers, uint16_t left, uint16_t right)
{
    uint16_t result = (uint16_t)(left & right);

    yt2_cpu_set_logic_flags(registers, result, 16u);
    return result;
}

uint8_t yt2_cpu_or8(yt2_registers *registers, uint8_t left, uint8_t right)
{
    uint8_t result = (uint8_t)(left | right);

    yt2_cpu_set_logic_flags(registers, result, 8u);
    return result;
}

uint16_t yt2_cpu_or16(yt2_registers *registers, uint16_t left, uint16_t right)
{
    uint16_t result = (uint16_t)(left | right);

    yt2_cpu_set_logic_flags(registers, result, 16u);
    return result;
}

uint8_t yt2_cpu_xor8(yt2_registers *registers, uint8_t left, uint8_t right)
{
    uint8_t result = (uint8_t)(left ^ right);

    yt2_cpu_set_logic_flags(registers, result, 8u);
    return result;
}

uint16_t yt2_cpu_xor16(yt2_registers *registers, uint16_t left, uint16_t right)
{
    uint16_t result = (uint16_t)(left ^ right);

    yt2_cpu_set_logic_flags(registers, result, 16u);
    return result;
}

uint8_t yt2_cpu_test8(yt2_registers *registers, uint8_t left, uint8_t right)
{
    uint8_t result = (uint8_t)(left & right);

    yt2_cpu_set_logic_flags(registers, result, 8u);
    return result;
}

uint16_t yt2_cpu_test16(yt2_registers *registers, uint16_t left, uint16_t right)
{
    uint16_t result = (uint16_t)(left & right);

    yt2_cpu_set_logic_flags(registers, result, 16u);
    return result;
}

uint8_t yt2_cpu_neg8(yt2_registers *registers, uint8_t value)
{
    return yt2_cpu_sub8(registers, 0u, value, 0u);
}

uint16_t yt2_cpu_neg16(yt2_registers *registers, uint16_t value)
{
    return yt2_cpu_sub16(registers, 0u, value, 0u);
}

int yt2_cpu_condition(const yt2_registers *registers, unsigned condition)
{
    int cf = yt2_cpu_get_flag(registers, YT2_FLAG_CF);
    int pf = yt2_cpu_get_flag(registers, YT2_FLAG_PF);
    int zf = yt2_cpu_get_flag(registers, YT2_FLAG_ZF);
    int sf = yt2_cpu_get_flag(registers, YT2_FLAG_SF);
    int of = yt2_cpu_get_flag(registers, YT2_FLAG_OF);

    switch (condition & 0x0Fu) {
    case 0x0u: return of;
    case 0x1u: return !of;
    case 0x2u: return cf;
    case 0x3u: return !cf;
    case 0x4u: return zf;
    case 0x5u: return !zf;
    case 0x6u: return cf || zf;
    case 0x7u: return !cf && !zf;
    case 0x8u: return sf;
    case 0x9u: return !sf;
    case 0xAu: return pf;
    case 0xBu: return !pf;
    case 0xCu: return sf != of;
    case 0xDu: return sf == of;
    case 0xEu: return zf || sf != of;
    case 0xFu: return !zf && sf == of;
    default: return 0;
    }
}
