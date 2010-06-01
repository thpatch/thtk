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

static int timer = 0;

static void add_anim(char*);
static void add_ecli(char*);
static void add_eclh(char*);

void yyerror(const char*);

typedef struct list_t {
    struct list_t* next;
    param_t* param;
} list_t;

static void instr_add(int id, int rank_mask, list_t* list);
static void label_create(char* label);
static int32_t label_find(sub_t* sub, const char* label);
static int make_stackinstr(int type, char stack1, char stack2, list_t* params);

int yylex(void);
extern FILE* yyin;

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
%token IF "if"
%token UNLESS "unless"
%token GOTO "goto"
%token AT "@"
%token BRACE_OPEN "{"
%token BRACE_CLOSE "}"
%token PARENTHESIS_OPEN "("
%token PARENTHESIS_CLOSE ")"
%token ILLEGAL_TOKEN "illegal token"
%token END_OF_FILE 0 "end of file"

%token LOAD
%token ADD "+"
%token SUBTRACT "-"
%token MULTIPLY "*"
%token DIVIDE "/"
%token MODULO "%"
%token LT "<"
%token LTEQ "<="
%token GT ">"
%token GTEQ ">="
%token EQUAL "=="
%token INEQUAL "!="
%token ASSIGN "="
%token NOT "!"
%token AND "&"
%token OR "|"
%token XOR "^"

%type <list> params
%type <list> cast_param
%type <list> square_param
%type <list> param
%type <list> includes
%type <integer> expression

%%

input:
    | input statement 
    ;

statement:
      "sub" IDENTIFIER "{" { add_eclh($2); } instructions "}" { add_eclh(NULL); }
    | "anim" "{" includes "}" {
        list_t* list = $3;

        while (list) {
            list_t* temp = list;
            add_anim(list->param->value.s.data);
            list = list->next;
            free(temp->param);
            free(temp);
        }
    }
    | "ecli" "{" includes "}" {
        list_t* list = $3;

        while (list) {
            list_t* temp = list;
            add_ecli(list->param->value.s.data);
            list = list->next;
            free(temp->param);
            free(temp);
        }
    }
    ;

includes:
      { $$ = NULL; }
    | TEXT ";" includes {
        $$ = malloc(sizeof(list_t));
        $$->param = malloc(sizeof(param_t));
        $$->param->type = 's';
        $$->param->value.s.data = $1;
        $$->next = $3;
    }
    ;

instructions:
    | instructions ";"
    | instructions IDENTIFIER ":" { label_create($2); }
    | instructions INTEGER ":" {
        if ($2 == timer || (timer > 0 && $2 < timer)) {
            char buf[256];
            snprintf(buf, 256, "illegal timer change: %d to %d", timer, $2);
            yyerror(buf);
        }
        timer = $2;
    }
    | instructions instruction ";"
    ;

/* TODO: Check the given parameters against the parameters expected for the
 *       instruction.  This requires passing a version parameter to eclc. */
instruction:
      INSTRUCTION RANK params { instr_add($1, $2, $3); }
    | INSTRUCTION params { instr_add($1, 0xff, $2); }
    | "if" expression "goto" square_param "@" square_param {
        if ($4->param->stack || $6->param->stack)
            yyerror("stack reference passed to goto");
        if ($4->param->type != 'o' || $6->param->type != 'i')
            yyerror("wrong parameter types for goto");
        $6->next = NULL;
        $4->next = $6;
        /* TODO: Use make_stackinstr or something here. */
        instr_add(14, 0xff, $4);
    }
    | "unless" expression "goto" square_param "@" square_param {
        if ($4->param->stack || $6->param->stack)
            yyerror("stack reference passed to goto");
        if ($4->param->type != 'o' || $6->param->type != 'i')
            yyerror("wrong parameter types for goto");
        $6->next = NULL;
        $4->next = $6;
        instr_add(13, 0xff, $4);
    }
    | "goto" square_param "@" square_param {
        if ($2->param->stack || $4->param->stack)
            yyerror("stack reference passed to goto");
        if ($2->param->type != 'o' || $4->param->type != 'i')
            yyerror("wrong parameter types for goto");
        $4->next = NULL;
        $2->next = $4;
        instr_add(12, 0xff, $2);
    }
    | square_param "=" expression {
        /* TODO: Error if 1 isn't a stack reference. */
        if (!$1->param->stack)
            yyerror("parameter is not a stack reference");
        $1->next = NULL;
        make_stackinstr(ASSIGN, $3, 0, $1);
    }
    | expression
    ;

