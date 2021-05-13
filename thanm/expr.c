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
#include <stdlib.h>
#include <math.h>
#include "expr.h"
#include "thanm.h"
#include "anmparse.h"
#include "reg.h"
#include "list.h"

/* v0 has no variables, and as such no operators. */

static const operation_t ops_v2_v3[] = {
                                   /* IDS_S       IDS_f */
   /* SYMBOL     TYPE                 norm  short norm  short*/
    { ASSIGN,    OP_ASSIGN,           37,   -1,   38,   -1  },
    { ADD,       OP_BINARY_COMMUT,    49,   39,   50,   40  },
    { SUBTRACT,  OP_BINARY_NONCOMMUT, 51,   41,   52,   42  },
    { MULTIPLY,  OP_BINARY_COMMUT,    53,   43,   54,   44  },
    { DIVIDE,    OP_BINARY_NONCOMMUT, 55,   45,   56,   46  },
    { MODULO,    OP_BINARY_NONCOMMUT, 57,   47,   58,   48  },
    { RAND,      OP_UNARY,            59,   -1,   60,   -1  },
    { SIN,       OP_UNARY,            -1,   -1,   61,   -1  },
    { COS,       OP_UNARY,            -1,   -1,   62,   -1  },
    { TAN,       OP_UNARY,            -1,   -1,   63,   -1  },
    { ACOS,      OP_UNARY,            -1,   -1,   64,   -1  },
    { ATAN,      OP_UNARY,            -1,   -1,   65,   -1  },
    { 0, 0, 0, 0, 0, 0}
};

static const operation_t ops_v4_v7[] = {
                                   /* IDS_S       IDS_f */
   /* SYMBOL     TYPE                 norm  short norm  short*/
    { ASSIGN,    OP_ASSIGN,           6,    -1,   7,    -1  },
    { ADD,       OP_BINARY_COMMUT,    18,   8,    19,   9   },
    { SUBTRACT,  OP_BINARY_NONCOMMUT, 20,   10,   21,   11  },
    { MULTIPLY,  OP_BINARY_COMMUT,    22,   12,   23,   13  },
    { DIVIDE,    OP_BINARY_NONCOMMUT, 24,   14,   25,   15  },
    { MODULO,    OP_BINARY_NONCOMMUT, 26,   16,   27,   17  },
    { RAND,      OP_UNARY,            40,   -1,   41,   -1  },
    { SIN,       OP_UNARY,            -1,   -1,   42,   -1  },
    { COS,       OP_UNARY,            -1,   -1,   43,   -1  },
    { TAN,       OP_UNARY,            -1,   -1,   44,   -1  },
    { ACOS,      OP_UNARY,            -1,   -1,   45,   -1  },
    { ATAN,      OP_UNARY,            -1,   -1,   46,   -1  },
    { 0, 0, 0, 0, 0, 0}
};

static const operation_t ops_v8[] = {
                                   /* IDS_S       IDS_f */
   /* SYMBOL     TYPE                 norm  short norm  short*/
    { ASSIGN,    OP_ASSIGN,           100,  -1,   101,  -1  },
    { ADD,       OP_BINARY_COMMUT,    112,  102,  113,  103 },
    { SUBTRACT,  OP_BINARY_NONCOMMUT, 114,  104,  115,  105 },
    { MULTIPLY,  OP_BINARY_COMMUT,    116,  106,  117,  107 },
    { DIVIDE,    OP_BINARY_NONCOMMUT, 118,  108,  119,  109 },
    { MODULO,    OP_BINARY_NONCOMMUT, 120,  110,  121,  111 },
    { RAND,      OP_UNARY,            122,   -1,  123,  -1  },
    { SIN,       OP_UNARY,            -1,    -1,  124,  -1  },
    { COS,       OP_UNARY,            -1,    -1,  125,  -1  },
    { TAN,       OP_UNARY,            -1,    -1,  127,  -1  },
    { ACOS,      OP_UNARY,            -1,    -1,  128,  -1  },
    { ATAN,      OP_UNARY,            -1,    -1,  129,  -1  },
    { 0, 0, 0, 0, 0, 0}
};

