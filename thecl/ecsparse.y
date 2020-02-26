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
#include "expr.h"
#include "path.h"
#include "file.h"
#include "list.h"
#include "program.h"
#include "thecl.h"
#include "value.h"

typedef struct {
    char* text;
} string_t;

static list_t* string_list_add(list_t* list, char* text);
static void string_list_free(list_t* list);

enum instr_flag {
    FLAG_RETURN_VAL = 2
};

static thecl_instr_t* instr_new(parser_state_t* state, unsigned int id, const char* format, ...);
static thecl_instr_t* instr_new_list(parser_state_t* state, unsigned int id, list_t* list);
static void instr_add(thecl_sub_t* sub, thecl_instr_t* instr);
static void instr_prepend(thecl_sub_t* sub, thecl_instr_t* instr);
/* Returns true if the created call was inline. */
static bool instr_create_call(parser_state_t *state, int type, char *name, list_t *params, bool needs_ret);

enum expression_type {
    EXPRESSION_OP,
    EXPRESSION_VAL,
    EXPRESSION_RANK_SWITCH,
    EXPRESSION_TERNARY,
    EXPRESSION_CALL
};

typedef struct expression_t {
    /* General things. */
    enum expression_type type;
    int id;
    /* For values: The value. */
    thecl_param_t* value;
    /* For operators: The child expressions. */
    /* This list is also used to store values for difficulty switches. */
    list_t children;
    /* For sub calls: the sub name and sub params. */
    char* name;
    list_t params;
    /* Resulting type of expression. */
    int result_type;
} expression_t;

typedef struct {
    expression_t *expr;
    char labelstr[250];
} switch_case_t;

static int parse_rank(const parser_state_t* state, const char* value);

static expression_t* expression_load_new(const parser_state_t* state, thecl_param_t* value);
static expression_t* expression_operation_new(const parser_state_t* state, const int* symbols, expression_t** operands);
static expression_t* expression_address_operation_new(const parser_state_t* state, const int* symbols, thecl_param_t* value);
static expression_t* expression_rank_switch_new(
    const parser_state_t* state, list_t* exprs
);
static expression_t* expression_ternary_new(const parser_state_t* state, expression_t* condition, expression_t* val1, expression_t* val2);
static expression_t* expression_call_new(const parser_state_t* state, list_t* param_list, char* sub_name);

static void expression_output(parser_state_t* state, expression_t* expr, int has_no_parents);
static void expression_free(expression_t* expr);
static void expression_optimize(parser_state_t* state, expression_t* expr);
#define EXPR_22(a, b, A, B) \
    expression_operation_new(state, (int[]){ a, b, 0 }, (expression_t*[]){ A, B, NULL })
#define EXPR_12(a, A, B) \
    expression_operation_new(state, (int[]){ a, 0 }, (expression_t*[]){ A, B, NULL })
#define EXPR_11(a, A) \
    expression_operation_new(state, (int[]){ a, 0 }, (expression_t*[]){ A, NULL })
#define EXPR_1A(a, A) \
    expression_address_operation_new(state, (int[]){ a, 0 }, A)
#define EXPR_21(a, b, A) \
    expression_operation_new(state, (int[]){ a, b, 0 }, (expression_t*[]){ A, NULL })

static expression_t *expression_copy(expression_t *expr);
static void expression_create_goto(parser_state_t *state, int type, char *labelstr);

/* Bison things. */
void yyerror(const parser_state_t*, const char*, ...);
int yylex(void);
extern FILE* yyin;

/* Parser APIs. */

/* Starts a new subroutine. */
static void sub_begin(parser_state_t* state, char* name, int is_timeline, int ret_type);
/* Closes the current subroutine. */
static void sub_finish(parser_state_t* state);

/* Begins a new scope. */
static void scope_begin(parser_state_t* state);
/* Ends the most recently started scope. */
static void scope_finish(parser_state_t* state);

/* Returns global definition of the given name, or NULL if it doesn't exist. */
static global_definition_t* global_get(parser_state_t* state, const char* name);

/* Creates a new variable in the specified subroutine. */
static thecl_variable_t* var_create(parser_state_t* state, thecl_sub_t* sub, const char* name, int type);
/* Creates a new variable in the specified subroutine, and assigns a value to it. */
static thecl_variable_t* var_create_assign(parser_state_t* state, thecl_sub_t* sub, const char* name, int type, expression_t* expr);
/* Returns true if the given variable is accessible in the current scope.. */
static bool var_accessible(parser_state_t* state, thecl_variable_t* var);
/* Returns variable of the given name in the specified sub, or NULL if the variable doesn't exist/is out of scope */
static thecl_variable_t* var_get(parser_state_t* state, thecl_sub_t* sub, const char* name);
/* Returns the stack offset of a specified variable in the specified sub. */
static int var_stack(parser_state_t* state, thecl_sub_t* sub, const char* name);
/* Returns the type of a specified variable in the specified sub. */
static int var_type(parser_state_t* state, thecl_sub_t* sub, const char* name);
/* Returns 1 if a variable of a given name exists, and 0 if it doesn't. */
static int var_exists(parser_state_t* state, thecl_sub_t* sub, const char* name);
/* Compiles a shorthand assignment operation on a given variable */
static void var_shorthand_assign(parser_state_t* state, thecl_param_t* param, expression_t* expr, int EXPRI, int EXPRF);
/* Stores a new label in the current subroutine pointing to the current offset. */
static void label_create(parser_state_t* state, char* label);

/* Update the current time label. */
void set_time(parser_state_t* state, int new_time);

/* Opens and parses the file of a given name. Returns a non-zero value on error. */
static int directive_include(parser_state_t* state, char* include_path);
/* Opens and loads an eclmap. */
static void directive_eclmap(parser_state_t* state, char* name);

/* Returned by Cast_Target2. */ 
static const char sub_param_ii[] = {'i', 'i'};
static const char sub_param_if[] = {'i', 'f'};
static const char sub_param_ff[] = {'f', 'f'};
static const char sub_param_fi[] = {'f', 'i'};

%}

%define parse.error verbose
%locations
%parse-param {parser_state_t* state}

%union {
    /* Values from Flex: */
    int integer;
    float floating;
    char* string;
    struct {
        unsigned int length;
        unsigned char* data;
    } bytes;

    /* Internal types: */
    struct thecl_param_t* param;
    struct expression_t* expression;
    struct list_t* list;
}

%token <integer> INSTRUCTION "instruction"
%token <string> IDENTIFIER "identifier"
%token <string> MNEMONIC "mnemonic"
%token <string> TEXT "text"
%token <integer> INTEGER "integer"
%token <floating> FLOATING "floating"
%token <string> RANK "rank"
%token <string> DIRECTIVE "directive"
%token COMMA ","
%token QUESTION "?"
%token COLON ":"
%token SEMICOLON ";"
%token SQUARE_OPEN "["
%token SQUARE_CLOSE "]"
%token CAST_INTEGER "_S"
%token CAST_FLOATING "_f"
%token CAST_II "_SS"
%token CAST_IF "_Sf"
%token CAST_FF "_ff"
%token CAST_FI "_fS"
%token ANIM "anim"
%token ECLI "ecli"
%token SUB "sub"
%token TIMELINE "timeline"
%token VAR "var"
%token INT "int"
%token FLOAT "float"
%token VOID "void"
%token INLINE "inline"
%token RETURN "return"
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
%token ELSE "else"
%token DO "do"
%token WHILE "while"
%token TIMES "times"
%token SWITCH "switch"
%token CASE "case"
%token DEFAULT "default"
%token BREAK "break"
%token ASYNC "async"
%token KILL
%token GLOBAL "global"
%token LOAD
%token LOADI
%token LOADF
%token ASSIGN "="
%token ASSIGNI
%token ASSIGNF
%token ASSIGNADD "+="
%token ASSIGNSUB "-="
%token ASSIGNMUL "*="
%token ASSIGNDIV "/="
%token ASSIGNMOD "%="
%token ASSIGNXOR "^="
%token ASSIGNBOR "|="
%token ASSIGNBAND "&="
%token ADD "+"
%token ADDI
%token ADDF
%token SUBTRACT "-"
%token SUBTRACTI
%token SUBTRACTF
%token MULTIPLY "*"
%token MULTIPLYI
%token MULTIPLYF
%token DIVIDE "/"
%token DIVIDEI
%token DIVIDEF
%token MODULO "%"
%token EQUAL "=="
%token EQUALI
%token EQUALF
%token INEQUAL "!="
%token INEQUALI
%token INEQUALF
%token LT "<"
%token LTI
%token LTF
%token LTEQ "<="
%token LTEQI
%token LTEQF
%token GT ">"
%token GTI
%token GTF
%token GTEQ ">="
%token GTEQI
%token GTEQF
%token NOT "!"
%token AND "&&"
%token OR "||"
%token XOR "^"
%token B_OR "|"
%token B_AND "&"
%token DEC "--"
%token NEG
%token NEGI
%token NEGF
%token SIN "sin"
%token COS "cos"
%token SQRT "sqrt"

%token DOLLAR "$"

%type <list> Text_Semicolon_List
%type <list> Instruction_Parameters_List
%type <list> Instruction_Parameters
%type <list> Rank_Switch_List

%type <expression> Expression
%type <expression> ExpressionSubsetInstParam
%type <expression> ExpressionSubsetInstruction
%type <expression> ExpressionLoadType
%type <expression> ExpressionCall
%type <expression> ExpressionSubset
%type <expression> Expression_Safe
%type <expression> ParenExpression

%type <param> Instruction_Parameter
%type <param> Address
%type <param> Address_Type
%type <param> Global_Def
%type <param> Integer
%type <param> Floating
%type <param> Text
%type <param> Label
%type <param> Load_Type
%type <param> Cast_Type

%type <integer> Cast_Target
%type <integer> DeclareKeyword
%type <integer> VarDeclaration

%type <string> Cast_Target2

%left QUESTION
%left OR
%left AND
%left B_OR
%left XOR
%left B_AND
%left EQUAL INEQUAL
%left LT LTEQ GT GTEQ
%left ADD SUBTRACT
%left MULTIPLY DIVIDE MODULO
%precedence NOT NEG
%precedence SIN COS SQRT
%precedence DEC

%expect 17
%%

Statements:
    %empty
    | Statements Statement
    ;

