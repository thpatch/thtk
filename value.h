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
#ifndef VALUE_H_
#define VALUE_H_

#include <config.h>
#include <inttypes.h>

/* TODO: Apply filter to value_t instead? */
typedef void (*value_filter)(unsigned char* data, size_t length);

typedef struct {
    int id;
    const char* format;
} id_format_pair_t;

/* Returns the format string for the format, or NULL on error. */
const char* find_format(
    const id_format_pair_t* fmts,
    int id);

typedef struct {
    int type;
    union {
        float f;
        double d;
        unsigned char b;
        char c;
        uint16_t u;
        int16_t s;
        uint32_t U;
        int32_t S;
        char* z;
        struct {
            size_t length;
            unsigned char* data;
        } m;
    } val;
} value_t;

/* Reads a value_t from a block of data.  Returns the number of bytes read, or
 * -1 on error.  The passed value is filled in. */
ssize_t value_from_data(
    const unsigned char* data,
    size_t data_length,
    value_t* value,
    char type,
    value_filter filter);

/* Reads a list of values, the list is terminated by a value with type 0.  NULL
 * is returned on error. */
value_t* value_list_from_data(
    const unsigned char* data,
    size_t data_length,
    const char* data_format,
    value_filter filter);

/* Parses a value from a text string. */
int value_from_text(
    const char* text,
    value_t* value,
    char type,
    value_filter filter);

/* Returns a text representation of a value. */
char* value_to_text(
    const value_t* value);

/* Returns the size of the data written, or -1 on error. */
ssize_t value_to_data(
    const value_t* value,
    unsigned char* data,
    size_t data_size,
    value_filter filter);

/* Returns the size of the value's data form, or -1 on error. */
ssize_t value_size(
    const value_t* value);

/* Frees contained data. */
void value_free(
    value_t* value);

#endif
