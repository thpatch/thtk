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
#include <thtk/thtk.h>

typedef struct {
    char name[256];
    /* Format-specific data. */
    uint32_t extra;
    /* These fields are -1 before being filled out. */
    /* Original file size. */
    ssize_t size;
    /* Compressed file size. */
    ssize_t zsize;
    /* Offset in archive. */
    ssize_t offset;
} thdat_entry_t;

void thdat_entry_init(thdat_entry_t* entry);

typedef struct thdat_module_t thdat_module_t;

struct thdat_t {
    unsigned int version;
    const thdat_module_t* module;
    thtk_io_t* stream;
    size_t entry_count;
    thdat_entry_t* entries;
    uint32_t offset;
};

/* Strip path names. */
#define THDAT_BASENAME 1
/* Force uppercase filenames. */
#define THDAT_UPPERCASE 2
/* Check filenames for 8.3 format. */
#define THDAT_8_3 4

struct thdat_module_t {
    /* THDAT_ flags. */
    uint32_t flags;

    int (*open)(thdat_t* thdat, thtk_error_t** error);
    int (*create)(thdat_t* thdat, thtk_error_t** error);
    int (*close)(thdat_t* thdat, thtk_error_t** error);

    ssize_t (*read)(thdat_t* thdat, int entry, thtk_io_t* output, thtk_error_t** error);
    ssize_t (*write)(thdat_t* thdat, int entry, thtk_io_t* input, size_t length, thtk_error_t** error);
};

#define ARRAY_GROW(counter, array, target) \
    do { \
        ++(counter); \
        (array) = realloc((array), (counter) * sizeof(*(array))); \
        (target) = &(array)[(counter) - 1]; \
    } while (0)

#endif
