#ifndef YT2_GAME_H
#define YT2_GAME_H

#include <setjmp.h>
#include <stdio.h>

#include "yt2/core.h"
#include "yt2/cpu.h"
#include "yt2/memory.h"
#include "yt2/service.h"
#include "yt2/transport.h"
#include "yt2/video.h"

/*
 * The transliterated game code runs the original 16-bit image inside the
 * flat address space.  The load module sits at YT2_LOAD_SEG; DOS relocation
 * values are already baked into the generated image, so every far pointer in
 * the code is a real runtime segment (load segment + original paragraph).
 */

#define YT2_LOAD_SEG 0x1000u
#define YT2_PSP_SEG (YT2_LOAD_SEG - 0x10u)
#define YT2_SENTINEL_SEG 0xF000u
#define YT2_SENTINEL_IRET 0x0000u
#define YT2_SENTINEL_RESUME 0x0001u
#define YT2_EMS_FRAME_SEG 0xE000u
#define YT2_EMS_PAGE_BYTES 0x4000u
#define YT2_EMS_HANDLES 16u
#define YT2_EMS_MAX_PAGES 1024u
#define YT2_DOS_FILES 16u
#define YT2_DOS_BLOCKS 64u
#define YT2_DOS_HEAP_START 0x4000u
#define YT2_DOS_HEAP_END 0xA000u

#define YT2_SCREEN_SEG 0xA000u

/* The game's DGROUP frame: S1DEA in the original layout. */
#define YT2_DGROUP_SEG (YT2_LOAD_SEG + 0x1DEAu)

/* DOS type-ahead queue drained by int 21h 06h (the BIOS ring equivalent). */
#define YT2_KEY_QUEUE 32u

#define YT2_MOUSE_LEFT 0x02u
#define YT2_MOUSE_RIGHT 0x04u

typedef enum yt2_r8 {
    YT2_R8_AL, YT2_R8_AH, YT2_R8_BL, YT2_R8_BH,
    YT2_R8_CL, YT2_R8_CH, YT2_R8_DL, YT2_R8_DH
} yt2_r8;

typedef struct yt2_dos_file {
    FILE *stream;
    int used;
} yt2_dos_file;

typedef struct yt2_ems_handle {
    int used;
    uint16_t pages;
    uint16_t page[YT2_EMS_MAX_PAGES];
} yt2_ems_handle;

typedef struct yt2_dos_block {
    uint16_t segment;
    uint16_t paragraphs;
    int used;
} yt2_dos_block;

