#include "yt2/game.h"

#include <stdio.h>
#include <string.h>

/*
 * The live folder link for the transliterated game.
 *
 * The M2 seam already owns the transport, the v0 framing and the display
 * state, so this file is only the glue: one present pass copies the game's
 * framebuffer and DAC into the video state and lets it emit the changed rows,
 * and the input callbacks feed the DOS type-ahead queue and the INT 33h shim.
 * The game presents at the input-poll, tick-ISR, DAC-write and delay-tick
 * entry points, so fades and other DAC-paced effects become frames instead
 * of being collapsed into the next poll.  It also presents after the
 * full-screen REP MOVSW that flushes the work buffer to A000: a real card
 * shows those writes immediately, and the attract gate's shooting star runs
 * through one flush per step inside a single tick.  Without that sample the
 * whole streak falls between passes.
 */

static void yt2_link_on_key(void *context, uint8_t scan, uint8_t ascii)
{
    yt2_key_push((yt2_machine *)context, scan, ascii);
}

static void yt2_link_on_mouse_move(void *context, int16_t dx, int16_t dy)
{
    yt2_mouse_move((yt2_machine *)context, dx, dy);
}

static void yt2_link_on_mouse_abs(void *context, uint16_t x, uint16_t y)
{
    yt2_mouse_abs((yt2_machine *)context, x, y);
}

static void yt2_link_on_mouse_button(void *context, uint8_t button, uint8_t down)
{
    yt2_mouse_button((yt2_machine *)context, button, down);
}

static void yt2_link_on_keyframe(void *context)
{
    yt2_machine *machine = (yt2_machine *)context;

    /* A keyframe is a fresh start for the client: mode, full palette, every
       row.  video_dirty also gets the pass past the unchanged-frame gate. */
    machine->video_dirty = 1;
    machine->present_force = 1;
    yt2_video_force_keyframe(&machine->video);
}

int yt2_link_open(yt2_machine *machine)
{
    yt2_input_handlers handlers;

    if (machine->link) {
        return YT2_OK;
    }
    if (machine->root[0] != '\0') {
        snprintf(machine->link_output_path, sizeof(machine->link_output_path),
                 "%s/OUTPUT.BIN", machine->root);
        snprintf(machine->link_input_path, sizeof(machine->link_input_path),
                 "%s/INPUT.BIN", machine->root);
    } else {
        snprintf(machine->link_output_path, sizeof(machine->link_output_path),
                 "OUTPUT.BIN");
        snprintf(machine->link_input_path, sizeof(machine->link_input_path),
                 "INPUT.BIN");
    }
    if (yt2_transport_folder_init(&machine->transport, &machine->folder,
                                  machine->link_output_path,
                                  machine->link_input_path, 0) != YT2_OK) {
        fprintf(stderr, "yt2: cannot create %s\n", machine->link_output_path);
        return YT2_ERR;
    }

    memset(&handlers, 0, sizeof(handlers));
    handlers.key = yt2_link_on_key;
    handlers.mouse_move = yt2_link_on_mouse_move;
    handlers.mouse_abs = yt2_link_on_mouse_abs;
    handlers.mouse_button = yt2_link_on_mouse_button;
    handlers.keyframe_request = yt2_link_on_keyframe;
    yt2_service_init(&machine->service, &machine->transport, &handlers, machine);
    yt2_video_init(&machine->video, 320, 200, 8);

    machine->link = 1;
    machine->link_ready = 1;
    return YT2_OK;
}

static void yt2_link_read_script(yt2_machine *machine)
{
    char line[128];

    while (machine->input_script != NULL) {
        unsigned long tick = 0;
        char kind[16];
        int a = 0;
        int b = 0;

        if (fgets(line, sizeof(line), machine->input_script) == NULL) {
            fclose(machine->input_script);
            machine->input_script = NULL;
            return;
        }
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' ||
            line[0] == '\0') {
            continue;
        }
        if (sscanf(line, "%lu %15s %i %i", &tick, kind, &a, &b) != 4) {
            fprintf(stderr, "yt2: bad input script line: %s", line);
            continue;
        }
        machine->script_tick = (uint32_t)tick;
        if (strcmp(kind, "key") == 0) {
            machine->script_kind = YT2_SCRIPT_KEY;
        } else if (strcmp(kind, "abs") == 0) {
            machine->script_kind = YT2_SCRIPT_ABS;
        } else if (strcmp(kind, "move") == 0) {
            machine->script_kind = YT2_SCRIPT_MOVE;
        } else if (strcmp(kind, "button") == 0) {
            machine->script_kind = YT2_SCRIPT_BUTTON;
        } else {
            fprintf(stderr, "yt2: unknown input script kind %s\n", kind);
            continue;
        }
        machine->script_a = a;
        machine->script_b = b;
        machine->script_pending = 1;
        return;
    }
}

/* One scripted event per poll, released at its guest tick.  The script is a
   test harness for the gameplay gates (the live link's client writes its
   records in real time); the callbacks are the same ones the link uses. */
