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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ecl.h"
#include "program.h"
#include "util.h"
#include "instr.h"

/* Bison things. */
void yyerror(const char*);
int yylex(void);
extern FILE* yyin;

/* Temporary parser structures. */
typedef struct list_t {
    struct list_t* next;
    void* data;
} list_t;

typedef struct expression_t {
    int instr;
    int type;
    unsigned int child_count;
    struct expression_t** children;
    list_t* params;
} expression_t;

/* Parser APIs. */
static void sub_begin(char* name);
static void sub_finish(void);

static expression_t* make_stackinstr(int type, expression_t* expr1, expression_t* expr2, list_t* param);
static void output_expression(expression_t* expression);

static list_t* make_list(void* data);
static param_t* make_param(unsigned char type);
static void instr_add(int id, list_t* list);
static void label_create(char* label);
static int32_t label_find(sub_t* sub, const char* label);

static void free_param(param_t* param);
static void free_params(list_t* node);
static void free_list(list_t* node);
static void free_expression(expression_t* expr);

/* Parser state. */
static int instr_time;
static int instr_rank = 0xff;
static int version;

static uint32_t anim_cnt;
static char** anim_list;
static uint32_t ecli_cnt;
static char** ecli_list;

static unsigned int sub_cnt;
static sub_t* subs;
static sub_t* current_sub;

%}

%error-verbose
%locations

%union {
    int integer;
    float floating;
    char* string;
    struct {
        unsigned int length;
        unsigned char* data;
    } bytes;
    struct list_t* list;
    struct param_t* param;
    struct expression_t* expression;
}

%token <integer> INSTRUCTION "instruction"
%token <string> IDENTIFIER "identifier"
%token <string> TEXT "text"
%token <bytes> CTEXT "encrypted text"
%token <integer> INTEGER "integer"
%token <floating> FLOATING "float"
%token <integer> RANK "rank"
%token COLON ":"
%token SEMICOLON ";"
%token SQUARE_OPEN "["
%token SQUARE_CLOSE "]"
%token CAST_FLOAT "(float)"
%token CAST_INT "(int)"
%token ANIM "anim"
%token ECLI "ecli"
%token SUB "sub"
%token AT "@"
%token BRACE_OPEN "{"
%token BRACE_CLOSE "}"
%token PARENTHESIS_OPEN "("
%token PARENTHESIS_CLOSE ")"
%token ILLEGAL_TOKEN "illegal token"
%token END_OF_FILE 0 "end of file"

%token GOTO "goto"
%token UNLESS "unless"
%token IF "if"
%token LOAD
%token ASSIGN "="
%token ADD "+"
%token SUBTRACT "-"
%token MULTIPLY "*"
%token DIVIDE "/"
%token MODULO "%"
%token EQUAL "=="
%token INEQUAL "!="
%token LT "<"
%token LTEQ "<="
%token GT ">"
%token GTEQ ">="
%token NOT "!"
%token AND "&"
%token OR "|"
%token XOR "^"

%type <list> Include_List

%type <param> Address
%type <param> Address_Type

%type <list> Instruction_Parameters
%type <param> Instruction_Parameter

%type <param> Integer
%type <param> Floating
%type <param> Text
%type <param> Label
%type <param> Encrypted_Text
%type <param> Cast_Value

%type <param> Cast_Type
%type <integer> Cast_Target

%type <expression> Expression
%type <param> Load_Type


%%

Statements:
    | Statement Statements
    ;

