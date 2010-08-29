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
#include <inttypes.h>
#include <string.h>

#include "bits.h"

/* Compression specification:
 *
 * The LZSS algorithm is used.  A single bit is used in front of every entry:
 * 1 means that a byte follows, and 0 that an (offset, length) follows.  The
 * size of the offset is 13 bits, and the length is 4 bits.  The minimum match
 * length is 3, allowing for a maximum match length of 18.  Offsets cannot
 * refer to entry 0 in the dictionary.
 *
 * The dictionary is 8192 bytes large, writing must start at index 1.  The last
 * 18 bytes (the dictionary pointer and in front of it) in the dictionary
 * cannot be read.  The dictionary is initialized to 0.
 *
 * Forward-looking matches are possible for repeating data.  A zero-offset,
 * zero-size entry is used to terminate the data.  This information is only
 * verified for TH10 and TH11. */

#define LZSS_DICTSIZE      0x2000
#define LZSS_DICTSIZE_MASK 0x1fff
#define LZSS_MIN_MATCH 3
/* LZSS_MIN_MATCH + 4 bits. */
#define LZSS_MAX_MATCH 18

/* Higher values seem to give both better speed and compression. */
#define HASH_SIZE 0x10000
#define HASH_NULL 0

/* This structure contains a hash for the dictionary and a special linked list
 * which is used for the entries in the hash. */
typedef struct {
    unsigned int hash[HASH_SIZE];
    unsigned int prev[LZSS_DICTSIZE];
    unsigned int next[LZSS_DICTSIZE];
} hash_t;

static inline unsigned int
generate_key(
    const unsigned char* array,
    const unsigned int base)
{
    return ((array[(base + 1) & LZSS_DICTSIZE_MASK] << 8) |
            array[(base + 2) & LZSS_DICTSIZE_MASK]) ^ (array[base] << 4);
}

static inline void
list_remove(
    hash_t* hash,
    const unsigned int key,
    const unsigned int offset)
{
    /* This function always removes the last entry in the list,
     * or no entry at all. */

    /* Set any previous entry's next pointer to HASH_NULL. */
    hash->next[hash->prev[offset]] = HASH_NULL;

    /* XXX: This condition is not neccessary, but it might
     * help optimization by not having to generate the key. */
    if (hash->prev[offset] == HASH_NULL)
        /* If the entry being removed was the head, clear the head. */
        if (hash->hash[key] == offset)
            hash->hash[key] = HASH_NULL;
}

static inline void
list_add(
    hash_t* hash,
    const unsigned int key,
    const unsigned int offset)
{
    hash->next[offset] = hash->hash[key];
    hash->prev[offset] = HASH_NULL;
    /* Update the previous pointer of the old head. */
    hash->prev[hash->hash[key]] = offset;
    hash->hash[key] = offset;
}

/* TODO: Make this read blocks for improved performance. */
typedef int (*read_byte_fptr)(void*);

