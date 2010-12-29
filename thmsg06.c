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
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "program.h"
#include "thmsg.h"
#include "util.h"
#include "value.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t time;
    uint8_t type;
    uint8_t length;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th06_msg_t;

static const id_format_pair_t th06_msg_fmts[] = {
    { 0, "" },
    { 1, "ss" },
    { 2, "ss" },
    { 3, "ssz" },
    { 4, "S" },
    { 5, "ss" },
    { 6, "" },
    { 7, "S" },
    { 8, "ssz" },
    { 9, "S" },
    { 10, "" },
    { 11, "" },
    { 12, "" },
    { 13, "S" },
    { 14, "" },
    { 0, NULL }
};

static const id_format_pair_t th08_msg_fmts[] = {
    { 0, "" },
    { 1, "ss" },
    { 2, "ss" },
    { 3, "ssx" },
    { 4, "S" },
    { 5, "ss" },
    { 6, "" },
    { 7, "S" },
    { 8, "ssx" },
    { 9, "S" },
    { 10, "" },
    { 11, "" },
    { 12, "" },
    { 13, "S" },
    { 14, "" },
    { 15, "SSSSS" },
    { 16, "x" },
    { 17, "SS" },
    { 18, "S" },
    { 19, "x" },
    { 20, "x" },
    { 21, "S" },
    { 22, "" },
    { 0, NULL }
};

static const id_format_pair_t th09_msg_fmts[] = {
    { 0, "" },
    { 1, "ss" },
    { 4, "S" },
    { 5, "ss" },
    { 7, "S" },
    { 8, "" },
    { 9, "S" },
    { 11, "" },
    { 13, "S" },
    { 15, "SSS" },
    { 14, "" },
    { 16, "x" },
    { 17, "SS" },
    { 23, "S" },
    { 24, "" },
    { 25, "" },
    { 28, "S" },
    { 0, NULL }
};

static const id_format_pair_t th10_msg_ed_fmts[] = {
    { 0, "" },
    { 3, "x" },
    { 5, "S" },
    { 6, "S" },
    { 7, "Sz" },
    { 8, "SSS" },
    { 9, "S" },
    { 10, "z" },
    { 11, "" },
    { 12, "SSS" },
    { 14, "S" },
    { 15, "SSS" },
    { 16, "SSS" },
    { 17, "SSS" },
    { 0, NULL }
};

static const id_format_pair_t th10_msg_fmts[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "S" },
    { 3, "" },
    { 4, "" },
    { 5, "" },
    { 6, "" },
    { 7, "" },
    { 8, "" },
    { 9, "S" },
    { 10, "S" },
    { 11, "" },
    { 12, "S" },
    { 13, "S" },
    { 14, "S" },
    { 15, "SSS" },
    { 16, "x" },
    { 17, "x" },
    { 18, "" },
    { 19, "" },
    { 20, "" },
    { 21, "" },
    { 22, "" },
    { 23, "" },
    { 25, "S" },
    { 0, NULL }
};

static const id_format_pair_t th11_msg_fmts[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "S" },
    { 3, "" },
    { 4, "" },
    { 5, "" },
    { 6, "" },
    { 7, "" },
    { 8, "" },
    { 9, "" },
    { 10, "S" },
    { 11, "S" },
    { 12, "" },
    { 13, "S" },
    { 14, "S" },
    { 17, "x" },
    { 19, "" },
    { 20, "" },
    { 21, "" },
    { 22, "" },
    { 25, "S" },
    { 0, NULL }
};

static const id_format_pair_t th12_msg_fmts[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "S" },
    { 3, "" },
    { 4, "" },
    { 5, "" },
    { 6, "" },
    { 7, "" },
    { 8, "" },
    { 10, "S" },
    { 11, "S" },
    { 12, "" },
    { 13, "S" },
    { 14, "S" },
    { 17, "x" },
    { 19, "" },
    { 20, "" },
    { 21, "" },
    { 22, "" },
    { 27, "S" },
    { 0, NULL }
};

static const id_format_pair_t th125_msg_fmts[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "S" },
    { 3, "" },
    { 4, "" },
    { 5, "" },
    { 6, "" },
    { 7, "" },
    { 8, "" },
    { 10, "S" },
    { 11, "S" },
    { 12, "" },
    { 13, "S" },
    { 14, "S" },
    { 17, "x" },
    { 19, "" },
    { 20, "" },
    { 25, "S" },
    { 0, NULL }
};

static const id_format_pair_t th128_msg_fmts[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "S" },
    { 4, "" },
    { 5, "" },
    { 6, "" },
    { 7, "" },
    { 8, "" },
    { 10, "S" },
    { 11, "S" },
    { 12, "" },
    { 13, "S" },
    { 14, "S" },
    { 17, "x" },
    { 18, "" },
    { 19, "" },
    { 20, "" },
    { 21, "" },
    { 22, "" },
    { 28, "SS" },
    { 29, "S" },
    { 30, "" },
    { 0, NULL }
};

static void
filter_xor(unsigned char* data, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i)
        data[i] ^= 0x77;
}

