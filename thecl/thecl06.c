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
#include <math.h>
#include "path.h"
#include "file.h"
#include "program.h"
#include "thecl.h"
#include "util.h"

typedef struct {
PACK_BEGIN
    uint16_t sub_count;
    uint16_t timeline_count;
    uint32_t offsets[];
PACK_END
} PACK_ATTRIBUTE th06_header_t;

typedef struct {
PACK_BEGIN
    uint32_t time;
    uint16_t id;
    uint16_t size;
    uint16_t rank_mask;
    uint16_t param_mask;
    unsigned char data[];
PACK_END
} PACK_ATTRIBUTE th06_instr_t;

typedef struct {
PACK_BEGIN
    uint16_t time;
    int16_t arg0;
    uint16_t id;
    uint8_t size;
    uint8_t rank;
    unsigned char data[];
PACK_END
} PACK_ATTRIBUTE th06_timeline_instr_t;

static ssize_t
th06_value_from_data(
    const unsigned char* data,
    size_t data_length,
    char type,
    value_t* value)
{
    if (type == 'n') {
        return value_from_data(data, data_length, 's', value);
    } else if (type == 'o' || type == 'N') {
        return value_from_data(data, data_length, 'S', value);
    } else {
        return value_from_data(data, data_length, type, value);
    }
}

