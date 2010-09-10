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
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "program.h"
#include "ecl.h"
#include "util.h"
#include "instr.h"
#include "ecsparse.h"

typedef struct {
    int id;
    const char* format;
} instr_fmt_t;

static const op_pair_t th10_ops[] = {
    /* Binary, type-alternatives. */
    { 2, ADD,      "+",  { ADDI,      "$+",  50, 'i' }, { ADDF,      "%+",  51, 'f' } },
    { 2, SUBTRACT, "-",  { SUBTRACTI, "$-",  52, 'i' }, { SUBTRACTF, "%-",  53, 'f' } },
    { 2, MULTIPLY, "*",  { MULTIPLYI, "$*",  54, 'i' }, { MULTIPLYF, "%*",  55, 'f' } },
    { 2, DIVIDE,   "/",  { DIVIDEI,   "$/",  56, 'i' }, { DIVIDEF,   "%/",  57, 'f' } },
    { 2, EQUAL,    "==", { EQUALI,    "$==", 59, 'i' }, { EQUALF,    "%==", 60, 'i' } },
    { 2, INEQUAL,  "!=", { INEQUALI,  "$!=", 61, 'i' }, { INEQUALF,  "%!=", 62, 'i' } },
    { 2, LT,       "<",  { LTI,       "$<",  63, 'i' }, { LTF,       "%<",  64, 'i' } },
    { 2, LTEQ,     "<=", { LTEQI,     "$<=", 65, 'i' }, { LTEQF,     "%<=", 66, 'i' } },
    { 2, GT,       ">",  { GTI,       "$>",  67, 'i' }, { GTF,       "%>",  68, 'i' } },
    { 2, GTEQ,     ">=", { GTEQI,     "$>=", 69, 'i' }, { GTEQF,     "%>=", 70, 'i' } },

    /* Binary, type-neutral. */
    { 2, MODULO,   "%",  { MODULO,    "%",   58, 'i' }, { MODULO,    "%",   58, 'i' } },
    { 2, OR,       "|",  { OR,        "|",   73, 'i' }, { OR,        "|",  73, 'i' } },
    { 2, AND,      "&",  { AND,       "&",   74, 'i' }, { AND,       "&",  74, 'i' } },
    { 2, XOR,      "^",  { XOR,       "^",   75, 'i' }, { XOR,       "^",   75, 'i' } },

    /* Unary. */
    { 1, NOT,      "!",  { NOT,       "!",   71, 'i' }, { NOT,       "!",   71, 'i' } },
    { 1, ASSIGN,   "=",  { ASSIGNI,   "$=",  43, 0   }, { ASSIGNF,   "%=",  45, 0   } },

    /* Non-operators. */
    { 0, LOAD,     "",   { LOADI,     "",    42, 'i' }, { LOADF,     "",    44, 'f' } },
    { 1, IF,       "",   { IF,        "",    14, 0   }, { IF,        "",    14, 0   } },
    { 1, UNLESS,   "",   { UNLESS,    "",    13, 0   }, { UNLESS,    "",    13, 0   } },
    { 0, GOTO,     "",   { GOTO,      "",    12, 0   }, { GOTO,      "",    12, 0   } },
    { 0, 0,        "",   { 0,         "",    0,  0   }, { 0,         "",    0,  0   } },
};

static const op_pair_t*
get_op_table(unsigned int version)
{
    if (   version != 10
        && version != 11
        && version != 12
        && version != 125
        && version != 128)
        return NULL;
    return th10_ops;
}

const op_pair_t*
op_find_instr(
    unsigned int version,
    int instr)
{
    const op_pair_t* table = get_op_table(version);
    unsigned int i;

    for (i = 0; table[i].token; ++i) {
        if (table[i].op_1.instr == instr)
            return &table[i];
        if (table[i].op_2.instr == instr)
            return &table[i];
    }

    return NULL;
}

const op_t*
op_find_token(
    unsigned int version,
    int token)
{
    const op_pair_t* table = get_op_table(version);
    unsigned int i;

    for (i = 0; table[i].token; ++i) {
        if (table[i].op_1.token == token)
            return &table[i].op_1;
        if (table[i].op_2.token == token)
            return &table[i].op_2;
    }

    return NULL;
}

const op_t*
op_find_stack(
    unsigned int version,
    int token,
    int stack_count,
    const int* stack_types)
{
    const op_pair_t* table = get_op_table(version);
    unsigned int i;

    for (i = 0; table[i].token; ++i) {
        if (   table[i].token == token
            && table[i].arity == stack_count) {
            int j;

            for (j = 0; j < stack_count; ++j) {
                if (stack_types[j] == 'f')
                    return &table[i].op_2;
            }

            return &table[i].op_1;
        }
    }

    return NULL;
}