static void
filter_sillyxor(unsigned char* data, size_t length)
{
    util_sillyxor(data, data, length, 0x77, 7, 16);
}

/* Pads the variable-length values to a multiple of four.  Padding is applied
 * even if the size is already a multiple of four. */
static void
value_pad(value_t* val)
{
    switch (val->type) {
    case 'z': {
        size_t zlen = strlen(val->val.z);
        size_t newlen = zlen + 4 - (zlen % 4);
        val->type = 'm';
        val->val.m.data = realloc(val->val.z, newlen);
        memset(val->val.m.data + newlen - (newlen - zlen), 0, newlen - zlen);
        val->val.m.length = newlen;
        break;
    }
    case 'm':
    case 'x': {
        size_t newlen = val->val.m.length + 4 - (val->val.m.length % 4);
        val->val.m.data = realloc(val->val.m.data, newlen);
        memset(val->val.m.data + newlen - (newlen - val->val.m.length), 0, newlen - val->val.m.length);
        val->val.m.length = newlen;
        break;
    }
    }
}

static const char*
th06_find_format(unsigned int version, int id)
{
    if (thmsg_opt_end) {
        switch (version) {
        case 10:
        case 11:
        case 12:
        case 128:
            return find_format(th10_msg_ed_fmts, id);
        default:
            fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, id);
            return NULL;
        }
    } else {
        switch (version) {
        case 6:
        case 7:
            return find_format(th06_msg_fmts, id);
        case 8:
            return find_format(th08_msg_fmts, id);
        case 9:
            return find_format(th09_msg_fmts, id);
        case 10:
            return find_format(th10_msg_fmts, id);
        case 11:
            return find_format(th11_msg_fmts, id);
        case 12:
            return find_format(th12_msg_fmts, id);
        case 125:
            return find_format(th125_msg_fmts, id);
        case 128:
            return find_format(th128_msg_fmts, id);
        default:
            fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, id);
            return NULL;
        }
    }
}

static int
th06_read(FILE* in, FILE* out, unsigned int version)
{
    const size_t entry_offset_mul = version >= 9 ? 2 : 1;
    const long file_size = file_fsize(in);
    uint32_t entry_count = 0;
    uint32_t* entry_offsets;
    uint16_t time = -1;
    int entry_new = 1;

    if (version != 6 && version != 7 && version != 8 && version != 9 && version != 10 && version != 11 && version != 12 && version != 125 && version != 128)
        return 0;

    if (!file_read(in, &entry_count, sizeof(uint32_t)))
        return 0;

    /* TODO: malloc wrapper. */
    entry_offsets = malloc(entry_count * sizeof(uint32_t) * entry_offset_mul);
    if (!entry_offsets) {
        fprintf(stderr, "%s: allocation of %lu bytes failed\n", argv0, entry_count * sizeof(uint32_t) * entry_offset_mul);
        return 0;
    }
    if (!file_read(in, entry_offsets, entry_count * sizeof(uint32_t) * entry_offset_mul))
        return 0;

    for (;;) {
        th06_msg_t msg;
        unsigned char data[256];
        long offset;
        int i;
        const char* format;
        value_t* values;

        offset = file_tell(in);
        if (offset >= file_size)
            break;

#if 0
        fprintf(out, "// %x\n", offset);
#endif

        if (!file_read(in, &msg, sizeof(th06_msg_t)))
            return 0;

        if (msg.time == 0 && msg.type == 0)
            entry_new = 1;

        if (entry_new && msg.type != 0) {
            unsigned int i;
            unsigned int entry_id = -1;
            entry_new = 0;
            time = -1;
            for (i = 0; i < entry_count; ++i) {
                if (offset == entry_offsets[i * entry_offset_mul]) {
                    entry_id = i;
                    fprintf(out, "entry %u", entry_id);
                    if (version >= 9)
                        fprintf(out, " (%u)", entry_offsets[1 + i * entry_offset_mul]);
                    fprintf(out, "\n");
                    break;
                }
            }
        }

        if (msg.time != time) {
            time = msg.time;
            fprintf(out, "@%u\n", time);
        }

        fprintf(out, "\t%d", msg.type);

        if (!file_read(in, data, msg.length))
            return 0;

        format = th06_find_format(version, (int)msg.type);
        if (!format)
            return 0;

        if (version >= 9)
            values = value_list_from_data(data, msg.length, format, filter_sillyxor);
        else if (version == 8)
            values = value_list_from_data(data, msg.length, format, filter_xor);
        else
            values = value_list_from_data(data, msg.length, format, NULL);

        for (i = 0; values && values[i].type; ++i) {
            char* disp;
            disp = value_to_text(&values[i]);
            fprintf(out, ",%s", disp);
            value_free(&values[i]);
            free(disp);
        }
        free(values);

        fprintf(out, "\n");
    }

    free(entry_offsets);

    return 1;
}

