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
#include "args.h"
#include "program.h"
#include "util.h"
#include "value.h"

const char*
find_format(const id_format_pair_t* fmts, int id)
{
    int i;
    for (i = 0; fmts[i].format; ++i) {
        if (fmts[i].id == id)
            return fmts[i].format;
    }
    fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, id);
    return NULL;
}

ssize_t
value_from_data(
    const unsigned char* data,
    size_t data_length,
    value_t* value,
    char type,
    value_filter filter)
{
#define READ(x, n) \
    if (data_length < n) { \
        fprintf(stderr, "%s: unexpected end of data, wanted to read %zu bytes for format " #x "\n", argv0, n); \
        return -1; \
    } \
    memcpy(&value->val.x, data, n); \
    return n;

    value->type = type;

    switch (type) {
    case 'f':
        READ(f, sizeof(float));
    case 'd':
        READ(d, sizeof(double));
    case 'b':
        READ(b, sizeof(unsigned char));
    case 'c':
        READ(c, sizeof(char));
    case 'u':
        READ(u, sizeof(uint16_t));
    case 's':
        READ(s, sizeof(int16_t));
    case 'U':
        READ(U, sizeof(uint32_t));
    case 'S':
        READ(S, sizeof(int32_t));
    case 'z':
    case 'm':
    case 'x':
        if (type == 'z') {
            value->val.z = malloc(data_length);
            memcpy(value->val.z, data, data_length);
        } else {
            value->val.m.length = data_length;
            value->val.m.data = malloc(data_length);
            memcpy(value->val.m.data, data, data_length);
            if (type == 'x' && filter)
                filter(value->val.m.data, data_length);
        }
        return data_length;
    default:
        fprintf(stderr, "%s: invalid type %c\n", argv0, type);
        return -1;
    }

#undef READ
}

value_t*
value_list_from_data(
    const unsigned char* data,
    size_t data_length,
    const char* format,
    value_filter filter)
{
    size_t format_length = strlen(format);
    value_t* values = calloc(format_length + 1, sizeof(value_t));
    size_t i;

    for (i = 0; i < format_length; ++i) {
        ssize_t incr = value_from_data(data, data_length, &values[i], format[i], filter);
        if (incr == -1) {
            /* TODO: Free values properly. */
            return NULL;
        }

        data += incr;
        data_length -= incr;
    }

    if (data_length)
        fprintf(stderr, "%s: %zu bytes left over when parsing format \"%s\"\n", argv0, data_length, format);

    return values;
}

int
value_from_text(
    const char* text,
    value_t* value,
    char type,
    value_filter filter)
{
#define READ(f, x) \
    if (sscanf(text, "%" #f, &x) != 1) { \
        fprintf(stderr, "%s: couldn't parse '%c' from  \"%s\"\n", argv0, type, text); \
        return 0; \
    }

    unsigned int utmp;
    int stmp;

    value->type = type;

    switch (type) {
    case 'f':
        READ(f, value->val.f);
        break;
    case 'd':
        READ(lf, value->val.d);
        break;
    case 'b':
        READ(u, utmp);
        value->val.b = utmp;
        break;
    case 'c':
        READ(d, stmp);
        value->val.c = stmp;
        break;
    case 'u':
        READ(u, utmp);
        value->val.u = utmp;
        break;
    case 's':
        READ(d, stmp);
        value->val.s = stmp;
        break;
    case 'U':
        READ(u, value->val.U);
        break;
    case 'S':
        READ(d, value->val.S);
        break;
    case 'z':
        value->val.z = strdup(text);
        break;
    case 'm':
    case 'x': {
        size_t zlen = strlen(text);
        value->val.m.data = malloc(zlen);
        memcpy(value->val.m.data, text, zlen);
        value->val.m.length = zlen;
        if (type == 'x' && filter)
            filter(value->val.m.data, zlen);
        break;
    }
    default:
        fprintf(stderr, "%s: invalid type %c\n", argv0, type);
        return 0;
    }

    return 1;

#undef READ
}

/* TODO: Should write to a passed buffer and return the number of bytes written. */
char*
value_to_text(
    const value_t* value)
{
    char temp[256];

    switch (value->type) {
    case 'f':
        snprintf(temp, 256, "%sf", util_printfloat(&value->val.f));
        break;
    case 'd':
        snprintf(temp, 256, "%f", value->val.d);
        break;
    case 'b':
        snprintf(temp, 256, "%02x", value->val.b);
        break;
    case 'c':
        snprintf(temp, 256, "%c", value->val.c);
        break;
    case 'u':
        snprintf(temp, 256, "%u", value->val.u);
        break;
    case 's':
        snprintf(temp, 256, "%i", value->val.s);
        break;
    case 'U':
        snprintf(temp, 256, "%u", value->val.U);
        break;
    case 'S':
        snprintf(temp, 256, "%i", value->val.S);
        break;
    case 'z':
        snprintf(temp, 256, "%s", value->val.z);
        break;
    case 'm':
        memcpy(temp, value->val.m.data, value->val.m.length);
        temp[value->val.m.length] = '\0';
        break;
    case 'x':
        memcpy(temp, value->val.m.data, value->val.m.length);
        temp[value->val.m.length] = '\0';
        break;
    default:
        fprintf(stderr, "%s: invalid type %c\n", argv0, value->type);
        return NULL;
    }

    return strdup(temp);
}

ssize_t
value_to_data(
    const value_t* value,
    unsigned char* data,
    size_t data_size,
    value_filter filter)
{
#define WRITE(x, n) \
    if (data_size < n) { \
        fprintf(stderr, "%s: unexpected end of data, wanted to write %zu bytes for format " #x "\n", argv0, n); \
        return -1; \
    } \
    memcpy(data, &value->val.x, n); \
    return n;

    switch (value->type) {
    case 'f':
        WRITE(f, sizeof(float));
    case 'd':
        WRITE(d, sizeof(double));
    case 'b':
        WRITE(b, sizeof(unsigned char));
    case 'c':
        WRITE(c, sizeof(char));
    case 'u':
        WRITE(u, sizeof(uint16_t));
    case 's':
        WRITE(s, sizeof(int16_t));
    case 'U':
        WRITE(U, sizeof(uint32_t));
    case 'S':
        WRITE(S, sizeof(int32_t));
    case 'z': {
        size_t zlen = strlen(value->val.z) + 1;
        if (data_size < zlen) {
            fprintf(stderr, "%s: unexpected end of data, wanted to write %zu bytes for format z\n", argv0, zlen);
            return -1;
        }
        memcpy(data, value->val.z, zlen);
        return zlen;
    }
    case 'm':
    case 'x':
        if (data_size < value->val.m.length) {
            fprintf(stderr, "%s: unexpected end of data, wanted to write %zu bytes for format m\n", argv0, value->val.m.length);
            return -1;
        }
        memcpy(data, value->val.m.data, value->val.m.length);
        if (value->type == 'x' && filter)
            filter(data, value->val.m.length);
        return value->val.m.length;
    default:
        fprintf(stderr, "%s: invalid type %c\n", argv0, value->type);
        return -1;
    }

#undef WRITE
}

ssize_t
value_size(
    const value_t* value)
{
    switch (value->type) {
    case 'f':
        return sizeof(float);
    case 'd':
        return sizeof(double);
    case 'b':
        return sizeof(unsigned char);
    case 'c':
        return sizeof(char);
    case 'u':
        return sizeof(uint16_t);
    case 's':
        return sizeof(int16_t);
    case 'U':
        return sizeof(uint32_t);
    case 'S':
        return sizeof(int32_t);
    case 'z':
        return strlen(value->val.z);
    case 'm':
    case 'x':
        return value->val.m.length;
    default:
        fprintf(stderr, "%s: invalid type %c\n", argv0, value->type);
        return -1;
    }
}

void
value_free(
    value_t* value)
{
    if (value->type == 'z' && value->val.z) {
        free(value->val.z);
        value->val.z = NULL;
    } else if ((value->type == 'm' || value->type == 'x') && value->val.m.length && value->val.m.data) {
        free(value->val.m.data);
        value->val.m.data = NULL;
        value->val.m.length = 0;
    }
    value->type = 0;
}
