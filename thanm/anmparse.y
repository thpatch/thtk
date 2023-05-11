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
#include "reg.h"
#include "expr.h"

/* Bison things. */
void yyerror(const parser_state_t*, const char*, ...);
int yylex(void);
#define yyin thanm_yyin
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

/* Returns instruction number given an identifier, or -1 if it's not an instruction. */
static int identifier_instr(char* ident);

/* Searches for a global definition with a given name. */
static global_t* global_find(parser_state_t* state, const char* name);

/* Creates a copy of the given param. */
static thanm_param_t* param_copy(thanm_param_t* param);

/* Creates a new variable and (optionally) initializes it with the given expr if it's non-NULL. */
static var_t* var_create(parser_state_t* state, char* name, int type, expr_t* expr);

/* Adds an instruction that assigns the given expr to the variable.  */
static void var_assign(parser_state_t* state, var_t* var, expr_t* expr);

/* Returns variable of a given name (or NULL if not found). */
static var_t* var_find(parser_state_t* state, char* name);

static void instr_check_types(parser_state_t* state, int id, list_t* params);

%}

%define api.prefix {thanm_yy}
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
    struct expr_t* expr;
}

%token <floating> FLOATING "floating"
%token <integer> INTEGER "integer"
%token <string> IDENTIFIER "identifier"
%token <string> TEXT "text"
%token <string> DIRECTIVE "directive"

%token COMMA ","
%token COLON ":"
%token SEMICOLON ";"
%token SQUARE_OPEN "["
%token SQUARE_CLOSE "]"
%token BRACE_OPEN "{"
%token BRACE_CLOSE "}"
%token PARENTHESIS_OPEN "("
%token PARENTHESIS_CLOSE ")"
%token ASSIGN "="
%token ASSIGNADD "+="
%token ASSIGNSUB "-="
%token ASSIGNMUL "*="
%token ASSIGNDIV "/="
%token ASSIGNMOD "%="
%token ADD "+"
%token SUBTRACT "-"
%token MULTIPLY "*"
%token DIVIDE "/"
%token MODULO "%"
%token RAND "rand"
%token SIN "sin"
%token COS "cos"
%token TAN "tan"
/* For some reason, there is no asin instruction in the game. */
%token ACOS "acos"
%token ATAN "atan"
%token DOLLAR "$"
%token ENTRY "entry"
%token SCRIPT "script"
%token GLOBAL "global"
%token INT "int"
%token FLOAT "float"
%token TIMEOF "timeof"
%token OFFSETOF "offsetof"
%token SCRIPTOF "scriptof"
%token SPRITEOF "spriteof"

%token ILLEGAL_TOKEN "invalid token"
%token END_OF_FILE 0 "end of file"

%type <integer> SomethingOf
%type <integer> TypeDeclaration
%type <integer> VariableDeclaration
%type <string> TextLike
%type <param> ParameterSimple
%type <list> Properties
%type <list> PropertyList
%type <list> Expressions
%type <list> ExpressionList
%type <prop_list_entry> PropertyListEntry
%type <prop_list_entry_value> PropertyListValue
%type <expr> Expression
%type <expr> ExpressionParam
%type <expr> ExpressionSubset

%precedence ASSIGNADD ASSIGNSUB ASSIGNMUL ASSIGNDIV ASSIGNMOD ASSIGN
%left ADD SUBTRACT
%left MULTIPLY DIVIDE MODULO

%expect 0

%%

Statements:
    %empty
    | Statements Statement

Statement:
    Entry
    | Script
    | Directive
    | "global" IDENTIFIER[name] "=" Expression[expr] ";" {
        global_t* global = (global_t*)malloc(sizeof(global_t));
        expr_error_t err = expr_output(state, $expr, NULL);
        if (err)
            yyerror(state, "expression error in globaldef initialization: %s", expr_strerror(err));

        if ($expr->type == EXPR_VAL) {
            global->name = $name;
            global->param = $expr->param;
            list_prepend_new(&state->globals, global);
            $expr->param = NULL;
        }

        expr_free($expr);
    }