Statement:
      "sub" IDENTIFIER {
        sub_begin($2);
        free($2);
    }
      "{" Instructions "}" { sub_finish(); }
    | "anim" "{" Include_List "}" {
        list_t* node = $3;

        while (node) {
            param_t* param = (param_t*)node->data;
            anim_cnt++;
            anim_list = realloc(anim_list, sizeof(char*) * anim_cnt);
            anim_list[anim_cnt - 1] = param->value.s.data;
            node = node->next;
        }

        free_params($3);
        free_list($3);
    }
    | "ecli" "{" Include_List "}" {
        list_t* node = $3;

        while (node) {
            param_t* param = (param_t*)node->data;
            ecli_cnt++;
            ecli_list = realloc(ecli_list, sizeof(char*) * ecli_cnt);
            ecli_list[ecli_cnt - 1] = param->value.s.data;
            node = node->next;
        }

        free_params($3);
        free_list($3);
    }
    ;

Include_List:
      { $$ = NULL; }
    | Text ";" Include_List {
        $$ = make_list($1);
        $$->next = $3;
    }
    ;

Instructions:
    | Instruction ";" Instructions
    | RANK { instr_rank = $1; } Instruction { instr_rank = 0xff; } ";" Instructions
    | IDENTIFIER ":" { label_create($1); } Instructions
    | INTEGER ":" {
        if ($1 == instr_time || (instr_time > 0 && $1 < instr_time)) {
            char buf[256];
            snprintf(buf, 256, "illegal timer change: %d to %d", instr_time, $1);
            yyerror(buf);
        }
        instr_time = $1;
    } Instructions
    ;

    /* TODO: Check the given parameters against the parameters expected for the
     *       instruction.  This requires passing a version parameter to eclc. */
Instruction:
      INSTRUCTION Instruction_Parameters {
        instr_add($1, $2);
        free_params($2);
        free_list($2);
    }
    | "if" Expression "goto" Label "@" Integer {
        list_t* label = make_list($4);
        list_t* time = make_list($6);
        label->next = time;
        output_expression($2);
        free_expression($2);
        instr_add(14, label);
        free_params(label);
        free_list(label);
    }
    | "unless" Expression "goto" Label "@" Integer {
        list_t* label = make_list($4);
        list_t* time = make_list($6);
        label->next = time;
        output_expression($2);
        free_expression($2);
        instr_add(13, label);
        free_params(label);
        free_list(label);
    }
    | "goto" Label "@" Integer {
        list_t* label = make_list($2);
        list_t* time = make_list($4);
        label->next = time;
        instr_add(12, label);
        free_params(label);
        free_list(label);
    }
    | Address "=" Expression {
        expression_t* expr = make_stackinstr(ASSIGN, $3, NULL, make_list($1));
        output_expression(expr);
        free_expression(expr);
    }
    | Expression {
        output_expression($1);
        free_expression($1);
    }
    ;

Instruction_Parameters:
      { $$ = NULL; }
    | Instruction_Parameter Instruction_Parameters {
        $$ = make_list($1);
        $$->next = $2;
    }
    ;

Instruction_Parameter:
      Address
    | Integer
    | Floating
    | Text
    | Label
    | Encrypted_Text
    | Cast_Value
    ;

Expression:
      Load_Type {
        $$ = make_stackinstr(LOAD, NULL, NULL, make_list($1));
    }
    | "(" "!" Expression ")"             { $$ = make_stackinstr(NOT,      $3, NULL, NULL); }
    | "(" Expression "&"  Expression ")" { $$ = make_stackinstr(AND,      $2, $4, NULL); }
    | "(" Expression "|"  Expression ")" { $$ = make_stackinstr(OR,       $2, $4, NULL); }
    | "(" Expression "^"  Expression ")" { $$ = make_stackinstr(XOR,      $2, $4, NULL); }
    | "(" Expression "+"  Expression ")" { $$ = make_stackinstr(ADD,      $2, $4, NULL); }
    | "(" Expression "-"  Expression ")" { $$ = make_stackinstr(SUBTRACT, $2, $4, NULL); }
    | "(" Expression "*"  Expression ")" { $$ = make_stackinstr(MULTIPLY, $2, $4, NULL); }
    | "(" Expression "/"  Expression ")" { $$ = make_stackinstr(DIVIDE,   $2, $4, NULL); }
    | "(" Expression "%"  Expression ")" { $$ = make_stackinstr(MODULO,   $2, $4, NULL); }
    | "(" Expression "==" Expression ")" { $$ = make_stackinstr(EQUAL,    $2, $4, NULL); }
    | "(" Expression "!=" Expression ")" { $$ = make_stackinstr(INEQUAL,  $2, $4, NULL); }
    | "(" Expression "<"  Expression ")" { $$ = make_stackinstr(LT,       $2, $4, NULL); }
    | "(" Expression "<=" Expression ")" { $$ = make_stackinstr(LTEQ,     $2, $4, NULL); }
    | "(" Expression ">"  Expression ")" { $$ = make_stackinstr(GT,       $2, $4, NULL); }
    | "(" Expression ">=" Expression ")" { $$ = make_stackinstr(GTEQ,     $2, $4, NULL); }
    ;

