#ifndef YT2_FILE_H
#define YT2_FILE_H

#include <stddef.h>
#include <stdint.h>

#include "yt2/status.h"

/*
 * The B2 platform file service.  It is deliberately small and ordinary C:
 * callers ask for a whole file, the service owns the allocation.  The DOS
 * file structs and handle semantics stay in B1.
 */
int yt2_file_load(const char *path, uint8_t **data, size_t *size);
int yt2_file_save(const char *path, const uint8_t *data, size_t size);
void yt2_file_free(uint8_t *data);

/*
 * Read up to size bytes from the start of a file (sub_01D84 reads the
 * 25 slot-name bytes). Returns YT2_STATUS_OK with *got set, or
 * YT2_STATUS_ERROR when the file cannot be opened.
 */
int yt2_file_load_prefix(const char *path, uint8_t *data, size_t size,
                         size_t *got);

#endif /* YT2_FILE_H */
