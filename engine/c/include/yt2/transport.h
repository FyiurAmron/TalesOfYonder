#ifndef YT2_TRANSPORT_H
#define YT2_TRANSPORT_H

#include <stdio.h>

#include "yt2/core.h"
#include "yt2/protocol.h"

/*
 * The service boundary moves complete v0 frames and returns input bytes.  It
 * knows nothing about the game; a backend decides whether the far side is a
 * file pair, a raw recording, an in-process sink or (later) a socket.
 */

/* The 16-byte YT2B record on the folder link (docs/stream.md).  The struct is
   the in-memory view; encode/decode do the little-endian bytes explicitly, so
   nothing depends on struct padding or host byte order. */
#define YT2_BIN_VERSION 1u
#define YT2_BIN_HEADER_SIZE 16u

typedef struct yt2_bin_header {
    char magic[4];              /* "YT2B" */
    uint16_t version;           /* 1 */
    uint16_t header_size;       /* 16 */
    uint32_t pit_rate;          /* YT2_PIT_RATE */
    uint8_t direction;          /* 0 output (engine), 1 input (client) */
    uint8_t reserved[3];        /* 0 */
} yt2_bin_header;

/* One timed-folder event: the PIT stamp, then the complete v0 frame length. */
typedef struct yt2_timed_record {
    uint32_t pit_stamp;
    uint32_t frame_bytes;
} yt2_timed_record;

/* The optional YT2S full-duplex capture (docs/protocol.md).  The engine does
   not write it; the client does, and the Python tools decode it. */
typedef struct yt2_session_header {
    char magic[4];              /* "YT2S" */
    uint16_t version;           /* 1 */
    uint16_t header_size;       /* 16 */
    uint64_t timebase;          /* 1,000,000,000 ns */
} yt2_session_header;

typedef struct yt2_session_event {
    uint64_t time_ns;
    uint8_t direction;          /* 0 engine->client, 1 client->engine */
    uint8_t reserved[3];
    uint32_t frame_bytes;
} yt2_session_event;

_Static_assert(sizeof(yt2_bin_header) == YT2_BIN_HEADER_SIZE,
               "YT2B header size");
_Static_assert(sizeof(yt2_timed_record) == 8, "YT2B timed record size");
_Static_assert(sizeof(yt2_session_header) == 16, "YT2S header size");
_Static_assert(sizeof(yt2_session_event) == 16, "YT2S event size");

void yt2_bin_header_init(yt2_bin_header *header, uint8_t direction);
void yt2_bin_header_encode(const yt2_bin_header *header, uint8_t *destination);
int yt2_bin_header_decode(const uint8_t *source, uint32_t available,
                          yt2_bin_header *header);

typedef struct yt2_transport {
    void *context;
    int (*write_frame)(void *context, const uint8_t *frame, uint32_t length);
    int (*read)(void *context, uint8_t *destination, uint32_t capacity,
                uint32_t *read);
    int (*flush)(void *context);
    int (*take_wrap)(void *context);
    void (*close)(void *context);
} yt2_transport;

static inline int yt2_transport_write(yt2_transport *transport,
                                      const uint8_t *frame, uint32_t length)
{
    return transport->write_frame(transport->context, frame, length);
}

static inline int yt2_transport_read(yt2_transport *transport, uint8_t *destination,
                                     uint32_t capacity, uint32_t *read)
{
    return transport->read(transport->context, destination, capacity, read);
}

static inline int yt2_transport_flush(yt2_transport *transport)
{
    return transport->flush(transport->context);
}

static inline int yt2_transport_take_wrap(yt2_transport *transport)
{
    return transport->take_wrap(transport->context);
}

static inline void yt2_transport_close(yt2_transport *transport)
{
    transport->close(transport->context);
}

typedef struct yt2_null_transport {
    uint32_t frames;
    uint32_t bytes;
} yt2_null_transport;

void yt2_transport_null_init(yt2_transport *transport,
                             yt2_null_transport *context);

typedef struct yt2_stream_transport {
    FILE *file;
    const char *path;
    uint32_t frames;
    uint32_t bytes;
} yt2_stream_transport;

int yt2_transport_stream_init(yt2_transport *transport,
                              yt2_stream_transport *context, const char *path);

#define YT2_FOLDER_VIDEO_LIMIT 0x4000000u
#define YT2_FOLDER_INPUT_LIMIT 0x10000u
#define YT2_FOLDER_BUFFER_SIZE 8192u

typedef struct yt2_folder_transport {
    FILE *output;
    FILE *input;
    const char *output_path;
    const char *input_path;

    uint32_t output_written;
    uint32_t output_frames;
    uint32_t output_wrapped;

    uint32_t input_frames;
    uint32_t input_resyncs;
    int input_from_start;
    int input_opened;
    int input_fresh;
    uint8_t input_mode;          /* 0 unknown, 1 timed YT2B, 2 raw v0 */
    long input_base;
    long input_position;
    long input_last_size;

    uint8_t input_buffer[YT2_FOLDER_BUFFER_SIZE];
    uint32_t input_length;
    uint32_t input_offset;

    uint8_t pending[YT2_MAX_FRAME];
    uint32_t pending_length;
    uint32_t pending_offset;
} yt2_folder_transport;

int yt2_transport_folder_init(yt2_transport *transport,
                              yt2_folder_transport *context,
                              const char *output_path, const char *input_path,
                              int input_from_start);

/* Test/helper access: the YT2B header bytes (16 bytes). */
void yt2_transport_bin_header(uint8_t *destination, uint8_t direction);

#endif /* YT2_TRANSPORT_H */