typedef struct yt2_machine {
    yt2_registers r;
    yt2_memory mem;

    uint16_t next_cs;
    uint16_t next_ip;
    int next_pending;
    int exited;
    int exit_code;
    int halted;
    int trace_api;
    int trace_input;
    uint32_t steps;
    uint32_t limit_ticks;
    int stop_requested;
    uint32_t delay_calls;
    uint64_t cycles;
    uint64_t tick_at;
    jmp_buf abort_jmp;
    int abort_armed;

    /* 386 register halves, only the .386 blocks use them. */
    uint32_t eax_high;
    uint32_t ebx_high;
    uint32_t ecx_high;
    uint32_t edx_high;

    /* Logical 18.2 Hz clock and the installed INT 1Ch vector. */
    uint32_t ticks;
    uint16_t int1c_cs;
    uint16_t int1c_ip;
    int in_tick;
    int nested;
    int nested_done;

    /* Video state (the framebuffer itself lives at A000:0000). */
    int video_mode;
    uint8_t palette[768];
    uint16_t dac_index;
    unsigned dac_phase;
    int vbe_mode;

    uint8_t vbe_bank[4][0x10000];
    int vbe_bank_active;

    /* Presentation log: the SEAM build presents at input-poll, tick-ISR and
       palette/delay hooks, so a hash per present gives the same frame
       sequence as a stream recording.  Only frames that changed are logged.
       present_tick/present_passes mirror the seam's three-passes-per-BIOS-tick
       gate, so a palette fade cannot flood the client. */
    FILE *present_log;
    unsigned long long present_frames;
    uint64_t present_last_fb;
    uint64_t present_last_pal;
    int present_have_last;
    int video_dirty;
    uint32_t present_tick;
    unsigned present_passes;
    uint8_t present_frame[64000];
    uint8_t present_palette[768];
    char present_dir[512];

    /* Ports the game touches directly. */
    uint8_t port61;
    uint8_t pit42;

    /* DOS file service. */
    char root[512];
    yt2_dos_file files[YT2_DOS_FILES];

    /* DOS paragraph allocator (conventional memory above the image). */
    yt2_dos_block dos_blocks[YT2_DOS_BLOCKS];
    unsigned dos_block_count;

    /* EMS 4.0. */
    uint8_t *ems_pool;
    uint16_t ems_total_pages;
    uint16_t ems_free_pages;
    uint8_t ems_page_owner[YT2_EMS_MAX_PAGES];
    uint16_t ems_free_page[YT2_EMS_MAX_PAGES];
    uint16_t ems_free_count;
    uint16_t ems_free_next;
    yt2_ems_handle ems_handle[YT2_EMS_HANDLES];
    uint16_t ems_window_handle[4];
    uint16_t ems_window_page[4];

    /* Checkpoint/diagnostic output. */
    uint64_t transfers;

    /* Folder link (live play): the M2 seam moved over OUTPUT.BIN/INPUT.BIN. */
    int link;
    int link_ready;
    int link_mode13;
    char link_output_path[1024];
    char link_input_path[1024];
    yt2_folder_transport folder;
    yt2_transport transport;
    yt2_service service;
    yt2_video video;

    /* Real-time pacing: guest cycles against the host's monotonic PIT clock. */
    int pace;
    uint32_t pace_at;
    int pace_started;
    uint32_t pace_wall_start;
    uint64_t pace_cycle_start;

    /* DOS keyboard type-ahead. */
    uint16_t key_queue[YT2_KEY_QUEUE];
    unsigned key_head;
    unsigned key_tail;
    unsigned key_count;
    int key_extended;
    uint8_t key_scan;

    /* INT 33h shim state (the game's fn_MouseInit/fn_MouseHandler peer). */
    uint16_t mouse_x;
    uint16_t mouse_y;
    uint16_t mouse_buttons;
    int16_t mouse_dx;
    int16_t mouse_dy;
    int16_t mouse_frac_x;
    int16_t mouse_frac_y;
    uint16_t mouse_handler_ip;
    uint16_t mouse_handler_cs;
    uint16_t mouse_mask;
    uint16_t mouse_ratio_x;
    uint16_t mouse_ratio_y;
} yt2_machine;

extern yt2_machine g_m;

/*
 * Guest cycle model.  The oracle profile runs a 486 at a fixed 30 000
 * cycles/ms (DOSBox-X); one 18.2 Hz BIOS tick lasts 65536/1193182 s, i.e.
 * 1 647 765 cycles.  Instructions are charged a flat cost for now; the
 * exact per-opcode costs can follow once the title is up.
 */
#define YT2_CYCLES_PER_MS 30000u
#define YT2_CYCLES_PER_TICK 1647765u
#define YT2_CYCLES_INSN 2u
#define YT2_VGA_FRAME_CYCLES 428040u   /* 640x400 at 70.086 Hz */
#define YT2_VGA_RETRACE_CYCLES 36000u  /* vertical retrace window */
#define YT2_PASSES_PER_TICK 3u         /* SEAM_PASSES_PER_TICK */

void yt2_cycles_slow(void);
void yt2_pace_slow(void);

static inline void yt2_cycles(uint32_t cycles)
{
    g_m.cycles += cycles;
    if (g_m.cycles >= g_m.tick_at) {
        yt2_cycles_slow();
    } else if (g_m.pace && g_m.cycles >= g_m.pace_at) {
        yt2_pace_slow();
    }
}

void yt2_game_init(yt2_machine *machine, const char *root);
void yt2_game_load(yt2_machine *machine);
int yt2_game_run(yt2_machine *machine, unsigned long max_transfers);
void yt2_game_shutdown(yt2_machine *machine);

/* Live folder link (game_link.c). */
int yt2_link_open(yt2_machine *machine);
void yt2_link_poll(yt2_machine *machine);
void yt2_link_present(yt2_machine *machine);
void yt2_link_close(yt2_machine *machine);

/* Input glue shared by the DOS/INT 33h services and the link (game_services.c). */
void yt2_key_push(yt2_machine *machine, uint8_t scan, uint8_t ascii);
void yt2_key_flush(yt2_machine *machine);
int yt2_key_pop(yt2_machine *machine, uint16_t *value);
void yt2_mouse_move(yt2_machine *machine, int16_t dx, int16_t dy);
void yt2_mouse_abs(yt2_machine *machine, uint16_t x, uint16_t y);
void yt2_mouse_button(yt2_machine *machine, uint8_t button, uint8_t down);