static void yt2_link_apply_script(yt2_machine *machine)
{
    if (!machine->script_pending) {
        yt2_link_read_script(machine);
    }
    if (!machine->script_pending || machine->ticks < machine->script_tick) {
        return;
    }
    switch (machine->script_kind) {
    case YT2_SCRIPT_KEY:
        yt2_key_push(machine, (uint8_t)machine->script_a,
                     (uint8_t)machine->script_b);
        break;
    case YT2_SCRIPT_ABS:
        yt2_mouse_abs(machine, (uint16_t)machine->script_a,
                      (uint16_t)machine->script_b);
        break;
    case YT2_SCRIPT_MOVE:
        yt2_mouse_move(machine, (int16_t)machine->script_a,
                       (int16_t)machine->script_b);
        break;
    case YT2_SCRIPT_BUTTON:
        yt2_mouse_button(machine, (uint8_t)machine->script_a,
                         (uint8_t)machine->script_b);
        break;
    default:
        break;
    }
    machine->script_pending = 0;
}

void yt2_link_poll(yt2_machine *machine)
{
    if (!machine->link_ready) {
        return;
    }
    /* Only take input where a callback may run.  The per-instruction cycle
       hook can present from inside a nested guest callback (it fired while
       the INT 33h handler was drawing its cursor), and a nested pass can
       also come from the INT 1Ch ISR.  The INT 33h shim refuses to re-enter
       the game from there: consuming a button edge on such a pass loses the
       click for good.  Leave the records in the transport; the next present
       outside guest callbacks takes them in order. */
    if (!yt2_machine_call_safe(machine)) {
        return;
    }
    yt2_link_apply_script(machine);
    if (yt2_service_poll(&machine->service) != YT2_OK) {
        /* The input file is a live document: a bad or half-written record
           while the client recreates it is not a reason to stop presenting.
           The transport resynchronises on the next pass; the picture must
           stay live even if the input side glitches. */
        if (!machine->link_read_warned) {
            fprintf(stderr,
                    "yt2: link input resynchronising; video continues\n");
            machine->link_read_warned = 1;
        }
    }
}

/* The VBE aperture shows one 64 KB bank at a time.  The machine mirrors that
   in vbe_bank[]; the active bank also sits at A000.  Read the active bank
   from the aperture and the others from the saved copies, so a row may cross
   a bank boundary without disturbing the game's mapping. */
static uint8_t yt2_link_vbe_byte(const yt2_machine *machine, uint32_t offset)
{
    unsigned bank = (unsigned)(offset >> 16);

    if ((int)bank == machine->vbe_bank_active) {
        return machine->mem.bytes[((uint32_t)YT2_SCREEN_SEG << 4) +
                                  (offset & 0xFFFFu)];
    }
    if (bank < YT2_VBE_BANKS) {
        return machine->vbe_bank[bank][offset & 0xFFFFu];
    }
    return 0;
}

static void yt2_link_fill_vbe(yt2_machine *machine, uint16_t height)
{
    unsigned row;

    for (row = 0; row < height; row++) {
        uint32_t offset = (uint32_t)row * 640u;
        unsigned column;

        for (column = 0; column < 640u; column++) {
            machine->video.pixels[offset + column] =
                yt2_link_vbe_byte(machine, offset + column);
        }
    }
}

void yt2_link_present(yt2_machine *machine)
{
    if (!machine->link_ready) {
        return;
    }
    if (machine->vbe_mode &&
        (machine->video_mode == 0x100 || machine->video_mode == 0x101)) {
        uint16_t height = machine->video_mode == 0x101 ? 480u : 400u;

        yt2_video_set_mode(&machine->video, 640, height, 8);
        if (machine->vbe_wait_palette) {
            /* The decoder has not loaded its palette yet.  Switch and blank
               the client, but keep the banks off the wire. */
            if (yt2_video_present_blank(&machine->video,
                                        &machine->service) != YT2_OK ||
                yt2_service_flush(&machine->service) != YT2_OK) {
                fprintf(stderr,
                        "yt2: link write failed; continuing without the client\n");
                machine->link_ready = 0;
                return;
            }
            if (yt2_service_take_wrap(&machine->service) != 0) {
                yt2_video_force_keyframe(&machine->video);
            }
            return;
        }
        yt2_link_fill_vbe(machine, height);
        yt2_video_set_palette(&machine->video, 0, 256,
                              machine->present_palette);
    } else if (machine->video_mode == 0x13 && !machine->vbe_mode) {
        yt2_video_set_mode(&machine->video, 320, 200, 8);
        memcpy(machine->video.pixels, machine->present_frame, 64000u);
        yt2_video_set_palette(&machine->video, 0, 256,
                              machine->present_palette);
    } else {
        return;
    }
    if (yt2_video_present(&machine->video, &machine->service) != YT2_OK ||
        yt2_service_flush(&machine->service) != YT2_OK) {
        fprintf(stderr, "yt2: link write failed; continuing without the client\n");
        machine->link_ready = 0;
        return;
    }
    if (yt2_service_take_wrap(&machine->service) != 0) {
        yt2_video_force_keyframe(&machine->video);
    }
}

void yt2_link_close(yt2_machine *machine)
{
    if (machine->link) {
        yt2_transport_close(&machine->transport);
        machine->link = 0;
        machine->link_ready = 0;
    }
}
