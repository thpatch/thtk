/*
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain this list
 *    of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce this
 *    list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
#include <config.h>
#include <stdlib.h>
#include <thtk/thtk.h>
#include "thcrypt105.h"
#include "thdat.h"
#include "util.h"

#define th75_name_len_from_thdat(thdat) ((thdat)->version == 75 ? 100 : 260)

static void
th75_path_normalize(
    char *s,
    char find,
    char rep)
{
    for (; *s; s++)
        if (*s == find)
            *s = rep;
}

static int
th75_open(
    thdat_t* thdat,
    thtk_error_t** error)
{
    const int name_len = th75_name_len_from_thdat(thdat);
    uint16_t entry_count;

    if (thtk_io_read(thdat->stream, &entry_count, 2, error) != 2)
        return 0;

    size_t header_size = (8+name_len)*entry_count;
    uint8_t *header_buf = malloc(header_size);
    if (thtk_io_read(thdat->stream, header_buf, header_size, error) !=
            header_size)
        return 0;
    th_crypt75_list(header_buf, header_size);

    thdat->entry_count = entry_count;
    thdat->entries = calloc(entry_count, sizeof(thdat_entry_t));

    uint8_t *ptr = header_buf;
    for (uint16_t i = 0; i < entry_count; i++) {
        thdat_entry_t *entry = &thdat->entries[i];
        thdat_entry_init(entry);

        strncpy(entry->name, (char *)ptr, name_len);
        entry->name[name_len-1] = 0;
        th75_path_normalize((char *)entry->name, '\\', '/');
        ptr += name_len;
        entry->size = *((uint32_t *)ptr);
        ptr += 4;
        entry->offset = *((uint32_t *)ptr);
        ptr += 4;
    }
    free(header_buf);

    return 1;
}

static ssize_t
th75_read(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error)
{
    thdat_entry_t *entry = &thdat->entries[entry_index];
    uint8_t *data = malloc(entry->size);

    int failed = 0;
#pragma omp critical
    {
        failed = (thtk_io_seek(thdat->stream, entry->offset, SEEK_SET, error) == -1) ||
                 (thtk_io_read(thdat->stream, data, entry->size, error) != entry->size);
    }

    if (failed)
        return -1;

    if (thdat->version == 7575)
        th_crypt105_file(data, entry->size, entry->offset, THCRYPT_MEGAMARI_KEY);

    if (thtk_io_write(output, data, entry->size, error) == -1)
        return -1;

    free(data);
    return 1;
}

static int
th75_create(
    thdat_t* thdat,
    thtk_error_t** error)
{
    const int name_len = th75_name_len_from_thdat(thdat);
    thdat->offset = 2 + (8+name_len)*thdat->entry_count;
    if (thtk_io_seek(thdat->stream, thdat->offset, SEEK_SET, error) == -1)
        return 0;
    return 1;
}

static ssize_t
th75_write(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error)
{
    thdat_entry_t *entry = &thdat->entries[entry_index];

    entry->size = input_length;
    uint8_t *data = malloc(entry->size);

    if (thtk_io_seek(input, 0, SEEK_SET, error) == -1)
        return -1;
    int ret = thtk_io_read(input, data, entry->size, error);
    if (ret != entry->size)
        return -1;

    /* FIXME: race condition on thdat->offset */
    if (thdat->version == 7575)
        th_crypt105_file(data, entry->size, thdat->offset, THCRYPT_MEGAMARI_KEY);

    int failed = 0;
#pragma omp critical
    {
        failed = (thtk_io_write(thdat->stream, data, entry->size, error) != entry->size);
        if (!failed) {
            entry->offset = thdat->offset;
            thdat->offset += entry->size;
        }
    }

    free(data);

    if (failed)
        return -1;

    return entry->size;
}

static int
th75_close(
    thdat_t* thdat,
    thtk_error_t** error)
{
    const int name_len = th75_name_len_from_thdat(thdat);
    uint16_t entry_count = thdat->entry_count;

    size_t header_size = (8+name_len)*entry_count;
    uint8_t *header_buf = calloc(1, header_size);

    uint8_t *ptr = header_buf;
    for (uint16_t i = 0; i < entry_count; i++) {
        thdat_entry_t *entry = &thdat->entries[i];
        strncpy((char *)ptr, entry->name, name_len);
        ptr[name_len-1] = 0;
        th75_path_normalize((char *)ptr, '/', '\\');
        ptr += name_len;
        *(uint32_t *)ptr = entry->size;
        ptr += 4;
        *(uint32_t *)ptr = entry->offset;
        ptr += 4;
    }
    th_crypt75_list(header_buf, header_size);

    if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
        return 0;

    if (thtk_io_write(thdat->stream, &entry_count, 2, error) != 2)
        return 0;
    if (thtk_io_write(thdat->stream, header_buf, header_size, error) !=
            header_size)
        return 0;

    free(header_buf);

    return 1;
}

const thdat_module_t archive_th75 = {
    0,
    th75_open,
    th75_create,
    th75_close,
    th75_read,
    th75_write
};
