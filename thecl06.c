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
#include "util.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t sub_count;
    uint16_t extra_count;
    uint32_t offsets[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th06_header_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint32_t time;
    uint16_t id;
    uint16_t size;
    uint16_t rank_mask;
    uint16_t param_mask;
    unsigned char data[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th06_instr_t;

static ssize_t
th08_value_from_data(
    const unsigned char* data,
    size_t data_length,
    char type,
    value_t* value)
{
    if (type == 'm') {
        int ret;
        value_t temp;
        switch (data_length) {
        case 224:
            ret = value_from_data(data, 48, type, &temp);
            util_xor(temp.val.m.data, 48, 0xaa, 0, 0);
            break;
        case 176:
            ret = value_from_data(data, 48, type, &temp);
            util_xor(temp.val.m.data, 48, 0xbb, 0, 0);
            break;
        case 128:
            ret = value_from_data(data, 64, type, &temp);
            util_xor(temp.val.m.data, 64, 0xdd, 0, 0);
            break;
        case 64:
            ret = value_from_data(data, 64, type, &temp);
            util_xor(temp.val.m.data, 64, 0xee, 0, 0);
            break;
        default:
            ret = value_from_data(data, data_length, type, &temp);
            break;
        }
        value->type = 'z';
        value->val.z = (char*)temp.val.m.data;
        return ret;
    } else
        return value_from_data(data, data_length, type, value);
}

static ssize_t
th95_value_from_data(
    const unsigned char* data,
    size_t data_length,
    char type,
    value_t* value)
{
    if (type == 'm') {
        int ret;
        value_t temp;
        ret = value_from_data(data, 48, 'm', &temp);
        util_xor(temp.val.m.data, 48, 0xaa, 0, 0);
        value->type = 'z';
        value->val.z = (char*)temp.val.m.data;
        return ret;
    } else
        return value_from_data(data, data_length, type, value);
}

static char*
th06_param_to_text(
    const thecl_param_t* param)
{
    if (param->type == 'z') {
        const size_t zlen = strlen(param->value.val.z);
        char* ret = malloc(4 + zlen * 2);
        char* temp = ret;
        *temp++ = '"';
        for (size_t z = 0; z < zlen; ++z) {
            if (!param->value.val.z[z])
                break;
            if (param->value.val.z[z] == '"')
                *temp++ = '\\';
            *temp++ = param->value.val.z[z];
        }
        *temp++ = '"';
        *temp++ = '\0';
        return ret;
    } else
        return value_to_text(&param->value);
}

/* TODO: Try to derive the TH07 table from this. */
static const id_format_pair_t th06_fmts[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "SS" },
    { 3, "SSS" },
    { 4, "SS" },
    { 5, "Sf" },
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
    { 20, "Sff" },
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
    { 67, "ssSSffffS" },
    { 68, "ssSSffffS" },
    { 69, "ssSSffffS" },
    { 70, "ssSSffffS" },
    { 71, "ssSSffSSS" },
    { 74, "ssSSffSSS" },
    { 75, "ssSSffffS" },
    { 76, "S" },
    { 77, "S" },
    { 78, "" },
    { 79, "" },
    { 81, "ffS" },
    { 82, "SSSSffff" },
    { 83, "" },
    { 84, "S" },
    { 85, "ssfSffffSSSSSS" },
    { 86, "ssffSfffSSSSSS" },
    { 87, "S" },
    { 88, "Sf" },
    { 90, "SSSS" },
    { 92, "S" },
    { 93, "ssz" },
    { 94, "" },
    { 95, "SfffssS" },
    { 96, "" },
    { 97, "S" },
    { 98, "ssssS" },
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
    { 118, "SSss" },
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

static const id_format_pair_t th07_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 2, "SS" },
    { 3, "SSS" },
    { 4, "SS" },
    { 5, "ff" },
    { 6, "SS" },
    { 7, "SSS" },
    { 8, "ff" },
    { 9, "fff" },
    { 10, "SS" },
    { 11, "ff" },
    { 12, "SSS" },
    { 13, "SSS" },
    { 14, "SSS" },
    { 15, "SSS" },
    { 16, "SSS" },
    { 17, "S" },
    { 18, "S" },
    { 19, "fff" },
    { 20, "fff" },
    { 21, "fff" },
    { 22, "fff" },
    { 24, "ff" },
    { 25, "ff" },
    { 26, "fffff" },
    { 27, "fSSSffff" },
    { 28, "SSSS" },
    { 30, "SSSS" },
    { 31, "ffSS" },
    { 32, "SSSS" },
    { 33, "ffSS" },
    { 34, "SSSS" },
    { 35, "fSSS" },
    { 36, "SSSS" },
    { 37, "ffSS" },
    { 38, "SSSS" },
    { 39, "fSSS" },
    { 40, "f" },
    { 41, "S" },
    { 42, "" },
    { 43, "SSS" },
    { 44, "ffS" },
    { 45, "S" },
    { 46, "ffS" },
    { 47, "ffS" },
    { 48, "f" },
    { 49, "S" },
    { 50, "f" },
    { 52, "fff" },
    { 54, "SSff" },
    { 55, "SSffS" },
    { 56, "Sfffffff" },
    { 57, "fS" },
    { 59, "S" },
    { 62, "ffff" },
    { 63, "" },
    { 64, "ssSSffffS" },
    { 65, "ssSSffffS" },
    { 66, "ssSSffffS" },
    { 67, "ssSSffffS" },
    { 69, "ssSSffffS" },
    { 72, "ssSSffffS" },
    { 73, "S" },
    { 74, "S" },
    { 75, "" },
    { 76, "" },
    { 78, "ffS" },
    { 79, "SSSSSff" },
    { 80, "" },
    { 81, "SS" },
    { 82, "ssfSSfffSSSSSS" },
    { 84, "S" },
    { 85, "Sf" },
    { 87, "SSSS" },
    { 89, "S" },
    { 90, "ssm" },
    { 91, "" },
    { 92, "SffSSSS" },
    { 93, "SffSSSS" },
    { 94, "" },
    { 95, "S" },
    { 96, "ssssS" },
    { 97, "SS" },
    { 98, "S" },
    { 99, "S" },
    { 100, "Sffff" },
    { 101, "fff" },
    { 102, "S" },
    { 103, "S" },
    { 104, "S" },
    { 105, "S" },
    { 106, "S" },
    { 107, "S" },
    { 108, "SS" },
    { 110, "S" },
    { 111, "S" },
    { 112, "S" },
    { 113, "S" },
    { 114, "S" },
    { 115, "S" },
    { 116, "S" },
    { 117, "SSS" },
    { 118, "SSSfSS" },
    { 119, "S" },
    { 120, "S" },
    { 121, "SS" },
    { 122, "SS" },
    { 124, "S" },
    { 125, "S" },
    { 126, "S" },
    { 128, "S" },
    { 131, "SSSSSS" },
    { 132, "S" },
    { 133, "" },
    { 135, "S" },
    { 136, "S" },
    { 137, "S" },
    { 138, "SSSS" },
    { 139, "SSSS" },
    { 140, "ffff" },
    { 141, "S" },
    { 142, "S" },
    { 143, "f" },
    { 144, "SS" },
    { 145, "SS" },
    { 146, "" },
    { 148, "SSS" },
    { 149, "SffS" },
    { 150, "f" },
    { 151, "ffff" },
    { 152, "Sf" },
    { 153, "fff" },
    { 154, "S" },
    { 155, "f" },
    { 156, "SS" },
    { 157, "Sf" },
    { 158, "Sff" },
    { 159, "ffff" },
    { 160, "S" },
    { 161, "S" },
    { 0, 0 },
};

