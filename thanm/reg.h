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

#ifndef REG_H_
#define REG_H_

#include "thanm.h"

typedef enum reg_lock_t {
   LOCK_UNLOCK, /* Register unlocked. */
   LOCK_EXPR, /* Register locked by expression evaluation. */
   LOCK_VAR, /* Register locked because it holds a user-defined variable. */
   LOCK_OTHER /* Register locked for non-standard reason (like user using it manually). */
} reg_lock_t;

typedef enum reg_purpose_t {
   PURPOSE_EXPR,
   PURPOSE_VAR,
   PURPOSE_OTHER
} reg_purpose_t;

struct reg_t {
   int id;
   int type; /* 'S' or 'f' */
   reg_lock_t lock;
   reg_purpose_t purpose;
};

/* Creates a new register and adds it to the list. */
reg_t* reg_new(int id, int type);

/* Frees all user-defined registers. */
void reg_free_user(void);

/* Unlocks all registers. */
void reg_reset(int ver);

/* Finds a suitable register, locks and returns it.
 * Returns NULL if no suitable registers were found. */
reg_t* reg_acquire(reg_purpose_t purpose, int type, int ver);

/* Returns a register with a given ID. */
reg_t* reg_get_by_id(int id, int ver);

/* Returns 1 if register with the given ID is locked, 0 otherwise. */
int reg_is_locked(int id, int ver);

/* Locks/unlocks a given register.
 * Returns 1 on error (when a register is locked with a different type than we want to lock it with). */
int reg_lock(reg_t* reg, reg_lock_t lock);

/* Locks/unlocks register with the given ID.
 * Returns 1 on error (when a register is locked with a different type than we want to lock it with).
 * Not finding the register is not treated as an error; as such, it returns 0 when that happens. */
int reg_lock_id(int id, reg_lock_t lock, int ver);

/* Returns a new parameter that references the register. */
thanm_param_t* reg_to_param(reg_t* reg);

 #endif
