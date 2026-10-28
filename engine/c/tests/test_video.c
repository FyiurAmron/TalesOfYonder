#include "test.h"

#include "yt2/video.h"

#include <string.h>

typedef struct video_transport {
    uint8_t output[32768];
    uint32_t output_length;
} video_transport;

static int video_write(void *context, const uint8_t *frame, uint32_t length)
{
    video_transport *transport = (video_transport *)context;

    if (transport->output_length + length > sizeof(transport->output)) {
        return YT2_ERR;
    }
    for (uint32_t index = 0; index < length; index++) {
        transport->output[transport->output_length + index] = frame[index];
    }
    transport->output_length += length;
    return YT2_OK;
}

static int video_read(void *context, uint8_t *destination, uint32_t capacity,
                      uint32_t *read)
{
    (void)context;
    (void)destination;
    (void)capacity;
    *read = 0;
    return YT2_OK;
}

static int video_flush(void *context)
{
    (void)context;
    return YT2_OK;
}

static int video_wrap(void *context)
{
    (void)context;
    return 0;
}

static void video_close(void *context)
{
    (void)context;
}

static void video_transport_init(yt2_transport *transport,
                                 video_transport *context)
{
    memset(context, 0, sizeof(*context));
    transport->context = context;
    transport->write_frame = video_write;
    transport->read = video_read;
    transport->flush = video_flush;
    transport->take_wrap = video_wrap;
    transport->close = video_close;
}

static uint32_t video_parse(uint8_t *data, uint32_t length, uint32_t *offset,
                            uint8_t family, uint8_t type, int *found)
{
    yt2_frame_reader reader;
    yt2_frame_view view;
    uint32_t start = *offset;

    yt2_frame_reader_init(&reader, data + start, length - start);
    if (yt2_frame_reader_next(&reader, &view) != 1) {
        *found = 0;
        return 0;
    }
    *found = view.header.family == family && view.payload[0] == type;
    *offset = start + YT2_FRAME_HEADER_SIZE + view.header.length;
    return view.header.length;
}

static int video_expect(uint8_t *data, uint32_t length, uint32_t *offset,
                        uint8_t family, uint8_t type)
{
    int found;

    (void)video_parse(data, length, offset, family, type, &found);
    return found;
}

int test_video(void)
{
    static yt2_video video;
    video_transport context;
    yt2_transport transport;
    yt2_service service;
    uint8_t palette[YT2_VIDEO_PALETTE_SIZE];
    uint8_t first = 0;
    uint8_t second = 0;
    uint8_t third = 0;
    uint32_t offset;

    for (uint32_t index = 0; index < YT2_VIDEO_PALETTE_SIZE; index++) {
        palette[index] = (uint8_t)(index & 0x3Fu);
    }
    video_transport_init(&transport, &context);
    yt2_service_init(&service, &transport, NULL, NULL);
    yt2_video_init(&video, 4, 2, 8);
    CHECK(yt2_video_set_palette(&video, 0, 256, palette) == YT2_OK);

    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.frames == 1u && video.rects == 2u && video.frame_ends == 1u);
    CHECK(video.palette_messages == 1u);
    offset = 0;
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 0));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 2));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 2));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 1));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 3));
    CHECK(offset == context.output_length);

    /* One changed pixel sends exactly one row and closes the frame. */
    video.pixels[0] = 9u;
    offset = context.output_length;
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.rects == 3u && video.frame_ends == 2u);
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 2));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 3));
    CHECK(offset == context.output_length);

    /* A palette-only change sends the minimal span and still presents. */
    first = video.palette[3];
    second = video.palette[4];
    third = video.palette[5];
    video.palette[3] = (uint8_t)(first + 1u);
    video.palette[4] = (uint8_t)(second + 1u);
    video.palette[5] = (uint8_t)(third + 1u);
    offset = context.output_length;
    {
        uint32_t palette_start = offset;

        CHECK(yt2_video_present(&video, &service) == YT2_OK);
        CHECK(video.rects == 3u && video.frame_ends == 3u);
        CHECK(video.palette_messages == 2u);
        CHECK(video_expect(context.output, context.output_length, &offset, 1, 1));
        /* Palette start/count are the second and third payload bytes. */
        CHECK(context.output[palette_start + YT2_FRAME_HEADER_SIZE] == 1u);
        CHECK(yt2_le16(context.output + palette_start + YT2_FRAME_HEADER_SIZE + 1) == 1u);
        CHECK(yt2_le16(context.output + palette_start + YT2_FRAME_HEADER_SIZE + 3) == 1u);
    }
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 3));
    CHECK(offset == context.output_length);

    /* A keyframe resends the mode, the full palette and every row. */
    yt2_video_force_keyframe(&video);
    offset = context.output_length;
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.frames == 4u && video.rects == 5u);
    CHECK(video.palette_messages == 3u && video.frame_ends == 4u);
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 0));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 2));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 2));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 1));
    CHECK(video_expect(context.output, context.output_length, &offset, 1, 3));
    CHECK(offset == context.output_length);

    /* Invalid modes/ranges are refused, not clamped. */
    CHECK(yt2_video_set_mode(&video, 0, 200, 8) == YT2_ERR);
    CHECK(yt2_video_set_mode(&video, 641, 200, 8) == YT2_ERR);
    CHECK(yt2_video_set_palette(&video, 255, 2, palette) == YT2_ERR);
    return 0;
}
