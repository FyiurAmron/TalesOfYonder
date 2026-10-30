#include "yt2/game.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Helper accessors                                                          */
/* ------------------------------------------------------------------------ */

static char *yt2_dos_string(yt2_machine *machine, uint16_t segment, uint16_t offset,
                            char *buffer, size_t size)
{
    size_t index = 0;
    while (index + 1u < size) {
        uint8_t value = machine->mem.bytes[yt2_flat(segment, (uint16_t)(offset + index))];
        buffer[index++] = (char)value;
        if (value == 0u) {
            break;
        }
    }
    buffer[index - 1u] = '\0';
    return buffer;
}

static void yt2_dos_result(yt2_machine *machine, uint16_t ax, int carry)
{
    machine->r.ax = ax;
    yt2_cpu_set_flag(&machine->r, YT2_FLAG_CF, carry);
}

/* ------------------------------------------------------------------------ */
/* Keyboard type-ahead (what DOS int 21h 06h/0Ch drain)                     */
/* ------------------------------------------------------------------------ */

void yt2_key_flush(yt2_machine *machine)
{
    machine->key_head = 0;
    machine->key_tail = 0;
    machine->key_count = 0;
    machine->key_extended = 0;
    machine->key_scan = 0;
}

void yt2_key_push(yt2_machine *machine, uint8_t scan, uint8_t ascii)
{
    if (machine->key_count >= YT2_KEY_QUEUE) {
        return;
    }
    if (machine->trace_input) {
        fprintf(stderr, "yt2: input key scan=%02X ascii=%02X\n",
                (unsigned)scan, (unsigned)ascii);
    }
    machine->key_queue[machine->key_tail] =
        (uint16_t)((uint16_t)ascii | ((uint16_t)scan << 8));
    machine->key_tail = (machine->key_tail + 1u) % YT2_KEY_QUEUE;
    machine->key_count++;
}

int yt2_key_pop(yt2_machine *machine, uint16_t *value)
{
    if (machine->key_count == 0u) {
        return 0;
    }
    *value = machine->key_queue[machine->key_head];
    machine->key_head = (machine->key_head + 1u) % YT2_KEY_QUEUE;
    machine->key_count--;
    return 1;
}

static const char *yt2_host_path(yt2_machine *machine, const char *dos_name,
                                 char *buffer, size_t size)
{
    if (machine->root[0] != '\0' && dos_name[0] != '/' &&
        !(dos_name[0] != '\0' && dos_name[1] == ':')) {
        snprintf(buffer, size, "%s/%s", machine->root, dos_name);
    } else {
        snprintf(buffer, size, "%s", dos_name);
    }
    return buffer;
}

static FILE *yt2_open_host(yt2_machine *machine, const char *dos_name, const char *mode,
                           int write_mode)
{
    char path[1024];
    FILE *stream;

    yt2_host_path(machine, dos_name, path, sizeof(path));
    stream = fopen(path, mode);
    if (stream == NULL && write_mode == 0) {
        size_t index;
        for (index = 0; path[index] != '\0'; index++) {
            if (path[index] >= 'A' && path[index] <= 'Z') {
                path[index] = (char)(path[index] - 'A' + 'a');
            }
        }
        stream = fopen(path, mode);
    }
    return stream;
}

static yt2_dos_file *yt2_dos_file_get(yt2_machine *machine, uint16_t handle)
{
    if (handle < 5u || handle >= 5u + YT2_DOS_FILES) {
        return NULL;
    }
    if (!machine->files[handle - 5u].used) {
        return NULL;
    }
    return &machine->files[handle - 5u];
}

static uint16_t yt2_dos_file_add(yt2_machine *machine, FILE *stream)
{
    unsigned index;
    for (index = 0; index < YT2_DOS_FILES; index++) {
        if (!machine->files[index].used) {
            machine->files[index].used = 1;
            machine->files[index].stream = stream;
            return (uint16_t)(5u + index);
        }
    }
    return 0xFFFFu;
}

/* ------------------------------------------------------------------------ */
/* Video BIOS                                                               */
/* ------------------------------------------------------------------------ */

static void yt2_video_set_default_palette(yt2_machine *machine)
{
    unsigned index;
    machine->video_dirty = 1;
    for (index = 0; index < 256u; index++) {
        machine->palette[index * 3u + 0u] = (uint8_t)(index >> 2);
        machine->palette[index * 3u + 1u] = (uint8_t)(index >> 2);
        machine->palette[index * 3u + 2u] = (uint8_t)(index >> 2);
    }
}

