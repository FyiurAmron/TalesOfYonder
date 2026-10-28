#ifndef YT2_PROTOCOL_H
#define YT2_PROTOCOL_H

#include "yt2/core.h"

/*
 * Protocol v0 framing and the message layouts from docs/protocol.md.
 * The same helpers serve the engine, the stub, the tests and the replay
 * harness; a transport only moves complete frames.
 */

#define YT2_FRAME_HEADER_SIZE 14u
#define YT2_MAX_PAYLOAD 2048u
#define YT2_MAX_FRAME (YT2_FRAME_HEADER_SIZE + YT2_MAX_PAYLOAD)
#define YT2_INPUT_MAX_PAYLOAD 32u
#define YT2_PROTOCOL_MAGIC 0x5954u
#define YT2_PROTOCOL_VERSION 0u

#define YT2_FLAG_COMPRESSED 0x01u
#define YT2_FLAG_ACK 0x02u
#define YT2_FLAG_EOF 0x04u

typedef enum yt2_family {
    YT2_FAMILY_VIDEO = 1,
    YT2_FAMILY_AUDIO = 2,
    YT2_FAMILY_INPUT = 3,
    YT2_FAMILY_TICK = 4,
    YT2_FAMILY_CONTROL = 5,
    YT2_FAMILY_FILES = 6
} yt2_family;

typedef enum yt2_video_message {
    YT2_VIDEO_SET_MODE = 0,
    YT2_VIDEO_PALETTE = 1,
    YT2_VIDEO_RECT = 2,
    YT2_VIDEO_FRAME_END = 3,
    YT2_VIDEO_RESET = 4,
    YT2_VIDEO_SCROLL = 5
} yt2_video_message;

typedef enum yt2_audio_message {
    YT2_AUDIO_OPL_WRITE = 0,
    YT2_AUDIO_OPL_BATCH = 1,
    YT2_AUDIO_CTRL = 2,
    YT2_AUDIO_OPL_BATCH_TIMED = 3,
    YT2_AUDIO_SPEAKER = 4,
    YT2_AUDIO_SAMPLE = 5
} yt2_audio_message;

typedef enum yt2_input_message {
    YT2_INPUT_KEY = 0,
    YT2_INPUT_MOUSE_MOVE = 1,
    YT2_INPUT_MOUSE_BUTTON = 2,
    YT2_INPUT_QUIT = 3
} yt2_input_message;

typedef enum yt2_control_message {
    YT2_CONTROL_HELLO = 0,
    YT2_CONTROL_BYE = 1,
    YT2_CONTROL_ERROR = 2,
    YT2_CONTROL_DEBUG = 3,
    YT2_CONTROL_KEYFRAME_REQUEST = 4
} yt2_control_message;

/*
 * Client INPUT type 3 is the absolute mouse position the current seam
 * understands; the protocol table lists only the first three types.
 */
#define YT2_INPUT_MOUSE_ABS 3u

typedef struct yt2_frame_header {
    uint8_t version;
    uint8_t family;
    uint8_t flags;
    uint8_t codec;
    uint32_t sequence;
    uint32_t length;
} yt2_frame_header;

void yt2_frame_header_write(uint8_t *destination, uint8_t family, uint8_t flags,
                            uint8_t codec, uint32_t sequence, uint32_t length);
int yt2_frame_header_parse(const uint8_t *source, uint32_t available,
                           yt2_frame_header *header);

int yt2_payload_video_set_mode(uint8_t *destination, uint16_t width,
                               uint16_t height, uint8_t bpp);
int yt2_payload_video_palette(uint8_t *destination, uint16_t start,
                              uint16_t count, const uint8_t *dac);
int yt2_payload_video_rect(uint8_t *destination, uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           const uint8_t *pixels);
int yt2_payload_video_frame_end(uint8_t *destination);
int yt2_payload_video_reset(uint8_t *destination);
int yt2_payload_video_scroll(uint8_t *destination, int8_t delta);

