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
    char magic[4];
    uint16_t unknown1; /* 1 */
    uint16_t include_length; /* include_offset + ANIM+ECLI length */
    uint32_t include_offset; /* sizeof(th10_header_t) */
    uint32_t zero1;
    uint32_t sub_count;
    uint32_t zero2[4];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th10_header_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    char magic[4];
    uint32_t count;
    unsigned char data[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th10_list_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    char magic[4];
    uint32_t data_offset; /* sizeof(th10_sub_t) */
    uint32_t zero[2];
    unsigned char data[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th10_sub_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint32_t time;
    uint16_t id;
    uint16_t size;
    uint16_t param_mask;
    /* The rank bitmask.
     *   1111LHNE
     * Bits mean: easy, normal, hard, lunatic. The rest are always set to 1. */
    uint8_t rank_mask;
    /* There doesn't seem to be a way of telling how many parameters there are
     * from the additional data. */
    uint8_t param_count;
    /* For TH13 this field stores the number of current stack references in the
     * parameter list. */
    uint32_t zero;
    unsigned char data[];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th10_instr_t;

static ssize_t
th10_value_from_data(
    const unsigned char* data,
    size_t data_length,
    char type,
    value_t* value)
{
    switch (type) {
    case 'D':
        return value_from_data(data, 2 * sizeof(uint32_t), 'm', value);
    case 'o':
        return value_from_data(data, data_length, 'S', value);
    case 'm':
    case 'x': {
        uint32_t length;
        memcpy(&length, data, sizeof(uint32_t));
        value_t temp;

        value_from_data(data + sizeof(length), length, 'm', &temp);
        if (type == 'x')
            util_xor(temp.val.m.data, length, 0x77, 7, 16);
        value->type = 'z';
        value->val.z = (char*)temp.val.m.data;

        return sizeof(uint32_t) + length;
    }
    default:
        return value_from_data(data, data_length, type, value);
    }
}

static char*
th10_param_to_text(
    const thecl_param_t* param)
{
    switch (param->type) {
    case 'f': {
        char* ret = malloc(1024);
        sprintf(ret, "%sf", util_printfloat(&param->value.val.f));
        return ret;
    }
    case 'm':
    case 'x': {
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
    }
    default:
        return value_to_text(&param->value);
    }
}

#define RANK_EASY    (1 << 0)
#define RANK_NORMAL  (1 << 1)
#define RANK_HARD    (1 << 2)
#define RANK_LUNATIC (1 << 3)

static const id_format_pair_t th10_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 10, "" },
    { 11, "m*D" },
    { 12, "oS" },
    { 13, "oS" },
    { 14, "oS" },
    { 15, "m*D" },
    { 16, "mS" },
    { 17, "S" },
    { 21, "" },
    { 40, "S" },
    { 42, "S" },
    { 43, "S" },
    { 44, "f" },
    { 45, "f" },
    { 50, "" },
    { 51, "" },
    { 52, "" },
    { 53, "" },
    { 54, "" },
    { 55, "" },
    { 56, "" },
    { 57, "" },
    { 58, "" },
    { 59, "" },
    { 61, "" },
    { 63, "" },
    { 64, "" },
    { 65, "" },
    { 67, "" },
    { 68, "" },
    { 69, "" },
    { 70, "" },
    { 71, "" },
    { 73, "" },
    { 74, "" },
    { 78, "S" },
    { 79, "" },
    { 81, "ffff" },
    { 82, "f" },
    { 83, "S" },
    { 85, "" },
    { 256, "mffSSS" },
    { 257, "mffSSS" },
    { 258, "S" },
    { 259, "SS" },
    { 260, "mffSSS" },
    { 261, "mffSSS" },
    { 262, "SS" },
    { 263, "SS" },
    { 264, "SS" },
    { 265, "mffSSS" },
    { 266, "mffSSS" },
    { 267, "mffSSS" },
    { 268, "mffSSS" },
    { 269, "S" },
    { 270, "mfffSSS" },
    { 272, "SS" },
    { 273, "SSf" },
    { 280, "ff" },
    { 281, "SSff" },
    { 282, "ff" },
    { 283, "SSfS" },
    { 284, "ff" },
    { 285, "SSff" },
    { 286, "ff" },
    { 287, "SSff" },
    { 288, "ffff" },
    { 290, "ffff" },
    { 291, "SSfffS" },
    { 292, "SSf" },
    { 294, "" },
    { 296, "SSf" },
    { 298, "ff" },
    { 299, "ff" },
    { 320, "ff" },
    { 321, "ff" },
    { 322, "S" },
    { 323, "S" },
    { 324, "Sfff" },
    { 325, "" },
    { 326, "" },
    { 327, "SS" },
    { 328, "ff" },
    { 329, "" },
    { 330, "S" },
    { 331, "S" },
    { 332, "S" },
    { 333, "" },
    { 334, "SSSm" },
    { 335, "S" },
    { 336, "S" },
    { 337, "SSS" },
    { 338, "S" },
    { 339, "" },
    { 340, "" },
    { 341, "Sm" },
    { 342, "SSSx" },
    { 343, "" },
    { 344, "S" },
    { 345, "" },
    { 346, "f" },
    { 347, "SfS" },
    { 355, "SSSSS" },
    { 356, "fffff" },
    { 357, "SSSx" },
    { 359, "SSSx" },
    { 360, "S" },
    { 361, "S" },
    { 362, "" },
    { 363, "" },
    { 364, "S" },
    { 365, "" },
    { 366, "SS" },
    { 367, "f" },
    { 368, "SSSS" },
    { 400, "S" },
    { 401, "S" },
    { 402, "SSS" },
    { 403, "Sff" },
    { 404, "Sff" },
    { 405, "Sff" },
    { 406, "SSS" },
    { 407, "SS" },
    { 409, "SSSSSSff" },
    { 410, "" },
    { 411, "SS" },
    { 412, "SSffffSf" },
    { 413, "SSSfffSSSSfS" },
    { 420, "f" },
    { 421, "f" },
    { 425, "SSSSSSS" },
    { 428, "SSffSfSf" },
    { 431, "SSffSfff" },
    { 433, "SSffSfff" },
    { 435, "Sffffffff" },
    { 436, "SSSSSSSSS" },
    { -1, NULL }
};

