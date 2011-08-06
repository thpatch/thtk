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
#include <thtk/thtk.h>

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

ssize_t
th_lzss(
    thtk_io_t* input,
    size_t input_size,
    thtk_io_t* output,
    thtk_error_t** error)
{
    struct bitstream bs;
    hash_t hash;
    unsigned char dict[LZSS_DICTSIZE];
    unsigned int dict_head = 1;
    unsigned int dict_head_key;
    unsigned int waiting_bytes = 0;
    size_t bytes_read = 0;
    unsigned int i;
    unsigned char c;

    if (!input || !output) {
        thtk_error_new(error, "input or output is NULL");
        return -1;
    }

    bitstream_init(&bs, output);
    memset(&hash, 0, sizeof(hash));
    memset(dict, 0, sizeof(dict));

    /* Fill the forward-looking buffer. */
    for (i = 0; i < LZSS_MAX_MATCH && i < input_size; ++i) {
        int ret = thtk_io_read(input, &c, 1, error);
        if (ret == -1) {
            return -1;
        } else if (ret != 1) {
            break;
        }
        ++bytes_read;
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
             offset != HASH_NULL && waiting_bytes > match_len;
             offset = hash.next[offset]) {
            /* First check a character further ahead to see if this match can
             * be any longer than the current match. */
            if (dict[(dict_head + match_len) & LZSS_DICTSIZE_MASK] ==
                dict[(offset + match_len) & LZSS_DICTSIZE_MASK]) {
                /* Then check the previous characters. */
                for (i = 0;
                     i < match_len &&
                     (dict[(dict_head + i) & LZSS_DICTSIZE_MASK] ==
                      dict[(offset + i) & LZSS_DICTSIZE_MASK]);
                     ++i)
                    ;

                if (i < match_len)
                    continue;

                /* Finally try to extend the match. */
                for (++match_len;
                     match_len < waiting_bytes &&
                     (dict[(dict_head + match_len) & LZSS_DICTSIZE_MASK] ==
                      dict[(offset + match_len) & LZSS_DICTSIZE_MASK]);
                     ++match_len)
                    ;

                match_offset = offset;
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

            if (bytes_read < input_size) {
                int ret = thtk_io_read(input, &c, 1, error);
                if (ret == 1) {
                    dict[offset] = c;
                    ++bytes_read;
                } else if (ret == 0) {
                    --waiting_bytes;
                } else {
                    return -1;
                }
            } else {
                --waiting_bytes;
            }

            dict_head = (dict_head + 1) & LZSS_DICTSIZE_MASK;
            dict_head_key = generate_key(dict, dict_head);
        }
    }

    bitstream_write1(&bs, 0);
    bitstream_write(&bs, 13, HASH_NULL);
    bitstream_write(&bs, 4, 0);

    bitstream_finish(&bs);

    return bs.byte_count;
}

ssize_t
th_unlzss(
    thtk_io_t* input,
    thtk_io_t* output,
    size_t output_size,
    thtk_error_t** error)
{
    unsigned char dict[LZSS_DICTSIZE];
    unsigned int dict_head = 1;
    unsigned int i;
    size_t bytes_written = 0;
    struct bitstream bs;

    if (!input || !output) {
        thtk_error_new(error, "input or output is NULL");
        return -1;
    }

    bitstream_init(&bs, input);

    memset(dict, 0, sizeof(dict));

    while (bytes_written < output_size) {
        if (bitstream_read1(&bs)) {
            unsigned char c = bitstream_read(&bs, 8);
            if (thtk_io_write(output, &c, 1, error) != 1)
                return -1;
            ++bytes_written;
            dict[dict_head] = c;
            dict_head = (dict_head + 1) & LZSS_DICTSIZE_MASK;
        } else {
            unsigned int match_offset = bitstream_read(&bs, 13);
            unsigned int match_len = bitstream_read(&bs, 4) + LZSS_MIN_MATCH;

            if (!match_offset)
                return bytes_written;

            for (i = 0; i < match_len; ++i) {
                unsigned char c = dict[(match_offset + i) & LZSS_DICTSIZE_MASK];
                if (thtk_io_write(output, &c, 1, error) != 1)
                    return -1;
                ++bytes_written;
                dict[dict_head] = c;
                dict_head = (dict_head + 1) & LZSS_DICTSIZE_MASK;
            }
        }
    }

    return bytes_written;
}
