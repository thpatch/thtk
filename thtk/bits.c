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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thtk/thtk.h>
#include "bits.h"

void
bitstream_init(
    struct bitstream* b,
    thtk_io_t* stream)
{
    b->byte_count = 0;
    b->byte = 0;
    b->bits = 0;
    b->stream = stream;
}

uint32_t
bitstream_read(
    struct bitstream* b,
    unsigned int bits)
{
    if (bits > 25) {
        assert(bits <= 32);
        uint32_t r = bitstream_read(b, 24);
        bits -= 24;
        return r<<bits | bitstream_read(b, bits);
    }
    while (bits > b->bits) {
        unsigned char c = 0;
        thtk_io_read(b->stream, &c, 1, NULL);
        b->byte = b->byte<<8 | c;
        b->bits += 8;
        b->byte_count++;
    }
    b->bits -= bits;
    return b->byte>>b->bits & (1<<bits)-1;
}

void
bitstream_write1(
    struct bitstream* b,
    unsigned int bit)
{
    b->byte <<= 1;
    b->byte |= (bit & 1);
    b->bits++;

    if (b->bits == 8) {
        thtk_io_write(b->stream, &b->byte, 1, NULL);
        b->bits = 0;
        b->byte = 0;
        b->byte_count++;
    }
}

void
bitstream_write(
    struct bitstream* b,
    unsigned int bits,
    uint32_t data)
{
    int i;
    if (bits > 32)
        bits = 32;
    for (i = bits - 1; i >= 0; --i) {
        const unsigned int bit = (data >> i) & 1;
        bitstream_write1(b, bit);
    }
}

void
bitstream_finish(
    struct bitstream* b)
{
    while (b->bits)
        bitstream_write1(b, 0);
}