Statement:
      DeclareKeyword IDENTIFIER {
        sub_begin(state, $2, 0, $1);
        state->current_sub->is_inline = false;
        free($2);
      }
      "(" ArgumentDeclaration ")" {
            ssize_t arity = state->current_sub->stack / 4;
            state->current_sub->arity = arity;
            char* format = malloc(arity + 1);
            for (ssize_t i = 0; i < arity; ++i) {
                thecl_variable_t* var = state->current_sub->vars[i];
                format[i] = var->type;
            }
            format[arity] = '\0';
            state->current_sub->format = format;
      }
      Subroutine_Body {
        sub_finish(state);
      }
    | "inline" DeclareKeyword IDENTIFIER {
        if (!is_post_th10(state->version)) {
            yyerror(state, "inline sub creation is not supported for pre-th10 games");
            /* Can't continue after this. */
            exit(2);
        }
        sub_begin(state, $3, 0, $2);
        state->current_sub->is_inline = true;
        free($3);
      }
      "(" ArgumentDeclaration ")" {
            ssize_t arity = state->current_sub->stack / 4;
            state->current_sub->arity = arity;
            char* format = malloc(arity + 1);
            for (ssize_t i = 0; i < arity; ++i) {
                thecl_variable_t* var = state->current_sub->vars[i];
                format[i] = var->type;
            }
            format[arity] = '\0';
            state->current_sub->format = format;
      }
      Subroutine_Body {
        sub_finish(state);
      }
    | "timeline" IDENTIFIER "(" ")" {
        sub_begin(state, $2, 1, 0);
        state->current_sub->is_inline = false;
        free($2);
      }
      Subroutine_Body {
        sub_finish(state);
      }
    | "anim" "{" Text_Semicolon_List "}" {
        string_t* str;
        list_for_each($3, str) {
            state->ecl->anim_count++;
            state->ecl->anim_names = realloc(state->ecl->anim_names, state->ecl->anim_count * sizeof(char*));
            state->ecl->anim_names[state->ecl->anim_count - 1] = strdup(str->text);
        }
        string_list_free($3);
      }
    | "ecli" "{" Text_Semicolon_List "}" {
        string_t* str;
        list_for_each($3, str) {
            state->ecl->ecli_count++;
            state->ecl->ecli_names = realloc(state->ecl->ecli_names, state->ecl->ecli_count * sizeof(char*));
            state->ecl->ecli_names[state->ecl->ecli_count - 1] = strdup(str->text);
        }
        string_list_free($3);
      }
    | "global" IDENTIFIER "=" Global_Def ";" {
        global_definition_t *def = malloc(sizeof(global_definition_t));
        strncpy(def->name, $2, 256);
        def->param = $4;
        list_prepend_new(&state->global_definitions, def);
        free($2);
      }
    | DIRECTIVE TEXT {
        char buf[256];
        if (strcmp($1, "include") == 0) {
            if (directive_include(state, $2) != 0) {
                /* For proper syntax error displaying, this needs to return. */
                free($1);
                free($2);
                return 1;
            }
        } else if (strcmp($1, "eclmap") == 0) {
            directive_eclmap(state, $2);
        } else if (strcmp($1, "nowarn") == 0) {
            state->ecl->no_warn = (strcmp($2, "true") == 0);
        } else if (strcmp($1, "ins") == 0 || strcmp($1, "timeline_ins") == 0) {
            int is_timeline = ($1)[0] == 't';
            if (strlen($2) < 256) {
                /* arg format: "id format", e.g. "200 SSff" */
                char* arg = $2;
                size_t s = 0;
                while(arg[s] != ' ' && arg[s] != '\0') {
                    buf[s] = arg[s];
                    ++s;
                }
                buf[s] = '\0';
                int id = strtol(buf, NULL, 10);
                
                arg += s;
                s = 0;
                size_t spaces = 0;
                while(arg[s + spaces] != '\0') {
                    if (arg[s + spaces] == ' ') {
                        ++spaces;
                        continue;
                    }

                    buf[s] = arg[s + spaces];
                    ++s;
                }
                buf[s] = '\0';
                seqmap_entry_t ent = { id, buf };
                seqmap_set(is_timeline ? g_eclmap->timeline_ins_signatures : g_eclmap->ins_signatures, &ent);
            } else {
                yyerror(state, "#ins: specified format is too long");
            }

        } else {
            yyerror(state, "unknown directive: %s", $1);
        }
        free($1);
        free($2);
    }
    ;

Subroutine_Body:
      "{" Instructions "}" {
          state->current_sub->forward_declaration = false;
      }
    | ";" {
          state->current_sub->forward_declaration = true;
      }
    ;

Global_Def:
      Address
    | Integer
    | Floating
;

Text_Semicolon_List:
      TEXT ";" {
        $$ = list_new();
        string_list_add($$, $1);
      }
    | Text_Semicolon_List TEXT ";" {
        $$ = string_list_add($1, $2);
      }
    ;

DeclareKeyword:
      "void" { $$ = 0; }
    | "sub" { $$ = 0; } /* for backwards compatibility */
    | "var" { $$ = '?'; }
    | "int" { $$ = 'S'; }
    | "float" { $$ = 'f'; }
    ;

VarDeclaration:
      DeclareKeyword
    | DeclareKeyword IDENTIFIER {
          $$ = $1;
          var_create(state, state->current_sub, $2, $1);
          free($2);
      }
    | DeclareKeyword IDENTIFIER "=" Expression {
          $$ = $1;
          var_create_assign(state, state->current_sub, $2, $1, $4);
          free($2);
      }
    | VarDeclaration "," IDENTIFIER {
          $$ = $1;
          var_create(state, state->current_sub, $3, $1);
          free($3);
      }
    | VarDeclaration "," IDENTIFIER "=" Expression {
          $$ = $1;
          var_create_assign(state, state->current_sub, $3, $1, $5);
          free($3);
      }
    ;

ArgumentDeclaration:
    %empty
    | DeclareKeyword IDENTIFIER {
          var_create(state, state->current_sub, $2, $1);
          free($2);
      }
    | ArgumentDeclaration "," DeclareKeyword IDENTIFIER {
          var_create(state, state->current_sub, $4, $3);
          free($4);
      }
    ;

Instructions:
    %empty
    | Instructions INTEGER ":" { set_time(state, $2); }
    | Instructions "+" INTEGER ":" { set_time(state, state->instr_time + $3); }
    | Instructions IDENTIFIER ":" { label_create(state, $2); free($2); }
    | Instructions Instruction ";"
    | Instructions Block
    | Instructions RANK { state->instr_rank = parse_rank(state, $2); } 
    | Instructions RANK ":" { state->instr_rank = parse_rank(state, $2); } Instruction { state->instr_rank = parse_rank(state, "*"); } ";"
    ;

ParenExpression:
      "(" Expression ")"
        { $$ = $2; }
    |  /*%expect 8*/ { yyerror(state, "deprecated syntax, use parens around expr"); } Expression  /*%expect 2*/
        { $$ = $2; }
    ;

Block:
      /* Moving the old if ... gotos to Block, because if else would break with them being in Instruction. */
      "if" ParenExpression[cond] "goto" Label[label] "@" Integer[time] ";" {
        const expr_t* expr = expr_get_by_symbol(state->version, IF);
        expression_output(state, $cond, 1);
        expression_free($cond);
        instr_add(state->current_sub, instr_new(state, expr->id, "pp", $label, $time));
      }
    | "unless" ParenExpression[cond] "goto" Label[label] "@" Integer[time] ";" {
        const expr_t* expr = expr_get_by_symbol(state->version, UNLESS);
        expression_output(state, $cond, 1);
        expression_free($cond);
        instr_add(state->current_sub, instr_new(state, expr->id, "pp", $label, $time));
      }
    | "if" ParenExpression[cond] "goto" IDENTIFIER[label] ";" {
        expression_output(state, $cond, 1);
        expression_free($cond);
        expression_create_goto(state, IF, $label);
      }
    | "unless" ParenExpression[cond] "goto" IDENTIFIER[label] ";" {
        expression_output(state, $cond, 1);
        expression_free($cond);
        expression_create_goto(state, UNLESS, $label);
      }
    | IfBlock
    | WhileBlock
    | TimesBlock
    | SwitchBlock
    ;

CodeBlock:
      "{" {
          scope_begin(state);
      } Instructions "}" {
          scope_finish(state);
      }
    | Instruction ";"
    ;

BreakStatement:
      "break" {
          list_node_t *head = state->block_stack.head;
          for(; head; head = head->next) {
              if (
                  strncmp(head->data, "while", 5) == 0 ||
                  strncmp(head->data, "switch", 6) == 0 ||
                  strncmp(head->data, "times", 5) == 0
              ) {
                  char labelstr[256];
                  snprintf(labelstr, 256, "%s_end", (char*)head->data);
                  expression_create_goto(state, GOTO, labelstr);
                  break;
              }
          }
          if(!head) {
              yyerror(state, "break not within while or switch");
              g_was_error = true;
          }
      }
      ;

IfBlock:
    "unless" ParenExpression[cond]  /*%expect 1*/ {
          char labelstr[256];
          snprintf(labelstr, 256, "unless_%i_%i", yylloc.first_line, yylloc.first_column);
          list_prepend_new(&state->block_stack, strdup(labelstr));
          expression_output(state, $cond, 1);
          expression_free($cond);
          expression_create_goto(state, IF, labelstr);
      } CodeBlock ElseBlock {
          list_node_t *head = state->block_stack.head;
          label_create(state, head->data);
          state->block_stack.head = head->next;
          free(head->data);
          list_del(&state->block_stack, head);
        }
    | "if" ParenExpression[cond]  /*%expect 1*/ {
          char labelstr[256];
          snprintf(labelstr, 256, "if_%i_%i", yylloc.first_line, yylloc.first_column);
          list_prepend_new(&state->block_stack, strdup(labelstr));
          expression_output(state, $cond, 1);
          expression_free($cond);
          expression_create_goto(state, UNLESS, labelstr);
      } CodeBlock ElseBlock {
          list_node_t *head = state->block_stack.head;
          label_create(state, head->data);
          free(head->data);
          list_del(&state->block_stack, head);
      }
      ;

ElseBlock:
    %empty
    | "else"  {
          char labelstr[256];
          snprintf(labelstr, 256, "if_%i_%i", yylloc.first_line, yylloc.first_column);
          expression_create_goto(state, GOTO, labelstr);
          list_node_t *head = state->block_stack.head;
          label_create(state, head->data);
          free(head->data);
          list_del(&state->block_stack, head);
          list_prepend_new(&state->block_stack, strdup(labelstr));
    } CodeBlock
    | "else" {
          char labelstr[256];
          snprintf(labelstr, 256, "if_%i_%i", yylloc.first_line, yylloc.first_column);
          expression_create_goto(state, GOTO, labelstr);
          list_node_t *head = state->block_stack.head;
          label_create(state, head->data);
          free(head->data);
          list_del(&state->block_stack, head);
          list_prepend_new(&state->block_stack, strdup(labelstr));
      } IfBlock
      ;

WhileBlock:
      "while" ParenExpression[cond] {
          char labelstr[256];
          snprintf(labelstr, 256, "while_%i_%i", yylloc.first_line, yylloc.first_column);
          char labelstr_st[256];
          char labelstr_end[256];
          snprintf(labelstr_st, 256, "%s_st", (char*)labelstr);
          snprintf(labelstr_end, 256, "%s_end", (char*)labelstr);

          list_prepend_new(&state->block_stack, strdup(labelstr));
          label_create(state, labelstr_st);
          expression_output(state, $cond, 1);
          expression_free($cond);
          expression_create_goto(state, UNLESS, labelstr_end);
      } CodeBlock {
          char labelstr_st[256];
          char labelstr_end[256];
          list_node_t *head = state->block_stack.head;
          snprintf(labelstr_st, 256, "%s_st", (char*)head->data);
          snprintf(labelstr_end, 256, "%s_end", (char*)head->data);

          expression_create_goto(state, GOTO, labelstr_st);
          label_create(state, labelstr_end);

          free(head->data);
          list_del(&state->block_stack, head);
      }
    | "do"  {
          char labelstr[256];
          snprintf(labelstr, 256, "while_%i_%i", yylloc.first_line, yylloc.first_column);
          char labelstr_st[256];
          char labelstr_end[256];
          snprintf(labelstr_st, 256, "%s_st", (char*)labelstr);
          snprintf(labelstr_end, 256, "%s_end", (char*)labelstr);

          list_prepend_new(&state->block_stack, strdup(labelstr));
          label_create(state, labelstr_st);
    } CodeBlock "while" ParenExpression[cond]  {
          char labelstr_st[256];
          char labelstr_end[256];
          list_node_t *head = state->block_stack.head;
          snprintf(labelstr_st, 256, "%s_st", (char*)head->data);
          snprintf(labelstr_end, 256, "%s_end", (char*)head->data);

          expression_output(state, $cond, 1);
          expression_free($cond);
          expression_create_goto(state, IF, labelstr_st);
          label_create(state, labelstr_end);

          free(head->data);
          list_del(&state->block_stack, head);
    } ";"
    ;

TimesBlock:
      "times" ParenExpression[count] {
          if (g_ecl_simplecreate) {
              yyerror(state, "times loops are not allowed in simple creation mode");
              exit(2);
          }
          if ($count->result_type != 'S') {
              yyerror(state, "invalid iteration count type for a times loop: %c", $count->result_type);
              exit(2);
          }

          char loop_name[256];
          snprintf(loop_name, 256, "times_%i_%i", yylloc.first_line, yylloc.first_column);
          thecl_variable_t* var = var_create_assign(state, state->current_sub, loop_name, 'S', $count);

          char labelstr_st[256];
          char labelstr_end[256];
          snprintf(labelstr_st, 256, "%s_st", (char*)loop_name);
          snprintf(labelstr_end, 256, "%s_end", (char*)loop_name);

          label_create(state, labelstr_st);
          
          thecl_param_t* param = param_new('S');
          param->stack = 1;
          param->value.val.S = var->stack;

          const expr_t* expr = expr_get_by_symbol(state->version, DEC);
          instr_add(state->current_sub, instr_new(state, expr->id, "p", param));

          expression_create_goto(state, UNLESS, labelstr_end);

          list_prepend_new(&state->block_stack, strdup(loop_name));
    } CodeBlock {
          char labelstr_st[256];
          char labelstr_end[256];
          list_node_t *head = state->block_stack.head;
          snprintf(labelstr_st, 256, "%s_st", (char*)head->data);
          snprintf(labelstr_end, 256, "%s_end", (char*)head->data);
          
          thecl_variable_t* var = var_get(state, state->current_sub, (char*)head->data);
          var->is_unused = true; /* Allow next created var to reuse the stack offset of this one. */

          expression_create_goto(state, GOTO, labelstr_st);
          label_create(state, labelstr_end);
          
          free(head->data);
          list_del(&state->block_stack, head);
    }
    ;

