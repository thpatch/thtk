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

#ifndef SEQMAP_H_
#define SEQMAP_H_

#include <config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "list.h"

#define SEQMAP_FLAG_ALLOC (1<<0)
typedef struct seqmap_entry_t {
    int key;
    char *value;
    int flags;
} seqmap_entry_t;

typedef list_t seqmap_t;

typedef int (*seqmap_setfunc_t)(
    void *state,
    int linenum,
    const seqmap_entry_t *ent);
typedef int (*seqmap_controlfunc_t)(
    void *state,
    int linenum,
    const char *cline);

/* Allocates and initalizes a new seqmap */
#define seqmap_new() ((seqmap_t*)list_new())
/* Frees a seqmap */
void seqmap_free(seqmap_t* map);
/* Sets an entry in a seqmap */
void seqmap_set(seqmap_t* map, const seqmap_entry_t* ent);
/* Finds an entry in a seqmap by key */
seqmap_entry_t* seqmap_get(seqmap_t* map, int key);
/* Finds an entry in a seqmap by value */
seqmap_entry_t* seqmap_find(seqmap_t* map, const char* value);
/* Loads entries from seqmap file (thread unsafe) */
void seqmap_load(const char* magic, void* state, seqmap_setfunc_t set, seqmap_controlfunc_t control, FILE* f, const char* fn);

#endif