int yt2_payload_control_hello(uint8_t *destination, uint8_t role, uint8_t version);
int yt2_payload_control_bye(uint8_t *destination);
int yt2_payload_control_error(uint8_t *destination, uint16_t code,
                              const char *text);
int yt2_payload_control_debug(uint8_t *destination, uint8_t channel,
                              const uint8_t *data, uint32_t length);
int yt2_payload_control_keyframe(uint8_t *destination);

int yt2_payload_input_key(uint8_t *destination, uint8_t down, uint8_t scan,
                          uint8_t ascii);
int yt2_payload_input_mouse_move(uint8_t *destination, int16_t dx, int16_t dy);
int yt2_payload_input_mouse_button(uint8_t *destination, uint8_t button,
                                   uint8_t down);
int yt2_payload_input_mouse_abs(uint8_t *destination, uint16_t x, uint16_t y);
int yt2_payload_input_quit(uint8_t *destination);

int yt2_payload_audio_opl_write(uint8_t *destination, uint16_t port,
                                uint8_t reg, uint8_t value);
int yt2_payload_audio_opl_batch(uint8_t *destination, uint8_t count,
                                const uint8_t *writes);
int yt2_payload_audio_ctrl(uint8_t *destination, uint16_t rate, uint16_t format,
                           uint8_t on);
int yt2_payload_audio_opl_batch_timed(uint8_t *destination, uint16_t port,
                                      uint8_t count, const uint8_t *writes);
int yt2_payload_audio_speaker(uint8_t *destination, uint8_t gate,
                              uint16_t divisor, uint32_t pit);
int yt2_payload_audio_sample(uint8_t *destination, uint16_t id, uint32_t offset,
                             uint32_t pit);

typedef struct yt2_frame_reader {
    const uint8_t *data;
    uint32_t size;
    uint32_t position;
} yt2_frame_reader;

typedef struct yt2_frame_view {
    yt2_frame_header header;
    const uint8_t *payload;
} yt2_frame_view;

void yt2_frame_reader_init(yt2_frame_reader *reader, const uint8_t *data,
                           uint32_t size);
int yt2_frame_reader_next(yt2_frame_reader *reader, yt2_frame_view *view);

typedef enum yt2_input_kind {
    YT2_INPUT_NONE = 0,
    YT2_INPUT_EVENT_KEY,
    YT2_INPUT_EVENT_MOUSE_MOVE,
    YT2_INPUT_EVENT_MOUSE_BUTTON,
    YT2_INPUT_EVENT_MOUSE_ABS,
    YT2_INPUT_EVENT_QUIT,
    YT2_INPUT_EVENT_HELLO,
    YT2_INPUT_EVENT_KEYFRAME_REQUEST,
    YT2_INPUT_EVENT_BYE,
    YT2_INPUT_EVENT_ERROR,
    YT2_INPUT_EVENT_DEBUG,
    YT2_INPUT_EVENT_UNKNOWN_FAMILY,
    YT2_INPUT_EVENT_UNKNOWN_MESSAGE
} yt2_input_kind;

typedef struct yt2_input_event {
    yt2_input_kind kind;
    uint8_t family;
    uint8_t type;
    uint8_t down;
    uint8_t scan;
    uint8_t ascii;
    uint8_t button;
    uint16_t x;
    uint16_t y;
    int16_t dx;
    int16_t dy;
    uint8_t role;
    uint8_t version;
    uint16_t code;
} yt2_input_event;

typedef struct yt2_input_parser {
    uint8_t buffer[64];
    uint16_t length;
    uint16_t needed;
} yt2_input_parser;

void yt2_input_parser_init(yt2_input_parser *parser);
void yt2_input_parser_reset(yt2_input_parser *parser);
int yt2_input_parser_feed(yt2_input_parser *parser, uint8_t byte,
                          yt2_input_event *event);

#endif /* YT2_PROTOCOL_H */
