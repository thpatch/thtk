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
#ifndef DATPACKER_H_
#define DATPACKER_H_

#include <config.h>
#include <inttypes.h>
#include <stdio.h>

extern char library_error[];
#define LIBRARY_ERROR_SIZE 512

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
     * 0, or -1 upon error.  Private data is freed upon error. */
    int (*write)(archive_t*, entry_t*, FILE*);
    /* Takes private data. Returns 0, or -1 upon error.  Private data is always
     * freed. */
    int (*close)(archive_t*);

    /* Takes a stream, and an archive version. */
    archive_t* (*open)(FILE*, unsigned int);
    /* Takes private data, the entry to extract, and the stream to write the
     * data to.  Returns 0, or -1 upon error. */
    int (*extract)(archive_t*, entry_t*, FILE*);
    /* XXX: Maybe something to clean up. */
} archive_module_t;

#endif
