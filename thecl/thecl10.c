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
#include <math.h>
#include "expr.h"
#include "path.h"
#include "file.h"
#include "program.h"
#include "thecl.h"
#include "util.h"
#include "value.h"

typedef struct {
PACK_BEGIN
    char magic[4];
    uint16_t unknown1; /* 1 */
    uint16_t include_length; /* include_offset + ANIM+ECLI length */
    uint32_t include_offset; /* sizeof(th10_header_t) */
    uint32_t zero1;
    uint32_t sub_count;
    uint32_t zero2[4];
PACK_END
} PACK_ATTRIBUTE th10_header_t;

typedef struct {
PACK_BEGIN
    char magic[4];
    uint32_t count;
    unsigned char data[];
PACK_END
} PACK_ATTRIBUTE th10_list_t;

typedef struct {
PACK_BEGIN
    char magic[4];
    uint32_t data_offset; /* sizeof(th10_sub_t) */
    uint32_t zero[2];
    unsigned char data[];
PACK_END
} PACK_ATTRIBUTE th10_sub_t;

typedef struct {
PACK_BEGIN
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
    /* From TH13 on, this field stores the number of current stack references
     * in the parameter list. */
    uint32_t zero;
    unsigned char data[];
PACK_END
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
        return value_from_data(data, sizeof(thecl_sub_param_t), 'm', value);
    case 'o':
    case 't':
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
            if (param->value.val.z[z] == '"' || param->value.val.z[z] == '\\')
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
    { 12, "ot" },
    { 13, "ot" },
    { 14, "ot" },
    { 15, "m*D" },
    { 16, "mS*D" },
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
    { 60, "" },
    { 61, "" },
    { 62, "" },
    { 63, "" },
    { 64, "" },
    { 65, "" },
    { 66, "" },
    { 67, "" },
    { 68, "" },
    { 69, "" },
    { 70, "" },
    { 71, "" },
    { 72, "" },
    { 73, "" },
    { 74, "" },
    { 75, "" },
    { 76, "" },
    { 77, "" },
    { 78, "S" },
    { 79, "" },
    { 80, "" },
    { 81, "ffff" },
    { 82, "f" },
    { 83, "S" },
    { 84, "" },
    { 85, "" },
    { 86, "fff" },
    { 87, "fffff" },
    { 88, "" },
    { 89, "SSS" },
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
    { 270, "mffSSSS" },
    { 271, "mffSSSS" },
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
    { 293, "SSf" },
    { 294, "" },
    { 295, "" },
    { 296, "SSf" },
    { 297, "ff" },
    { 298, "ff" },
    { 299, "ff" },
    { 320, "ff" },
    { 321, "ff" },
    { 322, "S" },
    { 323, "S" },
    { 324, "ffff" },
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
    { 348, "SSSx" },
    { 349, "ffff" },
    { 350, "ffffff" },
    { 351, "fff" },
    { 352, "SSSS" },
    { 353, "SSSSSS" },
    { 354, "SSS" },
    { 355, "SSSSS" },
    { 356, "fffff" },
    { 357, "SSSx" },
    { 358, "SSSx" },
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
    { 408, "SSS" },
    { 409, "SSSSSSff" },
    { 410, "" },
    { 411, "SS" },
    { 412, "SSffffSf" },
    { 413, "SSSfffSSSSfS" },
    { 414, "Sff" },
    { 415, "Sff" },
    { 416, "Sf" },
    { 417, "Sf" },
    { 418, "Sf" },
    { 419, "Sf" },
    { 420, "f" },
    { 421, "f" },
    { 422, "Sffffff" },
    { 423, "Sffffffffff" },
    { 424, "Sffff" },
    { 425, "SSSSSSS" },
    { 426, "SSSSSSSSSSS" },
    { 427, "SSSSS" },
    { 428, "SSffSfSf" },
    { 429, "SSSfffSSSSfS" },
    { 430, "Sff" },
    { 431, "SSffSfff" },
    { 432, "SSSfffSSSSfS" },
    { 433, "SSffSfff" },
    { 434, "SSSfffSSSSfS" },
    { 435, "Sffffffff" },
    { 436, "SSSSSSSSS" },
    { -1, NULL }
};

static const id_format_pair_t alcostg_fmts[] = {
    { 16, "mS*D" },
    { 274, "SS" },
    { 275, "SS" },
    { 300, "ffffff" },
    { 301, "ffffff" },
    { 302, "ffffff" },
    { 303, "ffffff" },
    { 369, "SSSS" },
    { 408, "SSS" },
    { 437, "S" },
    { 438, "SS" },
    { 441, "S" },
    { 442, "S" },
    { 443, "S" },
    { 444, "Sf" },
    { -1, NULL }
};

