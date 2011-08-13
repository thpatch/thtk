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
#include <errno.h>
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
    unsigned char data[];
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
    { 3, "ssm" },
    { 4, "S" },
    { 5, "ss" },
    { 6, "" },
    { 7, "S" },
    { 8, "ssm" },
    { 9, "S" },
    { 10, "" },
    { 11, "" },
    { 12, "" },
    { 13, "S" },
    { 14, "" },
    { 15, "SSSSS" },
    { 16, "m" },
    { 17, "SS" },
    { 18, "S" },
    { 19, "m" },
    { 20, "m" },
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
    { 16, "m" },
    { 17, "SS" },
    { 23, "S" },
    { 24, "" },
    { 25, "" },
    { 28, "S" },
    { 0, NULL }
};

static const id_format_pair_t th10_msg_ed_fmts[] = {
    { 0, "" },
    { 3, "m" },
    { 5, "S" },
    { 6, "S" },
    { 7, "Sz" },
    { 8, "SSS" },
    { 9, "S" },
    { 10, "z" },
    { 11, "" },
    { 12, "z" },
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
    { 16, "m" },
    { 17, "m" },
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
    { 17, "m" },
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
    { 17, "m" },
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
    { 17, "m" },
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
    { 17, "m" },
    { 18, "" },
    { 19, "" },
    { 20, "" },
    { 21, "" },
    { 22, "" },
    { 28, "ff" },
    { 29, "S" },
    { 30, "" },
    { 0, NULL }
};

static const id_format_pair_t th13_msg_fmts[] = {
    { 23, "" },
    { 24, "" },
    { 0, NULL }
};

static ssize_t
thmsg_value_to_data(
    const value_t* value,
    unsigned char* data,
    size_t data_length,
    unsigned int version)
{
    switch (value->type) {
    case 'm':
    case 'z': {
        ssize_t ret = value_to_data(value, data, data_length);

        size_t newlen = ret + 4 - (ret % 4);
        memset(data + ret, 0, newlen - ret);
        ret = newlen;

        if (value->type == 'm') {
            if (version == 8) {
                for (ssize_t i = 0; i < ret; ++i)
                    data[i] ^= 0x77;
            } else if (version >= 9) {
                util_xor(data, ret, 0x77, 7, 16);
            }
        }

        return ret;
    }
    default:
        return value_to_data(value, data, data_length);
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
        case 13: {
            const char* ret;
            ret = find_format(th13_msg_fmts, id);
            if (!ret) ret = find_format(th128_msg_fmts, id);
            return ret;
        }
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
    long file_size;
    uint16_t time = -1;
    int entry_new = 1;
    unsigned char* map;
    size_t entry_count;
    const int32_t* entry_offsets;
    const th06_msg_t* msg;

    file_size = file_fsize(in);
    if (file_size == -1)
        return 0;

    map = file_mmap(in, file_size);
    if (!map)
        return 0;

    entry_count = *((uint32_t*)map);
    entry_offsets = (int32_t*)(map + sizeof(uint32_t));
    msg = (th06_msg_t*)(map +
                        sizeof(uint32_t) +
                        entry_count * entry_offset_mul * sizeof(int32_t));

    for (;;) {
        const ptrdiff_t offset = (unsigned char*)msg - (unsigned char*)map;

        if (offset >= file_size)
            break;

        if (msg->time == 0 && msg->type == 0)
            entry_new = 1;

        if (entry_new && msg->type != 0) {
            size_t i;
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

        if (msg->time != time) {
            time = msg->time;
            fprintf(out, "@%u\n", time);
        }

        fprintf(out, "\t%d", msg->type);

        if (msg->length) {
            const char* format;
            value_t* values;
            int i;

            format = th06_find_format(version, (int)msg->type);
            if (!format) {
                fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, msg->type);
                file_munmap(map, file_size);
                return 0;
            }

            values = value_list_from_data(value_from_data, msg->data, msg->length, format);

            for (i = 0; values && values[i].type; ++i) {
                char* disp;
                if (values[i].type == 'm') {
                    if (version == 8) {
                        for (size_t j = 0; j < values[i].val.m.length; ++j)
                            values[i].val.m.data[j] ^= 0x77;
                    } else if (version >= 9) {
                        util_xor(values[i].val.m.data, values[i].val.m.length, 0x77, 7, 16);
                    }
                }
                disp = value_to_text(&values[i]);
                fprintf(out, ",%s", disp);
                value_free(&values[i]);
                free(disp);
            }

            free(values);
        }

        fprintf(out, "\n");

        msg = (th06_msg_t*)((unsigned char*)msg + sizeof(th06_msg_t) + msg->length);
    }

    file_munmap(map, file_size);

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
    entry_offsets = util_malloc(entry_count * sizeof(uint32_t) * entry_offset_mul);

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
            if (!format) {
                fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, msg.type);
                return 0;
            }

            for (i = 0; i < strlen(format); ++i) {
                ssize_t templength;
                unsigned char temp[1024] = { '\0' };
                value_t val;

                charp = strchr(charp, ',');
                if (!charp)
                    return 0;

                charp++;

                if (!value_from_text(charp, &val, format[i]))
                    return 0;

                if (padding_value.type) {
                    if (val.type == 'm' && version >= 12) {
                        val.val.m.data = realloc(val.val.m.data, val.val.m.length + 1 + padding_value.val.m.length);
                        val.val.m.data[val.val.m.length] = 0;
                        memcpy(val.val.m.data + val.val.m.length + 1, padding_value.val.m.data, padding_value.val.m.length);
                        val.val.m.length += padding_value.val.m.length;
                    }
                    value_free(&padding_value);
                    padding_value.type = 0;
                }

                templength = thmsg_value_to_data(&val, temp, 1024, version);

                if (version >= 9) {
                    if (val.type == 'm') {
                        if (val.val.m.data[0] == '|') {
                            padding_value.type = 'm';
                            padding_value.val.m.length = templength + 1;
                            padding_value.val.m.data = malloc(templength + 1);
                            memcpy(padding_value.val.m.data, temp, templength + 1);
                        }
                    }
                }

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
