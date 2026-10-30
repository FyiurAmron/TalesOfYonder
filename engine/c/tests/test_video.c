#include "test.h"

#include "yt2/video.h"

#include <string.h>

typedef struct video_transport {
    uint8_t output[131072];
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

typedef struct video_event {
    uint8_t family;
    uint8_t codec;
    const uint8_t *payload;
    uint32_t length;
} video_event;

static int video_next_event(uint8_t *data, uint32_t total, uint32_t *offset,
                            video_event *event)
{
    yt2_frame_header header;

    if (*offset + YT2_FRAME_HEADER_SIZE > total) {
        return 0;
    }
    if (yt2_frame_header_parse(data + *offset, total - *offset, &header) != YT2_OK) {
        return 0;
    }
    event->family = header.family;
    event->codec = header.codec;
    event->payload = data + *offset + YT2_FRAME_HEADER_SIZE;
    event->length = header.length;
    *offset += YT2_FRAME_HEADER_SIZE + header.length;
    return 1;
}

/* Decode one codec-1 rect payload (the type byte is at offset 0). */
static int video_decode_runs(const uint8_t *payload, uint32_t length,
                             uint8_t *pixels, uint16_t width, uint16_t height)
{
    uint32_t at = 9u;

    for (uint16_t row = 0; row < height; row++) {
        uint16_t written = 0;
        uint8_t previous = 0;
        int have = 0;

        while (written < width) {
            uint8_t op;
            uint16_t count;
            uint8_t value;

            if (at >= length) {
                return 0;
            }
            op = payload[at++];
            if (op == 3u) {
                break;
            }
            if (op == 0u) {
                if (!have || at >= length) {
                    return 0;
                }
                count = payload[at++];
                value = previous;
            } else if (op == 1u) {
                if (at + 1u >= length) {
                    return 0;
                }
                count = payload[at++];
                value = payload[at++];
            } else if (op == 2u) {
                if (at >= length) {
                    return 0;
                }
                count = payload[at++];
                if (at + count > length || written + count > width) {
                    return 0;
                }
                memcpy(pixels + (uint32_t)row * width + written,
                       payload + at, count);
                at += count;
                written = (uint16_t)(written + count);
                previous = payload[at - 1u];
                have = 1;
                continue;
            } else {
                return 0;
            }
            if (written + count > width) {
                return 0;
            }
            memset(pixels + (uint32_t)row * width + written, value, count);
            written = (uint16_t)(written + count);
            previous = value;
            have = 1;
        }
        if (written != width) {
            return 0;
        }
    }
    return 1;
}

/* Check a one-row codec-1 frame's encoded form against the source row. */
static int video_check_rect(video_transport *context, uint32_t start,
                            const uint8_t *expected, uint16_t width,
                            uint8_t *encoded, uint32_t *encoded_length)
{
    uint8_t decoded[YT2_VIDEO_MAX_WIDTH];
    uint32_t offset = start;
    video_event event;
    int rects = 0;
    int ends = 0;

    while (video_next_event(context->output, context->output_length,
                            &offset, &event)) {
        if (event.family != 1u || event.length == 0u) {
            continue;
        }
        if (event.payload[0] == 2u) {
            rects++;
            if (event.codec != 1u || event.length < 9u ||
                !video_decode_runs(event.payload, event.length, decoded,
                                   width, 1) ||
                memcmp(decoded, expected, width) != 0) {
                return 0;
            }
            if (encoded != NULL) {
                *encoded_length = event.length - 9u;
                memcpy(encoded, event.payload + 9u, event.length - 9u);
            }
        } else if (event.payload[0] == 3u) {
            ends++;
        }
    }
    return rects == 1 && ends == 1;
}

int test_video_rle(void)
{
    static yt2_video video;
    video_transport context;
    yt2_transport transport;
    yt2_service service;
    uint8_t palette[YT2_VIDEO_PALETTE_SIZE];
    uint8_t encoded[YT2_VIDEO_MAX_WIDTH + 8u];
    uint32_t encoded_length = 0;
    uint32_t state = 0x12345678u;
    static const uint8_t one_value[] = {0x01u, 0xFFu, 0x11u, 0x00u, 0x41u, 0x03u};
    static const uint8_t mixed[] = {
        0x02u, 0x03u, 0x07u, 0x07u, 0x08u,
        0x01u, 0xFFu, 0x22u, 0x00u, 0x3Eu, 0x03u
    };
    static const uint8_t split[] = {
        0x01u, 0xFFu, 0x33u, 0x00u, 0x01u, 0x01u, 0x40u, 0x44u, 0x03u
    };

    for (uint32_t index = 0; index < YT2_VIDEO_PALETTE_SIZE; index++) {
        palette[index] = (uint8_t)(index & 0x3Fu);
    }
    video_transport_init(&transport, &context);
    yt2_service_init(&service, &transport, NULL, NULL);
    yt2_video_init(&video, 320, 1, 8);
    CHECK(yt2_video_set_palette(&video, 0, 256, palette) == YT2_OK);

    /* One value, longer than 255: 01 FF 11 then 00 41. */
    memset(video.pixels, 0x11, 320);
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.rects == 1u);
    CHECK(video_check_rect(&context, 0, video.pixels, 320, encoded,
                           &encoded_length));
    CHECK(encoded_length == sizeof(one_value));
    CHECK(memcmp(encoded, one_value, sizeof(one_value)) == 0);

    /* Two literals and a single literal, then a long run. */
    memset(video.pixels, 0x22, 320);
    video.pixels[0] = 0x07u;
    video.pixels[1] = 0x07u;
    video.pixels[2] = 0x08u;
    encoded_length = 0;
    {
        uint32_t start = context.output_length;

        CHECK(yt2_video_present(&video, &service) == YT2_OK);
        CHECK(video_check_rect(&context, start, video.pixels, 320, encoded,
                               &encoded_length));
    }
    CHECK(encoded_length == sizeof(mixed));
    CHECK(memcmp(encoded, mixed, sizeof(mixed)) == 0);

    /* A 256-pixel run splits into 01 FF and 00 01. */
    memset(video.pixels, 0x33, 256);
    memset(video.pixels + 256, 0x44, 64);
    encoded_length = 0;
    {
        uint32_t start = context.output_length;

        CHECK(yt2_video_present(&video, &service) == YT2_OK);
        CHECK(video_check_rect(&context, start, video.pixels, 320, encoded,
                               &encoded_length));
    }
    CHECK(encoded_length == sizeof(split));
    CHECK(memcmp(encoded, split, sizeof(split)) == 0);

    /* Literal chunks split at 255 and still round-trip. */
    for (uint16_t index = 0; index < 320; index++) {
        video.pixels[index] = (uint8_t)(index & 1u);
    }
    {
        uint32_t start = context.output_length;

        CHECK(yt2_video_present(&video, &service) == YT2_OK);
        CHECK(video_check_rect(&context, start, video.pixels, 320, NULL, NULL));
    }

    /* A full literal chunk remembers its last pixel for a following 00. */
    for (uint16_t index = 0; index < 255; index++) {
        video.pixels[index] = (uint8_t)((index & 1u) != 0u ? 0x00u : 0x07u);
    }
    for (uint16_t index = 255; index < 260; index++) {
        video.pixels[index] = 0x07u;
    }
    memset(video.pixels + 260, 0x22, 60);
    {
        uint32_t start = context.output_length;

        CHECK(yt2_video_present(&video, &service) == YT2_OK);
        CHECK(video_check_rect(&context, start, video.pixels, 320, NULL, NULL));
    }

    /* Noise has no runs and still round-trips inside the row bound. */
    for (uint16_t index = 0; index < 320; index++) {
        state = state * 1103515245u + 12345u;
        video.pixels[index] = (uint8_t)(state >> 16);
    }
    {
        uint32_t start = context.output_length;

        CHECK(yt2_video_present(&video, &service) == YT2_OK);
        CHECK(video_check_rect(&context, start, video.pixels, 320, encoded,
                               &encoded_length));
    }
    CHECK(encoded_length <= 320u + 8u);
    return 0;
}

