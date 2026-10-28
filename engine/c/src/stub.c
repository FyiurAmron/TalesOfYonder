#include "yt2/platform.h"
#include "yt2/service.h"
#include "yt2/transport.h"
#include "yt2/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct yt2_stub {
    yt2_service service;
    yt2_transport transport;
    yt2_folder_transport folder;
    yt2_stream_transport stream;
    yt2_null_transport null_transport;
    yt2_video video;

    int quit;
    int force_keyframe;
    int verbose;
    uint32_t frame;
    uint32_t marker_x;
    uint32_t marker_y;
    uint8_t marker_color;
} yt2_stub;

static yt2_stub g_stub;

static uint8_t yt2_stub_background(uint16_t x, uint16_t y)
{
    uint8_t left = (uint8_t)((x >> 4) & 0x0Fu);
    uint8_t right = (uint8_t)((y >> 3) & 0x0Fu);

    return (uint8_t)(16u + (uint8_t)((left ^ right) & 0x0Fu));
}

static void yt2_stub_fill_background(yt2_stub *stub, uint32_t x0, uint32_t y0,
                                     uint32_t width, uint32_t height)
{
    uint32_t y;
    uint32_t x;

    for (y = 0; y < height; y++) {
        uint32_t row = (y0 + y) * stub->video.width;

        for (x = 0; x < width; x++) {
            stub->video.pixels[row + x0 + x] =
                yt2_stub_background((uint16_t)(x0 + x), (uint16_t)(y0 + y));
        }
    }
}

static void yt2_stub_draw_marker(yt2_stub *stub)
{
    uint32_t y;
    uint32_t x;
    uint32_t top = stub->marker_y * stub->video.width;

    for (y = 0; y < 16u; y++) {
        for (x = 0; x < 16u; x++) {
            uint8_t color = stub->marker_color;

            if (x == 0u || y == 0u || x == 15u || y == 15u) {
                color = 3;
            } else if (x >= 5u && x <= 10u && y >= 5u && y <= 10u) {
                color = 8;
            }
            stub->video.pixels[top + stub->marker_x + y * stub->video.width + x] =
                color;
        }
    }
}

static void yt2_stub_place_marker(yt2_stub *stub, uint32_t x, uint32_t y)
{
    if (x + 16u > stub->video.width) {
        x = stub->video.width - 16u;
    }
    if (y + 16u > stub->video.height) {
        y = stub->video.height - 16u;
    }
    yt2_stub_fill_background(stub, stub->marker_x, stub->marker_y, 16u, 16u);
    stub->marker_x = x;
    stub->marker_y = y;
    yt2_stub_draw_marker(stub);
}

static void yt2_stub_move_marker(yt2_stub *stub, int32_t dx, int32_t dy)
{
    int32_t x = (int32_t)stub->marker_x + dx;
    int32_t y = (int32_t)stub->marker_y + dy;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    yt2_stub_place_marker(stub, (uint32_t)x, (uint32_t)y);
}

static void yt2_stub_on_key(void *context, uint8_t scan, uint8_t ascii)
{
    yt2_stub *stub = (yt2_stub *)context;
    int32_t dx = (int32_t)(scan & 7u) * 8;
    int32_t dy = (int32_t)(ascii & 3u) * 8;

    if (dx == 0) {
        dx = 8;
    }
    if (dy == 0) {
        dy = 8;
    }
    yt2_stub_move_marker(stub, dx, dy);
    printf("yt2stub: key scan=0x%02X ascii=0x%02X\n",
           (unsigned)scan, (unsigned)ascii);
}

static void yt2_stub_on_mouse_move(void *context, int16_t dx, int16_t dy)
{
    yt2_stub *stub = (yt2_stub *)context;

    yt2_stub_move_marker(stub, (int32_t)dx * 4, (int32_t)dy * 4);
    if (stub->verbose) {
        printf("yt2stub: mouse move %d,%d\n", (int)dx, (int)dy);
    }
}

static void yt2_stub_on_mouse_abs(void *context, uint16_t x, uint16_t y)
{
    yt2_stub *stub = (yt2_stub *)context;

    yt2_stub_place_marker(stub, x, y);
    if (stub->verbose) {
        printf("yt2stub: mouse abs %u,%u\n", (unsigned)x, (unsigned)y);
    }
}

