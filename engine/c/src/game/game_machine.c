#include "yt2/game.h"
#include "game_segments.h"
#include "yt2/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

yt2_machine g_m;

/* ------------------------------------------------------------------------ */
/* Register plumbing                                                         */
/* ------------------------------------------------------------------------ */

uint8_t yt2_r8_get(const yt2_registers *registers, unsigned which)
{
    switch (which) {
    case YT2_R8_AL: return (uint8_t)(registers->ax & 0xFFu);
    case YT2_R8_AH: return (uint8_t)(registers->ax >> 8);
    case YT2_R8_BL: return (uint8_t)(registers->bx & 0xFFu);
    case YT2_R8_BH: return (uint8_t)(registers->bx >> 8);
    case YT2_R8_CL: return (uint8_t)(registers->cx & 0xFFu);
    case YT2_R8_CH: return (uint8_t)(registers->cx >> 8);
    case YT2_R8_DL: return (uint8_t)(registers->dx & 0xFFu);
    case YT2_R8_DH: return (uint8_t)(registers->dx >> 8);
    default: return 0;
    }
}

void yt2_r8_set(yt2_registers *registers, unsigned which, uint8_t value)
{
    switch (which) {
    case YT2_R8_AL:
        registers->ax = (uint16_t)((registers->ax & 0xFF00u) | value);
        break;
    case YT2_R8_AH:
        registers->ax = (uint16_t)((registers->ax & 0x00FFu) | ((uint16_t)value << 8));
        break;
    case YT2_R8_BL:
        registers->bx = (uint16_t)((registers->bx & 0xFF00u) | value);
        break;
    case YT2_R8_BH:
        registers->bx = (uint16_t)((registers->bx & 0x00FFu) | ((uint16_t)value << 8));
        break;
    case YT2_R8_CL:
        registers->cx = (uint16_t)((registers->cx & 0xFF00u) | value);
        break;
    case YT2_R8_CH:
        registers->cx = (uint16_t)((registers->cx & 0x00FFu) | ((uint16_t)value << 8));
        break;
    case YT2_R8_DL:
        registers->dx = (uint16_t)((registers->dx & 0xFF00u) | value);
        break;
    case YT2_R8_DH:
        registers->dx = (uint16_t)((registers->dx & 0x00FFu) | ((uint16_t)value << 8));
        break;
    default:
        break;
    }
}

uint32_t yt2_get_eax(void)
{
    return (g_m.eax_high & 0xFFFF0000u) | g_m.r.ax;
}

uint32_t yt2_get_ebx(void)
{
    return (g_m.ebx_high & 0xFFFF0000u) | g_m.r.bx;
}

uint32_t yt2_get_ecx(void)
{
    return (g_m.ecx_high & 0xFFFF0000u) | g_m.r.cx;
}

uint32_t yt2_get_edx(void)
{
    return (g_m.edx_high & 0xFFFF0000u) | g_m.r.dx;
}

void yt2_set_eax(uint32_t value)
{
    g_m.r.ax = (uint16_t)(value & 0xFFFFu);
    g_m.eax_high = value;
}

void yt2_set_ebx(uint32_t value)
{
    g_m.r.bx = (uint16_t)(value & 0xFFFFu);
    g_m.ebx_high = value;
}

void yt2_set_ecx(uint32_t value)
{
    g_m.r.cx = (uint16_t)(value & 0xFFFFu);
    g_m.ecx_high = value;
}

void yt2_set_edx(uint32_t value)
{
    g_m.r.dx = (uint16_t)(value & 0xFFFFu);
    g_m.edx_high = value;
}

/* ------------------------------------------------------------------------ */
/* Machine control                                                           */
/* ------------------------------------------------------------------------ */

void yt2_go(uint16_t cs, uint16_t ip)
{
    g_m.next_cs = cs;
    g_m.next_ip = ip;
    g_m.next_pending = 1;
}

void yt2_push16(uint16_t value)
{
    g_m.r.sp = (uint16_t)(g_m.r.sp - 2u);
    g_m.mem.bytes[yt2_flat(g_m.r.ss, g_m.r.sp)] = (uint8_t)(value & 0xFFu);
    g_m.mem.bytes[yt2_flat(g_m.r.ss, (uint16_t)(g_m.r.sp + 1u))] = (uint8_t)(value >> 8);
}

uint16_t yt2_pop16(void)
{
    uint16_t value = (uint16_t)((uint16_t)g_m.mem.bytes[yt2_flat(g_m.r.ss, g_m.r.sp)] |
                                ((uint16_t)g_m.mem.bytes[yt2_flat(g_m.r.ss, (uint16_t)(g_m.r.sp + 1u))] << 8));
    g_m.r.sp = (uint16_t)(g_m.r.sp + 2u);
    return value;
}

static void yt2_fail(yt2_machine *machine, const char *what, uint16_t segment,
                     uint16_t offset)
{
    fprintf(stderr, "yt2: %s at %04X:%04X (cs=%04X ip(rt)=%04X ds=%04X es=%04X ss=%04X sp=%04X)\n",
            what, segment, offset, machine->r.cs, machine->r.ip, machine->r.ds,
            machine->r.es, machine->r.ss, machine->r.sp);
    machine->exited = 1;
    machine->exit_code = -1;
}

void yt2_machine_bad_entry(yt2_machine *machine, uint16_t segment, uint16_t offset)
{
    yt2_fail(machine, "bad entry", segment, offset);
}

void yt2_machine_bad_gap(yt2_machine *machine, uint16_t segment, uint16_t from, uint16_t to)
{
    fprintf(stderr, "yt2: data gap entered at %04X:%04X..%04X\n", segment, from, to);
    yt2_fail(machine, "data gap", segment, from);
}

void yt2_machine_bad_jump(yt2_machine *machine, uint16_t segment, uint16_t offset)
{
    yt2_fail(machine, "jump outside the image", segment, offset);
}