SwitchBlock:
    "switch" ParenExpression[cond] {
          char name[256];
          list_prepend_new(&state->block_stack, NULL); /* The NULL acts as a sentinel of switch cases. */
          snprintf(name, 256, "switch_%i_%i", yylloc.first_line, yylloc.first_column);
          list_prepend_new(&state->block_stack, strdup(name));
          
          /* The expression value needs to be stored in a variable, in case some kind of RAND variable was passed. */
          thecl_variable_t* var = var_create(state, state->current_sub, name, $cond->result_type);
          expression_output(state, $cond, 1);
          thecl_param_t* param = param_new($cond->result_type);
          param->stack = 1;
          if (param->type == 'S')
              param->value.val.S = var->stack;
          else
              param->value.val.f = (float)var->stack;
          
          const expr_t* tmp = expr_get_by_symbol(state->version, $cond->result_type == 'S' ? ASSIGNI : ASSIGNF);
          instr_add(state->current_sub, instr_new(state, tmp->id, "p", param));
          list_prepend_new(&state->block_stack, var); /* We will need it later. */
          expression_free($cond);

          expression_create_goto(state, GOTO, name); /* Jump to the case checks. */
    } "{" {
          scope_begin(state);
    } CaseList "}" {
          scope_finish(state);

          list_node_t* head = state->block_stack.head;
          thecl_variable_t* var = (thecl_variable_t*)head->data;
          thecl_param_t* param = param_new(var->type);
          param->stack = 1;
          if (param->type == 'S')
              param->value.val.S = var->stack;
          else
              param->value.val.f = (float)var->stack;
          list_del(&state->block_stack, head);

          head = state->block_stack.head;

          char labelstr[256];
          snprintf(labelstr, 256, "%s_end", (char*)head->data);
          expression_create_goto(state, GOTO, labelstr);

          label_create(state, head->data);

          free(head->data);
          list_del(&state->block_stack, head);

          const expr_t* expr = expr_get_by_symbol(state->version, param->type == 'S' ? EQUALI : EQUALF);
          int id_cmp = expr->id;
          expr = expr_get_by_symbol(state->version, param->type == 'S' ? LOADI : LOADF);
          int id_load = expr->id;

          list_node_t *node = state->block_stack.head;
          while (node->data) {
              switch_case_t *switch_case = node->data;

              if (switch_case->expr != NULL) { /* if expr is NULL, it's the "default" case. */
                  expression_output(state, switch_case->expr, 1);
                  expression_free(switch_case->expr);

                  instr_add(state->current_sub, instr_new(state, id_load, "p", param_copy(param)));
                  instr_add(state->current_sub, instr_new(state, id_cmp, ""));

                  expression_create_goto(state, IF, switch_case->labelstr);
              } else {
                  expression_create_goto(state, GOTO, switch_case->labelstr);
              }
              list_node_t *buf = node;
              node = node->next;
              free(buf->data);
              free(buf);
          }
          param_free(param);
          if (node->next != NULL) /* Prevent crashing when there is nothing else on the block stack. */
              node->next->prev = NULL;
          state->block_stack.head = node->next;
          free(node);

          label_create(state, labelstr);
          var->is_unused = true; /* Allow next created var to use the same stack offset as this one. */
    }
    ;

CaseList:
    Case Instructions
    | CaseList Case Instructions
    ;

Case:
     "case" Expression_Safe ":" {
          switch_case_t *switch_case = malloc(sizeof(switch_case_t));
          switch_case->expr = $2;

          list_node_t *node = state->block_stack.head;
          if (((thecl_variable_t*)node->data)->type != $2->result_type)
              yyerror(state, "wrong value type in switch case");

          snprintf(switch_case->labelstr, 250, "case_%i_%i", yylloc.first_line, yylloc.first_column);

          label_create(state, switch_case->labelstr);
          
          /* Every case has to be prepended to the sentinel. */
          while(node->data) /* Sentinel has data=NULL */
              node = node->next;

          list_prepend_to(&state->block_stack, switch_case, node); /* Prepends to the sentinel. */
      }
    |
     "default" ":" {
          switch_case_t *switch_case = malloc(sizeof(switch_case_t));
          switch_case->expr = NULL;

          snprintf(switch_case->labelstr, 250, "case_%i_%i", yylloc.first_line, yylloc.first_column);
          label_create(state, switch_case->labelstr);

          list_node_t *node = state->block_stack.head;
          while(node->data)
              node = node->next;

          list_prepend_to(&state->block_stack, switch_case, node);
     } 
    ;

Instruction:
      "@" IDENTIFIER "(" Instruction_Parameters ")" {
          /* Force creating a sub call, even if it wasn't defined in the file earlier - useful for calling subs from default.ecl */
          instr_create_call(state, TH10_INS_CALL, $2, $4, false);
          if ($4 != NULL) {
              list_free_nodes($4);
              free($4);
          }
      }
      | "@" IDENTIFIER "(" Instruction_Parameters ")" "async" {
          /* Same as above, except use ins_15 (callAsync) instead of ins_11 (call) */
          instr_create_call(state, TH10_INS_CALL_ASYNC, $2, $4, false);
          if ($4 != NULL) {
              list_free_nodes($4);
              free($4);
          }
      }
      | "@" IDENTIFIER "(" Instruction_Parameters ")" "async" Expression {
          /* Can't have the parameter generated by Cast_Type
           * since it would cause expressions to be output
           * in the wrong order. */
          if ($7->result_type != 'S')
              yyerror(state, "bad id parameter type for async id sub call");

          thecl_param_t* param = param_new('S');
          if ($7->type == EXPRESSION_VAL) {
              param->value.val.S = $7->value->value.val.S;
          } else {
              param->stack = 1;
              param->is_expression_param = 'S';
              param->value.val.S = -1;
              list_prepend_new(&state->expressions, $7);
          }

          if ($4 == NULL)
              $4 = list_new();

          list_prepend_new($4, param);
          instr_create_call(state, TH10_INS_CALL_ASYNC_ID, $2, $4, false);
          list_free_nodes($4);
          free($4);
          if ($7->type == EXPRESSION_VAL)
            expression_free($7);
      }
      | IDENTIFIER "(" Instruction_Parameters ")" "async" {
          /* Since sub existence for call ins is checked after parsing, there is no need to check here anymore. */
          instr_create_call(state, TH10_INS_CALL_ASYNC, $1, $3, false);
          if ($3 != NULL) {
              list_free_nodes($3);
              free($3);
          }
      }
      | IDENTIFIER "(" Instruction_Parameters ")" "async" Expression {
          /* Can't have the parameter generated by Cast_Type
           * since it would cause expressions to be output
           * in the wrong order. */
          if ($6->result_type != 'S')
              yyerror(state, "bad id parameter type for async id sub call");

          thecl_param_t* param = param_new('S');
          if ($6->type == EXPRESSION_VAL) {
              param->value.val.S = $6->value->value.val.S;
          } else {
              param->stack = 1;
              param->is_expression_param = 'S';
              param->value.val.S = -1;
              list_prepend_new(&state->expressions, $6);
          }

          if ($3 == NULL)
              $3 = list_new();

          list_prepend_new($3, param);
          instr_create_call(state, TH10_INS_CALL_ASYNC_ID, $1, $3, false);
          list_free_nodes($3);
          free($3);
          if ($6->type == EXPRESSION_VAL)
            expression_free($6);
      }
      | IDENTIFIER "(" Instruction_Parameters ")" {
        instr_create_call(state, TH10_INS_CALL, $1, $3, false);
        if ($3 != NULL) {
            list_free_nodes($3);
            free($3);
        }
      }
      | MNEMONIC "(" Instruction_Parameters ")" {
        seqmap_entry_t* ent = seqmap_find(state->is_timeline_sub ? g_eclmap->timeline_ins_names : g_eclmap->ins_names, $1);
        if (!ent) {
            /* Default to creating a sub call */
            instr_create_call(state, TH10_INS_CALL, $1, $3, false);
        } else {
            expression_t* expr;
            list_for_each(&state->expressions, expr) {
                expression_output(state, expr, 1);
                expression_free(expr);
            }
            list_free_nodes(&state->expressions);
            instr_add(state->current_sub, instr_new_list(state, ent->key, $3));
        }
        if ($3 != NULL) {
            list_free_nodes($3);
            free($3);
        }
      }
    | INSTRUCTION "(" Instruction_Parameters ")" {
        expression_t* expr;
        list_for_each(&state->expressions, expr) {
            expression_output(state, expr, 1);
            expression_free(expr);
        }
        list_free_nodes(&state->expressions);

        instr_add(state->current_sub, instr_new_list(state, $1, $3));

        if ($3 != NULL) {
            list_free_nodes($3);
            free($3);
        }
      }
    | "goto" Label "@" Integer {
        const expr_t* expr = expr_get_by_symbol(state->version, GOTO);
        instr_add(state->current_sub, instr_new(state, expr->id, "pp", $2, $4));
    }
    | "goto" IDENTIFIER {
        /* Timeless goto automatically sets time to the time of the target label. */
        expression_create_goto(state, GOTO, $2);
    }
    | Assignment
    | ExpressionSubsetInstruction {
        expression_output(state, $1, 1);
        expression_free($1);
      }
    | VarDeclaration {
        if (g_ecl_simplecreate)
            instr_add(state->current_sub, instr_new(state, TH10_INS_STACK_ALLOC, "S", state->current_sub->stack));
     }
    | BreakStatement
    | "return" Expression {
        if (!is_post_th10(state->version))
            yyerror(state, "return statement is not supported pre-th10");

        if ($2->result_type != state->current_sub->ret_type)
            yyerror(state, "bad return type in the return statement");

        int flags_old = state->instr_flags;
        state->instr_flags |= FLAG_RETURN_VAL;
        expression_output(state, $2, 1);
        /* We can't just unset the flag, since it could've already been set earlier. */
        state->instr_flags = flags_old;

        if (state->current_sub->is_inline)
            expression_create_goto(state, GOTO, "inline_end");
        else {
            /* Since inline subs don't need to use return registers to return values,
             * I3/F3 are only set when the sub is not inline. */
            thecl_param_t* param = param_new($2->result_type);
            param->stack = 1;
            if ($2->result_type == 'S')
                param->value.val.S = TH10_VAR_I3;
            else
                param->value.val.f = TH10_VAR_F3;
        
            instr_add(state->current_sub, instr_new(state, $2->result_type == 'S' ? TH10_INS_SETI : TH10_INS_SETF, "p", param));
            instr_add(state->current_sub, instr_new(state, TH10_INS_RET_NORMAL, ""));
        }
     }
    | "return" {
        if (!is_post_th10(state->version))
            yyerror(state, "return statement is not supported pre-th10");

        if (state->current_sub->is_inline)
            expression_create_goto(state, GOTO, "inline_end");
        else 
            instr_add(state->current_sub, instr_new(state, TH10_INS_RET_NORMAL, ""));
    }
    ;

Assignment:
      Address "=" Expression {
        const expr_t* expr = expr_get_by_symbol(state->version, $1->type == 'S' ? ASSIGNI : ASSIGNF);
        expression_output(state, $3, 1);
        expression_free($3);
        instr_add(state->current_sub, instr_new(state, expr->id, "p", $1));
        thecl_variable_t* var = NULL;
        if ($1->value.type == 'S') {
            if ($1->value.val.S >= 0) var = state->current_sub->vars[$1->value.val.S / 4];
        } else {
            if ($1->value.val.f >= 0.0f) var = state->current_sub->vars[(int)$1->value.val.f / 4];
        }
        if (var != NULL) var->is_written = true;
      }
    | Address "+=" Expression { var_shorthand_assign(state, $1, $3, ADDI, ADDF); }
    | Address "-=" Expression { var_shorthand_assign(state, $1, $3, SUBTRACTI, SUBTRACTF); }
    | Address "*=" Expression { var_shorthand_assign(state, $1, $3, MULTIPLYI, MULTIPLYF); }
    | Address "/=" Expression { var_shorthand_assign(state, $1, $3, DIVIDEI, DIVIDEF); }
    | Address "%=" Expression { var_shorthand_assign(state, $1, $3, MODULO, 0); }
    | Address "^=" Expression { var_shorthand_assign(state, $1, $3, XOR, 0); }
    | Address "|=" Expression { var_shorthand_assign(state, $1, $3, B_OR, 0); }
    | Address "&=" Expression { var_shorthand_assign(state, $1, $3, B_AND, 0); }
