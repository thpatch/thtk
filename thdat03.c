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
#include "program.h"
#include "thdat.h"
#include "thrle.h"
#include "util.h"

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
    char name[13];
    uint16_t zsize;
    uint16_t size;
    uint32_t offset;
    uint8_t zero[8];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th03_entry_header_t;

/* TODO: Figure out if these can be calculated or not. */
const uint8_t archive_key = 0x12;
const uint8_t entry_key = 0x34;
/* Used for uncompressed entries. */
const uint16_t magic1 = 0xf388;
/* Used for compressed entries. */
const uint16_t magic2 = 0x9595;

static archive_t*
th03_open(FILE* stream, unsigned int version)
{
    archive_t* archive;
    th03_archive_header_t fh;
    th03_entry_header_t fe;
    entry_t* e;
    unsigned int i;
    unsigned char* data;

    if (!util_read(stream, &fh, sizeof(th03_archive_header_t)))
        return NULL;

    data = malloc(fh.size);
    if (!util_read(stream, data, fh.size))
        return NULL;

    for (i = 0; i < fh.size; ++i) {
        data[i] ^= fh.key;
        fh.key -= data[i];
    }

    archive = thdat_open(stream, version);

    for (i = 0; i < fh.count; ++i) {
        memcpy(&fe, data + i * sizeof(th03_entry_header_t), sizeof(th03_entry_header_t));
        if (!fe.magic) {
            fprintf(stderr, "fe.magic == 0?\n");
            break;
        }

        e = thdat_add_entry(archive);
        e->size = fe.size;
        e->zsize = fe.zsize;
        e->offset = fe.offset;
        e->extra = fe.key;
        memcpy(e->name, fe.name, 13);
    }

    return archive;
}

static int
th03_extract(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned int i;
    if (!util_seek(archive->stream, entry->offset))
        return 0;

    if (entry->size == entry->zsize) {
        for (i = 0; i < entry->zsize; ++i) {
            int c = fgetc(archive->stream);
            if (c == EOF) {
                fprintf(stderr, "%s: error while reading from archive: %s\n", argv0, strerror(errno));
                return 0;
            }
            fputc(c ^ entry->extra, stream);
        }
    } else {
        unsigned char* zbuf = malloc(entry->zsize);

        if (!util_read(archive->stream, zbuf, entry->zsize)) {
            free(zbuf);
            return 0;
        }

        for (i = 0; i < entry->zsize; ++i)
            zbuf[i] ^= entry->extra;

        th_unrle(zbuf, entry->zsize, stream);

        free(zbuf);
    }

    return 1;
}

static archive_t*
th03_create(FILE* stream, unsigned int version, unsigned int count)
{
    return archive_create(stream, version, sizeof(th03_archive_header_t) + (count + 1) * sizeof(th03_entry_header_t), count);
}

/* TODO: Check that filenames are 8.3, make it a THDAT_ flag. */
/* TODO: Find out if lowercase filenames are supported. */
static int
th03_write(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned int i;
    unsigned char* data;

    data = thdat_read_file(entry, stream);
    if (!data)
        return 1;

    data = thdat_rle(entry, data);

    for (i = 0; i < entry->zsize; ++i)
        data[i] ^= entry_key;

    return thdat_write_entry(archive, entry, data);
}

static int
th03_close(archive_t* archive)
{
    unsigned char* buffer;
    unsigned char* buffer_ptr;
    th03_archive_header_t fh;
    th03_entry_header_t fe;
    unsigned int i;
    unsigned int list_size = (archive->count + 1) * sizeof(th03_entry_header_t);
    unsigned char header_key = archive_key;

    if (!util_seek(archive->stream, 0))
        return 0;

    memset(&fh, 0, sizeof(th03_archive_header_t));

    fh.size = list_size;
    fh.unknown1 = 2;
    fh.count = archive->count;
    fh.key = header_key;

    util_write(archive->stream, &fh, sizeof(th03_archive_header_t));

    buffer = malloc(list_size);
    memset(buffer, 0, list_size);

    buffer_ptr = buffer;

    memset(&fe, 0, sizeof(th03_entry_header_t));

    for (i = 0; i < archive->count; ++i) {
        entry_t* entry = &archive->entries[i];
        fe.magic = entry->zsize == entry->size ? magic1 : magic2;
        fe.key = entry_key;
        memcpy(fe.name, entry->name, 13);
        fe.zsize = entry->zsize;
        fe.size = entry->size;
        fe.offset = entry->offset;

        buffer_ptr = mempcpy(buffer_ptr, &fe, sizeof(th03_entry_header_t));
    }

    for (i = 0; i < list_size; ++i) {
        unsigned char tmp = buffer[i];
        buffer[i] ^= header_key;
        header_key -= tmp;
    }

    if (!util_write(archive->stream, buffer, list_size))
        return 0;
    free(buffer);

    return 1;
}

const archive_module_t archive_th03 = {
    THDAT_BASENAME,
    th03_create,
    th03_write,
    th03_close,
    th03_open,
    th03_extract
};