static const id_format_pair_t th11_fmts[] = {
    { 276, "" },
    { 277, "ff" },
    { 278, "S" },
    { 289, "SSfff" },
    { 291, "SSffS" },
    { 305, "Sffffff" },
    { 306, "Sffffff" },
    { 307, "" },
    { 369, "S" },
    { 370, "S" },
    { 371, "S" },
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
    { 12, "ot" },
    { 13, "ot" },
    { 14, "ot" },
    { 15, "m*D" },
    { 16, "mS*D" },
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
    { 71, "" },
    { 72, "" },
    { 73, "" },
    { 74, "" },
    { 75, "" },
    { 76, "" },
    { 77, "" },
    { 78, "S" },
    { 79, "" },
    { 80, "" },
    { 81, "ffff" },
    { 82, "f" },
    { 83, "S" },
    { 84, "" },
    { 85, "" },
    { 86, "fff" },
    { 87, "fffff" },
    { 88, "" },
    { 89, "fff" },
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
    { 270, "mffSSSS" },
    { 271, "mffSSSS" },
    { 272, "SS" },
    { 273, "SSf" },
    { 274, "SS" },
    { 275, "SS" },
    { 276, "" },
    { 277, "Sf" },
    { 278, "Sff" },
    { 279, "Sff" },
    { 280, "mSSSSS" },
    { 281, "SS" },
    { 300, "ff" },
    { 301, "SSff" },
    { 302, "ff" },
    { 303, "SSff" },
    { 304, "ff" },
    { 305, "SSff" },
    { 306, "ff" },
    { 307, "SSff" },
    { 308, "ffff" },
    { 309, "SSfff" },
    { 310, "ffff" },
    { 311, "SSfff" },
    { 312, "SSf" },
    { 313, "SSf" },
    { 314, "" },
    { 315, "" },
    { 316, "ff" },
    { 317, "fff" },
    { 318, "ff" },
    { 319, "ff" },
    { 320, "ffffff" },
    { 321, "SSfffff" },
    { 322, "ffffff" },
    { 323, "SSfffff" },
    { 324, "S" },
    { 325, "Sffffff" },
    { 326, "Sffffff" },
    { 327, "" },
    { 328, "ff" },
    { 329, "Sfff" },
    { 330, "ff" },
    { 331, "SSff" },
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
    { 426, "f" },
    { 427, "SfS" },
    { 428, "SSSx" },
    { 429, "ffff" },
    { 430, "ffffff" },
    { 431, "fff" },
    { 432, "SSSS" },
    { 433, "SSSSSS" },
    { 434, "SSS" },
    { 435, "SSSSS" },
    { 436, "fffff" },
    { 437, "SSSx" },
    { 438, "SSSx" },
    { 439, "SSSx" },
    { 440, "S" },
    { 440, "S" },
    { 442, "" },
    { 443, "" },
    { 444, "S" },
    { 445, "" },
    { 446, "Sf" },
    { 447, "f" },
    { 448, "SSSS" },
    { 449, "S" },
    { 450, "S" },
    { 451, "S" },
    { 452, "f" },
    { 453, "S" },
    { 454, "" },
    { 455, "SS" },
    { 456, "ffS" },
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
    { 514, "Sffffff" },
    { 515, "Sffffffffff" },
    { 516, "Sffff" },
    { 517, "SSSSSSS" },
    { 518, "SSSSSSSSSSS" },
    { 519, "SSSSS" },
    { 520, "fff" },
    { 521, "Sffffffff" },
    { 522, "SSSSSSSSS" },
    { 523, "Sff" },
    { 524, "Sf" },
    { 525, "Sff" },
    { 526, "fS" },
    { 527, "" },
    { 528, "" },
    { 529, "S" },
    { 530, "S" },
    { 531, "S" },
    { 532, "f" },
    { 533, "f" },
    { 534, "S" },
    { 535, "S" },
    { 600, "Sffff" },
    { 601, "SSSSSS" },
    { 602, "S" },
    { 603, "SS" },
    { 604, "Sff" },
    { 605, "Sff" },
    { 606, "Sf" },
    { 607, "Sf" },
    { 608, "Sf" },
    { 609, "Sf" },
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
    { 12, "ot" },
    { 13, "ot" },
    { 14, "ot" },
    { 15, "m*D" },
    { 16, "mS*D" },
    { 17, "S" },
    { 18, "S" },
    { 19, "S" },
    { 20, "SS" },
    { 21, "" },
    { 22, "Sm" },
    { 23, "S" },
    { 24, "f" },
    { 30, "m*D" },
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
    { 62, "" },
    { 63, "" },
    { 65, "" },
    { 64, "" },
    { 66, "" },
    { 67, "" },
    { 68, "" },
    { 69, "" },
    { 70, "" },
    { 71, "" },
    { 72, "" },
    { 73, "" },
    { 74, "" },
    { 75, "" },
    { 76, "" },
    { 77, "" },
    { 78, "S" },
    { 81, "ffff" },
    { 82, "f" },
    { 83, "" },
    { 84, "" },
    { 85, "fff" },
    { 86, "fff" },
    { 87, "fffff" },
    { 88, "fffff" },
    { 89, "fff" },
    { 90, "fffff" },
    { 91, "SfSSff" },
    { 92, "SfSSff" },
    { 93, "ffff" },
    { 300, "mffSSS" },
    { 301, "mffSSS" },
    { 302, "S" },
    { 303, "SS" },
    { 304, "mffSSS" },
    { 305, "mffSSS" },
    { 306, "SS" },
    { 307, "SS" },
    { 308, "SS" },
    { 309, "mffSSS" },
    { 310, "mffSSS" },
    { 311, "mffSSS" },
    { 312, "mffSSS" },
    { 313, "S" },
    { 314, "SS" },
    { 315, "SSf" },
    { 316, "SS" },
    { 317, "SS" },
    { 318, "" },
    { 319, "Sf" },
    { 320, "Sff" },
    { 321, "mSSSSS" },
    { 322, "SS" },
    { 323, "SS" },
    { 324, "S" },
    { 325, "SSSS" },
    { 326, "SSSSSS" },
    { 327, "SS" },
    { 328, "SSSS" },
    { 329, "Sff" },
    { 330, "SSSff" },
    { 331, "SS" },
    { 332, "SSSS" },
    { 333, "SSSff" },
    { 334, "S" },
    { 335, "Sff" },
    { 400, "ff" },
    { 401, "SSff" },
    { 402, "ff" },
    { 403, "SSff" },
    { 404, "ff" },
    { 405, "SSff" },
    { 406, "ff" },
    { 407, "SSff" },
    { 408, "ffff" },
    { 409, "SSfff" },
    { 410, "ffff" },
    { 411, "SSfff" },
    { 412, "SSf" },
    { 413, "SSf" },
    { 414, "" },
    { 415, "" },
    { 416, "fff" },
    { 417, "fff" },
    { 418, "ff" },
    { 419, "ff" },
    { 420, "ffffff" },
    { 421, "SSfffff" },
    { 422, "ffffff" },
    { 423, "SSfffff" },
    { 424, "S" },
    { 425, "Sffffff" },
    { 426, "Sffffff" },
    { 427, "" },
    { 428, "ff" },
    { 429, "SSff" },
    { 430, "ff" },
    { 431, "SSff" },
    { 432, "S" },
    { 433, "S" },
    { 434, "SSSff" },
    { 435, "SSSff" },
    { 436, "SSff" },
    { 437, "SSff" },
    { 438, "SSSff" },
    { 439, "SSSff" },
    { 500, "ff" },
    { 501, "ff" },
    { 502, "S" },
    { 503, "S" },
    { 504, "ffff" },
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
    { 522, "SSSx" },
    { 523, "" },
    { 524, "S" },
    { 525, "" },
    { 526, "f" },
    { 527, "SfS" },
    { 528, "SSSx" },
    { 529, "ffff" },
    { 530, "ffffff" },
    { 531, "fff" },
    { 532, "SSSS" },
    { 533, "SSSSSS" },
    { 534, "SSS" },
    { 535, "SSSSS" },
    { 536, "fffff" },
    { 537, "SSSx" },
    { 538, "SSSx" },
    { 539, "SSSx" },
    { 540, "S" },
    { 541, "S" },
    { 542, "" },
    { 543, "" },
    { 544, "S" },
    { 545, "" },
    { 546, "SS" },
    { 547, "f" },
    { 548, "SSSS" },
    { 549, "S" },
    { 550, "S" },
    { 551, "S" },
    { 552, "S" },
    { 553, "S" },
    { 554, "" },
    { 555, "SS" },
    { 556, "m" },
    { 557, "SSSff" },
    { 558, "S" },
    { 559, "S" },
    { 560, "ff" },
    { 561, "" },
    { 562, "" },
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
    { 617, "Sffffff" },
    { 618, "Sffffffffff" },
    { 619, "Sffff" },
    { 620, "SSSSSSS" },
    { 621, "SSSSSSSSSSS" },
    { 622, "SSSSS" },
    { 623, "fff" },
    { 624, "Sffffffff" },
    { 625, "SSSSSSSSS" },
    { 626, "Sff" },
    { 627, "Sf" },
    { 628, "Sff" },
    { 629, "fS" },
    { 630, "S" },
    { 631, "S" },
    { 632, "S" },
    { 633, "S" },
    { 634, "S" },
    { 635, "f" },
    { 636, "f" },
    { 637, "S" },
    { 638, "S" },
    { 639, "S" },
    { 640, "SSm" },
    { 700, "Sffff" },
    { 701, "SSSSSS" },
    { 702, "S" },
    { 703, "SS" },
    { 704, "Sff" },
    { 705, "Sff" },
    { 706, "Sf" },
    { 707, "Sf" },
    { 708, "Sf" },
    { 709, "Sf" },
    { 710, "S" },
    { 711, "S" },
    { 712, "ff" },
    { 713, "S" },
    { 714, "SS" },
    { 800, "Sm" },
    { 801, "ffS" },
    { 802, "S" },
    { 900, "S" },
    { 1000, "S" },
    { 1001, "S" },
    { 1002, "S" },
    { 1003, "S" },
    { -1, NULL }
};

