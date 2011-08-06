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
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <thtk/thtk.h>
#include "thdat.h"
#include "thrle.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t magic;
    /* Appears unused. */
    uint8_t key;
    unsigned char name[13];
    uint32_t zsize;
    uint32_t size;
    uint32_t offset;
    uint32_t zero;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th02_entry_header_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t size;
    uint16_t unknown1;
    uint16_t count;
    uint8_t key;
    uint8_t zero2[9];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th03_archive_header_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t magic;
    uint8_t key;
    unsigned char name[13];
    uint16_t zsize;
    uint16_t size;
    uint32_t offset;
    uint32_t zero[2];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th03_entry_header_t;

/* TODO: These constants should be calculated instead. */
static const uint8_t archive_key = 0x12;
static const uint8_t entry_key = 0x34;

/* Used for uncompressed entries. */
static const uint16_t magic1 = 0xf388;
/* Used for compressed entries. */
static const uint16_t magic2 = 0x9595;

static int
th02_open(
    thdat_t* thdat,
    thtk_error_t** error)
{
    th03_archive_header_t th03_archive_header;
    th02_entry_header_t* th02_entry_headers = NULL;
    th03_entry_header_t* th03_entry_headers = NULL;

    if (thdat->version == 2) {
        th02_entry_header_t eh2;
        if (thtk_io_read(thdat->stream, &eh2, sizeof(eh2), error) != sizeof(eh2))
            return 0;

        if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
            return 0;

        if (eh2.offset % sizeof(eh2)) {
            thtk_error_new(error, "first entry offset invalid");
            return 0;
        }

        thdat->entry_count = (eh2.offset / sizeof(eh2)) - 1;
    } else {
        if (thtk_io_read(thdat->stream, &th03_archive_header, sizeof(th03_archive_header), error) != sizeof(th03_archive_header))
            return 0;
        thdat->entry_count = th03_archive_header.count;
    }

    thdat->entries = malloc(thdat->entry_count * sizeof(thdat_entry_t));

    if (thdat->version == 2) {
        th02_entry_headers = malloc(thdat->entry_count * sizeof(th02_entry_header_t));
        if (thtk_io_read(thdat->stream, th02_entry_headers, thdat->entry_count * sizeof(th02_entry_header_t), error) !=
            (ssize_t)(thdat->entry_count * sizeof(th02_entry_header_t)))
            return 0;
    } else {
        th03_entry_headers = malloc(thdat->entry_count * sizeof(th03_entry_header_t));
        if (thtk_io_read(thdat->stream, th03_entry_headers, thdat->entry_count * sizeof(th03_entry_header_t), error) !=
            (ssize_t)(thdat->entry_count * sizeof(th03_entry_header_t)))
            return 0;

        unsigned char* data = (unsigned char*)th03_entry_headers;
        for (size_t i = 0; i < thdat->entry_count * sizeof(th03_entry_header_t); ++i) {
            data[i] ^= th03_archive_header.key;
            th03_archive_header.key -= data[i];
        }
    }

    for (unsigned int e = 0; e < thdat->entry_count; ++e) {
        thdat_entry_t* entry = &thdat->entries[e];

        entry->extra = thdat->version == 2
            ? 0x12 /* th02_entry_headers[e].key */
            : th03_entry_headers[e].key;

        if (thdat->version == 2) {
            for (unsigned int i = 0; i < 13 && th02_entry_headers[e].name[i]; ++i)
                th02_entry_headers[e].name[i] ^= 0xff;
        }
        memcpy(entry->name, thdat->version == 2
            ? th02_entry_headers[e].name
            : th03_entry_headers[e].name, 13);
        entry->zsize = thdat->version == 2
            ? th02_entry_headers[e].zsize
            : th03_entry_headers[e].zsize;
        entry->size = thdat->version == 2
            ? th02_entry_headers[e].size
            : th03_entry_headers[e].size;
        entry->offset = thdat->version == 2
            ? th02_entry_headers[e].offset
            : th03_entry_headers[e].offset;
    }

    free(th02_entry_headers);
    free(th03_entry_headers);

    return 1;
}

static ssize_t
th02_read(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    unsigned char* data;
    ssize_t ret;
#pragma omp critical
    {
        data = thtk_io_map(thdat->stream, entry->offset, entry->zsize, error);
    }
    if (!data)
        return -1;

    for (ssize_t i = 0; i < entry->zsize; ++i)
        data[i] ^= entry->extra;

    if (entry->size == entry->zsize) {
        ret = thtk_io_write(output, data, entry->zsize, error);
        thtk_io_unmap(thdat->stream, data);
    } else {
        thtk_io_t* data_stream = thtk_io_open_memory(data, entry->zsize, error);
        if (!data_stream)
            return -1;
        ret = thtk_unrle(data_stream, entry->zsize, output, error);
        thtk_io_close(data_stream);
    }

    return ret;
}