;

Instruction_Parameters:
    %empty { $$ = NULL; }
    | Instruction_Parameters_List
    ;

Instruction_Parameters_List:
      Instruction_Parameter {
        $$ = list_new();
        list_append_new($$, $1);
      }
    | Instruction_Parameters_List "," Instruction_Parameter {
        $$ = $1;
        list_append_new($$, $3);
      }
    ;

Cast_Target2:
      CAST_II { $$ = sub_param_ii; }
    | CAST_IF { $$ = sub_param_if; }
    | CAST_FF { $$ = sub_param_ff; }
    | CAST_FI { $$ = sub_param_fi; }
    ;

Cast_Target:
      CAST_INTEGER  { $$ = 'S'; }
    | CAST_FLOATING { $$ = 'f'; }
    ;

Cast_Type:
      Address
    | Integer
    | Floating
    | "(" Expression ")" {
        list_prepend_new(&state->expressions, $2);

        $$ = param_new($2->result_type);
        $$->stack = 1;
        $$->is_expression_param = $2->result_type;
        if ($2->result_type == 'S') {
            $$->value.val.S = -1;
        } else {
            $$->value.val.f = -1.0f;
        }
      }
    ;

Instruction_Parameter:
      Load_Type
    | Text
    | Cast_Target2[types] Cast_Type[param] {
        $$ = param_new('D');
        $$->stack = $param->stack;
        $$->is_expression_param = $param->is_expression_param;
        $$->value.type = 'm';
        $$->value.val.m.length = sizeof(thecl_sub_param_t);
        $$->value.val.m.data = malloc(sizeof(thecl_sub_param_t));
        thecl_sub_param_t* D = (thecl_sub_param_t*)$$->value.val.m.data;
        D->zero = 0;
        D->from = $types[1];
        D->to = $types[0];
        if ($param->type == 'f') {
            D->val.f = $param->value.val.f;
        } else {
            D->val.S = $param->value.val.S;
        }
        param_free($param);
      }
      | ExpressionSubsetInstParam {
          if ($1->type == EXPRESSION_VAL) {
              $$ = $1->value;
              expression_free($1);
          }
          else {
              list_prepend_new(&state->expressions, $1);

              $$ = param_new($1->result_type);
              $$->stack = 1;
              $$->is_expression_param = $1->result_type;
              if ($1->result_type == 'S') {
                  $$->value.val.S = -1;
              } else {
                  $$->value.val.f = -1.0f;
              }
          }
      }
    ;

Rank_Switch_List:
      Expression_Safe ":" Expression_Safe {
        $$ = list_new();
        list_append_new($$, $1);
        list_append_new($$, $3);
      }
    | Rank_Switch_List ":" Expression_Safe {
        $$ = $1;
        list_append_new($$, $3);
      }
    ;

Expression:
      ExpressionLoadType
    | ExpressionCall
    | ExpressionSubset
    ;

ExpressionSubsetInstParam:
      ExpressionCall
    | ExpressionSubset
    ;

ExpressionSubsetInstruction:
      ExpressionLoadType
    | ExpressionSubset
    ;

ExpressionLoadType:
      Load_Type                      { $$ = expression_load_new(state, $1); }
    ;

ExpressionCall:
      IDENTIFIER "(" Instruction_Parameters ")"          { $$ = expression_call_new(state, $3, $1); }
    ;

/* This is the lowest common denominator between expression-instructions and expression-parameters */
ExpressionSubset:
                  "(" Expression ")" { $$ = $2; }
    | Cast_Target "(" Expression ")" { $$ = $3; $$->result_type = $1; }
    | Expression "+"   Expression { $$ = EXPR_22(ADDI,      ADDF,      $1, $3); }
    | Expression "-"   Expression { $$ = EXPR_22(SUBTRACTI, SUBTRACTF, $1, $3); }
    | Expression "*"   Expression { $$ = EXPR_22(MULTIPLYI, MULTIPLYF, $1, $3); }
    | Expression "/"   Expression { $$ = EXPR_22(DIVIDEI,   DIVIDEF,   $1, $3); }
    | Expression "%"   Expression { $$ = EXPR_12(MODULO,               $1, $3); }
    | Expression "=="  Expression { $$ = EXPR_22(EQUALI,    EQUALF,    $1, $3); }
    | Expression "!="  Expression { $$ = EXPR_22(INEQUALI,  INEQUALF,  $1, $3); }
    | Expression "<"   Expression { $$ = EXPR_22(LTI,       LTF,       $1, $3); }
    | Expression "<="  Expression { $$ = EXPR_22(LTEQI,     LTEQF,     $1, $3); }
    | Expression ">"   Expression { $$ = EXPR_22(GTI,       GTF,       $1, $3); }
    | Expression ">="  Expression { $$ = EXPR_22(GTEQI,     GTEQF,     $1, $3); }
    | "!" Expression              { $$ = EXPR_11(NOT,                  $2); }
    | Expression "||"  Expression { $$ = EXPR_12(OR,                   $1, $3); }
    | Expression "&&"  Expression { $$ = EXPR_12(AND,                  $1, $3); }
    | Expression "^"   Expression { $$ = EXPR_12(XOR,                  $1, $3); }
    | Expression "|" Expression   { $$ = EXPR_12(B_OR,                 $1, $3); }
    | Expression "&" Expression   { $$ = EXPR_12(B_AND,                $1, $3); }
    | "+" Expression              { $$ = $2; }
    | Address "--"                { 
                                    $$ = EXPR_1A(DEC, $1);
                                    if ($1->value.val.S >= 0) /* Stack variables only. This is also verrfied to be int by expression creation. */
                                        state->current_sub->vars[$1->value.val.S / 4]->is_written = true;
                                  }
    | "-" Expression  %prec NEG   {
                                      if (is_post_th13(state->version)) {
                                          $$ = EXPR_21(NEGI, NEGF, $2);
                                      } else {
                                          thecl_param_t* p = param_new($2->result_type);
                                          if (p->value.type == 'f')
                                            p->value.val.f = 0;
                                          else 
                                            p->value.val.S = 0;
                                          $$ = EXPR_22(SUBTRACTI, SUBTRACTF, expression_load_new(state, p), $2);
                                      }
                                  }
    | "sin" Expression            { $$ = EXPR_11(SIN,                  $2); }
    | "cos" Expression            { $$ = EXPR_11(COS,                  $2); }
    | "sqrt" Expression           { $$ = EXPR_11(SQRT,                 $2); }

    /* Custom expressions. */
    | Rank_Switch_List            { $$ = expression_rank_switch_new(state, $1); }
    | Expression "?" Expression_Safe ":" Expression_Safe  %prec QUESTION
                                  { $$ = expression_ternary_new(state, $1, $3, $5); }
    ;

/* 
   The purpose of this is to be used in places that contain certain tokens
   that could be a part of an expression too, to prevent such tokens from
   mistakenly being parsed as expressions.
   An example of such situation is the ':' from "case 1:" being parsed as a part of
   the rank switch expression.
   Of course, this still allows any expression to be put in - it just requires it to
   be in brackets (unless it's a literal), which prevents any bad things from happening.
*/
Expression_Safe:
      Load_Type                      { $$ = expression_load_new(state, $1); }
    |             "(" Expression ")" { $$ = $2; }
    | Cast_Target "(" Expression ")" { $$ = $3; $$->result_type = $1; }
    ;

Address:
      "[" Address_Type "]" {
        $$ = $2;
        $$->stack = 1;
      }
    | "$" IDENTIFIER {
        $$ = param_new('S');
        $$->stack = 1;
        $$->value.val.S = var_stack(state, state->current_sub, $2);
        free($2);
      }
    | "%" IDENTIFIER {
        $$ = param_new('f');
        $$->stack = 1;
        $$->value.val.f = var_stack(state, state->current_sub, $2);
        free($2);
      }
    | IDENTIFIER  /*%expect 2*/ {
        if (var_exists(state, state->current_sub, $1)) {
            int type = var_type(state, state->current_sub, $1);
            if (type == '?') {
                yyerror(state, "typeless variables need to have their type specified with a %% or $ prefix when used: %s", $1);
                exit(2);
            }
            $$ = param_new(type);
            $$->stack = 1;
            if (type == 'S') {
                $$->value.val.S = var_stack(state, state->current_sub, $1);
            } else {
                $$->value.val.f = var_stack(state, state->current_sub, $1);
            }
        } else {
            global_definition_t *def = global_get(state, $1);
            if (def != NULL) {
                $$ = param_copy(def->param);
            } else {
                if (
                       is_post_th10(state->version) /* Old versions don't have stack vars anyway, so no need to show the warning... */
                    && (state->current_sub == NULL || strncmp($1, state->current_sub->name, strlen(state->current_sub->name)) != 0)
                ) {
                    yyerror(state, "warning: %s not found as a variable or global definition, treating like a label instead.", $1);
                }
                $$ = param_new('o');
                $$->value.type = 'z';
                $$->value.val.z = strdup($1);
            }
        }
        free($1);
    }
    ;

Address_Type:
      Integer
    | Floating
    ;

Integer:
    INTEGER  /*%expect 1*/ {
        $$ = param_new('S');
        $$->value.val.S = $1;
      }
    ;

Floating:
    FLOATING {
        $$ = param_new('f');
        $$->value.val.f = $1;
      }
    ;

Text:
    TEXT {
        $$ = param_new('z');
        $$->value.val.z = $1;
      }
    ;

Label:
    IDENTIFIER {
        $$ = param_new('o');
        $$->value.type = 'z';
        $$->value.val.z = $1;
      }
    ;

Load_Type:
      Address
    | Integer
    | Floating
    ;

%%

static list_t*
string_list_add(
    list_t* list,
    char* text)
{
    string_t* s = malloc(sizeof(string_t));
    s->text = text;
    list_append_new(list, s);
    return list;
}

static void
string_list_free(
    list_t* list)
{
    string_t* s;
    list_for_each(list, s) {
        free(s->text);
        free(s);
    }
    list_free_nodes(list);
    free(list);
}

static thecl_instr_t*
instr_init(
    parser_state_t* state)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->time = state->instr_time;
    instr->rank = state->instr_rank;
    instr->flags = state->instr_flags;
    return instr;
}

static void
instr_set_types(
    parser_state_t* state,
    thecl_instr_t* instr)
{
    const char* format = state->instr_format(state->version, instr->id, state->is_timeline_sub);
    if (format == NULL) /* Error message for this is shown somewhere else. */
        return;

    thecl_param_t* param;
    int param_n = 1;
    list_for_each(&instr->params, param) {
        int new_type;
        /* XXX: How to check for errors?
         * Perhaps some kind of function that returns a list of satisfying types?
         * Or should there only be one type? */
        /* TODO: Implement * and ? if needed. */
        if (*format == '*')
            new_type = *(format + 1);
        else
            new_type = *format;

        if (new_type != param->type &&
            !(param->type == 'z' && (new_type == 'm' || new_type == 'x' || new_type == 'N' || new_type == 'n')) &&
            !(param->type == 'S' && (new_type == 's' || new_type == 'U' || new_type == 't'))
        ) {
            seqmap_entry_t* ent = seqmap_get(g_eclmap->ins_names, instr->id);
            char buf[128];
            if (ent == NULL)
                snprintf(buf, sizeof(buf), "%d", instr->id);
            else
                snprintf(buf, sizeof(buf), "%d (%s)", instr->id, ent->value);
            yyerror(state, "instr_set_types: in sub %s: wrong argument "
                     "type for parameter %d for opcode %s (expected: %c, got: %c)",
                     state->current_sub->name, param_n, buf, new_type, param->type);
        }

        param->type = new_type;

        if (*format != '*')
            ++format;

        /* Do not read past the end of the format string.
         * "Too many parameters" error will be thrown somewhere else anyway. */ 
        if (*format == '\0')
            break;

        ++param_n;
    }

    return;
}