static void yt2_stub_on_mouse_button(void *context, uint8_t button, uint8_t down)
{
    yt2_stub *stub = (yt2_stub *)context;

    if (down != 0u) {
        stub->marker_color = (button == 0u) ? 2u : 1u;
        yt2_stub_fill_background(stub, stub->marker_x, stub->marker_y, 16u, 16u);
        yt2_stub_draw_marker(stub);
    }
    printf("yt2stub: mouse button %u %s\n", (unsigned)button,
           down != 0u ? "down" : "up");
}

static void yt2_stub_on_quit(void *context)
{
    yt2_stub *stub = (yt2_stub *)context;

    stub->quit = 1;
    printf("yt2stub: quit\n");
}

static void yt2_stub_on_hello(void *context, uint8_t role, uint8_t version)
{
    (void)context;
    printf("yt2stub: client hello role=%u version=%u\n", (unsigned)role,
           (unsigned)version);
}

static void yt2_stub_on_keyframe(void *context)
{
    yt2_stub *stub = (yt2_stub *)context;

    stub->force_keyframe = 1;
    printf("yt2stub: keyframe request\n");
}

static void yt2_stub_on_unknown(void *context, uint8_t family, uint8_t type)
{
    (void)context;
    printf("yt2stub: unknown input family=%u type=%u\n", (unsigned)family,
           (unsigned)type);
}

static void yt2_stub_setup_handlers(yt2_stub *stub, yt2_input_handlers *handlers)
{
    memset(handlers, 0, sizeof(*handlers));
    handlers->key = yt2_stub_on_key;
    handlers->mouse_move = yt2_stub_on_mouse_move;
    handlers->mouse_button = yt2_stub_on_mouse_button;
    handlers->mouse_abs = yt2_stub_on_mouse_abs;
    handlers->quit = yt2_stub_on_quit;
    handlers->hello = yt2_stub_on_hello;
    handlers->keyframe_request = yt2_stub_on_keyframe;
    handlers->unknown = yt2_stub_on_unknown;
    (void)stub;
}

static void yt2_stub_init_palette(yt2_stub *stub)
{
    uint8_t palette[YT2_VIDEO_PALETTE_SIZE];
    uint32_t entry;

    for (entry = 0; entry < 256u; entry++) {
        palette[entry * 3u] = (uint8_t)((entry & 7u) * 9u);
        palette[entry * 3u + 1u] = (uint8_t)(((entry >> 3) & 7u) * 9u);
        palette[entry * 3u + 2u] = (uint8_t)(((entry >> 6) & 3u) * 21u);
    }
    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 63;
    palette[4] = 0;
    palette[5] = 0;
    palette[6] = 0;
    palette[7] = 63;
    palette[8] = 0;
    palette[9] = 63;
    palette[10] = 63;
    palette[11] = 63;
    (void)yt2_video_set_palette(&stub->video, 0, 256, palette);
}

static void yt2_stub_animate_palette(yt2_stub *stub)
{
    uint8_t palette[6];
    uint32_t frame = stub->frame;

    palette[0] = (uint8_t)(32u + ((frame * 7u) & 31u));
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 0;
    palette[4] = (uint8_t)(32u + ((frame * 5u) & 31u));
    palette[5] = 0;
    (void)yt2_video_set_palette(&stub->video, 1, 2, palette);
}

static int yt2_stub_open_transport(yt2_stub *stub, const char *kind,
                                   const char *output, const char *input,
                                   int input_from_start)
{
    if (strcmp(kind, "folder") == 0) {
        if (yt2_transport_folder_init(&stub->transport, &stub->folder, output,
                                      input, input_from_start) != YT2_OK) {
            fprintf(stderr, "yt2stub: cannot open %s\n", output);
            return YT2_ERR;
        }
        printf("yt2stub: folder output=%s input=%s\n", output, input);
        return YT2_OK;
    }
    if (strcmp(kind, "stream") == 0) {
        if (yt2_transport_stream_init(&stub->transport, &stub->stream, output) !=
            YT2_OK) {
            fprintf(stderr, "yt2stub: cannot open %s\n", output);
            return YT2_ERR;
        }
        printf("yt2stub: stream output=%s\n", output);
        return YT2_OK;
    }
    if (strcmp(kind, "null") == 0) {
        yt2_transport_null_init(&stub->transport, &stub->null_transport);
        printf("yt2stub: null transport\n");
        return YT2_OK;
    }
    fprintf(stderr, "yt2stub: unknown transport '%s'\n", kind);
    return YT2_ERR;
}

