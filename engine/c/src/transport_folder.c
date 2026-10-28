#include "yt2/transport.h"

#include "yt2/platform.h"

#include <string.h>

void yt2_transport_bin_header(uint8_t *destination, uint8_t direction)
{
    destination[0] = 'Y';
    destination[1] = 'T';
    destination[2] = '2';
    destination[3] = 'B';
    yt2_put_le16(destination + 4, 1);
    yt2_put_le16(destination + 6, 16);
    yt2_put_le32(destination + 8, YT2_PIT_RATE);
    destination[12] = direction;
    destination[13] = 0;
    destination[14] = 0;
    destination[15] = 0;
}

static int yt2_folder_header_is_valid(const uint8_t *header, uint8_t direction)
{
    return header[0] == 'Y' && header[1] == 'T' &&
           header[2] == '2' && header[3] == 'B' &&
           yt2_le16(header + 4) == 1u &&
           yt2_le16(header + 6) == 16u &&
           yt2_le32(header + 8) == YT2_PIT_RATE &&
           header[12] == direction &&
           header[13] == 0u && header[14] == 0u && header[15] == 0u;
}

static long yt2_folder_size(FILE *file)
{
    long size;

    if (fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }
    size = ftell(file);
    if (size < 0) {
        return -1;
    }
    return size;
}

static int yt2_folder_start_output(yt2_folder_transport *folder)
{
    uint8_t header[16];

    if (folder->output != NULL) {
        fclose(folder->output);
        folder->output = NULL;
    }
    folder->output = fopen(folder->output_path, "wb");
    if (folder->output == NULL) {
        return YT2_ERR;
    }
    yt2_transport_bin_header(header, 0);
    if (fwrite(header, 1, sizeof(header), folder->output) != sizeof(header)) {
        fclose(folder->output);
        folder->output = NULL;
        return YT2_ERR;
    }
    if (fflush(folder->output) != 0) {
        fclose(folder->output);
        folder->output = NULL;
        return YT2_ERR;
    }
    folder->output_written = 16;
    return YT2_OK;
}

static void yt2_folder_reset_input(yt2_folder_transport *folder)
{
    folder->input_length = 0;
    folder->input_offset = 0;
    folder->pending_length = 0;
    folder->pending_offset = 0;
}

static void yt2_folder_detect_input(yt2_folder_transport *folder)
{
    uint8_t header[16];
    long size = yt2_folder_size(folder->input);

    folder->input_mode = 0;
    folder->input_base = 0;
    folder->input_position = 0;
    folder->input_last_size = size;
    yt2_folder_reset_input(folder);
    if (size < 16) {
        folder->input_position = 0;
        return;
    }
    if (fseek(folder->input, 0, SEEK_SET) != 0 ||
        fread(header, 1, sizeof(header), folder->input) != sizeof(header)) {
        folder->input_position = 0;
        return;
    }
    if (yt2_folder_header_is_valid(header, 1)) {
        folder->input_mode = 1;
        folder->input_base = 16;
    } else {
        folder->input_mode = 2;
        folder->input_base = 0;
    }
    if (folder->input_from_start || folder->input_fresh) {
        folder->input_position = folder->input_base;
    } else {
        folder->input_position = size;
    }
}

static int yt2_folder_try_input(yt2_folder_transport *folder)
{
    long size;

    if (folder->input_opened) {
        return YT2_OK;
    }
    folder->input = fopen(folder->input_path, "rb");
    if (folder->input == NULL) {
        return YT2_OK;
    }
    folder->input_opened = 1;
    folder->input_fresh = 0;
    size = yt2_folder_size(folder->input);
    if (size < 16) {
        folder->input_fresh = 1;
    }
    yt2_folder_detect_input(folder);
    return YT2_OK;
}

static int yt2_folder_write(void *context, const uint8_t *frame, uint32_t length)
{
    yt2_folder_transport *folder = (yt2_folder_transport *)context;
    uint8_t record[8];
    uint32_t stamp;

    if (folder->output == NULL || length < YT2_FRAME_HEADER_SIZE ||
        length > YT2_MAX_FRAME) {
        return YT2_ERR;
    }
    if (folder->output_written + 8u + length > YT2_FOLDER_VIDEO_LIMIT) {
        if (yt2_folder_start_output(folder) != YT2_OK) {
            return YT2_ERR;
        }
        folder->output_wrapped = 1;
    }
    stamp = yt2_platform_pit_now();
    yt2_put_le32(record, stamp);
    yt2_put_le32(record + 4, length);
    if (fwrite(record, 1, sizeof(record), folder->output) != sizeof(record) ||
        fwrite(frame, 1, (size_t)length, folder->output) != (size_t)length) {
        return YT2_ERR;
    }
    /* A present writes a batch of records; the flush (a frame_end boundary in
       every live producer) publishes it, so a frame costs one commit. */
    folder->output_written += (uint32_t)sizeof(record) + length;
    folder->output_frames++;
    return YT2_OK;
}

static int yt2_folder_read_raw(yt2_folder_transport *folder, uint8_t *destination,
                               uint32_t capacity, uint32_t *read)
{
    long size = yt2_folder_size(folder->input);
    long available;
    uint32_t want;
    size_t got;

    if (size < 0) {
        return YT2_ERR;
    }
    if (size < folder->input_position) {
        yt2_folder_reset_input(folder);
        folder->input_mode = 0;
        folder->input_fresh = 1;
        yt2_folder_detect_input(folder);
        return YT2_OK;
    }
    available = size - folder->input_position;
    if (available <= 0) {
        return YT2_OK;
    }
    want = (available > (long)capacity) ? capacity : (uint32_t)available;
    if (fseek(folder->input, folder->input_position, SEEK_SET) != 0) {
        return YT2_ERR;
    }
    got = fread(destination, 1, (size_t)want, folder->input);
    if (got == 0u) {
        return YT2_ERR;
    }
    folder->input_position += (long)got;
    *read = (uint32_t)got;
    return YT2_OK;
}