static thecl_instr_t*
instr_new(
    parser_state_t* state,
    unsigned int id,
    const char* format,
    ...)
{
    va_list ap;
    thecl_instr_t* instr = instr_init(state);
    instr->id = id;

    va_start(ap, format);
    while (*format) {
        thecl_param_t* param;
        if (*format == 'p') {
            param = va_arg(ap, thecl_param_t*);
        } else if (*format == 'S') {
            param = param_new('S');
            param->value.val.S = va_arg(ap, int32_t);
        } else {
            param = NULL;
        }
        list_append_new(&instr->params, param);
        ++instr->param_count;
        ++format;
    }
    va_end(ap);

    instr_set_types(state, instr);

    instr->size = state->instr_size(state->version, instr, state->is_timeline_sub);

    return instr;
}

static thecl_instr_t*
instr_new_list(
    parser_state_t* state,
    unsigned int id,
    list_t* list)
{
    thecl_instr_t* instr = instr_init(state);
    thecl_param_t* param;

    instr->id = id;
    if (list) {
        int param_id = -1;
        if(state->uses_stack_offsets)
            list_for_each(list, param) if(param->is_expression_param) {
                if(param->value.type == 'S') {
                    param->value.val.S = param_id;
                } else if(param->value.type == 'f') {
                    param->value.val.f = param_id;
                } else if(param->type == 'D') {
                    thecl_sub_param_t* D = (thecl_sub_param_t*) param->value.val.m.data;
                    if (param->is_expression_param == 'S') {
                        D->val.S = param_id;
                    } else {
                        D->val.f = (float)param_id;
                    }
                }
                param_id--;
            }
        list_for_each(list, param) {
            ++instr->param_count;
            list_append_new(&instr->params, param);
        }
        list_free_nodes(list);
    }

    instr_set_types(state, instr);

    instr->size = state->instr_size(state->version, instr, state->is_timeline_sub);

    return instr;
}

static void
instr_add(
    thecl_sub_t* sub,
    thecl_instr_t* instr)
{
    list_append_new(&sub->instrs, instr);
    instr->offset = sub->offset;
    sub->offset += instr->size;
}

static void
instr_prepend(
    thecl_sub_t* sub,
    thecl_instr_t* instr)
{
    list_prepend_new(&sub->instrs, instr);
    instr->offset = 0;
    sub->offset += instr->size;

    thecl_instr_t* tmp_instr;
    list_for_each(&sub->instrs, tmp_instr) {
        tmp_instr->offset += instr->size;
    }

    thecl_label_t* tmp_label;
    list_for_each(&sub->labels, tmp_label) {
        tmp_label->offset += instr->size;
    }
}

static thecl_instr_t*
instr_copy(thecl_instr_t* instr) {
    thecl_instr_t* new_instr = malloc(sizeof(thecl_instr_t));
    memcpy(new_instr, instr, sizeof(thecl_instr_t));
    new_instr->string = strdup(instr->string);
    list_init(&new_instr->params);
    thecl_param_t* param;
    list_for_each(&instr->params, param) {
        thecl_param_t* new_param = param_copy(param);
        list_append_new(&new_instr->params, new_param);
    }
    return new_instr;
}

/* Returns true if the param is a system variable or a stack offset. */
static bool param_is_system_var(
    parser_state_t* state,
    thecl_param_t* param
) {
    if (param->stack) {
        if (param->value.type == 'S') {
            if (is_post_th10(state->version))
                return param->value.val.S < 0;
            return true; /* in pre-th10 there are no stack variables. */
        } else if (param->value.type == 'f') {
            if (is_post_th10(state->version))
                return param->value.val.f < 0.0f;
            return true;
        }
    }
    return false;
}

static void instr_create_inline_call(
    parser_state_t* state,
    thecl_sub_t* sub,
    list_t* params_org,
    bool needs_ret
) {
    /*   INLINE SUBS: how do they work?
     * Generally the main concept is that the sub that's inline gets parsed normally
     * (with some exceptions, like the return statement jumping to the end of the sub instead)
     * and then, when called, all insructions from the inline sub get copied into the caller,
     * with some instr parameters being replaced by the values provided as inline sub parameters,
     * stack variables being recreated, labels being adjusted etc.
     * 
     * So, how do parameters actually get passed? This part is a bit tricky, since it depends from
     * what the parameter actually is, and what the inline sub does with the parameter inside:
     * - For example, if we pass the RAND variable as a parameter, it needs to be copied into
     * a new variable, in order to keep the same value when being read multiple times.
     * - On the other hand, when passing a static value, creating a variable to store it in
     * would be a waste of time, and as such all occurences of the parameter are replaced
     * by the static value directly instead.
     * - When the parameter is an expression, it needs to be stored in a variable too,
     * since 1. evaluating the expression every time would be stupid, 2. expression does
     * not neccesairly have to result in the same value every time (thanks to variables like RAND).
     * - But, there still is another thing to keep in mind - the parameter could be written to
     * inside of the inline sub! In this case, creating a variable to store the parameter in
     * is absolutely necessary.
     * 
     * Return values are also a thing that's handled differently than in normal sub calls.
     * While they could be just saved in I3/F3 and then read from, this would be inefficient.
     * That's why they are handled in a different way - the return value simply gets pushed
     * to the stack, and then the caller can do whatever with it.
     * But what if the caller doesn't do anything with the return value? In this case, 
     * it shouldn't be pushed. Instructions responsible for creating and pushing the
     * return value have a special flag set, so we can just not copy them into the
     * caller if the caller doesn't do anything with the return value. Whether the caller
     * uses the return value or not is specified by the needs_ret parameter. */

    /* An inline sub can't call itself for obvious reasons. */
    if (strcmp(sub->name, state->current_sub->name) == 0) {
        yyerror(state, "an inline sub is not allowed to call itself");
        return;
    }
    char buf[256];

    /* A new variable is created in order to know whether the list was passed or created here later. */
    list_t* params = params_org == NULL ? list_new() : params_org;

    /* Verify parameter count and types before doing anything else. */
    size_t i = 0;
    thecl_param_t* param;
    list_for_each(params, param) {
        if (sub->format[i] == '\0') {
            yyerror(state, "too many paramters for inline sub \"%s\"", sub->name);
            list_for_each(params, param)
                param_free(param);
            return;
        }
        if (sub->format[i] != param->type) {
            yyerror(state, "wrong parameter %i when calling inline sub \"%s\", expected type: %c\n", i + 1, sub->name, sub->format[i]);
            list_for_each(params, param)
                param_free(param);
            return;
        }
        ++i;
    }
    if (sub->format[i] != '\0') {
        yyerror(state, "not enough parameters for inline sub \"%s\"", sub->name);
        list_for_each(params, param)
            param_free(param);
        return;
    }

    /* After making sure that everything is correct, we can now create the inline sub scope. */
    scope_begin(state);

    /* This string will be prepended to label names/var names etc. */
    char name[256];
    snprintf(name, 256, "%s_%d_%d_%s_", state->current_sub->name, yylloc.first_line, yylloc.first_column, sub->name);

    /* It's time to setup the param replacements.
     * As mentioned earlier, it is necessary to create vars if the param ever
     * gets written to, or the passed parameter is an expression.
     * We will use a param_replace array to replace all argument variable references from the code of copied inline sub. */
    thecl_param_t** param_replace = calloc(sub->arity, sizeof(thecl_param_t*));
    thecl_variable_t* var;
    i = 0;

    list_for_each(params, param) { /* It has alredy been verified that param amount is correct. */
        var = sub->vars[i];
        if (var->is_written || param->is_expression_param || param_is_system_var(state, param)) {

            if (param->is_expression_param && !var->is_written) {
                /* Check if the passed expression can be simplified to a literal value. */
                list_node_t* node = state->expressions.tail;
                expression_t* expr = (expression_t*)node->data;
                if (expr->type == EXPRESSION_VAL && expr->result_type == expr->value->value.type) {
                    /* Static value, otherwise it wouldn't be an uncasted expression param. */
                    param_replace[i] = param_copy(expr->value);
                    expression_free(expr);
                    list_del(&state->expressions, node);
                    ++i;
                    continue;
                }
            }

            /* Non-static param value or the param is written to, need to create var. */
            strcpy(buf, name);
            strcat(buf, var->name);
            thecl_variable_t* var = var_create(state, state->current_sub, buf, param->type);
            thecl_param_t* new_param = param_new(param->type);
            new_param->stack = 1;
            if (new_param->type == 'S')
                new_param->value.val.S = var->stack;
            else 
                new_param->value.val.f = (float)var->stack;


            if (param->is_expression_param) {
                /* The value is already on the stack, just pop it. */
                list_node_t* node = state->expressions.tail;
                expression_t* expr = (expression_t*)node->data;
                expression_output(state, expr, 1);
                expression_free(expr);
                list_del(&state->expressions, node);

                const expr_t* tmp = expr_get_by_symbol(state->version, param->type == 'S' ? ASSIGNI : ASSIGNF);
                instr_add(state->current_sub, instr_new(state, tmp->id, "p", param_copy(new_param)));
            } else {
                /* Value needs to be pushed to the stack first. */
                const expr_t* tmp = expr_get_by_symbol(state->version, param->type == 'S' ? LOADI : LOADF);
                instr_add(state->current_sub, instr_new(state, tmp->id, "p", param_copy(param)));

                tmp = expr_get_by_symbol(state->version, param->type == 'S' ? ASSIGNI : ASSIGNF);
                instr_add(state->current_sub, instr_new(state, tmp->id, "p", param_copy(new_param)));
            }
            param_replace[i] = new_param;
        } else {
            param_replace[i] = param_copy(param);
        }
        ++i;
    }

    /* Create non-param variables that the inline sub uses.. */
    thecl_variable_t** stack_replace = malloc(sizeof(thecl_variable_t*) * (sub->stack/4 - sub->arity));
    for (i = sub->arity; i < sub->stack / 4; ++i) {
        thecl_variable_t* var = sub->vars[i];
        snprintf(buf, 256, "%s%s", name, var->name);
        thecl_variable_t* var_new = var_create(state, state->current_sub, buf, var->type);
        stack_replace[i - sub->arity] = var_new;
    }

    /* Create labels that the inline sub uses (with changed offsets) */
    thecl_label_t* label;
    list_for_each(&sub->labels, label) {
        snprintf(buf, 256, "%s%s", name, label->name);
        thecl_label_t* new_label = malloc(sizeof(thecl_label_t) + strlen(buf) + 1);
        new_label->offset = label->offset + state->current_sub->offset;
        new_label->time = label->time + state->instr_time;
        strcpy(new_label->name, buf);
        list_append_new(&state->current_sub->labels, new_label);
    }

    /* And finally, copy the instructions. */

    int rank_empty = parse_rank(state, "-");
    thecl_instr_t* instr;
    list_for_each(&sub->instrs, instr) {
        /* Don't push the return value if nothing is going to pop it. */
        if (!needs_ret && (instr->flags & FLAG_RETURN_VAL))
            continue;

        thecl_instr_t* new_instr = instr_copy(instr);

        /* Set all of these to correct values. */
        new_instr->time += state->instr_time;
        new_instr->rank &= state->instr_rank;
        new_instr->flags |= state->instr_flags;

        if (new_instr->rank == rank_empty) {
            /* No reason to compile instructions that won't execute on any difficulty. */
            /* The reason why we don't call thecl_instr_free is that the param pointers are still the same
             * as in the original ins, and we do not want to free them. */
            free(new_instr);
            continue;
        }

        list_node_t* param_node;
        list_for_each_node(&new_instr->params, param_node) {
            /* Still reusing the same param variable as earlier. */
            param = (thecl_param_t*)param_node->data;
            if (param->stack) {
                if (param->type == 'D') {
                    thecl_sub_param_t* D = (thecl_sub_param_t*)param->value.val.m.data;
                    if (D->from == 'i') {
                        if (D->val.S < sub->arity*4 && D->val.S >= 0) {
                            /* Parameter. */
                            param->stack = param_replace[D->val.S / 4]->stack;
                            D->val.S = param_replace[D->val.S / 4]->value.val.S;
                        } else if (D->val.S > 0) {
                            /* Regular stack variable, needs adjusting the offset. */
                            D->val.S = stack_replace[D->val.S / 4 - sub->arity]->stack;
                        }
                    } else {
                        if (D->val.f < (float)(sub->arity*4 )&& D->val.f >= 0.0f) {
                            param->stack = param_replace[(int)D->val.f / 4]->stack;
                            D->val.f = param_replace[(int)D->val.f / 4]->value.val.f;
                        } else if (D->val.f > 0.0f) {
                            D->val.f = (float)stack_replace[(int)D->val.f / 4 - sub->arity]->stack;
                        }
                    }
                } else if (param->value.type == 'S') {
                    if (param->value.val.S < sub->arity*4 && param->value.val.S >= 0) {
                        /* Parameter. */
                        param_node->data = param_copy(param_replace[param->value.val.S / 4]);
                        param_free(param);
                    } else if (param->value.val.S > 0) {
                        /* Regular stack variable, needs adjusting the offset. */
                        param->value.val.S = stack_replace[param->value.val.S / 4 - sub->arity]->stack;
                    }
                } else if (param->value.type == 'f') {
                    if (param->value.val.f < (float)(sub->arity*4) && param->value.val.f >= 0.0f) {
                        param_node->data = param_copy(param_replace[(int)param->value.val.f / 4]);
                        param_free(param);
                    } else if (param->value.val.f > 0.0f) {
                        param->value.val.f = (float)stack_replace[(int)param->value.val.f / 4 - sub->arity]->stack;
                    }
                }
            } else if (param->type == 'o') {
                /* We also have to make sure that all jumps are correct. */
                snprintf(buf, 256, "%s%s", name, param->value.val.z);
                free(param->value.val.z);
                param->value.val.z = strdup(buf);
            } else if (param->type == 't') {
                /* Can be either S or z, depending on how the jump was created... */
                if (param->value.type == 'S')
                    param->value.val.S += state->instr_time;
                else {
                    snprintf(buf, 256, "%s%s", name, param->value.val.z);
                    free(param->value.val.z);
                    param->value.val.z = strdup(buf);
                }
            }
        }
        instr_add(state->current_sub, new_instr);
    }

    /* Time of the current sub has to be adjusted. */
    if (sub->time != 0)
        set_time(state, state->instr_time + sub->time);

    scope_finish(state);

    /* We have to mark variables that were marked as unused in the inline sub
     * as unused in the current sub as well. */
    for (size_t v=sub->arity; v<sub->var_count; ++v) {
        stack_replace[v - sub->arity]->is_unused = sub->vars[v]->is_unused;
    }

    /* Free stuff. */
    /* Still the same variables used here */
    i = 0;
    list_for_each(params, param) {
        param_free(param);
        param_free(param_replace[i++]);
    }
    /* Only free this list if it was created here.
     * It's empty, so no need to free nodes. */
    if (params_org == NULL)
        free(params);
    free(param_replace);
    free(stack_replace);
}

