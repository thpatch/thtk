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
#include <stddef.h>
#include <thtk/io.h>

struct thtk_io_t {
    void* private;

    ssize_t (*read)(thtk_io_t* io, void* buf, size_t count, thtk_error_t** error);
    ssize_t (*write)(thtk_io_t* io, const void* buf, size_t count, thtk_error_t** error);
    off_t (*seek)(thtk_io_t* io, off_t offset, int whence, thtk_error_t** error);
    unsigned char* (*map)(thtk_io_t* io, off_t offset, size_t count, thtk_error_t** error);
    void (*unmap)(thtk_io_t* io, unsigned char* map);
    int (*close)(thtk_io_t* io);
};

ssize_t
thtk_io_read(
    thtk_io_t* io,
    void* buf,
    size_t count,
    thtk_error_t** error)
{
    ssize_t ret;
    if (!io || !buf || !count) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    ret = io->read(io, buf, count, error);
    if (ret != (ssize_t)count) {
        thtk_error_new(error, "short read");
        return -1;
    }
    return ret;
}

ssize_t
thtk_io_write(
    thtk_io_t* io,
    const void* buf,
    size_t count,
    thtk_error_t** error)
{
    ssize_t ret;
    if (!io || !buf || !count) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    ret = io->write(io, buf, count, error);
    if (ret != (ssize_t)count) {
        thtk_error_new(error, "short write");
        return -1;
    }
    return ret;
}

off_t
thtk_io_seek(
    thtk_io_t* io,
    off_t offset,
    int whence,
    thtk_error_t** error)
{
    if (!io || !(whence == SEEK_CUR || whence == SEEK_SET || whence == SEEK_END)) {
        thtk_error_new(error, "invalid parameter passed");
        return (off_t)-1;
    }
    return io->seek(io, offset, whence, error);
}

unsigned char*
thtk_io_map(
    thtk_io_t* io,
    off_t offset,
    size_t count,
    thtk_error_t** error)
{
    if (!io || !count) {
        thtk_error_new(error, "invalid parameter passed");
        return NULL;
    }
    return io->map(io, offset, count, error);
}

void
thtk_io_unmap(
    thtk_io_t* io,
    unsigned char* map)
{
    if (!io || !map) {
        return;
    }
    io->unmap(io, map);
}

int
thtk_io_close(
    thtk_io_t* io)
{
    int ret;
    if (!io) {
        return 0;
    }
    ret = io->close(io);
    free(io);
    return ret;
}

static ssize_t
thtk_io_file_read(
    thtk_io_t* io,
    void* buf,
    size_t count,
    thtk_error_t** error)
{
    size_t ret = fread(buf, 1, count, (FILE*)io->private);
    if (ferror((FILE*)io->private)) {
        thtk_error_new(error, "error while reading: %s", strerror(errno));
        return -1;
    }
    return ret;
}

static ssize_t
thtk_io_file_write(
    thtk_io_t* io,
    const void* buf,
    size_t count,
    thtk_error_t** error)
{
    size_t ret = fwrite(buf, 1, count, (FILE*)io->private);
    if (ferror((FILE*)io->private)) {
        thtk_error_new(error, "error while writing: %s", strerror(errno));
        return -1;
    }
    return ret;
}

static off_t
thtk_io_file_seek(
    thtk_io_t* io,
    off_t offset,
    int whence,
    thtk_error_t** error)
{
    if (fseek(io->private, (long)offset, whence) == -1) {
        thtk_error_new(error, "error while seeking: %s", strerror(errno));
        return (off_t)-1;
    }

    return ftell(io->private);
}

static unsigned char*
thtk_io_file_map(
    thtk_io_t* io,
    off_t offset,
    size_t count,
    thtk_error_t** error)
{
    if (thtk_io_file_seek(io, offset, SEEK_SET, error) == -1)
        return NULL;
    unsigned char* map = malloc(count);
    if (thtk_io_file_read(io, map, count, error) != (ssize_t)count) {
        free(map);
        return NULL;
    }
    return map;
}

