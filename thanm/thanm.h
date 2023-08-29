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
#ifndef THANM_H_
#define THANM_H_

#include <config.h>
#include <anm_types.h>
#include "anmmap.h"
#include "value.h"
#include "list.h"
#include "path.h"

extern anmmap_t* g_anmmap;
extern unsigned int option_force;

const char *anm_find_format(unsigned version, unsigned header_version, int id);

typedef struct {
    /* The id in the offset struct may not be the real index. */
    int32_t real_index;
    anm_offset_t* offset;
    /* instrs of thanm_instr_t format */
    list_t instrs;
    /* instrs of anm_instr_t format */
    list_t raw_instrs;
    /* list of label_t */
    list_t labels;
    /* list of var_t */
    list_t vars;
} anm_script_t;

anm_script_t* anm_script_new(void);

typedef struct {
    uint16_t type;
    const char* format;
} opcode_fmt_t;

typedef struct {
    anm_header06_t* header;
    thtx_header_t* thtx;

    char* name;
    char* name2;
    char* filename;

    /* List of sprite19_t*. */
    list_t sprites;
    /* List of anm_script_t*. */
    list_t scripts;

    unsigned char* data;
} anm_entry_t;

typedef struct {
    unsigned char* map;
    long map_size;

    /* List of const char*. */
    list_t names;
    /* List of anm_entry_t*. */
    list_t entries;
} anm_archive_t;

enum {
    THANM_INSTR_INSTR,
    THANM_INSTR_TIME,
    THANM_INSTR_LABEL
};

typedef struct {
    int type;
    uint16_t id;
    uint16_t param_mask;
    int16_t time;
    uint32_t offset;
    uint32_t address;
    uint32_t size;
    list_t params;
} thanm_instr_t;

uint32_t instr_get_size(thanm_instr_t* instr, int32_t version);

typedef struct expr_t expr_t;

typedef struct thanm_param_t {
    int type;
    int is_var;
    expr_t* expr;
    value_t* val;
} thanm_param_t;

thanm_param_t* thanm_param_new(int type);
void thanm_param_free(thanm_param_t* param);

/* Parser things. */
typedef struct parser_state_t {
    /* Use to idicate that the compilation should not
     * continue after parsing is finished. */
    int was_error;
    int16_t time;
    int32_t default_version;
    int32_t current_version;
    uint32_t offset;
    uint32_t sprite_id;
    int32_t script_id;
    int32_t script_real_index;
    /* List of anm_entry_t */
    list_t entries;
    anm_entry_t* current_entry;
    anm_script_t* current_script;

    list_t globals;
    list_t sprite_names;
    list_t script_names;

    path_state_t path_state;

    char symbol_prefix[32];
    FILE* symbolfp;

    int version;
} parser_state_t;

typedef struct symbol_id_pair_t {
    uint32_t id;
    char* name;
} symbol_id_pair_t;

typedef struct label_t {
    uint32_t offset;
    int16_t time;
    char* name;
} label_t;

label_t* label_find(anm_script_t* script, char* name);

typedef struct global_t {
    char* name;
    thanm_param_t* param;
} global_t;

typedef struct reg_t reg_t;

typedef struct var_t {
    char* name;
    int type;
    reg_t* reg;
} var_t;

PACK_BEGIN;
typedef struct jpeg_sof_t {
    uint8_t SOF_marker[2];
    uint8_t length[2];
    uint8_t precision;
    uint8_t height[2];
    uint8_t width[2];
} jpeg_sof_t;
PACK_END;

PACK_BEGIN;
typedef struct png_IHDR_t {
    uint8_t length[4];
    uint8_t magic[4];
    uint8_t width[4];
    uint8_t height[4];
    uint8_t color_type;
    uint8_t compression;
    uint8_t filter;
    uint8_t interlace;
} png_IHDR_t;
PACK_END;

var_t* var_new(char* name, int type);

void var_free(var_t* var);

thanm_instr_t* thanm_instr_new(void);
thanm_instr_t* instr_new(parser_state_t* state, uint16_t id, list_t* params);

#define DEFAULTVAL 0xffff

extern FILE* thanm_yyin;
extern int thanm_yyparse(parser_state_t*);

#endif