int yt2_video_int10(yt2_machine *machine)
{
    uint8_t ah = (uint8_t)(machine->r.ax >> 8);
    uint8_t al = (uint8_t)(machine->r.ax & 0xFFu);

    switch (ah) {
    case 0x00u:
        if (al == 0x13u || al == 0x03u || al == 0x00u) {
            machine->video_mode = al;
            machine->vbe_mode = 0;
            machine->vbe_bank_active = -1;
            machine->vbe_wait_palette = 0;
            machine->vbe_palette_bytes = 0;
            if (al == 0x13u) {
                memset(machine->mem.bytes + ((uint32_t)YT2_SCREEN_SEG << 4), 0, 64000u);
                yt2_video_set_default_palette(machine);
                machine->dac_index = 0;
                machine->dac_phase = 0;
                machine->video_dirty = 1;
                machine->present_force = 1;
                yt2_video_force_keyframe(&machine->video);
            }
        }
        break;
    case 0x10u:
        if (al == 0x17u) {
            /* set block of DAC registers: ES:DX table, CX count, BX start */
            uint16_t count = machine->r.cx;
            uint16_t start = machine->r.bx;
            uint16_t offset = machine->r.dx;
            uint16_t index;
            for (index = 0; index < count; index++) {
                unsigned target = (unsigned)((start + index) & 0xFFu) * 3u;
                uint32_t source = yt2_flat(machine->r.es, (uint16_t)(offset + index * 3u));
                machine->palette[target + 0u] = (uint8_t)(machine->mem.bytes[source + 0u] & 0x3Fu);
                machine->palette[target + 1u] = (uint8_t)(machine->mem.bytes[source + 1u] & 0x3Fu);
                machine->palette[target + 2u] = (uint8_t)(machine->mem.bytes[source + 2u] & 0x3Fu);
                /* The block write is a DAC write too: keep v_PalCurrent in
                   step (see the 3C9 handler in game_machine.c). */
                memcpy(machine->mem.bytes + ((uint32_t)YT2_DGROUP_SEG << 4) +
                           0x4124u + target,
                       machine->palette + target, 3u);
            }
            if (machine->vbe_mode && machine->vbe_wait_palette &&
                count >= 256u) {
                machine->vbe_wait_palette = 0;
                machine->vbe_palette_bytes = 0;
            }
            machine->video_dirty = 1;
            /* The seam presents a palette block at the write, not at the
               next poll: scene palettes and fade "show" steps then reach the
               client in the same frame as the rows they belong to. */
            yt2_game_present(machine);
        }
        break;
    case 0x4Fu:
        if (al == 0x02u) {
            machine->r.ax = 0x004Fu;
            if (machine->r.bx == 0x0100u || machine->r.bx == 0x0101u) {
                machine->vbe_mode = 1;
                machine->video_mode = machine->r.bx;
                memset(machine->vbe_bank, 0, sizeof(machine->vbe_bank));
                memset(machine->mem.bytes + ((uint32_t)YT2_SCREEN_SEG << 4),
                       0, 0x10000u);
                machine->vbe_bank_active = -1;
                machine->vbe_wait_palette = 1;
                machine->vbe_palette_bytes = 0;
                machine->vbe_palette_deadline = machine->ticks + 60u;
                machine->video_dirty = 1;
                machine->present_force = 1;
                yt2_video_force_keyframe(&machine->video);
                /* Blank the client now.  The decoder writes the hidden banks
                   first and loads the palette last; the rows wait for it. */
                yt2_game_present(machine);
            }
        } else if (al == 0x05u) {
            uint16_t bank = machine->r.dx;
            if (machine->vbe_mode) {
                uint32_t screen = (uint32_t)YT2_SCREEN_SEG << 4;
                unsigned banks = machine->video_mode == 0x0101u ? 5u : 4u;

                if (bank < banks) {
                    if (machine->vbe_bank_active >= 0 &&
                        machine->vbe_bank_active < (int)YT2_VBE_BANKS) {
                        memcpy(machine->vbe_bank[machine->vbe_bank_active],
                               machine->mem.bytes + screen, 0x10000u);
                    }
                    memcpy(machine->mem.bytes + screen, machine->vbe_bank[bank],
                           0x10000u);
                    machine->vbe_bank_active = (int)bank;
                }
            }
        }
        break;
    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Clock                                                                    */
/* ------------------------------------------------------------------------ */

int yt2_clock_int1a(yt2_machine *machine)
{
    if ((uint8_t)(machine->r.ax >> 8) == 0x00u) {
        uint32_t count = machine->ticks % 0x1800B0u;
        machine->r.cx = (uint16_t)(count >> 16);
        machine->r.dx = (uint16_t)(count & 0xFFFFu);
        machine->r.ax = 0;
        if (machine->trace_api) {
            fprintf(stderr, "yt2: int 1Ah tick=%u dx=%04X int1c=%04X:%04X\n",
                    machine->ticks, machine->r.dx,
                    machine->int1c_cs, machine->int1c_ip);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Mouse                                                                    */
/* ------------------------------------------------------------------------ */

static uint16_t yt2_mouse_clamp_x(uint16_t x)
{
    return x > 319u ? 319u : x;
}

static uint16_t yt2_mouse_clamp_y(uint16_t y)
{
    return y > 199u ? 199u : y;
}

static uint16_t yt2_mouse_dgroup_get(uint16_t offset)
{
    return YT2_MEM16(YT2_DGROUP_SEG, offset);
}

static void yt2_mouse_event(yt2_machine *machine, uint16_t mask)
{
    uint16_t cs = machine->mouse_handler_cs;
    uint16_t ip = machine->mouse_handler_ip;

    if ((cs | ip) == 0u || (machine->mouse_mask & mask) == 0u) {
        return;
    }
    if (!yt2_machine_call_safe(machine)) {
        return;                     /* the next poll repeats nothing: drop */
    }
    if (machine->trace_input) {
        fprintf(stderr, "yt2: input mouse mask=%04X at %u,%u buttons=%04X\n",
                (unsigned)mask, (unsigned)machine->mouse_x,
                (unsigned)machine->mouse_y, (unsigned)machine->mouse_buttons);
    }
    machine->r.ax = mask;
    machine->r.bx = machine->mouse_buttons;
    machine->r.cx = machine->mouse_x;
    machine->r.dx = machine->mouse_y;
    yt2_machine_call(machine, cs, ip);
}

void yt2_mouse_move(yt2_machine *machine, int16_t dx, int16_t dy)
{
    int32_t scaled;
    int32_t x;
    int32_t y;

    if (dx == 0 && dy == 0) {
        return;
    }
    scaled = (int32_t)dx * (int32_t)machine->mouse_ratio_x + machine->mouse_frac_x;
    machine->mouse_frac_x = (int16_t)(scaled & 7);
    machine->mouse_dx = (int16_t)(machine->mouse_dx + (int16_t)(scaled >> 3));
    scaled = (int32_t)dy * (int32_t)machine->mouse_ratio_y + machine->mouse_frac_y;
    machine->mouse_frac_y = (int16_t)(scaled & 7);
    machine->mouse_dy = (int16_t)(machine->mouse_dy + (int16_t)(scaled >> 3));

    x = (int32_t)machine->mouse_x + dx;
    y = (int32_t)machine->mouse_y + dy;
    if (x < 0) {
        x = 0;
    } else if (x > 319) {
        x = 319;
    }
    if (y < 0) {
        y = 0;
    } else if (y > 199) {
        y = 199;
    }
    machine->mouse_x = (uint16_t)x;
    machine->mouse_y = (uint16_t)y;
    yt2_mouse_event(machine, 0x0001u);
}

void yt2_mouse_abs(yt2_machine *machine, uint16_t x, uint16_t y)
{
    uint16_t current_x;
    uint16_t current_y;

    x = yt2_mouse_clamp_x(x);
    y = yt2_mouse_clamp_y(y);
    current_x = yt2_mouse_dgroup_get(0x0F1Cu);   /* v_MouseNewX */
    current_y = yt2_mouse_dgroup_get(0x0F1Eu);   /* v_MouseNewY */
    machine->mouse_dx = (int16_t)(x - current_x);
    machine->mouse_dy = (int16_t)(y - current_y);
    machine->mouse_x = x;
    machine->mouse_y = y;
    yt2_mouse_event(machine, 0x0001u);
    /* The game clamps its own cursor: keep the click position in step. */
    machine->mouse_x = yt2_mouse_dgroup_get(0x0F1Cu);
    machine->mouse_y = yt2_mouse_dgroup_get(0x0F1Eu);
}

void yt2_mouse_button(yt2_machine *machine, uint8_t button, uint8_t down)
{
    uint16_t bit;
    uint16_t mask;

    if (button == 0u) {
        bit = YT2_MOUSE_LEFT;
        mask = down ? 0x0002u : 0x0004u;
    } else if (button == 2u) {
        bit = YT2_MOUSE_RIGHT;
        mask = down ? 0x0008u : 0x0010u;
    } else {
        return;                     /* middle: the game has no use for it */
    }
    if (down) {
        machine->mouse_buttons |= bit;
    } else {
        machine->mouse_buttons &= (uint16_t)~bit;
    }
    yt2_mouse_event(machine, mask);
}

int yt2_mouse_int33(yt2_machine *machine)
{
    uint16_t ax = machine->r.ax;

    switch (ax) {
    case 0x0000u:
        /* Reset: the game's start-up probe and its exit path.  A handler
           means the game is leaving, so stop delivering events. */
        machine->mouse_handler_ip = 0;
        machine->mouse_handler_cs = 0;
        machine->mouse_mask = 0;
        machine->r.ax = 0xFFFFu;
        machine->r.bx = 2;
        break;
    case 0x0003u:
        machine->r.bx = machine->mouse_buttons;
        machine->r.cx = machine->mouse_x;
        machine->r.dx = machine->mouse_y;
        break;
    case 0x0004u:
        machine->mouse_x = yt2_mouse_clamp_x(machine->r.cx);
        machine->mouse_y = yt2_mouse_clamp_y(machine->r.dx);
        break;
    case 0x000Bu:
        machine->r.cx = (uint16_t)machine->mouse_dx;
        machine->r.dx = (uint16_t)machine->mouse_dy;
        machine->mouse_dx = 0;
        machine->mouse_dy = 0;
        machine->r.bx = machine->mouse_buttons;
        break;
    case 0x000Cu:
        machine->mouse_handler_ip = machine->r.dx;
        machine->mouse_handler_cs = machine->r.es;
        machine->mouse_mask = machine->r.cx;
        if (machine->trace_input) {
            fprintf(stderr, "yt2: input mouse handler %04X:%04X mask=%04X\n",
                    (unsigned)machine->r.es, (unsigned)machine->r.dx,
                    (unsigned)machine->r.cx);
        }
        break;
    case 0x000Fu:
        machine->mouse_ratio_x = machine->r.cx;
        machine->mouse_ratio_y = machine->r.dx;
        break;
    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* EMS                                                                      */
/* ------------------------------------------------------------------------ */

static uint16_t yt2_ems_alloc_page(yt2_machine *machine)
{
    uint16_t page;
    if (machine->ems_free_count == 0u) {
        return 0xFFFFu;
    }
    page = machine->ems_free_page[machine->ems_free_next];
    machine->ems_free_next = (uint16_t)((machine->ems_free_next + 1u) % YT2_EMS_MAX_PAGES);
    machine->ems_free_count--;
    machine->ems_free_pages = machine->ems_free_count;
    machine->ems_page_owner[page] = 1;
    memset(machine->ems_pool + (uint32_t)page * YT2_EMS_PAGE_BYTES, 0, YT2_EMS_PAGE_BYTES);
    return page;
}

static void yt2_ems_flush_window(yt2_machine *machine)
{
    uint32_t screen = (uint32_t)YT2_EMS_FRAME_SEG << 4;
    unsigned slot;
    for (slot = 0; slot < 4u; slot++) {
        uint16_t handle = machine->ems_window_handle[slot];
        uint16_t logical;
        if (handle == 0xFFFFu || !machine->ems_handle[handle].used) {
            continue;
        }
        logical = machine->ems_window_page[slot];
        if (logical < machine->ems_handle[handle].pages) {
            uint16_t page = machine->ems_handle[handle].page[logical];
            memcpy(machine->ems_pool + (uint32_t)page * YT2_EMS_PAGE_BYTES,
                   machine->mem.bytes + screen + (uint32_t)slot * YT2_EMS_PAGE_BYTES,
                   YT2_EMS_PAGE_BYTES);
        }
    }
}

static void yt2_ems_load_window(yt2_machine *machine)
{
    uint32_t screen = (uint32_t)YT2_EMS_FRAME_SEG << 4;
    unsigned slot;
    for (slot = 0; slot < 4u; slot++) {
        uint16_t handle = machine->ems_window_handle[slot];
        uint16_t logical = machine->ems_window_page[slot];
        if (handle != 0xFFFFu && machine->ems_handle[handle].used &&
            logical < machine->ems_handle[handle].pages) {
            uint16_t page = machine->ems_handle[handle].page[logical];
            memcpy(machine->mem.bytes + screen + (uint32_t)slot * YT2_EMS_PAGE_BYTES,
                   machine->ems_pool + (uint32_t)page * YT2_EMS_PAGE_BYTES,
                   YT2_EMS_PAGE_BYTES);
        } else {
            memset(machine->mem.bytes + screen + (uint32_t)slot * YT2_EMS_PAGE_BYTES,
                   0, YT2_EMS_PAGE_BYTES);
        }
    }
}

static void yt2_ems_map(yt2_machine *machine, unsigned physical, uint16_t handle,
                        uint16_t logical)
{
    if (physical < 4u) {
        machine->ems_window_handle[physical] = handle;
        machine->ems_window_page[physical] = logical;
    }
}

int yt2_ems_int67(yt2_machine *machine)
{
    uint8_t ah = (uint8_t)(machine->r.ax >> 8);
    uint8_t al = (uint8_t)(machine->r.ax & 0xFFu);

    switch (ah) {
    case 0x40u:
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        break;
    case 0x41u:
        machine->r.bx = YT2_EMS_FRAME_SEG;
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        break;
    case 0x42u:
        machine->r.bx = machine->ems_free_pages;
        machine->r.dx = machine->ems_total_pages;
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        break;
    case 0x43u: {
        uint16_t pages = machine->r.bx;
        unsigned handle;
        for (handle = 1; handle < YT2_EMS_HANDLES; handle++) {
            if (!machine->ems_handle[handle].used) {
                break;
            }
        }
        if (handle == YT2_EMS_HANDLES || pages > machine->ems_free_pages) {
            yt2_r8_set(&machine->r, YT2_R8_AH, (handle == YT2_EMS_HANDLES) ? 0x85u : 0x88u);
            break;
        }
        machine->ems_handle[handle].used = 1;
        machine->ems_handle[handle].pages = pages;
        {
            uint16_t index;
            for (index = 0; index < pages; index++) {
                uint16_t page = yt2_ems_alloc_page(machine);
                machine->ems_handle[handle].page[index] = page;
            }
        }
        machine->r.dx = (uint16_t)handle;
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        break;
    }
    case 0x45u: {
        uint16_t handle = machine->r.dx;
        if (handle < YT2_EMS_HANDLES && machine->ems_handle[handle].used) {
            uint16_t index;
            for (index = 0; index < machine->ems_handle[handle].pages; index++) {
                uint16_t page = machine->ems_handle[handle].page[index];
                machine->ems_page_owner[page] = 0;
                machine->ems_free_page[machine->ems_free_next] = page;
                machine->ems_free_next = (uint16_t)((machine->ems_free_next + 1u) % YT2_EMS_MAX_PAGES);
                machine->ems_free_count++;
            }
            machine->ems_handle[handle].used = 0;
            machine->ems_handle[handle].pages = 0;
            machine->ems_free_pages = machine->ems_free_count;
            yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        } else {
            yt2_r8_set(&machine->r, YT2_R8_AH, 0x83u);
        }
        break;
    }
    case 0x46u:
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        yt2_r8_set(&machine->r, YT2_R8_AL, 0x40u);
        break;
    case 0x4Eu:
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        break;
    case 0x4Fu:
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        break;
    case 0x50u: {
        uint16_t handle = machine->r.dx;
        uint16_t count = machine->r.cx;
        uint16_t offset = machine->r.si;
        uint16_t index;
        if (handle >= YT2_EMS_HANDLES || !machine->ems_handle[handle].used) {
            yt2_r8_set(&machine->r, YT2_R8_AH, 0x83u);
            break;
        }
        if (al != 0u) {
            yt2_r8_set(&machine->r, YT2_R8_AH, 0x8Fu);
            break;
        }
        yt2_r8_set(&machine->r, YT2_R8_AH, 0);
        /* Flush the current window, apply the list, then load it. */
        yt2_ems_flush_window(machine);
        for (index = 0; index < count; index++) {
            uint32_t at = yt2_flat(machine->r.ds, (uint16_t)(offset + index * 4u));
            uint16_t logical = (uint16_t)(machine->mem.bytes[at] | (machine->mem.bytes[at + 1u] << 8));
            uint16_t physical = (uint16_t)(machine->mem.bytes[at + 2u] | (machine->mem.bytes[at + 3u] << 8));
            if (logical >= machine->ems_handle[handle].pages && logical != 0xFFFFu) {
                yt2_r8_set(&machine->r, YT2_R8_AH, 0x8Au);
                break;
            }
            if (physical >= 4u && physical != 0xFFFFu) {
                yt2_r8_set(&machine->r, YT2_R8_AH, 0x8Bu);
                break;
            }
            yt2_ems_map(machine, physical, handle, logical);
        }
        yt2_ems_load_window(machine);
        break;
    }
    default:
        yt2_r8_set(&machine->r, YT2_R8_AH, 0x84u);
        break;
    }
    if (machine->trace_api) {
        fprintf(stderr, "yt2: int 67h ah=%02X -> ah=%02X ax=%04X bx=%04X cx=%04X dx=%04X\n",
                ah, (unsigned)(machine->r.ax >> 8), machine->r.ax, machine->r.bx,
                machine->r.cx, machine->r.dx);
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Presentation log                                                          */
/* ------------------------------------------------------------------------ */

static uint64_t yt2_hash64(uint64_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = data;
    size_t index;
    for (index = 0; index < size; index++) {
        hash ^= bytes[index];
        hash *= 0x100000001B3ull;
    }
    return hash;
}

void yt2_game_present(yt2_machine *machine)
{
    const uint8_t *framebuffer =
        machine->mem.bytes + ((uint32_t)YT2_SCREEN_SEG << 4);
    uint64_t fb_hash;
    uint64_t pal_hash;
    int forced = machine->present_force;

    machine->present_force = 0;
    /* The SEAM polls the link inside every present pass. */
    if (machine->link_ready) {
        yt2_link_poll(machine);
    }
    if (machine->vbe_mode && machine->vbe_wait_palette &&
        machine->ticks >= machine->vbe_palette_deadline) {
        /* A VBE screen that never loads a palette must not stay blank
           forever; show it with the palette at hand after a short grace. */
        machine->vbe_wait_palette = 0;
        machine->vbe_palette_bytes = 0;
        machine->video_dirty = 1;
    }
    if (!machine->video_dirty) {
        return;
    }
    /* SEAM_PASSES_PER_TICK: the seam lets at most three passes run in one
       BIOS tick.  A fade loop writes the DAC faster than the display can
       show it, so the gate keeps the stream at the original's sampling rate
       and stops a tight draw loop from starving the game.  A resync/keyframe
       pass bypasses the budget, as seam_forcepass does. */
    if (!forced) {
        if (machine->present_tick != machine->ticks) {
            machine->present_tick = machine->ticks;
            machine->present_passes = 0u;
        }
        if (machine->present_passes >= YT2_PASSES_PER_TICK) {
            return;
        }
        machine->present_passes++;
    }
    machine->video_dirty = 0;
    memcpy(machine->present_frame, framebuffer, 64000u);
    memcpy(machine->present_palette, machine->palette, 768u);
    fb_hash = yt2_hash64(0xCBF29CE484222325ull, framebuffer, 64000u);
    pal_hash = yt2_hash64(0xCBF29CE484222325ull, machine->palette, 768u);
    if (machine->present_have_last && fb_hash == machine->present_last_fb &&
        pal_hash == machine->present_last_pal && !forced &&
        !(machine->link_ready && machine->video.force_full != 0u)) {
        return;
    }
    machine->present_have_last = 1;
    machine->present_last_fb = fb_hash;
    machine->present_last_pal = pal_hash;
    if (machine->present_log != NULL) {
        fprintf(machine->present_log, "%llu %u %016llX %016llX\n",
                machine->present_frames, machine->ticks,
                (unsigned long long)fb_hash, (unsigned long long)pal_hash);
    }
    if (machine->present_dir[0] != '\0') {
        char path[1024];
        FILE *stream;
        snprintf(path, sizeof(path), "%s/frame_%05llu.idx",
                 machine->present_dir, machine->present_frames);
        stream = fopen(path, "wb");
        if (stream != NULL) {
            fwrite(machine->present_frame, 1, 64000u, stream);
            fclose(stream);
        }
        snprintf(path, sizeof(path), "%s/frame_%05llu.pal",
                 machine->present_dir, machine->present_frames);
        stream = fopen(path, "wb");
        if (stream != NULL) {
            fwrite(machine->present_palette, 1, 768u, stream);
            fclose(stream);
        }
    }
    if (machine->link_ready) {
        yt2_link_present(machine);
    }
    machine->present_frames++;
}

/* ------------------------------------------------------------------------ */
/* DOS                                                                      */
/* ------------------------------------------------------------------------ */

static void yt2_dos_block_insert(yt2_machine *machine, unsigned index,
                                 uint16_t segment, uint16_t paragraphs, int used)
{
    if (machine->dos_block_count >= YT2_DOS_BLOCKS) {
        return;
    }
    memmove(&machine->dos_blocks[index + 1u], &machine->dos_blocks[index],
            (machine->dos_block_count - index) * sizeof(machine->dos_blocks[0]));
    machine->dos_blocks[index].segment = segment;
    machine->dos_blocks[index].paragraphs = paragraphs;
    machine->dos_blocks[index].used = used;
    machine->dos_block_count++;
}

static void yt2_dos_block_coalesce(yt2_machine *machine)
{
    unsigned index = 0;
    while (index + 1u < machine->dos_block_count) {
        yt2_dos_block *left = &machine->dos_blocks[index];
        yt2_dos_block *right = &machine->dos_blocks[index + 1u];
        if (!left->used && !right->used &&
            (uint32_t)left->segment + left->paragraphs == right->segment) {
            left->paragraphs = (uint16_t)(left->paragraphs + right->paragraphs);
            memmove(right, right + 1,
                    (machine->dos_block_count - index - 2u) * sizeof(machine->dos_blocks[0]));
            machine->dos_block_count--;
        } else {
            index++;
        }
    }
}

static int yt2_dos_alloc(yt2_machine *machine, uint16_t paragraphs, uint16_t *segment)
{
    unsigned index;
    for (index = 0; index < machine->dos_block_count; index++) {
        yt2_dos_block *block = &machine->dos_blocks[index];
        if (block->used || block->paragraphs < paragraphs) {
            continue;
        }
        if (block->paragraphs > paragraphs) {
            yt2_dos_block_insert(machine, index + 1u,
                                 (uint16_t)(block->segment + paragraphs),
                                 (uint16_t)(block->paragraphs - paragraphs), 0);
        }
        block->used = 1;
        block->paragraphs = paragraphs;
        *segment = block->segment;
        return 1;
    }
    return 0;
}

static int yt2_dos_free(yt2_machine *machine, uint16_t segment)
{
    unsigned index;
    for (index = 0; index < machine->dos_block_count; index++) {
        if (machine->dos_blocks[index].used &&
            machine->dos_blocks[index].segment == segment) {
            machine->dos_blocks[index].used = 0;
            yt2_dos_block_coalesce(machine);
            return 1;
        }
    }
    return 0;
}

static int yt2_dos_resize(yt2_machine *machine, uint16_t segment, uint16_t paragraphs,
                          uint16_t *maximum)
{
    unsigned index;
    for (index = 0; index < machine->dos_block_count; index++) {
        yt2_dos_block *block = &machine->dos_blocks[index];
        uint16_t available;
        if (!block->used || block->segment != segment) {
            continue;
        }
        available = block->paragraphs;
        if (index + 1u < machine->dos_block_count &&
            !machine->dos_blocks[index + 1u].used) {
            available = (uint16_t)(available + machine->dos_blocks[index + 1u].paragraphs);
        }
        if (maximum != NULL) {
            *maximum = available;
            return 1;
        }
        if (paragraphs > available) {
            return 0;
        }
        if (paragraphs < block->paragraphs) {
            yt2_dos_block_insert(machine, index + 1u,
                                 (uint16_t)(block->segment + paragraphs),
                                 (uint16_t)(block->paragraphs - paragraphs), 0);
            block->paragraphs = paragraphs;
        } else if (paragraphs > block->paragraphs) {
            yt2_dos_block *next = &machine->dos_blocks[index + 1u];
            block->paragraphs = (uint16_t)(block->paragraphs + next->paragraphs);
            memmove(next, next + 1,
                    (machine->dos_block_count - index - 2u) * sizeof(machine->dos_blocks[0]));
            machine->dos_block_count--;
            if (block->paragraphs > paragraphs) {
                yt2_dos_block_insert(machine, index + 1u,
                                     (uint16_t)(block->segment + paragraphs),
                                     (uint16_t)(block->paragraphs - paragraphs), 0);
                block->paragraphs = paragraphs;
            }
        }
        yt2_dos_block_coalesce(machine);
        return 1;
    }
    return 0;
}

static uint16_t yt2_dos_largest_free(const yt2_machine *machine)
{
    uint16_t largest = 0;
    unsigned index;
    for (index = 0; index < machine->dos_block_count; index++) {
        if (!machine->dos_blocks[index].used &&
            machine->dos_blocks[index].paragraphs > largest) {
            largest = machine->dos_blocks[index].paragraphs;
        }
    }
    return largest;
}

static void yt2_dos_open(yt2_machine *machine)
{
    char name[256];
    FILE *stream;
    uint8_t mode = (uint8_t)(machine->r.ax & 0xFFu);
    uint16_t handle;

    yt2_dos_string(machine, machine->r.ds, machine->r.dx, name, sizeof(name));
    stream = yt2_open_host(machine, name, (mode & 0x02u) ? "r+b" : "rb", mode & 0x02u);
    if (stream == NULL) {
        yt2_dos_result(machine, 2u, 1);
        return;
    }
    handle = yt2_dos_file_add(machine, stream);
    if (handle == 0xFFFFu) {
        fclose(stream);
        yt2_dos_result(machine, 4u, 1);
        return;
    }
    yt2_dos_result(machine, handle, 0);
}

static void yt2_dos_create(yt2_machine *machine)
{
    char name[256];
    FILE *stream;
    uint16_t handle;

    yt2_dos_string(machine, machine->r.ds, machine->r.dx, name, sizeof(name));
    stream = yt2_open_host(machine, name, "w+b", 1);
    if (stream == NULL) {
        yt2_dos_result(machine, 3u, 1);
        return;
    }
    handle = yt2_dos_file_add(machine, stream);
    if (handle == 0xFFFFu) {
        fclose(stream);
        yt2_dos_result(machine, 4u, 1);
        return;
    }
    yt2_dos_result(machine, handle, 0);
}

static void yt2_dos_read(yt2_machine *machine)
{
    yt2_dos_file *file = yt2_dos_file_get(machine, machine->r.bx);
    uint16_t count = machine->r.cx;
    uint16_t offset = machine->r.dx;
    uint32_t destination;
    size_t got;

    if (file == NULL) {
        yt2_dos_result(machine, 6u, 1);
        return;
    }
    destination = yt2_flat(machine->r.ds, offset);
    got = fread(machine->mem.bytes + destination, 1, count, file->stream);
    yt2_dos_result(machine, (uint16_t)got, ferror(file->stream) ? 1 : 0);
}

static void yt2_dos_write(yt2_machine *machine)
{
    yt2_dos_file *file = yt2_dos_file_get(machine, machine->r.bx);
    uint16_t count = machine->r.cx;
    uint16_t offset = machine->r.dx;
    uint32_t source;
    size_t put;

    if (file == NULL) {
        yt2_dos_result(machine, 6u, 1);
        return;
    }
    source = yt2_flat(machine->r.ds, offset);
    put = fwrite(machine->mem.bytes + source, 1, count, file->stream);
    yt2_dos_result(machine, (uint16_t)put, put != count ? 1 : 0);
}

static void yt2_dos_seek(yt2_machine *machine)
{
    yt2_dos_file *file = yt2_dos_file_get(machine, machine->r.bx);
    int whence = (int)(machine->r.ax & 0xFFu);
    int32_t offset = (int32_t)(((uint32_t)machine->r.cx << 16) | machine->r.dx);
    int base;

    if (file == NULL) {
        yt2_dos_result(machine, 6u, 1);
        return;
    }
    if (whence == 0) {
        base = SEEK_SET;
    } else if (whence == 1) {
        base = SEEK_CUR;
    } else if (whence == 2) {
        base = SEEK_END;
    } else {
        yt2_dos_result(machine, 1u, 1);
        return;
    }
    if (fseek(file->stream, offset, base) != 0) {
        yt2_dos_result(machine, 1u, 1);
        return;
    }
    {
        long position = ftell(file->stream);
        machine->r.ax = (uint16_t)((uint32_t)position & 0xFFFFu);
        machine->r.dx = (uint16_t)(((uint32_t)position >> 16) & 0xFFFFu);
        yt2_cpu_set_flag(&machine->r, YT2_FLAG_CF, 0);
    }
}

static void yt2_dos_close(yt2_machine *machine)
{
    yt2_dos_file *file = yt2_dos_file_get(machine, machine->r.bx);
    if (file == NULL) {
        yt2_dos_result(machine, 6u, 1);
        return;
    }
    fclose(file->stream);
    file->stream = NULL;
    file->used = 0;
    yt2_dos_result(machine, 0u, 0);
}

static void yt2_dos_delete(yt2_machine *machine)
{
    char name[256];
    char path[1024];
    yt2_dos_string(machine, machine->r.ds, machine->r.dx, name, sizeof(name));
    yt2_host_path(machine, name, path, sizeof(path));
    if (remove(path) != 0) {
        yt2_dos_result(machine, 2u, 1);
    } else {
        yt2_dos_result(machine, 0u, 0);
    }
}

int yt2_dos_int21(yt2_machine *machine)
{
    uint8_t ah = (uint8_t)(machine->r.ax >> 8);
    uint8_t al = (uint8_t)(machine->r.ax & 0xFFu);

    switch (ah) {
    case 0x06u:
        if ((uint8_t)(machine->r.dx & 0xFFu) == 0xFFu) {
            uint16_t entry;

            /* The SEAM build presents at the entry of the input poll. */
            yt2_game_present(machine);
            /* Direct console input: type-ahead first, extended keys as a
               zero byte followed by the scancode (what fn_PollInput reads). */
            if (machine->key_extended) {
                machine->key_extended = 0;
                machine->r.ax = machine->key_scan;
                if (machine->trace_input) {
                    fprintf(stderr, "yt2: input deliver scan=%02X\n",
                            (unsigned)machine->key_scan);
                }
                yt2_cpu_set_flag(&machine->r, YT2_FLAG_ZF, 0);
            } else if (yt2_key_pop(machine, &entry)) {
                uint8_t ascii = (uint8_t)(entry & 0xFFu);

                machine->r.ax = ascii;
                if (ascii == 0u) {
                    machine->key_extended = 1;
                    machine->key_scan = (uint8_t)(entry >> 8);
                }
                if (machine->trace_input) {
                    fprintf(stderr, "yt2: input deliver ascii=%02X scan=%02X\n",
                            (unsigned)ascii, (unsigned)(entry >> 8));
                }
                yt2_cpu_set_flag(&machine->r, YT2_FLAG_ZF, 0);
            } else {
                machine->r.ax = 0;
                yt2_cpu_set_flag(&machine->r, YT2_FLAG_ZF, 1);
            }
        } else {
            putchar(al);
            fflush(stdout);
        }
        break;
    case 0x0Cu:
        yt2_key_flush(machine);
        machine->r.ax = 0;
        break;
    case 0x09u: {
        uint16_t offset = machine->r.dx;
        unsigned count = 0;
        while (count < 4096u) {
            uint8_t value = machine->mem.bytes[yt2_flat(machine->r.ds, (uint16_t)(offset + count))];
            if (value == '$') {
                break;
            }
            putchar(value);
            count++;
        }
        fflush(stdout);
        break;
    }
    case 0x25u:
        if (al == 0x1Cu) {
            machine->int1c_ip = machine->r.dx;
            machine->int1c_cs = machine->r.ds;
        }
        break;
    case 0x35u:
        if (al == 0x1Cu) {
            machine->r.bx = machine->int1c_ip;
            machine->r.es = machine->int1c_cs;
        } else if (al == 0x67u) {
            machine->r.bx = 0;
            machine->r.es = 0xE000u;
        } else if (al == 0x33u) {
            /* The game probes for a mouse driver and rejects a null vector
               or an IRET stub; the sentinel paragraph starts with zeroes. */
            machine->r.bx = 0;
            machine->r.es = YT2_SENTINEL_SEG;
        } else {
            machine->r.bx = 0;
            machine->r.es = 0;
        }
        break;
    case 0x48u: {
        uint16_t segment = 0;
        if (machine->r.bx == 0xFFFFu) {
            machine->r.ax = 8;
            machine->r.bx = yt2_dos_largest_free(machine);
            yt2_cpu_set_flag(&machine->r, YT2_FLAG_CF, 1);
            break;
        }
        if (yt2_dos_alloc(machine, machine->r.bx, &segment)) {
            machine->r.ax = segment;
            yt2_cpu_set_flag(&machine->r, YT2_FLAG_CF, 0);
        } else {
            machine->r.ax = 8;
            machine->r.bx = yt2_dos_largest_free(machine);
            yt2_cpu_set_flag(&machine->r, YT2_FLAG_CF, 1);
        }
        if (machine->trace_api) {
            fprintf(stderr, "yt2: dos alloc %u paras -> %s %04X (blocks=%u)\n",
                    machine->r.bx, yt2_cpu_get_flag(&machine->r, YT2_FLAG_CF) ? "fail" : "seg",
                    machine->r.ax, machine->dos_block_count);
        }
        break;
    }
    case 0x49u:
        if (yt2_dos_free(machine, machine->r.es)) {
            yt2_dos_result(machine, 0u, 0);
        } else {
            yt2_dos_result(machine, 9u, 1);
        }
        if (machine->trace_api) {
            fprintf(stderr, "yt2: dos free %04X -> %s (blocks=%u)\n",
                    machine->r.es, yt2_cpu_get_flag(&machine->r, YT2_FLAG_CF) ? "fail" : "ok",
                    machine->dos_block_count);
        }
        break;
    case 0x4Au:
        if (machine->r.bx == 0xFFFFu) {
            uint16_t maximum = 0;
            if (yt2_dos_resize(machine, machine->r.es, 0u, &maximum)) {
                machine->r.bx = maximum;
                yt2_dos_result(machine, 0u, 0);
            } else {
                machine->r.ax = 9;
                yt2_cpu_set_flag(&machine->r, YT2_FLAG_CF, 1);
            }
        } else if (yt2_dos_resize(machine, machine->r.es, machine->r.bx, NULL)) {
            yt2_dos_result(machine, 0u, 0);
        } else {
            yt2_dos_result(machine, 9u, 1);
        }
        if (machine->trace_api) {
            fprintf(stderr, "yt2: dos resize %04X %u paras -> %s (blocks=%u)\n",
                    machine->r.es, machine->r.bx,
                    yt2_cpu_get_flag(&machine->r, YT2_FLAG_CF) ? "fail" : "ok",
                    machine->dos_block_count);
        }
        break;
    case 0x4Cu:
        machine->exited = 1;
        machine->exit_code = al;
        break;
    case 0x4Eu:
        /* No save files in the staged directory yet. */
        yt2_dos_result(machine, 2u, 1);
        break;
    case 0x51u:
        machine->r.bx = YT2_PSP_SEG;
        break;
    case 0x62u:
        machine->r.bx = YT2_PSP_SEG;
        break;
    case 0x2Cu:
        machine->r.cx = 0;
        machine->r.dx = 0;
        break;
    case 0x3Du:
        yt2_dos_open(machine);
        break;
    case 0x3Cu:
        yt2_dos_create(machine);
        break;
    case 0x3Eu:
        yt2_dos_close(machine);
        break;
    case 0x3Fu:
        yt2_dos_read(machine);
        break;
    case 0x40u:
        yt2_dos_write(machine);
        break;
    case 0x41u:
        yt2_dos_delete(machine);
        break;
    case 0x42u:
        yt2_dos_seek(machine);
        break;
    default:
        if (machine->trace_api) {
            fprintf(stderr, "yt2: unhandled int 21h ah=%02X ax=%04X bx=%04X cx=%04X dx=%04X\n",
                    ah, machine->r.ax, machine->r.bx, machine->r.cx, machine->r.dx);
        }
        yt2_dos_result(machine, 0u, 0);
        break;
    }
    return 0;
}
