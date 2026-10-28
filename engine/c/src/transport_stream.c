#include "yt2/transport.h"

static int yt2_stream_write(void *context, const uint8_t *frame, uint32_t length)
{
    yt2_stream_transport *stream = (yt2_stream_transport *)context;
    size_t written;

    if (stream->file == NULL || length == 0u) {
        return YT2_ERR;
    }
    written = fwrite(frame, 1, (size_t)length, stream->file);
    if (written != (size_t)length) {
        return YT2_ERR;
    }
    stream->frames++;
    stream->bytes += length;
    return YT2_OK;
}

static int yt2_stream_read(void *context, uint8_t *destination, uint32_t capacity,
                           uint32_t *read)
{
    (void)context;
    (void)destination;
    (void)capacity;
    *read = 0;
    return YT2_OK;
}

static int yt2_stream_flush(void *context)
{
    yt2_stream_transport *stream = (yt2_stream_transport *)context;

    if (stream->file == NULL) {
        return YT2_ERR;
    }
    return fflush(stream->file) == 0 ? YT2_OK : YT2_ERR;
}

static int yt2_stream_take_wrap(void *context)
{
    (void)context;
    return 0;
}

static void yt2_stream_close(void *context)
{
    yt2_stream_transport *stream = (yt2_stream_transport *)context;

    if (stream->file != NULL) {
        fclose(stream->file);
        stream->file = NULL;
    }
}

int yt2_transport_stream_init(yt2_transport *transport,
                              yt2_stream_transport *context, const char *path)
{
    context->file = fopen(path, "wb");
    context->path = path;
    context->frames = 0;
    context->bytes = 0;
    if (context->file == NULL) {
        return YT2_ERR;
    }
    transport->context = context;
    transport->write_frame = yt2_stream_write;
    transport->read = yt2_stream_read;
    transport->flush = yt2_stream_flush;
    transport->take_wrap = yt2_stream_take_wrap;
    transport->close = yt2_stream_close;
    return YT2_OK;
}
