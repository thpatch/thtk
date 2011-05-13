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
#ifndef IMAGE_H_
#define IMAGE_H_

#include <config.h>
#include <inttypes.h>
#include <stdio.h>

typedef enum {
    FORMAT_BGRA8888 = 1,
    FORMAT_BGR565   = 3,
    FORMAT_BGRA4444 = 5,
    FORMAT_RGBA8888 = 6, /* XXX: Also used internally. */
    FORMAT_GRAY8    = 7
} format_t;

unsigned int
format_Bpp(
    format_t format);

unsigned char*
format_from_rgba(
    const uint32_t* data,
    unsigned int pixels,
    format_t format);

char*
format_to_rgba(
    const char* data,
    unsigned int pixels,
    format_t format);

typedef struct {
    char* data;
    unsigned int width;
    unsigned int height;
    format_t format;
} image_t;

image_t*
png_read(
    FILE* stream,
    format_t format);

void
png_write(
    FILE* stream,
    image_t* image);

#endif
