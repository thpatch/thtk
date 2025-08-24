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
#include <stddef.h>
#include "thecl.h"
#include "ecsparse.h"
#include "expr.h"

static const expr_t
th06_expressions[] = {
    /* While th06 doesn't support compiling expressions as of now,
     * having this is still useful because the compiler will be able to
     * simply calculate expression results compile-time (if possible).
     * And if it fails to get a raw value out of an expression, simply
     * throw an error... Negative ID will be used to indicate that the expression
     * can't be outputted (must be unique). Of course, in the future thecl could be upgraded
     * to actually compile the register-based expressions of th06 format. */
    /* And no, this doesn't break decompiling, as th06.c doesn't check expressions at all. */
    /*SYM         ID  RET     P  A    S   DISP  NB */
    { LOADI,      -1, 'S',  "S", 0, NULL, "p0", 0 },
    { LOADF,      -2, 'f',  "f", 0, NULL, "p0", 0 },

    { ADDI,       -3, 'S', NULL, 2, "SS", "s1 + s0", 0 },
    { ADDF,       -4, 'f', NULL, 2, "ff", "s1 + s0", 0 },
    { SUBTRACTI,  -5, 'S', NULL, 2, "SS", "s1 - s0", 0 },
    { SUBTRACTF,  -6, 'f', NULL, 2, "ff", "s1 - s0", 0 },
    { MULTIPLYI,  -7, 'S', NULL, 2, "SS", "s1 * s0", 0 },
    { MULTIPLYF,  -8, 'f', NULL, 2, "ff", "s1 * s0", 0 },
    { DIVIDEI,    -9, 'S', NULL, 2, "SS", "s1 / s0", 0 },
    { DIVIDEF,   -10, 'f', NULL, 2, "ff", "s1 / s0", 0 },
    { MODULO,    -11, 'S', NULL, 2, "SS", "s1 % s0", 0 },
    { EQUALI,    -12, 'S', NULL, 2, "SS", "s1 == s0", 0 },
    { EQUALF,    -13, 'S', NULL, 2, "ff", "s1 == s0", 0 },
    { INEQUALI,  -14, 'S', NULL, 2, "SS", "s1 != s0", 0 },
    { INEQUALF,  -15, 'S', NULL, 2, "ff", "s1 != s0", 0 },
    { LTI,       -16, 'S', NULL, 2, "SS", "s1 < s0", 0 },
    { LTF,       -17, 'S', NULL, 2, "ff", "s1 < s0", 0 },
    { LTEQI,     -18, 'S', NULL, 2, "SS", "s1 <= s0", 0 },
    { LTEQF,     -19, 'S', NULL, 2, "ff", "s1 <= s0", 0 },
    { GTI,       -20, 'S', NULL, 2, "SS", "s1 > s0", 0 },
    { GTF,       -21, 'S', NULL, 2, "ff", "s1 > s0", 0 },
    { GTEQI,     -22, 'S', NULL, 2, "SS", "s1 >= s0", 0 },
    { GTEQF,     -23, 'S', NULL, 2, "ff", "s1 >= s0", 0 },
    { NOTI,      -24, 'S', NULL, 1,  "S", "!s0", 0 },
    { NOTF,      -25, 'S', NULL, 1,  "f", "!s0", 0 },
    { OR,        -26, 'S', NULL, 2, "SS", "s1 || s0", 0 },
    { AND,       -27, 'S', NULL, 2, "SS", "s1 && s0", 0 },
    { XOR,       -28, 'S', NULL, 2, "SS", "s1 ^ s0", 0 },
    { B_OR,      -29, 'S', NULL, 2, "SS", "s1 | s0", 0 },
    { B_AND,     -30, 'S', NULL, 2, "SS", "s1 & s0", 0 },
    { DEC,       -31, 'S', NULL, 0, NULL, "p0--", 0 },
    { SIN,       -32, 'f', NULL, 1,  "f", "sin(s0)", 1 },
    { COS,       -33, 'f', NULL, 1,  "f", "cos(s0)", 1 },
    { NEGI,      -34, 'S', NULL, 1,  "S", "-s0", 0 },
    { NEGF,      -35, 'f', NULL, 1,  "f", "-s0", 0 },
    { SQRT,      -36, 'f', NULL, 1,  "f", "sqrt(s0)", 1 },
    { 0,           0,   0, NULL, 0, NULL, NULL, 0 }
};