void yt2_machine_divide_error(yt2_machine *machine)
{
    fprintf(stderr, "yt2: divide error (ax=%04X dx=%04X bx=%04X cx=%04X)\n",
            machine->r.ax, machine->r.dx, machine->r.bx, machine->r.cx);
    machine->exited = 1;
    machine->exit_code = -1;
}

void yt2_machine_sentinel(yt2_machine *machine, uint16_t offset)
{
    if (offset == YT2_SENTINEL_RESUME) {
        machine->nested_done = 1;
        machine->next_pending = 0;
        return;
    }
    /* YT2_SENTINEL_IRET: behave like a bare IRET stub. */
    {
        uint16_t ip = yt2_pop16();
        uint16_t cs = yt2_pop16();
        uint16_t flags = yt2_pop16();
        machine->r.flags = (uint16_t)(flags | 0x0002u);
        yt2_go(cs, ip);
    }
}

static void yt2_run_nested(yt2_machine *machine)
{
    uint16_t saved_cs = machine->next_cs;
    uint16_t saved_ip = machine->next_ip;
    int saved_pending = machine->next_pending;
    uint16_t saved_reg_cs = machine->r.cs;
    uint16_t saved_reg_ip = machine->r.ip;

    /* Interrupt frame: iret pops IP, CS, FLAGS. */
    yt2_push16(machine->r.flags);
    yt2_push16(YT2_SENTINEL_SEG);
    yt2_push16(YT2_SENTINEL_RESUME);

    machine->nested = 1;
    machine->nested_done = 0;
    machine->next_cs = machine->int1c_cs;
    machine->next_ip = machine->int1c_ip;
    machine->next_pending = 1;

    while (!machine->nested_done && !machine->exited && !machine->stop_requested) {
        uint16_t cs = machine->next_cs;
        uint16_t ip = machine->next_ip;
        if (!machine->next_pending) {
            break;
        }
        machine->next_pending = 0;
        machine->transfers++;
        if (machine->trace_api) {
            fprintf(stderr, "yt2: dispatch %04X:%04X (ax=%04X bx=%04X cx=%04X dx=%04X ds=%04X es=%04X ss=%04X sp=%04X)\n",
                    cs, ip, machine->r.ax, machine->r.bx, machine->r.cx, machine->r.dx,
                    machine->r.ds, machine->r.es, machine->r.ss, machine->r.sp);
        }
        yt2_game_dispatch(cs, ip);
    }

    machine->nested = 0;
    machine->next_cs = saved_cs;
    machine->next_ip = saved_ip;
    machine->next_pending = saved_pending;
    /* The resumed code reads CS-relative data, so restore its code segment. */
    machine->r.cs = saved_reg_cs;
    machine->r.ip = saved_reg_ip;
}

int yt2_machine_call_safe(const yt2_machine *machine)
{
    return !machine->nested && !machine->in_tick && !machine->exited &&
           !machine->stop_requested;
}

void yt2_machine_call(yt2_machine *machine, uint16_t segment, uint16_t offset)
{
    uint16_t saved_cs = machine->next_cs;
    uint16_t saved_ip = machine->next_ip;
    int saved_pending = machine->next_pending;
    uint16_t saved_reg_cs = machine->r.cs;
    uint16_t saved_reg_ip = machine->r.ip;
    uint16_t saved_sp = machine->r.sp;

    if (!yt2_machine_call_safe(machine)) {
        return;
    }

    /* A far call: CS then IP, with the sentinel's resume stub as the return
       address.  No flags word - the game's handler ends with retf. */
    yt2_push16(YT2_SENTINEL_SEG);
    yt2_push16(YT2_SENTINEL_RESUME);

    machine->nested = 1;
    machine->nested_done = 0;
    machine->next_cs = segment;
    machine->next_ip = offset;
    machine->next_pending = 1;

    while (!machine->nested_done && !machine->exited && !machine->stop_requested) {
        uint16_t cs = machine->next_cs;
        uint16_t ip = machine->next_ip;
        if (!machine->next_pending) {
            break;
        }
        machine->next_pending = 0;
        machine->transfers++;
        if (machine->trace_api) {
            fprintf(stderr, "yt2: call %04X:%04X (ax=%04X bx=%04X cx=%04X dx=%04X ds=%04X es=%04X ss=%04X sp=%04X)\n",
                    cs, ip, machine->r.ax, machine->r.bx, machine->r.cx, machine->r.dx,
                    machine->r.ds, machine->r.es, machine->r.ss, machine->r.sp);
        }
        yt2_game_dispatch(cs, ip);
    }

    machine->nested = 0;
    machine->next_cs = saved_cs;
    machine->next_ip = saved_ip;
    machine->next_pending = saved_pending;
    machine->r.sp = saved_sp;
    machine->r.cs = saved_reg_cs;
    machine->r.ip = saved_reg_ip;
}

void yt2_pace_slow(void)
{
    uint64_t target;
    uint32_t elapsed;
    uint32_t now;

    /* Re-arm first: everything below may run inside nested guest code. */
    g_m.pace_at = (uint32_t)(g_m.cycles + 65536u);
    if (!g_m.pace || g_m.in_tick || g_m.nested) {
        return;
    }
    now = yt2_platform_pit_now();
    if (!g_m.pace_started) {
        g_m.pace_started = 1;
        g_m.pace_wall_start = now;
        g_m.pace_cycle_start = g_m.cycles;
        return;
    }
    target = (g_m.cycles - g_m.pace_cycle_start) * (uint64_t)YT2_PIT_RATE /
             ((uint64_t)YT2_CYCLES_PER_MS * 1000u);
    elapsed = now - g_m.pace_wall_start;
    if (target > 0xFFFFFFFFu) {
        /* The 32-bit PIT stamp wrapped; rebase rather than stall. */
        g_m.pace_wall_start = now;
        g_m.pace_cycle_start = g_m.cycles;
        return;
    }
    if (target > (uint64_t)elapsed + 4000u) {
        uint64_t ahead = target - elapsed;
        uint64_t milliseconds = ahead * 1000u / YT2_PIT_RATE;

        if (milliseconds > 50u) {
            milliseconds = 50u;
        }
        if (milliseconds > 0u) {
            yt2_platform_sleep_ms((uint32_t)milliseconds);
        }
    } else if ((uint64_t)elapsed > target + 2u * YT2_PIT_RATE) {
        /* A host stall (debugger, scheduler): do not run at double speed. */
        g_m.pace_wall_start = now;
        g_m.pace_cycle_start = g_m.cycles;
    }
}

