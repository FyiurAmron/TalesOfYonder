/*
 * yt2p-intro: run the scripted start-up sequence headless.
 *
 *   yt2p-intro PICTURES.VGA WORLD.DAT [--frames N] [--input SCRIPT]
 *       [--dump-frames DIR] [--no-cursor] [--flag-p]
 *
 * Prints one `frame tick W H idx-fnv pal-fnv` line per emission (B1
 * present.log format plus dimensions; the VBE logo is 640x480, the rest
 * 320x200) and the audio trigger log plus the exit transition on stderr.
 * The input script stamps raw inputs (`<tick> key <code>`,
 * `<tick> click <x> <y>`); a poll consumes every event at or before the
 * current tick, like the original's DOS typeahead queue.  With --pxdiff
 * each line gains the pixels-changed count against the previous emission,
 * so the gate can tell single-pixel steps from recomposes without dumps.
 */
#include "yt2/intro.h"
#include "yt2/titlemenu.h"
#include "yt2/palette.h"
#include "yt2/pictures.h"
#include "yt2/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define INTRO_FRAME_SIZE (320u * 200u)

typedef struct input_event {
    unsigned long tick;
    int is_click;
    unsigned int key;
    int x;
    int y;
} input_event;

typedef struct input_queue {
    input_event *events;
    size_t count;
    size_t next;
    yt2_intro *intro;
} input_queue;

static uint64_t checksum(const uint8_t *bytes, size_t size)
{
    uint64_t value = UINT64_C(0xCBF29CE484222325);

    for (size_t index = 0; index < size; index++) {
        value ^= bytes[index];
        value *= UINT64_C(0x100000001B3);
    }
    return value;
}

typedef struct emit_state {
    unsigned long frames;
    unsigned long cap;
    const char *dump_dir;
    int pxdiff;
    uint8_t *previous;
    size_t previous_size;
    int have_previous;
} emit_state;

typedef struct tool_context {
    emit_state emit;
    input_queue queue;
} tool_context;

static int emit_sized(const uint8_t *frame, const uint8_t *palette,
                        unsigned int width, unsigned int height,
                        unsigned long tick, void *user)
{
    emit_state *state = &((tool_context *)user)->emit;
    size_t size = (size_t)width * height;

    printf("%lu %lu %u %u %016llX %016llX", state->frames, tick, width,
           height, (unsigned long long)checksum(frame, size),
           (unsigned long long)checksum(palette, YT2_PALETTE_SIZE));
    if (state->pxdiff) {
        unsigned long changed = (unsigned long)size;

        if (state->have_previous && state->previous_size == size) {
            changed = 0;
            for (size_t i = 0; i < size; i++) {
                if (frame[i] != state->previous[i]) {
                    changed++;
                }
            }
        }
        printf(" %lu", changed);
        if (state->previous != NULL) {
            memcpy(state->previous, frame, size);
            state->previous_size = size;
            state->have_previous = 1;
        }
    }
    putchar('\n');
    if (state->dump_dir != NULL) {
        char path[1024];
        FILE *stream;

        snprintf(path, sizeof(path), "%s/frame_%05lu.idx", state->dump_dir,
                 state->frames);
        stream = fopen(path, "wb");
        if (stream != NULL) {
            fwrite(frame, 1, size, stream);
            fclose(stream);
        }
        snprintf(path, sizeof(path), "%s/frame_%05lu.pal", state->dump_dir,
                 state->frames);
        stream = fopen(path, "wb");
        if (stream != NULL) {
            fwrite(palette, 1, YT2_PALETTE_SIZE, stream);
            fclose(stream);
        }
    }
    state->frames++;
    return state->frames >= state->cap;
}

static int emit_frame(const uint8_t *frame, const uint8_t *palette,
                      unsigned long tick, void *user)
{
    return emit_sized(frame, palette, YT2_INTRO_WIDTH, YT2_INTRO_HEIGHT,
                      tick, user);
}

static int emit_vbe(const uint8_t *indices, const uint8_t *palette,
                    unsigned long tick, void *user)
{
    return emit_sized(indices, palette, YT2_INTRO_VBE_WIDTH,
                      YT2_INTRO_VBE_HEIGHT, tick, user);
}

static void poll_input(yt2_intro_input *input, void *user)
{
    input_queue *queue = &((tool_context *)user)->queue;
    unsigned long now = queue->intro->tick;

    while (queue->next < queue->count &&
           queue->events[queue->next].tick <= now) {
        const input_event *event = &queue->events[queue->next++];

        if (event->is_click) {
            input->left_pressed = 1;
            input->mouse_x = event->x;
            input->mouse_y = event->y;
        } else {
            input->have_key = 1;
            input->key = (uint8_t)event->key;
        }
    }
}