/* The six 640-pixel rows from tests/seam-rle/rltest.asm, with the encoded
   lengths that harness measured from the asm encoder. */
int test_video_rle_width(void)
{
    static yt2_video video;
    video_transport context;
    yt2_transport transport;
    yt2_service service;
    uint8_t palette[YT2_VIDEO_PALETTE_SIZE];
    uint8_t encoded[YT2_VIDEO_MAX_WIDTH + 8u];
    uint32_t encoded_length;
    static const uint16_t expected[6] = {8u, 647u, 330u, 647u, 11u, 564u};

    for (uint32_t index = 0; index < YT2_VIDEO_PALETTE_SIZE; index++) {
        palette[index] = (uint8_t)(index & 0x3Fu);
    }
    video_transport_init(&transport, &context);
    yt2_service_init(&service, &transport, NULL, NULL);
    yt2_video_init(&video, 640, 1, 8);
    CHECK(yt2_video_set_palette(&video, 0, 256, palette) == YT2_OK);

    for (uint32_t row = 0; row < 6u; row++) {
        uint32_t start;
        uint16_t state = 0x1234u;

        switch (row) {
        case 0u:
            memset(video.pixels, 7, 640);
            break;
        case 1u:
            for (uint32_t index = 0; index < 640u; index++) {
                video.pixels[index] = (uint8_t)index;
            }
            break;
        case 2u:
            memset(video.pixels, 0, 640);
            for (uint32_t index = 0; index < 320u; index++) {
                video.pixels[index] = (uint8_t)((index & 1u) != 0u ? 0xFFu : 0u);
            }
            break;
        case 3u:
            for (uint32_t index = 0; index < 640u; index++) {
                state = (uint16_t)(state * 25173u + 13849u);
                video.pixels[index] = (uint8_t)(state >> 8);
            }
            break;
        case 4u:
            memset(video.pixels, 3, 300);
            memset(video.pixels + 300, 9, 340);
            break;
        default: {
            uint16_t value = 10;
            uint32_t at = 0;

            for (uint32_t group = 0; group < 80u; group++) {
                for (uint32_t count = 0; count < 3u; count++) {
                    video.pixels[at++] = (uint8_t)value;
                }
                for (uint32_t count = 0; count < 2u; count++) {
                    video.pixels[at++] = (uint8_t)(value + 1u);
                }
                for (uint32_t count = 0; count < 3u; count++) {
                    video.pixels[at++] = (uint8_t)(value + 2u);
                }
                value = (uint16_t)(value + 2u);
            }
            CHECK(at == 640u);
            break;
        }
        }
        start = context.output_length;
        encoded_length = 0;
        CHECK(yt2_video_present(&video, &service) == YT2_OK);
        CHECK(video_check_rect(&context, start, video.pixels, 640, encoded,
                               &encoded_length));
        CHECK(encoded_length == expected[row]);
    }
    return 0;
}

