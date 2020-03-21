%option noinput nounput noyywrap
%top {
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
#include <stdint.h>
}
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "thanm.h"
    #include "anmparse.h"
    #include "util.h"
    void yyerror(parser_state_t*, const char*, ...);
    /* yylloc.first_column is initialized to 1. */
    #define YY_USER_INIT yylloc.first_column = 0
    /* XXX: The builtin YY_INPUT does something with the newlines that doesn't
     * work well with Wine/Windows. */
    /* TODO: Add some \r handling to the line counting stuff. */
    #define YY_INPUT(buf,result,max_size) \
    { \
        int c = getc_unlocked(yyin); \
        if (c == EOF) { \
            result = YY_NULL; \
        } else { \
            if (c == '\n') { \
                yylloc.first_line++; \
                yylloc.last_line = yylloc.first_line; \
                yylloc.first_column = 0; \
            } else { \
                yylloc.first_column++; \
            } \
            yylloc.last_column = yylloc.first_column; \
            buf[0] = c; \
            result = 1; \
        } \
    }

%x COMMENT COMMENT_SINGLE STRING
%%
","        return COMMA;
":"        return COLON;
";"        return SEMICOLON;
"["        return SQUARE_OPEN;
"]"        return SQUARE_CLOSE;
"{"        return BRACE_OPEN;
"}"        return BRACE_CLOSE;
"("        return PARENTHESIS_OPEN;
")"        return PARENTHESIS_CLOSE;
"="        return ASSIGN;
"+="       return ASSIGNADD;
"-="       return ASSIGNSUB;
"*="       return ASSIGNMUL;
"/="       return ASSIGNDIV;
"%="       return ASSIGNMOD;
"+"        return ADD;
"-"        return SUBTRACT;
"*"        return MULTIPLY;
"/"        return DIVIDE;
"%"        return MODULO;
"rand"     return RAND;
"sin"      return SIN;
"cos"      return COS;
"tan"      return TAN;
"acos"     return ACOS;
"atan"     return ATAN;
"$"        return DOLLAR;
"entry"    return ENTRY;
"script"   return SCRIPT;
"global"   return GLOBAL;
"int"      return INT;
"float"    return FLOAT;
"timeof"   return TIMEOF;
"offsetof" return OFFSETOF;
"scriptof" return SCRIPTOF;
"spriteof" return SPRITEOF;

(-|\+)?[0-9]+(\.([0-9]*f|[0-9]+)|f) {
    yylval.floating = strtof(yytext, NULL);
    return FLOATING;
}
rad\((-|\+)?[0-9]+(\.([0-9]*f|[0-9]+)|f)?\) {
    yylval.floating = strtof(yytext+4, NULL) / 180.0f * 3.14159265359f;
    return FLOATING;
}
-?[0-9]+ {
    yylval.integer = strtol(yytext, NULL, 10);
    return INTEGER;
}
(-|\+)?0x[0-9a-fA-F]+ {
    yylval.integer = strtoul(yytext, NULL, 16);
    return INTEGER;
}
(-|\+)?0b[0-1]+ {
    bool hasSign = yytext[0] == '-' || yytext[0] == '+';
    yylval.integer = strtoul(yytext + (hasSign ? 3 : 2), NULL, 2);
    if (yytext[0] == '-') yylval.integer = -yylval.integer;
    return INTEGER;
}

[a-zA-Z_][a-zA-Z0-9_]* {
    yylval.string = strdup(yytext);
    return IDENTIFIER;
}

#([a-zA-Z]+) {
    yylval.string = strdup(yytext + 1);
    return DIRECTIVE;
}

\" {
    BEGIN(STRING);
    yylval.string = strdup("");
}
<STRING>[^\"\n]+ {
    yylval.string = realloc(yylval.string, strlen(yylval.string) + yyleng + 1);
    strcat(yylval.string, yytext);
}
<STRING>\n {
    free(yylval.string);
    yyerror(NULL, "unterminated string");
    BEGIN(INITIAL);
    yyterminate();
}
<STRING>\" {
    BEGIN(INITIAL);
    return TEXT;
}

"/*"               BEGIN(COMMENT);
<COMMENT>[^*]*     |
<COMMENT>\*[^*/]*  ;
<COMMENT>"*/"      BEGIN(INITIAL);

"//"                  BEGIN(COMMENT_SINGLE);
<COMMENT_SINGLE>.*    ;
<COMMENT_SINGLE>"\n"  BEGIN(INITIAL);

[\t\n\r ]+ ;
. {
    char buf[256];
    snprintf(buf, 256, "illegal token: %s", yytext);
    yyerror(NULL, buf);
    return ILLEGAL_TOKEN;
}