static const id_format_pair_t th08_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 2, "S" },
    { 3, "S" },
    { 4, "SS" },
    { 5, "SSS" },
    { 6, "SS" },
    { 7, "ff" },
    { 9, "ff" },
    { 10, "SS" },
    { 11, "SS" },
    { 12, "SS" },
    { 13, "SS" },
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
    { 31, "S" },
    { 32, "ff" },
    { 33, "ff" },
    { 34, "fffff" },
    { 36, "fSSSffff" },
    { 37, "f" },
    { 38, "ffff" },
    { 39, "fffff" },
    { 40, "SSSS" },
    { 42, "SSSS" },
    { 44, "SSSS" },
    { 45, "ffSS" },
    { 46, "SSSS" },
    { 47, "ffSS" },
    { 48, "SSSS" },
    { 49, "ffSS" },
    { 50, "SSSS" },
    { 51, "ffSS" },
    { 52, "S" },
    { 53, "" },
    { 54, "S" },
    { 55, "S" },
    { 57, "SS" },
    { 58, "S" },
    { 59, "S" },
    { 61, "SS" },
    { 62, "" },
    { 63, "ff" },
    { 64, "SSff" },
    { 65, "ff" },
    { 66, "SSff" },
    { 67, "SSf" },
    { 70, "f" },
    { 71, "f" },
    { 72, "SffffSf" },
    { 73, "Sfff" },
    { 74, "Sff" },
    { 75, "ffff" },
    { 76, "" },
    { 77, "ff" },
    { 78, "ff" },
    { 79, "S" },
    { 80, "S" },
    { 81, "S" },
    { 82, "f" },
    { 83, "S" },
    { 87, "ffS" },
    { 88, "SS" },
    { 90, "SffSSS" },
    { 91, "SffSSS" },
    { 92, "SffSSS" },
    { 93, "SffSSSS" },
    { 94, "SffSSSS" },
    { 95, "" },
    { 96, "ssSSffffS" },
    { 97, "ssSSffffS" },
    { 98, "ssSSffffS" },
    { 99, "ssSSffffS" },
    { 100, "ssSSffffS" },
    { 101, "ssSSffffS" },
    { 104, "ssSSffffS" },
    { 105, "S" },
    { 106, "S" },
    { 107, "" },
    { 108, "" },
    { 110, "ff" },
    { 111, "SSSSSff" },
    { 112, "" },
    { 113, "SS" },
    { 114, "ssffffffSSSSSS" },
    { 116, "S" },
    { 117, "Sf" },
    { 119, "SSSS" },
    { 120, "S" },
    { 121, "S" },
    { 122, "ssSmmmm" },
    { 123, "" },
    { 124, "S" },
    { 126, "SS" },
    { 127, "S" },
    { 128, "Sffff" },
    { 129, "S" },
    { 130, "S" },
    { 131, "S" },
    { 132, "S" },
    { 133, "SSS" },
    { 134, "SS" },
    { 135, "SS" },
    { 136, "SS" },
    { 137, "SS" },
    { 139, "SSS" },
    { 140, "SSSfSS" },
    { 141, "S" },
    { 142, "S" },
    { 144, "SS" },
    { 145, "S" },
    { 147, "S" },
    { 148, "S" },
    { 152, "SSSSSS" },
    { 153, "" },
    { 155, "S" },
    { 157, "SSSS" },
    { 158, "SSSS" },
    { 159, "S" },
    { 160, "S" },
    { 162, "" },
    { 165, "f" },
    { 167, "Sf" },
    { 168, "S" },
    { 173, "S" },
    { 174, "S" },
    { 175, "S" },
    { 176, "S" },
    { 177, "S" },
    { 178, "SSf" },
    { 179, "" },
    { 181, "" },
    { 182, "S" },
    { 183, "S" },
    { 184, "S" },
    { 0, 0 },
};