static void
thtk_io_file_unmap(
    thtk_io_t* io,
    unsigned char* map)
{
    free(map);
}

static int
thtk_io_file_close(
    thtk_io_t* io)
{
    return fclose(io->private) == 0;
}

static const thtk_io_t
thtk_io_file_template = {
    NULL,
    thtk_io_file_read,
    thtk_io_file_write,
    thtk_io_file_seek,
    thtk_io_file_map,
    thtk_io_file_unmap,
    thtk_io_file_close,
};

thtk_io_t*
thtk_io_open_file(
    const char* path,
    const char* mode,
    thtk_error_t** error)
{
    thtk_io_t* io = malloc(sizeof(*io));
    *io = thtk_io_file_template;
    io->private = fopen(path, mode);

    if (!io->private) {
        thtk_error_new(error, "error while opening file `%s': %s", path, strerror(errno));
        free(io);
        return NULL;
    }

    return io;
}

typedef struct {
    off_t offset;
    ssize_t size;
    void* memory;
} thtk_io_memory_t;

static ssize_t
thtk_io_memory_read(
    thtk_io_t* io,
    void* buf,
    size_t count,
    thtk_error_t** error)
{
    thtk_io_memory_t* private = io->private;
    if (private->offset + (ssize_t)count >= private->size)
        count = private->size - private->offset;
    memcpy(buf, (unsigned char*)private->memory + private->offset, count);
    private->offset += count;
    return count;
}

static ssize_t
thtk_io_memory_write(
    thtk_io_t* io,
    const void* buf,
    size_t count,
    thtk_error_t** error)
{
    thtk_io_memory_t* private = io->private;
    if (private->offset + (ssize_t)count >= private->size)
        count = private->size - private->offset;
    memcpy((unsigned char*)private->memory + private->offset, buf, count);
    private->offset += count;
    return count;
}

static off_t
thtk_io_memory_seek(
    thtk_io_t* io,
    off_t offset,
    int whence,
    thtk_error_t** error)
{
    thtk_io_memory_t* private = io->private;
    switch (whence) {
    case SEEK_SET:
        if (offset > private->size ||
            offset < 0) {
            thtk_error_new(error, "seek out of bounds");
            return (off_t)-1;
        }
        private->offset = offset;
        break;
    case SEEK_CUR:
        if (private->offset + offset > private->size ||
            private->offset + offset < 0) {
            thtk_error_new(error, "seek out of bounds");
            return (off_t)-1;
        }
        private->offset += offset;
        break;
    case SEEK_END:
        if (private->size + offset > private->size ||
            private->size + offset < 0) {
            thtk_error_new(error, "seek out of bounds");
            return (off_t)-1;
        }
        private->offset = private->size + offset;
        break;
    default:
        thtk_error_new(error, "impossible");
        return (off_t)-1;
    }

    return private->offset;
}

static unsigned char*
thtk_io_memory_map(
    thtk_io_t* io,
    off_t offset,
    size_t count,
    thtk_error_t** error)
{
    thtk_io_memory_t* private = io->private;
    return (unsigned char*)private->memory + offset;
}

static void
thtk_io_memory_unmap(
    thtk_io_t* io,
    unsigned char* map)
{
    thtk_io_memory_t* private = io->private;
    if (private->memory == map)
        return;
    
    return;
}

static int
thtk_io_memory_close(
    thtk_io_t* io)
{
    thtk_io_memory_t* private = io->private;
    free(private->memory);
    free(io->private);
    return 1;
}

static const thtk_io_t
thtk_io_memory_template = {
    NULL,
    thtk_io_memory_read,
    thtk_io_memory_write,
    thtk_io_memory_seek,
    thtk_io_memory_map,
    thtk_io_memory_unmap,
    thtk_io_memory_close,
};