static bool
instr_create_call(
    parser_state_t *state,
    int type,
    char *name,
    list_t *params,
    bool needs_ret)
{
    /* First, check if the called sub is inline. */
    thecl_sub_t* sub;
    list_for_each(&state->ecl->subs, sub) {
        if (sub->is_inline && strcmp(sub->name, name) == 0) {
            if (type != TH10_INS_CALL) {
                yyerror(state, "Inline subs can't be called as async!");
            }
            instr_create_inline_call(state, sub, params, needs_ret);
            free(name);
            return true;
        }
    }

    /* Create new arg list */
    list_t *param_list = list_new();

    /* Instr name */
    thecl_param_t *param = param_new('z');
    param->value.type = 'z';
    param->value.val.z = name;
    list_append_new(param_list, param);

    int expr_params = 0;

    /* Add parameter casts */
    if (params != NULL) {
        expression_t* current_expr = NULL;

        list_node_t* node_expr = state->expressions.tail; 
        thecl_param_t *iter_param;
        list_node_t* iter_node;
        list_for_each_node(params, iter_node) {
            iter_param = (thecl_param_t*)iter_node->data;
            if (!iter_node->prev && type == TH10_INS_CALL_ASYNC_ID) {
                /* CALL_ASYNC_ID takes one more param for the slot ID at the beginning. */
                param = malloc(sizeof(thecl_param_t));
                memcpy(param, iter_param, sizeof(thecl_param_t));
                list_append_new(param_list, param);
                if (param->is_expression_param) {
                    ++expr_params;
                }
                continue;
            }

            param = param_new('D');
            param->stack = iter_param->stack;
            param->is_expression_param = iter_param->is_expression_param;
            param->value.type = 'm';
            param->value.val.m.length = sizeof(thecl_sub_param_t);
            param->value.val.m.data = malloc(sizeof(thecl_sub_param_t));
            thecl_sub_param_t* D = (thecl_sub_param_t*)param->value.val.m.data;
            D->zero = 0;
            bool is_load_expression = false;
            bool is_load_var = false;
            if (param->is_expression_param) {
                ++expr_params;
                
                current_expr = (expression_t*)node_expr->data;
                list_node_t* last_node = node_expr;
                node_expr = node_expr->prev;

                if (current_expr->type == EXPRESSION_VAL) {
                    const expr_t* expr = expr_get_by_id(state->version, current_expr->id);
                    is_load_expression = (expr->symbol == LOADI || expr->symbol == LOADF);
                    if (is_load_expression) {
                        is_load_var = current_expr->value->stack;
                        param->stack = is_load_var;
                        param->is_expression_param = 0;
                        list_del(&state->expressions, last_node);
                        expr_params--;
                    }
                }
            }

            if (iter_param->value.type == 'S' || iter_param->value.type == 'f') {
                D->to = iter_param->value.type == 'S' ? 'i' : 'f';
                if (is_load_expression) {
                    D->from = current_expr->value->type == 'S' ? 'i' : 'f';
                    if (D->from == 'S')
                        D->val.S = current_expr->value->value.val.S;
                    else
                        D->val.f = current_expr->value->value.val.f;
                } else {
                    D->from = D->to;
                    if (D->from == 'S')
                        D->val.S = iter_param->value.val.S;
                    else
                        D->val.f = iter_param->value.val.f;
                }
            } else {
                yyerror(state, "invalid sub parameter type '%c': only float/int are acceptable.", iter_param->value.type);
                g_was_error = true;
            }

            if (is_load_expression)
                expression_free(current_expr);

            param_free(iter_param);
            list_append_new(param_list, param);
        }
    }

    /* Output expressions from parameters.
     * DO NOT output all expressions, only the ones used. */
    expression_t* expr;
    list_node_t* node = state->expressions.head;
    while(expr_params--) {
        expr = (expression_t*)node->data;
        expression_output(state, expr, 0);
        expression_free(expr);

        list_node_t* node_prev = node;
        node = node->next;
        free(node_prev);
    }
    state->expressions.head = node;
    if (node == NULL) /* The list is empty. */
        state->expressions.tail = NULL;
    else
        node->prev = NULL;

    instr_add(state->current_sub, instr_new_list(state, type, param_list));
    return false;
}

static bool
check_rank_flag(
    const parser_state_t* state,
    const char* value,
    char flag)
{
    int count = 0;
    for (int i=0; value[i]; i++) if(value[i] == flag) count++;

    if (count == 0) return false;
    else if(count == 1) return true;
    else {
        yyerror(state, "check_rank_flag: in sub %s: duplicate rank flag %c in '%s'",
                 state->current_sub->name, flag, value);
        return true;
    }
}

static int
parse_rank(
    const parser_state_t* state,
    const char* value)
{
    int rank = state->has_overdrive_difficulty ? 0xC0 : 0xF0;

    if (check_rank_flag(state, value, '*')) {
        if (strlen(value) != 1) {
            yyerror(state,
                     "parse_rank: in sub %s: * should not be used with "
                     "other rank flags.",
                     state->current_sub->name);
        }
        return 0xFF;
    } else if (check_rank_flag(state, value, '-')) {
        if (strlen(value) != 1) {
            yyerror(state,
                     "parse_rank: in sub %s: - should not be used with "
                     "other rank flags.",
                     state->current_sub->name);
        }
        return rank;
    } else {
        if (check_rank_flag(state, value, 'E')) rank |= RANK_EASY;
        if (check_rank_flag(state, value, 'N')) rank |= RANK_NORMAL;
        if (check_rank_flag(state, value, 'H')) rank |= RANK_HARD;
        if (check_rank_flag(state, value, 'L')) rank |= RANK_LUNATIC;

        if (state->has_overdrive_difficulty) {
          if (check_rank_flag(state, value, 'X')) rank |= RANK_EXTRA;
          if (check_rank_flag(state, value, 'O')) rank |= RANK_OVERDRIVE;
        } else {
          if (check_rank_flag(state, value, '4')) rank &= ~RANK_ID_4;
          if (check_rank_flag(state, value, '5')) rank &= ~RANK_ID_5;
        }

        if (check_rank_flag(state, value, '6')) rank &= ~RANK_ID_6;
        if (check_rank_flag(state, value, '7')) rank &= ~RANK_ID_7;

        if (state->has_overdrive_difficulty && (check_rank_flag(state, value, '4') || check_rank_flag(state, value, '5'))) {
            yyerror(state,
                    "parse_rank: in sub %s: Rank flags 4 and 5 are not used in "
                    "TH13+. Use X for extra, and O for overdrive instead.",
                    state->current_sub->name);
        }
        if (!state->has_overdrive_difficulty && (check_rank_flag(state, value, 'X') || check_rank_flag(state, value, 'O'))) {
            yyerror(state,
                    "parse_rank: in sub %s: Rank flags X and O do not exist "
                    "before TH13. Use 4 and 5 for the unused difficulties flags "
                    "instead.",
                    state->current_sub->name);
        }
        if (check_rank_flag(state, value, 'W') || check_rank_flag(state, value, 'Y') || check_rank_flag(state, value, 'Z')) {
          yyerror(state,
                   "parse_rank: in sub %s: Rank flags W, X, Y and Z no "
                   "longer refer to unused difficulties 4-7. %s",
                   state->current_sub->name,
                   state->has_overdrive_difficulty
                       ? "In TH13+, use 6 and 7 for the remaining two unused "
                         "difficulties, X for extra, and O for overdrive."
                       : "Before TH13, use 4, 5, 6, and 7 to refer to the "
                         "unused difficulties.");
        }

        return rank;
    }
}

static expression_t*
expression_load_new(
    const parser_state_t* state,
    thecl_param_t* value)
{
    expression_t* ret = malloc(sizeof(expression_t));
    const expr_t* expr = expr_get_by_symbol(state->version, value->type == 'S' ? LOADI : LOADF);
    ret->type = EXPRESSION_VAL;
    ret->id = expr->id;
    ret->value = value;
    ret->result_type = value->type;
    return ret;
}

static expression_t*
expression_address_operation_new(
    const parser_state_t* state,
    const int* symbols,
    thecl_param_t* value)
{
    for (; *symbols; ++symbols) {
        const expr_t* expr = expr_get_by_symbol(state->version, *symbols);

        if (value->type != expr->param_format[0])
            continue;

        expression_t* ret = malloc(sizeof(expression_t));
        ret->type = EXPRESSION_VAL;
        ret->id = expr->id;
        ret->value = value;
        ret->result_type = expr->return_type;

        return ret;
    }

    return NULL;
}

static expression_t*
expression_operation_new(
    const parser_state_t* state,
    const int* symbols,
    expression_t** operands)
{
    for (; *symbols; ++symbols) {
        const expr_t* expr = expr_get_by_symbol(state->version, *symbols);

        for (size_t s = 0; s < expr->stack_arity; ++s)
            if (operands[s]->result_type != expr->stack_formats[s])
                goto continue_outer;

        expression_t* ret = malloc(sizeof(expression_t));
        ret->type = EXPRESSION_OP;
        ret->id = expr->id;
        ret->value = NULL;
        list_init(&ret->children);
        for (size_t o = 0; o < expr->stack_arity; ++o) {
            list_append_new(&ret->children, operands[o]);
        }
        ret->result_type = expr->return_type;

        if (!g_ecl_simplecreate)
            expression_optimize(state, ret);
        return ret;

        continue_outer: ;
    }

    /* Create error */
    char errbuf[512];
    errbuf[0] = 0;
    const expr_t *expr = expr_get_by_symbol(state->version, *symbols);
    if (expr) {
      snprintf(errbuf, 511, "%s: ", expr->display_format);
    }
    strncat(errbuf, "no expression found for type(s): ", 511 - strlen(errbuf));
    for (size_t s = 0; operands[s]; ++s) {
      if (s != 0)
        strncat(errbuf, " and ", 511 - strlen(errbuf));
      strncat(errbuf, (char *)&operands[s]->result_type, 1);
    }
    yyerror((parser_state_t *)state, errbuf);

    /* We cannot continue after this; the program would crash */
    exit(2);
}

