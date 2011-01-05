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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char*
buffer_add(
    unsigned char* buffer,
    unsigned int* buffer_pos,
    unsigned int* buffer_size,
    unsigned char b)
{
    if (*buffer_pos >= *buffer_size) {
        *buffer_size *= 2;
        buffer = realloc(buffer, *buffer_size);
    }
    buffer[(*buffer_pos)++] = b;
    return buffer;
}

unsigned char*
th_rle(
    const unsigned char* in,
    const unsigned int insize,
    unsigned int* const outsize)
{
    const unsigned char* orig_in = in;
    unsigned char* buffer = NULL;
    unsigned int buffer_size = 1024;
    int prevc = -1;
    int c = -1;
    int rl = 0;

    buffer = malloc(buffer_size);

    while (in - orig_in < (ptrdiff_t)insize) {
        c = *in++;
        if (rl) {
            if (c != prevc || rl == 0x100) {
                buffer = buffer_add(buffer, outsize, &buffer_size, rl - 1);
                rl = 0;
                buffer = buffer_add(buffer, outsize, &buffer_size, c);
            }
        } else {
            buffer = buffer_add(buffer, outsize, &buffer_size, c);
        }

        if (c == prevc)
            rl++;

        prevc = c;
    }

    if (rl)
        buffer = buffer_add(buffer, outsize, &buffer_size, rl - 1);

    return buffer;
}

void
th_unrle(
    const unsigned char* in,
    const unsigned int insize,
    FILE* stream)
{
    unsigned int i;
    if (insize < 3) {
        for (i = 0; i < insize; ++i)
            putc_unlocked(in[i], stream);
    } else if (insize >= 3) {
        const unsigned char* iend = in + insize;
        unsigned char prev, cur;
        prev = *in++;
        putc_unlocked(prev, stream);
        cur = *in++;
        putc_unlocked(cur, stream);
        while (in < iend) {
            if (prev == cur) {
                unsigned char count = *in++;
                for (i = 0; i < count; ++i)
                    putc_unlocked(cur, stream);

                if (in == iend)
                    break;
            }

            prev = cur;
            cur = *in++;
            putc_unlocked(cur, stream);
        }
    }
}
