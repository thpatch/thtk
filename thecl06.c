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
#include "file.h"
#include "program.h"
#include "thecl.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint32_t sub_count;
    uint32_t enemy_data_offset;
    uint32_t zero[2];
    uint32_t sub_offsets[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th06_header_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t time;
    /* Normally 0, 0xffff for the end instruction. */
    uint16_t unknown1;
    uint16_t id;
    uint16_t size;
    /* 0xff00 normally.  0x0X00 when masking certain difficulties. */
    uint16_t rank_mask;
    /* 0x00ff */
    uint16_t unknown3;
    unsigned char data[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th06_instr_t;

#if 0
typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t time;
    uint16_t unknown1;
    uint16_t type;
    uint16_t size;
    unsigned char data[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th06_enemy_data_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    float x;
    float y;
    uint32_t unknown2;
    uint16_t hp;
    int16_t item;
    uint32_t point;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th06_enemy_cont1_t;
#endif

static char*
th06_param_to_text(
    const thecl_param_t* param)
{
    if (param->type == 'm' || param->type == 'M') {
        char* ret = malloc(1024);
        char* temp = ret;
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

static const id_format_pair_t th06_fmts[] = {
    { 1, "S" },
    { 2, "SS" }, // jump?
    { 3, "SSS" }, // this looks like a jump
    { 4, "SS" }, // maybe a load
    { 5, "Sf" }, // load for floats?
    { 6, "SS" },
    { 8, "Sf" },
    { 9, "Sff" },
    { 10, "S" },
    { 13, "SSS" },
    { 14, "SSS" },
    { 15, "SSS" },
    { 16, "SSS" },
    { 17, "SSS" },
    { 18, "S" },
    { 20, "Sff" }, // probably a mathematical operation
    { 21, "Sff" },
    { 23, "Sff" },
    { 25, "Sffff" },
    { 26, "S" },
    { 27, "SS" },
    { 28, "ff" },
    { 29, "SS" },
    { 30, "SS" },
    { 31, "SS" },
    { 32, "SS" },
    { 33, "SS" },
    { 34, "SS" },
    { 35, "SSf" },
    { 36, "" },
    { 39, "SSSSS" },
    { 43, "ffS" },
    { 45, "ff" },
    { 46, "f" },
    { 47, "f" },
    { 48, "f" },
    { 49, "ff" },
    { 50, "ff" },
    { 51, "Sf" },
    { 52, "Sff" },
    { 56, "SffS" },
    { 57, "SffS" },
    { 59, "SffS" },
    { 61, "S" },
    { 63, "S" },
    { 65, "ffff" },
    { 66, "" },
#if 0
    { 67, "ssSSffffS" },
    { 68, "ssSSffffS" },
    { 69, "ssSSffffS" },
    { 70, "ssSSffffS" },
    { 71, "ssSSffSSS" },
    { 74, "ssSSffSSS" },
    { 75, "ssSSffffS" },
#else
    { 67, "SSSffffS" },
    { 68, "SSSffffS" },
    { 69, "SSSffffS" },
    { 70, "SSSffffS" },
    { 71, "SSSffSSS" },
    { 74, "SSSffSSS" },
    { 75, "SSSffffS" },
#endif
    { 76, "S" },
    { 77, "S" },
    { 78, "" },
    { 79, "" },
    { 81, "ffS" },
    { 82, "SSSSffff" },
    { 83, "" },
    { 84, "S" },
#if 0
    { 85, "ssfSffffSSSSSS" },
    { 86, "ssffSfffSSSSSS" },
#else
    { 85, "SfSffffSSSSSS" },
    { 86, "SffSfffSSSSSS" },
#endif
    { 87, "S" },
    { 88, "Sf" },
    { 90, "SSSS" },
    { 92, "S" },
#if 0
    { 93, "ssm" },
#else
    { 93, "Sm" },
#endif
    { 94, "" },
#if 0
    { 95, "SfffssS" },
#else
    { 95, "SfffSS" },
#endif
    { 96, "" },
    { 97, "S" },
#if 0
    { 98, "ssssS" },
#else
    { 98, "SSS" },
#endif
    { 99, "SS" },
    { 100, "S" },
    { 101, "S" },
    { 102, "Sffff" },
    { 103, "fff" },
    { 104, "S" },
    { 105, "S" },
    { 106, "S" },
    { 107, "S" },
    { 108, "S" },
    { 109, "SS" },
    { 111, "S" },
    { 112, "S" },
    { 113, "S" },
    { 114, "S" },
    { 115, "S" },
    { 116, "S" },
    { 117, "S" },
#if 0
    { 118, "SSss" },
#else
    { 118, "SSS" },
#endif
    { 119, "S" },
    { 120, "S" },
    { 121, "SS" },
    { 122, "S" },
    { 123, "S" },
    { 124, "S" },
    { 125, "" },
    { 126, "S" },
    { 127, "S" },
    { 128, "S" },
    { 129, "SS" },
    { 130, "S" },
    { 131, "ffSSSS" },
    { 132, "S" },
    { 133, "" },
    { 134, "" },
    { 135, "S" },
    { 0, 0 },
};

static thecl_t*
th06_open(
    FILE* stream,
    unsigned int __attribute__((unused)) version)
{
    long file_size;
    unsigned char* map;

    const th06_header_t* header;

    file_size = file_fsize(stream);
    if (file_size == -1)
        return NULL;

    map = file_mmap(stream, file_size);
    if (!map)
        return NULL;

    header = (th06_header_t*)map;

    thecl_t* ecl = thecl_new();
    ecl->sub_count = header->sub_count;

    for (size_t s = 0; s < header->sub_count; ++s) {
        unsigned int rank = 0xff << 8;
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

        const th06_instr_t* raw_instr = (th06_instr_t*)(map + header->sub_offsets[s]);
        while (raw_instr->time != 0xffff) {
            if (raw_instr->time != time) {
                thecl_instr_t* new = thecl_instr_time(raw_instr->time);
                list_append_new(&sub->instrs, new);
                time = raw_instr->time;
            }

            if (raw_instr->rank_mask != rank) {
                thecl_instr_t* new = thecl_instr_rank(raw_instr->rank_mask);
                list_append_new(&sub->instrs, new);
                rank = raw_instr->rank_mask;
            }

            thecl_instr_t* instr = thecl_instr_new();
            instr->id = raw_instr->id;

            if (raw_instr->size > sizeof(th06_instr_t)) {
                const char* format;

                if (raw_instr->id == 0)
                    format = "";
                else
                    format = find_format(th06_fmts, raw_instr->id);

                value_t* values = value_list_from_data(value_from_data, raw_instr->data, raw_instr->size - sizeof(th06_instr_t), format);
                if (!values) {
                    fprintf(stderr,
                        "\"%s\" %p %p %p %u %u %lu\n",
                        format,
                        map,
                        map + header->sub_offsets[s],
                        (void*)raw_instr,
                        raw_instr->id,
                        raw_instr->size,
                        sizeof(th06_instr_t));
                    abort();
                }
                value_t* value_iter = values;
                while (value_iter->type) {
                    thecl_param_t* param = param_new((*value_iter).type);
                    param->value = *value_iter;
                    ++value_iter;
                    list_append_new(&instr->params, param);
                }
                free(values);
            }

            list_append_new(&sub->instrs, instr);

            raw_instr = (th06_instr_t*)((char*)raw_instr + raw_instr->size);
        }

        list_append_new(&ecl->subs, sub);
    }

    thecl_local_data_t* local_data = malloc(sizeof(thecl_local_data_t) + (file_size - header->enemy_data_offset));
    strcpy(local_data->name, "th06_footer");
    local_data->data_length = file_size - header->enemy_data_offset;
    memcpy(local_data->data, map + header->enemy_data_offset, local_data->data_length);
    list_append_new(&ecl->local_data, local_data);

#if 0
    const th06_enemy_data_t* enemy_data = (th06_enemy_data_t*)(map + header->enemy_data_offset);
    while ((ptrdiff_t)enemy_data < (ptrdiff_t)(map + file_size)) {
        th06_enemy_cont1_t* extra1 = (th06_enemy_cont1_t*)enemy_data->data;
        fprintf(stderr, "%p %p %p %p | %u, %u, %u, %u\n",
            map,
            map + header->enemy_data_offset,
            (void*)enemy_data,
            map + file_size,
            enemy_data->time,
            enemy_data->unknown1,
            enemy_data->type,
            enemy_data->size);
        if (enemy_data->size == 28) {
            fprintf(stderr, "    %f, %f, %u, %u, %d, %u\n",
                extra1->x,
                extra1->y,
                extra1->unknown2,
                extra1->hp,
                extra1->item,
                extra1->point);
        }
        if (enemy_data->time == 0xffff)
            break;
        enemy_data = (th06_enemy_data_t*)((char*)enemy_data + enemy_data->size);
    }
#endif

    file_munmap(map, file_size);

    return ecl;
}

static void
th06_trans(
    thecl_t* ecl)
{
    if (!ecl)
        return;
}

static void
th06_dump(
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
            case THECL_INSTR_RANK:
                fprintf(out, "!%s%s%s%s",
                    (instr->rank >> 8) & RANK_EASY    ? "E" : "",
                    (instr->rank >> 8) & RANK_NORMAL  ? "N" : "",
                    (instr->rank >> 8) & RANK_HARD    ? "H" : "",
                    (instr->rank >> 8) & RANK_LUNATIC ? "L" : "");
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
                    char* ret = th06_param_to_text(param);
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
th06_instr_size(
    const thecl_instr_t* instr)
{
    size_t ret = sizeof(th06_instr_t);
    thecl_param_t* param;

    list_for_each(&instr->params, param) {
        if (param->type == 'M')
            ret += 34;
        else
            ret += value_size(&param->value);
    }

    return ret;
}

static thecl_t*
th06_parse(
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
    state.instr_size = th06_instr_size;

    yyin = in;

    if (yyparse(&state) != 0)
        return 0;

    return state.ecl;
}

static th06_instr_t*
th06_instr_serialize(
    thecl_instr_t* instr)
{
    th06_instr_t* ret;
    ret = malloc(instr->size);
    ret->time = instr->time;
    ret->unknown1 = 0;
    ret->id = instr->id;
    ret->size = instr->size;
    if (instr->rank != 0xff)
        ret->rank_mask = (instr->rank & 0xf) << 8;
    else
        ret->rank_mask = 0xff00;
    ret->unknown3 = 0x00ff;

    thecl_param_t* param;
    unsigned char* param_data = ret->data;
    list_for_each(&instr->params, param) {
        if (param->type == 'M') {
            memset(param_data, 0, 34);
            memcpy(param_data, param->value.val.m.data, param->value.val.m.length);
            param_data += 34;
        } else
            param_data += value_to_data(&param->value, param_data, instr->size - (param_data - (unsigned char*)ret));
    }

    return ret;
}

static int
th06_compile(
    const thecl_t* ecl,
    FILE* out)
{
    th06_header_t header = {
        ecl->sub_count,
        0,
        { 0, 0 }
    };

    if (!file_seekable(out)) {
        fprintf(stderr, "%s: output is not seekable\n", argv0);
        return 0;
    }

    file_seek(out, sizeof(th06_header_t) + ecl->sub_count * sizeof(uint32_t));

    thecl_sub_t* sub;
    uint32_t* offsets = malloc(ecl->sub_count * sizeof(uint32_t));
    size_t s = 0;
    list_for_each(&ecl->subs, sub) {
        offsets[s] = file_tell(out);

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr) {
            th06_instr_t* raw_instr = th06_instr_serialize(instr);
            file_write(out, raw_instr, raw_instr->size);
            free(raw_instr);
        }

        const th06_instr_t sentinel = { 0xffff, 0xffff, 0xffff, sizeof(th06_instr_t), 0xff00, 0x00ff };
        file_write(out, &sentinel, sizeof(th06_instr_t));

        ++s;
    }

    header.enemy_data_offset = file_tell(out);
    thecl_local_data_t* local_data;
    list_for_each(&ecl->local_data, local_data) {
        if (strcmp(local_data->name, "th06_footer") == 0) {
            file_write(out, local_data->data, local_data->data_length);
        }
    }

    file_seek(out, 0);
    file_write(out, &header, sizeof(th06_header_t));
    file_write(out, offsets, ecl->sub_count * sizeof(uint32_t));
    free(offsets);

    return 0;
}

const thecl_module_t th06_ecl = {
    .open = th06_open,
    .trans = th06_trans,
    .dump = th06_dump,
    .parse = th06_parse,
    .compile = th06_compile,
};