static int
th06_write(FILE* in, FILE* out, unsigned int version)
{
    const size_t entry_offset_mul = version >= 9 ? 2 : 1;
    uint32_t entry_count = 0;
    uint32_t* entry_offsets;
    unsigned int entry_num = 0;
    size_t i;
    char buffer[1024];
    th06_msg_t msg;
    char text[256];
    int msg_time, msg_type;
    value_t padding_value;
    padding_value.type = 0;

    if (version != 6 && version != 7 && version != 8 && version != 9 && version != 10 && version != 11 && version != 12 && version != 125 && version != 128)
        return 0;

    while (fgets(buffer, 1024, in)) {
        if (sscanf(buffer, " entry %u", &entry_num) == 1) {
            if (entry_num > entry_count)
                entry_count = entry_num;
        }
    }

    ++entry_count;

    if (!file_seek(in, 0))
        return 0;

    if (!file_seek(out, sizeof(uint32_t) + entry_count * sizeof(uint32_t) * entry_offset_mul))
        return 0;
    entry_offsets = malloc(entry_count * sizeof(uint32_t) * entry_offset_mul);
    if (!entry_offsets) {
        fprintf(stderr, "%s: allocation of %lu bytes failed\n", argv0, entry_count * sizeof(uint32_t) * entry_offset_mul);
        return 0;
    }

    if (version >= 9) {
        memset(entry_offsets, 0, entry_count * sizeof(uint32_t) * entry_offset_mul);
    } else {
        long offset = file_tell(out);
        for (i = 0; i < entry_count; ++i)
            entry_offsets[i] = offset;
    }

    /* Indicate the next parsed entry is the first. */
    msg.time = -1;

    while (fgets(buffer, 1024, in)) {
        memset(text, 0, 256);

        if (buffer[0]) {
            char* buffer_end = buffer + strlen(buffer) - 1;
            while (*buffer_end == '\n' || *buffer_end == '\r') {
                *buffer_end-- = '\0';
            }
        }

        if (sscanf(buffer, " entry %u", &entry_num) == 1) {
            msg.time = 0;
            entry_offsets[entry_num * entry_offset_mul] = file_tell(out);
            if (version >= 9) {
                unsigned int entry_flags = 0;
                if (sscanf(buffer, " entry %u (%u)", &entry_num, &entry_flags) != 2) {
                    fprintf(stderr, "%s:%s: no parse: %s\n", argv0, current_input, buffer);
                    return 0;
                }
                entry_offsets[1 + entry_num * entry_offset_mul] = entry_flags;
            }
        } else if (sscanf(buffer, " @%d", &msg_time) == 1) {
            msg.time = msg_time;
        } else if (sscanf(buffer, " %d", &msg_type) == 1) {
            long header_offset, end_offset;
            const char* format;
            const char* charp = buffer;

            msg.type = msg_type;
            msg.length = 0;

            header_offset = file_tell(out);
            file_seek(out, header_offset + sizeof(th06_msg_t));

            format = th06_find_format(version, (int)msg.type);
            if (!format)
                return 0;

            for (i = 0; i < strlen(format); ++i) {
                ssize_t templength;
                unsigned char temp[1024] = { '\0' };
                value_t val;

                charp = strchr(charp, ',');
                if (!charp)
                    return 0;

                charp++;

                if (!value_from_text(charp, &val, format[i], NULL))
                    return 0;

                if (padding_value.type) {
                    if (val.type == 'x' && version >= 12) {
                        val.val.m.data = realloc(val.val.m.data, val.val.m.length + 1 + padding_value.val.m.length);
                        val.val.m.data[val.val.m.length] = 0;
                        memcpy(val.val.m.data + val.val.m.length + 1, padding_value.val.m.data, padding_value.val.m.length);
                        val.val.m.length += padding_value.val.m.length;
                    }
                    value_free(&padding_value);
                    padding_value.type = 0;
                }

                value_pad(&val);
                if (version >= 9) {
                    templength = value_to_data(&val, temp, 1024, filter_sillyxor);
                    if (val.type == 'x') {
                        if (val.val.m.data[0] == '|') {
                            padding_value.type = 'm';
                            padding_value.val.m.length = templength + 1;
                            padding_value.val.m.data = malloc(templength + 1);
                            memcpy(padding_value.val.m.data, temp, templength + 1);
                        }
                    }
                } else if (version == 8)
                    templength = value_to_data(&val, temp, 1024, filter_xor);
                else
                    templength = value_to_data(&val, temp, 1024, NULL);
                msg.length += templength;
                if (!file_write(out, temp, templength))
                    return 0;

                value_free(&val);
            }

            end_offset = file_tell(out);

            file_seek(out, header_offset);
            if (!file_write(out, &msg, sizeof(th06_msg_t)))
                return 0;

            file_seek(out, end_offset);
        } else if (strncmp(buffer, "//", 2) == 0) {
            continue;
        } else {
            fprintf(stderr, "%s:%s: no parse: %s\n", argv0, current_input, buffer);
            return 0;
        }
    }

    if (!file_seek(out, 0))
        return 0;

    if (!file_write(out, &entry_count, sizeof(uint32_t)))
        return 0;

    if (!file_write(out, entry_offsets, entry_count * sizeof(uint32_t) * entry_offset_mul))
        return 0;

    free(entry_offsets);

    return 1;
}

const thmsg_module_t th06_msg = {
    th06_read,
    th06_write
};
