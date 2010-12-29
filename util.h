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

/* Allocates memory and aborts with an error message if the allocation failed. */
void* util_malloc(
    size_t size);

void util_print_version(
    const char* name,
    const char* version);

/* Returns an unique string representation of a float.  Returns a pointer to a
 * static buffer, not thread-safe. */
const char* util_printfloat(
    const void* data);

/* Creates all components of the path. */
void util_makepath(
    const char* path);

/* Writes the basename of src to dst. */
void util_basename(
    char* dst,
    size_t dstlen,
    const char* src);

/* Compares two strings.  Can be used by qsort. */
int util_strpcmp(
    const void* a,
    const void* b);

/* "Writes" a value to a buffer and returns a pointer to the memory location
 * after the written value. */
void* mempcpy(
    void* dest,
    const void* src,
    size_t n);

/* Performs simple encryption. */
void util_sillyxor(
    const unsigned char* in,
    unsigned char* out,
    int size,
    unsigned char key,
    unsigned char step,
    const unsigned char step2);

/* Converts a string from one character set to another. */
unsigned char* util_iconv(
    const char* to,
    const char* from,
    unsigned char* in,
    size_t insize,
    size_t* outsize);

#endif