Address:
      "[" Address_Type "]" {
        $$ = $2;
        $$->stack = 1;
    }
    ;

Address_Type:
      Integer
    | Floating
    ;

Integer:
    INTEGER {
        $$ = make_param('i');
        $$->value.i = $1;
    }
    ;

Floating:
    FLOATING {
        $$ = make_param('f');
        $$->value.f = $1;
    }
    ;

Text:
    TEXT {
        $$ = make_param('s');
        $$->value.s.length = strlen($1);
        $$->value.s.data = $1;
    }
    ;

Label:
    IDENTIFIER {
        $$ = make_param('o');
        $$->value.s.length = strlen($1);
        $$->value.s.data = $1;
    }
    ;

Encrypted_Text:
    CTEXT {
        $$ = make_param('c');
        $$->value.s.length = $1.length;
        $$->value.s.data = (char*)$1.data;
    }
    ;

Cast_Value:
    Cast_Target Cast_Type {
        $$ = make_param('D');
        $$->value.D[0] = (      $1 == 'f' ? 0x6600 : 0x6900)
                       | ($2->type == 'f' ?   0x66 :   0x69);
        if ($2->type == 'f') {
            memcpy(&$$->value.D[1], &$2->value.f, sizeof(float));
        } else {
            $$->value.D[1] = $2->value.i;
        }
        $$->stack = $2->stack;
        free_param($2);
    }
    ;

Cast_Target:
      "(int)"   { $$ = 'i'; }
    | "(float)" { $$ = 'f'; }
    ;

Cast_Type:
      Address
    | Integer
    | Floating
    ;

Load_Type:
      Address
    | Integer
    | Floating
    ;

%%

static void free_param(
    param_t* param)
{
    if (param) {
        /* ... */
        free(param);
    }
}

static void free_params(
    list_t* node)
{
    while (node) {
        free_param(node->data);
        node->data = NULL;
        node = node->next;
    }
}

/* Does not free data. */
static void free_list(
    list_t* node)
{
    if (node) {
        list_t* temp = node->next;
        free(node);
        if (temp)
            free_list(temp);
    }
}

static void free_expression(
    expression_t* expr)
{
    unsigned int i;

    free_params(expr->params);
    free_list(expr->params);

    for (i = 0; i < expr->child_count; ++i) {
        free_expression(expr->children[i]);
    }
    free(expr->children);
    free(expr);
}

