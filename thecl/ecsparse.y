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

static thecl_instr_t* instr_new(parser_state_t* state, unsigned int id, const char* format, ...);
static thecl_instr_t* instr_new_list(parser_state_t* state, unsigned int id, list_t* list);
static void instr_add(thecl_sub_t* sub, thecl_instr_t* instr);
static void instr_prepend(thecl_sub_t* sub, thecl_instr_t* instr);
static void instr_create_call(parser_state_t *state, int type, char *name, list_t *params);

enum expression_type {
    EXPRESSION_OP,
    EXPRESSION_VAL,
    EXPRESSION_RANK_SWITCH,
    EXPRESSION_TERNARY
};

typedef struct expression_t {
    /* Operator or value. */
    enum expression_type type;
    int id;
    /* For values: The value. */
    thecl_param_t* value;
    /* For operators: The child expressions. */
    /* This list is also used to store values for difficulty switches. */
    list_t children;
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

static void expression_output(parser_state_t* state, expression_t* expr, int has_no_parents);
static void expression_free(expression_t* expr);
static void expression_optimize(expression_t* expr);
#define EXPR_22(a, b, A, B) \
    expression_operation_new(state, (int[]){ a, b, 0 }, (expression_t*[]){ A, B, NULL })
#define EXPR_12(a, A, B) \
    expression_operation_new(state, (int[]){ a, 0 }, (expression_t*[]){ A, B, NULL })
#define EXPR_11(a, A) \
    expression_operation_new(state, (int[]){ a, 0 }, (expression_t*[]){ A, NULL })
#define EXPR_1A(a, A) \
    expression_address_operation_new(state, (int[]){ a, 0 }, A)
#define EXPR_1B(a, b, A) \
    expression_operation_new(state, (int[]){ a, b, 0 }, (expression_t*[]){ A, NULL })

static expression_t *expression_copy(expression_t *expr);
static void expression_create_goto(parser_state_t *state, int type, char *labelstr);

/* Bison things. */
void yyerror(parser_state_t*, const char*);
int yylex(void);
extern FILE* yyin;

/* Parser APIs. */

/* Starts a new subroutine. */
static void sub_begin(parser_state_t* state, char* name);
/* Closes the current subroutine. */
static void sub_finish(parser_state_t* state);

/* Creates a new variable in the specified subroutine. */
static void var_create(parser_state_t* state, thecl_sub_t* sub, const char* name, int type);
/* Creates a new variable in the specified subroutine, and assigns a value to it.. */
static void var_create_assign(parser_state_t* state, thecl_sub_t* sub, const char* name, int type, expression_t* expr);
/* Returns the stack offset of a specified variable in the specified sub. */
static int var_stack(parser_state_t* state, thecl_sub_t* sub, const char* name);
/* Returns the type of a specified variable in the specified sub. */
static int var_type(parser_state_t* state, thecl_sub_t* sub, const char* name);
/* Returns 1 if a variable of a given name exists, and 0 if it doesn't. */
static int var_exists(thecl_sub_t* sub, const char* name);
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

%}

%error-verbose
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
%token BREAK "break"
%token ASYNC "async"
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
%token NEG "-"
%token NEGI
%token NEGF
%token SIN "sin"
%token COS "cos"
%token SQRT "sqrt"

%token DOLLAR "$"

%type <list> Text_Semicolon_List
%type <list> Instruction_Parameters_List
%type <list> Instruction_Parameters
%type <list> Integer_List
%type <list> Rank_Switch_List
%type <list> Rank_Switch_Next_Value_List

%type <expression> Expression
%type <expression> Expression_Safe

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
%type <param> Rank_Switch_Next_Value

%type <integer> Cast_Target
%type <integer> Cast_Target2
%type <integer> VarDeclareKeyword
%type <integer> VarDeclaration

%nonassoc ADD ADDI ADDF SUBTRACT SUBTRACTI SUBTRACTF MULTIPLY MULTIPLYI MULTIPLYF DIVIDE DIVIDEI DIVIDEF EQUAL EQUALI EQUALF INEQUAL INEQUALI INEQUALF LT LTI LTF LTEQ LTEQI LTEQF GT GTI GTF GTEQ GTEQI GTEQF MODULO OR AND XOR B_OR B_AND
%left NOT NEG NEGI NEGF SIN COS SQRT
%right DEC

%%

Statements:
    | Statements Statement
    ;

