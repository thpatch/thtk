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

#ifndef ECLMAP_H_
#define ECLMAP_H_

#include <config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "list.h"

typedef struct eclmap_entry_t {
    int opcode;
    char* signature;
    char* mnemonic;
} eclmap_entry_t;

typedef list_t eclmap_t;

/* Allocates and initalizes a new eclmap */
#define eclmap_new() ((eclmap_t*)list_new())
/* Frees an eclmap */
void eclmap_free(eclmap_t* map);
/* Sets an entry in a eclmap */
void eclmap_set(eclmap_t* map, const eclmap_entry_t* ent);
/* Finds an entry in a eclmap by opcode */
eclmap_entry_t* eclmap_get(eclmap_t* map, int opcode);
/* Finds an entry in a eclmap by mnemonic */
eclmap_entry_t* eclmap_find(eclmap_t* map, const char* mnemonic);
/* Loads entries from eclmap file (thread unsafe) */
void eclmap_load(eclmap_t* opcodes, eclmap_t* globals, FILE* f, const char* fn);

#endif