static const operation_t*
op_get_table(
    int ver
) {
    switch(ver) {
        case 2:
        case 3:
            return ops_v2_v3;
        case 4:
        case 7:
            return ops_v4_v7;
        case 8:
            return ops_v8;
        default:
            return NULL;
    }
}

const operation_t*
op_get(
    int symbol,
    int ver
) {
    const operation_t* table = op_get_table(ver);
    if (table != NULL) {
        while(table->symbol) {
            if (table->symbol == symbol)
                return table;
            ++table;
        }
    }
    return NULL;
}

static expr_t*
expr_new(
    expr_type_t type
) {
    expr_t* ret = (expr_t*)malloc(sizeof(expr_t));
    list_init(&ret->children);
    ret->param = NULL;
    ret->type = type;
    ret->op = NULL;
    ret->reg = NULL;
    return ret;
}

expr_t*
expr_new_val(
    thanm_param_t* param,
    int ver
) {
    expr_t* ret = expr_new(EXPR_VAL);
    ret->param = param;
    if (param->is_var) {
        int id = param->val->type == 'S'
            ? param->val->val.S
            : (int)param->val->val.f;
        reg_t* reg = reg_get_by_id(id, ver);
        if (reg != NULL)
            ret->reg = reg;
    }
    return ret;
}

expr_t*
expr_new_val_reg(
    reg_t* reg
) {
    expr_t* ret = expr_new(EXPR_VAL);
    ret->param = reg_to_param(reg);
    ret->reg = reg;
    return ret;
}

expr_t*
expr_new_op(
    int symbol,
    expr_t* children[],
    int ver
) {
    const operation_t* op = op_get(symbol, ver);
    if (!op)
        return NULL;

    expr_t* ret = expr_new(EXPR_OP);
    ret->op = op;

    int ind = 0;
    while(children[ind]) {
        list_append_new(&ret->children, children[ind]);
        ++ind;
    }

    return ret;
}

expr_t*
expr_new_assign(
    int symbol,
    expr_t* target,
    expr_t* val,
    int ver
) {
    const operation_t* op = op_get(symbol, ver);
    if (!op)
        return NULL;

    expr_t* ret = expr_new(EXPR_ASSIGN);
    ret->op = op;

    list_append_new(&ret->children, target);
    list_append_new(&ret->children, val);

    return ret;
}

void
expr_free(
    expr_t* expr    
) {
    if (expr->param)
        thanm_param_free(expr->param);

    expr_t* child;
    list_for_each(&expr->children, child)
        expr_free(child);

    list_free_nodes(&expr->children);
    free(expr);
}

static int
expr_uses_reg(
    expr_t* expr,
    reg_t* reg
) {
    if (expr->type == EXPR_VAL)
        return expr->reg == reg;

    expr_t* child;
    list_for_each(&expr->children, child) {
        if (expr_uses_reg(child, reg))
            return 1;
    }
    return 0;
}