static int yt2_stub_run_demo(yt2_stub *stub, uint32_t frames, uint32_t fps,
                             int no_wait)
{
    yt2_input_handlers handlers;

    yt2_stub_setup_handlers(stub, &handlers);
    yt2_service_init(&stub->service, &stub->transport, &handlers, stub);
    yt2_video_init(&stub->video, 320, 200, 8);
    yt2_stub_init_palette(stub);
    yt2_stub_fill_background(stub, 0, 0, 320, 200);
    stub->marker_x = 16;
    stub->marker_y = 16;
    stub->marker_color = 2;
    yt2_stub_draw_marker(stub);
    if (yt2_service_control_hello(&stub->service, 0, 0) != YT2_OK) {
        return YT2_ERR;
    }
    for (;;) {
        if (stub->force_keyframe) {
            stub->force_keyframe = 0;
            yt2_video_force_keyframe(&stub->video);
        }
        yt2_stub_animate_palette(stub);
        if (yt2_video_present(&stub->video, &stub->service) != YT2_OK ||
            yt2_service_flush(&stub->service) != YT2_OK) {
            return YT2_ERR;
        }
        stub->frame++;
        if (stub->verbose) {
            printf("yt2stub: frame %u rects=%u palette=%u\n",
                   (unsigned)stub->frame, (unsigned)stub->video.rects,
                   (unsigned)stub->video.palette_messages);
        }
        if (frames != 0u && stub->frame >= frames) {
            break;
        }
        if (yt2_service_poll(&stub->service) != YT2_OK) {
            return YT2_ERR;
        }
        if (stub->quit) {
            break;
        }
        if (yt2_service_take_wrap(&stub->service) != 0) {
            yt2_video_force_keyframe(&stub->video);
        }
        if (!no_wait && fps > 0u) {
            yt2_platform_sleep_ms(1000u / fps);
        }
    }
    if (yt2_service_flush(&stub->service) != YT2_OK) {
        return YT2_ERR;
    }
    printf("yt2stub: done frames=%u records=%u\n", (unsigned)stub->frame,
           (unsigned)stub->service.sequence);
    return YT2_OK;
}

static int yt2_stub_payload_matches(const uint8_t *expected, int expected_length,
                                    const uint8_t *actual, int actual_length)
{
    if (expected_length != actual_length) {
        fprintf(stderr, "yt2stub: replay length mismatch %d != %d\n",
                expected_length, actual_length);
        return 0;
    }
    for (int index = 0; index < expected_length; index++) {
        if (expected[index] != actual[index]) {
            fprintf(stderr, "yt2stub: replay byte %d differs: %02X != %02X\n",
                    index, (unsigned)expected[index], (unsigned)actual[index]);
            return 0;
        }
    }
    return 1;
}

