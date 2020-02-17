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
%{
#include <config.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "file.h"
#include "list.h"
#include "program.h"
#include "thanm.h"
#include "value.h"

/* Bison things. */
void yyerror(const parser_state_t*, const char*, ...);
int yylex(void);
extern FILE* yyin;

typedef struct prop_list_entry_value_t {
    int type;
    union {
        int S;
        float f;
        char* t;
        list_t* l;
    } val;
} prop_list_entry_value_t;

typedef struct prop_list_entry_t {
    char* key;
    /* Not using value_t due to the need of list_t. */
    prop_list_entry_value_t* value;
} prop_list_entry_t;

/* Returns the entry with the given key, or NULL if it's not found. */
static prop_list_entry_t* prop_list_find(list_t* list, const char* key);

/* Recursively frees the property list content. Does not free the list itself. */
static void prop_list_free_nodes(list_t* list);

/* Creates a new instruction. */
static thanm_instr_t* instr_new(parser_state_t* state, uint16_t id, list_t* params);

/* Returns instruction number given an identifier, or -1 if it's not an instruction. */
static int identifier_instr(char* ident);

%}

%define parse.error verbose
%locations
%parse-param {parser_state_t* state}

%union {
    /* Token types (from flex) */
    int integer;
    float floating;
    char* string;

    /* Internal types */
    struct thanm_param_t* param;
    struct list_t* list;
    struct prop_list_entry_t* prop_list_entry;
    struct prop_list_entry_value_t* prop_list_entry_value;
}

%token <floating> FLOATING "floating"
%token <integer> INTEGER "integer"
%token <string> IDENTIFIER "identifier"
%token <string> TEXT "text"

%token PLUS "+"
%token COMMA ","
%token COLON ":"
%token SEMICOLON ";"
%token SQUARE_OPEN "["
%token SQUARE_CLOSE "]"
%token BRACE_OPEN "{"
%token BRACE_CLOSE "}"
%token PARENTHESIS_OPEN "("
%token PARENTHESIS_CLOSE ")"
%token MODULO "%"
%token DOLLAR "$"
%token ENTRY "entry"
%token SCRIPT "script"

%token ILLEGAL_TOKEN "invalid token"
%token END_OF_FILE 0 "end of file"

%type <param> ParameterLiteral
%type <param> ParameterVar
%type <param> Parameter
%type <list> PropertyList
%type <list> Parameters
%type <list> ParametersList
%type <prop_list_entry> PropertyListEntry
%type <prop_list_entry_value> PropertyListValue

%%

Statements:
    %empty
    | Statements Statement

Statement:
    Entry
    | Script

