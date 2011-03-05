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
#ifndef FILE_H_
#define FILE_H_

#include <stdio.h>
#include <sys/types.h>

/* A wrapper for fseek with SEEK_SET which prints an error message upon error.*/
int file_seek(
    FILE* stream,
    long offset);

/* Checks if a stream is seekable. */
int file_seekable(
    FILE* stream);

/* A wrapper for ftell which prints an error message upon error. */
long file_tell(
    FILE* stream);

/* A wrapper for fread which prints an error message upon error. */
int file_read(
    FILE* stream,
    void* buffer,
    size_t size);

/* A wrapper for fwrite which prints an error message upon error. */
int file_write(
    FILE* stream,
    const void* buffer,
    size_t size);

/* Reads a stream until '\0'.  Returns -1 on error. */
ssize_t file_read_asciiz(
    FILE* stream,
    char* buffer,
    size_t size);

/* Returns the filesize of the passed file stream, or -1 and an error message
 * upon error. */
long file_fsize(
    FILE* stream);

void* file_mmap(
    FILE* stream,
    size_t length);

int file_munmap(
    void* map,
    size_t length);

#endif