static const instr_fmt_t th10_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 10, "" },
    { 11, "s?D*" },
    { 12, "oi" },
    { 13, "oi" },
    { 14, "oi" },
    { 15, "s?D*" },
    { 16, "si" },
    { 17, "i" },
    { 21, "" },
    { 40, "i" },
    { 42, "i" },
    { 43, "i" },
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
    { 78, "i" },
    { 79, "" },
    { 81, "ffff" },
    { 82, "f" },
    { 83, "i" },
    { 85, "" },
    { 256, "sffiii" },
    { 257, "sffiii" },
    { 258, "i" },
    { 259, "ii" },
    { 260, "sffiii" },
    { 261, "sffiii" },
    { 262, "ii" },
    { 263, "ii" },
    { 264, "ii" },
    { 265, "sffiii" },
    { 266, "sffiii" },
    { 267, "sffiii" },
    { 268, "sffiii" },
    { 269, "i" },
    { 270, "sfffiii" },
    { 272, "ii" },
    { 273, "iif" },
    { 280, "ff" },
    { 281, "iiff" },
    { 282, "ff" },
    { 283, "iifi" },
    { 284, "ff" },
    { 285, "iiff" },
    { 286, "ff" },
    { 287, "iiff" },
    { 288, "ffff" },
    { 290, "ffff" },
    { 291, "iifffi" },
    { 292, "iif" },
    { 294, "" },
    { 296, "iif" },
    { 298, "ff" },
    { 299, "ff" },
    { 320, "ff" },
    { 321, "ff" },
    { 322, "i" },
    { 323, "i" },
    { 324, "ifff" },
    { 325, "" },
    { 326, "" },
    { 327, "ii" },
    { 328, "ff" },
    { 329, "" },
    { 330, "i" },
    { 331, "i" },
    { 332, "i" },
    { 333, "" },
    { 334, "iiis" },
    { 335, "i" },
    { 336, "i" },
    { 337, "iii" },
    { 338, "i" },
    { 339, "" },
    { 340, "" },
    { 341, "is" },
    { 342, "iiic" },
    { 343, "" },
    { 344, "i" },
    { 345, "" },
    { 346, "f" },
    { 347, "ifi" },
    { 355, "iiiii" },
    { 356, "fffff" },
    { 357, "iiic" },
    { 359, "iiic" },
    { 360, "i" },
    { 361, "i" },
    { 362, "" },
    { 363, "" },
    { 364, "i" },
    { 365, "" },
    { 366, "ii" },
    { 367, "f" },
    { 368, "iiii" },
    { 400, "i" },
    { 401, "i" },
    { 402, "iii" },
    { 403, "iff" },
    { 404, "iff" },
    { 405, "iff" },
    { 406, "iii" },
    { 407, "ii" },
    { 409, "iiiiiiff" },
    { 410, "" },
    { 411, "ii" },
    { 412, "iiffffif" },
    { 413, "iiifffiiiifi" },
    { 420, "f" },
    { 421, "f" },
    { 425, "iiiiiii" },
    { 428, "iiffifif" },
    { 431, "iiffifff" },
    { 433, "iiffifff" },
    { 435, "iffffffff" },
    { 436, "iiiiiiiii" },
    { -1, NULL }
};

static const instr_fmt_t th11_fmts[] = {
    { 66, "" },
    { 275, "fi" },
    { 276, "" },
    { 277, "ff" },
    { 278, "i" },
    { 289, "iifff" },
    { 291, "iiffi" },
    { 300, "ffffff" },
    { 302, "ffffff" },
    { 305, "iffffff" },
    { 307, "" },
    { 369, "i" },
    { 370, "i" },
    { 371, "i" },
    { 408, "iii" },
    { 437, "iff" },
    { 438, "if" },
    { 439, "iff" },
    { 440, "fi" },
    { 441, "i" },
    { 442, "i" },
    { 443, "i" },
    { 444, "i" },
    { 445, "i" },
    { 447, "f" },
    { 448, "i" },
    { 449, "i" },
    { 450, "i" },
    { 500, "i" },
    { -1, NULL }
};

