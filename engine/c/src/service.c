#include "yt2/service.h"

#include <string.h>

#define YT2_SERVICE_ERROR_TEXT "bad message"
#define YT2_SERVICE_ERROR_CODE 2u

void yt2_service_init(yt2_service *service, yt2_transport *transport,
                      const yt2_input_handlers *handlers, void *handler_context)
{
    memset(service, 0, sizeof(*service));
    service->transport = transport;
    if (handlers != NULL) {
        service->handlers = *handlers;
    }
    service->handler_context = handler_context;
    yt2_input_parser_init(&service->parser);
}

int yt2_service_send_payload(yt2_service *service, uint8_t family, uint8_t codec,
                             const uint8_t *payload, uint32_t length)
{
    uint8_t frame[YT2_MAX_FRAME];
    uint32_t sequence;

    if (service->failed || length > YT2_MAX_PAYLOAD) {
        return YT2_ERR;
    }
    sequence = service->sequence;
    service->sequence++;
    yt2_frame_header_write(frame, family, 0, codec, sequence, length);
    for (uint32_t index = 0; index < length; index++) {
        frame[YT2_FRAME_HEADER_SIZE + index] = payload[index];
    }
    if (yt2_transport_write(service->transport, frame,
                            YT2_FRAME_HEADER_SIZE + length) != YT2_OK) {
        service->failed = 1;
        return YT2_ERR;
    }
    return YT2_OK;
}

static int yt2_service_video_payload(yt2_service *service, uint8_t codec,
                                     const uint8_t *payload, int length)
{
    if (length < 0) {
        return YT2_ERR;
    }
    return yt2_service_send_payload(service, (uint8_t)YT2_FAMILY_VIDEO, codec,
                                    payload, (uint32_t)length);
}

int yt2_service_video_set_mode(yt2_service *service, uint16_t width,
                               uint16_t height, uint8_t bpp)
{
    uint8_t payload[8];
    int length = yt2_payload_video_set_mode(payload, width, height, bpp);

    return yt2_service_video_payload(service, 0, payload, length);
}

int yt2_service_video_palette(yt2_service *service, uint16_t start,
                              uint16_t count, const uint8_t *dac)
{
    uint8_t payload[YT2_MAX_PAYLOAD];
    int length = yt2_payload_video_palette(payload, start, count, dac);

    return yt2_service_video_payload(service, 0, payload, length);
}

int yt2_service_video_rect(yt2_service *service, uint8_t codec, uint16_t x,
                           uint16_t y, uint16_t width, uint16_t height,
                           const uint8_t *pixels)
{
    uint8_t payload[YT2_MAX_PAYLOAD];
    int length;

    if (codec != 0u) {
        return YT2_ERR;             /* row runs arrive with the port's video code */
    }
    length = yt2_payload_video_rect(payload, x, y, width, height, pixels);
    return yt2_service_video_payload(service, codec, payload, length);
}

int yt2_service_video_frame_end(yt2_service *service)
{
    uint8_t payload[2];
    int length = yt2_payload_video_frame_end(payload);

    return yt2_service_video_payload(service, 0, payload, length);
}

int yt2_service_video_reset(yt2_service *service)
{
    uint8_t payload[2];
    int length = yt2_payload_video_reset(payload);

    return yt2_service_video_payload(service, 0, payload, length);
}

int yt2_service_video_scroll(yt2_service *service, int8_t delta)
{
    uint8_t payload[2];
    int length = yt2_payload_video_scroll(payload, delta);

    return yt2_service_video_payload(service, 0, payload, length);
}

static int yt2_service_control_payload(yt2_service *service,
                                       const uint8_t *payload, int length)
{
    if (length < 0) {
        return YT2_ERR;
    }
    return yt2_service_send_payload(service, (uint8_t)YT2_FAMILY_CONTROL, 0,
                                    payload, (uint32_t)length);
}

int yt2_service_control_hello(yt2_service *service, uint8_t role,
                              uint8_t version)
{
    uint8_t payload[4];
    int length = yt2_payload_control_hello(payload, role, version);

    return yt2_service_control_payload(service, payload, length);
}

int yt2_service_control_bye(yt2_service *service)
{
    uint8_t payload[2];
    int length = yt2_payload_control_bye(payload);

    return yt2_service_control_payload(service, payload, length);
}

