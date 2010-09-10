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
#ifndef ECL_H_
#define ECL_H_

#include <config.h>
#include <inttypes.h>

#define RANK_EASY    (1 << 0)
#define RANK_NORMAL  (1 << 1)
#define RANK_HARD    (1 << 2)
#define RANK_LUNATIC (1 << 3)

#define ECLDUMP_MODE_NORMAL     0
#define ECLDUMP_MODE_RAWDUMP    1
#define ECLDUMP_MODE_PARAMETERS 2

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t unknown1; /* 1 */
    uint16_t include_length; /* include_offset + ANIM+ECLI length */
    uint32_t include_offset; /* 0x24 */
    uint32_t zero1;
    uint32_t sub_cnt;
    uint32_t zero2[4];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE header_scpt_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    /* Size of an instruction header? */
    uint32_t unknown1; /* 0x10 */
    uint32_t zero[2];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE header_eclh_t;

typedef struct param_t {
    /* Same as instruction format strings. */
    unsigned char type;
    /* Reference to stack value or not. */
    unsigned int stack;
    union {
        int32_t i;
        float f;
        int32_t D[2];
        struct {
            uint32_t length;
            char* data;
        } s;
    } value;
} param_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint32_t time;
    uint16_t id;
    /* Size of the entire instruction. */
    uint16_t size;
    uint16_t param_mask;
    uint8_t rank_mask;
    uint8_t param_cnt;
    /* uint32_t zero; */
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
    param_t* params;
    uint32_t offset;
    unsigned int label;
    char* string;
    int type;
} PACK_ATTRIBUTE instr_t;

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
    uint8_t param_cnt;
    uint32_t zero;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
    /* extra: */
    uint16_t data_size;
    char* data;
    uint32_t offset;
} PACK_ATTRIBUTE raw_instr_t;

typedef struct {
    char* name;
    int32_t offset;
} label_t;

typedef struct {
    char* name;
    /* Used both for the file offset and instruction offsets. */
    uint32_t offset;
    unsigned int instr_cnt;
    raw_instr_t* raw_instrs;
    instr_t* instrs;
    unsigned int label_cnt;
    label_t* labels;
    unsigned int var_cnt;
    char** vars;
    int stack;
    int arity;
} sub_t;

typedef struct {
    header_scpt_t scpt;

    uint32_t anim_cnt;
    char** anim;

    uint32_t ecli_cnt;
    char** ecli;

    uint16_t sub_cnt;
    sub_t* subs;
} ecl_t;

#endif