void yt2_machine_tick(yt2_machine *machine)
{
    uint32_t count;

    machine->ticks = (machine->ticks + 1u) % 0x1800B0u;
    count = machine->ticks;
    machine->mem.bytes[0x46C] = (uint8_t)(count & 0xFFu);
    machine->mem.bytes[0x46D] = (uint8_t)((count >> 8) & 0xFFu);
    machine->mem.bytes[0x46E] = (uint8_t)((count >> 16) & 0xFFu);
    machine->mem.bytes[0x46F] = (uint8_t)((count >> 24) & 0xFFu);

    if (machine->in_tick || machine->nested) {
        return;
    }
    if (machine->int1c_cs == YT2_SENTINEL_SEG && machine->int1c_ip == YT2_SENTINEL_IRET) {
        return;
    }
    /* The SEAM tick handler presents the screen before the countdowns run. */
    yt2_game_present(machine);
    machine->in_tick = 1;
    yt2_run_nested(machine);
    machine->in_tick = 0;
}

void yt2_cycles_slow(void)
{
    if (g_m.in_tick || g_m.nested) {
        return;
    }
    while (g_m.cycles >= g_m.tick_at && !g_m.exited && !g_m.stop_requested) {
        g_m.tick_at += (uint64_t)YT2_CYCLES_PER_TICK;
        yt2_machine_tick(&g_m);
        if (g_m.limit_ticks != 0u && g_m.ticks >= g_m.limit_ticks) {
            g_m.stop_requested = 1;
        }
    }
    if (g_m.stop_requested && g_m.abort_armed) {
        g_m.abort_armed = 0;
        longjmp(g_m.abort_jmp, 1);
    }
}

int yt2_machine_int(yt2_machine *machine, unsigned number)
{
    switch (number) {
    case 0x21u:
        yt2_dos_int21(machine);
        if (machine->limit_ticks != 0u && machine->ticks >= machine->limit_ticks) {
            machine->stop_requested = 1;
        }
        return (machine->exited || machine->stop_requested) ? 1 : 0;
    case 0x10u:
        yt2_video_int10(machine);
        return 0;
    case 0x1Au:
        yt2_clock_int1a(machine);
        return 0;
    case 0x33u:
        yt2_mouse_int33(machine);
        return 0;
    case 0x67u:
        yt2_ems_int67(machine);
        return 0;
    case 0x1Cu:
        if (machine->int1c_cs != YT2_SENTINEL_SEG ||
            machine->int1c_ip != YT2_SENTINEL_IRET) {
            yt2_push16(machine->r.flags);
            yt2_push16(machine->r.cs);
            yt2_push16(machine->r.ip);
            machine->next_cs = machine->int1c_cs;
            machine->next_ip = machine->int1c_ip;
            machine->next_pending = 1;
            return 1;
        }
        return 0;
    case 0x00u:
    case 0x03u:
        return 0;
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/* CPU helpers                                                               */
/* ------------------------------------------------------------------------ */

uint8_t yt2_cpu_adc8(yt2_registers *r, uint8_t a, uint8_t b)
{
    return yt2_cpu_add8(r, a, b, (unsigned)yt2_cpu_get_flag(r, YT2_FLAG_CF));
}

uint16_t yt2_cpu_adc16(yt2_registers *r, uint16_t a, uint16_t b)
{
    return yt2_cpu_add16(r, a, b, (unsigned)yt2_cpu_get_flag(r, YT2_FLAG_CF));
}

uint8_t yt2_cpu_sbb8(yt2_registers *r, uint8_t a, uint8_t b)
{
    return yt2_cpu_sub8(r, a, b, (unsigned)yt2_cpu_get_flag(r, YT2_FLAG_CF));
}

uint16_t yt2_cpu_sbb16(yt2_registers *r, uint16_t a, uint16_t b)
{
    return yt2_cpu_sub16(r, a, b, (unsigned)yt2_cpu_get_flag(r, YT2_FLAG_CF));
}

static void yt2_logic32(yt2_registers *r, uint32_t result)
{
    r->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_PF | YT2_FLAG_AF |
                            YT2_FLAG_ZF | YT2_FLAG_SF | YT2_FLAG_OF);
    if (result == 0u) {
        r->flags |= YT2_FLAG_ZF;
    }
    if ((result & 0x80000000u) != 0u) {
        r->flags |= YT2_FLAG_SF;
    }
    {
        uint8_t low = (uint8_t)(result & 0xFFu);
        unsigned bits = 0;
        unsigned i;
        for (i = 0; i < 8; i++) {
            bits += (unsigned)((low >> i) & 1u);
        }
        if ((bits & 1u) == 0u) {
            r->flags |= YT2_FLAG_PF;
        }
    }
}

uint32_t yt2_cpu_add32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint64_t sum = (uint64_t)a + (uint64_t)b;
    uint32_t result = (uint32_t)sum;
    int carry = sum > 0xFFFFFFFFu;
    int aux = ((a ^ b ^ result) & 0x10u) != 0u;
    int overflow = ((a ^ result) & (b ^ result) & 0x80000000u) != 0u;

    yt2_logic32(r, result);
    if (carry) {
        r->flags |= YT2_FLAG_CF;
    }
    if (aux) {
        r->flags |= YT2_FLAG_AF;
    }
    if (overflow) {
        r->flags |= YT2_FLAG_OF;
    }
    return result;
}

