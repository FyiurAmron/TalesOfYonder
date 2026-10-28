#ifndef YT2_SERVICE_H
#define YT2_SERVICE_H

#include "yt2/protocol.h"
#include "yt2/transport.h"

/*
 * The seam: engine code sends semantic messages here, the transport decides
 * where they go.  Incoming INPUT/CONTROL frames are parsed here and handed to
 * the game as ordinary callbacks, so the game never sees a file or a socket.
 */

typedef struct yt2_input_handlers {
    void (*key)(void *context, uint8_t scan, uint8_t ascii);
    void (*mouse_move)(void *context, int16_t dx, int16_t dy);
    void (*mouse_button)(void *context, uint8_t button, uint8_t down);
    void (*mouse_abs)(void *context, uint16_t x, uint16_t y);
    void (*quit)(void *context);
    void (*hello)(void *context, uint8_t role, uint8_t version);
    void (*keyframe_request)(void *context);
    void (*unknown)(void *context, uint8_t family, uint8_t type);
} yt2_input_handlers;

typedef struct yt2_service {
    yt2_transport *transport;
    yt2_input_handlers handlers;
    void *handler_context;
    uint32_t sequence;
    uint8_t client_hello_seen;
    uint8_t failed;
    yt2_input_parser parser;
} yt2_service;

void yt2_service_init(yt2_service *service, yt2_transport *transport,
                      const yt2_input_handlers *handlers, void *handler_context);

int yt2_service_send_payload(yt2_service *service, uint8_t family, uint8_t codec,
                             const uint8_t *payload, uint32_t length);

int yt2_service_video_set_mode(yt2_service *service, uint16_t width,
                               uint16_t height, uint8_t bpp);
int yt2_service_video_palette(yt2_service *service, uint16_t start,
                              uint16_t count, const uint8_t *dac);
int yt2_service_video_rect(yt2_service *service, uint8_t codec, uint16_t x,
                           uint16_t y, uint16_t width, uint16_t height,
                           const uint8_t *pixels);
int yt2_service_video_frame_end(yt2_service *service);
int yt2_service_video_reset(yt2_service *service);
int yt2_service_video_scroll(yt2_service *service, int8_t delta);

int yt2_service_control_hello(yt2_service *service, uint8_t role,
                              uint8_t version);
int yt2_service_control_bye(yt2_service *service);
int yt2_service_control_error(yt2_service *service, uint16_t code,
                              const char *text);
int yt2_service_control_debug(yt2_service *service, uint8_t channel,
                              const uint8_t *data, uint32_t length);
int yt2_service_control_keyframe(yt2_service *service);

int yt2_service_poll(yt2_service *service);
int yt2_service_flush(yt2_service *service);
int yt2_service_take_wrap(yt2_service *service);

#endif /* YT2_SERVICE_H */
