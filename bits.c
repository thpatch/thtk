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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "bits.h"
#include "program.h"

void
bitstream_init(struct bitstream* b)
{
    b->byte_count = 0;
    b->byte = 0;
    b->bits = 0;
}

void
bitstream_init_stream(struct bitstream* b, FILE* stream)
{
    bitstream_init(b);
    b->type = BITSTREAM_STREAM;
    b->io.stream = stream;
}

void
bitstream_init_fixed(struct bitstream* b, unsigned char* buffer, unsigned int size)
{
    bitstream_init(b);
    b->type = BITSTREAM_BUFFER_FIXED;
    b->io.buffer.buffer = buffer;
    b->io.buffer.size = size;
}

void
bitstream_init_growing(struct bitstream* b, unsigned int size)
{
    bitstream_init_fixed(b, malloc(size), size);
    b->type = BITSTREAM_BUFFER_GROW;
}

void
bitstream_free(struct bitstream* b)
{
    if (b->type == BITSTREAM_STREAM) {
        fclose(b->io.stream);
    } else if (b->type == BITSTREAM_BUFFER_FIXED || b->type == BITSTREAM_BUFFER_GROW) {
        free(b->io.buffer.buffer);
    } else {
        abort();
    }
}

unsigned int
bitstream_read1(struct bitstream* b)
{
    unsigned int ret = 0;

    if (!b->bits) {
        if (b->type == BITSTREAM_STREAM) {
            int c = fgetc(b->io.stream);
            if (c == EOF) {
                fprintf(stderr, "%s: unexpected end of file\n", argv0);
                abort();
            }
            b->byte = c;
        } else if (b->type == BITSTREAM_BUFFER_FIXED) {
            if (b->byte_count >= b->io.buffer.size) {
                fprintf(stderr, "%s: buffer overflow\n", argv0);
                abort();
            }
            b->byte = b->io.buffer.buffer[b->byte_count];
        } else {
            abort();
        }

        b->byte_count++;
        b->bits = 8;
    }

    ret = (b->byte & 0x80) >> 7;
    b->byte <<= 1;
    b->bits--;
    return ret & 1;
}

uint32_t
bitstream_read(struct bitstream* b, unsigned int bits)
{
    uint32_t ret = 0;
    for (; bits; --bits)
        ret |= bitstream_read1(b) << (bits - 1);
    return ret;
}

void
bitstream_write1(struct bitstream* b, unsigned int bit)
{
    b->byte <<= 1;
    b->byte |= (bit & 1);
    b->bits++;

    if (b->bits == 8) {
        if (b->type == BITSTREAM_STREAM) {
            if (fputc(b->byte, b->io.stream) == EOF) {
                fprintf(stderr, "%s: error while writing: %s\n", argv0, strerror(errno));
                abort();
            }
        } else if (b->type == BITSTREAM_BUFFER_FIXED) {
            if (b->byte_count >= b->io.buffer.size) {
                fprintf(stderr, "%s: buffer overflow\n", argv0);
                abort();
            }
            b->io.buffer.buffer[b->byte_count] = b->byte;
        } else if (b->type == BITSTREAM_BUFFER_GROW) {
            if (b->byte_count >= b->io.buffer.size) {
                b->io.buffer.size <<= 1;
                b->io.buffer.buffer = realloc(b->io.buffer.buffer, b->io.buffer.size);
            }
            b->io.buffer.buffer[b->byte_count] = b->byte;
        } else {
            abort();
        }

        b->bits = 0;
        b->byte = 0;
        b->byte_count++;
    }
}

void
bitstream_write(struct bitstream* b, unsigned int bits, uint32_t data)
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
bitstream_finish(struct bitstream* b)
{
    while (b->bits)
        bitstream_write1(b, 0);
}
