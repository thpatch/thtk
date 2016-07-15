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
#ifndef EXPR_H_
#define EXPR_H_

#include <config.h>

typedef struct {
    int symbol;
    int id;
    /* 0 indicates no return value. */
    int return_type;
    char* param_format;
    size_t stack_arity;
    /* / can be used to separate several patterns ... or NULL to match anything
     * ... maybe a special symbol for that would be better. */
    char* stack_formats;
    /* This is a string used for displaying the expression,
     * [p0...pn] is used to insert the parameters.
     * [s0...sn] is used to insert the previous instructions. */
    char* display_format;
} expr_t;

/* Returns an expression by its symbol. */
const expr_t* expr_get_by_symbol(unsigned int version, int symbol);

/* Returns an expression by its id. */
const expr_t* expr_get_by_id(unsigned int version, int id);

/* A leaf expression is one that requires no other expressions, while returning
 * a value. */
int expr_is_leaf(unsigned int version, int id);

#endif
