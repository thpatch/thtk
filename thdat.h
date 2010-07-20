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
#ifndef THDAT_H_
#define THDAT_H_

#include <config.h>
#include <inttypes.h>
#include <stdio.h>

typedef struct {
    char name[256];
    uint32_t size;
    uint32_t zsize;
    uint32_t offset;

    uint32_t extra;
} entry_t;

typedef struct {
    unsigned int version;
    FILE* stream;
    uint32_t offset;
    uint32_t count;
    entry_t* entries;
} archive_t;

/* Strip path names. */
#define THDAT_BASENAME 1

typedef struct {
    /* THDAT_ flags. */
    uint32_t flags;
    /* Takes a truncated file opened for writing, archive version, and an
     * estimated file count.  Returns private data, or NULL upon error. */
    archive_t* (*create)(FILE*, unsigned int, unsigned int);
    /* Takes private data, a file opened for reading, and a filename.  Returns
     * 1, or 0 upon error.  Private data is freed upon error. */
    int (*write)(archive_t*, entry_t*, FILE*);
    /* Takes private data. Returns 1, or 0 upon error.  Private data is always
     * freed. */
    int (*close)(archive_t*);

    /* Takes a stream, and an archive version. */
    archive_t* (*open)(FILE*, unsigned int);
    /* Takes private data, the entry to extract, and the stream to write the
     * data to.  Returns 1, or 0 upon error. */
    int (*extract)(archive_t*, entry_t*, FILE*);
    /* XXX: Maybe something to clean up. */
} archive_module_t;

archive_t* thdat_open(FILE* stream, unsigned int version);
/* TODO: Rename functions. */
archive_t* archive_create(FILE* stream, uint32_t version, uint32_t offset, unsigned int count);

/* Reads entry->size bytes from the passed file descriptor.  Returns a newly
 * allocated buffer with the read data.  Returns NULL on error. */
unsigned char* thdat_read_file(entry_t* entry, FILE* stream);
/* Reads entry->size bytes from the passed file descriptor.  Returns a newly
 * allocated buffer containing an LZSS compressed version of the read data.
 * Returns NULL on error. */
unsigned char* thdat_read_file_lzss(entry_t* entry, FILE* stream);
/* Performs RLE on the passed buffer.  Frees the passed buffer.  Updates
 * entry->zsize.  Returns a newly allocated buffer containing the RLE data.
 * Returns NULL on error. */
unsigned char* thdat_rle(entry_t* entry, unsigned char* data);
entry_t* thdat_add_entry(archive_t* archive);
/* Writes the entry->zsize bytes from the passed buffer to the archive.  Frees
 * the passed buffer.  Updates the archive offset.  Returns 0 on error. */
int thdat_write_entry(archive_t* archive, entry_t* entry, unsigned char* data);
/* Sorts the entries in the archive according to their offsets. */
void thdat_sort(archive_t* archive);

#endif