static expression_t* 
expression_rank_switch_new(
    const parser_state_t* state, list_t* exprs
) {
    if (is_post_th10(state->version)) {
        expression_t* expr_main = malloc(sizeof(expression_t));
        expr_main->type = EXPRESSION_RANK_SWITCH;

        expression_t* expr = list_head(exprs);
        expr_main->result_type = expr->result_type;

        list_for_each(exprs, expr) {
            if (expr->result_type != expr_main->result_type) {
                yyerror(state, "inconsistent parameter types for difficulty switch");
                exit(2);
            }
        }

        expr_main->children = *exprs;
        free(exprs);
        return expr_main;
    }
    yyerror(state, "difficulty switch expression is not available in pre-th10 ECL");

    exit(2);
}

static expression_t*
expression_ternary_new(
    const parser_state_t* state,
    expression_t* cond,
    expression_t* val1,
    expression_t* val2
 ) {
    if (is_post_th10(state->version)) {
        if (val1->result_type != val2->result_type)
            yyerror(state, "inconsistent types for ternary operator");
        expression_t* expr = malloc(sizeof(expression_t));
        expr->type = EXPRESSION_TERNARY;
        expr->result_type = val1->result_type;
        list_init(&expr->children);
        list_append_new(&expr->children, cond);
        list_append_new(&expr->children, val1);
        list_append_new(&expr->children, val2);
        return expr;
    }
    yyerror(state, "ternary operator is not available in pre-th10 ECL");

    exit(2);
 }

static expression_t*
expression_call_new(
    const parser_state_t* state,
    list_t* param_list,
    char* sub_name
) {
    expression_t* expr = malloc(sizeof(expression_t));
    expr->type = EXPRESSION_CALL;
    expr->name = sub_name;
    if (param_list != NULL) {
        expr->params = *param_list;
        free(param_list);
    } else {
        list_init(&expr->params);
    }

    /* This requires called sub to already be on the list at this point.
     * Otherwise, getting expressions and everything else to work correctly
     * would require a lot of weird workarounds with how thecl works now. 
     * The core issue is that all sub declarations aren't being parsed and stored 
     * before the actual code of the subs is parsed, and this is what should be changed 
     * in the future. */
    thecl_sub_t* iter_sub;
    int ret_type = -1;
    list_for_each(&state->ecl->subs, iter_sub) {
        if (strcmp(iter_sub->name, sub_name) == 0) {
            ret_type = iter_sub->ret_type;
            break;
        }
    }

    char buf[256];
    if (ret_type == -1) {
        yyerror(state, "sub must be declared before being used in an expression: %s", sub_name);
        ret_type = 'S'; /* Default to something to continue parsing despite the error */
    } else if (ret_type == 0) {
        yyerror(state, "sub used in expression can not have void return type: %s", sub_name);
        ret_type = 'S';
    } else if (ret_type == '?') {
        yyerror(state, "sub used in expression can not have var return type: %s", sub_name);
        ret_type = 'S';
    }
    expr->result_type = ret_type;

    const expr_t* tmp_expr = expr_get_by_symbol(state->version, ret_type == 'S' ? LOADI : LOADF);
    expr->id = tmp_expr->id;

    thecl_param_t* param = param_new(ret_type);
    param->stack = 1;
    if (ret_type == 'S')
        param->value.val.S = TH10_VAR_I3;
    else
        param->value.val.f = TH10_VAR_F3;

    expr->value = param;

    return expr;
}


static expression_t *
expression_copy(
    expression_t *expr)
{
    expression_t *copy = malloc(sizeof(expression_t));
    memcpy(copy, expr, sizeof(expression_t));
    expression_t* child_expr;
    list_init(&copy->children);
    if (expr->type == EXPRESSION_OP || expr->type == EXPRESSION_RANK_SWITCH || expr->type == EXPRESSION_TERNARY) {
        list_for_each(&expr->children, child_expr)
            list_append_new(&copy->children, expression_copy(child_expr));
    } else if (expr->type == EXPRESSION_VAL || expr->type == EXPRESSION_CALL) {
        thecl_param_t *param = malloc(sizeof(thecl_param_t));
        memcpy(param, expr->value, sizeof(thecl_param_t));
        copy->value = param;
        if (expr->type == EXPRESSION_CALL) {
            copy->name = malloc(strlen(expr->name + 1));
            strcpy(copy->name, expr->name);
            thecl_param_t* child_param;
            list_for_each(&expr->params, child_param) {
                thecl_param_t* param_copy = malloc(sizeof(thecl_param_t));
                memcpy(param_copy, child_param, sizeof(thecl_param_t));
                list_append_new(&copy->params, param_copy);
            }
        }
    }
    return copy;
}

static void
expression_create_goto(
    parser_state_t *state,
    int type,
    char *labelstr)
{
    const expr_t* expr = expr_get_by_symbol(state->version, type);
    thecl_param_t *p1 = param_new('o');
    thecl_param_t *p2 = param_new('t');
    p1->value.type = 'z';
    p1->value.val.z = strdup(labelstr);
    p2->value.type = 'z';
    p2->value.val.z = strdup(labelstr);
    instr_add(state->current_sub, instr_new(state, expr->id, "pp", p1, p2));
}

static void
expression_output(
    parser_state_t* state,
    expression_t* expr,
    int has_no_parents)
{
    if (!g_ecl_simplecreate && has_no_parents)
        /* Since expression_optimize is already done recursively for children, it shouldn't be called for child expressions. */
        expression_optimize(state, expr);

    if (expr->type == EXPRESSION_VAL) {
        instr_add(state->current_sub, instr_new(state, expr->id, "p", expr->value));
    } else if (expr->type == EXPRESSION_OP) {
        expression_t* child_expr;
        list_for_each(&expr->children, child_expr) {
            expression_output(state, child_expr, 0);
        }

        instr_add(state->current_sub, instr_new(state, expr->id, ""));
    } else if (expr->type == EXPRESSION_RANK_SWITCH) {

        const int diff_amt = state->has_overdrive_difficulty ? 5 : 4;
        const char* diffs[5] = {"E", "N", "H", "L", "O"};

        int diff = 0;
        int rank_none = parse_rank(state, "-");
        int rank_org = state->instr_rank;

        expression_t* iter_expr;
        expression_t* last_expr = NULL;
        list_for_each(&expr->children, iter_expr) {
            if (last_expr != NULL) {
                state->instr_rank = rank_org & parse_rank(state, diffs[diff++]);
                if (state->instr_rank != rank_none) expression_output(state, last_expr, 1);
            }

            if (diff > 4) {
                yyerror(state, "too many parameters for difficulty switch");
                exit(2);
            }

            last_expr = iter_expr;
        }

        /* Set last expr to all remaining difficulties. */
        char diff_str[5] = "";
        while(diff < diff_amt) {
            const char* next_diff = diffs[diff++];
            strcat(diff_str, next_diff);
        }

        state->instr_rank = rank_org & parse_rank(state, diff_str);
        if (state->instr_rank != rank_none) expression_output(state, last_expr, 1);

        state->instr_rank = rank_org;
    } else if (expr->type == EXPRESSION_TERNARY) {
        char labelstr_unless[256];
        char labelstr_end[256];

        snprintf(labelstr_unless, 256, "ternary_unless_%d_%d", yylloc.first_line, yylloc.first_column);
        snprintf(labelstr_end, 256, "ternary_end_%d_%d", yylloc.first_line, yylloc.first_column);

        int i = 0;
        expression_t* child_expr;
        list_for_each(&expr->children, child_expr) {
            expression_output(state, child_expr, 1);
            if (i == 0) {
                expression_create_goto(state, UNLESS, labelstr_unless);
            } else if (i == 1) {
                expression_create_goto(state, GOTO, labelstr_end);
                label_create(state, labelstr_unless);
            } else {
                label_create(state, labelstr_end);
            }
            ++i;
        }
    } else if (expr->type == EXPRESSION_CALL) {
        /* Inline calls don't use return registers, so only push I3/F3 if the call was not inline. */
        if (!instr_create_call(state, TH10_INS_CALL, expr->name, &expr->params, true))
            instr_add(state->current_sub, instr_new(state, expr->id, "p", expr->value));
    }
}

static void
expression_optimize(
    parser_state_t* state,
    expression_t* expression)
{
    if (expression->type != EXPRESSION_OP) return;

    int child_cnt = 0;
    expression_t* child_expr_1;
    expression_t* child_expr_2;
    expression_t* child_expr;
    list_for_each(&expression->children, child_expr) {
        if (child_expr->type == EXPRESSION_OP) {
            expression_optimize(state, child_expr);
        }

        if (child_cnt == 0) {
            child_expr_1 = child_expr;
        } else if (child_cnt == 1) {
            child_expr_2 = child_expr;
        }
        ++child_cnt;
    }

    /* TODO: handle some single-child expressions, such as sin or cos */
    if (child_cnt != 2) return;

    if (
           child_expr_1->type != EXPRESSION_VAL
        || child_expr_2->type != EXPRESSION_VAL
        || child_expr_1->value->stack /* Variables are not acceptable, obviously. */
        || child_expr_2->value->stack
    ) return;

    const expr_t* tmp_expr = expr_get_by_id(state->version, expression->id);
    
    /* Need to get the type from tmp_expr->return_type, since expression->result_type could have been modified by typecasts. */
    thecl_param_t* param = param_new(tmp_expr->return_type);

    int res1 = child_expr_1->value->type;
    int res2 = child_expr_2->value->type;
    
    int val1S = res1 == 'S' ? child_expr_1->value->value.val.S : (int)(child_expr_1->value->value.val.f);
    float val1f = res1 == 'f' ? child_expr_1->value->value.val.f : (float)(child_expr_1->value->value.val.S);
    int val2S = res2 == 'S' ? child_expr_2->value->value.val.S : (int)(child_expr_2->value->value.val.f);
    float val2f = res2 == 'f' ? child_expr_2->value->value.val.f : (float)(child_expr_2->value->value.val.S);

    switch(tmp_expr->symbol) {
        case ADDI:
            param->value.val.S = val1S + val2S;
        break;
        case ADDF:
            param->value.val.f = val1f + val2f;
        break;
        case SUBTRACTI:
            param->value.val.S = val1S - val2S;
        break;
        case SUBTRACTF:
            param->value.val.f = val1f - val2f;
        break;
        case MULTIPLYI:
            param->value.val.S = val1S * val2S;
        break;
        case MULTIPLYF:
            param->value.val.f = val1f * val2f;
        break;
        case DIVIDEI:
            param->value.val.S = val1S / val2S;
        break;
        case DIVIDEF:
            param->value.val.f = val1f / val2f;
        break;
        case MODULO:
            param->value.val.S = val1S % val2S;
        break;
        case EQUALI:
            param->value.val.S = val1S == val2S;
        break;
        case EQUALF:
            param->value.val.S = val1f == val2f;
        break;
        case INEQUALI:
            param->value.val.S = val1S != val2S;
        break;
        case INEQUALF:
            param->value.val.S = val1f != val2f;
        break;
        case LTI:
            param->value.val.S = val1S < val2S;
        break;
        case LTF:
            param->value.val.S = val1f < val2f;
        break;
        case LTEQI:
            param->value.val.S = val1S <= val2S;
        break;
        case LTEQF:
            param->value.val.S = val1f <= val2f;
        break;
        case GTI:
            param->value.val.S = val1S > val2S;
        break;
        case GTF:
            param->value.val.S = val1f > val2f;
        break;
        case GTEQI:
            param->value.val.S = val1S >= val2S;
        break;
        case GTEQF:
            param->value.val.S = val1f >= val2f;
        break;
        case OR:
            param->value.val.S = val1S || val2S;
        break;
        case AND:
            param->value.val.S = val1S && val2S;
        break;
        case XOR:
            param->value.val.S = val1S ^ val2S;
        break;
        case B_OR:
            param->value.val.S = val1S | val2S;
        break;
        case B_AND:
            param->value.val.S = val1S & val2S;
        break;
        default:
            /* Since the cases above cover all existing 2-parameter expressions there is no possibility of this ever hapenning.
               Just putting this error message in case someone adds new expressions and forgets about handling them here... */
            yyerror(state, "Math preprocessing error! Try using simple creation mode.");
    }

    expression->value = param;
    expression->type = EXPRESSION_VAL;
    tmp_expr = expr_get_by_symbol(state->version, param->type == 'S' ? LOADI : LOADF);
    expression->id = tmp_expr->id;

    param_free(child_expr_1->value);
    param_free(child_expr_2->value);
    expression_free(child_expr_1);
    expression_free(child_expr_2);
    list_free_nodes(&expression->children);
}