Entry:
    "entry" IDENTIFIER[entry_name] "{" Properties[prop_list] "}" {
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
        #define REQUIRE(x, y, l) { \
            prop = prop_list_find(l, x); \
            if (prop == NULL)  { \
                yyerror(state, "missing entry property: '" x "'"); \
                return 1; \
            } else if (prop->value->type != y) { \
                yyerror(state, "wrong value type for entry property: '" x "'"); \
                return 1; \
            } \
        }

        #define OPTIONAL(x, y, l) { \
            prop = prop_list_find(l, x); \
            if (prop && prop->value->type != y) { \
                yyerror(state, "wrong value type for entry property: '" x "'"); \
                return 1; \
            } \
        }

        if (state->default_version == -1)
            REQUIRE("version", 'S', $prop_list)
        else
            OPTIONAL("version", 'S', $prop_list)

        entry->header->version = prop ? prop->value->val.S : state->default_version;
        state->current_version = entry->header->version;

        REQUIRE("name", 't', $prop_list);
        entry->name = strdup(prop->value->val.t);

        OPTIONAL("name2", 't', $prop_list);
        if (prop) entry->name2 = strdup(prop->value->val.t);

        OPTIONAL("format", 'S', $prop_list);
        entry->header->format = prop ? prop->value->val.S : 1;

        OPTIONAL("width", 'S', $prop_list);
        entry->header->w = prop ? prop->value->val.S : DEFAULTVAL;

        OPTIONAL("height", 'S', $prop_list);
        entry->header->h = prop ? prop->value->val.S : DEFAULTVAL;

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
        entry->header->hasdata = prop ? prop->value->val.S : 1;

        if (entry->header->hasdata) {
            OPTIONAL("THTXSize", 'S', $prop_list);
            entry->thtx->size = prop ? prop->value->val.S : DEFAULTVAL;

            OPTIONAL("THTXFormat", 'S', $prop_list);
            entry->thtx->format = prop ? prop->value->val.S : DEFAULTVAL;

            OPTIONAL("THTXWidth", 'S', $prop_list);
            entry->thtx->w = prop ? prop->value->val.S : DEFAULTVAL;

            OPTIONAL("THTXHeight", 'S', $prop_list);
            entry->thtx->h = prop ? prop->value->val.S : DEFAULTVAL;

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

        prop_list_free_nodes($prop_list);
        free($prop_list);
        free($entry_name);
        list_append_new(&state->entries, entry);
        state->current_entry = entry;
    }

Properties:
    %empty {
        $$ = list_new();
    }
    | PropertyList[list] {
        $$ = $list;
    }

PropertyList:
    PropertyListEntry[prop] {
        $$ = list_new();
        list_append_new($$, $prop);
    }
    | PropertyList[list] "," PropertyListEntry[prop] {
        list_append_new($list, $prop);
    }

