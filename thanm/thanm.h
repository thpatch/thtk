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

extern anmmap_t* g_anmmap;
extern unsigned int option_force;

const id_format_pair_t* anm_get_formats(uint32_t version);

typedef struct {
    anm_offset_t* offset;
    /* instrs of thanm_instr_t format */
    list_t instrs;
    /* instrs of anm_instr_t format */
    list_t raw_instrs;
    /* list of label_t */
    list_t labels;
} anm_script_t;

typedef struct {
    uint16_t type;
    const char* format;
} opcode_fmt_t;

typedef struct {
    anm_header06_t* header;
    thtx_header_t* thtx;

    char* name;
    char* name2;

    /* List of sprite_t*. */
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
    uint32_t size;
    list_t params;
} thanm_instr_t;

thanm_instr_t* thanm_instr_new();

typedef struct thanm_param_t {
    int type;
    int is_var;
    value_t* val;
} thanm_param_t;

thanm_param_t* thanm_param_new(int type);

/* Parser things. */
typedef struct {
    /* Use to idicate that the compilation should not
     * continue after parsing is finished. */
    int was_error;
    int16_t time;
    int32_t default_version;
    uint32_t offset;
    uint32_t sprite_id;
    uint32_t script_id;
    /* List of anm_entry_t */
    list_t entries;
    anm_entry_t* current_entry;
    anm_script_t* current_script;

    list_t sprite_names;
    list_t script_names;
} parser_state_t;

typedef struct symbol_id_pair_t {
    uint32_t id;
    char* name;
} symbol_id_pair_t;

typedef struct label_t {
    uint32_t offset;
    char* name;
} label_t;

label_t* label_find(anm_script_t* script, char* name);

extern FILE* yyin;
extern int yyparse(parser_state_t*);

#endif