uint32_t yt2_cpu_sub32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint32_t result = (uint32_t)(a - b);
    int carry = a < b;
    int aux = ((a ^ b ^ result) & 0x10u) != 0u;
    int overflow = ((a ^ b) & (a ^ result) & 0x80000000u) != 0u;

    yt2_logic32(r, result);
    if (carry) {
        r->flags |= YT2_FLAG_CF;
    }
    if (aux) {
        r->flags |= YT2_FLAG_AF;
    }
    if (overflow) {
        r->flags |= YT2_FLAG_OF;
    }
    return result;
}

uint32_t yt2_cpu_adc32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint32_t carry = (uint32_t)yt2_cpu_get_flag(r, YT2_FLAG_CF);
    uint64_t sum = (uint64_t)a + (uint64_t)b + carry;
    uint32_t result = (uint32_t)sum;
    int carry_out = sum > 0xFFFFFFFFu;
    int aux = ((a ^ b ^ result) & 0x10u) != 0u;
    int overflow = ((a ^ result) & (b ^ result) & 0x80000000u) != 0u;

    yt2_logic32(r, result);
    if (carry_out) {
        r->flags |= YT2_FLAG_CF;
    }
    if (aux) {
        r->flags |= YT2_FLAG_AF;
    }
    if (overflow) {
        r->flags |= YT2_FLAG_OF;
    }
    return result;
}

uint32_t yt2_cpu_sbb32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint32_t borrow = (uint32_t)yt2_cpu_get_flag(r, YT2_FLAG_CF);
    uint64_t amount = (uint64_t)b + borrow;
    uint32_t result = (uint32_t)((uint64_t)a - amount);
    int carry_out = (uint64_t)a < amount;
    int aux = ((a ^ b ^ result) & 0x10u) != 0u;
    int overflow = ((a ^ b) & (a ^ result) & 0x80000000u) != 0u;

    yt2_logic32(r, result);
    if (carry_out) {
        r->flags |= YT2_FLAG_CF;
    }
    if (aux) {
        r->flags |= YT2_FLAG_AF;
    }
    if (overflow) {
        r->flags |= YT2_FLAG_OF;
    }
    return result;
}

uint32_t yt2_cpu_and32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint32_t result = a & b;
    yt2_logic32(r, result);
    return result;
}

uint32_t yt2_cpu_or32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint32_t result = a | b;
    yt2_logic32(r, result);
    return result;
}

uint32_t yt2_cpu_xor32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint32_t result = a ^ b;
    yt2_logic32(r, result);
    return result;
}

uint32_t yt2_cpu_test32(yt2_registers *r, uint32_t a, uint32_t b)
{
    uint32_t result = a & b;
    yt2_logic32(r, result);
    return result;
}

uint32_t yt2_cpu_neg32(yt2_registers *r, uint32_t a)
{
    return yt2_cpu_sub32(r, 0u, a);
}

uint32_t yt2_cpu_inc32(yt2_registers *r, uint32_t a)
{
    uint16_t cf = r->flags & YT2_FLAG_CF;
    uint32_t result = yt2_cpu_add32(r, a, 1u);
    r->flags = (uint16_t)((r->flags & (uint16_t)~YT2_FLAG_CF) | cf);
    return result;
}

uint32_t yt2_cpu_dec32(yt2_registers *r, uint32_t a)
{
    uint16_t cf = r->flags & YT2_FLAG_CF;
    uint32_t result = yt2_cpu_sub32(r, a, 1u);
    r->flags = (uint16_t)((r->flags & (uint16_t)~YT2_FLAG_CF) | cf);
    return result;
}

uint32_t yt2_cpu_cmp32(yt2_registers *r, uint32_t a, uint32_t b)
{
    return yt2_cpu_sub32(r, a, b);
}

void yt2_cpu_mul8(yt2_registers *r, uint8_t value)
{
    uint16_t product = (uint16_t)((uint16_t)(uint8_t)(r->ax & 0xFFu) * value);
    r->ax = product;
    r->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_OF);
    if ((product & 0xFF00u) != 0u) {
        r->flags |= (uint16_t)(YT2_FLAG_CF | YT2_FLAG_OF);
    }
}

void yt2_cpu_mul16(yt2_registers *r, uint16_t value)
{
    uint32_t product = (uint32_t)r->ax * (uint32_t)value;
    r->ax = (uint16_t)(product & 0xFFFFu);
    r->dx = (uint16_t)(product >> 16);
    r->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_OF);
    if ((product & 0xFFFF0000u) != 0u) {
        r->flags |= (uint16_t)(YT2_FLAG_CF | YT2_FLAG_OF);
    }
}

void yt2_cpu_imul8(yt2_registers *r, uint8_t value)
{
    int16_t product = (int16_t)((int8_t)(r->ax & 0xFFu) * (int8_t)value);
    r->ax = (uint16_t)product;
    r->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_OF);
    if (product < -128 || product > 127) {
        r->flags |= (uint16_t)(YT2_FLAG_CF | YT2_FLAG_OF);
    }
}

void yt2_cpu_imul16(yt2_registers *r, uint16_t value)
{
    int32_t product = (int32_t)(int16_t)r->ax * (int32_t)(int16_t)value;
    r->ax = (uint16_t)((uint32_t)product & 0xFFFFu);
    r->dx = (uint16_t)(((uint32_t)product >> 16) & 0xFFFFu);
    r->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_OF);
    if (product < -32768 || product > 32767) {
        r->flags |= (uint16_t)(YT2_FLAG_CF | YT2_FLAG_OF);
    }
}

int yt2_cpu_div8(yt2_registers *r, uint8_t value)
{
    unsigned dividend;
    unsigned quotient;

    if (value == 0u) {
        return 0;
    }
    dividend = r->ax;
    quotient = dividend / value;
    if (quotient > 0xFFu) {
        return 0;
    }
    r->ax = (uint16_t)((uint16_t)(dividend % value) << 8) | (uint16_t)quotient;
    return 1;
}

