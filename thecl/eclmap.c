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
#include "eclmap.h"
#include "program.h"
#include "util.h"

static void
eclmap_dump(
    eclmap_t* map)
{
    eclmap_entry_t* ent;
    list_for_each(map, ent) {
        printf("%d %s %s\n",
            ent->opcode,
            ent->signature ? ent->signature : "?",
            ent->mnemonic ? ent->mnemonic : "?");
    }
}

void
eclmap_free(
    eclmap_t* map)
{
    eclmap_entry_t* ent;

    if(!map) return;

    list_for_each(map, ent) {
        if(ent) {
            free(ent->signature);
            free(ent->mnemonic);
        }
        free(ent);
    }

    list_free_nodes(map);

    free(map);
}

/* Allocates new eclmap entry, appends it to a eclmap, and returns it */
static eclmap_entry_t*
eclmap_append_new(
    eclmap_t* map,
    int opcode)
{
    eclmap_entry_t* ent = malloc(sizeof(eclmap_entry_t));
    ent->opcode = opcode;
    ent->signature = NULL;
    ent->mnemonic = NULL;
    list_append_new(map, ent);
    return ent;
}

void
eclmap_set(
    eclmap_t* map,
    const eclmap_entry_t* ent1)
{
    eclmap_entry_t* ent2;
    list_for_each(map, ent2) {
        if(ent2 && ent2->opcode == ent1->opcode) {
            break;
        }
    }

    if(!ent2) {
        ent2 = eclmap_append_new(map, ent1->opcode);
    }

    if(ent1->signature) {
        free(ent2->signature);
        ent2->signature = strdup(ent1->signature);
    }

    if(ent1->mnemonic) {
        free(ent2->mnemonic);
        ent2->mnemonic = strdup(ent1->mnemonic);
    }
}

eclmap_entry_t*
eclmap_get(
    eclmap_t* map,
    int opcode)
{
    eclmap_entry_t* ent;
    list_for_each(map, ent) {
        if(ent && ent->opcode == opcode) {
            break;
        }
    }
    return ent;
}

eclmap_entry_t*
eclmap_find(
    eclmap_t* map,
    const char* mnemonic)
{
    eclmap_entry_t* ent;
    list_for_each(map, ent) {
        if(ent && ent->mnemonic && !strcmp(ent->mnemonic, mnemonic)) {
            break;
        }
    }
    return ent;
}

void
eclmap_load(
    eclmap_t* opcodes,
    eclmap_t* timeline_opcodes,
    eclmap_t* globals,
    FILE* f,
    const char* fn)
{
    static char buffer[512];

    /* TODO: maybe do proper parsing with lex/yacc? */

    if(!fgets(buffer, sizeof(buffer), f)) {
        fprintf(stderr, "%s:%s: couldn't read the first line\n",argv0,fn);
        return;
    }

    if(util_strcmp_ref(buffer, stringref("eclmap"))) {
        fprintf(stderr, "%s:%s:1: invalid magic\n",argv0,fn);
        return;
    }

    int linecount = 1;
    while(fgets(buffer, sizeof(buffer), f)) {
        int ent_type = ECLMAP_ENT_INVALID;
      
        char *ptr, *ptrend;
        eclmap_entry_t ent;

        ++linecount;

        /* remove the comments */
        ptr = strchr(buffer, '#');
        if(ptr) *ptr = '\0';

        /* parse opcode */
        ptr = strtok(buffer, " \t\r\n");
        if(!ptr) continue; /* 0 tokens -> empty line, no need to error */

        errno = 0;
        ent.opcode = strtol(ptr, &ptrend, 0);
        if(ptr == ptrend || errno != 0) {
            fprintf(stderr, "%s:%s:%u: opcode token is not a number\n",argv0,fn,linecount);
            continue;
        }

        /* parse signature */
        ptr = strtok(NULL, " \t\r\n");
        if(!ptr) {
            fprintf(stderr, "%s:%s:%u: not enough tokens\n",argv0,fn,linecount);
            continue;
        }

        /* validate signature */
        if(ptr[0] == '?') {
            ent.signature = NULL;
            ent_type = ECLMAP_ENT_OPCODE;
        } else if (ptr[0] == '@') {
            ent.signature = NULL;
            ent_type = ECLMAP_ENT_TIMELINE_OPCODE;
        } else if(ptr[0] == '$' || ptr[0] == '%') {
            ent.signature = ptr;
            ent_type = ECLMAP_ENT_GLOBAL;
        } else {
            ent.signature = ptr;
            if(ptr[0] == '_') ptr[0] = '\0'; /* allow empty strings to be specified with "_" */
            /* TODO: validate signature */
            fprintf(stderr, "%s:%s:%u: warning: signature mapping is not yet implemented\n",argv0,fn,linecount);
        }

        /* parse mnemonic */
        ptr = strtok(NULL, " \t\r\n");
        if(!ptr) {
            fprintf(stderr, "%s:%s:%u: not enough tokens\n",argv0,fn,linecount);
            continue;
        }

        /* validate mnemonic */
        if(ptr[0] == '?' || ptr[0] == '@') {
            ent.mnemonic = NULL;
        }
        else {
            ent.mnemonic = ptr;
            if(ptr[0] >= '0' && ptr[0] <= '9') { /* first character must not be digit */
                fprintf(stderr, "%s:%s:%u: '%s' isn't valid identifier\n",argv0,fn,linecount,ent.mnemonic);
                continue;
            }
            while(*ptr) {
                if(!(*ptr >= '0' && *ptr <='9' || *ptr >= 'a' && *ptr <= 'z' || *ptr >= 'A' && *ptr <= 'Z' || *ptr == '_')) {
                    break;
                }
                ptr++;
            }
            if(*ptr) {
                fprintf(stderr, "%s:%s:%u: '%s' isn't valid identifier\n",argv0,fn,linecount,ent.mnemonic);
                continue;
            }
            if(!util_strcmp_ref(ent.mnemonic, stringref("ins_"))) {
                fprintf(stderr, "%s:%s:%u: mnemonic can't start with 'ins_'\n",argv0,fn,linecount);
            }
        }

        eclmap_t* dest;
        switch(ent_type) {
            case ECLMAP_ENT_INVALID:
                continue;
            case ECLMAP_ENT_OPCODE:
                dest = opcodes;
                break;
            case ECLMAP_ENT_TIMELINE_OPCODE:
                dest = timeline_opcodes;
                break;
            case ECLMAP_ENT_GLOBAL:
                dest = globals;
        }

        eclmap_set(dest, &ent);
    }
}
