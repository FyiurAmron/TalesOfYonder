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

void yt2_link_poll(yt2_machine *machine)
{
    if (!machine->link_ready) {
        return;
    }
    if (yt2_service_poll(&machine->service) != YT2_OK) {
        fprintf(stderr, "yt2: link lost; continuing without the client\n");
        machine->link_ready = 0;
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