/*
 * Run a far call into guest code to completion (the mouse callback path).
 * The caller must be at the top level: not inside a tick or another call.
 */
void yt2_machine_call(yt2_machine *machine, uint16_t segment, uint16_t offset);
int yt2_machine_call_safe(const yt2_machine *machine);

void yt2_game_dispatch(uint16_t seg, uint16_t off);

void yt2_game_present(yt2_machine *machine);

void yt2_go(uint16_t cs, uint16_t ip);
void yt2_push16(uint16_t value);
uint16_t yt2_pop16(void);

uint8_t yt2_r8_get(const yt2_registers *registers, unsigned which);
void yt2_r8_set(yt2_registers *registers, unsigned which, uint8_t value);

uint32_t yt2_get_eax(void);
uint32_t yt2_get_ebx(void);
uint32_t yt2_get_ecx(void);
uint32_t yt2_get_edx(void);
void yt2_set_eax(uint32_t value);
void yt2_set_ebx(uint32_t value);
void yt2_set_ecx(uint32_t value);
void yt2_set_edx(uint32_t value);

static inline uint32_t yt2_flat(uint16_t segment, uint16_t offset)
{
    return ((uint32_t)segment << 4) + (uint32_t)offset;
}

static inline uint16_t yt2_ea(uint16_t base, uint16_t index, uint16_t disp)
{
    return (uint16_t)(base + index + disp);
}

#define YT2_MEM8(seg, off) \
    (g_m.mem.bytes[yt2_flat((uint16_t)(seg), (uint16_t)(off))])
#define YT2_MEM16(seg, off) \
    ((uint16_t)((uint16_t)YT2_MEM8((seg), (off)) | \
                ((uint16_t)YT2_MEM8((seg), (uint16_t)((off) + 1u)) << 8)))
#define YT2_MEM32(seg, off) \
    ((uint32_t)YT2_MEM8((seg), (off)) | \
     ((uint32_t)YT2_MEM8((seg), (uint16_t)((off) + 1u)) << 8) | \
     ((uint32_t)YT2_MEM8((seg), (uint16_t)((off) + 2u)) << 16) | \
     ((uint32_t)YT2_MEM8((seg), (uint16_t)((off) + 3u)) << 24))

static inline void yt2_mark_video(uint32_t address)
{
    if (address >= 0xA0000u && address < 0xB0000u) {
        g_m.video_dirty = 1;
    }
}

static inline void yt2_set8(uint16_t segment, uint16_t offset, uint8_t value)
{
    uint32_t address = yt2_flat(segment, offset);
    yt2_mark_video(address);
    g_m.mem.bytes[address] = value;
}

static inline void yt2_set16(uint16_t segment, uint16_t offset, uint16_t value)
{
    uint32_t address = yt2_flat(segment, offset);
    yt2_mark_video(address);
    g_m.mem.bytes[address] = (uint8_t)(value & 0xFFu);
    g_m.mem.bytes[address + 1u] = (uint8_t)(value >> 8);
}

static inline void yt2_set32(uint16_t segment, uint16_t offset, uint32_t value)
{
    uint32_t address = yt2_flat(segment, offset);
    yt2_mark_video(address);
    g_m.mem.bytes[address] = (uint8_t)(value & 0xFFu);
    g_m.mem.bytes[address + 1u] = (uint8_t)((value >> 8) & 0xFFu);
    g_m.mem.bytes[address + 2u] = (uint8_t)((value >> 16) & 0xFFu);
    g_m.mem.bytes[address + 3u] = (uint8_t)((value >> 24) & 0xFFu);
}

/* CPU helpers beyond yt2/cpu.h (32-bit forms, mul/div, shifts, strings). */
uint8_t yt2_cpu_adc8(yt2_registers *r, uint8_t a, uint8_t b);
uint16_t yt2_cpu_adc16(yt2_registers *r, uint16_t a, uint16_t b);
uint32_t yt2_cpu_adc32(yt2_registers *r, uint32_t a, uint32_t b);
uint8_t yt2_cpu_sbb8(yt2_registers *r, uint8_t a, uint8_t b);
uint16_t yt2_cpu_sbb16(yt2_registers *r, uint16_t a, uint16_t b);
uint32_t yt2_cpu_sbb32(yt2_registers *r, uint32_t a, uint32_t b);