static int yt2_stub_replay_video(yt2_stub *stub, const yt2_frame_view *view)
{
    const uint8_t *payload = view->payload;
    uint32_t length = view->header.length;
    uint8_t encoded[YT2_MAX_PAYLOAD];

    if (length == 0u) {
        return YT2_ERR;
    }
    switch (payload[0]) {
    case YT2_VIDEO_SET_MODE: {
        uint16_t width;
        uint16_t height;
        int size;

        if (length != 6u) {
            return YT2_ERR;
        }
        width = yt2_le16(payload + 1);
        height = yt2_le16(payload + 3);
        size = yt2_payload_video_set_mode(encoded, width, height, payload[5]);
        if (!yt2_stub_payload_matches(payload, (int)length, encoded, size)) {
            return YT2_ERR;
        }
        return yt2_service_video_set_mode(&stub->service, width, height, payload[5]);
    }
    case YT2_VIDEO_PALETTE: {
        uint16_t start;
        uint16_t count;
        int size;

        if (length < 5u) {
            return YT2_ERR;
        }
        start = yt2_le16(payload + 1);
        count = yt2_le16(payload + 3);
        if (length != 5u + (uint32_t)count * 3u) {
            return YT2_ERR;
        }
        size = yt2_payload_video_palette(encoded, start, count, payload + 5);
        if (!yt2_stub_payload_matches(payload, (int)length, encoded, size)) {
            return YT2_ERR;
        }
        return yt2_service_video_palette(&stub->service, start, count, payload + 5);
    }
    case YT2_VIDEO_RECT: {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
        uint32_t pixels;
        int size;

        if (length < 9u || view->header.codec != 0u) {
            return YT2_ERR;
        }
        x = yt2_le16(payload + 1);
        y = yt2_le16(payload + 3);
        width = yt2_le16(payload + 5);
        height = yt2_le16(payload + 7);
        pixels = (uint32_t)width * (uint32_t)height;
        if (length != 9u + pixels) {
            return YT2_ERR;
        }
        size = yt2_payload_video_rect(encoded, x, y, width, height, payload + 9);
        if (!yt2_stub_payload_matches(payload, (int)length, encoded, size)) {
            return YT2_ERR;
        }
        return yt2_service_video_rect(&stub->service, 0, x, y, width, height,
                                      payload + 9);
    }
    case YT2_VIDEO_FRAME_END:
        if (length != 1u) {
            return YT2_ERR;
        }
        return yt2_service_video_frame_end(&stub->service);
    case YT2_VIDEO_RESET:
        if (length != 1u) {
            return YT2_ERR;
        }
        return yt2_service_video_reset(&stub->service);
    case YT2_VIDEO_SCROLL:
        if (length != 2u) {
            return YT2_ERR;
        }
        return yt2_service_video_scroll(&stub->service, (int8_t)payload[1]);
    default:
        return YT2_ERR;
    }
}

static int yt2_stub_replay_audio(yt2_stub *stub, const yt2_frame_view *view)
{
    const uint8_t *payload = view->payload;
    uint32_t length = view->header.length;
    uint8_t encoded[YT2_MAX_PAYLOAD];
    int size = YT2_ERR;

    if (length == 0u) {
        return YT2_ERR;
    }
    switch (payload[0]) {
    case YT2_AUDIO_OPL_WRITE:
        if (length == 5u) {
            size = yt2_payload_audio_opl_write(encoded, yt2_le16(payload + 1),
                                               payload[3], payload[4]);
        }
        break;
    case YT2_AUDIO_OPL_BATCH: {
        uint8_t count;

        if (length < 2u) {
            return YT2_ERR;
        }
        count = payload[1];
        if (length == 2u + (uint32_t)count * 4u) {
            size = yt2_payload_audio_opl_batch(encoded, count, payload + 2);
        }
        break;
    }
    case YT2_AUDIO_CTRL:
        if (length == 6u) {
            size = yt2_payload_audio_ctrl(encoded, yt2_le16(payload + 1),
                                          yt2_le16(payload + 3), payload[5]);
        }
        break;
    case YT2_AUDIO_OPL_BATCH_TIMED: {
        uint8_t count;

        if (length < 4u) {
            return YT2_ERR;
        }
        count = payload[3];
        if (length == 4u + (uint32_t)count * 6u) {
            size = yt2_payload_audio_opl_batch_timed(encoded, yt2_le16(payload + 1),
                                                     count, payload + 4);
        }
        break;
    }
    case YT2_AUDIO_SPEAKER:
        if (length == 8u) {
            size = yt2_payload_audio_speaker(encoded, payload[1],
                                             yt2_le16(payload + 2),
                                             yt2_le32(payload + 4));
        }
        break;
    case YT2_AUDIO_SAMPLE:
        if (length == 11u) {
            size = yt2_payload_audio_sample(encoded, yt2_le16(payload + 1),
                                            yt2_le32(payload + 3),
                                            yt2_le32(payload + 7));
        }
        break;
    default:
        return YT2_ERR;
    }
    if (size < 0 ||
        !yt2_stub_payload_matches(payload, (int)length, encoded, size)) {
        return YT2_ERR;
    }
    return yt2_service_send_payload(&stub->service, (uint8_t)YT2_FAMILY_AUDIO,
                                    view->header.codec, encoded, (uint32_t)size);
}