static const expr_t
th10_expressions[] = {
    /* The program checks against the number of params, as well as the
     * requested stack depth, and does the replacements. */
    /* p0 is the first param, p1 the second ... */
    /* s0 is the previous instruction, s1 the one previous to s0 ... */

    /*SYM         ID  RET     P  A    S   DISP                      NB */
    { RETURN,     10,   0, NULL, 0,  NULL,          "return", 0 },

    { GOTO,       12,   0, "ot", 0,  "S",           "goto p0 @ p1", 0 },
    { UNLESS,     13,   0, "ot", 1,  "S", "unless (s0) goto p0 @ p1", 1 },
    { IF,         14,   0, "ot", 1,  "S",     "if (s0) goto p0 @ p1", 1 },

    { LOADI,      42, 'S',  "S", 0, NULL, "p0", 0 },
    { ASSIGNI,    43,   0,  "S", 1,  "S", "p0 = s0", 1 },
    { LOADF,      44, 'f',  "f", 0, NULL, "p0", 0 },
    { ASSIGNF,    45,   0,  "f", 1,  "f", "p0 = s0", 1 },

    { ADDI,       50, 'S', NULL, 2, "SS", "s1 + s0", 0 },
    { ADDF,       51, 'f', NULL, 2, "ff", "s1 + s0", 0 },
    { SUBTRACTI,  52, 'S', NULL, 2, "SS", "s1 - s0", 0 },
    { SUBTRACTF,  53, 'f', NULL, 2, "ff", "s1 - s0", 0 },
    { MULTIPLYI,  54, 'S', NULL, 2, "SS", "s1 * s0", 0 },
    { MULTIPLYF,  55, 'f', NULL, 2, "ff", "s1 * s0", 0 },
    { DIVIDEI,    56, 'S', NULL, 2, "SS", "s1 / s0", 0 },
    { DIVIDEF,    57, 'f', NULL, 2, "ff", "s1 / s0", 0 },
    { MODULO,     58, 'S', NULL, 2, "SS", "s1 % s0", 0 },
    { EQUALI,     59, 'S', NULL, 2, "SS", "s1 == s0", 0 },
    { EQUALF,     60, 'S', NULL, 2, "ff", "s1 == s0", 0 },
    { INEQUALI,   61, 'S', NULL, 2, "SS", "s1 != s0", 0 },
    { INEQUALF,   62, 'S', NULL, 2, "ff", "s1 != s0", 0 },
    { LTI,        63, 'S', NULL, 2, "SS", "s1 < s0", 0 },
    { LTF,        64, 'S', NULL, 2, "ff", "s1 < s0", 0 },
    { LTEQI,      65, 'S', NULL, 2, "SS", "s1 <= s0", 0 },
    { LTEQF,      66, 'S', NULL, 2, "ff", "s1 <= s0", 0 },
    { GTI,        67, 'S', NULL, 2, "SS", "s1 > s0", 0 },
    { GTF,        68, 'S', NULL, 2, "ff", "s1 > s0", 0 },
    { GTEQI,      69, 'S', NULL, 2, "SS", "s1 >= s0", 0 },
    { GTEQF,      70, 'S', NULL, 2, "ff", "s1 >= s0", 0 },
    { NOTI,       71, 'S', NULL, 1,  "S", "!s0", 0 },
    { NOTF,       72, 'S', NULL, 1,  "f", "!s0", 0 },
    { OR,         73, 'S', NULL, 2, "SS", "s1 || s0", 0 },
    { AND,        74, 'S', NULL, 2, "SS", "s1 && s0", 0 },
    { XOR,        75, 'S', NULL, 2, "SS", "s1 ^ s0", 0 },
    { B_OR,       76, 'S', NULL, 2, "SS", "s1 | s0", 0 },
    { B_AND,      77, 'S', NULL, 2, "SS", "s1 & s0", 0 },
    { DEC,        78, 'S',  "S", 0, NULL, "p0--", 0 },
    { SIN,        79, 'f', NULL, 1,  "f", "sin(s0)", 1 },
    { COS,        80, 'f', NULL, 1,  "f", "cos(s0)", 1 },
    { NEGI,       84, 'S', NULL, 1,  "S", "-s0", 0 },
    // NEGF existed since MoF but had a broken implementation until DS
    { NEGF,       -1, 'f', NULL, 1,  "f", "-s0", 0 },
    { SQRT,       -2, 'f', NULL, 1,  "f", "sqrt(s0)", 1 },
    { 0,           0,   0, NULL, 0, NULL, NULL, 0 }
};