static const id_format_pair_t th11_fmts[] = {
    { 66, "" },
    { 275, "fS" },
    { 276, "" },
    { 277, "ff" },
    { 278, "S" },
    { 289, "SSfff" },
    { 291, "SSffS" },
    { 300, "ffffff" },
    { 302, "ffffff" },
    { 305, "Sffffff" },
    { 307, "" },
    { 369, "S" },
    { 370, "S" },
    { 371, "S" },
    { 408, "SSS" },
    { 437, "Sff" },
    { 438, "Sf" },
    { 439, "Sff" },
    { 440, "fS" },
    { 441, "S" },
    { 442, "S" },
    { 443, "S" },
    { 444, "S" },
    { 445, "S" },
    { 447, "f" },
    { 448, "S" },
    { 449, "S" },
    { 450, "S" },
    { 500, "S" },
    { -1, NULL }
};

static const id_format_pair_t th12_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 10, "" },
    { 11, "m*D" },
    { 12, "oS" },
    { 13, "oS" },
    { 14, "oS" },
    { 15, "m*D" },
    { 16, "mS*D" },
    { 17, "f" },
    { 21, "" },
    { 40, "S" },
    { 42, "S" },
    { 43, "S" },
    { 44, "f" },
    { 45, "f" },
    { 50, "" },
    { 51, "" },
    { 52, "" },
    { 53, "" },
    { 54, "" },
    { 55, "" },
    { 56, "" },
    { 57, "" },
    { 58, "" },
    { 59, "" },
    { 60, "" },
    { 61, "" },
    { 63, "" },
    { 64, "" },
    { 65, "" },
    { 66, "" },
    { 67, "" },
    { 68, "" },
    { 69, "" },
    { 70, "" },
    { 73, "" },
    { 74, "" },
    { 78, "S" },
    { 81, "ffff" },
    { 82, "f" },
    { 83, "S" },
    { 86, "fff" },
    { 87, "fffff" },
    { 89, "fff" },
    { 256, "mffSSS" },
    { 257, "mffSSS" },
    { 258, "S" },
    { 259, "SS" },
    { 260, "mffSSS" },
    { 261, "mffSSS" },
    { 262, "SS" },
    { 263, "SS" },
    { 264, "fS" },
    { 265, "mffSSS" },
    { 267, "mffSSS" },
    { 269, "f" },
    { 270, "mffSSSS" },
    { 273, "SSf" },
    { 274, "fS" },
    { 275, "fS" },
    { 276, "" },
    { 277, "ff" },
    { 278, "Sff" },
    { 279, "Sff" },
    { 280, "mSSSSS" },
    { 281, "Sf" },
    { 300, "ff" },
    { 301, "SSff" },
    { 304, "ff" },
    { 305, "SSff" },
    { 306, "fS" },
    { 307, "SSff" },
    { 308, "ffff" },
    { 309, "SSfff" },
    { 310, "ffff" },
    { 311, "SSfff" },
    { 312, "SSf" },
    { 320, "ffffff" },
    { 321, "SSfffff" },
    { 325, "Sffffff" },
    { 328, "ff" },
    { 329, "Sfff" },
    { 400, "ff" },
    { 401, "ff" },
    { 402, "S" },
    { 403, "S" },
    { 404, "ffff" },
    { 405, "" },
    { 406, "" },
    { 407, "SS" },
    { 408, "ff" },
    { 409, "" },
    { 410, "S" },
    { 411, "S" },
    { 412, "S" },
    { 413, "" },
    { 414, "SSSm" },
    { 415, "S" },
    { 416, "S" },
    { 417, "SSS" },
    { 418, "S" },
    { 419, "" },
    { 420, "" },
    { 421, "Sm" },
    { 422, "SSSx" },
    { 423, "" },
    { 424, "S" },
    { 425, "" },
    { 427, "SfS" },
    { 435, "SSSSS" },
    { 436, "fffff" },
    { 437, "SSSx" },
    { 438, "SSSx" },
    { 439, "SSSx" },
    { 440, "S" },
    { 442, "" },
    { 443, "" },
    { 444, "S" },
    { 445, "" },
    { 446, "Sf" },
    { 447, "f" },
    { 448, "SSSS" },
    { 452, "f" },
    { 453, "S" },
    { 454, "" },
    { 455, "SS" },
    { 500, "S" },
    { 501, "S" },
    { 502, "SSS" },
    { 503, "Sff" },
    { 504, "Sff" },
    { 505, "Sff" },
    { 506, "SSS" },
    { 507, "SS" },
    { 508, "SSS" },
    { 509, "SSSSSSff" },
    { 510, "" },
    { 511, "SS" },
    { 512, "f" },
    { 513, "f" },
    { 520, "fff" },
    { 521, "Sffffffff" },
    { 522, "SSSSSSSSS" },
    { 523, "Sff" },
    { 524, "Sf" },
    { 525, "Sff" },
    { 526, "fS" },
    { 529, "S" },
    { 531, "S" },
    { 534, "S" },
    { 600, "Sffff" },
    { 601, "SSSSSS" },
    { 602, "S" },
    { 603, "fS" },
    { 608, "Sf" },
    { 610, "S" },
    { 611, "S" },
    { 700, "S" },
    { -1, NULL }
};