static int yt2_stub_replay_control(yt2_stub *stub, const yt2_frame_view *view)
{
    const uint8_t *payload = view->payload;
    uint32_t length = view->header.length;
    uint8_t encoded[YT2_MAX_PAYLOAD];
    int size = YT2_ERR;

    if (length == 0u) {
        return YT2_ERR;
    }
    switch (payload[0]) {
    case YT2_CONTROL_HELLO:
        if (length == 3u) {
            size = yt2_payload_control_hello(encoded, payload[1], payload[2]);
        }
        break;
    case YT2_CONTROL_BYE:
        if (length == 1u) {
            size = yt2_payload_control_bye(encoded);
        }
        break;
    case YT2_CONTROL_ERROR:
        if (length >= 3u) {
            char text[YT2_MAX_PAYLOAD];
            uint32_t count = length - 3u;

            if (count >= sizeof(text)) {
                return YT2_ERR;
            }
            memcpy(text, payload + 3, count);
            text[count] = '\0';
            size = yt2_payload_control_error(encoded, yt2_le16(payload + 1), text);
        }
        break;
    case YT2_CONTROL_DEBUG:
        if (length >= 2u) {
            size = yt2_payload_control_debug(encoded, payload[1], payload + 2,
                                             length - 2u);
        }
        break;
    case YT2_CONTROL_KEYFRAME_REQUEST:
        if (length == 1u) {
            size = yt2_payload_control_keyframe(encoded);
        }
        break;
    default:
        return YT2_ERR;
    }
    if (size < 0 ||
        !yt2_stub_payload_matches(payload, (int)length, encoded, size)) {
        return YT2_ERR;
    }
    return yt2_service_send_payload(&stub->service, (uint8_t)YT2_FAMILY_CONTROL,
                                    view->header.codec, encoded, (uint32_t)size);
}

static uint8_t *yt2_stub_load_file(const char *path, uint32_t *size)
{
    FILE *file;
    long length;
    uint8_t *data;

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length <= 0 || length > 0x1000000L) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    data = (uint8_t *)malloc((size_t)length);
    if (data == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (uint32_t)length;
    return data;
}

static int yt2_stub_replay(yt2_stub *stub, const char *path)
{
    uint32_t size = 0;
    uint8_t *data = yt2_stub_load_file(path, &size);
    yt2_frame_reader reader;
    yt2_frame_view view;
    uint32_t frames = 0;

    if (data == NULL) {
        fprintf(stderr, "yt2stub: cannot read %s\n", path);
        return YT2_ERR;
    }
    yt2_service_init(&stub->service, &stub->transport, NULL, NULL);
    yt2_frame_reader_init(&reader, data, size);
    for (;;) {
        int status = yt2_frame_reader_next(&reader, &view);

        if (status == 0) {
            break;
        }
        if (status < 0) {
            fprintf(stderr, "yt2stub: invalid v0 frame at %u\n",
                    (unsigned)reader.position);
            free(data);
            return YT2_ERR;
        }
        if (view.header.flags != 0u || view.header.sequence != stub->service.sequence) {
            fprintf(stderr, "yt2stub: replay header mismatch at frame %u\n",
                    (unsigned)frames);
            free(data);
            return YT2_ERR;
        }
        if (view.header.family == (uint8_t)YT2_FAMILY_VIDEO) {
            if (yt2_stub_replay_video(stub, &view) != YT2_OK) {
                free(data);
                return YT2_ERR;
            }
        } else if (view.header.family == (uint8_t)YT2_FAMILY_AUDIO) {
            if (yt2_stub_replay_audio(stub, &view) != YT2_OK) {
                free(data);
                return YT2_ERR;
            }
        } else if (view.header.family == (uint8_t)YT2_FAMILY_CONTROL) {
            if (yt2_stub_replay_control(stub, &view) != YT2_OK) {
                free(data);
                return YT2_ERR;
            }
        } else {
            fprintf(stderr, "yt2stub: unsupported replay family %u\n",
                    (unsigned)view.header.family);
            free(data);
            return YT2_ERR;
        }
        frames++;
    }
    free(data);
    if (yt2_service_flush(&stub->service) != YT2_OK) {
        return YT2_ERR;
    }
    printf("yt2stub: replay frames=%u records=%u bytes=%u\n", (unsigned)frames,
           (unsigned)stub->service.sequence, (unsigned)size);
    return YT2_OK;
}