PropertyListEntry:
    IDENTIFIER[key] ":" PropertyListValue[val] {
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
    | "{" Properties "}" {
        $$ = (prop_list_entry_value_t*)malloc(sizeof(prop_list_entry_value_t));
        $$->type = 'l';
        $$->val.l = $2;
    }
    | IDENTIFIER {
        $$ = (prop_list_entry_value_t*)malloc(sizeof(prop_list_entry_value_t));
        global_t* global = global_find(state, $1);
        if (global == NULL) {
            yyerror(state, "global definition not found: %s", $1);
            $$->type = 'S';
            $$->val.S = 0;
        } else {
            if (global->param->is_var)
                yyerror(state, "variables are not acceptable in parameter lists"
                    "(through global definition: %s)", $1);
            switch(global->param->type) {
                case 'S':
                    $$->type = 'S';
                    $$->val.S = global->param->val->val.S;
                    break;
                case 'f':
                    $$->type = 'f';
                    $$->val.f = global->param->val->val.f;
                    break;
                case 'z':
                    $$->type = 't';
                    $$->val.t = strdup(global->param->val->val.z);
                    break;
                default:
                    $$->type = 'S';
                    $$->val.S = 0;
                    yyerror(state, "parameter type '%c' is not acceptable in parameter lists"
                        "(through global definition: %s)", global->param->type, $1);
                    break;
            }
        }
    }

Script:
    "script" ScriptOptionalId IDENTIFIER[name] {
        if (state->current_entry == NULL) {
            yyerror(state, "an entry is required before a script");
            return 1;
        }
        anm_script_t* script = anm_script_new();
        reg_reset(state->current_version);
        script->offset = malloc(sizeof(*script->offset));
        script->offset->id = state->script_id++;
        script->real_index = state->script_real_index++;

        symbol_id_pair_t* symbol = (symbol_id_pair_t*)malloc(sizeof(symbol_id_pair_t));
        symbol->id = script->real_index;
        symbol->name = $name;
        list_append_new(&state->script_names, symbol);
        if (state->symbolfp != NULL)
                fprintf(state->symbolfp, "global %s%s = %d;\n", state->symbol_prefix, symbol->name, symbol->id);

        list_append_new(&state->current_entry->scripts, script);
        state->current_script = script;
        state->offset = 0;
        state->time = 0;
    } "{" ScriptStatements "}" {
        state->current_script = NULL;
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
        if (label_find(state->current_script, $name) != NULL) {
            yyerror(state, "duplicate label: %s", $name);
        }
        label_t* label = (label_t*)malloc(sizeof(label_t));
        label->name = $name;
        label->offset = state->offset;
        label->time = state->time;
        list_append_new(&state->current_script->labels, label);
    }
    | IDENTIFIER[ident] "(" Expressions[exprs] ")" ";" {
        int id = identifier_instr($ident);
        if (id == -1) {
            yyerror(state, "unknown mnemonic: %s", $ident);
            return 1;
        }

        list_t regs_to_free;
        list_init(&regs_to_free);

        list_t* param_list = list_new();

        expr_t* expr;
        list_for_each($exprs, expr) {
            expr_error_t err = expr_output(state, expr, NULL);
            if (err)
                yyerror(state, "expression error in instr parameter: %s", expr_strerror(err));

            if (expr->reg && expr->reg->lock == LOCK_EXPR)
                list_append_new(&regs_to_free, expr->reg);

            list_append_new(param_list, expr->param);

            expr->param = NULL;
            expr_free(expr);
        }
        list_free_nodes($exprs);
        free($exprs);

        instr_check_types(state, id, param_list);
        thanm_instr_t* instr = instr_new(state, id, param_list);
        list_append_new(&state->current_script->instrs, instr);

        reg_t* reg;
        list_for_each(&regs_to_free, reg)
            reg_lock(reg, LOCK_UNLOCK);

        free($ident);
    }
    | VariableDeclaration ";"
    | ExpressionSubset ";" {
        expr_error_t err = expr_output(state, $1, NULL);
        if (err)
            yyerror(state, "expression error: %s", expr_strerror(err));

        if ($1->reg && $1->reg->lock == LOCK_EXPR)
            reg_lock($1->reg, LOCK_UNLOCK);
        expr_free($1);
    }

TypeDeclaration:
    "int" {
        $$ = 'S';
    }
    | "float" {
        $$ = 'f';
    }

VariableDeclaration:
    TypeDeclaration[type] IDENTIFIER[name] "=" Expression[expr] {
        $$ = $type;
        var_create(state, $name, $type, $expr);
    }
    | TypeDeclaration[type] IDENTIFIER[name] {
        $$ = $type;
        var_create(state, $name, $type, NULL);
    }
    | VariableDeclaration[type] "," IDENTIFIER[name] "=" Expression[expr] {
        $$ = $type;
        var_create(state, $name, $type, $expr);
    }
    | VariableDeclaration[type] "," IDENTIFIER[name] {
        $$ = $type;
        var_create(state, $name, $type, NULL);
    }

Expressions:
    %empty {
        $$ = list_new();
    }
    | ExpressionList {
        $$ = $1;
    }

ExpressionList:
    Expression[expr] {
        $$ = list_new();
        list_append_new($$, $expr);
    }
    | ExpressionList[list] "," Expression[expr] {
        list_append_new($list, $expr);
    }

ParameterSimple:
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
    | "[" INTEGER "]" {
        if (reg_lock_id($2, LOCK_OTHER, state->current_version)) {
            yyerror(state, "warning: global var [%d] is already used by the compiler", $2);
        }
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'S';
        val->val.S = $2;
        thanm_param_t* param = thanm_param_new('S');
        param->is_var = 1;
        param->val = val;
        $$ = param;
    }
    | "[" FLOATING "]" {
        if (reg_lock_id((int)$2, LOCK_OTHER, state->current_version)) {
            yyerror(state, "warning: global var [%f] is already used by the compiler", $2);
        }
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
            if (reg_lock_id(ent->key, LOCK_OTHER, state->current_version)) {
                yyerror(state, "warning: global var %s is already used by the compiler", $2);
            }
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
            if (reg_lock_id(ent->key, LOCK_OTHER, state->current_version)) {
                yyerror(state, "warning: global var %s is already used by the compiler", $2);
            }
            param->val->val.f = (float)ent->key;
        }
        free($2);
        $$ = param;
    }
    | IDENTIFIER {
        /* First, check for globaldefs, localvars and globalvars.
         * If it's neither, then simply make it a string param that
         * will be evaluated based on context (what format the instr expects) */
        global_t* global = global_find(state, $1);
        if (global) {
            $$ = param_copy(global->param);
        } else {
            var_t* var = var_find(state, $1);
            if (var) {
                $$ = reg_to_param(var->reg);
            } else {
                value_t* val = (value_t*)malloc(sizeof(value_t));
                thanm_param_t* param;
                seqmap_entry_t* ent = seqmap_find(g_anmmap->gvar_names, $1);
                if (ent) {
                    int id = ent->key;
                    if (reg_lock_id(id, LOCK_OTHER, state->current_version)) {
                        yyerror(state, "warning: global var %s is already used by the compiler", $1);
                    }
                    ent = seqmap_get(g_anmmap->gvar_types, id);
                    if (ent == NULL) {
                        /* Unknown type */
                        yyerror(state, "type of variable is unknown: %s", $1);
                        val->type = 'S';
                        val->val.S = 0;
                        param = thanm_param_new('S');
                        param->val = val;
                    } else {
                        val->type = ent->value[0] == '$' ? 'S' : 'f';
                        if (val->type == 'S')
                            val->val.S = id;
                        else
                            val->val.f = (float)id;

                        param = thanm_param_new(val->type);
                        param->is_var = 1;
                        param->val = val;
                    }
                    free($1);
                } else {
                    val->type = 'z';
                    val->val.z = $1;
                    param = thanm_param_new('z');
                    param->val = val;
                }
                $$ = param;
            }
        }
    }
    | SomethingOf[type] "(" IDENTIFIER[label] ")" {
        value_t* val = (value_t*)malloc(sizeof(value_t));
        val->type = 'z';
        val->val.z = $label;
        thanm_param_t* param = thanm_param_new($type);
        param->val = val;
        $$ = param;
    }