static expr_error_t
expr_evaluate(
    expr_t* expr
) {
    /* Small note about the return values of this function:
     * EXPR_ERR_SUCCESS is returned when expr_output should continue
     * executing. This is both the case when optimisations could be applied
     * and when they couldn't. In this function, the "success" refers to not
     * encountering any errors that would cause issues in the compiled code or
     * issues in the expr_output function. An example of when this function
     * ACTUALLY errors out is when it detects division by 0. */
    /* To check whether optimisations have been applied, one can check
     * if the expr->type after calling the function got changed to EXPR_VAL. */

    /* Can't do much about assignments. */
    if (expr->type != EXPR_OP)
        return EXPR_ERR_SUCCESS;

    /* Create temporary variables now to avoid a lot of duplicated
     * code in switch cases. */
    list_node_t* child1_node = expr->children.head;
    list_node_t* child2_node = child1_node ? child1_node->next : NULL;
    int child1S, child2S;
    float child1f, child2f;
    int type1, type2;
    if (child1_node) {
        thanm_param_t* param = ((expr_t*)child1_node->data)->param;
        if (param->is_var)
            return EXPR_ERR_SUCCESS;

        type1 = param->val->type;
        if (param->val->type == 'S') {
            child1S = param->val->val.S;
            child1f = (float)param->val->val.f;
        } else if (param->val->type == 'f') {
            child1S = (int)param->val->val.f;
            child1f = param->val->val.f;
        } else {
            /* Can happen when an expression like timeof is used; Can't do much
             * about it now because the value is unknown at the time this function runs,
             * solving this would require making thanm multi-pass. */
            return EXPR_ERR_SUCCESS;
        }
    }
    if (child2_node) {
        thanm_param_t* param = ((expr_t*)child2_node->data)->param;
        if (param->is_var)
            return EXPR_ERR_SUCCESS;

        type2 = param->val->type;
        if (param->val->type == 'S') {
            child2S = param->val->val.S;
            child2f = (float)param->val->val.f;
        }
        else if (param->val->type == 'f') {
            child2S = (int)param->val->val.f;
            child2f = param->val->val.f;
        }
        else {
            return EXPR_ERR_SUCCESS;
        }
    }

    /* For now, thanm doesn't support any typecasts... */
    if (!child1_node || (child2_node && type1 != type2))
        return EXPR_ERR_SUCCESS;

    int resS = 0;
    float resf = 0.0f;
    switch(expr->op->symbol) {
        case ADD:
            if (child2_node == NULL) return EXPR_ERR_SUCCESS;
            if (type1 == 'S')
                resS = child1S + child2S;
            else
                resf = child1f + child2f;
            break;
        case SUBTRACT:
            if (child2_node == NULL) return EXPR_ERR_SUCCESS;
            if (type1 == 'S')
                resS = child1S - child2S;
            else
                resf = child1f - child2f;
            break;
        case MULTIPLY:
            if (child2_node == NULL) return EXPR_ERR_SUCCESS;
            if (type1 == 'S')
                resS = child1S * child2S;
            else
                resf = child1f * child2f;
            break;
        case DIVIDE:
            if (child2_node == NULL) return EXPR_ERR_SUCCESS;
            if (type1 == 'S') {
                if (child2S == 0) {
                    return EXPR_ERR_DIVIDE_BY_ZERO;
                }
                resS = child1S / child2S;
            } else {
                if (child2f == 0.0f)
                    return EXPR_ERR_DIVIDE_BY_ZERO;
                resf = child1f / child2f;
             }
            break;
        case MODULO:
            if (child2_node == NULL) return EXPR_ERR_SUCCESS;
            if (type1 == 'S') {
                if (child2S == 0) {
                    return EXPR_ERR_DIVIDE_BY_ZERO;
                }
                resS = child1S % child2S;
            } else {
                if (child2f == 0.0f)
                    return EXPR_ERR_DIVIDE_BY_ZERO;
                resf = fmodf(child1f, child2f);
            }
            break;
        case RAND:
            /* For obvious reasons, we don't evaluate this one. */
            return EXPR_ERR_SUCCESS;
        case SIN:
            if (type1 == 'S')
                return EXPR_ERR_SUCCESS;
            resf = sinf(child1f);
            break;
        case COS:
            if (type1 == 'S')
                return EXPR_ERR_SUCCESS;
            resf = cosf(child1f);
            break;
        case TAN:
            if (type1 == 'S')
                return EXPR_ERR_SUCCESS;
            resf = tanf(child1f);
            break;
        case ACOS:
            if (type1 == 'S')
                return EXPR_ERR_SUCCESS;
            resf = acosf(child1f);
            break;
        case ATAN:
            if (type1 == 'S')
                return EXPR_ERR_SUCCESS;
            resf = atanf(child1f);
            break;
    }
    thanm_param_t* res_param = thanm_param_new(type1);
    res_param->val = (value_t*)malloc(sizeof(value_t));
    res_param->val->type = type1;
    if (type1 == 'S')
        res_param->val->val.S = resS;
    else
        res_param->val->val.f = resf;

    /* Now we transform the expression into an EXPR_VAL, using
     * the param we created. */
     expr->type = EXPR_VAL;
     expr->param = res_param;
     /* And finally, free the old children. */
     expr_t* child;
     list_for_each(&expr->children, child) {
        expr_free(child);
     }
     list_free_nodes(&expr->children);
     return EXPR_ERR_SUCCESS;
}

