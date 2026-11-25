#ifndef YT2_PROTOCOL_H
#define YT2_PROTOCOL_H

/*
 * Companion protocol v0 (docs/protocol.md, frozen): the wire contract B2
 * speaks to the host/client.  This is B2's own implementation; it shares the
 * spec with the seam and B1 builds but no code with either.
 *
 * Framing (14 bytes, little-endian, transport-independent):
 *
 *   u16 magic 0x5954 ('YT', so 54 59 on the wire), u8 version,
 *   u8 family, u8 flags, u8 codec, u32 seq, u32 len.
 *
 * Slice 1 carries VIDEO (engine to client) and parses INPUT/CONTROL (client
 * to engine) over the folder transport; row rects travel run-coded (codec 1)
 * and the AUDIO encoders below speak the full family.  Scroll, TICK and
 * session capture follow; every addition keeps these bytes unchanged.
 */

#include <stddef.h>
#include <stdint.h>

#define YT2_PROTOCOL_MAGIC 0x5954u
#define YT2_PROTOCOL_VERSION 0u
#define YT2_FRAME_HEADER_SIZE 14u

/* Families. */
#define YT2_FAMILY_VIDEO 1u
#define YT2_FAMILY_AUDIO 2u
#define YT2_FAMILY_INPUT 3u
#define YT2_FAMILY_TICK 4u
#define YT2_FAMILY_CONTROL 5u

/* VIDEO message types. */
#define YT2_VIDEO_SET_MODE 0u
#define YT2_VIDEO_PALETTE 1u
#define YT2_VIDEO_RECT 2u
#define YT2_VIDEO_FRAME_END 3u
#define YT2_VIDEO_RESET 4u
#define YT2_VIDEO_SCROLL 5u
#define YT2_VIDEO_SCROLL 5u

/* AUDIO message types. */
#define YT2_AUDIO_OPL_WRITE 0u
#define YT2_AUDIO_OPL_BATCH 1u
#define YT2_AUDIO_CTRL 2u
#define YT2_AUDIO_OPL_BATCH_TIMED 3u
#define YT2_AUDIO_SPEAKER 4u
#define YT2_AUDIO_SAMPLE 5u

/* An OPL batch keeps 1..128 writes, per the spec. */
#define YT2_AUDIO_BATCH_MAX 128u

/* One untimed OPL register write: port, register, value. */
typedef struct yt2_opl_write {
    uint16_t port;
    uint8_t reg;
    uint8_t val;
} yt2_opl_write;

/* One timed OPL register write: PIT stamp, register, value. */
typedef struct yt2_opl_timed_write {
    uint32_t pit;
    uint8_t reg;
    uint8_t val;
} yt2_opl_timed_write;

/* TICK message types. */
#define YT2_TICK_TICK 0u
#define YT2_TICK_PACE 1u

/* INPUT message types. */
#define YT2_INPUT_KEY 0u
#define YT2_INPUT_MOUSE_MOVE 1u
#define YT2_INPUT_MOUSE_BUTTON 2u
#define YT2_INPUT_QUIT 3u

/* CONTROL message types. */
#define YT2_CONTROL_HELLO 0u
#define YT2_CONTROL_BYE 1u
#define YT2_CONTROL_ERROR 2u
#define YT2_CONTROL_DEBUG 3u
#define YT2_CONTROL_KEYFRAME_REQUEST 4u

/* Payload codecs. */
#define YT2_CODEC_RAW 0u
#define YT2_CODEC_RUNS 1u

/* Roles for CONTROL hello. */
#define YT2_ROLE_ENGINE 0u
#define YT2_ROLE_CLIENT 1u

/* A 320x200 raw frame (full rect + header) fits with room to spare. */
#define YT2_PROTOCOL_MAX_FRAME 524288u

/*
 * Payloads stay at or under 2048 bytes on the wire: the companion client
 * enforces that limit, so a frame travels as one rect per row (h=1), never
 * as one big rect.  The limit is an interop rule, not a framing rule: the
 * encoders below accept any size the frame bound allows.
 */
#define YT2_PROTOCOL_MAX_PAYLOAD 2048u

typedef struct yt2_frame_header {
    uint8_t version;
    uint8_t family;
    uint8_t flags;
    uint8_t codec;
    uint32_t seq;
    uint32_t length;
} yt2_frame_header;

/* Client-to-engine input, decoded from one INPUT or CONTROL frame. */
typedef struct yt2_input_event {
    uint8_t kind;
    uint8_t down;
    uint8_t scancode;
    uint8_t ascii;
    int16_t dx;
    int16_t dy;
    uint8_t button;
    uint16_t x;
    uint16_t y;
    uint8_t role;
    uint8_t version;
} yt2_input_event;

