#ifndef YT2_TRANSPORT_H
#define YT2_TRANSPORT_H

/*
 * The folder transport (docs/protocol.md, "the folder pseudo-ring"): the
 * engine appends timed YT2B records to OUTPUT.BIN and follows INPUT.BIN for
 * client frames.  A YT2B file is a 16-byte header
 *
 *   ASCII "YT2B", u16 format_version 1, u16 header_bytes 16,
 *   u32 timebase 1,193,182, u8 direction, 3 reserved zeros
 *
 * and then records of u32 pit_stamp, u32 frame_bytes and one complete v0
 * frame.  OUTPUT.BIN carries direction 0, INPUT.BIN direction 1.
 *
 * Slice 1 restarts OUTPUT.BIN at the 64 MiB video limit (the reader rewinds
 * and the engine owes a keyframe) and reads INPUT.BIN as YT2B only; raw v0
 * input files are a later compatibility item.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define YT2_TRANSPORT_VIDEO_LIMIT 0x4000000u
#define YT2_TRANSPORT_DIRECTION_OUT 0u
#define YT2_TRANSPORT_DIRECTION_IN 1u

typedef struct yt2_folder_out {
    FILE *file;
    char path[1024];
    uint32_t written;
    int wrapped;
} yt2_folder_out;

typedef struct yt2_folder_in {
    FILE *file;
    char path[1024];
    long position;
    int opened;
    int wrapped;
    uint8_t *staging;
    size_t staged;
    size_t served;
    size_t capacity;
} yt2_folder_in;

/* Create DIR/OUTPUT.BIN fresh (direction 0) and flush the header. */
int yt2_folder_out_open(yt2_folder_out *out, const char *dir);

/*
 * Append one timed record.  Restarts the file past 64 MiB and flags the wrap
 * (see yt2_folder_out_take_wrapped).  The bytes publish on commit, so a
 * present costs one commit no matter how many records it holds.
 */
int yt2_folder_out_write(yt2_folder_out *out, const uint8_t *frame,
                         uint32_t length);

/* Publish appended records to followers. */
int yt2_folder_out_commit(yt2_folder_out *out);

/* True once when the output wrapped since the last call. */
int yt2_folder_out_take_wrapped(yt2_folder_out *out);

void yt2_folder_out_close(yt2_folder_out *out);

/* Attach to DIR/INPUT.BIN; a missing file is fine (the client owns it). */
int yt2_folder_in_open(yt2_folder_in *in, const char *dir);

/*
 * Poll for one complete client frame.  Returns 1 with `frame`/`length`
 * pointing into transport staging (valid until the next call), 0 when no
 * complete record is available, -1 on I/O failure.  A truncated writer
 * rewinds the reader past the header; a corrupt record resets the session.
 */
int yt2_folder_in_poll(yt2_folder_in *in, const uint8_t **frame,
                       uint32_t *length);

/* True once when the input rewound (new client session) since the call. */
int yt2_folder_in_take_wrapped(yt2_folder_in *in);

void yt2_folder_in_close(yt2_folder_in *in);

#endif /* YT2_TRANSPORT_H */