expression:
      square_param {
        $1->next = NULL;
        $$ = make_stackinstr(LOAD, 0, 0, $1);
    }
    | "(" "!" expression ")"             { $$ = make_stackinstr(NOT,      $3, 0,  NULL); }
    | "(" expression "&"  expression ")" { $$ = make_stackinstr(AND,      $2, $4, NULL); }
    | "(" expression "|"  expression ")" { $$ = make_stackinstr(OR,       $2, $4, NULL); }
    | "(" expression "^"  expression ")" { $$ = make_stackinstr(XOR,      $2, $4, NULL); }
    | "(" expression "+"  expression ")" { $$ = make_stackinstr(ADD,      $2, $4, NULL); }
    | "(" expression "-"  expression ")" { $$ = make_stackinstr(SUBTRACT, $2, $4, NULL); }
    | "(" expression "*"  expression ")" { $$ = make_stackinstr(MULTIPLY, $2, $4, NULL); }
    | "(" expression "/"  expression ")" { $$ = make_stackinstr(DIVIDE,   $2, $4, NULL); }
    | "(" expression "%"  expression ")" { $$ = make_stackinstr(MODULO,   $2, $4, NULL); }
    | "(" expression "==" expression ")" { $$ = make_stackinstr(EQUAL,    $2, $4, NULL); }
    | "(" expression "!=" expression ")" { $$ = make_stackinstr(INEQUAL,  $2, $4, NULL); }
    | "(" expression "<"  expression ")" { $$ = make_stackinstr(LT,       $2, $4, NULL); }
    | "(" expression "<=" expression ")" { $$ = make_stackinstr(LTEQ,     $2, $4, NULL); }
    | "(" expression ">"  expression ")" { $$ = make_stackinstr(GT,       $2, $4, NULL); }
    | "(" expression ">=" expression ")" { $$ = make_stackinstr(GTEQ,     $2, $4, NULL); }
    ;

params:
      { $$ = NULL; }
    | cast_param params { $$ = $1; $$->next = $2; }
    ;

cast_param:
      "(int)" square_param {
        float floating;
        int integer;
        $$ = $2;
        if ($$->param->type == 'f') {
            floating = $$->param->value.f;
            $$->param->value.D[0] = 0x6966;
            memcpy(&$$->param->value.D[1], &floating, sizeof(float));
        } else if ($$->param->type == 'i') {
            integer = $$->param->value.i;
            $$->param->value.D[0] = 0x6969;
            $$->param->value.D[1] = integer;
        } else {
            yyerror("integer or float expected");
            YYABORT;
        }
        $$->param->type = 'D';
    }
    | "(float)" square_param {
        float floating;
        int integer;
        $$ = $2;
        if ($$->param->type == 'f') {
            floating = $$->param->value.f;
            $$->param->value.D[0] = 0x6666;
            memcpy(&$$->param->value.D[1], &floating, sizeof(float));
        } else if ($$->param->type == 'i') {
            integer = $$->param->value.i;
            $$->param->value.D[0] = 0x6669;
            $$->param->value.D[1] = integer;
        } else {
            yyerror("integer or float expected");
            YYABORT;
        }
        $$->param->type = 'D';
    }
    | square_param
    ;

square_param:
      "[" param "]" {
        $$ = $2;
        $$->param->stack = 1;
    }
    | param {
        $$ = $1;
        $$->param->stack = 0;
    }
    ;