Expression:
      ExpressionParam
    | ExpressionSubset[expr] {
        /* Expr could be NULL if no operations were found. */
        if ($expr == NULL) {
            yyerror(state, "unable to create expression: no operators found for current version");
            return 1;
        }
        $$ = $expr;
    }

ExpressionParam:
    ParameterSimple[param] {
        $$ = expr_new_val($param, state->current_version);
    }
    | "(" ExpressionParam ")" {
        $$ = $2;
    }

ExpressionSubset:
      "(" ExpressionSubset ")"         { $$ = $2; }
    | ExpressionParam "=" Expression   { $$ = expr_new_assign(ASSIGN, $1, $3, state->current_version); }
    | ExpressionParam "+=" Expression  { $$ = expr_new_assign(ADD, $1, $3, state->current_version); }
    | ExpressionParam "-=" Expression  { $$ = expr_new_assign(SUBTRACT, $1, $3, state->current_version); }
    | ExpressionParam "*=" Expression  { $$ = expr_new_assign(MULTIPLY, $1, $3, state->current_version); }
    | ExpressionParam "/=" Expression  { $$ = expr_new_assign(DIVIDE, $1, $3, state->current_version); }
    | ExpressionParam "%=" Expression  { $$ = expr_new_assign(MODULO, $1, $3, state->current_version); }
    | Expression "+" Expression        { $$ = expr_new_binary(ADD, $1, $3, state->current_version); }
    | Expression "-" Expression        { $$ = expr_new_binary(SUBTRACT, $1, $3, state->current_version); }
    | Expression "*" Expression        { $$ = expr_new_binary(MULTIPLY, $1, $3, state->current_version); }
    | Expression "/" Expression        { $$ = expr_new_binary(DIVIDE, $1, $3, state->current_version); }
    | Expression "%" Expression        { $$ = expr_new_binary(MODULO, $1, $3, state->current_version); }
    | "rand" "(" Expression ")"        { $$ = expr_new_unary(RAND, $3, state->current_version); }
    | "sin" "(" Expression ")"         { $$ = expr_new_unary(SIN, $3, state->current_version); }
    | "cos" "(" Expression ")"         { $$ = expr_new_unary(COS, $3, state->current_version); }
    | "tan" "(" Expression ")"         { $$ = expr_new_unary(TAN, $3, state->current_version); }
    | "acos" "(" Expression ")"        { $$ = expr_new_unary(ACOS, $3, state->current_version); }
    | "atan" "(" Expression ")"        { $$ = expr_new_unary(ATAN, $3, state->current_version); }

