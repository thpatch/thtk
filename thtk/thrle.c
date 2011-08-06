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
#include <thtk/thtk.h>
#include "thrle.h"

ssize_t
thtk_rle(
    thtk_io_t* input,
    size_t input_size,
    thtk_io_t* output,
    thtk_error_t** error)
{
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    unsigned char prevc = 0;
    unsigned char c = 0;
    unsigned int rl = 0;

    if (!input || !output) {
        thtk_error_new(error, "input or output is NULL");
        return -1;
    }

    while (bytes_read < input_size) {
        if (thtk_io_read(input, &c, 1, error) != 1)
            return -1;
        if (bytes_read == 0) {
            prevc = ~c;
        }
        ++bytes_read;
        if (rl) {
            if (c != prevc || rl == 0x100) {
                unsigned char rle[2] = { rl - 1, c };
                if (thtk_io_write(output, rle, 2, error) != 2)
                    return -1;
                bytes_written += 2;
                rl = 0;
            }
        } else {
            if (thtk_io_write(output, &c, 1, error) != 1)
                return -1;
            ++bytes_written;
        }

        if (c == prevc)
            ++rl;

        prevc = c;
    }

    if (rl) {
        unsigned char len = rl - 1;
        if (thtk_io_write(output, &len, 1, error) != 1)
            return -1;
        ++bytes_written;
    }

    return bytes_written;
}

ssize_t
thtk_unrle(
    thtk_io_t* input,
    size_t input_size,
    thtk_io_t* output,
    thtk_error_t** error)
{
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    unsigned int i;

    if (!input || !output) {
        thtk_error_new(error, "input or output is NULL");
        return -1;
    }

    if (input_size < 3) {
        for (i = 0; i < input_size; ++i) {
            unsigned char c;
            if (thtk_io_read(input, &c, 1, error) != 1)
                return -1;
            ++bytes_read;
            if (thtk_io_write(output, &c, 1, error) != 1)
                return -1;
            ++bytes_written;
        }
        return bytes_written;
    } else if (input_size >= 3) {
        unsigned char prev, cur;
        if (thtk_io_read(input, &prev, 1, error) != 1)
            return -1;
        ++bytes_read;
        if (thtk_io_write(output, &prev, 1, error) != 1)
            return -1;
        ++bytes_written;
        if (thtk_io_read(input, &cur, 1, error) != 1)
            return -1;
        ++bytes_read;
        if (thtk_io_write(output, &cur, 1, error) != 1)
            return -1;
        ++bytes_written;
        while (bytes_read < input_size) {
            if (prev == cur) {
                unsigned char count;
                if (thtk_io_read(input, &count, 1, error) != 1)
                    return -1;
                ++bytes_read;
                for (i = 0; i < count; ++i) {
                    if (thtk_io_write(output, &cur, 1, error) != 1)
                        return -1;
                    ++bytes_written;
                }

                if (bytes_read == input_size)
                    break;
            }

            prev = cur;
            if (thtk_io_read(input, &cur, 1, error) != 1)
                return -1;
            ++bytes_read;
            if (thtk_io_write(output, &cur, 1, error) != 1)
                return -1;
            ++bytes_written;
        }
    }

    return bytes_written;
}
