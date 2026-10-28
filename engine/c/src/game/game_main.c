#include "yt2/game.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void yt2_dump_checkpoint(yt2_machine *machine, const char *directory)
{
    char path[1024];
    FILE *stream;
    uint32_t base = (uint32_t)YT2_LOAD_SEG << 4;
    static const uint32_t dgroup_start = 0x1DEAu * 16u;
    static const uint32_t dgroup_end = 0x2D652u;

    snprintf(path, sizeof(path), "%s/state.txt", directory);
    stream = fopen(path, "w");
    if (stream != NULL) {
        fprintf(stream, "transfers=%llu\n", (unsigned long long)machine->transfers);
        fprintf(stream, "ticks=%u\n", machine->ticks);
        fprintf(stream, "exited=%d exit_code=%d\n", machine->exited, machine->exit_code);
        fprintf(stream, "video_mode=%X vbe_mode=%d\n", machine->video_mode, machine->vbe_mode);
        fprintf(stream, "cs=%04X ip=%04X ds=%04X es=%04X ss=%04X sp=%04X\n",
                machine->r.cs, machine->r.ip, machine->r.ds, machine->r.es,
                machine->r.ss, machine->r.sp);
        fprintf(stream, "ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X bp=%04X flags=%04X\n",
                machine->r.ax, machine->r.bx, machine->r.cx, machine->r.dx,
                machine->r.si, machine->r.di, machine->r.bp, machine->r.flags);
        fclose(stream);
    }

    snprintf(path, sizeof(path), "%s/dgroup.bin", directory);
    stream = fopen(path, "wb");
    if (stream != NULL) {
        fwrite(machine->mem.bytes + base + dgroup_start, 1,
               dgroup_end - dgroup_start, stream);
        fclose(stream);
    }

    snprintf(path, sizeof(path), "%s/video.bin", directory);
    stream = fopen(path, "wb");
    if (stream != NULL) {
        if (machine->video_mode == 0x13) {
            fwrite(machine->mem.bytes + ((uint32_t)YT2_SCREEN_SEG << 4), 1, 64000u, stream);
        } else if (machine->vbe_mode) {
            fwrite(machine->mem.bytes + ((uint32_t)YT2_SCREEN_SEG << 4), 1, 0x10000u, stream);
        }
        fclose(stream);
    }

    snprintf(path, sizeof(path), "%s/palette.bin", directory);
    stream = fopen(path, "wb");
    if (stream != NULL) {
        fwrite(machine->palette, 1, 768u, stream);
        fclose(stream);
    }
}

static int yt2_dump_ppm(yt2_machine *machine, const char *path)
{
    FILE *stream;
    const uint8_t *frame = machine->present_have_last
                               ? machine->present_frame
                               : machine->mem.bytes + ((uint32_t)YT2_SCREEN_SEG << 4);
    const uint8_t *palette = machine->present_have_last
                                 ? machine->present_palette
                                 : machine->palette;
    unsigned x;
    unsigned y;

    stream = fopen(path, "wb");
    if (stream == NULL) {
        fprintf(stderr, "yt2: cannot write %s: %s\n", path, strerror(errno));
        return -1;
    }
    fprintf(stream, "P6\n320 200\n255\n");
    for (y = 0; y < 200u; y++) {
        const uint8_t *row = frame + y * 320u;
        for (x = 0; x < 320u; x++) {
            unsigned index = row[x];
            uint8_t rgb[3];
            rgb[0] = (uint8_t)((palette[index * 3u + 0u] & 0x3Fu) * 255u / 63u);
            rgb[1] = (uint8_t)((palette[index * 3u + 1u] & 0x3Fu) * 255u / 63u);
            rgb[2] = (uint8_t)((palette[index * 3u + 2u] & 0x3Fu) * 255u / 63u);
            fwrite(rgb, 1, 3, stream);
        }
    }
    fclose(stream);
    return 0;
}