static const expr_t
alcostg_expressions[] = {
    /*SYM         ID  RET     P  A    S   DISP */
    { SQRT,       88, 'f', NULL, 1,  "f", "sqrt(s0)", 1 },
    { 0,           0,   0, NULL, 0, NULL, NULL, 0 }
};

static const expr_t
th125_expressions[] = {
    /*SYM         ID  RET     P  A    S   DISP */
    { NEGF,       85, 'f', NULL, 1,  "f", "-s0", 0 },
    { 0,           0,   0, NULL, 0, NULL, NULL, 0 }
};

static const expr_t
th13_expressions[] = {
    /*SYM         ID  RET     P  A    S   DISP */
    { NEGI,       83, 'S', NULL, 1, "S", "-s0", 0 },
    { NEGF,       84, 'f', NULL, 1, "f", "-s0", 0 },
    { 0,           0,   0, NULL, 0, NULL, NULL, 0 }
};

static const expr_t*
expr_get_by_symbol_from_table(
    const expr_t* table,
    int symbol)
{
    while (table->symbol) {
        if (table->symbol == symbol)
            return table;
        ++table;
    }

    return NULL;
}

const expr_t*
expr_get_by_symbol(
    unsigned int version,
    int symbol)
{
    const expr_t* ret = NULL;

    switch (engine_version(version)) {
        default:
        case VER_POST_TH13:
            ret = expr_get_by_symbol_from_table(th13_expressions, symbol);
            if (ret) break;
        case VER_POST_TH125:
            ret = expr_get_by_symbol_from_table(th125_expressions, symbol);
            if (ret) break;
        case VER_POST_ALCOSTG:
            ret = expr_get_by_symbol_from_table(alcostg_expressions, symbol);
            if (ret) break;
        case VER_POST_TH10:
            ret = expr_get_by_symbol_from_table(th10_expressions, symbol);
            break;
        case VER_PRE_TH10:
        case VER_PRE_TH8:
            ret = expr_get_by_symbol_from_table(th06_expressions, symbol);
    }

    return ret;
}

static const expr_t*
expr_get_by_id_from_table(
    const expr_t* table,
    int id)
{
    while (table->symbol) {
        if (table->id == id)
            return table;
        ++table;
    }

    return NULL;
}

const expr_t*
expr_get_by_id(
    unsigned int version,
    int id)
{
    const expr_t* ret = NULL;

    switch (engine_version(version)) {
        default:
        case VER_POST_TH13:
            ret = expr_get_by_id_from_table(th13_expressions, id);
            if (ret) break;
        case VER_POST_TH125:
            ret = expr_get_by_id_from_table(th125_expressions, id);
            if (ret) break;
        case VER_POST_ALCOSTG:
            ret = expr_get_by_id_from_table(alcostg_expressions, id);
            if (ret) break;
        case VER_POST_TH10:
            ret = expr_get_by_id_from_table(th10_expressions, id);
            break;
        case VER_PRE_TH10:
        case VER_PRE_TH8:
            ret = expr_get_by_id_from_table(th06_expressions, id);
    }

    return ret;
}

int
expr_is_leaf(
    unsigned int version,
    int id)
{
    const expr_t* expr = expr_get_by_id(version, id);

    if (!expr)
        return 0;

    return expr->stack_arity == 0 &&
           expr->return_type != 0;
}