static int yt2_folder_read_timed(yt2_folder_transport *folder, uint8_t *destination,
                                 uint32_t capacity, uint32_t *read)
{
    uint32_t available;

    if (folder->pending_offset < folder->pending_length) {
        available = folder->pending_length - folder->pending_offset;
        if (available > capacity) {
            available = capacity;
        }
        for (uint32_t index = 0; index < available; index++) {
            destination[index] = folder->pending[folder->pending_offset + index];
        }
        folder->pending_offset += available;
        *read = available;
        return YT2_OK;
    }
    folder->pending_length = 0;
    folder->pending_offset = 0;

    for (;;) {
        long size;
        long available_bytes;
        uint32_t frame_length;

        if (folder->input_offset > 0u) {
            uint32_t left = folder->input_length - folder->input_offset;

            if (left > 0u) {
                memmove(folder->input_buffer,
                        folder->input_buffer + folder->input_offset, left);
            }
            folder->input_length = left;
            folder->input_offset = 0;
        }

        size = yt2_folder_size(folder->input);
        if (size < 0) {
            return YT2_ERR;
        }
        if (size < folder->input_position) {
            yt2_folder_reset_input(folder);
            folder->input_mode = 0;
            folder->input_fresh = 1;
            yt2_folder_detect_input(folder);
            return YT2_OK;
        }
        available_bytes = size - folder->input_position;
        if (available_bytes > 0) {
            uint32_t room = (uint32_t)sizeof(folder->input_buffer) -
                            folder->input_length;
            uint32_t want = (available_bytes > (long)room) ? room
                                                           : (uint32_t)available_bytes;
            size_t got;

            if (want == 0u) {
                return YT2_ERR;
            }
            if (fseek(folder->input, folder->input_position, SEEK_SET) != 0) {
                return YT2_ERR;
            }
            got = fread(folder->input_buffer + folder->input_length, 1,
                        (size_t)want, folder->input);
            if (got == 0u) {
                return YT2_ERR;
            }
            folder->input_length += (uint32_t)got;
            folder->input_position += (long)got;
        }

        if (folder->input_length - folder->input_offset < 8u) {
            return YT2_OK;
        }
        frame_length = yt2_le32(folder->input_buffer + folder->input_offset + 4);
        if (frame_length < YT2_FRAME_HEADER_SIZE ||
            frame_length > YT2_MAX_FRAME) {
            return YT2_ERR;
        }
        if (folder->input_length - folder->input_offset < 8u + frame_length) {
            return YT2_OK;
        }
        for (uint32_t index = 0; index < frame_length; index++) {
            folder->pending[index] =
                folder->input_buffer[folder->input_offset + 8u + index];
        }
        folder->pending_length = frame_length;
        folder->pending_offset = 0;
        folder->input_offset += 8u + frame_length;
        folder->input_frames++;
        break;
    }

    available = folder->pending_length;
    if (available > capacity) {
        available = capacity;
    }
    for (uint32_t index = 0; index < available; index++) {
        destination[index] = folder->pending[index];
    }
    folder->pending_offset = available;
    *read = available;
    return YT2_OK;
}

static int yt2_folder_read(void *context, uint8_t *destination, uint32_t capacity,
                           uint32_t *read)
{
    yt2_folder_transport *folder = (yt2_folder_transport *)context;
    long size;

    *read = 0;
    if (yt2_folder_try_input(folder) != YT2_OK) {
        return YT2_ERR;
    }
    if (folder->input == NULL) {
        return YT2_OK;
    }
    size = yt2_folder_size(folder->input);
    if (size < 0) {
        return YT2_ERR;
    }
    if (folder->input_mode == 0) {
        if (size < 16) {
            return YT2_OK;
        }
        folder->input_fresh = 1;
        yt2_folder_detect_input(folder);
    }
    folder->input_last_size = size;
    if (folder->input_mode == 1) {
        return yt2_folder_read_timed(folder, destination, capacity, read);
    }
    return yt2_folder_read_raw(folder, destination, capacity, read);
}

static int yt2_folder_flush(void *context)
{
    yt2_folder_transport *folder = (yt2_folder_transport *)context;

    if (folder->output == NULL) {
        return YT2_ERR;
    }
    return fflush(folder->output) == 0 ? YT2_OK : YT2_ERR;
}

static int yt2_folder_take_wrap(void *context)
{
    yt2_folder_transport *folder = (yt2_folder_transport *)context;
    int wrapped = folder->output_wrapped != 0u;

    folder->output_wrapped = 0;
    return wrapped;
}

static void yt2_folder_close(void *context)
{
    yt2_folder_transport *folder = (yt2_folder_transport *)context;

    if (folder->output != NULL) {
        fclose(folder->output);
        folder->output = NULL;
    }
    if (folder->input != NULL) {
        fclose(folder->input);
        folder->input = NULL;
    }
}

int yt2_transport_folder_init(yt2_transport *transport,
                              yt2_folder_transport *context,
                              const char *output_path, const char *input_path,
                              int input_from_start)
{
    memset(context, 0, sizeof(*context));
    context->output_path = output_path;
    context->input_path = input_path;
    context->input_from_start = input_from_start;
    if (yt2_folder_start_output(context) != YT2_OK) {
        return YT2_ERR;
    }
    transport->context = context;
    transport->write_frame = yt2_folder_write;
    transport->read = yt2_folder_read;
    transport->flush = yt2_folder_flush;
    transport->take_wrap = yt2_folder_take_wrap;
    transport->close = yt2_folder_close;
    return YT2_OK;
}