param:
      CTEXT {
        $$ = malloc(sizeof(list_t));
        $$->param = malloc(sizeof(param_t));
        $$->param->type = 'c';
        $$->param->value.s.length = $1.length;
        $$->param->value.s.data = (char*)$1.data;
    }
    | TEXT {
        $$ = malloc(sizeof(list_t));
        $$->param = malloc(sizeof(param_t));
        $$->param->type = 's';
        $$->param->value.s.length = strlen($1);
        $$->param->value.s.data = $1;
    }
    | INTEGER {
        $$ = malloc(sizeof(list_t));
        $$->param = malloc(sizeof(param_t));
        $$->param->type = 'i';
        $$->param->value.i = $1;
    }
    | FLOATING {
        $$ = malloc(sizeof(list_t));
        $$->param = malloc(sizeof(param_t));
        $$->param->type = 'f';
        $$->param->value.f = $1;
    }
    | IDENTIFIER {
        $$ = malloc(sizeof(list_t));
        $$->param = malloc(sizeof(param_t));
        $$->param->type = 'o';
        $$->param->value.s.length = strlen($1);
        $$->param->value.s.data = $1;
    }
    ;

%%

static uint32_t anim_cnt;
static char** anim_list;
static uint32_t ecli_cnt;
static char** ecli_list;

static unsigned int sub_cnt;
static sub_t* subs;
static sub_t* current_sub;

static int
make_stackinstr(int type, char stack1, char stack2, list_t* params)
{
    const stackinstr_t* i;

    for (i = get_stackinstrs(10); i->type; ++i) {
        int ok = 1;

        if (i->type == type) {
            list_t* p;
            unsigned int j = 0;

            switch (strlen(i->stack)) {
            case 2:
                if (i->stack[1] != stack2)
                    ok = 0;
            case 1:
                if (i->stack[0] != stack1)
                    ok = 0;
            case 0:
                break;
            default:
                fprintf(stderr, "error wrong strlen\n");
                ok = 0;
                /* Error. */
                break;
            }

            for (p = params; p; p = p->next) {
                if (j > strlen(i->params))
                    ok = 0;
                if (i->params[j] != p->param->type)
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
        sprintf(buf, "no match found for %d, %c, %c", type, stack1, stack2);
        for (p = params; p; p = p->next) {
            fprintf(stderr, "  %c\n", p->param->type);
        }
        yyerror(buf);
        return 0;
    }

    instr_add(i->instr, 0xff, params);

    return i->value;
}

static void
add_eclh(char* arg)
{
    if (!arg) {
        current_sub = NULL;
        return;
    }

    sub_cnt++;
    subs = realloc(subs, sub_cnt * sizeof(sub_t));
    current_sub = &subs[sub_cnt - 1];

    timer = 0;
    current_sub->name = arg;
    current_sub->instr_cnt = 0;
    current_sub->instrs = NULL;
    current_sub->offset = 0;
    current_sub->label_cnt = 0;
    current_sub->labels = NULL;
}

static void
label_create(char* name)
{
    current_sub->label_cnt++;
    current_sub->labels = realloc(current_sub->labels, sizeof(label_t) * current_sub->label_cnt);
    current_sub->labels[current_sub->label_cnt - 1].name = name;
    current_sub->labels[current_sub->label_cnt - 1].offset = current_sub->offset;
}

static int32_t
label_find(sub_t* sub, const char* name)
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
instr_create(instr_t* instr, uint32_t time, uint16_t id, uint16_t param_mask, uint8_t rank_mask, uint8_t param_cnt, param_t* params)
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
            padded_length = instr->params[i].value.s.length + (4 - instr->params[i].value.s.length % 4);
            instr->size += sizeof(uint32_t);
            instr->size += padded_length;
            break;
        }
    }
}