SomethingOf:
    "timeof" {
        $$ = 't';
    }
    | "offsetof" {
        $$ = 'o';
    }
    | "scriptof" {
        $$ = 'N';
    }
    | "spriteof" {
        $$ = 'n';
    }

TextLike:
    TEXT {
        $$ = $1;
    }
    | INTEGER {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", $1);
        $$ = strdup(buf);
    }
    | FLOATING {
        char buf[32];
        snprintf(buf, sizeof(buf), "%f", $1);
        $$ = strdup(buf);
    }

Directive:
    DIRECTIVE[type] TextLike[arg] {
        int was_fatal_error = 0;

        if (strcmp($type, "version") == 0) {
            uint32_t ver = strtoul($arg, NULL, 10);
            state->default_version = ver;
            if (state->current_version == -1)
                state->current_version = ver;
        } else if (strcmp($type, "anmmap") == 0) {
            char* path = path_get_full(&state->path_state, $arg);
            FILE* map_file = fopen(path, "r");
            if (map_file == NULL) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, path, strerror(errno));
            } else {
                anmmap_load(g_anmmap, map_file, $arg);
                fclose(map_file);
            }
            free(path);
        } else if (strcmp($type, "include") == 0) {
            char* path = path_get_full(&state->path_state, $arg);
            FILE* include_file = fopen(path, "r");
            if (include_file == NULL) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, path, strerror(errno));
            } else {
                FILE* in_org = yyin;
                YYLTYPE loc_org = yylloc;
                const char* input_org = current_input;

                current_input = path;
                yyin = include_file;
                yylloc.first_line = 1;
                yylloc.first_column = 1;
                yylloc.last_line = 1;
                yylloc.last_column = 1;

                path_add(&state->path_state, path);

                was_fatal_error = yyparse(state);

                fclose(include_file);
                path_remove(&state->path_state);

                yyin = in_org;
                yylloc = loc_org;
                current_input = input_org;
            }
            free(path);
        } else if (strcmp($type, "symbol_prefix") == 0) {
            if (strlen($arg) + 1 > sizeof(state->symbol_prefix)) {
                yyerror(state, "#symbol_prefix: specified prefix is too long");
            } else {
                strcpy(state->symbol_prefix, $arg);
            }
        } else {
            yyerror(state, "unknown directive: %s", $type);
        }
        free($type);
        free($arg);

        if (was_fatal_error)
            return 1;
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
    }
    list_free_nodes(list);
}