static const id_format_pair_t th09_fmts[] = {
    { 14, "SS" },
    { 63, "Sf" },
    { 64, "SSSf" },
    { 110, "SS" },
    { 114, "ssffSSffSSSSSS" },
    { 138, "S" },
    { 185, "S" },
    { 186, "" },
    { 187, "S" },
    { 0, 0 },
};

static const id_format_pair_t th95_fmts[] = {
    { 0, "" },
    { 14, "SS" },
    { 42, "SfSS" },
    { 43, "fffS" },
    { 49, "fffS" },
    { 56, "SSSSSS" },
    { 84, "Sff" },
    { 85, "" },
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
    { 104, "m" },
    { 105, "" },
    { 108, "SS" },
    { 109, "S" },
    { 112, "S" },
    { 114, "S" },
    { 115, "SSS" },
    { 117, "SS" },
    { 118, "S" },
    { 119, "S" },
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
    { 157, "SSfffSSSSfSff" },
    { 0, 0 },
};

static const char*
th06_find_format(
    unsigned int version,
    unsigned int id)
{
    const char* ret = NULL;

    switch (version) {
    case 95:
        if (!ret) ret = find_format(th95_fmts, id);
        if (!ret) ret = find_format(th08_fmts, id);
        break;
    case 9:
        if (!ret) ret = find_format(th09_fmts, id);
    case 8:
        if (!ret) ret = find_format(th08_fmts, id);
        break;
    case 7:
        if (!ret) ret = find_format(th07_fmts, id);
        break;
    case 6:
        if (!ret) ret = find_format(th06_fmts, id);
        break;
    default:
        fprintf(stderr, "%s: unsupported version: %u\n", argv0, version);
        return NULL;
    }

    if (!ret)
        fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, id);

    return ret;
}