static void
expression_free(
    expression_t* expr)
{
    expression_t* child_expr;
    if (expr->type == EXPRESSION_OP || expr->type == EXPRESSION_RANK_SWITCH || expr->type == EXPRESSION_TERNARY) {
        list_for_each(&expr->children, child_expr)
            expression_free(child_expr);
        list_free_nodes(&expr->children);
    }
    free(expr);
}

static void
sub_begin(
    parser_state_t* state,
    char* name,
    int is_timeline,
    int ret_type)
{
    scope_begin(state);

    state->is_timeline_sub = is_timeline;

    thecl_sub_t* sub = malloc(sizeof(thecl_sub_t));

    sub->name = strdup(name);
    sub->ret_type = ret_type;
    list_init(&sub->instrs);
    sub->stack = 0;
    sub->arity = 0;
    sub->var_count = 0;
    sub->vars = NULL;
    sub->offset = 0;
    sub->time = 0;
    list_init(&sub->labels);

    if (!state->uses_numbered_subs) {
        if (is_timeline) {
            yyerror(state, "timelines don't exist in th10+");
        }

        /* Touhou expects the list of subs to be sorted by name. */
        thecl_sub_t* iter_sub;
        list_for_each(&state->ecl->subs, iter_sub) {
            int diff = strcmp(name, iter_sub->name);
            if(diff == 0 && !iter_sub->forward_declaration) {
                yyerror(state, "duplicate sub: %s", name);
                g_was_error = true;
                break;
            } else if(diff < 0) {
                list_prepend_to(&state->ecl->subs, sub, node);
                goto no_append;
            }
        }
    }
    if (is_timeline)
        list_append_new(&state->ecl->timelines, sub);
    else
        list_append_new(&state->ecl->subs, sub);

no_append:

    state->instr_time = 0;
    state->instr_rank = 0xff;
    state->current_sub = sub;
}

static void
sub_finish(
    parser_state_t* state)
{
    if (is_post_th10(state->ecl->version) && !g_ecl_simplecreate && !state->current_sub->is_inline) {
        
        thecl_instr_t* var_ins = instr_new(state, TH10_INS_STACK_ALLOC, "S", state->current_sub->stack);
        var_ins->time = 0;
        var_ins->rank = parse_rank(state, "*");
        instr_prepend(state->current_sub, var_ins);

        thecl_instr_t* last_ins = list_tail(&state->current_sub->instrs);
        if (last_ins == NULL || last_ins->id != TH10_INS_RET_NORMAL && last_ins->id != TH10_INS_RET_BIG) {
            thecl_instr_t* ret_ins = instr_new(state, TH10_INS_RET_NORMAL, "");
            ret_ins->rank = parse_rank(state, "*");
            instr_add(state->current_sub, ret_ins);
        }
    } else if (state->current_sub->is_inline) {
        thecl_instr_t* last_ins = list_tail(&state->current_sub->instrs);
        const expr_t* tmp = expr_get_by_symbol(state->version, GOTO);
        if (last_ins != NULL && last_ins->id == tmp->id) {
            thecl_param_t* label_param = list_head(&last_ins->params);
            if (strcmp(label_param->value.val.z, "inline_end") == 0) {
                /* Remove useless goto. */
                list_del(&state->current_sub->instrs, state->current_sub->instrs.tail);
                state->current_sub->offset -= last_ins->size;
                thecl_instr_free(last_ins);
            }
        }
        label_create(state, "inline_end");
    }

    if (!state->is_timeline_sub && !state->current_sub->forward_declaration && !state->current_sub->is_inline)
        ++state->ecl->sub_count;
    state->current_sub = NULL;

    scope_finish(state);
}

static void
scope_begin(
    parser_state_t* state
) {
    ++state->scope_cnt;
    state->scope_stack = realloc(state->scope_stack, sizeof(int)*state->scope_cnt);
    state->scope_stack[state->scope_cnt - 1] = state->scope_id++;
}

static void
scope_finish(
    parser_state_t* state
) {
    --state->scope_cnt;
    state->scope_stack = realloc(state->scope_stack, sizeof(int)*state->scope_cnt);
}

static global_definition_t*
global_get(
    parser_state_t* state,
    const char* name
) {
    global_definition_t *def;
    list_for_each(&state->global_definitions, def) {
        if (strcmp(def->name, name) == 0)
            return def;
    }
    return NULL;
}

static bool
var_stack_used(
    parser_state_t* state,
    thecl_sub_t* sub,
    int stack
) {
    for (size_t v=0; v<sub->var_count; ++v) {
        if (sub->vars[v]->stack == stack && !sub->vars[v]->is_unused)
            return true;
    }
    return false;
}

static int
var_get_new_stack(
    parser_state_t* state,
    thecl_sub_t* sub
) {
    int stack = 0;
    while(1) {
        if (!var_stack_used(state, sub, stack))
            return stack;
        stack += 4;
    }
}

static thecl_variable_t*
var_create(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name,
    int type)
{
    if (!is_post_th10(state->version)) {
        yyerror(state, "stack variable declaration is not allowed in version: %i", state->version);
        exit(2);
    }
    if (g_ecl_simplecreate && type != 0) {
        yyerror(state, "only typeless variables are allowed in simple creation mode: %s", name);
        exit(2);
    }
    if (var_exists(state, sub, name)) {
        yyerror(state, "redeclaration of variable: %s", name);
    }
    if (type == 0) {
        yyerror(state, "variables can't be declared as 'void', use 'var' to declare typeless vars.");
        type = '?';
    }
    if (global_get(state, name) != NULL) {
        yyerror(state, "identifier %s is already used as a global definition name", name);
    }

    thecl_variable_t* var = malloc(sizeof(thecl_variable_t));
    var->name = strdup(name);
    var->type = type;
    var->stack = var_get_new_stack(state, sub);
    var->is_written = false;
    var->is_unused = false;
    var->scope = state->scope_stack[state->scope_cnt - 1];

    ++sub->var_count;
    sub->vars = realloc(sub->vars, sub->var_count * sizeof(thecl_variable_t*));
    sub->vars[sub->var_count - 1] = var;

    if (var->stack == sub->stack) /* Only increment the stack if the variable uses aa new offset. */
        sub->stack += 4;
    return var;
}

static thecl_variable_t*
var_create_assign(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name,
    int type,
    expression_t* expr)
{
    if (g_ecl_simplecreate) {
        yyerror(state, "var creation with assignment is not allowed in simple creation mode");
        exit(2);
    }
    if (type == '?') {
        yyerror(state, "var creation with assignment requires int/float declaration keyword");
        exit(2);
    }
    thecl_variable_t* var = var_create(state, sub, name, type);
    var->is_written = true;

    thecl_param_t* param = param_new(type);
    param->value.type = type;
    if (type == 'S') {
        param->value.val.S = var->stack;
    } else {
        param->value.val.f = (float)var->stack;
    }
    param->stack = 1;

    expression_output(state, expr, 1);
    expression_free(expr);

    const expr_t* expr_assign = expr_get_by_symbol(state->version, type == 'S' ? ASSIGNI : ASSIGNF);
    instr_add(state->current_sub, instr_new(state, expr_assign->id, "p", param));

    return var;
}

static bool
var_accessible(
    parser_state_t* state,
    thecl_variable_t* var
) {
    for (int scope_state=0; scope_state<state->scope_cnt; ++scope_state) {
        if (state->scope_stack[scope_state] == var->scope)
            return true;
    }
    return false;
}

static thecl_variable_t*
var_get(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name
) {
    for (size_t i = 0; i < sub->var_count; ++i) {
        if (strcmp(name, sub->vars[i]->name) == 0 && var_accessible(state, sub->vars[i]))
            return sub->vars[i];
    }
    return NULL;
}

static int
var_stack(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name)
{
    seqmap_entry_t* ent = seqmap_find(g_eclmap->gvar_names, name);
    if (ent) return ent->key;

    thecl_variable_t* var = var_get(state, sub, name);
    if (var != NULL)
        return var->stack;

    yyerror(state, "variable not found: %s", name);
    return 0;
}

static int
var_type(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name)
{
    seqmap_entry_t* ent = seqmap_find(g_eclmap->gvar_names, name);
    if (ent) {
        ent = seqmap_get(g_eclmap->gvar_types, ent->key);
        if (ent)
            return ent->value[0] == '$' ? 'S' : 'f';
    }
    
    thecl_variable_t* var = var_get(state, sub, name);
    if (var != NULL)
        return var->type;

    yyerror(state, "variable not found: %s", name);
    return 0;
}

static int
var_exists(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name)
{
    seqmap_entry_t* ent = seqmap_find(g_eclmap->gvar_names, name);
    if (ent) return 1;

    if (sub == NULL) return 0; /* we are outside of sub scope, no point in searching for variables */

    return var_get(state, sub, name) != NULL;
}

static void
var_shorthand_assign(
    parser_state_t* state,
    thecl_param_t* param,
    expression_t* expr_assign,
    int EXPRI,
    int EXPRF)
{
    /* Can't use the same param twice, so a copy is created. */
    thecl_param_t* param_clone = malloc(sizeof(thecl_param_t));
    memcpy(param_clone, param, sizeof(thecl_param_t));

    expression_t* expr_load = expression_load_new(state, param_clone);
    expression_t* expr_main = EXPR_22(EXPRI, EXPRF, expr_load, expr_assign);
    expression_output(state, expr_main, 1);
    expression_free(expr_main);
    /* No need to free expr_load or expr, since they both got freed as children of expr_main. */

    const expr_t* expr = expr_get_by_symbol(state->version, param->type == 'S' ? ASSIGNI : ASSIGNF);
    instr_add(state->current_sub, instr_new(state, expr->id, "p", param));

    thecl_variable_t* var = NULL;
    if (param->value.type == 'S') {
        if (param->value.val.S >= 0) var = state->current_sub->vars[param->value.val.S / 4];
    } else {
        if (param->value.val.f >= 0.0f) var = state->current_sub->vars[(int)param->value.val.f / 4];
    }
    if (var != NULL) var->is_written = true;
}

static void
label_create(
    parser_state_t* state,
    char* name)
{
    thecl_label_t* label = malloc(sizeof(thecl_label_t) + strlen(name) + 1);
    list_prepend_new(&state->current_sub->labels, label);
    label->offset = state->current_sub->offset;
    label->time = state->instr_time;
    strcpy(label->name, name);
}

void
set_time(
    parser_state_t* state,
    int new_time)
{
    if (new_time == state->instr_time || (state->instr_time > 0 && new_time < state->instr_time)) {
        yyerror(state, "illegal timer change: %d to %d", state->instr_time, new_time);
    }
    state->current_sub->time = new_time;
    state->instr_time = new_time;
}

static int
directive_include(
    parser_state_t* state,
    char* include_path)
{
    char* path = path_get_full(state, include_path);
    FILE* include_file = fopen(path, "rb");

    if (include_file != NULL) {
                
        FILE* in_org = yyin;
        YYLTYPE loc_org = yylloc;
        const char* input_org = current_input;

        current_input = include_path;
        yyin = include_file;
        yylloc.first_line = 1;
        yylloc.first_column = 1;
        yylloc.last_line = 1;
        yylloc.last_column = 1;

        path_add(state, path);

        int err = yyparse(state);

        fclose(include_file);
        path_remove(state);

        if (err) {
            free(path);
            return 1;
        }

        yyin = in_org;
        yylloc = loc_org;
        current_input = input_org;
    } else {
        yyerror(state, "#include error: couldn't open %s for reading", path);
        return 1;
    }
    free(path);
    return 0;
}

static void
directive_eclmap(
parser_state_t* state,
char* name) 
{
    char* path = path_get_full(state, name);
    FILE* map_file = fopen(path, "r");
    if (map_file == NULL) {
        yyerror(state, "#eclmap error: couldn't open %s for reading", path);
    } else {
        eclmap_load(state->version, g_eclmap, map_file, path);
        eclmap_rebuild(g_eclmap);
        fclose(map_file);
    }
    free(path);
}

void
yyerror(
    const parser_state_t* state,
    const char* format,
    ...)
{
    /* TODO: Research standard row and column range formats. */
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
