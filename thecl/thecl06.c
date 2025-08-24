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
#include "cp932.h"

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

// v1/v2 suffixes are used to prevent accidental use of
// the wrong field when translating from internal format
typedef struct {
PACK_BEGIN
    union {
        struct {
            int16_t time_v1;
            int16_t arg0;
        };
        int32_t time_v2;
    };
    uint16_t id;
    union {
        struct {
            uint8_t size_v2;
            uint8_t rank;
        };
        int16_t size_v1;
    };
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
    } else if (type == 'o' || type == 't' || type == 'N' || type == 'T') {
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
    } else if (type == 'o' || type == 't' || type == 'N' || type == 'T') {
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
    } else if (type == 'o' || type == 't' || type == 'N' || type == 'T') {
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
        char *zstr = param->value.val.z;
        if (g_ecl_encode_cp932)
            zstr = cp932_to_utf8(malloc(cp932_to_utf8_len(zstr) + 1), zstr);
        const size_t zlen = strlen(zstr);
        char* ret = malloc(4 + zlen * 2);
        char* temp = ret;
        *temp++ = '"';
        for (size_t z = 0; z < zlen; ++z) {
            if (!zstr[z])
                break;
            if (zstr[z] == '"' || zstr[z] == '\\')
                *temp++ = '\\';
            *temp++ = zstr[z];
        }
        *temp++ = '"';
        *temp++ = '\0';
        if (g_ecl_encode_cp932)
            free(zstr);
        return ret;
    } else
        return value_to_text(&param->value);
}

