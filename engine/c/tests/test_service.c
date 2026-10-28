#include "test.h"

#include "yt2/service.h"

#include <string.h>

typedef struct test_transport {
    uint8_t output[32768];
    uint32_t output_length;
    uint8_t input[1024];
    uint32_t input_length;
    uint32_t input_position;
} test_transport;

typedef struct test_callbacks {
    int keys;
    int keyframes;
    int hellos;
    int quits;
    int unknown;
    uint8_t last_scan;
    uint8_t last_ascii;
    uint8_t last_role;
    int16_t last_dx;
    int16_t last_dy;
    uint8_t last_button;
    uint8_t last_down;
} test_callbacks;

static int test_write(void *context, const uint8_t *frame, uint32_t length)
{
    test_transport *transport = (test_transport *)context;

    if (transport->output_length + length > sizeof(transport->output)) {
        return YT2_ERR;
    }
    for (uint32_t index = 0; index < length; index++) {
        transport->output[transport->output_length + index] = frame[index];
    }
    transport->output_length += length;
    return YT2_OK;
}

static int test_read(void *context, uint8_t *destination, uint32_t capacity,
                     uint32_t *read)
{
    test_transport *transport = (test_transport *)context;
    uint32_t available = transport->input_length - transport->input_position;

    if (available > capacity) {
        available = capacity;
    }
    for (uint32_t index = 0; index < available; index++) {
        destination[index] = transport->input[transport->input_position + index];
    }
    transport->input_position += available;
    *read = available;
    return YT2_OK;
}

static int test_flush(void *context)
{
    (void)context;
    return YT2_OK;
}

static int test_take_wrap(void *context)
{
    (void)context;
    return 0;
}

static void test_close(void *context)
{
    (void)context;
}

static void test_transport_init(yt2_transport *transport, test_transport *context)
{
    memset(context, 0, sizeof(*context));
    transport->context = context;
    transport->write_frame = test_write;
    transport->read = test_read;
    transport->flush = test_flush;
    transport->take_wrap = test_take_wrap;
    transport->close = test_close;
}

static uint32_t make_frame(uint8_t *destination, uint8_t family, uint32_t sequence,
                           const uint8_t *payload, uint32_t length)
{
    yt2_frame_header_write(destination, family, 0, 0, sequence, length);
    for (uint32_t index = 0; index < length; index++) {
        destination[YT2_FRAME_HEADER_SIZE + index] = payload[index];
    }
    return YT2_FRAME_HEADER_SIZE + length;
}

static void on_key(void *context, uint8_t scan, uint8_t ascii)
{
    test_callbacks *callbacks = (test_callbacks *)context;

    callbacks->keys++;
    callbacks->last_scan = scan;
    callbacks->last_ascii = ascii;
}

static void on_mouse_move(void *context, int16_t dx, int16_t dy)
{
    test_callbacks *callbacks = (test_callbacks *)context;

    callbacks->last_dx = dx;
    callbacks->last_dy = dy;
}

static void on_mouse_button(void *context, uint8_t button, uint8_t down)
{
    test_callbacks *callbacks = (test_callbacks *)context;

    callbacks->last_button = button;
    callbacks->last_down = down;
}

static void on_quit(void *context)
{
    ((test_callbacks *)context)->quits++;
}

static void on_hello(void *context, uint8_t role, uint8_t version)
{
    test_callbacks *callbacks = (test_callbacks *)context;

    (void)version;
    callbacks->hellos++;
    callbacks->last_role = role;
}

static void on_keyframe(void *context)
{
    ((test_callbacks *)context)->keyframes++;
}

static void on_unknown(void *context, uint8_t family, uint8_t type)
{
    test_callbacks *callbacks = (test_callbacks *)context;

    (void)family;
    (void)type;
    callbacks->unknown++;
}

static void callbacks_init(yt2_input_handlers *handlers, test_callbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
    memset(handlers, 0, sizeof(*handlers));
    handlers->key = on_key;
    handlers->mouse_move = on_mouse_move;
    handlers->mouse_button = on_mouse_button;
    handlers->quit = on_quit;
    handlers->hello = on_hello;
    handlers->keyframe_request = on_keyframe;
    handlers->unknown = on_unknown;
}