Statement:
      "sub" IDENTIFIER {
        sub_begin(state, $2, 0);
        free($2);
      }
      "(" ArgumentDeclaration ")" {
            state->current_sub->arity = state->current_sub->stack / 4;
      }
      "{" Subroutine_Body "}" {
        sub_finish(state);
      }
    | "timeline" IDENTIFIER "(" ")" {
        sub_begin(state, $2, 1);
      }
      "{" Subroutine_Body "}" {
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
    | "global" "[" IDENTIFIER "]" "=" Global_Def ";" {
        global_definition_t *def = malloc(sizeof(global_definition_t));
        strncpy(def->name, $3, 256);
        def->param = $6;
        list_append_new(&state->global_definitions, def);
        free($3);
      }
    | DIRECTIVE TEXT {
        if (strcmp($1, "include") == 0) {
            if (directive_include(state, $2) != 0) {
                /* For proper syntax error displaying, this needs to return. */
                free($1);
                free($2);
                return 1;
            }
        } else if (strcmp($1, "eclmap") == 0) {
            directive_eclmap(state, $2);
        } else {
            char buf[256];
            snprintf(buf, 256, "unknown directive: %s", $1);
            yyerror(state, buf);
        }
        free($1);
        free($2);
    }
    ;

Integer_List:
      Integer {
        $$ = list_new();
        list_append_new($$, $1);
      }
    | Integer_List Integer {
        $$ = $1;
        list_append_new($$, $2);
      }
    ;

Subroutine_Body:
    Instructions
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

VarDeclareKeyword:
      "var" { $$ = 0 }
    | "int" { $$ = 'S' }
    | "float" { $$ = 'f' }
    ;

VarDeclaration:
      VarDeclareKeyword
    | VarDeclareKeyword IDENTIFIER {
          $$ = $1;
          var_create(state, state->current_sub, $2, $1);
          free($2);
      }
    | VarDeclareKeyword IDENTIFIER "=" Expression {
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
    /* The | at the beginning is obviously intentional and needed to allow creating subs with no arguments. */
    | VarDeclareKeyword IDENTIFIER {
          var_create(state, state->current_sub, $2, $1);
          free($2);
      }
    | ArgumentDeclaration "," VarDeclareKeyword IDENTIFIER {
          var_create(state, state->current_sub, $4, $3);
          free($4);
      }
    ;

Instructions:
    | Instructions INTEGER ":" { set_time(state, $2); }
    | Instructions IDENTIFIER ":" { label_create(state, $2); free($2); }
    | Instructions Instruction ";"
    | Instructions Block
    | Instructions RANK { state->instr_rank = parse_rank(state, $2); } 
    ;

Block:
      /* Moving the old if ... gotos to Block, because if else would break with them being in Instruction. */
      "if" Expression "goto" Label "@" Integer ";" {
        const expr_t* expr = expr_get_by_symbol(state->version, IF);
        expression_output(state, $2, 1);
        expression_free($2);
        instr_add(state->current_sub, instr_new(state, expr->id, "pp", $4, $6));
      }
    | "unless" Expression "goto" Label "@" Integer ";" {
        const expr_t* expr = expr_get_by_symbol(state->version, UNLESS);
        expression_output(state, $2, 1);
        expression_free($2);
        instr_add(state->current_sub, instr_new(state, expr->id, "pp", $4, $6));
      }
    | IfBlock
    | WhileBlock
    | TimesBlock
    | SwitchBlock
    ;

CodeBlock:
      "{" Instructions "}"
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
    "unless" Expression {
          char labelstr[256];
          snprintf(labelstr, 256, "unless_%i_%i", yylloc.first_line, yylloc.first_column);
          list_prepend_new(&state->block_stack, strdup(labelstr));
          expression_output(state, $2, 1);
          expression_free($2);
          expression_create_goto(state, IF, labelstr);
      } CodeBlock ElseBlock {
          list_node_t *head = state->block_stack.head;
          label_create(state, head->data);
          state->block_stack.head = head->next;
          free(head->data);
          list_del(&state->block_stack, head);
        }
    | "if" Expression {
          char labelstr[256];
          snprintf(labelstr, 256, "if_%i_%i", yylloc.first_line, yylloc.first_column);
          list_prepend_new(&state->block_stack, strdup(labelstr));
          expression_output(state, $2, 1);
          expression_free($2);
          expression_create_goto(state, UNLESS, labelstr);
      } CodeBlock ElseBlock {
          list_node_t *head = state->block_stack.head;
          label_create(state, head->data);
          free(head->data);
          list_del(&state->block_stack, head);
      }
      ;

ElseBlock:
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
      "while" Expression {
          char labelstr[256];
          snprintf(labelstr, 256, "while_%i_%i", yylloc.first_line, yylloc.first_column);
          char labelstr_st[256];
          char labelstr_end[256];
          snprintf(labelstr_st, 256, "%s_st", (char*)labelstr);
          snprintf(labelstr_end, 256, "%s_end", (char*)labelstr);

          list_prepend_new(&state->block_stack, strdup(labelstr));
          label_create(state, labelstr_st);
          expression_output(state, $2, 1);
          expression_free($2);
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
    } CodeBlock "while" Expression  {
          char labelstr_st[256];
          char labelstr_end[256];
          list_node_t *head = state->block_stack.head;
          snprintf(labelstr_st, 256, "%s_st", (char*)head->data);
          snprintf(labelstr_end, 256, "%s_end", (char*)head->data);

          expression_output(state, $5, 1);
          expression_free($5);
          expression_create_goto(state, IF, labelstr_st);
          label_create(state, labelstr_end);

          free(head->data);
          list_del(&state->block_stack, head);
    } ";"
    ;

TimesBlock:
      "times" Expression {
          if (g_ecl_simplecreate) {
              yyerror(state, "times loops are not allowed in simple creation mode");
              exit(2);
          }
          char loop_name[256];
          snprintf(loop_name, 256, "times_%i_%i", yylloc.first_line, yylloc.first_column);
          var_create(state, state->current_sub, loop_name, 'S');
        
          if ($2->result_type != 'S') {
              char buf[256];
              snprintf(buf, 256, "invalid iteration count type for a times loop: %c", $2->result_type);
              yyerror(state, buf);
              exit(2);
          }

          expression_output(state, $2, 1);
          expression_free($2);
          
          thecl_param_t* param = param_new('S');
          param->stack = 1;
          param->value.val.S = state->current_sub->stack - 4;

          expr_t* expr = expr_get_by_symbol(state->version, ASSIGNI);
          instr_add(state->current_sub, instr_new(state, expr->id, "p", param));

          char labelstr_st[256];
          char labelstr_end[256];
          snprintf(labelstr_st, 256, "%s_st", (char*)loop_name);
          snprintf(labelstr_end, 256, "%s_end", (char*)loop_name);

          label_create(state, labelstr_st);
          
          param = param_new('S');
          param->stack = 1;
          param->value.val.S = state->current_sub->stack - 4;

          expr = expr_get_by_symbol(state->version, DEC);
          instr_add(state->current_sub, instr_new(state, expr->id, "p", param));

          expression_create_goto(state, UNLESS, labelstr_end);

          list_prepend_new(&state->block_stack, strdup(loop_name));
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
    ;

SwitchBlock:
    "switch" Expression {
          char labelstr[256];
          list_prepend_new(&state->block_stack, NULL);
          snprintf(labelstr, 256, "switch_%i_%i", yylloc.first_line, yylloc.first_column);
          list_prepend_new(&state->block_stack, strdup(labelstr));
          expression_create_goto(state, GOTO, labelstr);
    } "{" CaseList "}" {
          list_node_t *head = state->block_stack.head;

          char labelstr[256];
          snprintf(labelstr, 256, "%s_end", (char*)head->data);
          expression_create_goto(state, GOTO, labelstr);

          label_create(state, head->data);

          free(head->data);
          list_del(&state->block_stack, head);

          list_node_t *node = state->block_stack.head;
          while (node->data) {
              switch_case_t *switch_case = node->data;
              expression_output(state, switch_case->expr, 1);
              expression_t *copy = expression_copy($2);
              expression_output(state, copy, 1);
              expression_free(copy);

              const expr_t* expr = expr_get_by_symbol(state->version, EQUALI);
              instr_add(state->current_sub, instr_new(state, expr->id, ""));

              expression_free(switch_case->expr);
              expression_create_goto(state, IF, switch_case->labelstr);
              list_node_t *buf = node;
              node = node->next;
              free(buf->data);
              free(buf);
          }
          if (node->next != NULL) /* Prevent crashing when there is nothing else on the block stack. */
              node->next->prev = NULL;
          state->block_stack.head = node->next;
          free(node);

          label_create(state, labelstr);
          free($2->value);
          expression_free($2);
    }
    ;

CaseList:
    Case
    | Case Instructions
    | CaseList Case
    | CaseList Case Instructions
    ;

Case:
     "case" Expression_Safe ":" {
          switch_case_t *switch_case = malloc(sizeof(switch_case_t));
          switch_case->expr = $2;
          snprintf(switch_case->labelstr, 250, "case_%i_%i", yylloc.first_line, yylloc.first_column);

          label_create(state, switch_case->labelstr);

          list_node_t *head = state->block_stack.head;
          if (head->next) {
              list_prepend_to(&state->block_stack, switch_case, head->next);
          } else {
              list_append_new(&state->block_stack, switch_case);
          }
      }
    ;

    /* TODO: Check the given parameters against the parameters expected for the
     *       instruction. */
Instruction:
      "@" IDENTIFIER "(" Instruction_Parameters ")" {
          /* Force creating a sub call, even if it wasn't defined in the file earlier - useful for calling subs from default.ecl */
          instr_create_call(state, TH10_INS_CALL, $2, $4);
          if ($4 != NULL) {
              list_free_nodes($4);
              free($4);
          }
      }
      | "@" IDENTIFIER "(" Instruction_Parameters ")" "async" {
          /* Same as above, except use ins_15 (callAsync) instead of ins_11 (call) */
          instr_create_call(state, TH10_INS_CALL_ASYNC, $2, $4);
          if ($4 != NULL) {
              list_free_nodes($4);
              free($4);
          }
      } 
      | IDENTIFIER "(" Instruction_Parameters ")" "async" {
          /* Search for sub */
          bool sub_found = false;
          thecl_sub_t* iter_sub;
          list_for_each(&state->ecl->subs, iter_sub) {
              if (strcmp(iter_sub->name, $1) == 0) {
                  sub_found = true;
              }
          }
          if (sub_found) {
              instr_create_call(state, TH10_INS_CALL_ASYNC, $1, $3);
              if ($3 != NULL) list_free_nodes($3);
          } else {
              char errbuf[256];
              snprintf(errbuf, 256, "unknown sub: %s", $1);
              yyerror(state, errbuf);
              g_was_error = true;
          }

          free($3);
      }
      | IDENTIFIER "(" Instruction_Parameters ")" {
        expression_t* expr;
        list_for_each(&state->expressions, expr) {
            expression_output(state, expr, 1);
            expression_free(expr);
        }
        list_free_nodes(&state->expressions);

        eclmap_entry_t* ent = eclmap_find(state->is_timeline_sub ? g_eclmap_timeline_opcode : g_eclmap_opcode, $1);
        if (!ent) {
            /* Search for sub */
            bool sub_found = false;
            thecl_sub_t* iter_sub;
            list_for_each(&state->ecl->subs, iter_sub) {
                if (strcmp(iter_sub->name, $1) == 0) {
                    sub_found = true;
                }
            }
            if (sub_found) {
                instr_create_call(state, TH10_INS_CALL, $1, $3);
                if ($3 != NULL) list_free_nodes($3);;
            } else {
                char errbuf[256];
                snprintf(errbuf, 256, "unknown mnemonic: %s", $1);
                yyerror(state, errbuf);
                g_was_error = true;
            }
        }
        else {
            instr_add(state->current_sub, instr_new_list(state, ent->opcode, $3));
        }

        free($3);
      }
    | INSTRUCTION "(" Instruction_Parameters ")" {
        expression_t* expr;
        list_for_each(&state->expressions, expr) {
            expression_output(state, expr, 1);
            expression_free(expr);
        }
        list_free_nodes(&state->expressions);

        instr_add(state->current_sub, instr_new_list(state, $1, $3));

        free($3);
      }
    | "goto" Label "@" Integer {
        const expr_t* expr = expr_get_by_symbol(state->version, GOTO);
        instr_add(state->current_sub, instr_new(state, expr->id, "pp", $2, $4));
      }
    | Assignment
    | Expression {
        expression_output(state, $1, 1);
        expression_free($1);
      }
    | VarDeclaration {
        if (g_ecl_simplecreate)
            instr_add(state->current_sub, instr_new(state, TH10_INS_STACK_ALLOC, "S", state->current_sub->stack));
    }
    | BreakStatement
    ;

Assignment:
      Address "=" Expression {
        const expr_t* expr = expr_get_by_symbol(state->version, $1->type == 'S' ? ASSIGNI : ASSIGNF);
        expression_output(state, $3, 1);
        expression_free($3);
        instr_add(state->current_sub, instr_new(state, expr->id, "p", $1));
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
      { $$ = NULL; }
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
      CAST_II { $$ = 0x6969; }
    | CAST_IF { $$ = 0x6966; }
    | CAST_FF { $$ = 0x6666; }
    | CAST_FI { $$ = 0x6669; }
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
      Address
    | Integer
    | Floating
    | Text
    | Cast_Target2 Cast_Type {
        $$ = param_new('D');
        $$->stack = $2->stack;
        $$->is_expression_param = $2->is_expression_param;
        $$->value.type = 'm';
        $$->value.val.m.length = 2 * sizeof(int32_t);
        $$->value.val.m.data = malloc(2 * sizeof(int32_t));
        int32_t* D = (int32_t*)$$->value.val.m.data;
        D[0] = $1;
        if ($2->type == 'f') {
            memcpy(&D[1], &$2->value.val.f, sizeof(float));
        } else {
            D[1] = $2->value.val.S;
        }
        param_free($2);
      }
    | Cast_Target "(" Expression ")" {
        list_prepend_new(&state->expressions, $3);

        $$ = param_new($1);
        $$->stack = 1;
        $$->is_expression_param = $1;
        if ($1 == 'S') {
            $$->value.val.S = -1;
        } else {
            $$->value.val.f = -1.0f;
        }
      }
      | Expression {
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
    ;

Rank_Switch_Next_Value: 
      ":" Expression_Safe {$$ = $2}
    ;

Rank_Switch_List:
      Expression_Safe Rank_Switch_Next_Value_List {
        $$ = $2;
        list_prepend_new($$, $1);
      }
    ;

Rank_Switch_Next_Value_List:
      Rank_Switch_Next_Value {
        $$ = list_new();
        list_append_new($$, $1);
      }
    | Rank_Switch_Next_Value_List Rank_Switch_Next_Value {
        $$ = $1;
        list_append_new($$, $2);
    }
    ;

Expression:
      Load_Type                      { $$ = expression_load_new(state, $1); }
    |             "(" Expression ")" { $$ = $2; }
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
    | Address "--"                { $$ = EXPR_1A(DEC,                  $1); }
    | "-" Expression              { $$ = EXPR_1B(NEGI,      NEGF,      $2); }
    | "sin" Expression            { $$ = EXPR_11(SIN,                  $2); }
    | "cos" Expression            { $$ = EXPR_11(COS,                  $2); }
    | "sqrt" Expression           { $$ = EXPR_11(SQRT,                 $2); }

    /* Custom expressions. */
    | Rank_Switch_List            { $$ = expression_rank_switch_new(state, $1); }
    | Expression "?" Expression_Safe ":" Expression_Safe { $$ = expression_ternary_new(state, $1, $3, $5); }
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
    |             "(" Expression ")" { $$ = $2 ;}
    | Cast_Target "(" Expression ")" { $$ = $3; $$->result_type = $1 }
    ;

Address:
      "[" Address_Type "]" {
        $$ = $2;
        $$->stack = 1;
      }
    | "[" IDENTIFIER "]" {
        global_definition_t *def;
        bool found = 0;
        list_for_each(&state->global_definitions, def) {
            if (strcmp(def->name, $2) == 0) {
                thecl_param_t *param = malloc(sizeof(thecl_param_t));
                memcpy(param, def->param, sizeof(thecl_param_t));
                $$ = param;
                found = 1;
                break;
            }
        }
        if(!found) {
            char errbuf[256];
            snprintf(errbuf, 256, "instr_set_types: in sub %s: global definition not found: %s",
                     state->current_sub->name, $2);
            yyerror(state, errbuf);
            exit(1);
        }
        free($2);
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
    | IDENTIFIER {
        if (var_exists(state->current_sub, $1)) {
            int type = var_type(state, state->current_sub, $1);
            if (type == 0) {
                char buf[256];
                snprintf(buf, 256, "typeless variables need to have their type specified with a %% or $ prefix when used: %s", $1);
                yyerror(state, buf);
                exit(2);
            }
            $$ = param_new(type);
            $$->stack = 1;
            if (type == 'S') {
                $$->value.val.S = var_stack(state, state->current_sub, $1);
            } else {
                $$->value.val.f = var_stack(state, state->current_sub, $1);
            }
            free($1);
        } else {
            if (
                   is_post_th10(state->version) /* Old versions don't have stack vars anyway, so no need to show the warning... */
                && strncmp($1, state->current_sub->name, strlen(state->current_sub->name)) != 0
            ) {
                char buf[256];
                snprintf(buf, 256, "warning: %s not found as a variable, treating like a label instead.", $1);
                yyerror(state, buf);
            }
            $$ = param_new('o');
            $$->value.type = 'z';
            $$->value.val.z = $1;
        }
    }
    ;

Address_Type:
      Integer
    | Floating
    ;

Integer:
    INTEGER {
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
    return instr;
}

static void
instr_set_types(
    parser_state_t* state,
    thecl_instr_t* instr)
{
    const char* format = state->instr_format(state->version, instr->id, state->is_timeline_sub);

    thecl_param_t* param;
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

            char errbuf[256];
            snprintf(errbuf, 256, "instr_set_types: in sub %s: wrong argument "
                     "type for opcode %d (expected: %c, got: %c)",
                     state->current_sub->name, instr->id, new_type, param->type);
            yyerror(state, errbuf);
        }

        param->type = new_type;

        if (*format != '*')
            ++format;
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
                } else if(param->value.type == 'D') {
                    int32_t* D = (int32_t*) param->value.val.m.data;
                    if (param->is_expression_param == 'S') {
                        D[1] = param_id;
                    } else {
                        float as_float = param_id;
                        memcpy(&D[1], &as_float, sizeof(float));
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

static void
instr_create_call(
    parser_state_t *state,
    int type,
    char *name,
    list_t *params)
{
    /* Create new arg list */
    list_t *param_list = list_new();

    /* Instr name */
    thecl_param_t *param = param_new('z');
    param->value.type = 'z';
    param->value.val.z = name;
    list_append_new(param_list, param);

    /* Add parameter casts */
    thecl_param_t *iter_param;
    if (params != NULL)
        list_for_each(params, iter_param) {
            param = param_new('D');
            param->stack = iter_param->stack;
            param->is_expression_param = iter_param->is_expression_param;
            param->value.type = 'm';
            param->value.val.m.length = 2 * sizeof(int32_t);
            param->value.val.m.data = malloc(2 * sizeof(int32_t));
            int32_t *D = (int32_t *)param->value.val.m.data;
            switch (iter_param->value.type) {
            case 'S':
                D[0] = 0x6969;
                D[1] = iter_param->value.val.S;
                break;
            case 'f':
                D[0] = 0x6666;
                memcpy(&D[1], &iter_param->value.val.f, sizeof(float));
                break;
            default:
                yyerror(state, "invalid sub parameter");
                g_was_error = true;
            }
            param_free(iter_param);
            list_append_new(param_list, param);
        }

    /* Output expressions from parameters. */
    expression_t* expr;
    list_for_each(&state->expressions, expr) {
        expression_output(state, expr, 1);
        expression_free(expr);
    }
    list_free_nodes(&state->expressions);

    instr_add(state->current_sub, instr_new_list(state, type, param_list));
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
        char errbuf[256];
        snprintf(errbuf, 256, "check_rank_flag: in sub %s: duplicate rank flag %c in '%s'",
                 state->current_sub->name, flag, value);
        yyerror(state, errbuf);
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
            char errbuf[256];
            snprintf(errbuf, 256,
                     "parse_rank: in sub %s: * should not be used with "
                     "other rank flags.",
                     state->current_sub->name);
            yyerror(state, errbuf);
        }
        return 0xFF;
    } else if (check_rank_flag(state, value, '-')) {
        if (strlen(value) != 1) {
            char errbuf[256];
            snprintf(errbuf, 256,
                     "parse_rank: in sub %s: - should not be used with "
                     "other rank flags.",
                     state->current_sub->name);
            yyerror(state, errbuf);
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
            char errbuf[256];
            snprintf(errbuf, 256,
                    "parse_rank: in sub %s: Rank flags 4 and 5 are not used in "
                    "TH13+. Use X for extra, and O for overdrive instead.",
                    state->current_sub->name);
            yyerror(state, errbuf);
        }
        if (!state->has_overdrive_difficulty && (check_rank_flag(state, value, 'X') || check_rank_flag(state, value, 'O'))) {
            char errbuf[256];
            snprintf(errbuf, 256,
                    "parse_rank: in sub %s: Rank flags X and O do not exist "
                    "before TH13. Use 4 and 5 for the unused difficulties flags "
                    "instead.",
                    state->current_sub->name);
            yyerror(state, errbuf);
        }
        if (check_rank_flag(state, value, 'W') || check_rank_flag(state, value, 'Y') || check_rank_flag(state, value, 'Z')) {
          char errbuf[256];
          snprintf(errbuf, 256,
                   "parse_rank: in sub %s: Rank flags W, X, Y and Z no "
                   "longer refer to unused difficulties 4-7. %s",
                   state->current_sub->name,
                   state->has_overdrive_difficulty
                       ? "In TH13+, use 6 and 7 for the remaining two unused "
                         "difficulties, X for extra, and O for overdrive."
                       : "Before TH13, use 4, 5, 6, and 7 to refer to the "
                         "unused difficulties.");
          yyerror(state, errbuf);
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
    const int *symbol = symbols;
    for (; *symbol; ++symbol) {
        const expr_t* expr = expr_get_by_symbol(state->version, *symbol);

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
    } else if (expr->type == EXPRESSION_VAL) {
        thecl_param_t *param = malloc(sizeof(thecl_param_t));
        memcpy(param, expr->value, sizeof(thecl_param_t));
        copy->value = param;
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
        // Since expression_optimize is already done recursively for children, it shouldn't be called for child expressions.
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
            char* next_diff = diffs[diff++];
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

    // TODO: handle some single-child expressions, such as sin or cos
    if (child_cnt != 2) return;

    if (
           child_expr_1->type != EXPRESSION_VAL
        || child_expr_2->type != EXPRESSION_VAL
        || child_expr_1->value->stack // Variables are not acceptable, obviously.
        || child_expr_2->value->stack
    ) return;

    expr_t* tmp_expr = expr_get_by_id(state->version, expression->id);
    
    // Need to get the type from tmp_expr->return_type, since expression->result_type could have been modified by typecasts.
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
            // Since the cases above cover all existing 2-parameter expressions there is no possibility of this ever hapenning.
            // Just putting this error message in case someone adds new expressions and forgets about handling them here...
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
    int is_timeline)
{
    state->is_timeline_sub = is_timeline;

    thecl_sub_t* sub = malloc(sizeof(thecl_sub_t));

    sub->name = strdup(name);
    list_init(&sub->instrs);
    sub->stack = 0;
    sub->arity = 0;
    sub->var_count = 0;
    sub->vars = NULL;
    sub->offset = 0;
    list_init(&sub->labels);

    if (!state->uses_numbered_subs) {
        if (is_timeline) {
            yyerror(state, "timelines don't exist in th10+");
        }

        // Touhou expects the list of subs to be sorted by name.
        thecl_sub_t* iter_sub;
        list_for_each(&state->ecl->subs, iter_sub) {
            int diff = strcmp(name, iter_sub->name);
            if(diff == 0) {
                char buf[256];
                snprintf(buf, 256, "duplicate sub: %s", name);
                yyerror(state, buf);
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

    if (!is_timeline) ++state->ecl->sub_count;
    state->instr_time = 0;
    state->instr_rank = 0xff;
    state->current_sub = sub;
}

static void
sub_finish(
    parser_state_t* state)
{
    if (is_post_th10(state->ecl->version) && !g_ecl_simplecreate) {
        
        thecl_instr_t* var_ins = instr_new(state, TH10_INS_STACK_ALLOC, "S", state->current_sub->stack);
        var_ins->time = 0;
        instr_prepend(state->current_sub, var_ins);

        thecl_instr_t* last_ins = list_tail(&state->current_sub->instrs);
        if (last_ins == NULL || last_ins->id != TH10_INS_RET_NORMAL && last_ins->id != TH10_INS_RET_BIG) {
            instr_add(state->current_sub, instr_new(state, TH10_INS_RET_NORMAL, ""));
        }
    }

    state->current_sub = NULL;
}

static void
var_create(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name,
    int type)
{
    if (!is_post_th10(state->version)) {
        char buf[256];
        snprintf(buf, 256, "stack variable declaration is not allowed in version: %i", state->version);
        yyerror(state, buf);
        exit(2);
    }
    if (g_ecl_simplecreate && type != 0) {
        char buf[256];
        snprintf(buf, 256, "only typeless variables are allowed in simple creation mode: %s", name);
        yyerror(state, buf);
        exit(2);
    }
    if (var_exists(sub, name)) {
        char buf[256];
        snprintf(buf, 256, "redeclaration of variable: %s", name);
        yyerror(state, buf);
    }

    thecl_variable_t* var = malloc(sizeof(thecl_variable_t));
    var->name = strdup(name);
    var->type = type;

    ++sub->var_count;
    sub->vars = realloc(sub->vars, sub->var_count * sizeof(thecl_variable_t*));
    sub->vars[sub->var_count - 1] = var;

    sub->stack += 4;
}

static void
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
    if (type == 0) {
        yyerror(state, "var creation with assignment requires int/float declaration keyword");
        exit(2);
    }
    var_create(state, sub, name, type);

    expression_output(state, expr, 1);
    expression_free(expr);

    thecl_param_t* param = param_new(type);
    param->value.type = type;
    if (type == 'S') {
        param->value.val.S = state->current_sub->stack - 4;
    } else {
        param->value.val.f = state->current_sub->stack - 4;
    }
    param->stack = 1;

    const expr_t* expr_assign = expr_get_by_symbol(state->version, type == 'S' ? ASSIGNI : ASSIGNF);
    instr_add(state->current_sub, instr_new(state, expr_assign->id, "p", param));
}

static int
var_stack(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name)
{
    // TODO: Check if a local is shadowed by a global.

    eclmap_entry_t* ent = eclmap_find(g_eclmap_global, name);
    if (ent) return ent->opcode;

    char buf[256];
    unsigned int i;
    for (i = 0; i < sub->var_count; ++i) {
        if (strcmp(name, sub->vars[i]->name) == 0)
            return i * 4;
    }
    snprintf(buf, 256, "variable not found: %s", name);
    yyerror(state, buf);
    return 0;
}

static int
var_type(
    parser_state_t* state,
    thecl_sub_t* sub,
    const char* name)
{
    eclmap_entry_t* ent = eclmap_find(g_eclmap_global, name);
    if (ent) return ent->signature[0] == '$' ? 'S' : 'f';
    
    char buf[256];
    unsigned int i;
    for (i = 0; i < sub->var_count; ++i) {
        if (strcmp(name, sub->vars[i]->name) == 0)
            return sub->vars[i]->type;
    }
    snprintf(buf, 256, "variable not found: %s", name);
    yyerror(state, buf);
    return 0;
}

static int
var_exists(
    thecl_sub_t* sub,
    const char* name)
{
    eclmap_entry_t* ent = eclmap_find(g_eclmap_global, name);
    if (ent) return 1;

    unsigned int i;
    for (i = 0; i < sub->var_count; ++i) {
        if (strcmp(name, sub->vars[i]->name) == 0) return 1;
    }

    return 0;
}

static void
var_shorthand_assign(
    parser_state_t* state,
    thecl_param_t* param,
    expression_t* expr_assign,
    int EXPRI,
    int EXPRF)
{
    // Can't use the same param twice, so a copy is created.
    thecl_param_t* param_clone = malloc(sizeof(thecl_param_t));
    memcpy(param_clone, param, sizeof(thecl_param_t));

    expression_t* expr_load = expression_load_new(state, param_clone);
    expression_t* expr_main = EXPR_22(EXPRI, EXPRF, expr_load, expr_assign);
    expression_output(state, expr_main, 1);
    expression_free(expr_main);
    // No need to free expr_load or expr, since they both got freed as children of expr_main.

    const expr_t* expr = expr_get_by_symbol(state->version, param->type == 'S' ? ASSIGNI : ASSIGNF);
    instr_add(state->current_sub, instr_new(state, expr->id, "p", param));
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
        char buf[256];
        snprintf(buf, 256, "illegal timer change: %d to %d", state->instr_time, new_time);
        yyerror(state, buf);
    }
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
        char* input_org = current_input;

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
        char buf[256];
        snprintf(buf, 256, "#include error: couldn't open %s for reading", path);
        yyerror(state, buf);
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
        char buf[256];
        snprintf(buf, 256, "#eclmap error: couldn't open %s for reading", path);
        yyerror(state, buf);
    } else {
        eclmap_load(g_eclmap_opcode, g_eclmap_timeline_opcode, g_eclmap_global, map_file, path);
        fclose(map_file);
    }
    free(path);
}

void
yyerror(
    parser_state_t* state,
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