Entry:
    "entry" IDENTIFIER[entry_name] "{" PropertyList[prop_list] "}" {
        anm_entry_t* entry = (anm_entry_t*)malloc(sizeof(anm_entry_t));
        entry->header = (anm_header06_t*)calloc(1, sizeof(anm_header06_t));
        entry->thtx = (thtx_header_t*)calloc(1, sizeof(thtx_header_t));

        entry->thtx->magic[0] = 'T';
        entry->thtx->magic[1] = 'H';
        entry->thtx->magic[2] = 'T';
        entry->thtx->magic[3] = 'X';

        entry->name = NULL;
        entry->name2 = NULL;
        list_init(&entry->sprites);
        list_init(&entry->scripts);
        entry->data = NULL;

        prop_list_entry_t* prop;
        #define REQUIRE(x, y, l) \
            prop = prop_list_find(l, x); \
            if (prop == NULL)  { \
                yyerror(state, "missing entry property: '" x "'"); \
                return 1; \
            } else if (prop->value->type != y) { \
                yyerror(state, "wrong value type for entry property: '" x "'"); \
                return 1; \
            }

        #define OPTIONAL(x, y, l) \
            prop = prop_list_find(l, x); \
            if (prop && prop->value->type != y) { \
                yyerror(state, "wrong value type for entry property: '" x "'"); \
                return 1; \
            }

        REQUIRE("version", 'S', $prop_list);
        entry->header->version = prop->value->val.S;

        REQUIRE("name", 't', $prop_list);
        entry->name = strdup(prop->value->val.t);

        OPTIONAL("name2", 't', $prop_list);
        if (prop) entry->name2 = strdup(prop->value->val.t);

        REQUIRE("format", 'S', $prop_list);
        entry->header->format = prop->value->val.S;

        REQUIRE("width", 'S', $prop_list);
        entry->header->w = prop->value->val.S;

        REQUIRE("height", 'S', $prop_list);
        entry->header->h = prop->value->val.S;

        OPTIONAL("xOffset", 'S', $prop_list);
        entry->header->x = prop ? prop->value->val.S : 0;

        OPTIONAL("yOffset", 'S', $prop_list);
        entry->header->y = prop ? prop->value->val.S : 0;

        OPTIONAL("colorKey", 'S', $prop_list);
        entry->header->colorkey = prop ? prop->value->val.S : 0;

        OPTIONAL("memoryPriority", 'S', $prop_list);
        entry->header->memorypriority =
            prop ? prop->value->val.S : (entry->header->version >= 1 ? 10 : 0);

        OPTIONAL("lowResScale", 'S', $prop_list);
        entry->header->lowresscale = prop ? prop->value->val.S : 0;

        OPTIONAL("hasData", 'S', $prop_list);
        entry->header->hasdata = prop ? prop->value->val.S : 0;

        if (entry->header->hasdata) {
            REQUIRE("THTXSize", 'S', $prop_list);
            entry->thtx->size = prop->value->val.S;

            REQUIRE("THTXFormat", 'S', $prop_list);
            entry->thtx->format = prop->value->val.S;

            REQUIRE("THTXWidth", 'S', $prop_list);
            entry->thtx->w = prop->value->val.S;
        
            REQUIRE("THTXHeight", 'S', $prop_list);
            entry->thtx->h = prop->value->val.S;

            OPTIONAL("THTXZero", 'S', $prop_list);
            entry->thtx->zero = prop ? prop->value->val.S : 0;
        }

        OPTIONAL("sprites", 'l', $prop_list);
        if (prop) {
            list_for_each(prop->value->val.l, prop) {
                if (prop->value->type != 'l') {
                    yyerror(state, "%s: expected property list for sprite definition, got a single value instead", prop->key);
                    continue;
                }
                list_t* inner_list = prop->value->val.l;
                char* name = prop->key;

                sprite_t* sprite = (sprite_t*)malloc(sizeof(sprite_t));

                OPTIONAL("id", 'S', inner_list);
                if (prop) state->sprite_id = prop->value->val.S;
                sprite->id = state->sprite_id++;

                REQUIRE("x", 'S', inner_list);
                sprite->x = (float)prop->value->val.S;
                REQUIRE("y", 'S', inner_list);
                sprite->y = (float)prop->value->val.S;
                REQUIRE("w", 'S', inner_list);
                sprite->w = (float)prop->value->val.S;
                REQUIRE("h", 'S', inner_list);
                sprite->h = (float)prop->value->val.S;
                list_append_new(&entry->sprites, sprite);

                symbol_id_pair_t* symbol = (symbol_id_pair_t*)malloc(sizeof(symbol_id_pair_t));
                symbol->id = sprite->id;
                symbol->name = strdup(name);
                list_append_new(&state->sprite_names, symbol);
            }
        }

        #undef OPTIONAL
        #undef REQUIRE

        free($prop_list);
        free($entry_name);
        list_append_new(&state->entries, entry);
        state->current_entry = entry;
    }

PropertyList:
    %empty {
        $$ = list_new();
    }
    | PropertyList[list] PropertyListEntry[prop] {
        list_append_new($list, $prop);
    }

PropertyListEntry:
    IDENTIFIER[key] ":" PropertyListValue[val] ";" {
        $$ = (prop_list_entry_t*)malloc(sizeof(prop_list_entry_t));
        $$->key = $key;
        $$->value = $val;
    }

PropertyListValue:
    INTEGER {
        $$ = (prop_list_entry_value_t*)malloc(sizeof(prop_list_entry_value_t));
        $$->type = 'S';
        $$->val.S = $1;
    }
    | FLOATING {
        $$ = (prop_list_entry_value_t*)malloc(sizeof(prop_list_entry_value_t));
        $$->type = 'f';
        $$->val.f = $1;
    }
    | TEXT {
        $$ = (prop_list_entry_value_t*)malloc(sizeof(prop_list_entry_value_t));
        $$->type = 't';
        $$->val.t = $1;
    }
    | "{" PropertyList "}" {
        $$ = (prop_list_entry_value_t*)malloc(sizeof(prop_list_entry_value_t));
        $$->type = 'l';
        $$->val.l = $2;
    }