static void yt2_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--game DIR] [--transfers N] [--ticks N] [--trace-api] [--dump DIR]\n"
            "          [--ppm FILE] [--present-log FILE] [--present-dir DIR]\n"
            "          [--live|--link] [--no-pace] [--trace-input]\n"
            "\n"
            "  --live     play: folder link (OUTPUT.BIN/INPUT.BIN) and real-time pacing\n"
            "  --link     folder link only, as fast as the host runs\n"
            "  --no-pace  with --live, run the link without the 30,000 cycles/ms clock\n"
            "  --transfers N  dispatch cap (default 0 = unlimited)\n",
            program);
}

int main(int argc, char **argv)
{
    const char *game = ".";
    const char *dump = NULL;
    const char *ppm = NULL;
    const char *present_log = NULL;
    const char *present_dir = NULL;
    unsigned long transfers = 0;
    unsigned long limit_ticks = 0;
    int trace = 0;
    int trace_input = 0;
    int link = 0;
    int pace = 0;
    int no_pace = 0;
    int index;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--game") == 0 && index + 1 < argc) {
            game = argv[++index];
        } else if (strcmp(argv[index], "--transfers") == 0 && index + 1 < argc) {
            transfers = strtoul(argv[++index], NULL, 0);
        } else if (strcmp(argv[index], "--ticks") == 0 && index + 1 < argc) {
            limit_ticks = strtoul(argv[++index], NULL, 0);
        } else if (strcmp(argv[index], "--dump") == 0 && index + 1 < argc) {
            dump = argv[++index];
        } else if (strcmp(argv[index], "--ppm") == 0 && index + 1 < argc) {
            ppm = argv[++index];
        } else if (strcmp(argv[index], "--present-log") == 0 && index + 1 < argc) {
            present_log = argv[++index];
        } else if (strcmp(argv[index], "--present-dir") == 0 && index + 1 < argc) {
            present_dir = argv[++index];
        } else if (strcmp(argv[index], "--live") == 0) {
            link = 1;
            pace = 1;
        } else if (strcmp(argv[index], "--link") == 0) {
            link = 1;
        } else if (strcmp(argv[index], "--no-pace") == 0) {
            no_pace = 1;
        } else if (strcmp(argv[index], "--trace-api") == 0) {
            trace = 1;
        } else if (strcmp(argv[index], "--trace-input") == 0) {
            trace_input = 1;
        } else if (strcmp(argv[index], "--help") == 0) {
            yt2_usage(argv[0]);
            return 0;
        } else {
            yt2_usage(argv[0]);
            return 2;
        }
    }

    yt2_game_init(&g_m, game);
    if (link && yt2_link_open(&g_m) != YT2_OK) {
        return 2;
    }
    g_m.pace = no_pace ? 0 : pace;
    if (present_log != NULL) {
        g_m.present_log = fopen(present_log, "w");
        if (g_m.present_log == NULL) {
            fprintf(stderr, "yt2: cannot write %s: %s\n", present_log, strerror(errno));
            return 2;
        }
    }
    if (present_dir != NULL) {
        snprintf(g_m.present_dir, sizeof(g_m.present_dir), "%s", present_dir);
    }
    g_m.trace_api = trace;
    g_m.trace_input = trace_input;
    g_m.limit_ticks = (uint32_t)limit_ticks;
    yt2_game_load(&g_m);

    (void)yt2_game_run(&g_m, transfers);

    if (dump != NULL) {
        yt2_dump_checkpoint(&g_m, dump);
    }
    if (ppm != NULL) {
        yt2_dump_ppm(&g_m, ppm);
    }
    fprintf(stderr, "yt2: transfers=%llu ticks=%u frames=%llu exited=%d code=%d\n",
            (unsigned long long)g_m.transfers, g_m.ticks, g_m.present_frames,
            g_m.exited, g_m.exit_code);
    if (g_m.present_log != NULL) {
        fclose(g_m.present_log);
        g_m.present_log = NULL;
    }
    yt2_game_shutdown(&g_m);
    return g_m.exit_code != 0 ? 1 : 0;
}
