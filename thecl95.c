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
#include "expr.h"
#include "file.h"
#include "program.h"
#include "thecl.h"
#include "util.h"
#include "value.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint32_t magic; /* 0x00000800 */
    uint16_t sub_count;
    uint16_t unknown; /* 0x0001 */
    uint32_t size2; /* filesize - sizeof(th95_header_t) */
    uint32_t filesize;
    uint32_t zero[14];
    uint32_t sub_offsets[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th95_header_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    int32_t time; /* 0xffffffff (-1) for the last entry. */
    int16_t id; /* 0xffff (-1) for the last entry. */
    uint16_t size; /* sizeof(data) + sizeof(th95_instr_t) */
    uint16_t unknown1; /* 0xff00. */
    uint8_t param_mask; /* 0xff (-1?) for the last entry. */
    uint8_t unknown2; /* 0x18 for instruction 0x9b, otherwise 0x00. */
    unsigned char data[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th95_instr_t;

static ssize_t
th95_value_from_data(
    const unsigned char* data,
    size_t data_length,
    char type,
    value_t* value)
{
    if (type == 'x' || type == 'X') {
        int ret;
        ret = value_from_data(data, data_length, 'm', value);
        util_xor(value->val.m.data, ret, 0xaa, 0, 0);
        return ret;
    } else
        return value_from_data(data, data_length, type, value);
}

static char*
th95_param_to_text(
    const thecl_param_t* param)
{
    if (param->type == 'x' || param->type == 'X') {
        char* ret = malloc(1024);
        char* temp = ret;
        *temp++ = 'C';
        *temp++ = '"';
        for (size_t i = 0; i < param->value.val.m.length; ++i) {
            if (!param->value.val.m.data[i])
                break;
            if (param->value.val.m.data[i] == '"')
                *temp++ = '\\';
            *temp++ = param->value.val.m.data[i];
        }
        *temp++ = '"';
        *temp++ = '\0';
        return ret;
    } else
        return value_to_text(&param->value);
}

static const id_format_pair_t th95_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 2, "S" },
    { 4, "SS" },
    { 5, "SSS" },
    { 6, "SS" },
    { 7, "ff" },
    { 10, "SS" },
    { 11, "SS" },
    { 14, "SS" },
    { 15, "ff" },
    { 16, "ff" },
    { 17, "ff" },
    { 18, "ff" },
    { 20, "SSS" },
    { 21, "SSS" },
    { 22, "SSS" },
    { 23, "SSS" },
    { 24, "SSS" },
    { 25, "fff" },
    { 26, "fff" },
    { 27, "fff" },
    { 28, "fff" },
    { 30, "S" },
    { 36, "fSSSffff" },
    { 37, "f" },
    { 38, "ffff" },
    { 40, "SSSS" },
    { 42, "SfSS" },
    { 43, "fffS" },
    { 44, "SSSS" },
    { 45, "ffSS" },
    { 47, "ffSS" },
    { 48, "SSSS" },
    { 49, "fffS" },
    { 50, "SSSS" },
    { 51, "ffSS" },
    { 52, "S" },
    { 53, "" },
    { 54, "S" },
    { 55, "S" },
    { 56, "SSSSSS" },
    { 62, "" },
    { 63, "ff" },
    { 64, "SSff" },
    { 65, "ff" },
    { 66, "SSff" },
    { 67, "SSf" },
    { 70, "f" },
    { 71, "f" },
    { 75, "ffff" },
    { 76, "" },
    { 77, "ff" },
    { 80, "S" },
    { 81, "S" },
    { 83, "S" },
    { 84, "Sff" },
    { 85, "" },
    /* Should be ss instead of S. */
    { 86, "SSSffffS" },
    { 87, "SSSffffS" },
    { 88, "SSSffffS" },
    { 89, "SSSffffS" },
    { 90, "SSSffffS" },
    { 91, "SSSffffS" },
    { 94, "SSSffffS" },
    { 100, "ff" },
    { 101, "SSSSSff" },
    { 102, "" },
    { 103, "SS" },
    { 104, "x" },
    { 105, "" },
    { 106, "S" },
    { 108, "SS" },
    { 109, "S" },
    { 112, "S" },
    { 114, "S" },
    { 115, "SSS" },
    { 117, "SS" },
    { 118, "S" },
    { 119, "S" },
    { 124, "S" },
    { 132, "S" },
    { 141, "S" },
    { 142, "" },
    { 143, "SS" },
    { 144, "S" },
    { 145, "SSffff" },
    { 146, "SSffff" },
    { 147, "SSfffSSSSfS" },
    { 149, "f" },
    { 150, "S" },
    { 151, "SS" },
    { 152, "fS" },
    { 153, "SSfffSSSSfS" },
    { 155, "SSfffSSSSffff" },
    { 157, "SSfffSSSSfSff" }
};