static unsigned char*
th_lz(
    unsigned int* const outsize,
    read_byte_fptr read_byte,
    void* data)
{
    struct bitstream bs;
    hash_t hash;
    unsigned char dict[LZSS_DICTSIZE];
    unsigned int dict_head = 1;
    unsigned int dict_head_key;
    unsigned int waiting_bytes = 0;
    unsigned int i;
    int c;

    bitstream_init_growing(&bs, 4096);
    memset(&hash, 0, sizeof(hash));
    memset(dict, 0, sizeof(dict));

    /* Fill the forward-looking buffer. */
    for (i = 0; i < LZSS_MAX_MATCH; ++i) {
        c = read_byte(data);
        if (c == -1)
            break;
        dict[dict_head + i] = c;
        waiting_bytes++;
    }

    dict_head_key = generate_key(dict, dict_head);

    while (waiting_bytes) {
        unsigned int match_len = LZSS_MIN_MATCH - 1;
        unsigned int match_offset = 0;
        unsigned int offset;

        /* Find a good match. */
        for (offset = hash.hash[dict_head_key];
             offset != HASH_NULL;
             offset = hash.next[offset]) {
            unsigned int match_tmp = 0;
            for (i = 0; i < waiting_bytes; ++i) {
                if (dict[(dict_head + i) & LZSS_DICTSIZE_MASK] !=
                    dict[(offset + i) & LZSS_DICTSIZE_MASK])
                    break;
                match_tmp++;
            }
            if (match_tmp > match_len) {
                match_len = match_tmp;
                match_offset = offset;
                if (match_len == waiting_bytes)
                    break;
            }
        }

        /* Write data to the output buffer. */
        if (match_len < LZSS_MIN_MATCH) {
            match_len = 1;
            bitstream_write1(&bs, 1);
            bitstream_write(&bs, 8, dict[dict_head]);
        } else {
            bitstream_write1(&bs, 0);
            bitstream_write(&bs, 13, match_offset);
            bitstream_write(&bs, 4, match_len - LZSS_MIN_MATCH);
        }

        /* Add bytes to the dictionary. */
        for (i = 0; i < match_len; ++i) {
            const unsigned int offset =
                (dict_head + LZSS_MAX_MATCH) & LZSS_DICTSIZE_MASK;

            if (offset != HASH_NULL)
                list_remove(&hash, generate_key(dict, offset), offset);
            if (dict_head != HASH_NULL)
                list_add(&hash, dict_head_key, dict_head);

            c = read_byte(data);
            if (c != -1) {
                dict[offset] = c;
            } else {
                waiting_bytes--;
            }

            dict_head = (dict_head + 1) & LZSS_DICTSIZE_MASK;
            dict_head_key = generate_key(dict, dict_head);
        }
    }

    bitstream_write1(&bs, 0);
    bitstream_write(&bs, 13, HASH_NULL);
    bitstream_write(&bs, 4, 0);

    bitstream_finish(&bs);

    *outsize = bs.byte_count;
    return bs.io.buffer.buffer;
}

static int
read_byte_file(
    FILE* stream)
{
    int c = fgetc_unlocked(stream);

    if (c == EOF)
        return -1;
    else
        return c;
}

unsigned char*
th_lz_file(
    FILE* stream,
    unsigned int* const outsize)
{
    return th_lz(outsize, (read_byte_fptr)read_byte_file, stream);
}

static int
read_byte_mem(
    const unsigned char** ptrs)
{
    if (ptrs[0] >= ptrs[1])
        return -1;
    else
        return *ptrs[0]++;
}

unsigned char*
th_lz_mem(
    const unsigned char* in,
    const unsigned int insize,
    unsigned int* const outsize)
{
    unsigned char* ptrs[2];
    ptrs[0] = (unsigned char*)in;
    ptrs[1] = (unsigned char*)in + insize;
    return th_lz(outsize, (read_byte_fptr)read_byte_mem, &ptrs);
}

static void
th_unlz(
    struct bitstream* bs,
    unsigned char* out,
    const unsigned int outsize)
{
    unsigned char* oend = out + outsize;
    unsigned char dict[LZSS_DICTSIZE];
    unsigned int dict_head = 1;
    unsigned int i;

    memset(dict, 0, sizeof(dict));

    while (out < oend) {
        if (bitstream_read1(bs)) {
            char c = bitstream_read(bs, 8);
            *out++ = c;
            dict[dict_head] = c;
            dict_head = (dict_head + 1) & LZSS_DICTSIZE_MASK;
        } else {
            unsigned int match_offset = bitstream_read(bs, 13);
            unsigned int match_len = bitstream_read(bs, 4) + LZSS_MIN_MATCH;

            if (!match_offset)
                return;

            for (i = 0; i < match_len; ++i) {
                char c = dict[(match_offset + i) & LZSS_DICTSIZE_MASK];
                *out++ = c;
                dict[dict_head] = c;
                dict_head = (dict_head + 1) & LZSS_DICTSIZE_MASK;
            }
        }
    }
}

void
th_unlz_file(
    FILE* stream,
    unsigned char* const out,
    const unsigned int outsize)
{
    struct bitstream bs;
    bitstream_init_stream(&bs, stream);
    th_unlz(&bs, out, outsize);
}

void
th_unlz_mem(
    unsigned char* const in,
    const unsigned int insize,
    unsigned char* const out,
    const unsigned int outsize)
{
    struct bitstream bs;
    bitstream_init_fixed(&bs, in, insize);
    th_unlz(&bs, out, outsize);
}
