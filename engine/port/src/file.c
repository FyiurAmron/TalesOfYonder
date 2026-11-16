#include "yt2/file.h"

#include <stdio.h>
#include <stdlib.h>

int yt2_file_load(const char *path, uint8_t **data, size_t *size)
{
    FILE *stream;
    long length;
    uint8_t *buffer;

    if (path == NULL || data == NULL || size == NULL) {
        return YT2_STATUS_ERROR;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (fseek(stream, 0, SEEK_END) != 0 ||
        (length = ftell(stream)) < 0 ||
        fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return YT2_STATUS_ERROR;
    }
    buffer = malloc((size_t)length);
    if (buffer == NULL) {
        fclose(stream);
        return YT2_STATUS_ERROR;
    }
    if (fread(buffer, 1, (size_t)length, stream) != (size_t)length) {
        free(buffer);
        fclose(stream);
        return YT2_STATUS_ERROR;
    }
    fclose(stream);
    *data = buffer;
    *size = (size_t)length;
    return YT2_STATUS_OK;
}

int yt2_file_save(const char *path, const uint8_t *data, size_t size)
{
    FILE *stream;
    int status = YT2_STATUS_ERROR;

    if (path == NULL || data == NULL) {
        return YT2_STATUS_ERROR;
    }
    stream = fopen(path, "wb");
    if (stream == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (fwrite(data, 1, size, stream) != size) {
        fclose(stream);
        return YT2_STATUS_ERROR;
    }
    if (fclose(stream) == 0) {
        status = YT2_STATUS_OK;
    }
    return status;
}

void yt2_file_free(uint8_t *data)
{
    free(data);
}

int yt2_file_load_prefix(const char *path, uint8_t *data, size_t size,
                         size_t *got)
{
    FILE *stream;
    size_t count = 0u;

    if (path == NULL || data == NULL || got == NULL) {
        return YT2_STATUS_ERROR;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        return YT2_STATUS_ERROR;
    }
    if (size != 0u) {
        count = fread(data, 1, size, stream);
    }
    fclose(stream);
    *got = count;
    return YT2_STATUS_OK;
}