int yt2_cpu_div16(yt2_registers *r, uint16_t value)
{
    uint32_t dividend = ((uint32_t)r->dx << 16) | r->ax;
    uint32_t quotient;

    if (value == 0u) {
        return 0;
    }
    quotient = dividend / value;
    if (quotient > 0xFFFFu) {
        return 0;
    }
    r->ax = (uint16_t)quotient;
    r->dx = (uint16_t)(dividend % value);
    return 1;
}

int yt2_cpu_idiv8(yt2_registers *r, uint8_t value)
{
    int dividend = (int16_t)r->ax;
    int divisor = (int8_t)value;
    int quotient;
    int remainder;

    if (divisor == 0) {
        return 0;
    }
    quotient = dividend / divisor;
    remainder = dividend % divisor;
    if (quotient < -128 || quotient > 127) {
        return 0;
    }
    r->ax = (uint16_t)(((uint16_t)((uint8_t)(int8_t)remainder)) << 8) |
            (uint16_t)(uint8_t)(int8_t)quotient;
    return 1;
}

int yt2_cpu_idiv16(yt2_registers *r, uint16_t value)
{
    int32_t dividend = (int32_t)(((uint32_t)r->dx << 16) | r->ax);
    int32_t divisor = (int16_t)value;
    int32_t quotient;
    int32_t remainder;

    if (divisor == 0) {
        return 0;
    }
    quotient = dividend / divisor;
    remainder = dividend % divisor;
    if (quotient < -32768 || quotient > 32767) {
        return 0;
    }
    r->ax = (uint16_t)quotient;
    r->dx = (uint16_t)remainder;
    return 1;
}

static uint8_t yt2_shift_count(unsigned count)
{
    return (uint8_t)(count & 0x1Fu);
}

static uint16_t yt2_shift_flags(yt2_registers *r, uint16_t value, uint8_t count,
                                int last_carry)
{
    r->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_OF);
    if (count != 0u) {
        if (last_carry) {
            r->flags |= YT2_FLAG_CF;
        }
        if (count == 1u) {
            if (((value & 0x8000u) != 0u) != (last_carry != 0)) {
                r->flags |= YT2_FLAG_OF;
            }
        }
    }
    return value;
}

uint8_t yt2_cpu_shl8(yt2_registers *r, uint8_t value, unsigned count)
{
    unsigned n = yt2_shift_count(count);
    uint8_t result = value;
    int carry = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        carry = (result & 0x80u) != 0u;
        result = (uint8_t)(result << 1);
    }
    return (uint8_t)yt2_shift_flags(r, result, (uint8_t)n, carry);
}

uint16_t yt2_cpu_shl16(yt2_registers *r, uint16_t value, unsigned count)
{
    unsigned n = yt2_shift_count(count);
    uint16_t result = value;
    int carry = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        carry = (result & 0x8000u) != 0u;
        result = (uint16_t)(result << 1);
    }
    return yt2_shift_flags(r, result, (uint8_t)n, carry);
}

uint32_t yt2_cpu_shl32(yt2_registers *r, uint32_t value, unsigned count)
{
    unsigned n = yt2_shift_count(count);
    uint32_t result = value;
    int carry = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        carry = (result & 0x80000000u) != 0u;
        result <<= 1;
    }
    r->flags &= (uint16_t)~(YT2_FLAG_CF | YT2_FLAG_OF);
    if (n != 0u) {
        if (carry) {
            r->flags |= YT2_FLAG_CF;
        }
    }
    return result;
}

uint8_t yt2_cpu_shr8(yt2_registers *r, uint8_t value, unsigned count)
{
    unsigned n = yt2_shift_count(count);
    uint8_t result = value;
    int carry = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        carry = (result & 1u) != 0u;
        result = (uint8_t)(result >> 1);
    }
    return (uint8_t)yt2_shift_flags(r, result, (uint8_t)n, carry);
}

uint16_t yt2_cpu_shr16(yt2_registers *r, uint16_t value, unsigned count)
{
    unsigned n = yt2_shift_count(count);
    uint16_t result = value;
    int carry = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        carry = (result & 1u) != 0u;
        result = (uint16_t)(result >> 1);
    }
    return yt2_shift_flags(r, result, (uint8_t)n, carry);
}

uint32_t yt2_cpu_shr32(yt2_registers *r, uint32_t value, unsigned count)
{
    (void)r;
    unsigned n = yt2_shift_count(count);
    uint32_t result = value;
    unsigned i;
    for (i = 0; i < n; i++) {
        result >>= 1;
    }
    return result;
}

uint8_t yt2_cpu_sar8(yt2_registers *r, uint8_t value, unsigned count)
{
    unsigned n = yt2_shift_count(count);
    int8_t result = (int8_t)value;
    int carry = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        carry = (result & 1) != 0;
        result = (int8_t)(result >> 1);
    }
    return (uint8_t)yt2_shift_flags(r, (uint16_t)(uint8_t)result, (uint8_t)n, carry);
}

uint16_t yt2_cpu_sar16(yt2_registers *r, uint16_t value, unsigned count)
{
    unsigned n = yt2_shift_count(count);
    int16_t result = (int16_t)value;
    int carry = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        carry = (result & 1) != 0;
        result = (int16_t)(result >> 1);
    }
    return yt2_shift_flags(r, (uint16_t)result, (uint8_t)n, carry);
}

uint32_t yt2_cpu_sar32(yt2_registers *r, uint32_t value, unsigned count)
{
    (void)r;
    unsigned n = yt2_shift_count(count);
    int32_t result = (int32_t)value;
    unsigned i;
    for (i = 0; i < n; i++) {
        result >>= 1;
    }
    return (uint32_t)result;
}

