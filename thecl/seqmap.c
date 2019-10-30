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
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "seqmap.h"
#include "program.h"
#include "util.h"

void
seqmap_free(
    seqmap_t* map)
{
    seqmap_entry_t* ent;

    if (!map) return;

    list_for_each(map, ent) {
        if (ent && ent->flags & SEQMAP_FLAG_ALLOC) {
            if (ent) {
                free(ent->value);
            }
            free(ent);
        }
    }

    list_free_nodes(map);

    free(map);
}

/* Allocates new seqmap entry, appends it to a seqmap, and returns it */
static seqmap_entry_t*
seqmap_append_new(
    seqmap_t* map,
    int key)
{
    seqmap_entry_t* ent = malloc(sizeof(seqmap_entry_t));
    ent->key = key;
    ent->value = NULL;
    ent->flags = SEQMAP_FLAG_ALLOC;
    list_append_new(map, ent);
    return ent;
}

void
seqmap_set(
    seqmap_t* map,
    const seqmap_entry_t* ent1)
{
    seqmap_entry_t* ent2;
    list_for_each(map, ent2) {
        if (ent2 && ent2->key == ent1->key) {
            break;
        }
    }

    if (!ent2) {
        ent2 = seqmap_append_new(map, ent1->key);
    }

    free(ent2->value);
    ent2->value = ent1->value ? strdup(ent1->value) : NULL;
}

seqmap_entry_t*
seqmap_get(
    seqmap_t* map,
    int key)
{
    seqmap_entry_t* ent;
    list_for_each(map, ent) {
        if (ent && ent->key == key) {
            break;
        }
    }
    return ent;
}

seqmap_entry_t*
seqmap_find(
    seqmap_t* map,
    const char* value)
{
    seqmap_entry_t* ent;
    list_for_each(map, ent) {
        if (ent && ent->value && !strcmp(ent->value, value)) {
            break;
        }
    }
    return ent;
}

void
seqmap_load(
    const char* magic,
    void *state,
    seqmap_setfunc_t set,
    seqmap_controlfunc_t control,
    FILE* f,
    const char* fn)
{
    static char buffer[512];

    if (!fgets(buffer, sizeof(buffer), f)) {
        fprintf(stderr, "%s:%s: couldn't read the first line\n",argv0,fn);
        return;
    }

    if (strncmp(buffer, magic, strlen(magic))) {
        fprintf(stderr, "%s:%s:1: invalid magic\n",argv0,fn);
        return;
    }

    int linecount = 1;
    while (fgets(buffer, sizeof(buffer), f)) {
        char *ptr, *ptrend;
        seqmap_entry_t ent;

        ++linecount;

        /* remove the comments */
        ptr = strchr(buffer, '#');
        if (ptr)
            *ptr = '\0';

        /* parse key */
        ptr = strtok(buffer, " \t\r\n");
        if (!ptr)
            continue; /* 0 tokens -> empty line, no need to error */

        if (ptr[0] == '!') {
            control(state, linecount, ptr);
            continue;
        }

        errno = 0;
        ent.key = strtol(ptr, &ptrend, 0);
        if (ptr == ptrend || errno != 0) {
            fprintf(stderr, "%s:%s:%u: key token is not a number\n",argv0,fn,linecount);
            continue;
        }

        /* parse value */
        ptr = strtok(NULL, " \t\r\n");
        if (!ptr) {
            fprintf(stderr, "%s:%s:%u: not enough tokens\n",argv0,fn,linecount);
            continue;
        }

        /* validate value */
        if (ptr[0] == '_' && ptr[1] == '\0') {
            ptr[0] = '\0'; /* allow empty strings to be specified with "_" */
        }
        ent.value = ptr;

        set(state, linecount, &ent);
    }
}
