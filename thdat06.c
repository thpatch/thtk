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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "bits.h"
#include "thdat.h"
#include "thlzss.h"
#include "util.h"

static uint32_t
th06_read_uint32(struct bitstream* b)
{
    uint32_t size = bitstream_read(b, 2);
    return bitstream_read(b, (size + 1) * 8);
}

static void
th06_write_uint32(struct bitstream* b, uint32_t value)
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
th06_read_string(struct bitstream* b, unsigned int length, char* data)
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
th06_write_string(struct bitstream* b, unsigned int length, char* data)
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

static archive_t*
th06_open(FILE* stream, unsigned int version)
{
    archive_t* archive;
    char magic[4];
    entry_t* e;
    unsigned int i;

    if (!util_read(stream, magic, 4))
        return NULL;

    archive = thdat_open(stream, version);

    if (strncmp(magic, "PBG3", 4) == 0) {
        struct bitstream b;
        uint32_t count;
        bitstream_init_stream(&b, stream);
        archive->version = 6;
        count = th06_read_uint32(&b);
        archive->offset = th06_read_uint32(&b);

        if (!util_seek(stream, archive->offset)) {
            free(archive);
            return NULL;
        }

        bitstream_init_stream(&b, stream);
        for (i = 0; i < count; ++i) {
            e = thdat_add_entry(archive);
            th06_read_uint32(&b);
            th06_read_uint32(&b);
            e->extra = th06_read_uint32(&b);
            e->offset = th06_read_uint32(&b);
            e->size = th06_read_uint32(&b);
            th06_read_string(&b, 255, e->name);
        }
    } else if (strncmp(magic, "PBG4", 4) == 0) {
        char* data;
        char* ptr;
        th07_header_t h;
        archive->version = 7;
        if (!util_read(stream, &h, sizeof(th07_header_t))) {
            free(archive);
            return NULL;
        }

        data = malloc(h.size);

        if (!util_seek(stream, h.offset)) {
            free(data);
            free(archive);
            return NULL;
        }
        th_unlz_file(stream, (unsigned char*)data, h.size);

        ptr = data;
        for (i = 0; i < h.count; ++i) {
            e = thdat_add_entry(archive);
            strcpy(e->name, ptr);
            ptr += strlen(e->name) + 1;
            memcpy(&e->offset, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
            memcpy(&e->size, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
            ptr += sizeof(uint32_t);
        }

        free(data);
    } else {
        /* TODO: Proper error message. */
        abort();
    }

    return archive;
}

static int
th06_extract(archive_t* archive, entry_t* entry, FILE* stream)
{
    int ret = 0;
    unsigned char* data = malloc(entry->size);

#pragma omp critical
{
    if (util_seek(archive->stream, entry->offset)) {
        ret = 1;
        th_unlz_file(archive->stream, data, entry->size);
    }
}

    if (!ret || !util_write(stream, data, entry->size)) {
        free(data);
        return 0;
    }

    free(data);

    return 1;
}

static archive_t*
th06_create(FILE* stream, unsigned int version, unsigned int count)
{
    /* 13 is the largest size the header can have, so some bytes might be
     * wasted. */
    return archive_create(stream, version, version == 6 ? 13 : 16, count);
}

static int
th06_write(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned char* data;

    /* There is a chance that one of the games support uncompressed data. */

    data = thdat_read_file_lzss(entry, stream);

    if (archive->version == 6) {
        unsigned int i;
        for (i = 0; i < entry->zsize; ++i)
            entry->extra += data[i];
    }

    return thdat_write_entry(archive, entry, data);
}

static int
th06_close(archive_t* archive)
{
    const char* magic = archive->version == 6 ? "PBG3" : "PBG4";
    unsigned int i;
    uint32_t header[3];
    unsigned char* buffer = NULL;
    unsigned char* buffer_ptr = NULL;
    unsigned char* zbuffer;
    unsigned int list_size = 0;
    unsigned int list_zsize = 0;
    struct bitstream b;

    thdat_sort(archive);

    if (archive->version == 6) {
        bitstream_init_growing(&b, 1024);
    } else {
        for (i = 0; i < archive->count; ++i)
            list_size += strlen(archive->entries[i].name) + 1 + (sizeof(uint32_t) * 3);

        list_size += 4;
        buffer = malloc(list_size);
        memset(buffer, 0, list_size);

        buffer_ptr = buffer;
    }

    for (i = 0; i < archive->count; ++i) {
        const uint32_t zero = 0;
        entry_t* entry = &archive->entries[i];
        /* These values are unknown, but it seems they can be ignored. */
        uint32_t unknown1 = 0; /* The same for all entries in an archive. */
        uint32_t unknown2 = 0; /* Starts at a high value.
                                * Increases by a random multiple of a thousand
                                * per entry. */

        if (archive->version == 6) {
            th06_write_uint32(&b, unknown1);
            th06_write_uint32(&b, unknown2);
            th06_write_uint32(&b, entry->extra);
            th06_write_uint32(&b, entry->offset);
            th06_write_uint32(&b, entry->size);
            th06_write_string(&b, strlen(entry->name) + 1, entry->name);
        } else {
            buffer_ptr = mempcpy(buffer_ptr, entry->name, strlen(entry->name) + 1);
            buffer_ptr = mempcpy(buffer_ptr, &entry->offset, sizeof(uint32_t));
            buffer_ptr = mempcpy(buffer_ptr, &entry->size, sizeof(uint32_t));
            buffer_ptr = mempcpy(buffer_ptr, &zero, sizeof(uint32_t));
        }
    }

    if (archive->version == 6) {
        bitstream_finish(&b);

        if (!util_write(archive->stream, b.io.buffer.buffer, b.byte_count)) {
            bitstream_free(&b);
            return 0;
        }

        bitstream_free(&b);
    } else {
        memset(buffer_ptr, 0, sizeof(uint32_t));

        zbuffer = th_lz_mem(buffer, list_size, &list_zsize);
        free(buffer);

        if (!util_write(archive->stream, zbuffer, list_zsize)) {
            free(zbuffer);
            return 0;
        }
        free(zbuffer);
    }

    if (!util_seek(archive->stream, 0))
        return 0;

    if (!util_write(archive->stream, magic, 4))
        return 0;

    if (archive->version == 6) {
        bitstream_init_fixed(&b, malloc(9), 9);

        th06_write_uint32(&b, archive->count);
        th06_write_uint32(&b, archive->offset);

        bitstream_finish(&b);

        if (!util_write(archive->stream, b.io.buffer.buffer, b.byte_count)) {
            bitstream_free(&b);
            return 0;
        }

        bitstream_free(&b);
    } else {
        header[0] = archive->count;
        header[1] = archive->offset;
        header[2] = list_size;

        if (!util_write(archive->stream, header, sizeof(header)))
            return 0;
    }

    return 1;
}

const archive_module_t archive_th06 = {
    THDAT_BASENAME,
    th06_create,
    th06_write,
    th06_close,
    th06_open,
    th06_extract
};
