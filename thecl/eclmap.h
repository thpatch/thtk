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
#include "seqmap.h"

typedef struct eclmap_t {
    seqmap_t *ins_names;
    seqmap_t *ins_signatures;
    seqmap_t* ins_rets;
    seqmap_t *gvar_names;
    seqmap_t *gvar_types;
    seqmap_t *timeline_ins_names;
    seqmap_t *timeline_ins_signatures;
    char **mnem_set;
    size_t mnem_set_len;
} eclmap_t;

/* Allocates and initalizes a new eclmap */
eclmap_t* eclmap_new();
/* Frees an eclmap */
void eclmap_free(eclmap_t* map);
/* Loads entries from eclmap file (thread unsafe) */
void eclmap_load(unsigned int version, eclmap_t* emap, FILE* f, const char* fn);
/* Rebuilds mnemonic set. Do this after adding entries to mnemonic maps. */
void eclmap_rebuild(eclmap_t *emap);
/* Returns whether identifier is a mnemonic */
int eclmap_is_mnemonic(eclmap_t *emap, const char *mnem);

#endif