static const instr_fmt_t th12_fmts[] = {
    { 0, "" },
    { 1, "" },
    { 10, "" },
    { 11, "s?D*" },
    { 12, "oi" },
    { 13, "oi" },
    { 14, "oi" },
    { 15, "s?D*" },
    { 16, "si?D*" },
    { 17, "f" },
    { 21, "" },
    { 40, "i" },
    { 42, "i" },
    { 43, "i" },
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
    { 78, "i" },
    { 81, "ffff" },
    { 82, "f" },
    { 83, "i" },
    { 86, "fff" },
    { 87, "fffff" },
    { 89, "fff" },
    { 256, "sffiii" },
    { 257, "sffiii" },
    { 258, "i" },
    { 259, "ii" },
    { 260, "sffiii" },
    { 261, "sffiii" },
    { 262, "ii" },
    { 263, "ii" },
    { 264, "fi" },
    { 265, "sffiii" },
    { 267, "sffiii" },
    { 269, "f" },
    { 270, "sffiiii" },
    { 273, "iif" },
    { 274, "fi" },
    { 275, "fi" },
    { 276, "" },
    { 277, "ff" },
    { 278, "iff" },
    { 279, "iff" },
    { 280, "siiiii" },
    { 281, "if" },
    { 300, "ff" },
    { 301, "iiff" },
    { 304, "ff" },
    { 305, "iiff" },
    { 306, "fi" },
    { 307, "iiff" },
    { 308, "ffff" },
    { 309, "iifff" },
    { 310, "ffff" },
    { 311, "iifff" },
    { 312, "iif" },
    { 320, "ffffff" },
    { 321, "iifffff" },
    { 325, "iffffff" },
    { 328, "ff" },
    { 329, "ifff" },
    { 400, "ff" },
    { 401, "ff" },
    { 402, "i" },
    { 403, "i" },
    { 404, "ffff" },
    { 405, "" },
    { 406, "" },
    { 407, "ii" },
    { 408, "ff" },
    { 409, "" },
    { 410, "i" },
    { 411, "i" },
    { 412, "i" },
    { 413, "" },
    { 414, "iiis" },
    { 415, "i" },
    { 416, "i" },
    { 417, "iii" },
    { 418, "i" },
    { 419, "" },
    { 420, "" },
    { 421, "is" },
    { 422, "iiic" },
    { 423, "" },
    { 424, "i" },
    { 425, "" },
    { 427, "ifi" },
    { 435, "iiiii" },
    { 436, "fffff" },
    { 437, "iiic" },
    { 438, "iiic" },
    { 439, "iiic" },
    { 440, "i" },
    { 442, "" },
    { 443, "" },
    { 444, "i" },
    { 445, "" },
    { 446, "if" },
    { 447, "f" },
    { 448, "iiii" },
    { 452, "f" },
    { 453, "i" },
    { 454, "" },
    { 455, "ii" },
    { 500, "i" },
    { 501, "i" },
    { 502, "iii" },
    { 503, "iff" },
    { 504, "iff" },
    { 505, "iff" },
    { 506, "iii" },
    { 507, "ii" },
    { 508, "iii" },
    { 509, "iiiiiiff" },
    { 510, "" },
    { 511, "ii" },
    { 512, "f" },
    { 513, "f" },
    { 520, "fff" },
    { 521, "iffffffff" },
    { 522, "iiiiiiiii" },
    { 523, "iff" },
    { 524, "if" },
    { 525, "iff" },
    { 526, "fi" },
    { 529, "i" },
    { 531, "i" },
    { 534, "i" },
    { 600, "iffff" },
    { 601, "iiiiii" },
    { 602, "i" },
    { 603, "fi" },
    { 608, "if" },
    { 610, "i" },
    { 611, "i" },
    { 700, "i" },
    { -1, NULL }
};

static const instr_fmt_t th125_fmts[] = {
    { 30, "s?D*" },
    { 62, "" },
    { 77, "" },
    { 79, "" },
    { 80, "" },
    { 85, "" },
    { 88, "" },
    { 90, "fffff" },
    { 91, "ifiiff" },
    { 282, "ii" },
    { 303, "iifi" },
    { 306, "fi" },
    { 307, "iiff" },
    { 318, "if" },
    { 322, "fffifi" },
    { 327, "" },
    { 333, "i" },
    { 426, "f" },
    { 457, "" },
    { 458, "i" },
    { 459, "i" },
    { 460, "f" },
    { 461, "f" },
    { 462, "i" },
    { 463, "s" },
    { 532, "f" },
    { 536, "i" },
    { 604, "iif" },
    { 609, "if" },
    { 612, "ff" },
    { -1, NULL }
};

static const instr_fmt_t th128_fmts[] = {
    { 22, "is" },
    { 71, "" },
    { 283, "ffi" },
    { 537, "fifiiiffffff" },
    { 538, "iis" },
    { 613, "i" },
    { 614, "fi" },
    { 615, "ii" },
    { -1, NULL }
};

