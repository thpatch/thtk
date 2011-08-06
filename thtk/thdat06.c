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
#include <string.h>
#include <thtk/thtk.h>
#include "bits.h"
#include "thdat.h"
#include "thlzss.h"

static uint32_t
th06_read_uint32(
    struct bitstream* b)
{
    uint32_t size = bitstream_read(b, 2);
    return bitstream_read(b, (size + 1) * 8);
}

static void
th06_write_uint32(
    struct bitstream* b,
    uint32_t value)
{
    unsigned int size = 1;
    if (value & 0xffffff00) {
        size = 2;
        if (value & 0xffff0000) {
            size = 3;
            if (value & 0xff000000)
                size = 4;
        }
    }

    bitstream_write(b, 2, size - 1);
    bitstream_write(b, size * 8, value);
}

static void
th06_read_string(
    struct bitstream* b,
    unsigned int length,
    char* data)
{
    while (length) {
        *data = bitstream_read(b, 8);
        if (!*data)
            break;
        data++;
        length--;
    }
}

static void
th06_write_string(
    struct bitstream* b,
    unsigned int length,
    char* data)
{
    unsigned int i;
    for (i = 0; i < length; ++i)
        bitstream_write(b, 8, data[i]);
}

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint32_t count;
    uint32_t offset;
    uint32_t size;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th07_header_t;

static int
th06_open(
    thdat_t* thdat,
    thtk_error_t** error)
{
    char magic[4];

    if (thtk_io_read(thdat->stream, magic, 4, error) != 4)
        return 0;

    if (strncmp(magic, "PBG3", 4) == 0) {
        struct bitstream b;
        bitstream_init(&b, thdat->stream);
        uint32_t entry_count = th06_read_uint32(&b);
        thdat->offset = th06_read_uint32(&b);

        if (thtk_io_seek(thdat->stream, thdat->offset, SEEK_SET, error) == -1)
            return 0;

        bitstream_init(&b, thdat->stream);
        for (unsigned int i = 0; i < entry_count; ++i) {
            thdat_entry_t* entry;
            ARRAY_GROW(thdat->entry_count, thdat->entries, entry);
            thdat_entry_init(entry);
            th06_read_uint32(&b);
            th06_read_uint32(&b);
            entry->extra = th06_read_uint32(&b);
            entry->offset = th06_read_uint32(&b);
            entry->size = th06_read_uint32(&b);
            th06_read_string(&b, 255, entry->name);
        }
    } else if (strncmp(magic, "PBG4", 4) == 0) {
        th07_header_t header;

        if (thtk_io_read(thdat->stream, &header, sizeof(header), error) != sizeof(header))
            return 0;

        off_t end = thtk_io_seek(thdat->stream, 0, SEEK_END, error);
        if (end == -1)
            return 0;

        if (thtk_io_seek(thdat->stream, header.offset, SEEK_SET, error) == -1)
            return 0;

        thtk_io_t* entry_headers = thtk_io_open_growing_memory(error);
        /* XXX: I can't use header.size for this. */
        if (th_unlzss(thdat->stream, entry_headers, header.size, error) == -1)
            return 0;

        const uint32_t* ptr = (uint32_t*)thtk_io_map(entry_headers, 0, header.size, error);
        if (!ptr)
            return 0;
        for (unsigned int i = 0; i < header.count; ++i) {
            thdat_entry_t* entry = NULL;
            ARRAY_GROW(thdat->entry_count, thdat->entries, entry);
            thdat_entry_init(entry);
            strcpy(entry->name, (char*)ptr);
            ptr = (uint32_t*)((char*)ptr + strlen(entry->name) + 1);
            entry->offset = *ptr++;
            entry->size = *ptr++;
            entry->extra = *ptr++;
        }

        thtk_io_unmap(entry_headers, (unsigned char*)ptr);
        thtk_io_close(entry_headers);
    } else {
        thtk_error_new(error, "magic string not recognized");
        return 0;
    }

    off_t end_offset = thtk_io_seek(thdat->stream, 0, SEEK_END, error);
    if (end_offset == -1)
        return 0;

    thdat_entry_t* prev = NULL;
    for (unsigned int i = 0; i < thdat->entry_count; ++i) {
        thdat_entry_t* entry = &thdat->entries[i];
        if (prev)
            prev->zsize = entry->offset - prev->offset;
        prev = entry;
    }
    prev->zsize = end_offset - prev->offset;

    return 1;
}

static ssize_t
th06_read(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    unsigned char* zdata = malloc(entry->zsize);

    int failed;
#pragma omp critical
    {
        failed = (thtk_io_seek(thdat->stream, entry->offset, SEEK_SET, error) == -1) ||
                 (thtk_io_read(thdat->stream, zdata, entry->zsize, error) != entry->zsize);
    }
    if (failed)
        return -1;

    thtk_io_t* zdata_stream = thtk_io_open_memory(zdata, entry->zsize, error);
    if (!zdata_stream)
        return -1;

    int ret = th_unlzss(zdata_stream, output, entry->size, error);

    thtk_io_close(zdata_stream);
    
    return ret;
}

