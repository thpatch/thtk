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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "thcrypt.h"

int
th_encrypt(unsigned char* data, unsigned int size, unsigned char key, const unsigned char step, unsigned int block, unsigned int limit)
{
    const unsigned char* end;
    unsigned char* ip;
    unsigned char* mblock;
    unsigned char* temp;
    unsigned int increment = (block >> 1) + (block & 1);

    mblock = malloc(block);

    if (size < block >> 2)
        size = 0;
    else
        size -= (size % block < block >> 2) * size % block + size % 2;

    if (limit % block != 0)
        limit = limit + (block - (limit % block));

    end = data + (size < limit ? size : limit);

    while (data < end) {
        if (end - data < block) {
            block = end - data;
            increment = (block >> 1) + (block & 1);
        }

        temp = mblock;

        for (ip = data + block - 1; ip > data; ip -= 2) {
            *temp = *ip ^ key;
            *(temp + increment) = *(ip - 1) ^ (key + step * increment);
            temp++;
            key += step;
        }

        if (block & 1) {
            *temp = *ip ^ key;
            key += step;
        }
        key += step * increment;

        memcpy(data, mblock, block);
        data += block;
    }

    free(mblock);

    return 0;
}