static int load_script(const char *path, input_queue *queue)
{
    FILE *stream = fopen(path, "r");
    char line[256];

    if (stream == NULL) {
        fprintf(stderr, "yt2p-intro: cannot read %s\n", path);
        return 0;
    }
    while (fgets(line, sizeof(line), stream) != NULL) {
        input_event event;
        char kind[16];
        unsigned long tick;
        unsigned int a;
        unsigned int b;

        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        if (sscanf(line, "%lu %15s %u %u", &tick, kind, &a, &b) < 3) {
            fprintf(stderr, "yt2p-intro: bad script line: %s", line);
            fclose(stream);
            return 0;
        }
        memset(&event, 0, sizeof(event));
        event.tick = tick;
        if (strcmp(kind, "key") == 0) {
            event.key = a;
        } else if (strcmp(kind, "click") == 0) {
            event.is_click = 1;
            event.x = (int)a;
            event.y = (int)b;
        } else {
            fprintf(stderr, "yt2p-intro: bad script kind: %s", line);
            fclose(stream);
            return 0;
        }
        {
            input_event *grown =
                realloc(queue->events, (queue->count + 1u) * sizeof(*grown));

            if (grown == NULL) {
                fclose(stream);
                return 0;
            }
            queue->events = grown;
            queue->events[queue->count++] = event;
        }
    }
    fclose(stream);
    return 1;
}

static const char *transition_name(yt2_intro_transition transition)
{
    switch (transition) {
    case YT2_INTRO_TRANSITION_QUIT:
        return "quit";
    case YT2_INTRO_TRANSITION_CREATE:
        return "create";
    case YT2_INTRO_TRANSITION_LOAD:
        return "load";
    case YT2_INTRO_TRANSITION_EMBARK:
        return "embark";
    case YT2_INTRO_TRANSITION_RULES:
        return "rules";
    case YT2_INTRO_TRANSITION_NONE:
    default:
        return "none";
    }
}

static const char *audio_name(yt2_intro_audio_op op)
{
    switch (op) {
    case YT2_INTRO_AUDIO_MUSIC:
        return "music";
    case YT2_INTRO_AUDIO_SFX:
        return "sfx";
    case YT2_INTRO_AUDIO_MUSIC_STOP:
        return "music-stop";
    case YT2_INTRO_AUDIO_MUSIC_SERVICE:
        return "music-service";
    default:
        return "unknown";
    }
}

int main(int argc, char **argv)
{
    const char *pictures_path = NULL;
    const char *world_path = NULL;
    const char *script_path = NULL;
    tool_context context;
    yt2_pictures pictures;
    yt2_world world;
    yt2_intro intro;
    yt2_titlemenu titlemenu;
    yt2_intro_transition transition;
    int no_cursor = 0;
    int flag_p = 0;
    int result = 1;
    int i;

    memset(&context, 0, sizeof(context));
    context.emit.cap = 8000u;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            context.emit.cap =
                (unsigned long)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            script_path = argv[++i];
        } else if (strcmp(argv[i], "--dump-frames") == 0 && i + 1 < argc) {
            context.emit.dump_dir = argv[++i];
        } else if (strcmp(argv[i], "--pxdiff") == 0) {
            context.emit.pxdiff = 1;
        } else if (strcmp(argv[i], "--no-cursor") == 0) {
            no_cursor = 1;
        } else if (strcmp(argv[i], "--flag-p") == 0) {
            flag_p = 1;
        } else if (pictures_path == NULL) {
            pictures_path = argv[i];
        } else if (world_path == NULL) {
            world_path = argv[i];
        } else {
            fprintf(stderr,
                    "usage: %s PICTURES.VGA WORLD.DAT [--frames N] "
                    "[--input SCRIPT] [--dump-frames DIR] [--no-cursor] "
                    "[--flag-p]\n",
                    argv[0]);
            return 2;
        }
    }
    if (pictures_path == NULL || world_path == NULL) {
        fprintf(stderr, "yt2p-intro: need PICTURES.VGA and WORLD.DAT\n");
        return 2;
    }
    if (script_path != NULL && !load_script(script_path, &context.queue)) {
        free(context.queue.events);
        return 1;
    }
    if (context.emit.dump_dir != NULL) {
        mkdir(context.emit.dump_dir, 0755);
    }
    if (yt2_pictures_load(pictures_path, &pictures) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-intro: cannot load %s\n", pictures_path);
        free(context.queue.events);
        return 1;
    }
    if (yt2_world_load(world_path, &world) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-intro: cannot load %s\n", world_path);
        yt2_pictures_free(&pictures);
        free(context.queue.events);
        return 1;
    }
    context.queue.intro = &intro;
    context.emit.previous =
        malloc((size_t)YT2_INTRO_VBE_WIDTH * YT2_INTRO_VBE_HEIGHT);
    if (yt2_intro_init(&intro, &pictures, &world, emit_frame, poll_input,
                       &context) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-intro: cannot init\n");
        goto done;
    }
    intro.vbe_show = emit_vbe;
    if (no_cursor) {
        intro.engine_cursor = 0;
    }
    if (flag_p) {
        intro.flag_p = 1;
    }
    if (yt2_titlemenu_init(&titlemenu, &intro, 0u) != YT2_STATUS_OK) {
        fprintf(stderr, "yt2p-intro: cannot init menu\n");
        goto done;
    }
    transition = yt2_titlemenu_boot(&titlemenu);
    fprintf(stderr, "yt2p-intro: transition %s frames %lu ticks %lu\n",
            transition_name(transition), context.emit.frames, intro.tick);
    for (i = 0; (unsigned int)i < intro.audio_count; i++) {
        fprintf(stderr, "yt2p-intro: audio %lu %s %u\n",
                intro.audio[i].tick, audio_name(intro.audio[i].op),
                intro.audio[i].id);
    }
    result = 0;
    yt2_intro_free(&intro);
done:
    yt2_world_free(&world);
    yt2_pictures_free(&pictures);
    free(context.queue.events);
    free(context.emit.previous);
    return result;
}