static int
th06_create(
    thdat_t* thdat,
    thtk_error_t** error)
{
    /* 13 is the largest size the header can have, so some bytes might be
     * wasted. */
    thdat->offset = thdat->version == 6 ? 13 : 16;
    if (thtk_io_seek(thdat->stream, thdat->offset, SEEK_SET, error) == -1)
        return 0;
    return 1;
}

static ssize_t
th06_write(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    entry->size = input_length;
    thtk_io_t* zdata_stream = thtk_io_open_growing_memory(error);
    if (!zdata_stream)
        return -1;
    /* There is a chance that one of the games support uncompressed data. */

    if ((entry->zsize = th_lzss(input, entry->size, zdata_stream, error)) == -1)
        return -1;

    unsigned char* zdata = thtk_io_map(zdata_stream, 0, entry->zsize, error);
    if (!zdata)
        return -1;

    if (thdat->version == 6) {
        entry->extra = 0;
        for (ssize_t i = 0; i < entry->zsize; ++i)
            entry->extra += zdata[i];
    }

    int ret;

#pragma omp critical
    {
        ret = thtk_io_write(thdat->stream, zdata, entry->zsize, error);
        entry->offset = thdat->offset;
        thdat->offset += entry->zsize;
    }

    thtk_io_unmap(zdata_stream, zdata);
    thtk_io_close(zdata_stream);

    if (ret != entry->zsize)
        return -1;

    return ret;
}

static int
th06_close(
    thdat_t* thdat,
    thtk_error_t** error)
{
    const char* magic = thdat->version == 6 ? "PBG3" : "PBG4";
    unsigned int i;
    uint32_t header[3];
    const uint32_t zero = 0;
    struct bitstream b;
    ssize_t buffer_size;
    thtk_io_t* buffer = NULL;

    if (thdat->version == 6) {
        bitstream_init(&b, thdat->stream);
    } else {
        buffer = thtk_io_open_growing_memory(error);
    }

    for (i = 0; i < thdat->entry_count; ++i) {
        thdat_entry_t* entry = &thdat->entries[i];

        if (thdat->version == 6) {
            /* These values are unknown, but it seems they can be ignored. */
            uint32_t unknown1 = 0; /* The same for all entries in an archive. */
            uint32_t unknown2 = 0; /* Starts at a high value.
                                    * Increases by a random multiple of a thousand
                                    * per entry. */
            th06_write_uint32(&b, unknown1);
            th06_write_uint32(&b, unknown2);
            th06_write_uint32(&b, entry->extra);
            th06_write_uint32(&b, entry->offset);
            th06_write_uint32(&b, entry->size);
            th06_write_string(&b, strlen(entry->name) + 1, entry->name);
        } else {
            if (thtk_io_write(buffer, entry->name, strlen(entry->name) + 1, error) == -1)
                return 0;
            if (thtk_io_write(buffer, &entry->offset, sizeof(uint32_t), error) != sizeof(uint32_t))
                return 0;
            if (thtk_io_write(buffer, &entry->size, sizeof(uint32_t), error) != sizeof(uint32_t))
                return 0;
            if (thtk_io_write(buffer, &zero, sizeof(uint32_t), error) != sizeof(uint32_t))
                return 0;
        }
    }

    if (thdat->version == 6) {
        bitstream_finish(&b);
    } else {
        if (thtk_io_write(buffer, &zero, sizeof(uint32_t), error) != sizeof(uint32_t))
            return 0;

        if ((buffer_size = thtk_io_seek(buffer, 0, SEEK_CUR, error)) == -1)
            return 0;
        if (thtk_io_seek(buffer, 0, SEEK_SET, error) == -1)
            return 0;
        if (th_lzss(buffer, buffer_size, thdat->stream, error) == -1)
            return 0;
        thtk_io_close(buffer);
    }

    if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
        return 0;

    if (thtk_io_write(thdat->stream, magic, 4, error) == -1)
        return 0;

    if (thdat->version == 6) {
        bitstream_init(&b, thdat->stream);
        th06_write_uint32(&b, thdat->entry_count);
        th06_write_uint32(&b, thdat->offset);
        bitstream_finish(&b);
    } else {
        header[0] = thdat->entry_count;
        header[1] = thdat->offset;
        header[2] = buffer_size;

        if (thtk_io_write(thdat->stream, header, sizeof(header), error) == -1)
            return 0;
    }

    return 1;
}

const thdat_module_t archive_th06 = {
    THDAT_BASENAME,
    th06_open,
    th06_create,
    th06_close,
    th06_read,
    th06_write
};
