#ifndef YT2_CPU_H
#define YT2_CPU_H

#include "yt2/core.h"

/*
 * Explicit 286 register state and the ALU/flag rules the emitted game code
 * needs.  There is no interpreter here: the transliterated functions do the
 * arithmetic themselves, and these helpers give them the exact 16-bit
 * semantics (wrap casts, CF/AF/OF/PF/SF/ZF) in one place.
 */

#define YT2_FLAG_CF 0x0001u
#define YT2_FLAG_PF 0x0004u
#define YT2_FLAG_AF 0x0010u
#define YT2_FLAG_ZF 0x0040u
#define YT2_FLAG_SF 0x0080u
#define YT2_FLAG_TF 0x0100u
#define YT2_FLAG_IF 0x0200u
#define YT2_FLAG_DF 0x0400u
#define YT2_FLAG_OF 0x0800u

typedef struct yt2_registers {
    uint16_t ax;
    uint16_t bx;
    uint16_t cx;
    uint16_t dx;
    uint16_t si;
    uint16_t di;
    uint16_t bp;
    uint16_t sp;
    uint16_t cs;
    uint16_t ds;
    uint16_t es;
    uint16_t ss;
    uint16_t ip;
    uint16_t flags;
} yt2_registers;

void yt2_cpu_reset(yt2_registers *registers);

uint8_t yt2_cpu_lo8(uint16_t value);
uint8_t yt2_cpu_hi8(uint16_t value);
uint16_t yt2_cpu_make16(uint8_t high, uint8_t low);

int yt2_cpu_get_flag(const yt2_registers *registers, uint16_t flag);
void yt2_cpu_set_flag(yt2_registers *registers, uint16_t flag, int value);

uint8_t yt2_cpu_add8(yt2_registers *registers, uint8_t left, uint8_t right,
                     unsigned carry);
uint16_t yt2_cpu_add16(yt2_registers *registers, uint16_t left, uint16_t right,
                       unsigned carry);
uint8_t yt2_cpu_sub8(yt2_registers *registers, uint8_t left, uint8_t right,
                     unsigned borrow);
uint16_t yt2_cpu_sub16(yt2_registers *registers, uint16_t left, uint16_t right,
                       unsigned borrow);
uint8_t yt2_cpu_inc8(yt2_registers *registers, uint8_t value);
uint16_t yt2_cpu_inc16(yt2_registers *registers, uint16_t value);
uint8_t yt2_cpu_dec8(yt2_registers *registers, uint8_t value);
uint16_t yt2_cpu_dec16(yt2_registers *registers, uint16_t value);

uint8_t yt2_cpu_and8(yt2_registers *registers, uint8_t left, uint8_t right);
uint16_t yt2_cpu_and16(yt2_registers *registers, uint16_t left, uint16_t right);
uint8_t yt2_cpu_or8(yt2_registers *registers, uint8_t left, uint8_t right);
uint16_t yt2_cpu_or16(yt2_registers *registers, uint16_t left, uint16_t right);
uint8_t yt2_cpu_xor8(yt2_registers *registers, uint8_t left, uint8_t right);
uint16_t yt2_cpu_xor16(yt2_registers *registers, uint16_t left, uint16_t right);
uint8_t yt2_cpu_test8(yt2_registers *registers, uint8_t left, uint8_t right);
uint16_t yt2_cpu_test16(yt2_registers *registers, uint16_t left, uint16_t right);

uint8_t yt2_cpu_neg8(yt2_registers *registers, uint8_t value);
uint16_t yt2_cpu_neg16(yt2_registers *registers, uint16_t value);

int yt2_cpu_condition(const yt2_registers *registers, unsigned condition);

#endif /* YT2_CPU_H */