void yt2_cpu_daa(yt2_registers *r)
{
    uint8_t al = (uint8_t)(r->ax & 0xFFu);
    uint8_t old_al = al;
    int old_cf = yt2_cpu_get_flag(r, YT2_FLAG_CF);
    uint16_t keep = r->flags & (uint16_t)(YT2_FLAG_CF | YT2_FLAG_AF);

    if ((al & 0x0Fu) > 9u || yt2_cpu_get_flag(r, YT2_FLAG_AF)) {
        al = (uint8_t)(al + 6u);
        keep |= YT2_FLAG_AF;
        if (old_cf || al < old_al) {
            keep |= YT2_FLAG_CF;
        }
    } else {
        keep &= (uint16_t)~YT2_FLAG_AF;
    }
    if (old_al > 0x99u || old_cf) {
        al = (uint8_t)(al + 0x60u);
        keep |= YT2_FLAG_CF;
    }
    yt2_cpu_test8(r, al, 0u);
    r->flags |= keep;
    yt2_r8_set(r, YT2_R8_AL, al);
}

void yt2_cpu_das(yt2_registers *r)
{
    uint8_t al = (uint8_t)(r->ax & 0xFFu);
    uint8_t old_al = al;
    int old_cf = yt2_cpu_get_flag(r, YT2_FLAG_CF);
    uint16_t keep = r->flags & (uint16_t)(YT2_FLAG_CF | YT2_FLAG_AF);

    if ((al & 0x0Fu) > 9u || yt2_cpu_get_flag(r, YT2_FLAG_AF)) {
        al = (uint8_t)(al - 6u);
        keep |= YT2_FLAG_AF;
        if (old_cf || al > old_al) {
            keep |= YT2_FLAG_CF;
        }
    } else {
        keep &= (uint16_t)~YT2_FLAG_AF;
    }
    if (old_al > 0x99u || old_cf) {
        al = (uint8_t)(al - 0x60u);
        keep |= YT2_FLAG_CF;
    }
    yt2_cpu_test8(r, al, 0u);
    r->flags |= keep;
    yt2_r8_set(r, YT2_R8_AL, al);
}

/* ------------------------------------------------------------------------ */
/* String instructions                                                       */
/* ------------------------------------------------------------------------ */

static void yt2_string_step(yt2_machine *m, unsigned size, int advance_si,
                            int advance_di)
{
    uint16_t delta = (uint16_t)size;
    if ((m->r.flags & YT2_FLAG_DF) != 0u) {
        delta = (uint16_t)(0u - delta);
    }
    /* Only the operands an instruction actually uses move: MOVS/CMPS move
       both pointers, LODS/OUTS only source, STOS/SCAS only destination. */
    if (advance_si) {
        m->r.si = (uint16_t)(m->r.si + delta);
    }
    if (advance_di) {
        m->r.di = (uint16_t)(m->r.di + delta);
    }
}

static uint8_t yt2_read8(yt2_machine *m, uint16_t seg, uint16_t off)
{
    return m->mem.bytes[yt2_flat(seg, off)];
}

static uint16_t yt2_read16(yt2_machine *m, uint16_t seg, uint16_t off)
{
    return (uint16_t)((uint16_t)yt2_read8(m, seg, off) |
                      ((uint16_t)yt2_read8(m, seg, (uint16_t)(off + 1u)) << 8));
}

static void yt2_write8(yt2_machine *m, uint16_t seg, uint16_t off, uint8_t value)
{
    uint32_t address = yt2_flat(seg, off);
    yt2_mark_video(address);
    m->mem.bytes[address] = value;
}

static void yt2_write16(yt2_machine *m, uint16_t seg, uint16_t off, uint16_t value)
{
    yt2_write8(m, seg, off, (uint8_t)(value & 0xFFu));
    yt2_write8(m, seg, (uint16_t)(off + 1u), (uint8_t)(value >> 8));
}

void yt2_string_movs(yt2_machine *m, unsigned size, uint16_t source_seg,
                     unsigned repeat)
{
    uint16_t start_cx = m->r.cx;

    do {
        if (size == 1u) {
            yt2_write8(m, m->r.es, m->r.di, yt2_read8(m, source_seg, m->r.si));
        } else {
            yt2_write16(m, m->r.es, m->r.di, yt2_read16(m, source_seg, m->r.si));
        }
        yt2_string_step(m, size, 1, 1);
        if (repeat == 0u) {
            return;
        }
        m->r.cx = (uint16_t)(m->r.cx - 1u);
    } while (m->r.cx != 0u);

    /* fn_1A6B5 copies the whole work buffer into the VGA aperture with one
       REP MOVSW.  That is the game's frame flush: on a real card the new
       picture is visible immediately, and short-lived effects (the shooting
       star on the attract gate) last only a few milliseconds.  Sample here
       as well as at the poll/DAC/tick points, or a whole transient can fall
       between two presents.  The three-pass budget still applies. */
    if (repeat != 0u && size == 2u && start_cx == 0x7D00u &&
        m->r.es == (uint16_t)YT2_SCREEN_SEG) {
        yt2_game_present(m);
    }
}

void yt2_string_stos(yt2_machine *m, unsigned size, unsigned repeat)
{
    do {
        if (size == 1u) {
            yt2_write8(m, m->r.es, m->r.di, (uint8_t)(m->r.ax & 0xFFu));
        } else {
            yt2_write16(m, m->r.es, m->r.di, m->r.ax);
        }
        yt2_string_step(m, size, 0, 1);
        if (repeat == 0u) {
            return;
        }
        m->r.cx = (uint16_t)(m->r.cx - 1u);
    } while (m->r.cx != 0u);
}

void yt2_string_lods(yt2_machine *m, unsigned size, uint16_t source_seg,
                     unsigned repeat)
{
    do {
        if (size == 1u) {
            yt2_r8_set(&m->r, YT2_R8_AL, yt2_read8(m, source_seg, m->r.si));
        } else {
            m->r.ax = yt2_read16(m, source_seg, m->r.si);
        }
        yt2_string_step(m, size, 1, 0);
        if (repeat == 0u) {
            return;
        }
        m->r.cx = (uint16_t)(m->r.cx - 1u);
    } while (m->r.cx != 0u);
}