static expression_t*
make_stackinstr(
    int type,
    expression_t* expr1,
    expression_t* expr2,
    list_t* params)
{
    expression_t* out;
    unsigned int j = 0;
    const stackinstr_t* i;

    for (i = get_stackinstrs(version); i->type; ++i) {
        int ok = 1;

        if (i->type == type) {
            list_t* p;

            switch (strlen(i->stack)) {
            case 2:
                if (!expr2 || i->stack[1] != expr2->type)
                    ok = 0;
            case 1:
                if (!expr1 || i->stack[0] != expr1->type)
                    ok = 0;
            case 0:
                break;
            default:
                fprintf(stderr, "error wrong strlen\n");
                ok = 0;
                /* Error. */
                break;
            }

            j = 0;
            for (p = params; p; p = p->next) {
                param_t* param = p->data;
                if (j > strlen(i->params))
                    ok = 0;
                if (i->params[j] != param->type)
                    ok = 0;
                j++;
            }

            if (ok)
                break;
        }
    }

    if (!i->type) {
        list_t* p;
        char buf[256];
        sprintf(buf, "no match found for %d, '%c', '%c'", type, expr1 ? expr1->type : '-', expr2 ? expr2->type : '-');
        for (p = params; p; p = p->next) {
            param_t* param = p->data;
            fprintf(stderr, "  %c\n", param->type);
        }
        yyerror(buf);
        return NULL;
    }

    out = malloc(sizeof(expression_t));
    out->instr = i->instr;
    out->type = i->value;
    if (strlen(i->stack)) {
        out->child_count = strlen(i->stack);
        out->children = malloc(strlen(i->stack) * sizeof(expression_t*));
        if (strlen(i->stack) > 0)
            out->children[0] = expr1;
        if (strlen(i->stack) > 1)
            out->children[1] = expr2;
    } else {
        out->child_count = 0;
        out->children = NULL;
    }
    out->params = params;

    return out;
}

static list_t*
make_list(
    void* data)
{
    list_t* l = malloc(sizeof(list_t));
    l->next = NULL;
    l->data = data;
    return l;
}

static param_t*
make_param(
    unsigned char type)
{
    param_t* p = malloc(sizeof(param_t));
    p->type = type;
    p->stack = 0;
    return p;
}

static void
output_expression(
    expression_t* expr)
{
    unsigned int i;

    for (i = 0; i < expr->child_count; ++i) {
        output_expression(expr->children[i]);
    }

    instr_add(expr->instr, expr->params);
}

static void
sub_begin(
    char* name)
{
    sub_cnt++;
    subs = realloc(subs, sub_cnt * sizeof(sub_t));
    current_sub = &subs[sub_cnt - 1];

    instr_time = 0;
    current_sub->name = strdup(name);
    current_sub->instr_cnt = 0;
    current_sub->instrs = NULL;
    current_sub->offset = 0;
    current_sub->label_cnt = 0;
    current_sub->labels = NULL;
}

static void
sub_finish(void)
{
    current_sub = NULL;
}

static void
label_create(
    char* name)
{
    current_sub->label_cnt++;
    current_sub->labels =
        realloc(current_sub->labels, sizeof(label_t) * current_sub->label_cnt);
    current_sub->labels[current_sub->label_cnt - 1].name = name;
    current_sub->labels[current_sub->label_cnt - 1].offset =
        current_sub->offset;
}

static int32_t
label_find(
    sub_t* sub,
    const char* name)
{
    char buf[256];
    unsigned int i;
    for (i = 0; i < sub->label_cnt; ++i) {
        if (strcmp(sub->labels[i].name, name) == 0)
            return sub->labels[i].offset;
    }
    snprintf(buf, 256, "label not found: %s", name);
    yyerror(buf);
    return 0;
}

static void
instr_create(
    instr_t* instr,
    uint32_t time,
    uint16_t id,
    uint16_t param_mask,
    uint8_t rank_mask,
    uint8_t param_cnt,
    param_t* params)
{
    unsigned int i;

    instr->time = time;
    instr->id = id;
    instr->size = 16;
    instr->param_mask = param_mask;
    instr->rank_mask = rank_mask;
    instr->param_cnt = param_cnt;
    instr->params = params;
    instr->offset = 0;

    for (i = 0; i < param_cnt; ++i) {
        unsigned int padded_length;
        switch (params[i].type) {
        case 'i':
        case 'o':
        case 'f':
            instr->size += sizeof(int32_t);
            break;
        case 'D':
            instr->size += sizeof(uint32_t) * 2;
            break;
        case 's':
        case 'c':
            padded_length = instr->params[i].value.s.length +
                (4 - instr->params[i].value.s.length % 4);
            instr->size += sizeof(uint32_t);
            instr->size += padded_length;
            break;
        }
    }
}

