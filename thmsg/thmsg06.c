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
PACK_BEGIN
    uint16_t time;
    uint8_t type;
    uint8_t length;
PACK_END
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
    { 3, "ssm" },
    { 8, "ssm" },
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
    { 8, "" },
    { 15, "SSS" },
    { 23, "S" },
    { 24, "" },
    { 25, "" },
    { 28, "S" },
    { 0, NULL }
};

static const id_format_pair_t th10_msg_fmts[] = {
    { 1, "S" },
    { 2, "S" },
    { 3, "" },
    { 4, "" },
    { 5, "" },
    { 7, "" },
    { 10, "S" },
    { 12, "S" },
    { 14, "S" },
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
    { 9, "" },
    { 10, "S" },
    { 11, "S" },
    { 12, "" },
    { 0, NULL }
};

static const id_format_pair_t th12_msg_fmts[] = {
    { 27, "f" },
    { 0, NULL }
};

static const id_format_pair_t th128_msg_fmts[] = {
    { 28, "ff" },
    { 29, "S" },
    { 30, "" },
    { 0, NULL }
};

static const id_format_pair_t th13_msg_fmts[] = {
    { 31, "S" },
    { 0, NULL }
};

static const id_format_pair_t th14_msg_fmts[] = {
    { 5, "S" },
    { 8, "S" },
    { 14, "SS" },
    { 20, "S" },
    { 32, "S" },
    { 0, NULL }
};

static const id_format_pair_t th143_msg_fmts[] = {
    { 33, "S" },
    { 0, NULL }
};

static const id_format_pair_t th16_msg_fmts[] = {
    { 19, "S" },
    { 34, "SS" },
    { 35, "" },
    { 0, NULL }
};

static const id_format_pair_t th18_msg_fmts[] = {
    { 4, "S" },
    { 7, "S" },
    { 13, "SS" },
    { 36, "" },
    { 0, NULL }
};

static const id_format_pair_t th185_msg_fmts[] = {
    { 37, "" },
    { 38, "" },
    { 39, "" },
    { 0, NULL }
};

static const id_format_pair_t th19_msg_fmts[] = {
    { 42, "S" },
    { 43, "S" },
    { 44, "ff" },
    { 45, "ff" },
    { 46, "SS" },
    { 47, "SS" },
    { 50, "S" },
    { 51, "S" },
    { 52, "" },
    { 53, "" },
    { 54, "" },
    { 55, "" },
    { 56, "" },
    { 0, NULL }
};

/* NEWHU: 19 */

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
    const char* ret = NULL;

    if (thmsg_opt_end) {
        switch (version) {
        case 10:
        case 11:
        case 12:
        case 128:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 185:
        case 19:
        /* NEWHU: 19 */
            ret = find_format(th10_msg_ed_fmts, id);
            break;
        default:
            fprintf(stderr, "%s: unsupported version: %u\n", argv0, version);
            return NULL;
        }
    } else {
        switch (version) {
        /* NEWHU: 19 */
        case 19:
            if ((ret = find_format(th19_msg_fmts, id))) break; /* fallthrough */
        case 185:
            if ((ret = find_format(th185_msg_fmts, id))) break; /* fallthrough */
        case 18:
            if ((ret = find_format(th18_msg_fmts, id))) break; /* fallthrough */
        case 17:
        case 165:
        case 16:
            if ((ret = find_format(th16_msg_fmts, id))) break; /* fallthrough */
        case 15:
        case 143:
            if ((ret = find_format(th143_msg_fmts, id))) break; /* fallthrough */
        case 14:
            if ((ret = find_format(th14_msg_fmts, id))) break; /* fallthrough */
        case 13:
            if ((ret = find_format(th13_msg_fmts, id))) break; /* fallthrough */
        case 128:
            if ((ret = find_format(th128_msg_fmts, id))) break; /* fallthrough */
        case 125:
        case 12:
            if ((ret = find_format(th12_msg_fmts, id))) break; /* fallthrough */
        case 11:
            if ((ret = find_format(th11_msg_fmts, id))) break; /* fallthrough */
        case 10:
            if ((ret = find_format(th10_msg_fmts, id))) break; /* fallthrough */
        case 9:
            if ((ret = find_format(th09_msg_fmts, id))) break; /* fallthrough */
        case 8:
            if ((ret = find_format(th08_msg_fmts, id))) break; /* fallthrough */
        case 7:
        case 6:
            ret = find_format(th06_msg_fmts, id);
            break;
        default:
            fprintf(stderr, "%s: unsupported version: %u\n", argv0, version);
            return NULL;
        }
    }

    if (!ret)
        fprintf(stderr, "%s: id %d was not found in the main format table\n", argv0, id);

    return ret;
}