/* Inherits th13_fmts */
static const id_format_pair_t th14_fmts[] = {
    { 336, "SS" },
    { 337, "SSfff" },
    { 440, "f" },
    { 441, "SSf" },
    { 442, "f" },
    { 443, "SSf" },
    { 444, "f" },
    { 445, "SSf" },
    { 446, "f" },
    { 447, "SSf" },
    { 563, "S" },
    { 564, "f" },
    { 565, "f" },
    { 566, "" },
    { 567, "S" },
    { 568, "S" },
    { 641, "S" },
    { 901, "" },
    { 902, "" },
    { -1, NULL }
};

static const id_format_pair_t th143_fmts[] = {
    { 1004, "" },
    { 1005, "" },
    { 1006, "" },
    { -1, NULL }
};

static const id_format_pair_t th15_fmts[] = {
    { 569, "S" },
    { 570, "" },
    { 571, "" },
    { -1, NULL }
};

static const id_format_pair_t th16_fmts[] = {
    { 337, "SS" },
    { 338, "SSfff" },
    { 339, "SSS" },
    { 340, "S" },
    { 572, "S" },
    { 1000, "SSS" },
    { -1, NULL }
};

static const id_format_pair_t th165_fmts[] = {
    { 1000, "" },
    { 1001, "" },
    { 1002, "f" },
    { 1005, "S" },
    { 1006, "f" },
    { 1007, "S" },
    { 1008, "S" },
    { 1009, "S" },
    { 1010, "f" },
    { 1011, "S" },
    { 1012, "S" },
    { 1013, "" },
    { 1014, "S" },
    { -1, NULL }
};