Script:
    "script" ScriptOptionalId IDENTIFIER[name] {
        anm_script_t* script = (anm_script_t*)malloc(sizeof(anm_script_t));
        list_init(&script->instrs);
        list_init(&script->raw_instrs);
        script->offset = malloc(sizeof(*script->offset));
        script->offset->id = state->script_id++;

        symbol_id_pair_t* symbol = (symbol_id_pair_t*)malloc(sizeof(symbol_id_pair_t));
        symbol->id = script->offset->id;
        symbol->name = $name;
        list_append_new(&state->script_names, symbol);

        list_append_new(&state->current_entry->scripts, script);
        state->current_script = script;
        state->offset = 0;
        state->time = 0;
    } "{" ScriptStatements "}" {
        state->current_script= NULL;
    }

ScriptOptionalId:
    %empty
    | INTEGER[id] {
        state->script_id = $id;
    }

ScriptStatements:
    %empty
    | ScriptStatements ScriptStatement

ScriptStatement:
    INTEGER[time] ":" {
        state->time = $time;
    }
    | "+" INTEGER[time] ":" {
        state->time += $time;
    }
    | IDENTIFIER[name] ":" {
        label_t* label = (label_t*)malloc(sizeof(label_t));
        label->name = $name;
        label->offset = state->offset;
        list_append_new(&state->labels, label);
    }
    | IDENTIFIER[ident] "(" Parameters[params] ")" ";" {
        int id = identifier_instr($ident);
        if (id == -1) {
            yyerror(state, "unknown mnemonic: %s", $ident);
            return 1;
        }
        thanm_instr_t* instr = instr_new(state, id, $params);
        list_append_new(&state->current_script->instrs, instr);

        free($ident);
    }

Parameters:
    %empty {
        $$ = list_new();
    }
    | ParametersList {
        $$ = $1;
    }

ParametersList:
    Parameter[param] {
        $$ = list_new();
        list_append_new($$, $param);
    }
    | ParametersList[list] "," Parameter[param] {
        list_append_new($list, $param);
    }

Parameter:
    ParameterLiteral {
        $$ = $1;
    }
    | ParameterVar {
        $$ = $1;
    }

ParameterLiteral:
    INTEGER {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'S';
        val->val.S = $1;
        thanm_param_t* param = thanm_param_new('S');
        param->val = val;
        $$ = param;
    }
    | FLOATING {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'f';
        val->val.f = $1;
        thanm_param_t* param = thanm_param_new('f');
        param->val = val;
        $$ = param;
    }
    | TEXT {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'z';
        val->val.z = $1;
        thanm_param_t* param = thanm_param_new('z');
        param->val = val;
        $$ = param;
    }

ParameterVar:
    "[" INTEGER "]" {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'S';
        val->val.S = $2;
        thanm_param_t* param = thanm_param_new('S');
        param->is_var = 1;
        param->val = val;
        $$ = param;
    }
    | "[" FLOATING "]" {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'f';
        val->val.f = $2;
        thanm_param_t* param = thanm_param_new('f');
        param->is_var = 1;
        param->val = val;
        $$ = param;
    }
    | "$" IDENTIFIER {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'S';
        thanm_param_t* param = thanm_param_new('S');
        param->is_var = 1;
        param->val = val;

        seqmap_entry_t* ent = seqmap_find(g_anmmap->gvar_names, $2);
        if (ent == NULL) {
            yyerror(state, "unknown variable: %s", $2);
            param->val->val.S = 0;
        } else {
            param->val->val.S = ent->key;
        }
        free($2);
        $$ = param;
    }
    | "%" IDENTIFIER {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'f';
        thanm_param_t* param = thanm_param_new('f');
        param->is_var = 1;
        param->val = val;

        seqmap_entry_t* ent = seqmap_find(g_anmmap->gvar_names, $2);
        if (ent == NULL) {
            yyerror(state, "unknown variable: %s", $2);
            param->val->val.f = 0.0f;
        } else {
            param->val->val.f = (float)ent->key;
        }
        free($2);
        $$ = param;
    }
    | IDENTIFIER {
        /* This can either be a variable or a label...
         * Do the same as thecl - check if a variable exists, otherwise use as label. */
        value_t* val = (value_t*)malloc(sizeof(value_t));
        thanm_param_t* param;
        seqmap_entry_t* ent = seqmap_find(g_anmmap->gvar_types, $1);
        if (ent) {
            val->type = ent->value[0] == '$' ? 'S' : 'f';
            param = thanm_param_new(val->type);
            param->val = val;
            ent = seqmap_find(g_anmmap->gvar_names, $1);
            if (ent == NULL) {
                /* Why would you define a variable type, but not variable name? */
                yyerror(state, "unknown variable: %s", $1);
                if (val->type == 'S')
                    val->val.S = 0;
                else
                    val->val.f = 0.0f;
            } else {
                if (val->type == 'S')
                    val->val.S = ent->key;
                else
                    val->val.f = 0.0f;
            }
            free($1);
        } else {
            val->type = 'z';
            val->val.z = $1;
            param = thanm_param_new('o');
            param->val = val;
        }
        $$ = param;
    }