static const char*
search_format(
    int id,
    const instr_fmt_t* formats)
{
    unsigned int i;

    for (i = 0; formats[i].id != -1; ++i)
        if (formats[i].id == id)
            return formats[i].format;

    return NULL;
}

static const char*
format_find(
    unsigned int version,
    int id)
{
    const char* ret = NULL;

    switch (version) {
    case 10:
        if (!ret) ret = search_format(id, th10_fmts);
        break;
    case 11:
        if (!ret) ret = search_format(id, th11_fmts);
        if (!ret) ret = search_format(id, th10_fmts);
        break;
    case 12:
        if (!ret) ret = search_format(id, th12_fmts);
        break;
    case 125:
        if (!ret) ret = search_format(id, th125_fmts);
        if (!ret) ret = search_format(id, th12_fmts);
        break;
    case 128:
        if (!ret) ret = search_format(id, th128_fmts);
        if (!ret) ret = search_format(id, th125_fmts);
        if (!ret) ret = search_format(id, th12_fmts);
        break;
    default:
        fprintf(stderr, "%s: unsupported version: %u\n", argv0, version);
        return NULL;
    }

    if (!ret)
        fprintf(stderr, "%s: instruction %u is not defined for version %u\n",
            argv0, id, version);

    return ret;
}

instr_t*
instr_parse(
    unsigned int version,
    const raw_instr_t* rinstr,
    instr_t* instr)
{
    unsigned int param_cnt;
    unsigned int i;
    unsigned int data_offset = 0;
    const char* format = NULL;

    format = format_find(version, rinstr->id);
    if (!format)
        return NULL;

    param_cnt = strlen(format);

    memset(instr, 0, sizeof(instr_t));
    instr->time = rinstr->time;
    instr->id = rinstr->id;
    instr->size = rinstr->size;
    instr->param_mask = rinstr->param_mask;
    instr->rank_mask = rinstr->rank_mask;
    instr->param_cnt = 0;
    instr->params = NULL;
    instr->offset = 0;

    for (i = 0; i < param_cnt; ++i) {
        if (format[i] == '?') {
            if (data_offset == rinstr->data_size)
                i = param_cnt;
        } else if (format[i] == '*') {
            if (data_offset == rinstr->data_size)
                i = param_cnt;
            else
                i -= 2;
        } else {
            param_t* param;

            if (data_offset == rinstr->data_size) {
                fprintf(stderr,
                    "%s:%s: data size too small for instruction %u of version %u\n",
                    argv0, current_input, rinstr->id, version);
                return NULL;
            }

            instr->param_cnt++;
            instr->params =
                realloc(instr->params, instr->param_cnt * sizeof(param_t));
            param = &instr->params[instr->param_cnt - 1];
            param->type = format[i];

            if (data_offset + 4 > rinstr->data_size) {
                fprintf(stderr,
                    "%s:%s: format size larger than data size for instruction %u of version %u\n",
                    argv0, current_input, rinstr->id, version);
                return NULL;
            }

            memcpy(&param->value, rinstr->data + data_offset, 4);
            data_offset += 4;

            if (format[i] == 's' || format[i] == 'c') {
                if (data_offset + param->value.s.length > rinstr->data_size) {
                    fprintf(stderr,
                        "%s:%s: format size larger than data size for instruction %u of version %u\n",
                        argv0, current_input, rinstr->id, version);
                    return NULL;
                }
                param->value.s.data = malloc(param->value.s.length);
                memcpy(param->value.s.data, rinstr->data + data_offset,
                    param->value.s.length);
                data_offset += param->value.s.length;

                if (format[i] == 'c') {
                    size_t outsize;
                    util_sillyxor((unsigned char*)param->value.s.data,
                        (unsigned char*)param->value.s.data,
                        param->value.s.length, 119, 7, 16);
                    param->value.s.data = (char*)util_iconv(
                        "UTF-8", "CP932",
                        (unsigned char*)param->value.s.data,
                        param->value.s.length, &outsize);
                }
            } else if (format[i] == 'D') {
                if (data_offset + 4 > rinstr->data_size) {
                    fprintf(stderr,
                        "%s:%s: format size larger than data size for instruction %u of version %u\n",
                        argv0, current_input, rinstr->id, version);
                    return NULL;
                }
                memcpy(&param->value.D[1], rinstr->data + data_offset, 4);
                data_offset += 4;
            }
        }
    }

    if (data_offset != rinstr->data_size) {
        fprintf(stderr,
            "%s:%s: data size differs from format size for instruction %u of version %u\n",
            argv0, current_input, rinstr->id, version);
        return NULL;
    }

    return instr;
}