static int
th06_read(FILE* in, FILE* out, unsigned int version)
{
    const size_t entry_offset_mul = version >= 9 ? 2 : 1;
    const size_t header_extra = version == 19 ? 0x50 : 0;
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
    entry_offsets = (int32_t*)(map + sizeof(uint32_t) + header_extra);
    msg = (th06_msg_t*)(map +
                        sizeof(uint32_t) + header_extra +
                        entry_count * entry_offset_mul * sizeof(int32_t));

    if (header_extra) {
        int i;
        const int32_t *extra_values = (int32_t*)(map + sizeof(uint32_t));
        fprintf(out, "header(");
        for (i = 0; i < header_extra/4; i++) {
            fprintf(out, "%s%d", i==0 ? "" : ", ", extra_values[i]);
        }
        fprintf(out, ")\n");
    }

    for (;;) {
        const ptrdiff_t offset = (unsigned char*)msg - (unsigned char*)map;
        const char* format;

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
                    if (version == 19) {
                        uint32_t w = entry_offsets[1 + i * entry_offset_mul];
                        fprintf(out, " (%u, %u, %u, %u)", w&0xff, w>>8 & 0xff, w>>16 & 0xff, w>>24 & 0xff);
                    } else if (version >= 9) {
                        fprintf(out, " (%u)", entry_offsets[1 + i * entry_offset_mul]);
                    }
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

        /* Must be here for the missing format error to be properly displayed,
         * even if msg->length is 0. */
        format = th06_find_format(version, (int)msg->type);
        if (!format) {
            fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, msg->type);
            file_munmap(map, file_size);
            return 0;
        }

        if (msg->length) {
            value_t* values;
            int i;

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
                fprintf(out, ";%s", disp);
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
    int32_t extra_values[20] = {0};
    char lookahead;
    value_t padding_value;
    padding_value.type = 0;

    while (fgets(buffer, 1024, in)) {
        int pos = 0;
        if (sscanf(buffer, " header %c%n", &lookahead, &pos) == 1 && lookahead == '(') {
            int i;
            char *p = buffer+pos;
            for (i = 0; i < sizeof(extra_values)/sizeof(extra_values[0]); i++) {
                if (sscanf(p, "%d %c%n", &extra_values[i], &lookahead, &pos) != 2) {
                    fprintf(stderr, "%s:%s: no parse: %s\n", argv0, current_input, buffer);
                    return 0;
                }
                if (lookahead == ')')
                    break;
                if (lookahead != ',') {
                    fprintf(stderr, "%s:%s: no parse: %s\n", argv0, current_input, buffer);
                    return 0;
                }
                p += pos;
            }
        }
        if (sscanf(buffer, " entry %u", &entry_num) == 1) {
            if (entry_num > entry_count)
                entry_count = entry_num;
        }
    }

    ++entry_count;

    if (!file_seek(in, 0))
        return 0;

    if (!file_seek(out, sizeof(uint32_t) + (version == 19)*sizeof(extra_values) + entry_count * sizeof(uint32_t) * entry_offset_mul))
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

        if (sscanf(buffer, " header %c", &lookahead) == 1) {
            /* do nothing */
        } else if (sscanf(buffer, " entry %u", &entry_num) == 1) {
            msg.time = 0;
            entry_offsets[entry_num * entry_offset_mul] = file_tell(out);
            if (version >= 9) {
                unsigned int entry_flags = 0;
                if (version == 19) {
                    unsigned int flags[4];
                    if (sscanf(buffer, " entry %u (%u, %u, %u, %u)", &entry_num,
                            &flags[0], &flags[1], &flags[2], &flags[3]) != 5
                            || flags[0] > 255 || flags[1] > 255 || flags[2] > 255 || flags[3] > 255) {
                        fprintf(stderr, "%s:%s: no parse: %s\n", argv0, current_input, buffer);
                        return 0;
                    }
                    entry_flags = flags[0] | flags[1]<<8 | flags[2]<<16 | flags[3]<<24;
                } else {
                    if (sscanf(buffer, " entry %u (%u)", &entry_num, &entry_flags) != 2) {
                        fprintf(stderr, "%s:%s: no parse: %s\n", argv0, current_input, buffer);
                        return 0;
                    }
                }
                entry_offsets[1 + entry_num * entry_offset_mul] = entry_flags;
            }
        } else if (sscanf(buffer, " @%d", &msg_time) == 1) {
            msg.time = msg_time;
        } else if (sscanf(buffer, " %d", &msg_type) == 1) {
            long header_offset, end_offset;
            const char* format;
            const char* charp = buffer;
            size_t param_count;

            msg.type = msg_type;
            msg.length = 0;

            header_offset = file_tell(out);
            file_seek(out, header_offset + sizeof(th06_msg_t));

            format = th06_find_format(version, (int)msg.type);
            if (!format) {
                fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, msg.type);
                return 0;
            }

            param_count = strlen(format);
            for (i = 0; i < param_count; ++i) {
                ssize_t templength;
                unsigned char temp[1024] = { '\0' };
                value_t val;
                const char *charp_prev = charp;

                charp = strchr(charp, ';');
                if (!charp) {
                    size_t missing = param_count - i;

                    fprintf(stderr, "%s: entry %u, @%u, id %d: expected %u more %s, separated with ;\n",
                        argv0, entry_num, msg.time, msg.type,
                        (unsigned int)missing, missing == 1 ? "parameter" : "parameters");

                    if(i == 0) {
                        while ( (charp_prev = strchr(charp_prev, ',')) != NULL) {
                            charp_prev++;
                            i++;
                        }
                        if (i == param_count) {
                            fprintf(stderr, "%s: entry %u, @%u, id %d: found %u %s instead, this is an old msg dump\n",
                                argv0, entry_num, msg.time, msg.type,
                                (unsigned int)param_count, param_count == 1 ? "comma" : "commas");
                        }
                    }
                    return 0;
                }

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
        } else if (util_strcmp_ref(buffer, stringref("//")) == 0) {
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

    if (version == 19 && !file_write(out, &extra_values, sizeof(extra_values)))
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