static void
instr_check_types(
    parser_state_t* state,
    int id,
    list_t* param_list
) {
    char opcode_msg[64];
    seqmap_entry_t* ent = seqmap_get(g_anmmap->ins_names, id);
    if (ent) {
        snprintf(opcode_msg, sizeof(opcode_msg), "%d (%s)", id, ent->value);
    } else {
        snprintf(opcode_msg, sizeof(opcode_msg), "%d", id);
    }

    const char* format = anm_find_format(state->version, state->current_entry->header->version, id);
    if (format == NULL) {
        state->was_error = 1;
        yyerror(state, "opcode %s is not known to exist in version %d", opcode_msg, state->current_entry->header->version);
        return;
    }
    thanm_param_t* param;
    size_t i = 0;
    list_for_each(param_list, param) {
        char c = format[i];
        if (c == '\0') {
            state->was_error = 1;
            yyerror(state, "too many parameters for opcode %s", opcode_msg);
            break;
        }
        if (c == 'S') {
            /* Allow types that get converted to integers later for integer formats. */
            if (param->type == 't' || param->type == 'o' || param->type == 'N' || param->type == 'n')
                c = param->type;
        } else if (c == 'n' || c == 'N' || c == 'o' || c == 't') {
            /* This is to tell the anm_serialize_instr function what it should do
             * with the string value of the param, based on the instruction format. */
            if (param->type == 'z')
                param->type = c;
            else if (param->type == 'S') /* Allow numbers for things that get converted to numbers anyway. */
                c = param->type;
        } else if (c == 's' && param->type == 'S') {
            /* Convert to int16 */
            param->type = 's';
            param->val->type = 's';
            param->val->val.s = (int16_t)param->val->val.S;
        }

        if (param->type != c) {
            state->was_error = 1;
            yyerror(state, "wrong parameter %d type for opcode %s, expected: %c", i + 1, opcode_msg, c);
        }
        ++i;
    }
    if (format[i] != '\0') {
        // state->was_error = 1; - it compiles fine, needed for VD photo.anm "support" (ins_439 format in th165 and th18 differs)
        yyerror(state, "not enough parameters for opcode %s", opcode_msg);
    }
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

static global_t*
global_find(
    parser_state_t* state,
    const char* name
) {
    global_t* global;
    list_for_each(&state->globals, global) {
        if (strcmp(name, global->name) == 0)
            return global;
    }
    return NULL;
}

static thanm_param_t*
param_copy(
    thanm_param_t* param
) {
    thanm_param_t* copy = (thanm_param_t*)malloc(sizeof(thanm_instr_t));
    copy->type = param->type;
    copy->is_var = param->is_var;
    value_t* val_copy = (value_t*)malloc(sizeof(value_t));
    memcpy(val_copy, param->val, sizeof(value_t));
    if (val_copy->type == 'z')
        val_copy->val.z = strdup(val_copy->val.z);
    copy->val = val_copy;
    return copy;
}

static var_t*
var_create(
    parser_state_t* state,
    char* name,
    int type,
    expr_t* expr
) {
    /* Create variables without assigning registers, and only assign them if
     * the var is actually used. */
    var_t* var = var_new(name, type);
    if (expr != NULL) {
        var_assign(state, var, expr);
    }
    list_append_new(&state->current_script->vars, var);
    return var;
}

static void
var_assign(
    parser_state_t* state,
    var_t* var,
    expr_t* expr
) {
    reg_t* reg = reg_acquire(PURPOSE_VAR, var->type, state->current_version);
    if (reg == NULL) {
        yyerror(state, "unable to use variable %s: registers full", var->name);
        return;
    }
    var->reg = reg;

    expr_t* new_expr = expr_new_assign(ASSIGN, expr_new_val_reg(reg), expr, state->current_version);

    expr_error_t err = expr_output(state, new_expr, NULL);
    if (err)
        yyerror(state, "expression error in variable initialization: %s", expr_strerror(err));

    if (new_expr->reg && new_expr->reg->lock == LOCK_EXPR)
        reg_lock(reg, LOCK_UNLOCK);
    expr_free(new_expr);
}

static var_t*
var_find(
    parser_state_t* state,
    char* name
) {
    var_t* var;
    if (state->current_script) {
        list_for_each(&state->current_script->vars, var) {
            if (strcmp(var->name, name) == 0) {
                /* If something was looking for the variable,
                 * it probably expects it to have a register. */
                if (var->reg == NULL) {
                    reg_t* reg = reg_acquire(PURPOSE_VAR, var->type, state->current_version);
                    if (reg == NULL) {
                        yyerror(state, "unable to use variable %s: registers full", name);
                        return NULL;
                    }
                    var->reg = reg;
                }
                return var;
            }
        }
    }
    return NULL;
}

void
yyerror(
    const parser_state_t* state,
    const char* format,
    ...)
{
    (void)state;
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