#define YT2_INPUT_EVENT_NONE 0u
#define YT2_INPUT_EVENT_KEY 1u
#define YT2_INPUT_EVENT_MOUSE_MOVE 2u
#define YT2_INPUT_EVENT_MOUSE_BUTTON 3u
#define YT2_INPUT_EVENT_QUIT 4u
#define YT2_INPUT_EVENT_HELLO 5u
#define YT2_INPUT_EVENT_KEYFRAME_REQUEST 6u
#define YT2_INPUT_EVENT_MOUSE_ABS 7u

/* Encode one 14-byte frame header.  `out` must hold 14 bytes. */
void yt2_frame_encode_header(uint8_t *out, uint8_t family, uint8_t flags,
                             uint8_t codec, uint32_t seq, uint32_t length);

/*
 * Decode and validate a 14-byte header.  Returns the payload length on
 * success, -1 when the magic, version or length is bad.
 */
int yt2_frame_decode_header(const uint8_t *frame, size_t size,
                            yt2_frame_header *header);

/*
 * VIDEO encoders.  Each writes one message payload (starting with the type
 * byte) and returns its length, or -1 when `capacity` is too small.
 */
int yt2_video_encode_set_mode(uint8_t *out, size_t capacity, unsigned int w,
                              unsigned int h, unsigned int bpp);
int yt2_video_encode_palette(uint8_t *out, size_t capacity, unsigned int start,
                             unsigned int count, const uint8_t *rgb);
int yt2_video_encode_rect(uint8_t *out, size_t capacity, unsigned int x,
                          unsigned int y, unsigned int w, unsigned int h,
                          const uint8_t *pixels);
int yt2_video_encode_frame_end(uint8_t *out, size_t capacity);

/*
 * Whole-screen pan: 2 bytes (type 5, i8 dy; positive moves the picture
 * down).  The slid-in rows plus every row that did not move with the
 * picture follow as rects in the same frame; |dy| must be smaller than
 * the client height (the caller guarantees this).  Codec stays 0.
 */
int yt2_video_encode_scroll(uint8_t *out, size_t capacity, int dy);

/*
 * TICK encoders (family 4, codec 0): tick stamps a presented frame with
 * the engine's logical tick, pace announces milliseconds per tick.
 */
int yt2_tick_encode_tick(uint8_t *out, size_t capacity, uint32_t n);
int yt2_tick_encode_pace(uint8_t *out, size_t capacity, unsigned int ms);

/*
 * Codec 1 row-runs rect: the same 9-byte rect header, then each row as runs
 * (`00 nn` repeats the previous pixel, `01 nn vv` repeats vv, `02 nn ...`
 * takes nn literals, `03` ends the row).  Runs of three or more pixels use
 * `00`/`01` split at 255; shorter runs merge into literal chunks of at most
 * 255, so the worst case per row is w + 2*ceil(w/255) + 1 (647 bytes for a
 * 640-pixel noise row) and a flat row is 4 bytes plus the `03`.  Rows encode
 * independently; the frame's codec byte must be YT2_CODEC_RUNS.
 */
int yt2_video_encode_rect_runs(uint8_t *out, size_t capacity, unsigned int x,
                               unsigned int y, unsigned int w, unsigned int h,
                               const uint8_t *pixels);

/*
 * AUDIO encoders.  Each writes one message payload (starting with the type
 * byte) and returns its length, or -1 when `capacity` is too small or the
 * arguments are out of range.  Frames carrying these stay codec 0.
 */
int yt2_audio_encode_opl_write(uint8_t *out, size_t capacity, unsigned int port,
                               unsigned int reg, unsigned int val);
int yt2_audio_encode_opl_batch(uint8_t *out, size_t capacity,
                               unsigned int count,
                               const yt2_opl_write *writes);
int yt2_audio_encode_ctrl(uint8_t *out, size_t capacity, unsigned int rate,
                          unsigned int fmt, unsigned int on);
int yt2_audio_encode_opl_batch_timed(uint8_t *out, size_t capacity,
                                     unsigned int port, unsigned int count,
                                     const yt2_opl_timed_write *writes);
int yt2_audio_encode_speaker(uint8_t *out, size_t capacity, unsigned int gate,
                             unsigned int divisor, uint32_t pit);
int yt2_audio_encode_sample(uint8_t *out, size_t capacity, unsigned int id,
                            uint32_t offset, uint32_t pit);

/* CONTROL hello encoder (role + version, 3 bytes). */
int yt2_control_encode_hello(uint8_t *out, size_t capacity, unsigned int role,
                             unsigned int version);

/*
 * Decode one client-to-engine frame into `event` (caller sets kind to
 * YT2_INPUT_EVENT_NONE first).  Returns 1 when the frame decoded, 0 when it
 * is well-framed but not for the engine (unknown family or type, short
 * payload, wrong version): the caller ignores those, per the spec.
 *
 * INPUT type 3 carries the absolute pointer (mouse_abs, five bytes) when the
 * payload is long enough and quit otherwise; that length rule is what the
 * seam, B1 and the client all implement.
 */
int yt2_input_decode_frame(const uint8_t *frame, size_t size,
                           yt2_input_event *event);

#endif /* YT2_PROTOCOL_H */
