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
#ifndef THTK_IO_H_
#define THTK_IO_H_

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <thtk/error.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct thtk_io_t thtk_io_t;

/* See the documentation for read(2).  Returns the number of bytes read, or -1
 * on error. */
THTK_EXPORT ssize_t thtk_io_read(thtk_io_t* io, void* buf, size_t count, thtk_error_t** error);
/* See the documentation for write(2).  Returns the number of bytes written, or
 * -1 on error. */
THTK_EXPORT ssize_t thtk_io_write(thtk_io_t* io, const void* buf, size_t count, thtk_error_t** error);
/* See the documentation for lseek(2).  Returns the new offset, or -1 on error. */
THTK_EXPORT off_t thtk_io_seek(thtk_io_t* io, off_t offset, int whence, thtk_error_t** error);
/* Returns a memory location which maps to the content of the IO object at the specified offset.
 * What happens to the underlying object when the data is changed is not yet defined. */
THTK_EXPORT unsigned char* thtk_io_map(thtk_io_t* io, off_t offset, size_t count, thtk_error_t** error);
/* Frees a mapping. */
THTK_EXPORT void thtk_io_unmap(thtk_io_t* io, unsigned char* map);
/* Closes and frees the IO object.  Returns 0 on error, otherwise 1. */
THTK_EXPORT int thtk_io_close(thtk_io_t* io);
/* See the documentation for pread(2).  Returns the number of bytes read, or -1
 * on error. */
THTK_EXPORT ssize_t thtk_io_pread(thtk_io_t* io, void* buf, size_t count, off_t offset, thtk_error_t** error);
/* See the documentation for pwrite(2).  Returns the number of bytes written, or
 * -1 on error. */
THTK_EXPORT ssize_t thtk_io_pwrite(thtk_io_t* io, const void* buf, size_t count, off_t offset, thtk_error_t** error);

/* Opens a file in the mode specified, the mode works as it does for fopen. */
THTK_EXPORT thtk_io_t* thtk_io_open_file(const char* path, const char* mode, thtk_error_t** error);
#ifdef _WIN32
THTK_EXPORT thtk_io_t* thtk_io_open_file_w(const wchar_t* path, const wchar_t* mode, thtk_error_t** error);
#endif
/* Opens a memory buffer for IO. */
THTK_EXPORT thtk_io_t* thtk_io_open_memory(void* buf, size_t size, thtk_error_t** error);
/* Creates a new memory buffer that automatically expands. */
THTK_EXPORT thtk_io_t* thtk_io_open_growing_memory(thtk_error_t** error);

#ifdef __cplusplus
}
#endif

#endif