static const id_format_pair_t th17_fmts[] = {
    { 904, "f" },
    { 1001, "S"},
    { -1, NULL }
};

/* NEWHU: */

static const char*
th10_find_format(
    unsigned int version,
    unsigned int id,
    bool is_timeline)
{
    if (is_timeline) return NULL;

    seqmap_entry_t *ent = seqmap_get(g_eclmap->ins_signatures, id);
    if (ent)
        return ent->value;

    const char* ret = NULL;

    switch (version) {
    /* Intentional fallthroughs, obviously */
    /* NEWHU: */
    case 17:
        if (!ret) ret = find_format(th17_fmts, id);
    case 165:
        if (!ret) ret = find_format(th165_fmts, id);
    case 16:
        if (!ret) ret = find_format(th16_fmts, id);
    case 15:
        if (!ret) ret = find_format(th15_fmts, id);
    case 143:
        if (!ret) ret = find_format(th143_fmts, id);
    case 14:
        if (!ret) ret = find_format(th14_fmts, id);
    case 13:
        if (!ret) ret = find_format(th13_fmts, id);
    case 128:
        if (!ret) ret = find_format(th128_fmts, id);
    case 125:
        if (!ret) ret = find_format(th125_fmts, id);
    case 12:
        if (!ret) ret = find_format(th12_fmts, id);
    case 11:
        if (!ret) ret = find_format(th11_fmts, id);
    case 103:
        if (!ret) ret = find_format(alcostg_fmts, id);
    case 10:
        if (!ret) ret = find_format(th10_fmts, id);
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
    ecl->version = version;

    header = (th10_header_t*)map;
    if (util_strcmp_ref(header->magic, stringref("SCPT")) != 0) {
        fprintf(stderr, "%s:%s: SCPT signature missing\n",
            argv0, current_input);
        return NULL;
    }

    anim_list = (th10_list_t*)(map + header->include_offset);
    if (util_strcmp_ref(anim_list->magic, stringref("ANIM")) != 0) {
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
    if (util_strcmp_ref(ecli_list->magic, stringref("ECLI")) != 0) {
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
        sub->format = NULL;
        sub->forward_declaration = false;
        sub->is_inline = false;
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
        if (util_strcmp_ref(raw_sub->magic, stringref("ECLH")) != 0) {
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

            uint32_t param_mask = instr->param_mask;
            const char* format = th10_find_format(version, instr->id, 0);
            
            size_t param_size_total = instr->size - sizeof(th10_instr_t);
            if (format == NULL) {
                fprintf(stderr, "%s: (total parameter size is %zu)\n",
                    argv0, param_size_total);
                /* Use default format (dump all params as integers) in order to not crash. */
                format = "*S";
            }

            if (param_size_total > 0) {
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

static void
th10_create_forward_declaraion(
    thecl_t* ecl,
    char* name,
    int arity)
{
    thecl_sub_t* sub = malloc(sizeof(thecl_sub_t));
    sub->name = strdup(name);
    sub->format = NULL;
    sub->forward_declaration = true;
    list_init(&sub->instrs);
    list_init(&sub->labels);
    sub->stack = 0;
    sub->arity = arity;
    sub->var_count = 0;
    sub->vars = NULL;

    list_prepend_new(&ecl->subs, sub);
}

void
th10_trans(
    thecl_t* ecl)
{
    thecl_sub_t* sub;

    if (!ecl)
        return;

    if (!g_ecl_rawoutput) list_for_each(&ecl->subs, sub) {
        list_node_t* node;
        list_node_t* node_next;
        list_for_each_node_safe(&sub->instrs, node, node_next) {
            thecl_instr_t* instr = node->data;
            thecl_param_t* param;

            if (instr->type == THECL_INSTR_INSTR) {
                switch(instr->id) {
                    case TH10_INS_STACK_ALLOC:
                        param = list_head(&instr->params);
                        sub->stack = param->value.val.S;
                        break;
                    case TH10_INS_CALL:
                    case TH10_INS_CALL_ASYNC:
                    case TH10_INS_CALL_ASYNC_ID:
                        param = list_head(&instr->params);
                        thecl_sub_t* found_sub = th10_find_sub(ecl, param->value.val.z);
                        int required_param_cnt = instr->id == TH10_INS_CALL_ASYNC_ID ? 2 : 1;
                        if (found_sub)
                            th10_set_arity(found_sub, instr->param_count - required_param_cnt);
                        else
                            th10_create_forward_declaraion(ecl, param->value.val.z, instr->param_count - required_param_cnt);
                        break;
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

    size_t pat_len = strlen(pat);
    size_t rep_len = strlen(rep);

    for (;;) {
        char* ntemp = strstr(temp, pat);
        if (!ntemp) {
            strcpy(str, temp);
            break;
        }
        strncpy(str, temp, ntemp - temp);
        str += ntemp - temp;
        strcpy(str, rep);
        str += rep_len;
        temp = ntemp + pat_len;
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
        node = node->prev;
    }

    return ret;
}

/* TODO: Split this into a function that uses p, and one that uses param.
 *       Have the p function wrap the param function. */
static char*
th10_stringify_param(
    int version,
    thecl_sub_t* sub,
    list_node_t* node,
    size_t p,
    thecl_param_t* param,
    size_t* removed,
    int no_brackets)
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
        thecl_sub_param_t* D = (thecl_sub_param_t*)param->value.val.m.data;

        if (D->zero != 0) {
            fprintf(stderr, "%s: bad ECL file - 'D' param padding is nonzero\n", argv0);
            abort();
        }

        if (D->from == 'f' && D->to == 'f') {
            new_value.val.f = D->val.f;
            sprintf(temp, "_ff");
            new_value.type = 'f';
        } else if (D->from == 'i' && D->to == 'f') {
            new_value.val.f = D->val.f;
            sprintf(temp, "_fS");
            new_value.type = 'S';
        } else if (D->from == 'f' && D->to == 'i') {
            new_value.val.S = D->val.S;
            sprintf(temp, "_Sf");
            new_value.type = 'f';
        } else if (D->from == 'i' && D->to == 'i') {
            new_value.val.S = D->val.S;
            sprintf(temp, "_SS");
            new_value.type = 'S';
        } else {
            fprintf(stderr, "%s: bad ECL file - invalid types in 'D' param\n", argv0);
            abort();
        }

        thecl_param_t temp_param = *param;
        temp_param.type = new_value.type;
        temp_param.value = new_value;
        strcat(temp, " ");
        char* str_temp = th10_stringify_param(version, sub, node, 0, &temp_param, removed, 0);
        strcat(temp, str_temp);
        free(str_temp);

        return strdup(temp);
    }
    default: {
        thecl_instr_t* rep = th10_stack_index(node, *removed);
        if (!g_ecl_rawoutput &&
            param->value.type == 'S' &&
            param->stack &&
            param->value.val.S >= 0 &&
            (param->value.val.S % 4) == 0) {
            sprintf(temp, "$%c", 'A' + param->value.val.S / 4);
        } else if (
            /* TODO: Also check that it is a multiple of four. */
            !g_ecl_rawoutput &&
            param->value.type == 'f' &&
            param->stack &&
            param->value.val.f >= 0.0f) {
            sprintf(temp, "%%%c", 'A' + (int)param->value.val.f / 4);
        } else if (
            param->value.type == 'S' &&
            param->stack &&
            (is_post_th13(version) ? param->value.val.S == -(*removed + 1) : param->value.val.S == -1) &&
            rep &&
            rep->op_type) {
            ++*removed;
                if (rep->op_type != 'S') {
                    sprintf(temp, "_S(%s)", rep->string);
                } else if (!no_brackets) {
                    sprintf(temp, "(%s)", rep->string);
                } else {
                    sprintf(temp, "%s", rep->string);
                }
        } else if (
            param->value.type == 'f' &&
            param->stack &&
            (is_post_th13(version) ? param->value.val.f == -(*removed + 1.0f) : param->value.val.f == -1.0f) &&
            rep &&
            rep->op_type) {
            ++*removed;
                if (rep->op_type != 'f') {
                    sprintf(temp, "_f(%s)", rep->string);
                } else if (!no_brackets) {
                    sprintf(temp, "(%s)", rep->string);
                } else {
                    sprintf(temp, "%s", rep->string);
                }
        } else {
            if (param->stack && (param->value.type == 'f' || param->value.type == 'S')) {
                int val = param->value.type == 'f' ? floor(param->value.val.f) : param->value.val.S;
                seqmap_entry_t* ent = seqmap_get(g_eclmap->gvar_names, val);
                if (ent) {
                    sprintf(temp, "%c%s", param->value.type == 'f' ? '%' : '$', ent->value);
                    return strdup(temp);
                }
            }

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
    int version,
    thecl_sub_t* sub,
    list_node_t* node)
{
    thecl_instr_t* instr = node->data;
    size_t p;
    char string[1024] = { '\0' };
    char temp[256];

    if (instr->type != THECL_INSTR_INSTR)
        return;

    if (instr->id == TH10_INS_STACK_ALLOC && !g_ecl_rawoutput) {
        if (sub->arity * 4 != sub->stack) {
            /* Don't output empty var declarations. */
            strcat(string, "var");
            for (p = sub->arity * 4; p < sub->stack; p += 4) {
                if (p != sub->arity * 4)
                    strcat(string, ",");
                sprintf(temp, " %c", 'A' + p / 4);
                strcat(string, temp);
            }
        }
    } else if (
           (instr->id == TH10_INS_CALL || instr->id == TH10_INS_CALL_ASYNC || instr->id == TH10_INS_CALL_ASYNC_ID)
        && !g_ecl_rawoutput
     ) {
         strcat(string, "@");
         size_t removed = 0;
         char* async_id_str;
         size_t first_sub_param = instr->id == TH10_INS_CALL_ASYNC_ID ? 2 : 1;
         for (p = 0; p < instr->param_count; ++p) {
             thecl_param_t* param = th10_param_index(instr, p);
             if (p == 0) {
                 strcat(string, param->value.val.z);
                 strcat(string, "(");
             } else if (p == 1 && instr->id == TH10_INS_CALL_ASYNC_ID) {
                 async_id_str = th10_stringify_param(version, sub, node, 0, param, &removed, 1);
             } else {
                value_t new_value;
                thecl_sub_param_t* D = (thecl_sub_param_t*)param->value.val.m.data;

                if (D->zero != 0) {
                    fprintf(stderr, "%s: bad ECL file - 'D' param padding is nonzero\n", argv0);
                    abort();
                }

                if (D->from == 'f' && D->to == 'f') {
                    new_value.val.f = D->val.f;
                    strcpy(temp, "%s");
                    new_value.type = 'f';
                }
                else if (D->from == 'i' && D->to == 'f') {
                    new_value.val.f = D->val.f;
                    strcpy(temp, "_f(%s)");
                    new_value.type = 'S';
                }
                else if (D->from == 'f' && D->to == 'i') {
                    new_value.val.S = D->val.S;
                    strcpy(temp, "_S(%s)");
                    new_value.type = 'f';
                }
                else if (D->from == 'i' && D->to == 'i') {
                    new_value.val.S = D->val.S;
                    strcpy(temp, "%s");
                    new_value.type = 'S';
                }
                else {
                    fprintf(stderr, "%s: bad ECL file - invalid types in 'D' param\n", argv0);
                    abort();
                }

                thecl_param_t temp_param = *param;
                temp_param.type = new_value.type;
                temp_param.value = new_value;
                char* param_string = th10_stringify_param(version, sub, node, 0, &temp_param, &removed, 1);
                char temp2[256];
                sprintf(temp2, temp, param_string);
                free(param_string);

                if (p != first_sub_param)
                    strcat(string, ", ");
                strcat(string, temp2);
             }
         }
         for (size_t s = 0; s < removed; ++s) {
            list_node_t* prev = node->prev;
            thecl_instr_free(prev->data);
            list_del(&sub->instrs, prev);
        }
        strcat(string, ")");
        if (instr->id == TH10_INS_CALL_ASYNC)
            strcat(string, " async");
        else if (instr->id == TH10_INS_CALL_ASYNC_ID) {
            strcat(string, " async ");
            strcat(string, async_id_str);
            free(async_id_str);
        }
     } else {
        seqmap_entry_t *ent = seqmap_get(g_eclmap->ins_names, instr->id);
        if (ent) {
            sprintf(string, "%s(", ent->value);
        }
        else {
            sprintf(string, "ins_%u(", instr->id);
        }

        size_t removed = 0;
        for (p = 0; p < instr->param_count; ++p) {
            char* param_string = th10_stringify_param(version, sub, node, p, NULL, &removed, 1);
            if (p != 0)
               strcat(string, ", ");
            strcat(string, param_string);
            free(param_string);
        }
        for (size_t s = 0; s < removed; ++s) {
            list_node_t* prev = node->prev;
            thecl_instr_free(prev->data);
            list_del(&sub->instrs, prev);
        }
        strcat(string, ")");
    }
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
        fprintf(out, "\nvoid %s(", sub->name);
        for (p = 0; p < sub->arity; ++p) {
            if (p != 0)
                fprintf(out, ", ");
            fprintf(out, "var %c", 'A' + p);
        }
        fprintf(out, ")");

        if (sub->forward_declaration) {
            fprintf(out, ";\n");
            continue;
        }

        fprintf(out, "\n{\n");

        list_node_t* node;
        list_node_t* node_next;
        unsigned int time_last = 0;
        list_for_each_node_safe(&sub->instrs, node, node_next) {
            thecl_instr_t* instr = node->data;

            char temp[1024] = { '\0' };

            switch (instr->type) {
            case THECL_INSTR_TIME:
                sprintf(temp, "+%u: //%u", instr->time - time_last, instr->time);
                time_last = instr->time;
                instr->string = strdup(temp);
                break;
            case THECL_INSTR_RANK:
                if(instr->rank == 0xFF)
                    instr->string = strdup("!*");
                else if(instr->rank == (is_post_th13(ecl->version) ? 0xC0 : 0xF0))
                    instr->string = strdup("!-");
                else {
                    if (is_post_th13(ecl->version)) {
                        sprintf(temp, "!%s%s%s%s%s%s%s%s",
                                (instr->rank) & RANK_EASY      ? "E" : "",
                                (instr->rank) & RANK_NORMAL    ? "N" : "",
                                (instr->rank) & RANK_HARD      ? "H" : "",
                                (instr->rank) & RANK_LUNATIC   ? "L" : "",
                                (instr->rank) & RANK_EXTRA     ? "X" : "",
                                (instr->rank) & RANK_OVERDRIVE ? "O" : "",
                                !((instr->rank) & RANK_ID_6)   ? "6" : "",
                                !((instr->rank) & RANK_ID_7)   ? "7" : "");
                    } else {
                        sprintf(temp, "!%s%s%s%s%s%s%s%s",
                                (instr->rank) & RANK_EASY      ? "E" : "",
                                (instr->rank) & RANK_NORMAL    ? "N" : "",
                                (instr->rank) & RANK_HARD      ? "H" : "",
                                (instr->rank) & RANK_LUNATIC   ? "L" : "",
                                !((instr->rank) & RANK_ID_4)   ? "4" : "",
                                !((instr->rank) & RANK_ID_5)   ? "5" : "",
                                !((instr->rank) & RANK_ID_6)   ? "6" : "",
                                !((instr->rank) & RANK_ID_7)   ? "7" : "");
                    }
                    instr->string = strdup(temp);
                }
                break;
            case THECL_INSTR_LABEL:
                sprintf(temp, "%s_%u:", sub->name, instr->offset);
                instr->string = strdup(temp);
                break;
            case THECL_INSTR_INSTR: {
                const expr_t* expr = g_ecl_rawoutput ? NULL : expr_get_by_id(ecl->version, instr->id);

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
                        } else if (!expr_is_leaf(10, rep->id) && !expr->no_brackets) {
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
                        char* rep_str = th10_stringify_param(ecl->version, sub, node, p, NULL, &removed, 1);
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
                    th10_stringify_instr(ecl->version, sub, node);
                }
                break;
            }
            }
        }

        list_for_each(&sub->instrs, instr) {
            if (instr->string[0] != '\0') { /* In some cases string can be empty, like ins_40(0) dumps. */
                if (instr->type == THECL_INSTR_INSTR) {
                    fprintf(out, "    %s;\n", instr->string);
                } else {
                    fprintf(out, "%s\n", instr->string);
                }
            }
        }

        fprintf(out, "}\n");
    }
}

static size_t
th10_instr_size(
    unsigned int version,
    const thecl_instr_t* instr,
    bool is_timeline)
{
    if (is_timeline) return 0;
    
    size_t ret = sizeof(th10_instr_t);
    thecl_param_t* param;

    list_for_each(&instr->params, param) {
        /* XXX: I think 'z' is what will be passed ... */
        if (param->type == 'm' || param->type == 'x') {
            size_t zlen = strlen(param->value.val.z);
            ret += sizeof(uint32_t) + zlen + (4 - (zlen % 4));
        } else if (param->type == 'o' || param->type == 't') {
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
    char* filename,
    unsigned int version)
{
    parser_state_t state;

    state.instr_time = 0;
    state.instr_rank = 0xff;
    state.instr_flags = 0;
    state.version = version;
    state.uses_numbered_subs = false;
    state.has_overdrive_difficulty = is_post_th13(version);
    state.uses_stack_offsets = is_post_th13(version);
    list_init(&state.expressions);
    list_init(&state.block_stack);
    list_init(&state.global_definitions);
    state.scope_stack = NULL;
    state.scope_cnt = 0;
    state.scope_id = 0;
    state.current_sub = NULL;
    state.ecl = thecl_new();
    state.ecl->version = version;
    state.instr_format = th10_find_format;
    state.instr_size = th10_instr_size;

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

static const unsigned char*
th10_find_sub_format(
    char* sub_name,
    const list_t* subs)
{
    const thecl_sub_t* sub;
    list_for_each(subs, sub) {
        if (!sub->is_inline && !strcmp(sub->name, sub_name)) return sub->format;
    }
    return NULL;
}

static unsigned char*
th10_instr_serialize(
    unsigned int version,
    thecl_sub_t* sub,
    thecl_instr_t* instr,
    const list_t* subs,
    bool no_warn)
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

    seqmap_entry_t* ent = seqmap_get(g_eclmap->ins_names, instr->id);
    char buf[128];
    if (ent == NULL)
        snprintf(buf, sizeof(buf), "%d", instr->id);
    else
        snprintf(buf, sizeof(buf), "%d (%s)", instr->id, ent->value);

    const char* expected_format = th10_find_format(version, instr->id, 0);
    if (expected_format == NULL)
        fprintf(stderr, "%s:th10_instr_serialize: in sub %s: instruction with id %s is not known to exist in version %d\n", argv0, sub->name, buf, version);
    else {
        list_for_each(&instr->params, param) {
            if (expected_format[0] == 0) {
                fprintf(stderr, "%s:th10_instr_serialize: in sub %s: too many arguments for opcode %s\n", argv0, sub->name, buf);
                break;
            }
            if (expected_format[0] != '*') expected_format++;
        }
        if (expected_format[0] != '*' && expected_format[0] != 0)
            fprintf(stderr, "%s:th10_instr_serialize: in sub %s: too few arguments for opcode %s\n", argv0, sub->name, buf);
    }

    if (!g_ecl_simplecreate && (instr->id == TH10_INS_CALL || instr->id == TH10_INS_CALL_ASYNC || instr->id == TH10_INS_CALL_ASYNC_ID)) {
        /* Validate sub call parameters. */
        list_node_t* node = instr->params.head;
        thecl_param_t* sub_name_param = node->data;
        char* sub_name = sub_name_param->value.val.z;
        const char* format = th10_find_sub_format(sub_name, subs);
        if (format == NULL) {
            if (!no_warn)
                fprintf(stderr, "%s:th10_instr_serialize: in sub %s: unknown sub call \"%s\" (use the #nowarn \"true\" directive to disable this warning)\n",
                        argv0, sub->name, sub_name);
        } else {
            size_t v = 0;
            int first_iter = 1;
            while(node = node->next) {
                /* CALL_ASYNC_ID has an extra param for slot ID (type verified in ecsparse) */
                if (instr->id == TH10_INS_CALL_ASYNC_ID && first_iter) {
                    first_iter = 0;
                    continue;
                }

                thecl_param_t* param = node->data;
                thecl_sub_param_t* D = (thecl_sub_param_t*)param->value.val.m.data;
                if (format[v] == '\0') {
                    fprintf(stderr, "%s:th10_instr_serialize: in sub %s: too many parameters when calling sub \"%s\"\n",
                            argv0, sub->name, sub_name);
                    break;
                 } else if (
                    format[v] != '?' &&
                    ((D->to == 'i' && format[v] == 'f') ||
                    (D->to == 'f' && format[v] == 'S'))
                ) {
                    fprintf(stderr, "%s:th10_instr_serialize: in sub %s: wrong type for parameter %i when calling sub \"%s\", expected type: %c\n",
                            argv0, sub->name, (int)(v + 1), sub_name, format[v]);
                }
                ++v;
            }
            if (format[v] != '\0') 
                fprintf(stderr, "%s:th10_instr_serialize: in sub %s: not enough parameters when calling sub %s\n",
                        argv0, sub->name, sub_name);
        }
    }

    list_for_each(&instr->params, param) {
        if (param->stack)
            ret->param_mask |= 1 << param_count;
        ++param_count;
        if (param->type == 'o') {
            /* This calculates the relative offset from the current instruction. */
            uint32_t label = label_offset(sub, param->value.val.z) - instr->offset;
            memcpy(param_data, &label, sizeof(uint32_t));
            param_data += sizeof(uint32_t);
        } else if (param->type == 't') {
            /* Time value for jump instructions - either read from a label or directly
               from param value, depenging on how the jump was created. */
            int32_t time;
            if (param->value.type == 'z') {
                time = label_time(sub, param->value.val.z);
            } else {
                time = param->value.val.S;
            }
            memcpy(param_data, &time, sizeof(int32_t));
            param_data += sizeof(int32_t);
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

        if (param->stack && is_post_th13(version)) {
            if (param->type == 'f' && param->value.val.f == -(ret->zero + 1.0f)) {
                ++ret->zero;
            } else if (param->type == 'S' && param->value.val.S == -(ret->zero + 1)) {
                ++ret->zero;
            } else if (param->type == 'D') {
                thecl_sub_param_t *temp = (thecl_sub_param_t*)param->value.val.m.data;

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
        if (sub->forward_declaration || sub->is_inline)
            continue;

        if (!file_write(out, sub->name, strlen(sub->name) + 1))
            return 0;
    }

    pos = file_tell(out);
    if (pos % 4 != 0)
        file_seek(out, pos + 4 - pos % 4);

    uint16_t max_opcode;
    /* TODO: Get max opcodes for the rest of the games */
    switch (ecl->version)
    {
    case 14:
        max_opcode = 1003;
        break;
    default:
        max_opcode = 0xFFFFU;
        break;
    }

    list_for_each(&ecl->subs, sub) {
        if (sub->forward_declaration || sub->is_inline)
            continue;

        thecl_instr_t* instr;
        sub->offset = file_tell(out);
        if (!file_write(out, &sub_header, sizeof(th10_sub_t)))
            return 0;

        list_for_each(&sub->instrs, instr) {
            if (instr->id > max_opcode) {
                fprintf(stderr, "%s: warning: opcode: id %hu was higher than the maximum %hu\n", argv0, instr->id, max_opcode);
            }
            unsigned char* data = th10_instr_serialize(ecl->version, sub, instr, &ecl->subs, ecl->no_warn);
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
        if (sub->forward_declaration || sub->is_inline)
            continue;
        if (!file_write(out, &sub->offset, sizeof(uint32_t)))
            return 0;
    }

    return 1;
}

static void
th10_create_header(
    const thecl_t* ecl,
    FILE* out
) {
    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {
        if (sub->forward_declaration || sub->is_inline)
            continue;

        fprintf(out, "\nvoid %s(", sub->name);
        for (ssize_t i = 0; i < sub->arity; ++i) {
            thecl_variable_t* var = sub->vars[i];
            if (i != 0) fprintf(out, ", ");

            if (var->type == 'S') fprintf(out, "int ");
            else if (var->type == 'f') fprintf(out, "float ");
            else fprintf(out, "var ");

            fprintf(out, var->name);
        }
        fprintf(out, ");\n");
    }
}

const thecl_module_t th10_ecl = {
    .open = th10_open,
    .trans = th10_trans,
    .dump = th10_dump,
    .parse = th10_parse,
    .compile = th10_compile,
    .create_header = th10_create_header
};