int yt2_service_control_error(yt2_service *service, uint16_t code,
                              const char *text)
{
    uint8_t payload[YT2_MAX_PAYLOAD];
    int length = yt2_payload_control_error(payload, code, text);

    return yt2_service_control_payload(service, payload, length);
}

int yt2_service_control_debug(yt2_service *service, uint8_t channel,
                              const uint8_t *data, uint32_t length)
{
    uint8_t payload[YT2_MAX_PAYLOAD];
    int size = yt2_payload_control_debug(payload, channel, data, length);

    return yt2_service_control_payload(service, payload, size);
}

int yt2_service_control_keyframe(yt2_service *service)
{
    uint8_t payload[2];
    int length = yt2_payload_control_keyframe(payload);

    return yt2_service_control_payload(service, payload, length);
}

static void yt2_service_dispatch(yt2_service *service, const yt2_input_event *event)
{
    switch (event->kind) {
    case YT2_INPUT_EVENT_HELLO:
        if (!service->client_hello_seen) {
            service->client_hello_seen = 1;
            (void)yt2_service_control_hello(service, 0, 0);
        }
        if (service->handlers.hello != NULL) {
            service->handlers.hello(service->handler_context, event->role,
                                    event->version);
        }
        break;
    case YT2_INPUT_EVENT_KEYFRAME_REQUEST:
        if (service->handlers.keyframe_request != NULL) {
            service->handlers.keyframe_request(service->handler_context);
        }
        break;
    case YT2_INPUT_EVENT_KEY:
        if (event->down != 0u && service->handlers.key != NULL) {
            service->handlers.key(service->handler_context, event->scan,
                                  event->ascii);
        }
        break;
    case YT2_INPUT_EVENT_MOUSE_MOVE:
        if (service->handlers.mouse_move != NULL) {
            service->handlers.mouse_move(service->handler_context, event->dx,
                                         event->dy);
        }
        break;
    case YT2_INPUT_EVENT_MOUSE_BUTTON:
        if (service->handlers.mouse_button != NULL) {
            service->handlers.mouse_button(service->handler_context, event->button,
                                           event->down);
        }
        break;
    case YT2_INPUT_EVENT_MOUSE_ABS:
        if (service->handlers.mouse_abs != NULL) {
            service->handlers.mouse_abs(service->handler_context, event->x,
                                        event->y);
        }
        break;
    case YT2_INPUT_EVENT_QUIT:
        if (service->handlers.quit != NULL) {
            service->handlers.quit(service->handler_context);
        }
        break;
    case YT2_INPUT_EVENT_UNKNOWN_FAMILY:
        (void)yt2_service_control_error(service, YT2_SERVICE_ERROR_CODE,
                                        YT2_SERVICE_ERROR_TEXT);
        if (service->handlers.unknown != NULL) {
            service->handlers.unknown(service->handler_context, event->family,
                                      event->type);
        }
        break;
    case YT2_INPUT_EVENT_UNKNOWN_MESSAGE:
        if (service->handlers.unknown != NULL) {
            service->handlers.unknown(service->handler_context, event->family,
                                      event->type);
        }
        break;
    case YT2_INPUT_EVENT_BYE:
    case YT2_INPUT_EVENT_ERROR:
    case YT2_INPUT_EVENT_DEBUG:
    case YT2_INPUT_NONE:
    default:
        break;
    }
}

int yt2_service_poll(yt2_service *service)
{
    uint8_t bytes[256];
    unsigned rounds = 0;

    if (service->failed) {
        return YT2_ERR;
    }
    for (;;) {
        uint32_t got = 0;

        if (yt2_transport_read(service->transport, bytes, sizeof(bytes), &got) !=
            YT2_OK) {
            service->failed = 1;
            return YT2_ERR;
        }
        if (got == 0u) {
            break;
        }
        for (uint32_t index = 0; index < got; index++) {
            yt2_input_event event;

            memset(&event, 0, sizeof(event));
            if (yt2_input_parser_feed(&service->parser, bytes[index], &event) == 1) {
                yt2_service_dispatch(service, &event);
            }
        }
        rounds++;
        if (rounds >= 64u) {
            break;
        }
    }
    return YT2_OK;
}

int yt2_service_flush(yt2_service *service)
{
    if (service->failed) {
        return YT2_ERR;
    }
    return yt2_transport_flush(service->transport);
}

int yt2_service_take_wrap(yt2_service *service)
{
    return yt2_transport_take_wrap(service->transport);
}
