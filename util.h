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
#ifndef UTIL_H_
#define UTIL_H_

#include <config.h>
#include <stdio.h>
#include <inttypes.h>
#include <stddef.h>

#ifndef MIN
#  define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#  define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

void util_print_version(const char* name, const char* version);

/* Returns an unique string representation of a float. */
const char* util_printfloat(const void* data);

/* Returns a pointer to after the last directory separator in path. */
const char* util_shortname(const char* path);

/* Creates all components of the path. */
void util_makepath(const char* path);

/* Writes the basename of src to dst. */
void util_basename(char* dst, size_t dstlen, const char* src);

/* A wrapper for fseek with SEEK_SET which prints an error message upon error. */
int util_seek(FILE* stream, long offset);
/* Checks if a stream is seekable. */
int util_seekable(FILE* stream);
/* A wrapper for ftell which prints an error message upon error. */
long util_tell(FILE* stream);
/* A wrapper for fread which prints an error message upon error. */
int util_read(FILE* stream, void* buffer, size_t size);
/* A wrapper for fwrite which prints an error message upon error. */
int util_write(FILE* stream, const void* buffer, size_t size);
/* Returns the filesize of the passed file stream, or -1 and an error message
 * upon error. */
long util_fsize(FILE* stream);
/* Reads a stream until '\0'. */
ssize_t util_read_asciiz(char* buffer, size_t buffersize, FILE* stream);

/* Compares two strings.  Can be used by qsort. */
int util_strpcmp(const void* a, const void* b);

/* "Writes" a value to a buffer and returns a pointer to the memory location
 * after the written value. */
void* mempcpy(void* dest, const void* src, size_t n);

/* Performs simple encryption. */
void util_sillyxor(const unsigned char* in, unsigned char* out, int size, unsigned char key, unsigned char step, const unsigned char step2);

/* Converts a string from one character set to another. */
unsigned char* util_iconv(const char* to, const char* from, unsigned char* in, size_t insize, size_t* outsize);

#endif
