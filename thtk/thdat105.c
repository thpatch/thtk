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

typedef struct {
    uint16_t entry_count;
    uint32_t size;
} th105_archive_header_t;

static int
th105_open(
    thdat_t* thdat,
    thtk_error_t** error)
{
    th105_archive_header_t header;

    uint16_t entry_count;
    uint32_t header_size;

    if (thtk_io_read(thdat->stream, &entry_count, 2, error) == -1)
        return 0;
    if (thtk_io_read(thdat->stream, &header_size, 4, error) == -1)
        return 0;

    header.entry_count = entry_count;
    header.size = header_size;

    unsigned char* header_buf = malloc(header_size);
    if (thtk_io_read(thdat->stream, header_buf, header_size, error) !=
            header_size)
        return 0;
    th_crypt105_list(header_buf, header_size, 0xc5, 0x83, 0x53);

    thdat->entry_count = entry_count;
    thdat->entries = calloc(entry_count, sizeof(thdat_entry_t));

    if (header.entry_count) {
        unsigned char* ptr = header_buf;
        for (uint16_t i = 0; i < entry_count; ++i) {
            thdat_entry_t* entry = thdat->entries + i;
            thdat_entry_init(entry);

            entry->offset = *((uint32_t*)ptr);
            ptr += 4;
            entry->size = *((uint32_t*)ptr);
            ptr += 4;
            // zsize and extra are not used.

            unsigned char name_length = *(ptr++);
            strncpy(entry->name, (char*)ptr, name_length);
            ptr += name_length;
        }
    }
    free(header_buf);

    return 1;
}

static void
th105_decrypt_data(
    thdat_t* archive,
    thdat_entry_t* entry,
    unsigned char* data)
{
    (void)archive;
    th_crypt105_file(data, entry->size, entry->offset);
}

static ssize_t
th105_read(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error)
{
    thdat_entry_t* entry = thdat->entries + entry_index;
    unsigned char* data = malloc(entry->size);

    int failed = 0;
#pragma omp critical
    {
        failed = (thtk_io_seek(thdat->stream, entry->offset, SEEK_SET, error) == -1) ||
                 (thtk_io_read(thdat->stream, data, entry->size, error) != entry->size);
    }

    if (failed)
        return -1;

    th105_decrypt_data(thdat, entry, data);

    if (thtk_io_write(output, data, entry->size, error) == -1)
        return -1;

    free(data);
    return 1;
}

static int
th105_create(
    thdat_t* thdat,
    thtk_error_t** error)
{
    // entry list is given
    off_t size = 6;
    unsigned int i;
    for (i = 0; i < thdat->entry_count; ++i) {
        const thdat_entry_t* entry = thdat->entries + i;
        const size_t namelen = strlen(entry->name);
        size += 8; // for offset and size
        size += (1 + namelen); // for name
    }
    thdat->offset = size;
    if (thtk_io_seek(thdat->stream, thdat->offset, SEEK_SET, error) == -1)
        return 0;
    return 1;
}

static void
th105_encrypt_data(
    thdat_t* archive,
    thdat_entry_t* entry,
    unsigned char* data)
{
    th_crypt105_file(data, entry->size, archive->offset);
}

static ssize_t
th105_write(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error)
{
    thdat_entry_t* entry = thdat->entries + entry_index;
    unsigned char* data;

    entry->size = input_length;
    data = malloc(entry->size);
    if (thtk_io_seek(input, 0, SEEK_SET, error) == -1)
        return -1;
    int ret = thtk_io_read(input, data, entry->size, error);
    if (ret != entry->size)
        return -1;

    th105_encrypt_data(thdat, entry, data);

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
th105_close(
    thdat_t* thdat,
    thtk_error_t** error)
{
    unsigned char* buffer;
    uint16_t entry_count = thdat->entry_count;
    uint32_t header_size = 0;

    for (unsigned i = 0; i < entry_count; ++i) {
        const size_t namelen = strlen(thdat->entries[i].name);
        header_size += 9 + namelen;
    }

    if (header_size == 0) {
        thtk_error_new(error, "no entries");
        return 0;
    }

    buffer = malloc(header_size);

    unsigned char* buffer_ptr = buffer;
    for (unsigned i = 0; i < entry_count; i++) {
        uint32_t* buffer_ptr_32 = (uint32_t*) buffer_ptr;
        const thdat_entry_t* entry = thdat->entries + i;
        const uint8_t namelen = strlen(entry->name);
        *(buffer_ptr_32++) = entry->offset;
        *(buffer_ptr_32++) = entry->size;
        buffer_ptr = (unsigned char*) buffer_ptr_32;
        *(buffer_ptr++) = namelen;
        buffer_ptr = MEMPCPY(buffer_ptr, entry->name, namelen);
    }

    th_crypt105_list(buffer, header_size, 0xc5, 0x83, 0x53);

    if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
        return 0;

    if (thtk_io_write(thdat->stream, &entry_count, 2, error) == -1)
        return 0;
    if (thtk_io_write(thdat->stream, &header_size, 4, error) == -1)
        return 0;
    if (thtk_io_write(thdat->stream, buffer, header_size, error) == -1)
        return 0;

    free(buffer);

    return 1;
}

const thdat_module_t archive_th105 = {
    0,
    th105_open,
    th105_create,
    th105_close,
    th105_read,
    th105_write
};
