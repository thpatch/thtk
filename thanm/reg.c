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
#include "thanm.h"
#include "reg.h"

static reg_t
regs_v8[] = {
   /* ID     TYPE       LOCK          PURPOSE */
   { 10000,   'S',      LOCK_UNLOCK,  PURPOSE_EXPR },
   { 10001,   'S',      LOCK_UNLOCK,  PURPOSE_EXPR },
   { 10002,   'S',      LOCK_UNLOCK,  PURPOSE_EXPR },
   { 10003,   'S',      LOCK_UNLOCK,  PURPOSE_EXPR },
   { 10008,   'S',      LOCK_UNLOCK,  PURPOSE_VAR  },
   { 10009,   'S',      LOCK_UNLOCK,  PURPOSE_VAR  },

   { 10004,   'f',      LOCK_UNLOCK,  PURPOSE_EXPR },
   { 10005,   'f',      LOCK_UNLOCK,  PURPOSE_EXPR },
   { 10006,   'f',      LOCK_UNLOCK,  PURPOSE_EXPR },
   { 10007,   'f',      LOCK_UNLOCK,  PURPOSE_EXPR },
   /* TODO: research more registers and add them if they're general purpose.
    * (and also add registers for other versions) */

   /* Sentinel */
   { 0, 0, 0, 0 }
};

static list_t
regs_user = { NULL, NULL };

reg_t*
reg_new(
    int id,
    int type
) {
    reg_t* reg = malloc(sizeof(reg_t));
    reg->id = id;
    reg->type = type;
    reg->lock = LOCK_OTHER;
    reg->purpose = PURPOSE_OTHER;
    list_append_new(&regs_user, reg);
    return reg;
}

void
reg_free_user() {
    reg_t* reg;
    list_for_each(&regs_user, reg) {
        free(reg);
    }
    list_free_nodes(&regs_user);
}

static reg_t*
reg_get_table(
    int ver
) {
    reg_t* ret = NULL;
    switch (ver) {
    case 8:
        ret = regs_v8;
        break;
    }
    return ret;
}

void
reg_reset(
    int ver
) {
    reg_t* table = reg_get_table(ver);
    if (table) {
        while(table->type) {
            table->lock = LOCK_UNLOCK;
            ++table;
        }
    }
}

static reg_t*
reg_acquire_purpose(
    reg_purpose_t purpose,
    int type,
    reg_t* table
) {
    while(table->type) {
        if (
            table->purpose == purpose &&
            table->type == type &&
            table->lock == LOCK_UNLOCK
        ) {
            return table;
        }
        ++table;
    }
    return NULL;
}

reg_t*
reg_acquire(
   reg_purpose_t purpose,
   int type,
   int ver
) {
    reg_t* table = reg_get_table(ver);
    reg_t* ret = reg_acquire_purpose(purpose, type, table);
    if (ret == NULL) {
        /* Try to use a register that's usually for other things. */
        if (purpose == PURPOSE_EXPR)
            ret = reg_acquire_purpose(PURPOSE_VAR, type, table);
        else if (purpose == PURPOSE_VAR)
            ret = reg_acquire_purpose(PURPOSE_EXPR, type, table);
    }

    if (ret) {
        if (purpose == PURPOSE_EXPR)
            ret->lock = LOCK_EXPR;
        else if (purpose == PURPOSE_VAR)
            ret->lock = LOCK_VAR;
        else
            ret->lock = LOCK_OTHER;
    }

    return ret;
}

reg_t*
reg_get_by_id(
    int id,
    int ver
) {
    reg_t* table = reg_get_table(ver);
    if (table) {
        while(table->type) {
            if (table->id == id)
                return table;
            ++table;
        }
    }
    list_for_each(&regs_user, table) {
        if (table->id == id)
            return table;
    }
    return NULL;
}

int
reg_is_locked(
    int id,
    int ver
) {
    reg_t* reg = reg_get_by_id(id, ver);
    if (reg == NULL)
        return 0;
    return reg->lock != LOCK_UNLOCK;
}

int
reg_lock(
    reg_t* reg,
    reg_lock_t lock
) {
    if (lock == LOCK_UNLOCK || reg->lock == LOCK_UNLOCK || lock == reg->lock) {
        reg->lock = lock;
        return 0;
    }
    return 1;
}

int
reg_lock_id(
    int id,
    reg_lock_t lock,
    int ver
) {
    reg_t* reg = reg_get_by_id(id, ver);
    if (reg == NULL)
        return 0;

    return reg_lock(reg, lock);
}

thanm_param_t*
reg_to_param(
    reg_t* reg
) {
    thanm_param_t* param = thanm_param_new(reg->type);
    param->is_var = 1;
    param->val = malloc(sizeof(*param->val));
    param->val->type = reg->type;
    if (reg->type == 'S')
        param->val->val.S = reg->id;
    else
        param->val->val.f = (float)reg->id;
    return param;
}