static thecl_t*
th95_open(
    FILE* stream,
    unsigned int __attribute__((unused)) version)
{
    long file_size;
    unsigned char* map;

    const th95_header_t* header;

    file_size = file_fsize(stream);
    if (file_size == -1)
        return NULL;

    map = file_mmap(stream, file_size);
    if (!map)
        return NULL;

    header = (th95_header_t*)map;
    /* TODO: Verify header. */

    thecl_t* ecl = thecl_new();
    ecl->sub_count = header->sub_count;

    unsigned char* footer_pointer = NULL;

    for (size_t s = 0; s < header->sub_count; ++s) {
        char name[256];
        thecl_sub_t* sub = malloc(sizeof(thecl_sub_t));
        sprintf(name, "Sub%zu", s + 1);
        sub->name = strdup(name);
        list_init(&sub->instrs);
        sub->stack = 0;
        sub->arity = 0;
        sub->var_count = 0;
        sub->vars = NULL;
        sub->offset = 0;
        list_init(&sub->labels);
        int time = 0;

        const th95_instr_t* raw_instr = (th95_instr_t*)(map + header->sub_offsets[s]);
        while (raw_instr->time != -1) {
            if (raw_instr->time != time) {
                thecl_instr_t* new = thecl_instr_time(raw_instr->time);
                list_append_new(&sub->instrs, new);
                time = raw_instr->time;
            }

            thecl_instr_t* instr = thecl_instr_new();
            instr->id = raw_instr->id;

            if (raw_instr->size > sizeof(th95_instr_t)) {
                uint32_t param_mask = raw_instr->param_mask;
                const char* format = find_format(th95_fmts, raw_instr->id);
                value_t* values = value_list_from_data(th95_value_from_data, raw_instr->data, raw_instr->size - sizeof(th95_instr_t), format);
                value_t* value_iter = values;

                size_t p = 0;
                while (value_iter->type) {
                    thecl_param_t* param = param_new(format[p++]);
                    param->stack = param_mask & 1;
                    param_mask >>= 1;
                    param->value = *value_iter;
                    ++value_iter;

                    list_append_new(&instr->params, param);
                }
                free(values);
            }

            list_append_new(&sub->instrs, instr);

            raw_instr = (th95_instr_t*)((char*)raw_instr + raw_instr->size);
            footer_pointer = (unsigned char*)raw_instr + raw_instr->size;
        }

        list_append_new(&ecl->subs, sub);
    }


    thecl_local_data_t* local_data = malloc(sizeof(thecl_local_data_t) + (file_size - (footer_pointer - map)));
    strcpy(local_data->name, "th95_footer");
    local_data->data_length = file_size - (footer_pointer - map);
    memcpy(local_data->data, footer_pointer, local_data->data_length);
    list_append_new(&ecl->local_data, local_data);

    file_munmap(map, file_size);

    return ecl;
}

static void
th95_trans(
    thecl_t* ecl)
{
    if (!ecl)
        return;
}