static void yt2_stub_usage(const char *program)
{
    printf("usage: %s [options]\n", program);
    printf("  --transport folder|stream|null  where frames go (default folder)\n");
    printf("  --output PATH                   OUTPUT.BIN or the raw recording\n");
    printf("  --input PATH                    INPUT.BIN (folder transport)\n");
    printf("  --input-start                   consume existing input from the start\n");
    printf("  --frames N                      stop after N frames; 0 = until quit\n");
    printf("  --fps N                         frame pacing (default 24)\n");
    printf("  --no-wait                       do not sleep between frames\n");
    printf("  --replay-v0 PATH                re-emit a raw v0 recording and verify it\n");
    printf("  --verbose                       per-frame logging\n");
    printf("  --help                          this text\n");
}

int main(int argc, char **argv)
{
    yt2_stub *stub = &g_stub;
    const char *transport = "folder";
    const char *output = NULL;
    const char *input = "INPUT.BIN";
    const char *replay = NULL;
    uint32_t frames = 12;
    uint32_t fps = 24;
    int no_wait = 0;
    int input_from_start = 0;
    int index;

    setvbuf(stdout, NULL, _IOLBF, 0);
    memset(stub, 0, sizeof(*stub));
    for (index = 1; index < argc; index++) {
        const char *argument = argv[index];

        if (strcmp(argument, "--transport") == 0 && index + 1 < argc) {
            transport = argv[++index];
        } else if (strcmp(argument, "--output") == 0 && index + 1 < argc) {
            output = argv[++index];
        } else if (strcmp(argument, "--input") == 0 && index + 1 < argc) {
            input = argv[++index];
        } else if (strcmp(argument, "--input-start") == 0) {
            input_from_start = 1;
        } else if (strcmp(argument, "--frames") == 0 && index + 1 < argc) {
            frames = (uint32_t)strtoul(argv[++index], NULL, 10);
        } else if (strcmp(argument, "--fps") == 0 && index + 1 < argc) {
            fps = (uint32_t)strtoul(argv[++index], NULL, 10);
        } else if (strcmp(argument, "--no-wait") == 0) {
            no_wait = 1;
        } else if (strcmp(argument, "--replay-v0") == 0 && index + 1 < argc) {
            replay = argv[++index];
        } else if (strcmp(argument, "--verbose") == 0) {
            stub->verbose = 1;
        } else if (strcmp(argument, "--help") == 0) {
            yt2_stub_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "yt2stub: unknown option '%s'\n", argument);
            yt2_stub_usage(argv[0]);
            return 2;
        }
    }
    if (strcmp(transport, "folder") == 0) {
        if (output == NULL) {
            output = "OUTPUT.BIN";
        }
    } else if (strcmp(transport, "stream") == 0) {
        if (output == NULL) {
            output = "OUTPUT.v0";
        }
    } else if (strcmp(transport, "null") != 0) {
        fprintf(stderr, "yt2stub: unknown transport '%s'\n", transport);
        return 2;
    }
    if (yt2_stub_open_transport(stub, transport, output, input,
                                input_from_start) != YT2_OK) {
        return 1;
    }
    if (replay != NULL) {
        if (yt2_stub_replay(stub, replay) != YT2_OK) {
            yt2_transport_close(&stub->transport);
            return 1;
        }
        yt2_transport_close(&stub->transport);
        return 0;
    }
    if (yt2_stub_run_demo(stub, frames, fps, no_wait) != YT2_OK) {
        yt2_transport_close(&stub->transport);
        return 1;
    }
    yt2_transport_close(&stub->transport);
    return 0;
}
