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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "datpacker.h"
#include "program.h"
#include "thdat.h"
#include "thrle.h"
#include "util.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t magic;
    uint8_t key;
    char name[13];
    uint32_t zsize;
    uint32_t size;
    uint32_t offset;
    uint32_t zero;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th02_entry_header_t;

static archive_t*
th02_open(FILE* stream, unsigned int version)
{
    archive_t* archive = thdat_open(stream, version);
    th02_entry_header_t fe;
    entry_t* e;
    unsigned int i;

    for (;;) {
        if (!util_read(stream, &fe, sizeof(th02_entry_header_t)))
            return NULL;

        if (!fe.magic)
            break;

        e = thdat_add_entry(archive);
        e->size = fe.size;
        e->zsize = fe.zsize;
        e->offset = fe.offset;
        /* XXX: Does not appear to be used. */
        e->extra = fe.key;
        for (i = 0; i < 13 && fe.name[i]; ++i)
            fe.name[i] ^= 0xff;
        memcpy(e->name, fe.name, 13);
    }

    return archive;
}

static int
th02_extract(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned int i;
    if (!util_seek(archive->stream, entry->offset))
        return -1;

    if (entry->size == entry->zsize) {
        for (i = 0; i < entry->zsize; ++i) {
            int c = fgetc(archive->stream);
            if (c == EOF) {
                fprintf(stderr, "%s: error while reading from archive: %s\n", argv0, strerror(errno));
                return -1;
            }
            fputc(c ^ 0x12, stream);
        }
    } else {
        unsigned char* zbuf = malloc(entry->zsize);

        if (!util_read(archive->stream, zbuf, entry->zsize))
            return -1;

        for (i = 0; i < entry->zsize; ++i)
            zbuf[i] ^= 0x12;

        th_unrle(zbuf, entry->zsize, stream);

        free(zbuf);
    }

    return 0;
}

static archive_t*
th02_create(FILE* stream, unsigned int version, unsigned int count)
{
    return archive_create(stream, version, (count + 1) * sizeof(th02_entry_header_t), count);
}

/* TODO: Check that filenames are 8.3, make it a THDAT_ flag. */
/* TODO: Find out if lowercase filenames are supported. */
static int
th02_write(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned int i;
    unsigned char* data;

    for (i = 0; i < 13; ++i)
        if (entry->name[i])
            entry->name[i] ^= 0xff;

    data = thdat_read_file(entry, stream);
    if (!data)
        return -1;

    data = thdat_rle(entry, data);

    for (i = 0; i < entry->zsize; ++i)
        data[i] ^= 0x12;

    return thdat_write_entry(archive, entry, data);
}

static int
th02_close(archive_t* archive)
{
    unsigned char* buffer;
    unsigned char* buffer_ptr;
    unsigned int i;
    unsigned int list_size = (archive->count + 1) * sizeof(th02_entry_header_t);
    th02_entry_header_t fe;
    fe.key = 3;
    fe.zero = 0;

    if (!util_seek(archive->stream, 0))
        return -1;

    buffer = malloc(list_size);
    memset(buffer, 0, list_size);

    buffer_ptr = buffer;
    for (i = 0; i < archive->count; ++i) {
        entry_t* entry = &archive->entries[i];

        fe.magic = entry->zsize == entry->size ? 0xf388 : 0x9595;
        memcpy(fe.name, entry->name, 13);
        fe.zsize = entry->zsize;
        fe.size = entry->size;
        fe.offset = entry->offset;

        buffer_ptr = mempcpy(buffer_ptr, &fe, sizeof(th02_entry_header_t));
    }

    if (fwrite(buffer, list_size, 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        free(buffer);
        return -1;
    }
    free(buffer);

    return 0;
}

const archive_module_t archive_th02 = {
    THDAT_BASENAME,
    th02_create,
    th02_write,
    th02_close,
    th02_open,
    th02_extract
};
