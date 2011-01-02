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
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "file.h"
#include "program.h"

int
file_seek(
    FILE* stream,
    long offset)
{
    if (fseek(stream, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed seeking to %lu: %s\n",
            argv0, offset, strerror(errno));
        return 0;
    } else
        return 1;
}

int
file_seekable(
    FILE* stream)
{
    return ftell(stream) != -1;
}

long
file_tell(
    FILE* stream)
{
    long pos = ftell(stream);
    if (pos == -1)
        fprintf(stderr, "%s: ftell failed: %s\n", argv0, strerror(errno));
    return pos;
}

int
file_read(
    FILE* stream,
    void* buffer,
    size_t size)
{
    if (fread_unlocked(buffer, size, 1, stream) != 1 && size != 0) {
        if (feof_unlocked(stream)) {
            fprintf(stderr,
                "%s: failed reading %lu bytes: unexpected end of file\n",
                argv0, (long unsigned int)size);
        } else {
            fprintf(stderr, "%s: failed reading %lu bytes: %s\n",
                argv0, (long unsigned int)size, strerror(errno));
        }
        return 0;
    } else
        return 1;
}

int
file_write(
    FILE* stream,
    const void* buffer,
    size_t size)
{
    if (fwrite_unlocked(buffer, size, 1, stream) != 1 && size != 0) {
        fprintf(stderr, "%s: failed writing %lu bytes: %s\n",
            argv0, (long unsigned int)size, strerror(errno));
        return 0;
    } else
        return 1;
}

ssize_t
file_read_asciiz(
    FILE* stream,
    char* buffer,
    size_t size)
{
    unsigned int i;
    int c;
    for (i = 0; i < size; ++i) {
        if (i == size - 1) {
            return -1;
        }
        c = getc_unlocked(stream);
        if (c == EOF) {
            return -1;
        }
        buffer[i] = c;
        if (!c)
            break;
    }
    return i + 1;
}

/* Might be better to use stat, but it's probably less cross-platform. */
/* This will fail for 2GB+ files, but who cares. */
long
file_fsize(
    FILE* stream)
{
#if defined(HAVE_FILENO) && defined(HAVE_FSTAT)
    struct stat sb;
    int fd = fileno_unlocked(stream);

    if (fd == -1) {
        fprintf(stderr, "%s: invalid stream: %s\n", argv0, strerror(errno));
        return -1;
    }

    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "%s: fstat failed: %s\n", argv0, strerror(errno));
        return -1;
    }

    return sb.st_size;
#else
    long prev, end;

    if ((prev = file_tell(stream)) == -1)
        return -1;

    if (fseek(stream, 0, SEEK_END) == -1) {
        fprintf(stderr, "%s: failed seeking to end: %s\n",
            argv0, strerror(errno));
        return -1;
    }

    if ((end = file_tell(stream)) == -1)
        return -1;

    if (!file_seek(stream, prev))
        return -1;

    return end;
#endif
}

void*
file_mmap(
    FILE* stream,
    size_t length)
{
#ifdef HAVE_MMAP
    void* map = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fileno_unlocked(stream), 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "%s: mmap failed: %s\n", argv0, strerror(errno));
        return NULL;
    }
    return map;
#else
    void* buffer = malloc(length);
    if (!file_read(stream, buffer, length))
        return NULL;
    return buffer;
#endif
}

int
file_munmap(
    void* map,
    size_t length)
{
#ifdef HAVE_MUNMAP
    if (munmap(map, length) == -1) {
        fprintf(stderr, "%s: munmap failed: %s\n", argv0, strerror(errno));
        return 0;
    }
    return 1;
#else
    length = 0;
    free(map);
    return 0;
#endif
}