static int
th02_create(
    thdat_t* thdat,
    thtk_error_t** error)
{
    if (thdat->version == 2)
        thdat->offset = (thdat->entry_count + 1) * sizeof(th02_entry_header_t);
    else
        thdat->offset = sizeof(th03_archive_header_t) + (thdat->entry_count + 1) * sizeof(th03_entry_header_t);
    if (thtk_io_seek(thdat->stream, thdat->offset, SEEK_SET, error) == -1)
        return 0;
    return 1;
}

/* TODO: Find out if lowercase filenames are supported. */
static ssize_t
th02_write(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    entry->size = input_length;

    off_t input_offset = thtk_io_seek(input, 0, SEEK_CUR, error);
    if (input_offset == -1)
        return -1;

    if (thdat->version == 2) {
        for (unsigned int i = 0; i < 13; ++i)
            if (entry->name[i])
                entry->name[i] ^= 0xff;
    }

    thtk_io_t* output = thtk_io_open_growing_memory(error);
    if (!output)
        return -1;

    if ((entry->zsize = thtk_rle(input, entry->size, output, error)) == -1)
        return -1;

    if (entry->zsize >= entry->size) {
        entry->zsize = entry->size;
        thtk_io_close(output);
        if (thtk_io_seek(input, input_offset, SEEK_SET, error) == -1)
            return -1;
        output = input;
    }

    unsigned char* data = thtk_io_map(output, 0, entry->zsize, error);
    if (!data)
        return -1;

    for (ssize_t i = 0; i < entry->zsize; ++i)
        data[i] ^= thdat->version == 2 ? 0x12 : entry_key;

    ssize_t ret = -1;

#pragma omp critical
    {
        entry->offset = thtk_io_seek(thdat->stream, 0, SEEK_CUR, error);

        if (entry->offset != -1)
            ret = thtk_io_write(thdat->stream, data, entry->zsize, error);

        if (ret != -1)
            thdat->offset += ret;
    }

    thtk_io_unmap(output, data);

    if (output != input)
        thtk_io_close(output);

    return ret;
}

static int
th02_close(
    thdat_t* thdat,
    thtk_error_t** error)
{
    if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
        return 0;

    const th03_archive_header_t ah3 = {
        .size = (thdat->entry_count + 1) * sizeof(th03_entry_header_t),
        .unknown1 = 2,
        .count = thdat->entry_count,
        .key = archive_key
    };

    if (thdat->version > 2) {
        if (thtk_io_write(thdat->stream, &ah3, sizeof(ah3), error) == -1)
            return 0;
    }

    size_t buffer_size = (thdat->entry_count + 1) * (thdat->version == 2 ? sizeof(th02_entry_header_t) : sizeof(th03_entry_header_t));
    unsigned char* buffer = malloc(buffer_size);
    unsigned char* buffer_ptr = buffer;

    memset(buffer, 0, buffer_size);

    for (size_t i = 0; i < thdat->entry_count; ++i) {
        thdat_entry_t* entry = &thdat->entries[i];
        if (thdat->version == 2) {
            th02_entry_header_t eh2 = {
                .magic = entry->zsize == entry->size ? magic1 : magic2,
                .key = 3,
                .zsize = entry->zsize,
                .size = entry->size,
                .offset = entry->offset
            };

            memcpy(eh2.name, entry->name, 13);

            buffer_ptr = mempcpy(buffer_ptr, &eh2, sizeof(eh2));
        } else {
            th03_entry_header_t eh3 = {
                .magic = entry->zsize == entry->size ? magic1 : magic2,
                .key = entry_key,
                .zsize = entry->zsize,
                .size = entry->size,
                .offset = entry->offset
            };

            memcpy(eh3.name, entry->name, 13);

            buffer_ptr = mempcpy(buffer_ptr, &eh3, sizeof(eh3));
        }
    }

    if (thdat->version > 2) {
        uint32_t data_key = archive_key;
        for (uint16_t i = 0; i < buffer_size; ++i) {
            unsigned char tmp = buffer[i];
            buffer[i] ^= data_key;
            data_key -= tmp;
        }
    }

    if (thtk_io_write(thdat->stream, buffer, buffer_size, error) == -1) {
        free(buffer);
        return 0;
    }

    free(buffer);

    return 1;
}

const thdat_module_t archive_th02 = {
    THDAT_BASENAME | THDAT_UPPERCASE | THDAT_8_3,
    th02_open,
    th02_create,
    th02_close,
    th02_read,
    th02_write
};