%%

static prop_list_entry_t* 
prop_list_find(
    list_t* list, 
    const char* key
) {
    prop_list_entry_t* entry;
    list_for_each(list, entry) {
        if (strcmp(entry->key, key) == 0)
            return entry;
    }
    return NULL;
}

static void
prop_list_free_nodes(
    list_t* list
) {
    prop_list_entry_t* entry;
    list_for_each(list, entry) {
        if (entry->value->type == 't') {
            free(entry->value->val.t);
        } else if (entry->value->type == 'l') {
            prop_list_free_nodes(entry->value->val.l);
            free(entry->value->val.l);
        }
        free(entry->value);
        free(entry->key);
        free(entry);
        list_free_nodes(entry->value->val.l);
    }
}

static void
instr_check_types(
    parser_state_t* state,
    thanm_instr_t* instr
) {
    const id_format_pair_t* formats = anm_get_formats(state->current_entry->header->version);
    const char* format = find_format(formats, instr->id);
    if (format == NULL) {
        state->was_error = 1;
        yyerror(state, "opcode %d is not known to exist in version %d", instr->id, state->current_entry->header->version);
        return;
    }
    thanm_instr_t* param;
    size_t i = 0;
    list_for_each(&instr->params, param) {
        char c = format[i];
        if (c == '\0') {
            state->was_error = 1;
            yyerror(state, "too many parameters for opcode %d", instr->id);
            break;
        }
        if (c == 'n' || c == 'N' || c == 'o') {
            if (param->type == 'z')
                param->type = c;
            else if (param->type == 'S') /* Allow numbers for things that get converted to numbers anyway. */
                c = param->type;
        }
        
        if (param->type != c) {
            state->was_error = 1;
            yyerror(state, "wrong parameter %d type for opcode %d, expected: %c", i, instr->id, c);
        }
        ++i;
    }
    if (format[i] != '\0') {
        state->was_error = 1;
        yyerror(state, "not enough parameters for opcode %d", instr->id);
    }
}

static uint32_t
instr_get_size(
    parser_state_t* state,
    thanm_instr_t* instr
) {
    uint32_t size = sizeof(anm_instr_t);
    /* In ANM, parameter size is always 4 bytes (only int32 or float), so we can just add 4 to size for every param... */
    list_node_t* node;
    list_for_each_node(&instr->params, node)
        size += 4;

    return size;
}

static thanm_instr_t*
instr_new(
    parser_state_t* state,
    uint16_t id,
    list_t* params
) {
    thanm_instr_t* instr = thanm_instr_new();
    instr->type = THANM_INSTR_INSTR;
    instr->time = state->time;
    instr->offset = state->offset;
    instr->id = id;
    instr->params = *params;
    free(params);
    instr->size = instr_get_size(state, instr);
    instr_check_types(state, instr);
    state->offset += instr->size;
    return instr;
}

static int
identifier_instr(
    char* ident
) {
    if (strncmp(ident, "ins_", 4) == 0) {
        size_t i = strlen(ident) - 1;
        int valid = 1;
        int num = 0;
        int n = 1;
        while(i >= 4 && valid) {
            char c = ident[i];
            if (c < '0' || c > '9') {
                valid = 0;
            } else {
                num += (c - '0') * n;
            }
            --i;
            n *= 10;
        }
        if (valid)
            return num;
    }
    seqmap_entry_t* ent = seqmap_find(g_anmmap->ins_names, ident);
    if (ent)
        return ent->key;
    return -1;
}

void
yyerror(
    const parser_state_t* state,
    const char* format,
    ...)
{
    if (yylloc.first_line == yylloc.last_line) {
        if (yylloc.first_column == yylloc.last_column) {
            fprintf(stderr,
                    "%s:%s:%d,%d: ",
                    argv0, current_input,
                    yylloc.first_line, yylloc.first_column);
        } else {
            fprintf(stderr,
                    "%s:%s:%d,%d-%d: ",
                    argv0, current_input, yylloc.first_line,
                    yylloc.first_column, yylloc.last_column);
        }
    } else {
        fprintf(stderr,
                "%s:%s:%d,%d-%d,%d: ",
                argv0, current_input, yylloc.first_line,
                yylloc.first_column, yylloc.last_line, yylloc.last_column);
    }

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    fputc('\n', stderr);
}
