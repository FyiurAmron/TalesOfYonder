#include "yt2/transport.h"

static int yt2_null_write(void *context, const uint8_t *frame, uint32_t length)
{
    yt2_null_transport *null_transport = (yt2_null_transport *)context;

    (void)frame;
    if (length == 0u) {
        return YT2_ERR;
    }
    null_transport->frames++;
    null_transport->bytes += length;
    return YT2_OK;
}

static int yt2_null_read(void *context, uint8_t *destination, uint32_t capacity,
                         uint32_t *read)
{
    (void)context;
    (void)destination;
    (void)capacity;
    *read = 0;
    return YT2_OK;
}

static int yt2_null_flush(void *context)
{
    (void)context;
    return YT2_OK;
}

static int yt2_null_take_wrap(void *context)
{
    (void)context;
    return 0;
}

static void yt2_null_close(void *context)
{
    (void)context;
}

void yt2_transport_null_init(yt2_transport *transport,
                             yt2_null_transport *context)
{
    context->frames = 0;
    context->bytes = 0;
    transport->context = context;
    transport->write_frame = yt2_null_write;
    transport->read = yt2_null_read;
    transport->flush = yt2_null_flush;
    transport->take_wrap = yt2_null_take_wrap;
    transport->close = yt2_null_close;
}