void yt2_string_cmps(yt2_machine *m, unsigned size, uint16_t source_seg,
                     unsigned repeat)
{
    do {
        uint16_t left;
        uint16_t right;
        if (size == 1u) {
            left = yt2_read8(m, m->r.es, m->r.di);
            right = yt2_read8(m, source_seg, m->r.si);
            yt2_cpu_sub8(&m->r, (uint8_t)left, (uint8_t)right, 0u);
        } else {
            left = yt2_read16(m, m->r.es, m->r.di);
            right = yt2_read16(m, source_seg, m->r.si);
            yt2_cpu_sub16(&m->r, left, right, 0u);
        }
        yt2_string_step(m, size, 1, 1);
        if (repeat == 0u) {
            return;
        }
        m->r.cx = (uint16_t)(m->r.cx - 1u);
        if (repeat == 1u && !yt2_cpu_get_flag(&m->r, YT2_FLAG_ZF)) {
            return;
        }
        if (repeat == 2u && yt2_cpu_get_flag(&m->r, YT2_FLAG_ZF)) {
            return;
        }
    } while (m->r.cx != 0u);
}

void yt2_string_scas(yt2_machine *m, unsigned size, unsigned repeat)
{
    do {
        uint16_t memory_value;
        if (size == 1u) {
            memory_value = yt2_read8(m, m->r.es, m->r.di);
            yt2_cpu_sub8(&m->r, (uint8_t)(m->r.ax & 0xFFu), (uint8_t)memory_value, 0u);
        } else {
            memory_value = yt2_read16(m, m->r.es, m->r.di);
            yt2_cpu_sub16(&m->r, m->r.ax, memory_value, 0u);
        }
        yt2_string_step(m, size, 0, 1);
        if (repeat == 0u) {
            return;
        }
        m->r.cx = (uint16_t)(m->r.cx - 1u);
        if (repeat == 1u && !yt2_cpu_get_flag(&m->r, YT2_FLAG_ZF)) {
            return;
        }
        if (repeat == 2u && yt2_cpu_get_flag(&m->r, YT2_FLAG_ZF)) {
            return;
        }
    } while (m->r.cx != 0u);
}

void yt2_string_outs(yt2_machine *m, unsigned size, uint16_t source_seg,
                     unsigned repeat)
{
    do {
        if (size == 1u) {
            yt2_port_out8(m->r.dx, yt2_read8(m, source_seg, m->r.si));
        } else {
            yt2_port_out16(m->r.dx, yt2_read16(m, source_seg, m->r.si));
        }
        yt2_string_step(m, size, 1, 0);
        if (repeat == 0u) {
            return;
        }
        m->r.cx = (uint16_t)(m->r.cx - 1u);
    } while (m->r.cx != 0u);
}

/* ------------------------------------------------------------------------ */
/* Ports                                                                     */
/* ------------------------------------------------------------------------ */

uint8_t yt2_port_in8(uint16_t port)
{
    switch (port) {
    case 0x3DAu: {
        uint32_t phase = (uint32_t)(g_m.cycles % YT2_VGA_FRAME_CYCLES);
        uint8_t status = 0;
        if (phase + YT2_VGA_RETRACE_CYCLES >= YT2_VGA_FRAME_CYCLES) {
            status |= 0x08u;              /* vertical retrace active */
        }
        if (phase + YT2_VGA_FRAME_CYCLES / 8u >= YT2_VGA_FRAME_CYCLES) {
            status |= 0x01u;              /* display disabled (blanking) */
        }
        return status;
    }
    case 0x61u:
        return g_m.port61;
    default:
        return 0xFFu;
    }
}

uint16_t yt2_port_in16(uint16_t port)
{
    return (uint16_t)((uint16_t)yt2_port_in8(port) |
                      ((uint16_t)yt2_port_in8((uint16_t)(port + 1u)) << 8));
}

uint32_t yt2_port_in32(uint16_t port)
{
    return (uint32_t)yt2_port_in16(port) |
           ((uint32_t)yt2_port_in16((uint16_t)(port + 2u)) << 16);
}

void yt2_port_out8(uint16_t port, uint8_t value)
{
    switch (port) {
    case 0x3C8u:
        /* fn_WriteDAC and fn_WriteDAC768 set the DAC index at the start of a
           palette write.  The seam runs a present at that point (with the
           palette staged for the bytes it is about to write); presenting
           before the index changes sends the previous completed write, so a
           fade step is a frame instead of being lost until the next poll. */
        yt2_game_present(&g_m);
        g_m.dac_index = value;
        g_m.dac_phase = 0;
        break;
    case 0x3C9u:
        g_m.palette[(g_m.dac_index % 256u) * 3u + g_m.dac_phase] = (uint8_t)(value & 0x3Fu);
        /* v_PalCurrent (DGROUP:4124h) is documented as "the palette on the
           DAC", and the scene code shows it with a DAC block call.  A fade
           through the v_PalOutput staging buffer (fn_FadeIn and friends)
           writes the DAC without touching 4124h, so keep the buffer in step
           with every DAC byte or the next block call would restore a stale
           palette over the picture. */
        g_m.mem.bytes[((uint32_t)YT2_DGROUP_SEG << 4) + 0x4124u +
                      (g_m.dac_index % 256u) * 3u + g_m.dac_phase] =
            (uint8_t)(value & 0x3Fu);
        g_m.video_dirty = 1;
        g_m.dac_phase++;
        if (g_m.dac_phase == 3u) {
            g_m.dac_phase = 0;
            g_m.dac_index = (uint16_t)((g_m.dac_index + 1u) % 256u);
        }
        break;
    case 0x61u:
        g_m.port61 = value;
        break;
    case 0x42u:
        g_m.pit42 = value;
        break;
    default:
        break;
    }
}