static char*
th06_stringify_param(
    const char* sub_name,
    const thecl_instr_t* instr,
    const thecl_param_t* param,
    unsigned int version)
{
    char temp[512];
    switch(param->type) {
        // Some instructions use -1 as "none" instead of a sub ID
        case 'n':
            snprintf(temp, 512, param->value.val.s >= 0 ? "\"Sub%d\"" : "%d", param->value.val.s);
            return strdup(temp);
        case 'N':
            snprintf(temp, 512, param->value.val.S >= 0 ? "\"Sub%d\"" : "%d", param->value.val.S);
            return strdup(temp);
        case 'T':
            snprintf(temp, 512, param->value.val.S >= 0 ? "\"Timeline%d\"" : "%d", param->value.val.S);
            return strdup(temp);
        case 'o':
            snprintf(temp, 512, "%s_%d", sub_name, instr->offset + param->value.val.S);
            return strdup(temp);
        case 't':
            snprintf(temp, 512, "%d", param->value.val.S);
            return strdup(temp);
        default:
            switch (param->value.type) {
                case 'S': case 's': case 'U': case 'u': case 'C': case 'f':
                    if (param->stack || version == 6) {
                        int val;
                        switch (param->value.type) {
                            case 'S': case 'U': case 'C':
                                val = param->value.val.S;
                                break;
                            case 's':
                                val = param->value.val.s;
                                break;
                            case 'u':
                                val = param->value.val.u;
                                break;
                            case 'f':
                                val = (int)floorf(param->value.val.f);
                                break;
                        }
                        
                        seqmap_entry_t * ent = seqmap_get(g_eclmap->gvar_names, val);
                        if (ent) {
                            snprintf(temp, 256, "%c%s", param->value.type == 'f' ? '%' : '$', ent->value);
                            return strdup(temp);
                        }
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
    {1, "nfff"},
    {2, "nfffssS"},
    {3, "nfff"},
    {4, "nfffssS"},
    {5, "nfff"},
    {6, "nfffssS"},
    {7, "nfff"},
    {8, "s"},
    {9, ""},
    {10, "SS"},
    {11, "u"},
    {12, "s"},
    { 0, 0 },
};

static const id_format_pair_t th07_timeline_fmts[] = {
    {0, "nfffSSS"},
    {2, "nfffSSS"},
    {4, "nfffSSS"},
    {6, "nfffSSS"},
    {11, "s"},
    { 0, 0 },
};

static const id_format_pair_t th08_timeline_fmts[] = {
    { 0, "NffSSS"},
    { 1, "NffSSS"},
    { 2, "NfffSSS"},
    { 3, "NfSSS"},
    { 4, "NfffSSS"},
    { 5, "NfSSS"},
    { 6, "S"},
    { 7, ""},
    { 8, "Sss"}, // "Ss--"
    { 9, "S" },
    { 10, "S"},
    { 11, "NffSSSS"},
    { 12, "NffSSSS"},
    { 13, "T"},
    { 14, "T"},
    { 15, "NffSSS"},
    { 16, ""},
    { 0, 0 },
};

static const id_format_pair_t th09_timeline_fmts[] = {
    { 8, "SS"},
    { 17, "NffSSS"},
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
        - 't' - 'S' used as a time
        - 'N' - 'S' used as a sub name string (in call/enm creation instructions)
        - 'n' - 's' used as a sub name string
        - 'T' - 'S' used as a timeline name string
    */
    { 0, "" },
    { 1, "S" }, // "_"
    { 2, "to" },
    { 3, "toS" },
    { 4, "SS" },
    { 5, "Sf" },
    { 6, "SU" },
    { 7, "SUS" },
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
    { 29, "to" },
    { 30, "to" },
    { 31, "to" },
    { 32, "to" },
    { 33, "to" },
    { 34, "to" },
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
    { 85, "ssffffffSSSSSS" },
    { 86, "ssffffffSSSSSS" },
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
    { 98, "ssssS" }, // "sssss--"
    { 99, "SS" },
    { 100, "S" }, // "bbb-"
    { 101, "S" },
    { 102, "Sffff" },
    { 103, "fff" },
    { 104, "S" }, // "b---"
    { 105, "S" }, // "b---"
    { 106, "S" },
    { 107, "S" }, // "b---"
    { 108, "N" },
    { 109, "NS" },
    { 110, "S" },
    { 111, "S" },
    { 112, "S" },
    { 113, "S" },
    { 114, "N" },
    { 115, "S" },
    { 116, "N" },
    { 117, "S" }, // "b---"
    { 118, "SUC" },
    { 119, "S" },
    { 120, "S" }, // "b---"
    { 121, "SS" },
    { 122, "S" },
    { 123, "S" },
    { 124, "S" },
    { 125, "" },
    { 126, "S" },
    { 127, "S" },
    { 128, "S" }, // "s--"
    { 129, "SS" }, // "Ss--"
    { 130, "S" }, // "b---"
    { 131, "ffSSSS" },
    { 132, "S" }, // "b---"
    { 133, "" },
    { 134, "" },
    { 135, "S" }, // "b---"
    { 0, 0 },
};

static const id_format_pair_t th07_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 2, "to" },
    { 3, "toS" },
    { 4, "SS" },
    { 5, "ff" },
    { 6, "SU" },
    { 7, "SUS" },
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
    { 23, "fff" },
    { 24, "ff" },
    { 25, "ff" },
    { 26, "fffff" },
    { 27, "fSSSffff" },
    { 28, "SSto" },
    { 29, "ffto" },
    { 30, "SSto" },
    { 31, "ffto" },
    { 32, "SSto" },
    { 33, "ffto" },
    { 34, "SSto" },
    { 35, "ffto" },
    { 36, "SSto" },
    { 37, "ffto" },
    { 38, "SSto" },
    { 39, "ffto" },
    { 40, "f" },
    { 41, "N" },
    { 42, "" },
    { 43, "SSS" },
    { 44, "ffS" },
    { 45, "S" },
    { 46, "fff" },
    { 47, "fff" },
    { 48, "f" },
    { 49, "f" },
    { 50, "f" },
    { 51, "fff" },
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
    { 90, "sum" },
    { 91, "" },
    { 92, "NfffSSS" },
    { 93, "NfffSSS" },
    { 94, "" },
    { 95, "S" },
    { 96, "ssssS" }, // "sssss--"
    { 97, "SS" },
    { 98, "S" }, // "cbb-"
    { 99, "S" },
    { 100, "Sffff" },
    { 101, "fff" },
    { 102, "S" }, // "b---"
    { 103, "S" }, // "b---"
    { 104, "S" }, // "b---"
    { 105, "S" },
    { 106, "S" }, // "b---"
    { 107, "N" }, // "b---" but b is a byte N
    { 108, "NS" },
    { 109, "S" },
    { 110, "S" },
    { 111, "S" },
    { 112, "S" },
    { 113, "N" },
    { 114, "S" },
    { 115, "N" },
    { 116, "S" }, // "b---"
    { 117, "SUC" },
    { 118, "SUCfff" },
    { 119, "S" },
    { 120, "S" }, // "b---"
    { 121, "SS" },
    { 122, "SS" },
    { 123, "S" },
    { 124, "S" },
    { 125, "S" },
    { 126, "S" },
    { 127, "S" },
    { 128, "S" },
    { 129, "SS" }, // "Ss--"
    { 130, "S" }, // "b---"
    { 131, "ffSSSS" },
    { 132, "S" }, // "b---"
    { 133, "" },
    { 134, "" },
    { 135, "S" }, // "b---"
    { 136, "S" }, // "b---"
    { 137, "S" }, // "b---"
    { 138, "SSSS" }, // "b---SSS"
    { 139, "SSSC" },
    { 140, "ffff" },
    { 141, "S" },
    { 142, "S" },
    { 143, "f" },
    { 144, "SN" },
    { 145, "SS" },
    { 146, "" },
    { 147, "S" },
    { 148, "SSN" },
    { 149, "Sfff" },
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
    { 4, "to" },
    { 5, "toS" },
    { 6, "SS" },
    { 7, "ff" },
    { 8, "SS" },
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
    { 29, "fff" },
    { 30, "S" },
    { 31, "S" },
    { 32, "ff" },
    { 33, "ff" },
    { 34, "fffff" },
    { 35, "ffff" },
    { 36, "fSSSffff" },
    { 37, "f" },
    { 38, "ffff" },
    { 39, "fffff" },
    { 40, "SSto" },
    { 41, "ffto" },
    { 42, "SSto" },
    { 43, "ffto" },
    { 44, "SSto" },
    { 45, "ffto" },
    { 46, "SSto" },
    { 47, "ffto" },
    { 48, "SSto" },
    { 49, "ffto" },
    { 50, "SSto" },
    { 51, "ffto" },
    { 52, "N" },
    { 53, "" },
    { 54, "S" },
    { 55, "S" },
    { 56, "SSSSSS" },
    { 57, "SS" },
    { 58, "S" },
    { 59, "S" },
    { 60, "SSSSSS" },
    { 61, "SS" },
    { 62, "" },
    { 63, "ff" },
    { 64, "SSff" },
    { 65, "ff" },
    { 66, "SSff" },
    { 67, "SSf" },
    { 68, "ff" },
    { 69, "SSff" },
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
    { 88, "SN" },
    { 89, "SS" },
    { 90, "NffSSS" },
    { 91, "NffSSS" },
    { 92, "NffSSS" },
    { 93, "NfffSSS" },
    { 94, "NfffSSS" },
    { 95, "" },
    { 96, "ssSSffffS" },
    { 97, "ssSSffffS" },
    { 98, "ssSSffffS" },
    { 99, "ssSSffffS" },
    { 100, "ssSSffffS" },
    { 101, "ssSSffffS" },
    { 102, "ssSSffffS" },
    { 103, "ssSSffffS" },
    { 104, "ssSSffffS" },
    { 105, "S" },
    { 106, "S" },
    { 107, "" },
    { 108, "" },
    { 109, "" },
    { 110, "ff" },
    { 111, "SSSSSff" },
    { 112, "" },
    { 113, "SS" },
    { 114, "ssffffffSSSSSS" },
    { 115, "ssffffffSSSSSS" },
    { 116, "S" },
    { 117, "Sf" },
    { 118, "Sf" },
    { 119, "Sfff" },
    { 120, "S" },
    { 121, "S" },
    { 122, "suSmmmm" },
    { 123, "" },
    { 124, "S" },
    { 125, "S" },
    { 126, "NS" },
    { 127, "S" },
    { 128, "Sffff" },
    { 129, "S" }, // "b---"
    { 130, "N" }, // "n--"
    { 131, "S" },
    { 132, "S" },
    { 133, "SSN" },
    { 134, "SN" },
    { 135, "SN" },
    { 136, "SS" },
    { 137, "SS" },
    { 138, "S" }, // "cbb-"
    { 139, "SUC" },
    { 140, "SUCfff" },
    { 141, "S" },
    { 142, "S" },
    { 143, "S" },
    { 144, "SS" },
    { 145, "S" }, // "b---"
    { 146, "S" },
    { 147, "S" },
    { 148, "S" },
    { 149, "S" },
    { 150, "SS" }, // "Ss--"
    { 151, "S" }, // "b---"
    { 152, "ffSSSS" },
    { 153, "" },
    { 154, "" },
    { 155, "S" }, // "b---"
    { 156, "S" }, // "b---"
    { 157, "SSSS" }, // "b---SSS"
    { 158, "SSSC" },
    { 159, "S" },
    { 160, "S" },
    { 161, "f" },
    { 162, "" },
    { 163, "S" },
    { 164, "Sfff" },
    { 165, "f" },
    { 166, "ffff" },
    { 167, "Sf" },
    { 168, "S" },
    { 169, "f" },
    { 170, "SS" },
    { 171, "Sf" },
    { 172, "Sff" },
    { 173, "S" },
    { 174, "S" },
    { 175, "S" },
    { 176, "S" }, // "b---"
    { 177, "S" },
    { 178, "SSf" },
    { 179, "" },
    { 180, "" },
    { 181, "" },
    { 182, "S" },
    { 183, "S" },
    { 184, "S" },
    { 0, 0 },
};

static const id_format_pair_t th09_fmts[] = {
    { 185, "S" },
    { 186, "" },
    { 187, "S" },
    { 0, 0 },
};

static const id_format_pair_t th95_fmts[] = {
    { 83, "N" },
    { 84, "Nff" }, // Technically the game reads "Nfff" but all instructions are missing the final argument
    { 86, "ssSSffffS" },
    { 87, "ssSSffffS" },
    { 88, "ssSSffffS" },
    { 89, "ssSSffffS" },
    { 90, "ssSSffffS" },
    { 91, "ssSSffffS" },
    { 94, "ssSSffffS" },
    { 95, "S" },
    { 96, "S" },
    { 97, "" },
    { 98, "" },
    { 99, "" },
    { 100, "ff" },
    { 101, "SSSSSff" },
    { 102, "" },
    { 103, "SS" },
    { 104, "m" },
    { 105, "" },
    { 106, "" },
    { 108, "NS" },
    { 109, "S" },
    { 112, "N" }, // "n--"
    { 113, "S" },
    { 114, "S" },
    { 115, "SSN" },
    { 116, "SN" },
    { 117, "SN" },
    { 118, "S" },
    { 119, "S" },
    { 120, "S" }, // "b---"
    { 126, "S" }, // "b---"
    { 128, "" },
    { 130, "S" }, // "b---"
    { 131, "SSSS" }, // "b---SSS"
    { 133, "S" },
    { 136, "ffff" },
    { 137, "f" },
    { 138, "S" },
    { 139, "S" },
    { 140, "S" },
    { 142, "" },
    { 143, "SS" },
    { 144, "S" },
    { 145, "SSffff" },
    { 146, "SSffff" },
    { 147, "SSfffSSSSfS" },
    { 148, "SSfffSSSSfS" },
    { 149, "f" },
    { 150, "S" },
    { 151, "SS" },
    { 152, "fS" },
    { 153, "SSfffSSSSfS" },
    { 154, "SSfffSSSSfS" },
    { 155, "SSfffSSSSfSff" },
    { 156, "SSfffSSSSfSff" },
    { 157, "SSfffSSSSfSff" },
    { 158, "S" },
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
    if (is_pre_th8(version))
        return raw_instr->time_v1 == -1 && raw_instr->arg0 == 4;
    else
        return raw_instr->time_v2 == -1 && raw_instr->size_v2 == 0x00;
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
    if (!is_pre_th8(version))
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
            instr->address = instr->offset + header->offsets[timeline_count_max + s];

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

        bool is_timeline_v2 = !is_pre_th8(version);
        int32_t time = 0;
        uint8_t rank = 0xff;
        th06_timeline_instr_t* raw_instr = (th06_timeline_instr_t*)(map + header->offsets[e]);
        while(!th06_check_timeline_sentinel(version, raw_instr)) {
            if (is_timeline_v2) {
                if (raw_instr->rank != rank) {
                    thecl_instr_t* new = thecl_instr_rank(raw_instr->rank);
                    list_append_new(&timeline->instrs, new);
                    rank = raw_instr->rank;
                }
            }
            int32_t ins_time = is_timeline_v2 ? raw_instr->time_v2 : raw_instr->time_v1;
            if (ins_time != time) {
                thecl_instr_t* new = thecl_instr_time(ins_time);
                list_append_new(&timeline->instrs, new);
                time = ins_time;
            }

            thecl_instr_t* instr = thecl_instr_new();
            int32_t ins_size = is_timeline_v2 ? raw_instr->size_v2 : raw_instr->size_v1;
            instr->size = ins_size;
            instr->id = raw_instr->id;

            const char* format = th06_find_timeline_format(version, raw_instr->id);

            if (!format) {
                fprintf(stderr, "%-3d %04xB: ",
                    raw_instr->id,
                    ins_size
                    );

                for (size_t d = 0; d < ins_size - sizeof(th06_timeline_instr_t); d += 4) {
                    fprintf(stderr, " %08x (%d, %f)",
                        *(uint32_t*)(raw_instr->data + d),
                        *(int32_t*)(raw_instr->data + d),
                        *(float*)(raw_instr->data + d)
                        );
                }

                fprintf(stderr, "\n");
            } else {
                if (!is_timeline_v2) {
                    switch (format[0]) {
                        /* Ignore the first parameter if it is small enough to fit inside the struct */
                        case 'n': case 's': case 'u': case 'b': case 'c':
                        {
                            thecl_param_t* param0 = param_new(format[0]);
                            param0->value.val.s = raw_instr->arg0;
                            list_append_new(&instr->params, param0);
                            ++format;
                        }
                    }
                }
                if (ins_size > sizeof(th06_timeline_instr_t)) {
                    value_t* values = value_list_from_data(th06_value_from_data, raw_instr->data, ins_size - sizeof(th06_timeline_instr_t), format);
                    value_t* value_iter = values;
                    while (value_iter->type) {
                        thecl_param_t* param = param_new(*format++);
                        param->value = *value_iter;
                        ++value_iter;
                        list_append_new(&instr->params, param);
                    }
                    free(values);
                }

                list_append_new(&timeline->instrs, instr);
            }

            raw_instr = (th06_timeline_instr_t*)((char*)raw_instr + ins_size);
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
                else if(instr->rank == get_default_none_rank(ecl->version)) fprintf(out, "!-");
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
                if (g_ecl_hexdebug) {
                    fprintf(out, "    /* %5x (+%5x) */ ", instr->address, instr->offset);
                } else {
                    fprintf(out, "    ");
                }
                seqmap_entry_t *ent = seqmap_get(g_eclmap->ins_names, instr->id);
                if (ent) {
                    fprintf(out, "%s(", ent->value);
                } else {
                    fprintf(out, "ins_%u(", instr->id);
                }
                thecl_param_t* param;
                int first = 1;
                list_for_each(&instr->params, param) {
                    if (!first) {
                        fprintf(out, ", ");
                    } else {
                        first = 0;
                    }

                    char* ret = th06_stringify_param(sub->name, instr, param, ecl->version);
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
    bool is_timeline_v2 = !is_pre_th8(ecl->version);
    list_for_each(&ecl->timelines, timeline) {
        fprintf(out, "\ntimeline %s()\n{\n", timeline->name);
        thecl_instr_t* instr;
        int32_t time_last = 0;
        list_for_each(&timeline->instrs, instr) {
            switch(instr->type) {
            case THECL_INSTR_TIME: {
                int32_t ins_time = is_timeline_v2 ? instr->time : (int16_t)instr->time;
                if (ins_time != -1) /* The last ins has to always be -1, so let's not make it relative... */
                    fprintf(out, "+%d: //%d\n", ins_time - time_last, ins_time);
                else
                    fprintf(out, "%d:\n", ins_time);
                time_last = ins_time;
                break;
            }
            case THECL_INSTR_RANK:
                if (is_timeline_v2) {
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

                    char* ret = th06_stringify_param(timeline->name, instr, param, ecl->version);
                    if (ret) {
                        fprintf(out, "%s", ret);
                        free(ret);
                    }
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
    size_t ret = sizeof(th06_timeline_instr_t);
    thecl_param_t* param;
    int first = is_pre_th8(version);
    list_for_each(&instr->params, param) {
        /* The first parameter is already a part of the struct. */
        if (first) {
            first = 0;
            switch (param->type) {
                // All types that fit within arg0
                case 'n': case 's': case 'u': case 'b': case 'c':
                    continue;
            }
        }
        switch (param->type) {
            case 'n':
                ret += sizeof(uint16_t);
                break;
            case 'o': case 't': case 'N': case 'T':
                ret += sizeof(uint32_t);
                break;
            case 'z':
                if (g_ecl_encode_cp932) {
                    ret += utf8_to_cp932_len(param->value.val.z);
                    break;
                }
            default: {
                value_t v = param->value;
                v.type = param->type;
                ret += value_size(&v);
            }
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
            } else if (param->type == 'o' || param->type == 't' || param->type == 'N')  {
                ret += sizeof(uint32_t);
            } else if (param->type == 'z' && g_ecl_encode_cp932) {
                ret += utf8_to_cp932_len(param->value.val.z);
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
    state.has_numeric_difficulties = false;
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

    path_init(&state.path_state, filename, argv0);

    thecl_yyin = in;

    if (thecl_yyparse(&state) != 0)
        return 0;

    path_free(&state.path_state);

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
    if ((uintptr_t)name == UINT32_MAX) {
        return -1;
    }
    int32_t n = 0;
    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {
        if (!strcmp(sub->name, name)) return n;
        ++n;
    }
    fprintf(stderr, "%s: sub not found: %s\n", argv0, name);
    return 0;
}

static uint32_t
th06_find_timeline(
    const thecl_t* ecl,
    char* name)
{
    if ((uintptr_t)name == UINT32_MAX) {
        return -1;
    }
    int32_t n = 0;
    thecl_sub_t* timeline;
    list_for_each(&ecl->timelines, timeline) {
        if (!strcmp(timeline->name, name)) return n;
        ++n;
    }
    fprintf(stderr, "%s: timeline not found: %s\n", argv0, name);
    return 0;
}

static ssize_t
th06_serialize_data(
    const thecl_t* ecl,
    thecl_sub_t* sub,
    thecl_instr_t* instr,
    thecl_param_t* param,
    unsigned char* param_data,
    int param_count,
    size_t data_size,
    bool is_timeline)
{
    value_t v;
    switch (param->type) {
        case 'n':
            if (data_size < sizeof(int16_t))
                return -1;
            *(int16_t*)param_data = th06_find_sub(ecl, param->value.val.z);
            return sizeof(int16_t);
        case 'N':
            if (data_size < sizeof(int32_t))
                return -1;
            *(int32_t*)param_data = th06_find_sub(ecl, param->value.val.z);
            return sizeof(int32_t);
        case 'T':
            if (data_size < sizeof(int32_t))
                return -1;
            *(int32_t*)param_data = th06_find_timeline(ecl, param->value.val.z);
            return sizeof(int32_t);
        case 'o':
            if (data_size < sizeof(int32_t))
                return -1;
            *(int32_t*)param_data = label_offset(sub, param->value.val.z);
            return sizeof(int32_t);
        case 't':
            if (data_size < sizeof(int32_t))
                return -1;
            *(int32_t*)param_data = param->value.val.S;
            return sizeof(int32_t);
        case 'z': case 'm': case 'x':
            if (!is_timeline) {
                char *zstr = param->value.val.z;
                if (g_ecl_encode_cp932)
                    zstr = utf8_to_cp932(malloc(utf8_to_cp932_len(zstr) + 1), zstr);
                switch (ecl->version) {
                    case 6:
                        strncpy((char*)param_data, zstr, 34);
                        if (g_ecl_encode_cp932)
                            free(zstr);
                        return 34;
                    case 7: case 95:
                        strncpy((char*)param_data, zstr, 48);
                        util_xor(param_data, 48, 0xaa, 0, 0);
                        if (g_ecl_encode_cp932)
                            free(zstr);
                        return 48;
                    case 8:
                        switch (param_count) {
                            case 4: case 5:
                                strncpy((char*)param_data, zstr, 48);
                                util_xor(param_data, 48, param_count == 4 ? 0xaa : 0xbb, 0, 0);
                                if (g_ecl_encode_cp932)
                                    free(zstr);
                                return 48;
                            case 6: case 7:
                                strncpy((char*)param_data, zstr, 64);
                                util_xor(param_data, 64, param_count == 6 ? 0xdd : 0xee, 0, 0);
                                if (g_ecl_encode_cp932)
                                    free(zstr);
                                return 64;
                        }
                        break;
                }
            }
            return -1;
        case 'S':
            //if (data_size == sizeof(int16_t) && param->value.val.S == param->value.val.s) {
                //v.type = 's';
            //} else {
        default:
                v.type = param->type;
            //}
            v.val = param->value.val;
            return value_to_data(&v, param_data, data_size);
    }
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
        if (is_pre_th8(ecl->version))
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
        ssize_t param_size = th06_serialize_data(ecl, sub, instr, param, param_data, param_count, instr->size - (param_data - (unsigned char*)ret), false);
        if (param_size != -1) {
            param_data += param_size;
        }
    }

    return ret;
}

static th06_timeline_instr_t*
th06_timeline_instr_serialize(
    const thecl_t* ecl,
    thecl_sub_t* timeline,
    thecl_instr_t* instr)
{
    th06_timeline_instr_t* ret;
    ret = malloc(instr->size);
    int first;
    if (is_pre_th8(ecl->version)) {
        first = 1;
        ret->time_v1 = instr->time;
        ret->arg0 = 0; // Default value
        ret->size_v1 = instr->size;
    } else {
        first = 0;
        ret->time_v2 = instr->time;
        ret->size_v2 = instr->size;
        ret->rank = instr->rank;
    }
    ret->id = instr->id;

    thecl_param_t* param;
    unsigned char* param_data = ret->data;
    list_for_each(&instr->params, param) {
        if (first) {
            first = 0;
            switch (param->type) {
                // All types that fit within arg0
                case 'n': case 's': case 'u':
                    th06_serialize_data(ecl, timeline, instr, param, (unsigned char*)&ret->arg0, 0, sizeof(uint16_t), true);
                case 'b': case 'c':
                    th06_serialize_data(ecl, timeline, instr, param, (unsigned char*)&ret->arg0, 0, sizeof(uint8_t), true);
                    continue;
            }
        }
        ssize_t param_size = th06_serialize_data(ecl, timeline, instr, param, param_data, 0, instr->size - (param_data - (unsigned char*)ret), true);
        if (param_size != -1) {
            param_data += param_size;
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

    const char timeline_sentinel_th6[4] = {0xff, 0xff, 0x04, 0x00};
    const char timeline_sentinel_th8[8] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00};

    size_t o = 0;
    thecl_sub_t* timeline;
    bool timeline_v2 = !is_pre_th8(ecl->version);
    list_for_each(&ecl->timelines, timeline) {
        offsets[o++] = file_tell(out);

        thecl_instr_t* instr;
        list_for_each(&timeline->instrs, instr) {
            th06_timeline_instr_t* raw_instr = th06_timeline_instr_serialize(ecl, timeline, instr);

            file_write(out, raw_instr, timeline_v2 ? raw_instr->size_v2 : raw_instr->size_v1);
            free(raw_instr);
        }

        if (timeline_v2) {
            file_write(out, timeline_sentinel_th8, sizeof(timeline_sentinel_th8));
        } else {
            file_write(out, timeline_sentinel_th6, sizeof(timeline_sentinel_th6));
        }
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
