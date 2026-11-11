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
 * to engine) over the folder transport.  Codec 1, scroll, AUDIO, TICK and
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