void yt2_port_out16(uint16_t port, uint16_t value)
{
    yt2_port_out8(port, (uint8_t)(value & 0xFFu));
    yt2_port_out8((uint16_t)(port + 1u), (uint8_t)(value >> 8));
}

void yt2_port_out32(uint16_t port, uint32_t value)
{
    yt2_port_out16(port, (uint16_t)(value & 0xFFFFu));
    yt2_port_out16((uint16_t)(port + 2u), (uint16_t)(value >> 16));
}

/* ------------------------------------------------------------------------ */
/* Startup and run loop                                                      */
/* ------------------------------------------------------------------------ */

void yt2_game_init(yt2_machine *machine, const char *root)
{
    memset(machine, 0, sizeof(*machine));
    if (root != NULL) {
        snprintf(machine->root, sizeof(machine->root), "%s", root);
    }
    machine->dos_block_count = 1u;
    machine->dos_blocks[0].segment = YT2_DOS_HEAP_START;
    machine->dos_blocks[0].paragraphs = (uint16_t)(YT2_DOS_HEAP_END - YT2_DOS_HEAP_START);
    machine->dos_blocks[0].used = 0;
    machine->cycles = 0u;
    machine->tick_at = (uint64_t)YT2_CYCLES_PER_TICK;
    machine->pace_at = YT2_CYCLES_PER_MS * 2u;
    machine->int1c_cs = YT2_SENTINEL_SEG;
    machine->int1c_ip = YT2_SENTINEL_IRET;
    machine->mouse_x = 0x00E6u;
    machine->mouse_y = 0x00B4u;
    machine->mouse_ratio_x = 8u;
    machine->mouse_ratio_y = 8u;
    machine->ems_total_pages = 0x3C0u;
    machine->ems_free_pages = 0x3C0u;
    machine->ems_free_count = 0x3C0u;
    machine->ems_pool = calloc(YT2_EMS_MAX_PAGES, YT2_EMS_PAGE_BYTES);
    if (machine->ems_pool == NULL) {
        fprintf(stderr, "yt2: cannot allocate the EMS page pool\n");
        machine->exited = 1;
        machine->exit_code = -1;
        return;
    }
    {
        unsigned index;
        for (index = 0; index < YT2_EMS_MAX_PAGES; index++) {
            machine->ems_free_page[index] = (uint16_t)index;
        }
        for (index = 0; index < 4u; index++) {
            machine->ems_window_handle[index] = 0xFFFFu;
            machine->ems_window_page[index] = 0xFFFFu;
        }
    }
}

void yt2_game_load(yt2_machine *machine)
{
    uint32_t base = (uint32_t)YT2_LOAD_SEG << 4;
    uint8_t *psp;

    yt2_cpu_reset(&machine->r);
    memcpy(machine->mem.bytes + base, yt2_game_image, yt2_game_image_size);

    psp = machine->mem.bytes + ((uint32_t)YT2_PSP_SEG << 4);
    memset(psp, 0, 0x100u);
    /* CH2.BAT runs "SWREG.EXE %1 %2 %3 %4 /W /B"; the game checks for /B. */
    psp[0x80] = 6;
    memcpy(psp + 0x81, " /W /B", 6);
    psp[0x87] = 0x0D;

    machine->r.cs = (uint16_t)(YT2_LOAD_SEG + yt2_game_entry_cs);
    machine->r.ds = YT2_PSP_SEG;
    machine->r.es = YT2_PSP_SEG;
    machine->r.ss = (uint16_t)(YT2_LOAD_SEG + yt2_game_ss);
    machine->r.sp = yt2_game_sp;
    machine->r.ip = yt2_game_entry_ip;
    machine->r.flags = 0x0202u;

    machine->next_cs = machine->r.cs;
    machine->next_ip = machine->r.ip;
    machine->next_pending = 1;
}

int yt2_game_run(yt2_machine *machine, unsigned long max_transfers)
{
    machine->abort_armed = 1;
    if (setjmp(machine->abort_jmp) != 0) {
        machine->abort_armed = 0;
        return machine->exited ? machine->exit_code : 0;
    }
    while (!machine->exited && !machine->stop_requested &&
           (max_transfers == 0u || machine->transfers < max_transfers)) {
        uint16_t cs;
        uint16_t ip;
        if (!machine->next_pending) {
            break;
        }
        cs = machine->next_cs;
        ip = machine->next_ip;
        machine->next_pending = 0;
        machine->transfers++;
        /* The seam's fn_SeamDelayStub presents at fn_DelayTicks entry (S084D
           offset 4, reached at runtime as YT2_LOAD_SEG + 084Dh), so an
           animation that waits a tick between steps gets a frame per step
           even when it never polls input. */
        if (cs == (uint16_t)(YT2_LOAD_SEG + 0x084Du) && ip == 0x0004u) {
            yt2_game_present(machine);
        }
        if (machine->trace_api) {
            fprintf(stderr, "yt2: dispatch %04X:%04X (ax=%04X bx=%04X cx=%04X dx=%04X ds=%04X es=%04X ss=%04X sp=%04X)\n",
                    cs, ip, machine->r.ax, machine->r.bx, machine->r.cx, machine->r.dx,
                    machine->r.ds, machine->r.es, machine->r.ss, machine->r.sp);
        }
        yt2_game_dispatch(cs, ip);
    }
    machine->abort_armed = 0;
    return machine->exited ? machine->exit_code : 0;
}

void yt2_game_shutdown(yt2_machine *machine)
{
    unsigned index;

    yt2_link_close(machine);
    for (index = 0; index < YT2_DOS_FILES; index++) {
        if (machine->files[index].used && machine->files[index].stream != NULL) {
            fclose(machine->files[index].stream);
            machine->files[index].stream = NULL;
            machine->files[index].used = 0;
        }
    }
    free(machine->ems_pool);
    machine->ems_pool = NULL;
}