static thecl_t*
th06_open(
    FILE* stream,
    unsigned int version)
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

    /* TODO: Check magic. */
    if (version >= 8)
        header = (th06_header_t*)(map + sizeof(uint32_t));
    else
        header = (th06_header_t*)map;

    size_t extra_count;
    size_t extra_count_max;
    if (version == 9) {
        extra_count = header->extra_count;
        extra_count_max = header->extra_count;
    } else if (version == 6) {
        extra_count = 1;
        extra_count_max = 3;
    } else {
        extra_count = header->extra_count;
        extra_count_max = 16;
    }

    thecl_t* ecl = thecl_new();
    ecl->sub_count = header->sub_count;

    for (size_t s = 0; s < header->sub_count; ++s) {
        unsigned int rank = 0xff << 8;
        char name[256];
        thecl_sub_t* sub = malloc(sizeof(*sub));
        sprintf(name, "Sub%zu", s + 1);
        sub->name = strdup(name);
        list_init(&sub->instrs);
        sub->stack = 0;
        sub->arity = 0;
        sub->var_count = 0;
        sub->vars = NULL;
        sub->offset = 0;
        list_init(&sub->labels);
        unsigned int time = 0;

        const th06_instr_t* raw_instr =
            (th06_instr_t*)(map + header->offsets[extra_count_max + s]);
        while (raw_instr->time != 0xffffffff && raw_instr->size) {
            if (raw_instr->time != time) {
                thecl_instr_t* new = thecl_instr_time(raw_instr->time);
                list_append_new(&sub->instrs, new);
                time = raw_instr->time;
            }

            if (raw_instr->rank_mask != rank) {
                uint16_t rank_mask = raw_instr->rank_mask >> 8;
                if (version == 6)
                    rank_mask |= 0xf0;
                thecl_instr_t* new = thecl_instr_rank(rank_mask & 0xff);
                list_append_new(&sub->instrs, new);
                rank = raw_instr->rank_mask;
            }

            thecl_instr_t* instr = thecl_instr_new();
            instr->id = raw_instr->id;

            const char* format;

            if (raw_instr->id == 0)
                format = "";
            else
                format = th06_find_format(version, raw_instr->id);
            
            if (!format) {
                fprintf(stderr, "%-3d %04xB R%04x [%04x]: ",
                    raw_instr->id,
                    raw_instr->size,
                    raw_instr->rank_mask,
                    raw_instr->param_mask
                    );

                for (size_t d = 0; d < raw_instr->size - sizeof(th06_instr_t); d += 4) {
                    fprintf(stderr, " %08x (%d, %f)",
                        *(uint32_t*)(raw_instr->data + d),
                        *(int32_t*)(raw_instr->data + d),
                        *(float*)(raw_instr->data + d)
                        );
                }

                fprintf(stderr, "\n");
                goto next;
            }

            if (raw_instr->size > sizeof(th06_instr_t)) {
                value_t* values;
                if (version == 7 || version == 95)
                    values = value_list_from_data(th95_value_from_data, raw_instr->data, raw_instr->size - sizeof(th06_instr_t), format);
                else if (version == 8)
                    values = value_list_from_data(th08_value_from_data, raw_instr->data, raw_instr->size - sizeof(th06_instr_t), format);
                else
                    values = value_list_from_data(value_from_data, raw_instr->data, raw_instr->size - sizeof(th06_instr_t), format);
                value_t* value_iter = values;
                uint16_t param_mask = raw_instr->param_mask;
                while (value_iter->type) {
                    thecl_param_t* param = param_new((*value_iter).type);
                    param->value = *value_iter;
                    if (version != 6 && param_mask & 1)
                        param->stack = 1;
                    param_mask >>= 1;
                    ++value_iter;
                    list_append_new(&instr->params, param);
                }
                free(values);
            }

            list_append_new(&sub->instrs, instr);

next:
            raw_instr = (th06_instr_t*)((char*)raw_instr + raw_instr->size);
        }

        list_append_new(&ecl->subs, sub);
    }

    thecl_local_data_t* local_data;
    
    for (size_t e = 0; e < extra_count; ++e) {
        char name[128];
        sprintf(name, "th%02u_extra%zu", version, e + 1);

        size_t extra_length;
        if (e >= (size_t)(extra_count - 1))
            extra_length = file_size - header->offsets[e];
        else
            extra_length = header->offsets[e + 1] - header->offsets[e];

        local_data = malloc(sizeof(*local_data) + extra_length);
        strcpy(local_data->name, name);
        local_data->data_length = extra_length;
        memcpy(local_data->data, map + header->offsets[e], extra_length);
        list_append_new(&ecl->local_data, local_data);
    }

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
                fprintf(out, "!%s%s%s%s%s%s%s%s",
                    (instr->rank) & RANK_EASY    ? "E" : "",
                    (instr->rank) & RANK_NORMAL  ? "N" : "",
                    (instr->rank) & RANK_HARD    ? "H" : "",
                    (instr->rank) & RANK_LUNATIC ? "L" : "",
                    !((instr->rank) & RANK_UNKNOWN1) ? "W" : "",
                    !((instr->rank) & RANK_UNKNOWN2) ? "X" : "",
                    !((instr->rank) & RANK_UNKNOWN3) ? "Y" : "",
                    !((instr->rank) & RANK_UNKNOWN4) ? "Z" : "");
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
    unsigned int version,
    const thecl_instr_t* instr)
{
    size_t ret = sizeof(th06_instr_t);

    if        (version ==  6 && instr->id ==  93) {
        return ret + 4 + 34;
    } else if (version ==  7 && instr->id ==  90) {
        return ret + 4 + 48;
    } else if (version ==  8 && instr->id == 122) {
        return ret + 4 + 4 + 48 + 48 + 64 + 64;
    } else if (version == 95 && instr->id == 104) {
        return ret + 48;
    } else {
        thecl_param_t* param;
        list_for_each(&instr->params, param) {
            value_t v = param->value;
            v.type = param->type;
            ret += value_size(&v);
        }
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
    state.ecl->version = version;
    state.instr_format = th06_find_format;
    state.instr_size = th06_instr_size;

    yyin = in;

    if (yyparse(&state) != 0)
        return 0;

    return state.ecl;
}

static th06_instr_t*
th06_instr_serialize(
    const thecl_t* ecl,
    thecl_instr_t* instr)
{
    th06_instr_t* ret;
    ret = malloc(instr->size);
    ret->time = instr->time;
    ret->id = instr->id;
    ret->size = instr->size;

    if (instr->rank != 0xff) {
        ret->rank_mask = (instr->rank & 0xff) << 8;
        if (ecl->version == 6)
            ret->rank_mask &= 0x0f00;
    } else
        ret->rank_mask = 0xff00;

    if (ecl->version == 6)
        ret->param_mask = 0xff;
    else
        ret->param_mask = 0;

    int param_count = 0;
    thecl_param_t* param;
    unsigned char* param_data = ret->data;
    list_for_each(&instr->params, param) {
        if (param->stack)
            ret->param_mask |= 1 << param_count;
        ++param_count;
        if (param->value.type == 'z') {
            if (ecl->version == 6) {
                memset(param_data, 0, 34);
                strncpy((char*)param_data, param->value.val.z, 34);
                param_data += 34;
            } else if (ecl->version == 7 || ecl->version == 95) {
                memset(param_data, 0, 48);
                strncpy((char*)param_data, param->value.val.z, 48);
                util_xor(param_data, 48, 0xaa, 0, 0);
                param_data += 48;
            } else if (ecl->version == 8) {
                switch (param_count) {
                case 4:
                case 5:
                    memset(param_data, 0, 48);
                    strncpy((char*)param_data, param->value.val.z, 48);
                    util_xor(param_data, 48, param_count == 4 ? 0xaa : 0xbb, 0, 0);
                    param_data += 48;
                    break;
                case 6:
                case 7:
                    memset(param_data, 0, 64);
                    strncpy((char*)param_data, param->value.val.z, 64);
                    util_xor(param_data, 64, param_count == 6 ? 0xdd : 0xee, 0, 0);
                    param_data += 64;
                    break;
                }
            }
        } else {
            value_t v = param->value;
            v.type = param->type;
            param_data += value_to_data(&v, param_data, instr->size - (param_data - (unsigned char*)ret));
        }
    }

    return ret;
}

static int
th06_compile(
    const thecl_t* ecl,
    FILE* out)
{
    const uint32_t magic = ecl->version == 9 ? 0x00000900 : 0x00000800;
    const size_t header_size = sizeof(th06_header_t) + ((ecl->version >= 8) ? sizeof(uint32_t) : 0);

    th06_header_t header = {
        .sub_count = ecl->sub_count
    };
    uint32_t* offsets;
    const th06_instr_t sentinel = {
        .time = 0xffffffff,
        .id = 0xffff,
        .size = sizeof(sentinel),
        .rank_mask = 0xff00,
        .param_mask = 0x00ff
    };

    thecl_local_data_t* local_data;
    list_for_each(&ecl->local_data, local_data)
        ++header.extra_count;

    size_t extra_count;
    if (ecl->version == 9)
        extra_count = header.extra_count;
    else if (ecl->version == 6) {
        extra_count = 3;
        header.extra_count = 0;
    } else
        extra_count = 16;
    const size_t offset_count = extra_count + header.sub_count;

    if (!file_seekable(out)) {
        fprintf(stderr, "%s: output is not seekable\n", argv0);
        return 0;
    }

    file_seek(out, header_size + offset_count * sizeof(uint32_t));

    offsets = calloc(offset_count, sizeof(*offsets));

    thecl_sub_t* sub;
    size_t s = 0;
    list_for_each(&ecl->subs, sub) {
        offsets[extra_count + s] = file_tell(out);

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr) {
            th06_instr_t* raw_instr = th06_instr_serialize(ecl, instr);
            file_write(out, raw_instr, raw_instr->size);
            free(raw_instr);
        }

        file_write(out, &sentinel, sizeof(sentinel));

        ++s;
    }

    size_t o = 0;
    list_for_each(&ecl->local_data, local_data) {
        offsets[o++] = file_tell(out);
        file_write(out, local_data->data, local_data->data_length);
    }
    if (ecl->version != 9 && ecl->version != 6)
        offsets[o] = file_tell(out);

    file_seek(out, 0);
    if (ecl->version >= 8)
        file_write(out, &magic, sizeof(magic));
    file_write(out, &header, sizeof(header));
    file_write(out, offsets, offset_count * sizeof(*offsets));
    free(offsets);

    return 1;
}

const thecl_module_t th06_ecl = {
    .open = th06_open,
    .trans = th06_trans,
    .dump = th06_dump,
    .parse = th06_parse,
    .compile = th06_compile,
};