static const id_format_pair_t th125_fmts[] = {
    { 30, "m*D" },
    { 62, "" },
    { 77, "" },
    { 79, "" },
    { 80, "" },
    { 85, "" },
    { 88, "" },
    { 90, "fffff" },
    { 91, "SfSSff" },
    { 282, "SS" },
    { 303, "SSfS" },
    { 306, "fS" },
    { 307, "SSff" },
    { 318, "Sf" },
    { 322, "fffSfS" },
    { 327, "" },
    { 333, "S" },
    { 426, "f" },
    { 457, "" },
    { 458, "S" },
    { 459, "S" },
    { 460, "f" },
    { 461, "f" },
    { 462, "S" },
    { 463, "m" },
    { 532, "f" },
    { 536, "S" },
    { 604, "SSf" },
    { 609, "Sf" },
    { 612, "ff" },
    { -1, NULL }
};

static const id_format_pair_t th128_fmts[] = {
    { 22, "Sm" },
    { 71, "" },
    { 283, "ffS" },
    { 537, "fSfSSSffffff" },
    { 538, "SSm" },
    { 613, "S" },
    { 614, "fS" },
    { 615, "SS" },
    { -1, NULL }
};

static const id_format_pair_t th13_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 10, "" },
    { 11, "m*D" },
    { 12, "SS" },
    { 13, "SS" },
    { 14, "SS" },
    { 15, "m*D" },
    { 21, "" },
    { 22, "Sm" },
    { 40, "S" },
    { 42, "S" },
    { 43, "S" },
    { 44, "f" },
    { 45, "f" },
    { 50, "" },
    { 51, "" },
    { 52, "" },
    { 53, "" },
    { 55, "" },
    { 57, "" },
    { 58, "" },
    { 59, "" },
    { 61, "" },
    { 63, "" },
    { 65, "" },
    { 64, "" },
    { 66, "" },
    { 67, "" },
    { 68, "" },
    { 69, "" },
    { 70, "" },
    { 73, "" },
    { 74, "" },
    { 78, "S" },
    { 81, "ffff" },
    { 83, "S" },
    { 87, "Sff" },
    { 88, "fffff" },
    { 92, "SfSSff" },
    { 94, "ffff" },
    { 300, "mffSSS" },
    { 301, "mffSSS" },
    { 302, "S" },
    { 303, "SS" },
    { 304, "mffSSS" },
    { 306, "SS" },
    { 307, "SS" },
    { 308, "SS" },
    { 309, "mffSSS" },
    { 311, "mffSSS" },
    { 313, "S" },
    { 318, "SS" },
    { 322, "Sff" },
    { 323, "mSSSSS" },
    { 330, "SSSS" },
    { 331, "Sff" },
    { 332, "SSSff" },
    { 334, "SSSS" },
    { 335, "SSSff" },
    { 400, "ff" },
    { 401, "SSff" },
    { 404, "ff" },
    { 405, "SSff" },
    { 408, "ffSS" },
    { 409, "SSffS" },
    { 411, "SSffS" },
    { 412, "SSf" },
    { 420, "fffSff" },
    { 422, "fffSff" },
    { 425, "Sffffff" },
    { 429, "SSff" }, 
    { 433, "S" },
    { 500, "ff" },
    { 501, "ff" },
    { 502, "S" },
    { 503, "S" },
    { 504, "Sfff" },
    { 505, "" },
    { 506, "" },
    { 507, "SS" },
    { 508, "ff" },
    { 509, "" },
    { 510, "S" },
    { 511, "S" },
    { 512, "S" },
    { 513, "" },
    { 514, "SSSm" },
    { 515, "S" },
    { 516, "S" },
    { 517, "SSS" },
    { 518, "S" },
    { 519, "" },
    { 520, "" },
    { 521, "Sm" },
    { 523, "" },
    { 524, "S" },
    { 525, "" },
    { 527, "SfS" },
    { 535, "SSSSS" },
    { 536, "fffff" },
    { 537, "SSSx" },
    { 539, "SSSx" },
    { 540, "S" },
    { 545, "" },
    { 548, "SSSS" },
    { 554, "" },
    { 557, "m" },
    { 558, "SSSff" },
    { 559, "S" },
    { 600, "S" },
    { 601, "S" },
    { 602, "SSS" },
    { 603, "Sff" },
    { 604, "Sff" },
    { 605, "Sff" },
    { 606, "SSS" },
    { 607, "SS" },
    { 608, "SSS" },
    { 609, "SSSSSSff" },
    { 610, "SSSSSSSSffff" },
    { 611, "SSSSSff" },
    { 612, "SSSSSSSffff" },
    { 613, "" },
    { 614, "SS" },
    { 615, "f" },
    { 616, "f" },
    { 624, "Sffffffff" },
    { 625, "SSSSSSSSS" },
    { 626, "Sff" },
    { 627, "Sf" },
    { 629, "fS" },
    { 632, "S" },
    { 700, "Sffff" },
    { 701, "SSSSSS" },
    { 703, "SS" },
    { 708, "Sf" },
    { 711, "S" },
    { 800, "S" },
    { 1001, "S" },
    { 1002, "S" },
    { 1003, "S" },
    { -1, NULL }
};

