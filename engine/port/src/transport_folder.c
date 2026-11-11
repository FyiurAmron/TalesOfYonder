#include "yt2/transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yt2/clock.h"
#include "yt2/protocol.h"
#include "yt2/status.h"

#define YT2_BIN_HEADER_SIZE 16u
#define YT2_BIN_RECORD_SIZE 8u
#define YT2_BIN_VERSION 1u
#define YT2_BIN_TIMEBASE 1193182u
#define YT2_INPUT_STAGING_SIZE 65536u

static void put_le16(uint8_t *out, unsigned int value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint32_t get_le32(const uint8_t *in)
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

static void write_header(uint8_t *out, unsigned int direction)
{
    out[0] = 'Y';
    out[1] = 'T';
    out[2] = '2';
    out[3] = 'B';
    put_le16(out + 4, YT2_BIN_VERSION);
    put_le16(out + 6, YT2_BIN_HEADER_SIZE);
    put_le32(out + 8, YT2_BIN_TIMEBASE);
    out[12] = (uint8_t)direction;
    out[13] = 0;
    out[14] = 0;
    out[15] = 0;
}

static int header_is_valid(const uint8_t *header, unsigned int direction)
{
    return header[0] == 'Y' && header[1] == 'T' && header[2] == '2' &&
           header[3] == 'B' && header[4] == YT2_BIN_VERSION && header[5] == 0 &&
           header[6] == YT2_BIN_HEADER_SIZE && header[7] == 0 &&
           get_le32(header + 8) == YT2_BIN_TIMEBASE &&
           header[12] == (uint8_t)direction && header[13] == 0 &&
           header[14] == 0 && header[15] == 0;
}

static long file_size(FILE *file)
{
    long size;

    if (fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }
    size = ftell(file);
    return size;
}

int yt2_folder_out_open(yt2_folder_out *out, const char *dir)
{
    uint8_t header[YT2_BIN_HEADER_SIZE];
    int written;

    if (out == NULL || dir == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(out, 0, sizeof(*out));
    written = snprintf(out->path, sizeof(out->path), "%s/OUTPUT.BIN", dir);
    if (written < 0 || (size_t)written >= sizeof(out->path)) {
        return YT2_STATUS_ERROR;
    }
    out->file = fopen(out->path, "wb");
    if (out->file == NULL) {
        return YT2_STATUS_ERROR;
    }
    write_header(header, YT2_TRANSPORT_DIRECTION_OUT);
    if (fwrite(header, 1, sizeof(header), out->file) != sizeof(header) ||
        fflush(out->file) != 0) {
        fclose(out->file);
        out->file = NULL;
        return YT2_STATUS_ERROR;
    }
    out->written = YT2_BIN_HEADER_SIZE;
    return YT2_STATUS_OK;
}

int yt2_folder_out_write(yt2_folder_out *out, const uint8_t *frame,
                         uint32_t length)
{
    uint8_t record[YT2_BIN_RECORD_SIZE];

    if (out == NULL || out->file == NULL || frame == NULL ||
        length < YT2_FRAME_HEADER_SIZE || length > YT2_PROTOCOL_MAX_FRAME) {
        return YT2_STATUS_ERROR;
    }
    if (out->written + YT2_BIN_RECORD_SIZE + length >
        YT2_TRANSPORT_VIDEO_LIMIT) {
        uint8_t header[YT2_BIN_HEADER_SIZE];

        fclose(out->file);
        out->file = fopen(out->path, "wb");
        if (out->file == NULL) {
            return YT2_STATUS_ERROR;
        }
        write_header(header, YT2_TRANSPORT_DIRECTION_OUT);
        if (fwrite(header, 1, sizeof(header), out->file) != sizeof(header)) {
            return YT2_STATUS_ERROR;
        }
        out->written = YT2_BIN_HEADER_SIZE;
        out->wrapped = 1;
    }
    put_le32(record, yt2_pit_now());
    put_le32(record + 4, length);
    if (fwrite(record, 1, sizeof(record), out->file) != sizeof(record) ||
        fwrite(frame, 1, length, out->file) != length) {
        return YT2_STATUS_ERROR;
    }
    out->written += YT2_BIN_RECORD_SIZE + length;
    return YT2_STATUS_OK;
}

int yt2_folder_out_commit(yt2_folder_out *out)
{
    if (out == NULL || out->file == NULL) {
        return YT2_STATUS_ERROR;
    }
    return fflush(out->file) == 0 ? YT2_STATUS_OK : YT2_STATUS_ERROR;
}

int yt2_folder_out_take_wrapped(yt2_folder_out *out)
{
    int wrapped;

    if (out == NULL) {
        return 0;
    }
    wrapped = out->wrapped;
    out->wrapped = 0;
    return wrapped;
}

void yt2_folder_out_close(yt2_folder_out *out)
{
    if (out == NULL) {
        return;
    }
    if (out->file != NULL) {
        fclose(out->file);
    }
    memset(out, 0, sizeof(*out));
}

int yt2_folder_in_open(yt2_folder_in *in, const char *dir)
{
    int written;

    if (in == NULL || dir == NULL) {
        return YT2_STATUS_ERROR;
    }
    memset(in, 0, sizeof(*in));
    written = snprintf(in->path, sizeof(in->path), "%s/INPUT.BIN", dir);
    if (written < 0 || (size_t)written >= sizeof(in->path)) {
        return YT2_STATUS_ERROR;
    }
    in->staging = malloc(YT2_INPUT_STAGING_SIZE);
    if (in->staging == NULL) {
        return YT2_STATUS_ERROR;
    }
    in->capacity = YT2_INPUT_STAGING_SIZE;
    return YT2_STATUS_OK;
}

static int folder_in_detect(yt2_folder_in *in)
{
    uint8_t header[YT2_BIN_HEADER_SIZE];
    long size;

    in->position = 0;
    in->staged = 0;
    in->served = 0;
    size = file_size(in->file);
    if (size < 0 || size < (long)YT2_BIN_HEADER_SIZE) {
        return YT2_STATUS_OK;
    }
    if (fseek(in->file, 0, SEEK_SET) != 0 ||
        fread(header, 1, sizeof(header), in->file) != sizeof(header)) {
        return YT2_STATUS_ERROR;
    }
    if (!header_is_valid(header, YT2_TRANSPORT_DIRECTION_IN)) {
        return YT2_STATUS_OK;
    }
    /* Consume the header and read what follows, including a handshake the
       client wrote before we attached (hello answers once, keyframes are
       idempotent, game input is never retried, so nothing doubles). */
    in->position = YT2_BIN_HEADER_SIZE;
    return YT2_STATUS_OK;
}

int yt2_folder_in_poll(yt2_folder_in *in, const uint8_t **frame,
                       uint32_t *length)
{
    long size;
    long available;
    size_t want;
    size_t got;

    if (in == NULL || frame == NULL || length == NULL) {
        return -1;
    }
    *frame = NULL;
    *length = 0;
    if (!in->opened) {
        in->file = fopen(in->path, "rb");
        if (in->file == NULL) {
            return 0;
        }
        in->opened = 1;
        if (folder_in_detect(in) != YT2_STATUS_OK) {
            return -1;
        }
    }
    /* Drop the record handed out last call; the pointer stays valid until
       this one, so the bytes must not move underneath the caller. */
    if (in->served > 0u) {
        if (in->served > in->staged) {
            return -1;
        }
        in->staged -= in->served;
        memmove(in->staging, in->staging + in->served, in->staged);
        in->served = 0u;
    }
    size = file_size(in->file);
    if (size < 0) {
        return -1;
    }
    if (size < in->position) {
        /* The writer truncated under us: new session, answer hello again. */
        in->wrapped = 1;
        if (folder_in_detect(in) != YT2_STATUS_OK) {
            return -1;
        }
        return 0;
    }
    /* Serve a whole staged record before reading more. */
    while (in->staged >= YT2_BIN_RECORD_SIZE) {
        uint32_t payload =
            ((uint32_t)in->staging[4] | ((uint32_t)in->staging[5] << 8) |
             ((uint32_t)in->staging[6] << 16) |
             ((uint32_t)in->staging[7] << 24));
        uint32_t total;

        if (payload < YT2_FRAME_HEADER_SIZE ||
            payload > YT2_PROTOCOL_MAX_FRAME) {
            /* Corrupt record: reset the session, the client retries. */
            in->wrapped = 1;
            if (folder_in_detect(in) != YT2_STATUS_OK) {
                return -1;
            }
            return 0;
        }
        total = YT2_BIN_RECORD_SIZE + payload;
        if (in->staged < total) {
            break;
        }
        *frame = in->staging + YT2_BIN_RECORD_SIZE;
        *length = payload;
        in->served = total;
        return 1;
    }
    available = size - in->position;
    if (available <= 0) {
        return 0;
    }
    if (fseek(in->file, in->position, SEEK_SET) != 0) {
        return -1;
    }
    want = (size_t)available;
    if (want > in->capacity - in->staged) {
        want = in->capacity - in->staged;
    }
    if (want == 0) {
        return 0;
    }
    got = fread(in->staging + in->staged, 1, want, in->file);
    in->staged += got;
    in->position += (long)got;
    return 0;
}

int yt2_folder_in_take_wrapped(yt2_folder_in *in)
{
    int wrapped;

    if (in == NULL) {
        return 0;
    }
    wrapped = in->wrapped;
    in->wrapped = 0;
    return wrapped;
}

void yt2_folder_in_close(yt2_folder_in *in)
{
    if (in == NULL) {
        return;
    }
    if (in->file != NULL) {
        fclose(in->file);
    }
    free(in->staging);
    memset(in, 0, sizeof(*in));
}