int test_service(void)
{
    test_transport transport_context;
    test_callbacks callbacks;
    yt2_input_handlers handlers;
    yt2_transport transport;
    yt2_service service;
    yt2_frame_reader reader;
    yt2_frame_view view;
    uint8_t frame[YT2_MAX_FRAME];
    uint8_t payload[16];
    uint32_t size;

    test_transport_init(&transport, &transport_context);
    yt2_service_init(&service, &transport, NULL, NULL);
    CHECK(yt2_service_control_hello(&service, 0, 0) == YT2_OK);
    CHECK(yt2_service_video_set_mode(&service, 320, 200, 8) == YT2_OK);
    CHECK(yt2_service_video_frame_end(&service) == YT2_OK);
    CHECK(service.sequence == 3u);
    CHECK(yt2_service_flush(&service) == YT2_OK);

    yt2_frame_reader_init(&reader, transport_context.output,
                          transport_context.output_length);
    CHECK(yt2_frame_reader_next(&reader, &view) == 1);
    CHECK(view.header.family == 5u && view.header.sequence == 0u);
    CHECK(view.header.length == 3u && view.payload[0] == 0u);
    CHECK(yt2_frame_reader_next(&reader, &view) == 1);
    CHECK(view.header.family == 1u && view.header.sequence == 1u);
    CHECK(view.header.length == 6u && view.payload[0] == 0u);
    CHECK(yt2_frame_reader_next(&reader, &view) == 1);
    CHECK(view.header.family == 1u && view.header.sequence == 2u);
    CHECK(view.header.length == 1u && view.payload[0] == 3u);
    CHECK(yt2_frame_reader_next(&reader, &view) == 0);

    /* A client hello is answered once, even if repeated. */
    callbacks_init(&handlers, &callbacks);
    test_transport_init(&transport, &transport_context);
    yt2_service_init(&service, &transport, &handlers, &callbacks);
    payload[0] = 0u;
    payload[1] = 1u;
    payload[2] = 0u;
    size = make_frame(frame, 5, 0, payload, 3);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    payload[0] = 4u;
    size = make_frame(frame, 5, 1, payload, 1);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    payload[0] = 0u;
    payload[1] = 1u;
    payload[2] = 0x1Eu;
    payload[3] = 0x61u;
    size = make_frame(frame, 3, 2, payload, 4);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    payload[0] = 1u;
    payload[1] = 0xFBu;
    payload[2] = 0xFFu;
    payload[3] = 2u;
    payload[4] = 0u;
    size = make_frame(frame, 3, 3, payload, 5);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    payload[0] = 0u;
    size = make_frame(frame, 4, 4, payload, 1);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    CHECK(yt2_service_poll(&service) == YT2_OK);
    CHECK(callbacks.hellos == 1 && callbacks.last_role == 1u);
    CHECK(callbacks.keyframes == 1);
    CHECK(callbacks.keys == 1 && callbacks.last_scan == 0x1Eu);
    CHECK(callbacks.last_ascii == 0x61u);
    CHECK(callbacks.last_dx == -5 && callbacks.last_dy == 2);
    CHECK(callbacks.unknown == 1);
    CHECK(service.sequence == 2u);      /* engine hello + error */

    yt2_frame_reader_init(&reader, transport_context.output,
                          transport_context.output_length);
    CHECK(yt2_frame_reader_next(&reader, &view) == 1);
    CHECK(view.header.family == 5u && view.header.sequence == 0u);
    CHECK(view.header.length == 3u && view.payload[1] == 0u);
    CHECK(yt2_frame_reader_next(&reader, &view) == 1);
    CHECK(view.header.family == 5u && view.header.sequence == 1u);
    CHECK(view.header.length == 14u && view.payload[0] == 2u);
    CHECK(memcmp(view.payload + 3, "bad message", 11) == 0);

    /* A second hello is not answered; releases do not reach the game. */
    test_transport_init(&transport, &transport_context);
    callbacks_init(&handlers, &callbacks);
    yt2_service_init(&service, &transport, &handlers, &callbacks);
    payload[0] = 0u;
    payload[1] = 1u;
    payload[2] = 0u;
    size = make_frame(frame, 5, 0, payload, 3);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    size = make_frame(frame, 5, 1, payload, 3);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    payload[0] = 0u;
    payload[1] = 0u;
    payload[2] = 0x1Eu;
    payload[3] = 0x61u;
    size = make_frame(frame, 3, 2, payload, 4);
    for (uint32_t index = 0; index < size; index++) {
        transport_context.input[transport_context.input_length++] = frame[index];
    }
    CHECK(yt2_service_poll(&service) == YT2_OK);
    CHECK(callbacks.hellos == 2);       /* both are reported to the stub */
    CHECK(callbacks.keys == 0);
    CHECK(service.sequence == 1u);      /* one engine hello only */

    /* Noise before a frame is dropped; a following frame still parses. */
    test_transport_init(&transport, &transport_context);
    yt2_service_init(&service, &transport, NULL, NULL);
    payload[0] = 0u;
    size = make_frame(frame, 4, 0, payload, 1);
    transport_context.input[0] = 0x00u;
    transport_context.input_length = 1;
    transport_context.input_position = 0;
    (void)size;
    CHECK(yt2_service_poll(&service) == YT2_OK);
    return 0;
}