/* Parse one presented frame: its optional scroll and its one-row rects. */
static int video_frame_rows(video_transport *context, uint32_t start,
                            int *scroll_delta, int *has_scroll,
                            uint8_t *seen, int *rects, int *ends)
{
    uint32_t offset = start;
    video_event event;

    *scroll_delta = 0;
    *has_scroll = 0;
    *rects = 0;
    *ends = 0;
    while (video_next_event(context->output, context->output_length,
                            &offset, &event)) {
        if (event.family != 1u || event.length == 0u) {
            continue;
        }
        if (event.payload[0] == 5u) {
            if (*has_scroll || *rects != 0 || event.codec != 0u ||
                event.length != 2u) {
                return 0;
            }
            *has_scroll = 1;
            *scroll_delta = (int8_t)event.payload[1];
        } else if (event.payload[0] == 2u) {
            uint16_t x;
            uint16_t y;
            uint16_t width;
            uint16_t height;

            if (event.codec != 1u || event.length < 9u) {
                return 0;
            }
            x = yt2_le16(event.payload + 1);
            y = yt2_le16(event.payload + 3);
            width = yt2_le16(event.payload + 5);
            height = yt2_le16(event.payload + 7);
            if (x != 0u || width != 320u || height != 1u ||
                y >= YT2_VIDEO_MAX_HEIGHT || seen[y] != 0u) {
                return 0;
            }
            seen[y] = 1u;
            (*rects)++;
        } else if (event.payload[0] == 3u) {
            (*ends)++;
        }
    }
    return 1;
}