static ssize_t
th08_value_from_data(
    const unsigned char* data,
    size_t data_length,
    char type,
    value_t* value)
{
    if (type == 'n') {
        return value_from_data(data, data_length, 's', value);
    } else if (type == 'o' || type == 'N') {
        return value_from_data(data, data_length, 'S', value);
    } else if (type == 'm') {
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
    if (type == 'n') {
        return value_from_data(data, data_length, 's', value);
    } else if (type == 'o' || type == 'N') {
        return value_from_data(data, data_length, 'S', value);
    } else if (type == 'm') {
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
    if (param->type == 'z' || param->type == 'm') {
        const size_t zlen = strlen(param->value.val.z);
        char* ret = malloc(4 + zlen * 2);
        char* temp = ret;
        *temp++ = '"';
        for (size_t z = 0; z < zlen; ++z) {
            if (!param->value.val.z[z])
                break;
            if (param->value.val.z[z] == '"' || param->value.val.z[z] == '\\')
                *temp++ = '\\';
            *temp++ = param->value.val.z[z];
        }
        *temp++ = '"';
        *temp++ = '\0';
        return ret;
    } else
        return value_to_text(&param->value);
}

static char*
th06_stringify_param(
    const thecl_sub_t* sub,
    const thecl_instr_t* instr,
    const thecl_param_t* param,
    unsigned int version)
{
    char temp[512];
    switch(param->type) {
        case 'n':
            snprintf(temp, 512, "\"Sub%d\"", param->value.val.s);
            return strdup(temp);
        case 'N':
            snprintf(temp, 512, "\"Sub%d\"", param->value.val.S);
            return strdup(temp);
        case 'o':
            snprintf(temp, 512, "%s_%d", sub->name, instr->offset + param->value.val.S);
            return strdup(temp);
        default:
            if (
                   (param->stack || version == 6)
                && (param->value.type == 'f' || param->value.type == 'S' || param->value.type == 's')
            ) {
                int val;
                if (param->value.type == 'S') val = param->value.val.S;
                else if (param->value.type == 'f') val = floor(param->value.val.f);
                else val = param->value.val.s;

                seqmap_entry_t* ent = seqmap_get(g_eclmap->gvar_names, val);
                if (ent) {
                    snprintf(temp, 256, "%c%s", param->value.type == 'f' ? '%' : '$', ent->value);
                    return strdup(temp);
                }
            }


            char* ret = th06_param_to_text(param);
            sprintf(temp, "%s%s%s",
                param->stack ? "[" : "",
                ret,
                param->stack ? "]" : "");
            free(ret);

            return strdup(temp);
    }
}

static const id_format_pair_t th06_timeline_fmts[] = {
    /* the first parameter is a part of the struct and is always s */
    {0, "nfffssS"},
    {2, "nfffssS"},
    {4, "nfffssS"},
    {6, "nfffssS"},
    {8, "s"},
    {9, "s"},
    {10, "sSS"},
    {12, "s"},
    { 0, 0 },
};

static const id_format_pair_t th07_timeline_fmts[] = {
    {0, "sfffssSS"},
    {2, "sfffssSS"},
    {4, "sfffssSS"},
    {6, "sfffssSS"},
    { 0, 0 },
};

static const id_format_pair_t th08_timeline_fmts[] = {
    { 0, "sSffssSS"},
    { 1, "sSffssSS"},
    { 2, "sSffssSSS"},
    { 4, "sSffssSSS"},
    { 6, "sS"},
    { 7, "sS"},
    { 8, "sSS"},
    { 10, "sS"},
    { 13, "sS"},
    { 14, "sS"},
    { 15, "sSffssSS"},
    { 16, "s"},
    { 0, 0 },
};

static const id_format_pair_t th09_timeline_fmts[] = {
    { 17, "sSffssSS"},
    { 0, 0 },
};

static const char*
th06_find_timeline_format(
    unsigned int version,
    unsigned int id)
{
    seqmap_entry_t *ent = seqmap_get(g_eclmap->timeline_ins_signatures, id);
    if (ent)
        return ent->value;

    const char* ret = NULL;

    switch(version) {
        case 95:
        case 9:
            if ((ret = find_format(th09_timeline_fmts, id))) break; /* fallthrough */
        case 8:
            ret = find_format(th08_timeline_fmts, id);
            break;
        case 7:
            if ((ret = find_format(th07_timeline_fmts, id))) break; /* fallthrough */
        case 6:
            ret = find_format(th06_timeline_fmts, id);
            break;
        default:
            fprintf(stderr, "%s: unsupported version: %u\n", argv0, version);
            return NULL;
    }

    if (!ret)
        fprintf(stderr, "%s: id %d was not found in the timeline format table\n", argv0, id);

    return ret;
}

/* TODO: Try to derive the TH07 table from this. */
static const id_format_pair_t th06_fmts[] = {
    /* Special param types:
        - 'o' - 'S' used as a label (offset in jump instructions)
        - 'N' - 'S' used as a sub name string (in call/enm creation instructions)
        - 'n' - 's' used as a sub name string
    */
    { 0, "" },
    { 1, "S" },
    { 2, "So" },
    { 3, "SoS" },
    { 4, "SS" },
    { 5, "Sf" },
    { 6, "SS" },
    { 7, "SSS" },
    { 8, "Sf" },
    { 9, "Sff" },
    { 10, "S" },
    { 11, "S" },
    { 12, "S" },
    { 13, "SSS" },
    { 14, "SSS" },
    { 15, "SSS" },
    { 16, "SSS" },
    { 17, "SSS" },
    { 18, "S" },
    { 19, "S" },
    { 20, "Sff" },
    { 21, "Sff" },
    { 22, "Sff" },
    { 23, "Sff" },
    { 24, "Sff" },
    { 25, "Sffff" },
    { 26, "S" },
    { 27, "SS" },
    { 28, "ff" },
    { 29, "So" },
    { 30, "So" },
    { 31, "So" },
    { 32, "So" },
    { 33, "So" },
    { 34, "So" },
    { 35, "NSf" },
    { 36, "" },
    { 37, "NSfSS" },
    { 38, "NSfSS" },
    { 39, "NSfSS" },
    { 40, "NSfSS" },
    { 41, "NSfSS" },
    { 42, "NSfSS" },
    { 43, "fff" },
    { 44, "fff" },
    { 45, "ff" },
    { 46, "f" },
    { 47, "f" },
    { 48, "f" },
    { 49, "ff" },
    { 50, "ff" },
    { 51, "ff" },
    { 52, "Sff" },
    { 53, "Sff" },
    { 54, "Sff" },
    { 55, "Sff" },
    { 56, "Sfff" },
    { 57, "Sfff" },
    { 58, "Sfff" },
    { 59, "Sfff" },
    { 60, "Sfff" },
    { 61, "S" },
    { 62, "S" },
    { 63, "S" },
    { 64, "S" },
    { 65, "ffff" },
    { 66, "" },
    { 67, "ssSSffffS" },
    { 68, "ssSSffffS" },
    { 69, "ssSSffffS" },
    { 70, "ssSSffffS" },
    { 71, "ssSSffffS" },
    { 72, "ssSSffffS" },
    { 73, "ssSSffffS" },
    { 74, "ssSSffffS" },
    { 75, "ssSSffffS" },
    { 76, "S" },
    { 77, "S" },
    { 78, "" },
    { 79, "" },
    { 80, "" },
    { 81, "fff" },
    { 82, "SSSSffff" },
    { 83, "" },
    { 84, "S" },
    { 85, "ssfSffffSSSSSS" },
    { 86, "ssffSfffSSSSSS" },
    { 87, "S" },
    { 88, "Sf" },
    { 89, "Sf" },
    { 90, "Sfff" },
    { 91, "S" },
    { 92, "S" },
    { 93, "ssz" },
    { 94, "" },
    { 95, "NfffssS" },
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
    { 108, "N" },
    { 109, "NS" },
    { 110, "S" },
    { 111, "S" },
    { 112, "S" },
    { 113, "S" },
    { 114, "N" },
    { 115, "S" },
    { 116, "N" },
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
};

static const id_format_pair_t th07_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 2, "So" },
    { 3, "SoS" },
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
    { 28, "SSSo" },
    { 29, "ffSo" },
    { 30, "SSSo" },
    { 31, "ffSo" },
    { 32, "SSSo" },
    { 33, "ffSo" },
    { 34, "SSSo" },
    { 35, "ffSo" },
    { 36, "SSSo" },
    { 37, "ffSo" },
    { 38, "SSSo" },
    { 39, "ffSo" },
    { 40, "f" },
    { 41, "S" },
    { 42, "" },
    { 43, "SSS" },
    { 44, "ffS" },
    { 45, "S" },
    { 46, "fff" },
    { 47, "ffS" },
    { 48, "f" },
    { 49, "S" },
    { 50, "f" },
    { 52, "fff" },
    { 53, "ff" },
    { 54, "SSff" },
    { 55, "SSfff" },
    { 56, "Sfffffff" },
    { 57, "ff" },
    { 58, "ff" },
    { 59, "S" },
    { 60, "S" },
    { 61, "S" },
    { 62, "ffff" },
    { 63, "" },
    { 64, "ssSSffffS" },
    { 65, "ssSSffffS" },
    { 66, "ssSSffffS" },
    { 67, "ssSSffffS" },
    { 68, "ssSSffffS" },
    { 69, "ssSSffffS" },
    { 70, "ssSSffffS" },
    { 71, "ssSSffffS" },
    { 72, "ssSSffffS" },
    { 73, "S" },
    { 74, "S" },
    { 75, "" },
    { 76, "" },
    { 77, "" },
    { 78, "fff" },
    { 79, "SSSSSff" },
    { 80, "" },
    { 81, "SS" },
    { 82, "ssffffffSSSSSS" },
    { 83, "ssffffffSSSSSS" },
    { 84, "S" },
    { 85, "Sf" },
    { 86, "Sf" },
    { 87, "Sfff" },
    { 88, "S" },
    { 89, "S" },
    { 90, "ssm" },
    { 91, "" },
    { 92, "SfffSSS" },
    { 93, "SfffSSS" },
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
    { 118, "SSSfff" },
    { 119, "S" },
    { 120, "S" },
    { 121, "SS" },
    { 122, "SS" },
    { 124, "S" },
    { 125, "S" },
    { 126, "S" },
    { 128, "S" },
    { 131, "ffSSSS" },
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
    { 4, "So" },
    { 5, "SoS" },
    { 6, "SS" },
    { 7, "ff" },
    { 9, "ff" },
    { 10, "SS" },
    { 11, "SS" },
    { 12, "SS" },
    { 13, "SS" },
    { 14, "SS" },
    { 15, "ff" },
    { 16, "ff" },
    { 17, "ff" },
    { 18, "ff" },
    { 19, "ff" },
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
    { 40, "SSSo" },
    { 42, "SSSo" },
    { 44, "SSSo" },
    { 45, "ffSo" },
    { 46, "SSSo" },
    { 47, "ffSo" },
    { 48, "SSSo" },
    { 49, "ffSo" },
    { 50, "SSSo" },
    { 51, "ffSo" },
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
    { 72, "Sffffff" },
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
    { 84, "" },
    { 85, "" },
    { 86, "SSS" },
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
    { 118, "Sf" },
    { 119, "Sfff" },
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
    { 152, "ffSSSS" },
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
    unsigned int id,
    bool is_timeline)
{
    if (is_timeline) return th06_find_timeline_format(version, id);

    seqmap_entry_t *ent = seqmap_get(g_eclmap->ins_signatures, id);
    if (ent)
        return ent->value;

    const char* ret = NULL;

    switch (version) {
    case 95:
        if ((ret = find_format(th95_fmts, id))) break;
        ret = find_format(th08_fmts, id);
        break;
    case 9:
        if ((ret = find_format(th09_fmts, id))) break; /* fallthrough */
    case 8:
        ret = find_format(th08_fmts, id);
        break;
    case 7:
        ret = find_format(th07_fmts, id);
        break;
    case 6:
        ret = find_format(th06_fmts, id);
        break;
    default:
        fprintf(stderr, "%s: unsupported version: %u\n", argv0, version);
        return NULL;
    }

    if (!ret)
        fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, id);

    return ret;
}