static const char*
th10_find_format(
    unsigned int version,
    unsigned int id)
{
    const char* ret = NULL;

    switch (version) {
    case 11:
        if (!ret) ret = find_format(th11_fmts, id);
    case 10:
        if (!ret) ret = find_format(th10_fmts, id);
        break;
    case 128:
        if (!ret) ret = find_format(th128_fmts, id);
    case 125:
        if (!ret) ret = find_format(th125_fmts, id);
    case 12:
        if (!ret) ret = find_format(th12_fmts, id);
        break;
    case 13:
        if (!ret) ret = find_format(th13_fmts, id);
        break;
    default:
        fprintf(stderr, "%s: unsupported version: %u\n", argv0, version);
        return NULL;
    }

    if (!ret)
        fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, id);

    return ret;
}

static void
th10_insert_labels(
    thecl_t* ecl)
{
    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr) {

            thecl_param_t* param;
            list_for_each(&instr->params, param) {

                if (param->type == 'o') {

                    list_node_t* node;
                    list_for_each_node(&sub->instrs, node) {
                        thecl_instr_t* instr_find = node->data;

                        if (instr_find->offset == instr->offset + param->value.val.S) {

                            if (instr_find->type != THECL_INSTR_LABEL) {
                                thecl_instr_t* label = thecl_instr_label(instr_find->offset);
                                list_prepend_to(&sub->instrs, label, node);
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
th10_open(
    FILE* stream,
    unsigned int version)
{
    /* Input. */
    long file_size;
    unsigned char* map;

    /* Helpers. */
    const th10_header_t* header;
    const th10_list_t* anim_list;
    const th10_list_t* ecli_list;
    const uint32_t* sub_offsets;

    /* Temporary. */
    char* string_data;
    size_t i;

    /* Output data. */
    thecl_t* ecl;

    file_size = file_fsize(stream);
    if (file_size == -1)
        return NULL;

    map = file_mmap(stream, file_size);
    if (!map)
        return NULL;

    ecl = thecl_new();

    header = (th10_header_t*)map;
    if (strncmp(header->magic, "SCPT", 4) != 0) {
        fprintf(stderr, "%s:%s: SCPT signature missing\n",
            argv0, current_input);
        return NULL;
    }

    anim_list = (th10_list_t*)(map + header->include_offset);
    if (strncmp(anim_list->magic, "ANIM", 4) != 0) {
        fprintf(stderr, "%s:%s: ANIM signature missing\n",
            argv0, current_input);
        return NULL;
    }

    ecl->anim_count = anim_list->count;
    ecl->anim_names = malloc(anim_list->count * sizeof(char*));
    string_data = (char*)anim_list->data;
    for (i = 0; i < anim_list->count; ++i) {
        ecl->anim_names[i] = strdup(string_data);
        string_data += strlen(ecl->anim_names[i]) + 1;
    }

    while ((ptrdiff_t)string_data % 4)
        ++string_data;
    ecli_list = (th10_list_t*)string_data;
    if (strncmp(ecli_list->magic, "ECLI", 4) != 0) {
        fprintf(stderr, "%s:%s: ECLI signature missing\n",
            argv0, current_input);
        return NULL;
    }

    ecl->ecli_count = ecli_list->count;
    ecl->ecli_names = malloc(ecli_list->count * sizeof(char*));
    string_data = (char*)ecli_list->data;
    for (i = 0; i < ecli_list->count; ++i) {
        ecl->ecli_names[i] = strdup(string_data);
        string_data += strlen(ecl->ecli_names[i]) + 1;
    }

    while ((ptrdiff_t)string_data % 4)
        ++string_data;
    sub_offsets = (uint32_t*)(string_data + ((4 - (ptrdiff_t)string_data) % 4));

    ecl->sub_count = header->sub_count;
    string_data = (char*)(sub_offsets + header->sub_count);

    for (i = 0; i < header->sub_count; ++i) {
        const th10_sub_t* raw_sub;
        const th10_instr_t* instr;
        unsigned int time = 0;
        unsigned int rank = 0xff;
        thecl_sub_t* sub = malloc(sizeof(thecl_sub_t));

        sub->name = strdup(string_data);
        list_init(&sub->instrs);
        sub->stack = 0;
        sub->arity = -1;
        sub->var_count = 0;
        sub->vars = NULL;
        /* XXX: Do I need to store this? */
        sub->offset = sub_offsets[i];
        list_init(&sub->labels);

        list_append_new(&ecl->subs, sub);

        string_data += strlen(sub->name) + 1;

        raw_sub = (th10_sub_t*)(map + sub_offsets[i]);
        if (strncmp(raw_sub->magic, "ECLH", 4) != 0) {
            fprintf(stderr, "%s:%s: ECLH signature missing\n",
                argv0, current_input);
            return NULL;
        }

        for (instr = (th10_instr_t*)raw_sub->data;
             (unsigned char*)instr != map + file_size
             && (unsigned char*)instr != map + sub_offsets[i + 1];
             instr = (th10_instr_t*)((unsigned char*)instr + instr->size)) {

            if (instr->time != time) {
                thecl_instr_t* new = thecl_instr_time(instr->time);
                new->offset = (ptrdiff_t)instr - (ptrdiff_t)raw_sub;
                list_append_new(&sub->instrs, new);
                time = instr->time;
            }

            if (instr->rank_mask != rank) {
                thecl_instr_t* new = thecl_instr_rank(instr->rank_mask);
                new->offset = (ptrdiff_t)instr - (ptrdiff_t)raw_sub;
                list_append_new(&sub->instrs, new);
                rank = instr->rank_mask;
            }

            thecl_instr_t* new_instr = thecl_instr_new();
            list_append_new(&sub->instrs, new_instr);
            new_instr->id = instr->id;
            new_instr->param_count = instr->param_count;
            new_instr->offset = (ptrdiff_t)instr - (ptrdiff_t)raw_sub;
            list_init(&new_instr->params);

            if (instr->size > sizeof(th10_instr_t)) {
                uint32_t param_mask = instr->param_mask;
                const char* format = th10_find_format(version, instr->id);
                /* TODO: Handle format == NULL. */

                value_t* values = value_list_from_data(th10_value_from_data, instr->data, instr->size - sizeof(th10_instr_t), format);
                if (!values)
                    return NULL;
                value_t* value_iter = values;

                size_t p = 0;
                char f;
                while (value_iter->type) {
                    if (format[p] == '*') {
                        f = format[p + 1];
                    } else {
                        f = format[p];
                    }
                    thecl_param_t* param = param_new(f);
                    param->stack = param_mask & 1;
                    param_mask >>= 1;
                    param->value = *value_iter;
                    list_append_new(&new_instr->params, param);
                    ++value_iter;
                    if (format[p] != '*')
                        ++p;
                }
                free(values);
            }
        }
    }

    th10_insert_labels(ecl);

    return ecl;
}

static thecl_sub_t*
th10_find_sub(
    thecl_t* ecl,
    const char* name)
{
    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {
        if (strcmp(sub->name, name) == 0)
            return sub;
    }
    return NULL;
}

static void
th10_set_arity(
    thecl_sub_t* sub,
    unsigned int arity)
{
    if (sub->arity != -1 &&
        sub->arity != arity) {
        fprintf(stderr, "%s:%s: arity mismatch %zd %u for %s\n",
            argv0, current_input,
            sub->arity, arity, sub->name);
    } else {
        sub->arity = arity;
    }
}

void
th10_trans(
    thecl_t* ecl)
{
    thecl_sub_t* sub;

    if (!ecl)
        return;

    list_for_each(&ecl->subs, sub) {
        list_node_t* node;
        list_node_t* node_next;
        list_for_each_node_safe(&sub->instrs, node, node_next) {
            thecl_instr_t* instr = node->data;

            if (instr->type == THECL_INSTR_INSTR && instr->id == 40) {
                thecl_param_t* param = list_head(&instr->params);
                sub->stack = param->value.val.S;
                list_del(&sub->instrs, node);
                thecl_instr_free(instr);
                continue;
            }

            if (instr->type == THECL_INSTR_INSTR) {
                thecl_param_t* param;
                list_for_each(&instr->params, param) {
                    if (param->type == 'm') {
                        thecl_sub_t* found_sub = th10_find_sub(ecl, param->value.val.z);
                        if (found_sub) {
                            if (instr->id == 11 ||
                                instr->id == 15) {
                                th10_set_arity(found_sub, instr->param_count - 1);
                            } else {
                                thecl_sub_t* found_sub = th10_find_sub(ecl, param->value.val.z);
                                th10_set_arity(found_sub, 0);
                            }
                        }
                    }
                }
            }
        }
    }
}

/* This assumes str can contain as much as is needed. */
static void
str_replace(char* str, const char* pat, const char* rep)
{
    char* temp_org = strdup(str);
    char* temp = temp_org;

    for (;;) {
        if (*temp == '\0') {
            *str++ = '\0';
            break;
        } else if (strncmp(temp, pat, strlen(pat)) == 0) {
            sprintf(str, "%s", rep);
            str += strlen(rep);
            temp += strlen(pat);
        } else {
            *str++ = *temp++;
        }
    }

    free(temp_org);
}

static thecl_param_t*
th10_param_index(
    thecl_instr_t* instr,
    size_t index)
{
    thecl_param_t* param;
    list_for_each(&instr->params, param) {
        if (!index--)
            return param;
    }
    return NULL;
}

static void*
th10_stack_index(
    list_node_t* start_node,
    size_t index)
{
    list_node_t* node;
    size_t i;

    for (i = 0, node = start_node->prev; i < index; ++i, node = node->prev) {
        if (!node)
            return NULL;
    }

    if (!node)
        return NULL;

    return node->data;
}

static size_t
th10_stack_size(
    list_node_t* start_node)
{
    size_t ret = 0;

    list_node_t* node = start_node->prev;
    while (node) {
        thecl_instr_t* instr = node->data;
        if (instr->type != THECL_INSTR_INSTR)
            break;
        if (!instr->op_type)
            break;
        ++ret;
        node = node->next;
    }

    return ret;
}

/* TODO: Split this into a function that uses p, and one that uses param.
 *       Have the p function wrap the param function. */
static char*
th10_stringify_param(
    thecl_sub_t* sub,
    list_node_t* node,
    size_t p,
    thecl_param_t* param,
    size_t* removed,
    int special)
{
    thecl_instr_t* instr = node->data;

    char temp[512];
    if (!param)
        param = th10_param_index(instr, p);

    switch (param->type) {
    case 'o': {
        sprintf(temp, "%s_%d", sub->name, instr->offset + param->value.val.S);
        return strdup(temp);
    }
    case 'D': {
        value_t new_value;
        int32_t* D = (int*)param->value.val.m.data;
        memcpy(&new_value.val.S, &D[1], sizeof(int32_t));
        if (D[0] == 0x6666 /* ff */) {
            sprintf(temp, "_ff");
            new_value.type = 'f';
        } else if (D[0] == 0x6669 /* fi */) {
            sprintf(temp, "_fS");
            new_value.type = 'S';
        } else if (D[0] == 0x6966 /* if */) {
            sprintf(temp, "_Sf");
            new_value.type = 'f';
        } else if (D[0] == 0x6969 /* ii */) {
            sprintf(temp, "_SS");
            new_value.type = 'S';
        } else {
            fprintf(stderr, "%s: ...\n", argv0);
            abort();
        }

        thecl_param_t temp_param = *param;
        temp_param.type = new_value.type;
        temp_param.value = new_value;
        strcat(temp, " ");
        char* str_temp = th10_stringify_param(sub, node, 0, &temp_param, removed, 1);
        strcat(temp, str_temp);
        free(str_temp);

        return strdup(temp);
    }
    default: {
        thecl_instr_t* rep = th10_stack_index(node, *removed);
        if (param->value.type == 'S' &&
            param->stack &&
            param->value.val.S >= 0 &&
            (param->value.val.S % 4) == 0) {
            sprintf(temp, "$%c", 'A' + param->value.val.S / 4);
        } else if (
            /* TODO: Also check that it is a multiple of four. */
            param->value.type == 'f' &&
            param->stack &&
            param->value.val.f >= 0.0f) {
            sprintf(temp, "%%%c", 'A' + (int)param->value.val.f / 4);
        } else if (
            param->value.type == 'S' &&
            param->stack &&
            (param->value.val.S == -1 || param->value.val.S == -(*removed + 1)) &&
            rep &&
            rep->op_type) {
            ++*removed;
            if (special)
                sprintf(temp, "(%s)", rep->string);
            else
                sprintf(temp, "_S(%s)", rep->string);
        } else if (
            param->value.type == 'f' &&
            param->stack &&
            (param->value.val.f == -1.0f || param->value.val.f == -(*removed + 1.0f)) &&
            rep &&
            rep->op_type) {
            ++*removed;
            if (special)
                sprintf(temp, "(%s)", rep->string);
            else
                sprintf(temp, "_f(%s)", rep->string);
        } else {
            char* ret = th10_param_to_text(param);
            sprintf(temp, "%s%s%s",
                param->stack ? "[" : "",
                ret,
                param->stack ? "]" : "");
            free(ret);
        }

        return strdup(temp);
    }
    }

    return NULL;
}

static void
th10_stringify_instr(
    thecl_sub_t* sub,
    list_node_t* node)
{
    thecl_instr_t* instr = node->data;
    size_t p;
    char string[1024] = { '\0' };

    if (instr->type != THECL_INSTR_INSTR)
        return;

    sprintf(string, "ins_%u(", instr->id);
    for (p = 0; p < instr->param_count; ++p) {
        size_t removed = 0;
        char* param_string = th10_stringify_param(sub, node, p, NULL, &removed, 0);
        if (p != 0)
            strcat(string, ", ");
        strcat(string, param_string);
        free(param_string);

        for (size_t s = 0; s < removed; ++s) {
            list_node_t* prev = node->prev;
            thecl_instr_free(prev->data);
            list_del(&sub->instrs, prev);
        }
    }
    strcat(string, ")");

    instr->string = strdup(string);
}

static void
th10_dump(
    const thecl_t* ecl,
    FILE* out)
{
    thecl_sub_t* sub;

    if (ecl->anim_count) {
        unsigned int i;
        fprintf(out, "anim { ");
        for (i = 0; i < ecl->anim_count; ++i)
            fprintf(out, "\"%s\"; ", ecl->anim_names[i]);
        fprintf(out, "}\n");
    }

    if (ecl->ecli_count) {
        unsigned int i;
        fprintf(out, "ecli { ");
        for (i = 0; i < ecl->ecli_count; ++i)
            fprintf(out, "\"%s\"; ", ecl->ecli_names[i]);
        fprintf(out, "}\n");
    }

    list_for_each(&ecl->subs, sub) {
        unsigned int p;
        thecl_instr_t* instr;

        if (sub->arity == -1)
            sub->arity = 0;
        fprintf(out, "\nsub %s(", sub->name);
        for (p = 0; p < sub->arity; ++p) {
            if (p != 0)
                fprintf(out, " ");
            fprintf(out, "%c", 'A' + p);
        }
        fprintf(out, ")\n{\n");

        fprintf(out, "    var");
        for (p = sub->arity * 4; p < sub->stack; p += 4) {
            fprintf(out, " %c", 'A' + p / 4);
        }
        fprintf(out, ";\n");

        list_node_t* node;
        list_node_t* node_next;
        list_for_each_node_safe(&sub->instrs, node, node_next) {
            thecl_instr_t* instr = node->data;

            char temp[1024] = { '\0' };

            switch (instr->type) {
            case THECL_INSTR_TIME:
                sprintf(temp, "%u:", instr->time);
                instr->string = strdup(temp);
                break;
            case THECL_INSTR_RANK:
                sprintf(temp, "!%s%s%s%s%s%s%s%s",
                    (instr->rank) & RANK_EASY    ? "E" : "",
                    (instr->rank) & RANK_NORMAL  ? "N" : "",
                    (instr->rank) & RANK_HARD    ? "H" : "",
                    (instr->rank) & RANK_LUNATIC ? "L" : "",
                    !((instr->rank) & RANK_UNKNOWN1) ? "W" : "",
                    !((instr->rank) & RANK_UNKNOWN2) ? "X" : "",
                    !((instr->rank) & RANK_UNKNOWN3) ? "Y" : "",
                    !((instr->rank) & RANK_UNKNOWN4) ? "Z" : "");
                instr->string = strdup(temp);
                break;
            case THECL_INSTR_LABEL:
                sprintf(temp, "%s_%u:", sub->name, instr->offset);
                instr->string = strdup(temp);
                break;
            case THECL_INSTR_INSTR: {
                const expr_t* expr = expr_get_by_id(10, instr->id);

                if (expr) {
                    char pat[4];
                    size_t param_count = expr->param_format ? strlen(expr->param_format) : 0;
                    size_t stack_count = expr->stack_arity;

                    /* TODO: Display errors. */
                    if (th10_stack_size(node) < stack_count)
                        goto normal;

                    if (instr->param_count != param_count)
                        goto normal;

                    for (size_t p = 0; p < param_count; ++p) {
                        if (th10_param_index(instr, p)->type != expr->param_format[p])
                            goto normal;
                    }

                    sprintf(temp, "%s", expr->display_format);

                    for (size_t s = 0; s < stack_count; ++s) {
                        sprintf(pat, "s%zu", s);
                        thecl_instr_t* rep = th10_stack_index(node, s);
                        if (rep->op_type != expr->stack_formats[s]) {
                            char rep_str[1024] = { '\0' };
                            /* XXX: Only supports S and f. */
                            sprintf(rep_str, "%s(%s)", expr->stack_formats[s] == 'S' ? "_S" : "_f", rep->string);
                            str_replace(temp, pat, rep_str);
                        } else if (!expr_is_leaf(10, rep->id)) {
                            char rep_str[1024] = { '\0' };
                            sprintf(rep_str, "(%s)", rep->string);
                            str_replace(temp, pat, rep_str);
                        } else {
                            str_replace(temp, pat, rep->string);
                        }
                    }

                    /* What matters is that the correct stack offset is grabbed
                     * ... I should do this before -1. */
                    for (size_t s = 0; s < expr->stack_arity; ++s) {
                        list_node_t* prev = node->prev;
                        thecl_instr_free(prev->data);
                        list_del(&sub->instrs, prev);
                    }

                    for (size_t p = 0; p < param_count; ++p) {
                        size_t removed = 0;
                        sprintf(pat, "p%zu", p);
                        char* rep_str = th10_stringify_param(sub, node, p, NULL, &removed, 0);
                        str_replace(temp, pat, rep_str);
                        free(rep_str);

                        for (size_t s = 0; s < removed; ++s) {
                            list_node_t* prev = node->prev;
                            thecl_instr_free(prev->data);
                            list_del(&sub->instrs, prev);
                        }
                    }

                    instr->string = strdup(temp);
                    instr->op_type = expr->return_type;
                } else {
normal:
                    th10_stringify_instr(sub, node);
                }
                break;
            }
            }
        }

        list_for_each(&sub->instrs, instr) {
            if (instr->type == THECL_INSTR_INSTR) {
                fprintf(out, "    %s;\n", instr->string);
            } else {
                fprintf(out, "%s\n", instr->string);
            }
        }

        fprintf(out, "}\n");
    }
}

static size_t
th10_instr_size(
    unsigned int __attribute__((unused)) version,
    const thecl_instr_t* instr)
{
    size_t ret = sizeof(th10_instr_t);
    thecl_param_t* param;

    list_for_each(&instr->params, param) {
        /* XXX: I think 'z' is what will be passed ... */
        if (param->type == 'm' || param->type == 'x') {
            size_t zlen = strlen(param->value.val.z);
            ret += sizeof(uint32_t) + zlen + (4 - (zlen % 4));
        } else if (param->type == 'o') {
            ret += sizeof(uint32_t);
        } else {
            ret += value_size(&param->value);
        }
    }

    return ret;
}

static thecl_t*
th10_parse(
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
    state.instr_format = th10_find_format;
    state.instr_size = th10_instr_size;

    yyin = in;

    if (yyparse(&state) != 0)
        return 0;

    return state.ecl;
}

static int32_t
label_find(
    thecl_sub_t* sub,
    const char* name)
{
    thecl_label_t* label;
    list_for_each(&sub->labels, label) {
        if (strcmp(label->name, name) == 0)
            return label->offset;
    }
    fprintf(stderr, "%s: label not found: %s\n", argv0, name);
    return 0;
}

static unsigned char*
th10_instr_serialize(
    unsigned int version,
    thecl_sub_t* sub,
    thecl_instr_t* instr)
{
    th10_instr_t* ret;
    thecl_param_t* param;

    ret = malloc(instr->size);

    ret->time = instr->time;
    ret->id = instr->id;
    ret->size = instr->size;
    ret->param_mask = 0;
    ret->rank_mask = instr->rank;
    ret->param_count = instr->param_count;
    ret->zero = 0;

    unsigned char* param_data = ret->data;
    int param_count = 0;

    list_for_each(&instr->params, param) {
        if (param->stack)
            ret->param_mask |= 1 << param_count;
        ++param_count;
        if (param->type == 'o') {
            /* This calculates the relative offset from the current instruction. */
            uint32_t label = label_find(sub, param->value.val.z) - instr->offset;
            memcpy(param_data, &label, sizeof(uint32_t));
            param_data += sizeof(uint32_t);
        } else if (param->type == 'x' || param->type == 'm') {
            size_t zlen = strlen(param->value.val.z);
            uint32_t padded_length = zlen + (4 - (zlen % 4));
            memcpy(param_data, &padded_length, sizeof(padded_length));
            param_data += sizeof(padded_length);
            memset(param_data, 0, padded_length);
            strncpy((char*)param_data, param->value.val.z, zlen);
            if (param->type == 'x')
                util_xor(param_data, padded_length, 0x77, 7, 16);
            param_data += padded_length;
        } else
            param_data += value_to_data(&param->value, param_data, instr->size - (param_data - (unsigned char*)ret));

        if (param->stack && version == 13) {
            if (param->type == 'f' && param->value.val.f == -(ret->zero + 1.0f)) {
                ++ret->zero;
            } else if (param->type == 'S' && param->value.val.S == -(ret->zero + 1)) {
                ++ret->zero;
            } else if (param->type == 'D') {
                struct {
                    char from;
                    char to;
                    union {
                        int32_t S;
                        float f;
                    } val;
                } *temp = (void*)param->value.val.m.data;

                if (temp->from == 'f' && temp->val.f == -(ret->zero + 1.0f))
                    ++ret->zero;
                else if (temp->from == 'i' && temp->val.S == -(ret->zero + 1))
                    ++ret->zero;
            }
        }
    }

    ret->zero <<= 3;

    return (unsigned char*)ret;
}

static int
th10_compile(
    const thecl_t* ecl,
    FILE* out)
{
    long pos;
    unsigned int i;
    th10_header_t header = { "SCPT", 1, 0, 0, 0, ecl->sub_count, { 0 } };
    const th10_list_t anim_list = { "ANIM", ecl->anim_count };
    const th10_list_t ecli_list = { "ECLI", ecl->ecli_count };
    const th10_sub_t sub_header = { "ECLH", sizeof(th10_sub_t), { 0, 0 } };
    thecl_sub_t* sub;

    if (!file_seekable(out)) {
        fprintf(stderr, "%s: output is not seekable\n", argv0);
        return 0;
    }

    if (!file_write(out, &header, sizeof(th10_header_t)))
        return 0;

    header.include_offset = file_tell(out);

    if (!file_write(out, &anim_list, sizeof(th10_list_t)))
        return 0;
    for (i = 0; i < ecl->anim_count; ++i) {
        if (!file_write(out, ecl->anim_names[i], strlen(ecl->anim_names[i]) + 1))
            return 0;
    }

    pos = file_tell(out);
    if (pos % 4 != 0)
        file_seek(out, pos + 4 - pos % 4);

    if (!file_write(out, &ecli_list, sizeof(th10_list_t)))
        return 0;
    for (i = 0; i < ecl->ecli_count; ++i) {
        if (!file_write(out, ecl->ecli_names[i], strlen(ecl->ecli_names[i]) + 1))
            return 0;
    }

    pos = file_tell(out);
    if (pos % 4 != 0)
        file_seek(out, pos + 4 - pos % 4);

    pos = file_tell(out);
    header.include_length = pos - header.include_offset;

    file_seek(out, pos + ecl->sub_count * sizeof(uint32_t));

    list_for_each(&ecl->subs, sub) {
        if (!file_write(out, sub->name, strlen(sub->name) + 1))
            return 0;
    }

    pos = file_tell(out);
    if (pos % 4 != 0)
        file_seek(out, pos + 4 - pos % 4);

    list_for_each(&ecl->subs, sub) {
        thecl_instr_t* instr;
        sub->offset = file_tell(out);
        if (!file_write(out, &sub_header, sizeof(th10_sub_t)))
            return 0;

        list_for_each(&sub->instrs, instr) {
            unsigned char* data = th10_instr_serialize(ecl->version, sub, instr);
            if (!file_write(out, data, instr->size))
                return 0;
            free(data);
        }
    }

    file_seek(out, 0);
    if (!file_write(out, &header, sizeof(th10_header_t)))
        return 0;

    file_seek(out, header.include_offset + header.include_length);
    list_for_each(&ecl->subs, sub) {
        if (!file_write(out, &sub->offset, sizeof(uint32_t)))
            return 0;
    }

    return 1;
}

const thecl_module_t th10_ecl = {
    .open = th10_open,
    .trans = th10_trans,
    .dump = th10_dump,
    .parse = th10_parse,
    .compile = th10_compile,
};