int test_video_scroll(void)
{
    static yt2_video video;
    video_transport context;
    yt2_transport transport;
    yt2_service service;
    uint8_t palette[YT2_VIDEO_PALETTE_SIZE];
    uint8_t seen[YT2_VIDEO_MAX_HEIGHT];
    uint32_t before;
    uint32_t frame_ends;
    int scroll_delta;
    int has_scroll;
    int rects;
    int ends;

    for (uint32_t index = 0; index < YT2_VIDEO_PALETTE_SIZE; index++) {
        palette[index] = (uint8_t)(index & 0x3Fu);
    }
    video_transport_init(&transport, &context);
    yt2_service_init(&service, &transport, NULL, NULL);
    yt2_video_init(&video, 320, 200, 8);
    CHECK(yt2_video_set_palette(&video, 0, 256, palette) == YT2_OK);
    for (uint16_t row = 0; row < 200; row++) {
        for (uint16_t column = 0; column < 320; column++) {
            video.pixels[(uint32_t)row * 320u + column] =
                (uint8_t)(row * 3u + (column & 7u));
        }
    }
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.rects == 200u);
    CHECK(video.scrolls == 0u);

    /* The picture moves down two rows; row 100 changed on its own. */
    memmove(video.pixels + 2u * 320u, video.pixels, 198u * 320u);
    memset(video.pixels, 0xF0u, 320u);
    memset(video.pixels + 320u, 0xF1u, 320u);
    memset(video.pixels + 100u * 320u, 0x5Au, 320u);
    before = context.output_length;
    memset(seen, 0, sizeof(seen));
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.scrolls == 1u);
    CHECK(video.rects == 203u);
    CHECK(video_frame_rows(&context, before, &scroll_delta, &has_scroll,
                           seen, &rects, &ends));
    CHECK(has_scroll == 1 && scroll_delta == 2);
    CHECK(rects == 3 && ends == 1);
    CHECK(seen[0] != 0u && seen[1] != 0u && seen[100] != 0u);

    /* The picture moves up three rows: one scroll, three incoming rows. */
    memmove(video.pixels, video.pixels + 3u * 320u, 197u * 320u);
    for (uint16_t row = 197; row < 200; row++) {
        memset(video.pixels + (uint32_t)row * 320u,
               (uint8_t)(0xE0u + row - 197u), 320u);
    }
    before = context.output_length;
    memset(seen, 0, sizeof(seen));
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.scrolls == 2u);
    CHECK(video.rects == 206u);
    CHECK(video_frame_rows(&context, before, &scroll_delta, &has_scroll,
                           seen, &rects, &ends));
    CHECK(has_scroll == 1 && scroll_delta == -3);
    CHECK(rects == 3 && ends == 1);
    CHECK(seen[197] != 0u && seen[198] != 0u && seen[199] != 0u);

    /* An unchanged screen presents nothing. */
    before = context.output_length;
    frame_ends = video.frame_ends;
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(context.output_length == before);
    CHECK(video.frame_ends == frame_ends);

    /* A repaint is not a pan: no scroll, every row goes out. */
    for (uint16_t row = 0; row < 200; row++) {
        memset(video.pixels + (uint32_t)row * 320u,
               (uint8_t)(0x80u + row), 320u);
    }
    before = context.output_length;
    memset(seen, 0, sizeof(seen));
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.scrolls == 2u);
    CHECK(video.rects == 406u);
    CHECK(video_frame_rows(&context, before, &scroll_delta, &has_scroll,
                           seen, &rects, &ends));
    CHECK(has_scroll == 0);
    CHECK(rects == 200 && ends == 1);
    for (uint16_t row = 0; row < 200; row++) {
        CHECK(seen[row] != 0u);
    }

    /* Eight rows is the largest recognised step. */
    memmove(video.pixels + 8u * 320u, video.pixels, 192u * 320u);
    for (uint16_t row = 0; row < 8; row++) {
        memset(video.pixels + (uint32_t)row * 320u,
               (uint8_t)(0xD0u + row), 320u);
    }
    before = context.output_length;
    memset(seen, 0, sizeof(seen));
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.scrolls == 3u);
    CHECK(video.rects == 414u);
    CHECK(video_frame_rows(&context, before, &scroll_delta, &has_scroll,
                           seen, &rects, &ends));
    CHECK(has_scroll == 1 && scroll_delta == 8);
    CHECK(rects == 8 && ends == 1);
    for (uint16_t row = 0; row < 8; row++) {
        CHECK(seen[row] != 0u);
    }

    /* Nine rows is a repaint fallback: no scroll, every row goes out. */
    memmove(video.pixels + 9u * 320u, video.pixels, 191u * 320u);
    for (uint16_t row = 0; row < 9; row++) {
        memset(video.pixels + (uint32_t)row * 320u,
               (uint8_t)(0xC0u + row), 320u);
    }
    before = context.output_length;
    memset(seen, 0, sizeof(seen));
    CHECK(yt2_video_present(&video, &service) == YT2_OK);
    CHECK(video.scrolls == 3u);
    CHECK(video.rects == 614u);
    CHECK(video_frame_rows(&context, before, &scroll_delta, &has_scroll,
                           seen, &rects, &ends));
    CHECK(has_scroll == 0);
    CHECK(rects == 200 && ends == 1);
    for (uint16_t row = 0; row < 200; row++) {
        CHECK(seen[row] != 0u);
    }
    return 0;
}