static void
th95_dump(
    const thecl_t* ecl,
    FILE* out)
{
    if (!ecl || !out)
        return;

    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {
        fprintf(out, "\nsub %s()\n{\n", sub->name);

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr) {
            switch (instr->type) {
            case THECL_INSTR_TIME:
                fprintf(out, "%u:\n", instr->time);
                break;
            case THECL_INSTR_INSTR: {
                fprintf(out, "    ins_%u(", instr->id);
                thecl_param_t* param;
                int first = 1;
                list_for_each(&instr->params, param) {
                    if (!first) {
                        fprintf(out, ", ");
                    } else {
                        first = 0;
                    }
                    char* ret = th95_param_to_text(param);
                    fprintf(out, "%s%s%s",
                        param->stack ? "[" : "",
                        ret,
                        param->stack ? "]" : "");
                    free(ret);
                }
                fprintf(out, ");\n");
                break;
            }
            default:
                abort();
            }
        }

        fprintf(out, "}\n");
    }

    thecl_local_data_t* local_data;
    list_for_each(&ecl->local_data, local_data) {
        fprintf(out, "\nlocal %s {\n", local_data->name);
        for (size_t b = 0; b < local_data->data_length; ++b) {
            fprintf(out, "    0x%02x\n", local_data->data[b]);
        }
        fprintf(out, "}\n");
    }
}

static size_t
th95_instr_size(
    const thecl_t __attribute__((unused))* ecl,
    const thecl_instr_t* instr)
{
    size_t ret = sizeof(th95_instr_t);
    thecl_param_t* param;

    list_for_each(&instr->params, param) {
        if (param->type == 'x' || param->type == 'X')
            ret += 0x30;
        else
            ret += value_size(&param->value);
    }

    return ret;
}

static thecl_t*
th95_parse(
    FILE* in,
    unsigned int version)
{
    parser_state_t state;

    state.instr_time = 0;
    state.instr_rank = 0xff;
    state.version = version;
    list_init(&state.expressions);
    state.current_sub = NULL;
    state.ecl = thecl_new();
    state.instr_size = th95_instr_size;

    yyin = in;

    if (yyparse(&state) != 0)
        return 0;

    return state.ecl;
}

static int
th95_compile(
    const thecl_t* ecl,
    FILE* out)
{
    long file_size;
    th95_header_t header = {
        0x00000800,
        ecl->sub_count,
        0x0001,
        0,
        0,
        { 0 }
    };

    if (!file_seekable(out)) {
        fprintf(stderr, "%s: output is not seekable\n", argv0);
        return 0;
    }

    file_seek(out, sizeof(th95_header_t) + ecl->sub_count * sizeof(uint32_t));

    thecl_sub_t* sub;
    uint32_t* offsets = malloc(ecl->sub_count * sizeof(uint32_t));
    size_t s = 0;
    list_for_each(&ecl->subs, sub) {
        offsets[s] = file_tell(out);

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr) {
            th95_instr_t raw_instr = {
                instr->time, instr->id, th95_instr_size(ecl, instr), 0xff00, 0, instr->id == 0x9b ? 0x18 : 0x00 };

            thecl_param_t* param;
            size_t p = 0;
            list_for_each(&instr->params, param) {
                if (param->stack)
                    raw_instr.param_mask |= 1 << p;
                ++p;
            }

            file_write(out, &raw_instr, sizeof(th95_instr_t));

            list_for_each(&instr->params, param) {
                unsigned char temp[512];
                uint32_t ret;

                if (param->type == 'X' || param->type == 'x') {
                    memset(temp, 0, 0x30);
                    memcpy(temp, param->value.val.m.data, param->value.val.m.length);
                    util_xor(temp, 0x30, 0xaa, 0, 0);
                    ret = 0x30;
                } else {
                    ret = value_to_data(&param->value, temp, 512);
                }

                file_write(out, temp, ret);
            }
        }

        const th95_instr_t sentinel = { -1, -1, sizeof(th95_instr_t), 0xff00, 0xff, 0x00 };
        file_write(out, &sentinel, sizeof(th95_instr_t));

        ++s;
    }

    thecl_local_data_t* local_data;
    list_for_each(&ecl->local_data, local_data) {
        if (strcmp(local_data->name, "th95_footer") == 0) {
            file_write(out, local_data->data, local_data->data_length);
        }
    }

    file_size = file_tell(out);
    header.filesize = file_size;
    header.size2 = file_size - sizeof(th95_header_t);

    file_seek(out, 0);
    file_write(out, &header, sizeof(th95_header_t));
    file_write(out, offsets, ecl->sub_count * sizeof(uint32_t));
    free(offsets);

    return 1;
}

const thecl_module_t th95_ecl = {
    .open = th95_open,
    .trans = th95_trans,
    .dump = th95_dump,
    .parse = th95_parse,
    .compile = th95_compile,
};