static char*
instr_serialize(
    sub_t* sub,
    instr_t* op)
{
    unsigned int i;
    char* data = calloc(op->size, 1);
    unsigned int offset = 0;

    memcpy(data, op, 12);
    offset += 16;

    for (i = 0; i < op->param_cnt; ++i) {
        unsigned int padded_length;
        int32_t label;
        switch (op->params[i].type) {
        case 'i':
            memcpy(data + offset, &op->params[i].value.i, sizeof(int32_t));
            offset += sizeof(int32_t);
            break;
        case 'f':
            memcpy(data + offset, &op->params[i].value.f, sizeof(float));
            offset += sizeof(float);
            break;
        case 'D':
            memcpy(data + offset, &op->params[i].value.D, sizeof(int32_t) * 2);
            offset += sizeof(int32_t) * 2;
            break;
        case 'o':
            label = label_find(sub, op->params[i].value.s.data) - op->offset;
            memcpy(data + offset, &label, sizeof(int32_t));
            offset += sizeof(int32_t);

            free(op->params[i].value.s.data);

            break;
        case 's':
        case 'c':
            padded_length = op->params[i].value.s.length +
                (4 - op->params[i].value.s.length % 4);

            memcpy(data + offset, &padded_length, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            if (op->params[i].type == 'c') {
                memcpy(data + offset, op->params[i].value.s.data,
                    padded_length);
            } else {
                memcpy(data + offset, op->params[i].value.s.data,
                    op->params[i].value.s.length);
            }
            offset += padded_length;

            free(op->params[i].value.s.data);

            break;
        }
    }

    free(op->params);

    return data;
}

static void
instr_add(
    int id,
    list_t* list)
{
    int param_mask = 0;
    uint8_t param_cnt = 0;
    param_t* params = NULL;
    instr_t* op;

    current_sub->instr_cnt++;
    current_sub->instrs =
        realloc(current_sub->instrs, current_sub->instr_cnt * sizeof(instr_t));

    op = &current_sub->instrs[current_sub->instr_cnt - 1];

    while (list) {
        param_t* param;
        list_t* temp;
        param_cnt++;

        param = list->data;

        params = realloc(params, param_cnt * sizeof(param_t));
        params[param_cnt - 1] = *param;

        if (param->stack)
            param_mask |= 1 << (param_cnt - 1);

        temp = list;
        list = list->next;
    }

    instr_create(op, instr_time, id, param_mask, instr_rank, param_cnt, params);

    op->offset = current_sub->offset;
    current_sub->offset += op->size;
}

void
yyerror(
    const char* str)
{
    /* TODO: Research standard row and column range formats. */
    if (yylloc.first_line == yylloc.last_line) {
        if (yylloc.first_column == yylloc.last_column) {
            fprintf(stderr,
                    "%s:%s:%d,%d: %s\n",
                    argv0, current_input,
                    yylloc.first_line, yylloc.first_column, str);
        } else {
            fprintf(stderr,
                    "%s:%s:%d,%d-%d: %s\n",
                    argv0, current_input, yylloc.first_line,
                    yylloc.first_column, yylloc.last_column, str);
        }
    } else {
        fprintf(stderr,
                "%s:%s:%d,%d-%d,%d: %s\n",
                argv0, current_input, yylloc.first_line,
                yylloc.first_column, yylloc.last_line, yylloc.last_column, str);
    }
}

int
compile_ecs(
    FILE* in,
    FILE* out,
    unsigned int parse_version)
{
    long pos;
    unsigned int i;
    const uint32_t zero = 0;
    header_scpt_t header;
    header_eclh_t eclh;

    version = parse_version;

    if (!util_seekable(out)) {
        fprintf(stderr, "%s: output is not seekable\n", argv0);
        return 0;
    }

    yyin = in;

    if (yyparse() != 0)
        return 0;

    /* TODO: Use a memory buffer instead of writing many small chunks. */

    header.unknown1 = 1;
    header.include_length = 0;
    header.include_offset = 0;
    header.zero1 = 0;
    header.sub_cnt = sub_cnt;
    header.zero2[0] = 0;
    header.zero2[1] = 0;
    header.zero2[2] = 0;
    header.zero2[3] = 0;

    if (fputs("SCPT", out) == EOF) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 0;
    }
    if (!util_write(out, &header, sizeof(header_scpt_t)))
        return 0;

    header.include_offset = util_tell(out);
    if (header.include_offset == (uint32_t)-1) {
        return 0;
    }
    if (fputs("ANIM", out) == EOF) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 0;
    }
    if (!util_write(out, &anim_cnt, sizeof(uint32_t)))
        return 0;
    for (i = 0; i < anim_cnt; ++i) {
        if (!util_write(out, anim_list[i], strlen(anim_list[i]) + 1))
            return 0;
        free(anim_list[i]);
    }
    free(anim_list);

    pos = util_tell(out);
    if (pos == -1)
        return 0;
    if (pos % 4 != 0) {
        if (!util_write(out, &zero, 4 - pos % 4))
            return 0;
    }

    if (fputs("ECLI", out) == EOF) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 0;
    }
    if (!util_write(out, &ecli_cnt, sizeof(uint32_t)))
        return 0;
    for (i = 0; i < ecli_cnt; ++i) {
        if (!util_write(out, ecli_list[i], strlen(ecli_list[i]) + 1))
            return 0;
        free(ecli_list[i]);
    }
    free(ecli_list);

    pos = util_tell(out);
    if (pos == -1)
        return 0;
    if (pos % 4 != 0) {
        if (!util_write(out, &zero, 4 - pos % 4))
            return 0;
    }
    pos = util_tell(out);
    if (pos == -1)
        return 0;
    header.include_length = pos - header.include_offset;

    for (i = 0; i < sub_cnt; ++i) {
        if (!util_write(out, &zero, sizeof(uint32_t)))
            return 0;
    }

    for (i = 0; i < sub_cnt; ++i) {
        if (!util_write(out, subs[i].name, strlen(subs[i].name) + 1))
            return 0;
        free(subs[i].name);
    }

    pos = util_tell(out);
    if (pos == -1)
        return 0;
    if (pos % 4 != 0) {
        if (!util_write(out, &zero, 4 - pos % 4))
            return 0;
    }

    eclh.unknown1 = 0x10;
    eclh.zero[0] = 0;
    eclh.zero[1] = 0;

    for (i = 0; i < sub_cnt; ++i) {
        unsigned int j;
        subs[i].offset = util_tell(out);
        if (subs[i].offset == (uint32_t)-1)
            return 0;
        if (fputs("ECLH", out) == EOF) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 0;
        }
        if (!util_write(out, &eclh, sizeof(header_eclh_t)))
            return 0;
        for (j = 0; j < subs[i].instr_cnt; ++j) {
            char* data = instr_serialize(&subs[i], &subs[i].instrs[j]);
            if (!util_write(out, data, subs[i].instrs[j].size))
                return 0;
            free(data);
        }
        for (j = 0; j < subs[i].label_cnt; ++j) {
            free(subs[i].labels[j].name);
        }
        free(subs[i].labels);
        free(subs[i].instrs);
    }

    util_seek(out, 4);
    if (!util_write(out, &header, sizeof(header_scpt_t)))
        return 0;

    util_seek(out, header.include_offset + header.include_length);
    for (i = 0; i < sub_cnt; ++i) {
        if (!util_write(out, &subs[i].offset, sizeof(uint32_t)))
            return 0;
    }
    free(subs);

    return 1;
}
