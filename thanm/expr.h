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

#include "thanm.h"
#include "list.h"
#include "reg.h"

typedef enum operation_type_t {
    OP_ASSIGN,
    OP_UNARY,
    OP_BINARY_COMMUT,
    OP_BINARY_NONCOMMUT
} operation_type_t;

typedef struct operation_id_pair_t {
    /* IDs normal are isntructions like:
     * isetAdd(&x, 2, 2) -> *x = 2 + 2 */
    int id_normal;
    /* IDs short are instructions like:
     * iadd(&x, 2) -> *x = *x + 2 */
    int id_short;
} operation_id_pair_t;

typedef struct operation_t {
    int symbol;
    operation_type_t type;
    operation_id_pair_t ids_S;
    operation_id_pair_t ids_f;
} operation_t;

const operation_t* op_get(int symbol, int ver);

typedef enum expr_type_t {
    EXPR_VAL,
    EXPR_OP,
    EXPR_ASSIGN
} expr_type_t;

typedef struct expr_t {
    expr_type_t type;
    const operation_t* op;
    /* For EXPR_VAL: the value (as param) */
    thanm_param_t* param;
    /* Also for EXPR_VAL: the register (if used) */
    reg_t* reg;
    /* For EXPR_OP/ASSIGN: List of expr_t */
    list_t children;
} expr_t;

expr_t* expr_new_val(thanm_param_t* param, int ver);
expr_t* expr_new_val_reg(reg_t* reg);
expr_t* expr_new_op(int symbol, expr_t* children[], int ver);

#define expr_new_unary(s, c1, ver) \
    expr_new_op(s, (expr_t*[]){c1, NULL}, ver)

#define expr_new_binary(s, c1, c2, ver) \
    expr_new_op(s, (expr_t*[]){c1, c2, NULL}, ver)

expr_t* expr_new_assign(int symbol, expr_t* target, expr_t* val, int ver);

typedef enum expr_error_t {
    EXPR_ERR_SUCCESS,
    EXPR_ERR_BAD_TYPES,
    EXPR_ERR_BAD_LVAL,
    EXPR_ERR_NO_INSTR,
    EXPR_ERR_NO_SCRIPT,
    EXPR_ERR_REG_FULL
} expr_error_t;

void expr_free(expr_t* expr);
expr_error_t expr_output(parser_state_t* state, expr_t* expr, reg_t* out);
const char* expr_strerror(expr_error_t err);

#endif