static int
th06_check_timeline_sentinel(
    unsigned int version,
    th06_timeline_instr_t* raw_instr)
{
    if (version < 8)
        return raw_instr->time == 0xffff && raw_instr->arg0 == 4;
    else
        return raw_instr->time == 0xffff && (uint16_t)raw_instr->arg0 == 0xffff && raw_instr->size == 0x00;
}

static void
th06_insert_labels(
    thecl_t* ecl)
{
    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr) {

            thecl_param_t* param;
            list_for_each(&instr->params, param) {

                if (param->type == 'o') {
                    if (param->value.val.S + instr->offset == 0) {
                        /* Beginning of the sub, so the label can't be inserted
                         * by looking for instr to insert if after. */
                        thecl_instr_t* label = thecl_instr_label(0);
                        list_prepend_new(&sub->instrs, label);
                        continue;
                    }

                    list_node_t* node;
                    list_for_each_node(&sub->instrs, node) {
                        thecl_instr_t* instr_find = node->data;

                        if (instr_find->offset + instr_find->size == instr->offset + param->value.val.S) {

                            thecl_instr_t* instr_next = node->next ? (thecl_instr_t*)node->next->data : NULL;
                            if (instr_next == NULL || (instr_next != NULL && instr_next->type != THECL_INSTR_LABEL)) {
                                thecl_instr_t* label = thecl_instr_label(instr_find->offset + instr_find->size);
                                list_append_to(&sub->instrs, label, node);
                            }

                            break;
                        }
                    }
                }
            }
        }
    }
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

    size_t timeline_count;
    size_t timeline_count_max;
    if (version == 9) {
        timeline_count = header->timeline_count;
        timeline_count_max = header->timeline_count;
    } else if (version == 6) {
        timeline_count = 1;
        timeline_count_max = 3;
    } else {
        timeline_count = header->timeline_count;
        timeline_count_max = 16;
    }

    thecl_t* ecl = thecl_new();
    ecl->version = version;
    ecl->sub_count = header->sub_count;

    for (size_t s = 0; s < header->sub_count; ++s) {
        unsigned int rank = 0xff << 8;
        char name[256];
        thecl_sub_t* sub = malloc(sizeof(*sub));
        memset(sub, 0, sizeof(*sub));
        sprintf(name, "Sub%zu", s);
        sub->name = strdup(name);
        sub->format = NULL;
        list_init(&sub->instrs);
        list_init(&sub->labels);
        unsigned int time = 0;

        const void* raw_sub = map + header->offsets[timeline_count_max + s];
        const th06_instr_t* raw_instr = (th06_instr_t*)(raw_sub);
        while (raw_instr->time != 0xffffffff && raw_instr->size) {
            if (raw_instr->time != time) {
                thecl_instr_t* new = thecl_instr_time(raw_instr->time);
                new->offset = (ptrdiff_t)raw_instr - (ptrdiff_t)raw_sub;
                list_append_new(&sub->instrs, new);
                time = raw_instr->time;
            }

            if (raw_instr->rank_mask != rank) {
                uint16_t rank_mask = raw_instr->rank_mask >> 8;
                if (version == 6 || version == 7)
                    rank_mask |= 0xf0;
                thecl_instr_t* new = thecl_instr_rank(rank_mask & 0xff);
                new->offset = (ptrdiff_t)raw_instr - (ptrdiff_t)raw_sub;
                list_append_new(&sub->instrs, new);
                rank = raw_instr->rank_mask;
            }

            thecl_instr_t* instr = thecl_instr_new();
            instr->id = raw_instr->id;
            instr->size = raw_instr->size;
            instr->offset = (ptrdiff_t)raw_instr - (ptrdiff_t)raw_sub;

            const char* format;

            if (raw_instr->id == 0)
                format = "";
            else
                format = th06_find_format(version, raw_instr->id, 0);

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
                    values = value_list_from_data(th06_value_from_data, raw_instr->data, raw_instr->size - sizeof(th06_instr_t), format);
                value_t* value_iter = values;
                uint16_t param_mask = raw_instr->param_mask;

                size_t p = 0;
                char f;
                while (value_iter->type) {
                    f = format[p];
                    thecl_param_t* param = param_new(f);
                    param->value = *value_iter;
                    if (version != 6 && param_mask & 1)
                        param->stack = 1;
                    param_mask >>= 1;
                    ++value_iter;
                    ++p;
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

    th06_insert_labels(ecl);

    thecl_timeline_t* timeline;

    for (size_t e = 0; e < timeline_count; ++e) {
        char name[128];
        sprintf(name, "Timeline%zu", e);

        timeline = malloc(sizeof(*timeline));
        strcpy(timeline->name, name);
        list_init(&timeline->instrs);

        uint16_t time = 0;
        uint8_t rank = version >= 8 ? 0xff : 0x00;
        th06_timeline_instr_t* raw_instr = (th06_timeline_instr_t*)(map + header->offsets[e]);
        while(!th06_check_timeline_sentinel(version, raw_instr)) {
            if (raw_instr->rank != rank) {
                thecl_instr_t* new = thecl_instr_rank(raw_instr->rank);
                list_append_new(&timeline->instrs, new);
                rank = raw_instr->rank;
            }
            if (raw_instr->time != time) {
                thecl_instr_t* new = thecl_instr_time(raw_instr->time);
                list_append_new(&timeline->instrs, new);
                time = raw_instr->time;
            }

            thecl_instr_t* instr = thecl_instr_new();
            instr->size = raw_instr->size;
            instr->id = raw_instr->id;

            const char* format = th06_find_timeline_format(version, raw_instr->id);

            if (!format) {
                fprintf(stderr, "%-3d %04xB: ",
                    raw_instr->id,
                    raw_instr->size
                    );

                for (size_t d = 0; d < raw_instr->size - sizeof(th06_timeline_instr_t); d += 4) {
                    fprintf(stderr, " %08x (%d, %f)",
                        *(uint32_t*)(raw_instr->data + d),
                        *(int32_t*)(raw_instr->data + d),
                        *(float*)(raw_instr->data + d)
                        );
                }

                fprintf(stderr, "\n");
            } else {
                thecl_param_t* param0 = param_new(format[0]);
                param0->value.val.s = raw_instr->arg0;
                list_append_new(&instr->params, param0);

                if (raw_instr->size > sizeof(th06_timeline_instr_t)) {
                    /* The first parameter is in the struct and is always s, so it needs to be ignored here. */
                    value_t* values = value_list_from_data(value_from_data, raw_instr->data, raw_instr->size - sizeof(th06_timeline_instr_t), format + 1);
                    value_t* value_iter = values;
                    while (value_iter->type) {
                        thecl_param_t* param = param_new(value_iter->type);
                        param->value = *value_iter;
                        ++value_iter;
                        list_append_new(&instr->params, param);
                    }
                    free(values);
                }

                list_append_new(&timeline->instrs, instr);
            }

            raw_instr = (th06_timeline_instr_t*)((char*)raw_instr + raw_instr->size);
        }

        list_append_new(&ecl->timelines, timeline);
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
        unsigned int time_last = 0;
        list_for_each(&sub->instrs, instr) {
            switch (instr->type) {
            case THECL_INSTR_TIME:
                fprintf(out, "+%u: //%u\n", instr->time - time_last, instr->time);
                time_last = instr->time;
                break;
            case THECL_INSTR_RANK:
                if(instr->rank == 0xFF) fprintf(out, "!*");
                else if(instr->rank == 0xF0) fprintf(out, "!-");
                else {
                    fprintf(out, "!%s%s%s%s%s%s%s%s",
                        (instr->rank) & RANK_EASY    ? "E" : "",
                        (instr->rank) & RANK_NORMAL  ? "N" : "",
                        (instr->rank) & RANK_HARD    ? "H" : "",
                        (instr->rank) & RANK_LUNATIC ? "L" : "",
                        !((instr->rank) & RANK_ID_4) ? "4" : "",
                        !((instr->rank) & RANK_ID_5) ? "5" : "",
                        !((instr->rank) & RANK_ID_6) ? "6" : "",
                        !((instr->rank) & RANK_ID_7) ? "7" : "");
                }
                break;
            case THECL_INSTR_LABEL:
                fprintf(out, "%s_%u:\n", sub->name, instr->offset);
                break;
            case THECL_INSTR_INSTR: {
                seqmap_entry_t *ent = seqmap_get(g_eclmap->ins_names, instr->id);
                if (ent) {
                    fprintf(out, "    %s(", ent->value);
                }
                else {
                    fprintf(out, "    ins_%u(", instr->id);
                }
                thecl_param_t* param;
                int first = 1;
                list_for_each(&instr->params, param) {
                    if (!first) {
                        fprintf(out, ", ");
                    } else {
                        first = 0;
                    }

                    char* ret = th06_stringify_param(sub, instr, param, ecl->version);
                    fprintf(out, "%s", ret);
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

    thecl_timeline_t* timeline;
    list_for_each(&ecl->timelines, timeline) {
        fprintf(out, "\ntimeline %s()\n{\n", timeline->name);
        thecl_instr_t* instr;
        unsigned int time_last = 0;
        list_for_each(&timeline->instrs, instr) {
            switch(instr->type) {
            case THECL_INSTR_TIME:
                if (instr->time != 0xffff) /* The last ins has to always be 0xffff, so let's not make it relative... */
                    fprintf(out, "+%u: //%u\n", instr->time - time_last, instr->time);
                else
                    fprintf(out, "%u:\n", instr->time);
                time_last = instr->time;
                break;
            case THECL_INSTR_RANK:
                if(instr->rank == 0xFF) fprintf(out, "!*\n");
                else if(instr->rank == 0xF0) fprintf(out, "!-\n");
                else {
                    fprintf(out, "!%s%s%s%s%s%s%s%s\n",
                        (instr->rank) & RANK_EASY    ? "E" : "",
                        (instr->rank) & RANK_NORMAL  ? "N" : "",
                        (instr->rank) & RANK_HARD    ? "H" : "",
                        (instr->rank) & RANK_LUNATIC ? "L" : "",
                        !((instr->rank) & RANK_ID_4) ? "4" : "",
                        !((instr->rank) & RANK_ID_5) ? "5" : "",
                        !((instr->rank) & RANK_ID_6) ? "6" : "",
                        !((instr->rank) & RANK_ID_7) ? "7" : "");
                }
                break;
            case THECL_INSTR_INSTR: {
                seqmap_entry_t *ent = seqmap_get(g_eclmap->timeline_ins_names, instr->id);
                if (ent)
                    fprintf(out, "    %s(", ent->value);
                else
                    fprintf(out, "    ins_%u(", instr->id);

                int first = 1;
                thecl_param_t* param;
                list_for_each(&instr->params, param) {
                    if (!first) {
                        fprintf(out, ", ");
                    } else {
                        first = 0;
                    }

                    char* ret = th06_stringify_param(NULL, instr, param, ecl->version);
                    fprintf(out, "%s", ret);
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
}

static size_t
th06_timeline_instr_size(
    unsigned int version,
    const thecl_instr_t* instr)
{
    (void)version;
    size_t ret = sizeof(th06_timeline_instr_t);
    thecl_param_t* param;
    int first = 1;
    list_for_each(&instr->params, param) {
        /* The first parameter is already a part of the struct. */
        if (first) first = 0;
        else {
            value_t v = param->value;
            v.type = param->type;
            ret += value_size(&v);
        }
    }

    return ret;
}

static size_t
th06_instr_size(
    unsigned int version,
    const thecl_instr_t* instr,
    bool is_timeline)
{
    if (is_timeline) return th06_timeline_instr_size(version, instr);

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
            if (param->type == 'n') {
                ret += sizeof(uint16_t);
            } else if (param->type == 'o' || param->type == 'N')  {
                ret += sizeof(uint32_t);
            } else {
                value_t v = param->value;
                v.type = param->type;
                ret += value_size(&v);
            }
        }
    }

    return ret;
}

static thecl_t*
th06_parse(
    FILE* in,
    char* filename,
    unsigned int version)
{
    parser_state_t state;

    state.instr_time = 0;
    state.instr_rank = 0xff;
    state.instr_flags = 0;
    state.version = version;
    state.uses_numbered_subs = true;
    state.has_overdrive_difficulty = false;
    state.uses_stack_offsets = false;
    list_init(&state.expressions);
    list_init(&state.block_stack);
    list_init(&state.global_definitions);
    state.scope_stack = NULL;
    state.scope_cnt = 0;
    state.scope_id = 0;
    state.current_sub = NULL;
    state.ecl = thecl_new();
    state.ecl->version = version;
    state.instr_format = th06_find_format;
    state.instr_size = th06_instr_size;

    state.path_cnt = 0;
    state.path_stack = NULL;
    path_add(&state, filename);

    yyin = in;

    if (yyparse(&state) != 0)
        return 0;

    global_definition_t* def;
    list_for_each(&state.global_definitions, def) {
        free(def->param);
        free(def);
    }
    list_free_nodes(&state.global_definitions);

    free(state.scope_stack);

    return state.ecl;
}

static uint32_t
th06_find_sub(
    const thecl_t* ecl,
    char* name)
{
    uint32_t n = 0;
    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {
        if (!strcmp(sub->name, name)) return n;
        ++n;
    }
    fprintf(stderr, "%s: sub not found: %s\n", argv0, name);
    return 0;
}

static th06_instr_t*
th06_instr_serialize(
    const thecl_t* ecl,
    thecl_sub_t* sub,
    thecl_instr_t* instr)
{
    th06_instr_t* ret;
    ret = malloc(instr->size);
    ret->time = instr->time;
    ret->id = instr->id;
    ret->size = instr->size;

    if (instr->rank != 0xff) {
        ret->rank_mask = (instr->rank & 0xff) << 8;
        if (ecl->version == 6 || ecl->version == 7)
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
        if (param->type == 'n') {
            uint16_t num = th06_find_sub(ecl, param->value.val.z);
            memcpy(param_data, &num, sizeof(uint16_t));
            param_data += sizeof(uint16_t);
        } else if (param->type == 'N') {
            uint32_t num = th06_find_sub(ecl, param->value.val.z);
            memcpy(param_data, &num, sizeof(uint32_t));
            param_data += sizeof(uint32_t);
        } else if (param->type == 'o') {
            uint32_t label = label_offset(sub, param->value.val.z) - instr->offset;
            memcpy(param_data, &label, sizeof(uint32_t));
            param_data += sizeof(uint32_t);
        } else if (param->value.type == 'z') {
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

static th06_timeline_instr_t*
th06_timeline_instr_serialize(
    const thecl_t* ecl,
    thecl_instr_t* instr)
{
    th06_timeline_instr_t* ret;
    ret = malloc(instr->size);
    ret->time = instr->time;
    ret->id = instr->id;
    ret->size = instr->size;
    ret->rank = ecl->version < 8 ? 0 : instr->rank;

    thecl_param_t* param;
    unsigned char* param_data = ret->data;
    int first = 1;
    list_for_each(&instr->params, param) {
        if (first) {
            if (param->type == 'n')
                ret->arg0 = th06_find_sub(ecl, param->value.val.z);
            else
                ret->arg0 = param->value.val.s;
            first = 0;
        } else {
            /* TODO: implement other param types if necessary */
            /* Or maybe it would be better to make a function that both this
               and th06_instr_serialize would use for writing param data? */
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

    thecl_timeline_t* iter_timeline;
    list_for_each(&ecl->timelines, iter_timeline)
        ++header.timeline_count;

    size_t timeline_count;
    if (ecl->version == 9)
        timeline_count = header.timeline_count;
    else if (ecl->version == 6) {
        timeline_count = 3;
        header.timeline_count = 0;
    } else
        timeline_count = 16;
    const size_t offset_count = timeline_count + header.sub_count;

    if (!file_seekable(out)) {
        fprintf(stderr, "%s: output is not seekable\n", argv0);
        return 0;
    }

    file_seek(out, header_size + offset_count * sizeof(uint32_t));

    offsets = calloc(offset_count, sizeof(*offsets));

    thecl_sub_t* sub;
    uint16_t max_opcode;
    /* TODO: Get max opcodes for the rest of the games */
    switch (ecl->version)
    {
    case 6:
        max_opcode = 135;
        break;
    default:
        max_opcode = 0xFFFFU;
        break;
    }
    size_t s = 0;
    list_for_each(&ecl->subs, sub) {
        offsets[timeline_count + s] = file_tell(out);

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr) {
            th06_instr_t* raw_instr = th06_instr_serialize(ecl, sub, instr);
            file_write(out, raw_instr, raw_instr->size);
            if (raw_instr->id > max_opcode) {
                fprintf(stderr, "%s: warning: opcode: id %hu was higher than the maximum %hu\n", argv0, raw_instr->id, max_opcode);
            }
            free(raw_instr);
        }

        file_write(out, &sentinel, sizeof(sentinel));

        ++s;
    }

    char timeline_sentinel_th6[4] = {0xff, 0xff, 0x04, 0x00};
    char timeline_sentinel_th8[8] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00};

    size_t o = 0;
    thecl_sub_t* timeline;
    list_for_each(&ecl->timelines, timeline) {
        offsets[o++] = file_tell(out);

        thecl_instr_t* instr;
        list_for_each(&timeline->instrs, instr) {
            th06_timeline_instr_t* raw_instr = th06_timeline_instr_serialize(ecl, instr);

            file_write(out, raw_instr, raw_instr->size);
            free(raw_instr);
        }

        file_write(out, ecl->version < 8 ? timeline_sentinel_th6 : timeline_sentinel_th8, ecl->version < 8 ? 4 : 8);
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
    .create_header = NULL
};