thtk_io_t*
thtk_io_open_memory(
    void* buf,
    size_t size,
    thtk_error_t** error)
{
    thtk_io_t* io = malloc(sizeof(*io));
    *io = thtk_io_memory_template;
    thtk_io_memory_t* private = malloc(sizeof(*private));
    private->offset = 0;
    private->size = size;
    private->memory = buf;
    io->private = private;

    return io;
}

typedef struct {
    off_t offset;
    ssize_t size;
    ssize_t memory_size;
    void* memory;
} thtk_io_growing_memory_t;

static ssize_t
thtk_io_growing_memory_read(
    thtk_io_t* io,
    void* buf,
    size_t count,
    thtk_error_t** error)
{
    thtk_io_growing_memory_t* private = io->private;
    if (private->offset + (ssize_t)count >= private->size)
        count = private->size - private->offset;
    memcpy(buf, (unsigned char*)private->memory + private->offset, count);
    private->offset += count;
    return count;
}

static ssize_t
thtk_io_growing_memory_write(
    thtk_io_t* io,
    const void* buf,
    size_t count,
    thtk_error_t** error)
{
    thtk_io_growing_memory_t* private = io->private;
    if (private->offset + (ssize_t)count >= private->size) {
        private->size = private->offset + (ssize_t)count;
        if (private->size >= private->memory_size) {
            while (private->size >= private->memory_size) {
                if (!private->memory_size) {
                    private->memory_size = 4096;
                } else {
                    private->memory_size <<= 1;
                }
            }
            private->memory = realloc(private->memory, private->memory_size);
        }
    }
    memcpy((unsigned char*)(private->memory) + private->offset, buf, count);
    private->offset += count;
    return count;
}

static off_t
thtk_io_growing_memory_seek(
    thtk_io_t* io,
    off_t offset,
    int whence,
    thtk_error_t** error)
{
    thtk_io_growing_memory_t* private = io->private;
    switch (whence) {
    case SEEK_SET:
        if (offset > private->size ||
            offset < 0) {
            thtk_error_new(error, "seek out of bounds");
            return (off_t)-1;
        }
        private->offset = offset;
        break;
    case SEEK_CUR:
        if (private->offset + offset > private->size ||
            private->offset + offset < 0) {
            thtk_error_new(error, "seek out of bounds");
            return (off_t)-1;
        }
        private->offset += offset;
        break;
    case SEEK_END:
        if (private->size + offset > private->size ||
            private->size + offset < 0) {
            thtk_error_new(error, "seek out of bounds");
            return (off_t)-1;
        }
        private->offset = private->size + offset;
        break;
    default:
        thtk_error_new(error, "impossible");
        return (off_t)-1;
    }

    return private->offset;
}

static unsigned char*
thtk_io_growing_memory_map(
    thtk_io_t* io,
    off_t offset,
    size_t count,
    thtk_error_t** error)
{
    thtk_io_growing_memory_t* private = io->private;
    return (unsigned char*)private->memory + offset;
}

static void
thtk_io_growing_memory_unmap(
    thtk_io_t* io,
    unsigned char* map)
{
    thtk_io_growing_memory_t* private = io->private;
    if (private->memory == map)
        return;
    
    return;
}

static int
thtk_io_growing_memory_close(
    thtk_io_t* io)
{
    thtk_io_growing_memory_t* private = io->private;
    free(private->memory);
    free(io->private);
    return 1;
}

static const thtk_io_t
thtk_io_growing_memory_template = {
    NULL,
    thtk_io_growing_memory_read,
    thtk_io_growing_memory_write,
    thtk_io_growing_memory_seek,
    thtk_io_growing_memory_map,
    thtk_io_growing_memory_unmap,
    thtk_io_growing_memory_close,
};

thtk_io_t*
thtk_io_open_growing_memory(
    thtk_error_t** error)
{
    thtk_io_t* io = malloc(sizeof(*io));
    *io = thtk_io_growing_memory_template;
    thtk_io_growing_memory_t* private = malloc(sizeof(*private));
    private->offset = 0;
    private->size = 0;
    private->memory_size = 0;
    private->memory = NULL;
    io->private = private;

    return io;
}