static char*
instr_serialize(sub_t* sub, instr_t* op)
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
            padded_length = op->params[i].value.s.length + (4 - op->params[i].value.s.length % 4);

            memcpy(data + offset, &padded_length, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            if (op->params[i].type == 'c') {
                memcpy(data + offset, op->params[i].value.s.data, padded_length);
            } else {
                memcpy(data + offset, op->params[i].value.s.data, op->params[i].value.s.length);
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
instr_add(int id, int rank_mask, list_t* list)
{
    int param_mask = 0;
    uint8_t param_cnt = 0;
    param_t* params = NULL;
    instr_t* op;

    current_sub->instr_cnt++;
    current_sub->instrs = realloc(current_sub->instrs, current_sub->instr_cnt * sizeof(instr_t));

    op = &current_sub->instrs[current_sub->instr_cnt - 1];

    while (list) {
        list_t* temp;
        param_cnt++;

        params = realloc(params, param_cnt * sizeof(param_t));
        params[param_cnt - 1] = *list->param;

        if (list->param->stack)
            param_mask |= 1 << (param_cnt - 1);

        temp = list;
        list = list->next;
        free(temp->param);
        free(temp);
    }

    instr_create(op, timer, id, param_mask, rank_mask, param_cnt, params);

    op->offset = current_sub->offset;
    current_sub->offset += op->size;
}

static void
add_anim(char* arg)
{
    anim_cnt++;
    anim_list = realloc(anim_list, sizeof(char*) * anim_cnt);
    anim_list[anim_cnt - 1] = arg;
}

static void
add_ecli(char* arg)
{
    ecli_cnt++;
    ecli_list = realloc(ecli_list, sizeof(char*) * ecli_cnt);
    ecli_list[ecli_cnt - 1] = arg;
}

void
yyerror(const char* str)
{
    /* TODO: Research standard row and column range formats. */
    if (yylloc.first_line == yylloc.last_line) {
        if (yylloc.first_column == yylloc.last_column) {
            fprintf(stderr,
                    "%s:%s:%d,%d: %s\n",
                    argv0, current_input, yylloc.first_line, yylloc.first_column, str);
        } else {
            fprintf(stderr,
                    "%s:%s:%d,%d-%d: %s\n",
                    argv0, current_input, yylloc.first_line, yylloc.first_column, yylloc.last_column, str);
        }
    } else {
        fprintf(stderr,
                "%s:%s:%d,%d-%d,%d: %s\n",
                argv0, current_input, yylloc.first_line, yylloc.first_column, yylloc.last_line, yylloc.last_column, str);
    }
}

static void
print_usage(void)
{
    printf("Usage: %s -v {10,11,12,125} [OPTION]... [FILE]\n"
           "OPTION can be:\n"
           "  -o FILE  write output to the specified file\n"
           "  -h       display this help and exit\n"
           "  -V       display version information and exit\n\n"
           "Additional documentation might be available at <" PACKAGE_URL ">.\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

int
main(int argc, char* argv[])
{
    long pos;
    int ret;
    unsigned int i;
    int j;
    FILE* out;
    const uint32_t zero = 0;
    unsigned int version = 0;
    header_scpt_t header;
    header_eclh_t eclh;

    yyin = stdin;
    argv0 = "eclc";
    current_input = "(stdin)";
    out = stdout;

    for (j = 1; j < argc; ++j) {
        if (strcmp(argv[j], "-h") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[j], "-V") == 0) {
            util_print_version("eclc", PACKAGE_THECL_VERSION);
            return 0;
        } else if (strcmp(argv[j], "-v") == 0) {
            ++j;
            if (j == argc) {
                print_usage();
                return 1;
            }
            version = strtol(argv[j], NULL, 10);
        } else if (strcmp(argv[j], "-o") == 0) {
            ++j;
            if (j == argc) {
                print_usage();
                return 1;
            }
            out = fopen(argv[j], "wb");
            if (!out) {
                fprintf(stderr, "%s: couldn't open %s for writing: %s\n", argv0, argv[j], strerror(errno));
                return 1;
            }
        } else {
            break;
        }
    }

    if (version != 10 && version != 11 && version != 12 && version != 125) {
        print_usage();
        return 1;
    }

    if (j != argc) {
        yyin = fopen(argv[j], "r");
        current_input = argv[j];
        if (!yyin) {
            fprintf(stderr, "%s: couldn't open %s for reading: %s\n", argv0, argv[j], strerror(errno));
            return 1;
        }
    }

    ret = yyparse();

    if (ret != 0)
        return ret;

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
        return 1;
    }
    if (fwrite(&header, sizeof(header_scpt_t), 1, out) != 1) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 1;
    }

    header.include_offset = util_tell(out);
    if (header.include_offset == (uint32_t)-1) {
        return 1;
    }
    if (fputs("ANIM", out) == EOF) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 1;
    }
    if (fwrite(&anim_cnt, sizeof(uint32_t), 1, out) != 1) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 1;
    }
    for (i = 0; i < anim_cnt; ++i) {
        if (fwrite(anim_list[i], strlen(anim_list[i]) + 1, 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
        free(anim_list[i]);
    }
    free(anim_list);

    pos = util_tell(out);
    if (pos == -1)
        return 1;
    if (pos % 4 != 0) {
        if (fwrite(&zero, 4 - pos % 4, 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
    }

    if (fputs("ECLI", out) == EOF) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 1;
    }
    if (fwrite(&ecli_cnt, sizeof(uint32_t), 1, out) != 1) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 1;
    }
    for (i = 0; i < ecli_cnt; ++i) {
        if (fwrite(ecli_list[i], strlen(ecli_list[i]) + 1, 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
        free(ecli_list[i]);
    }
    free(ecli_list);

    pos = util_tell(out);
    if (pos == -1)
        return 1;
    if (pos % 4 != 0) {
        if (fwrite(&zero, 4 - pos % 4, 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
    }
    pos = util_tell(out);
    if (pos == -1)
        return 1;
    header.include_length = pos - header.include_offset;

    for (i = 0; i < sub_cnt; ++i) {
        if (fwrite(&zero, sizeof(uint32_t), 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
    }

    for (i = 0; i < sub_cnt; ++i) {
        if (fwrite(subs[i].name, strlen(subs[i].name) + 1, 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
        free(subs[i].name);
    }

    pos = util_tell(out);
    if (pos == -1)
        return 1;
    if (pos % 4 != 0) {
        if (fwrite(&zero, 4 - pos % 4, 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
    }

    eclh.unknown1 = 0x10;
    eclh.zero[0] = 0;
    eclh.zero[1] = 0;

    for (i = 0; i < sub_cnt; ++i) {
        unsigned int j;
        subs[i].offset = util_tell(out);
        if (subs[i].offset == (uint32_t)-1)
            return 1;
        if (fputs("ECLH", out) == EOF) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
        if (fwrite(&eclh, sizeof(header_eclh_t), 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
        for (j = 0; j < subs[i].instr_cnt; ++j) {
            char* data = instr_serialize(&subs[i], &subs[i].instrs[j]);
            if (fwrite(data, subs[i].instrs[j].size, 1, out) != 1) {
                fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
                return 1;
            }
            free(data);
        }
        for (j = 0; j < subs[i].label_cnt; ++j) {
            free(subs[i].labels[j].name);
        }
        free(subs[i].labels);
        free(subs[i].instrs);
    }

    util_seek(out, 4, NULL);
    if (fwrite(&header, sizeof(header_scpt_t), 1, out) != 1) {
        fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
        return 1;
    }

    util_seek(out, header.include_offset + header.include_length, NULL);
    for (i = 0; i < sub_cnt; ++i) {
        if (fwrite(&subs[i].offset, sizeof(uint32_t), 1, out) != 1) {
            fprintf(stderr, "%s: couldn't write: %s\n", argv0, strerror(errno));
            return 1;
        }
    }
    free(subs);

    fclose(yyin);
    fclose(out);
    return 0;
}