uint32_t yt2_cpu_add32(yt2_registers *r, uint32_t a, uint32_t b);
uint32_t yt2_cpu_sub32(yt2_registers *r, uint32_t a, uint32_t b);
uint32_t yt2_cpu_and32(yt2_registers *r, uint32_t a, uint32_t b);
uint32_t yt2_cpu_or32(yt2_registers *r, uint32_t a, uint32_t b);
uint32_t yt2_cpu_xor32(yt2_registers *r, uint32_t a, uint32_t b);
uint32_t yt2_cpu_test32(yt2_registers *r, uint32_t a, uint32_t b);
uint32_t yt2_cpu_neg32(yt2_registers *r, uint32_t a);
uint32_t yt2_cpu_inc32(yt2_registers *r, uint32_t a);
uint32_t yt2_cpu_dec32(yt2_registers *r, uint32_t a);
uint32_t yt2_cpu_cmp32(yt2_registers *r, uint32_t a, uint32_t b);

void yt2_cpu_mul8(yt2_registers *r, uint8_t value);
void yt2_cpu_mul16(yt2_registers *r, uint16_t value);
void yt2_cpu_imul8(yt2_registers *r, uint8_t value);
void yt2_cpu_imul16(yt2_registers *r, uint16_t value);
int yt2_cpu_div8(yt2_registers *r, uint8_t value);
int yt2_cpu_div16(yt2_registers *r, uint16_t value);
int yt2_cpu_idiv8(yt2_registers *r, uint8_t value);
int yt2_cpu_idiv16(yt2_registers *r, uint16_t value);

uint8_t yt2_cpu_shl8(yt2_registers *r, uint8_t value, unsigned count);
uint16_t yt2_cpu_shl16(yt2_registers *r, uint16_t value, unsigned count);
uint32_t yt2_cpu_shl32(yt2_registers *r, uint32_t value, unsigned count);
uint8_t yt2_cpu_shr8(yt2_registers *r, uint8_t value, unsigned count);
uint16_t yt2_cpu_shr16(yt2_registers *r, uint16_t value, unsigned count);
uint32_t yt2_cpu_shr32(yt2_registers *r, uint32_t value, unsigned count);
uint8_t yt2_cpu_sar8(yt2_registers *r, uint8_t value, unsigned count);
uint16_t yt2_cpu_sar16(yt2_registers *r, uint16_t value, unsigned count);
uint32_t yt2_cpu_sar32(yt2_registers *r, uint32_t value, unsigned count);

void yt2_cpu_daa(yt2_registers *r);
void yt2_cpu_das(yt2_registers *r);

void yt2_string_movs(yt2_machine *m, unsigned size, uint16_t source_seg, unsigned repeat);
void yt2_string_stos(yt2_machine *m, unsigned size, unsigned repeat);
void yt2_string_lods(yt2_machine *m, unsigned size, uint16_t source_seg, unsigned repeat);
void yt2_string_cmps(yt2_machine *m, unsigned size, uint16_t source_seg, unsigned repeat);
void yt2_string_scas(yt2_machine *m, unsigned size, unsigned repeat);
void yt2_string_outs(yt2_machine *m, unsigned size, uint16_t source_seg, unsigned repeat);

uint8_t yt2_port_in8(uint16_t port);
uint16_t yt2_port_in16(uint16_t port);
uint32_t yt2_port_in32(uint16_t port);
void yt2_port_out8(uint16_t port, uint8_t value);
void yt2_port_out16(uint16_t port, uint16_t value);
void yt2_port_out32(uint16_t port, uint32_t value);

int yt2_machine_int(yt2_machine *machine, unsigned number);
void yt2_machine_sentinel(yt2_machine *machine, uint16_t offset);
void yt2_machine_bad_entry(yt2_machine *machine, uint16_t segment, uint16_t offset);
void yt2_machine_bad_gap(yt2_machine *machine, uint16_t segment, uint16_t from, uint16_t to);
void yt2_machine_bad_jump(yt2_machine *machine, uint16_t segment, uint16_t offset);
void yt2_machine_divide_error(yt2_machine *machine);

void yt2_machine_tick(yt2_machine *machine);

/* Services (game_services.c). */
int yt2_dos_int21(yt2_machine *machine);
int yt2_video_int10(yt2_machine *machine);
int yt2_clock_int1a(yt2_machine *machine);
int yt2_mouse_int33(yt2_machine *machine);
int yt2_ems_int67(yt2_machine *machine);

#endif /* YT2_GAME_H */