expr_error_t
expr_output(
    parser_state_t* state,
    expr_t* expr,
    reg_t* out
) {
    /* Expression is already value - no work needed. */
    if (expr->type == EXPR_VAL)
        return EXPR_ERR_SUCCESS;

    expr_t* child;
    if (expr->type == EXPR_ASSIGN) {
        /* For EXPR_ASSIGN, we have to override the out. */
        /* Setting out to the reg itself is a nice optimisation.
         * However, when you get something like x = 2 * 2 + x,
         * things get bad, because x is first set to 2*2, and then x
         * is added to x. Except it's not the same x as at the beginning.
         * So this optimisation can only be safely done if the register is not used
         * within the RHS expression (there might be better ways to determine that). */
        reg_t* target = ((expr_t*)list_head(&expr->children))->reg;
        if (expr->op->type == OP_ASSIGN && !expr_uses_reg(list_tail(&expr->children), target)) {
            out = target;
        } else {
            out = NULL;
        }
    }

    /* First of all, we have to output the child expressions. */
    reg_t* tmp_out = out;
    list_for_each(&expr->children, child) {
        if (child->type != EXPR_VAL) {
            expr_error_t err = expr_output(state, child, tmp_out);
            /* Obviously, we only want one of the expr results to be the same as out. */
            tmp_out = NULL;
            if (err != EXPR_ERR_SUCCESS)
                return err;
        }
    }
    /* Now all child expr are EXPR_VAL. */

    /* Are the expressions valid as far as types go?
     * We have to decide what type ('S'/'f') it is. */
    int type = 0;
    list_for_each(&expr->children, child) {
        if (type == 0)
            type = child->param->type;
        else if (type != 0 && child->param->type != type)
            return EXPR_ERR_BAD_TYPES;
    }
    if (out != NULL && out->type != type)
        return EXPR_ERR_BAD_TYPES;

    /* Perform compile-time evaluation if possible. */
    expr_error_t eval_err = expr_evaluate(expr);
    if (eval_err != EXPR_ERR_SUCCESS)
        return eval_err;
    if (expr->type == EXPR_VAL)
        return EXPR_ERR_SUCCESS;

    if (state->current_script == NULL)
        return EXPR_ERR_NO_SCRIPT;

    /* We have to decide where the result goes.
     * If it's not specified, we have to figure it out ourselves. */
    if (out == NULL) {
        if (expr->type == EXPR_ASSIGN) {
            /* In this case, we obviously want the result in the target. */
            expr_t* target = (expr_t*)list_head(&expr->children);
            if (target->reg == NULL) {
                /* User did something wrong, or the register is unknown. */
                if (!target->param->is_var)
                    return EXPR_ERR_BAD_LVAL; /* ...user did something wrong. */

                /* We don't know the register the user used! We need
                 * to add it. */
                int id = target->param->val->type == 'S'
                    ? target->param->val->val.S
                    : (int)target->param->val->val.f;

                out = reg_new(id, target->param->type);
            } else {
                out = target->reg;
            }
        } else {
            /* Perhaps one of the children is a register?
             * In this case we could use that as output. */
            expr_t* found = NULL;
            list_for_each(&expr->children, child) {
                if (child->reg && child->reg->lock == LOCK_EXPR) {
                    found = child;
                    break;
                }
            }
            /* If it's found, then that's great - we can use it.
             * Otherwise, we have to use a new register. */
            if (found)
                out = found->reg;
            else {
                out = reg_acquire(PURPOSE_EXPR, type, state->current_version);
                if (out == NULL)
                    return EXPR_ERR_REG_FULL;
            }
        }
    }

    /* At this point, we know what to do with the output
     * and what the return type is. We have to determine what instr to use.
     * Obviously, we want to use the short instruction variants if possible. */
    const operation_id_pair_t* ids = type == 'S' ? &expr->op->ids_S : &expr->op->ids_f;
    int use_short = 0;
    expr_t* ignore_expr = NULL;

    /* Does the short ID even exist? */
    if (ids->id_short != -1) {
        /* If it does, can we use it? The target must be the same
         * as one of the children for OP_BINARY_COMMUT,
         * or the same as the first child for OP_BINARY_NONCOMMUT. */
        /* Also, short IDs only exist for binary operations, so
         * we don't need to check if they're binary. */
        int child_id = 0;
        list_for_each(&expr->children, child) {
            if (child->reg == out && child->reg->lock) {
                ignore_expr = child;
                break;
            }
            ++child_id;
        }
        if (ignore_expr) {
            if (child_id == 0 || expr->op->type == OP_BINARY_COMMUT)
                use_short = 1;
            else
                ignore_expr = NULL;
        }
    }

    int id = use_short ? ids->id_short : ids->id_normal;
    if (id == -1)
        return EXPR_ERR_NO_INSTR;

    /* Finally, we can output the instruction. */
    /* But... it may happen that with EXPR_ASSIGN+OP_ASSIGN,
     * the register is already what it should be, and outputting
     * this instr would lead to something like iset([10008], [10008]).
     * Obviously, we do NOT want that. So there's a failsafe here. */

    if (expr->type != EXPR_ASSIGN || expr->op->type != OP_ASSIGN ||
        out != ((expr_t*)list_tail(&expr->children))->reg) {

        list_t* params = list_new();
        list_append_new(params, reg_to_param(out));

        int cnt = 0;
        list_for_each(&expr->children, child) {
            if (expr->type == EXPR_ASSIGN && cnt++ == 0) {
                /* First child of EXPR_ASSIGN is the target, which
                 * was already added to the list - don't add it again as child. */
                continue;
            }
            if (child == ignore_expr)
                continue;
            list_append_new(params, child->param);
            child->param = NULL; /* Setting it to NULL to avoid freeing it twice.  */
        }
        thanm_instr_t* instr = instr_new(state, id, params);
        list_append_new(&state->current_script->instrs, instr);
    }

    /* Now, expr has to be simplified to EXPR_VAL that uses
     * the register as param. */
    expr->type = EXPR_VAL;
    expr->param = reg_to_param(out);
    expr->reg = out;

    /* Also, get rid of and free child expressions. */
    list_for_each(&expr->children, child) {
        if (child->reg && child->reg->lock == LOCK_EXPR && child->reg != out)
            reg_lock(child->reg, LOCK_UNLOCK);
        expr_free(child);
    }

    list_free_nodes(&expr->children);
    return EXPR_ERR_SUCCESS;
}

const char*
expr_strerror(
    expr_error_t err
) {
    static const char* const errs[] = {
        "success", /* EXPR_ERR_SUCCESS */
        "type mismatch", /* EXPR_ERR_BAD_TYPES */
        "invalid assignment - no register on the left-hand side", /* EXPR_ERR_BAD_LVAL */
        "no instruction found for given types", /* EXPR_ERR_NO_INSTR */
        "can't compile instructions outside of a script", /* EXPR_ERR_NO_SCRIPT */
        "no temporary registers available", /* EXPR_ERR_REG_FULL */
        "division or modulo by 0" /* EXPR_ERR_DIVIDE_BY_ZERO */
    };
    return errs[err];
}
