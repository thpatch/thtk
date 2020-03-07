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
#include <string.h>
#include "anmmap.h"
#include "program.h"
#include "util.h"

anmmap_t*
anmmap_new() {
    anmmap_t* map = (anmmap_t*)malloc(sizeof(anmmap_t));
    if (map) {
        map->ins_names = seqmap_new();
        map->gvar_names = seqmap_new();
        map->gvar_types = seqmap_new();
    }
    return map;
}

void
anmmap_free(
    anmmap_t* map
) {
    if (map) {
        seqmap_free(map->ins_names);
        seqmap_free(map->gvar_names);
        seqmap_free(map->gvar_types);
        free(map);
    }
}

typedef struct state_t {
    anmmap_t* amap;
    seqmap_t* smap;
    int ident;
    const char* fn;
} state_t;

static int
control(
    state_t* state,
    int linenum,
    const char* cline
) {
    if (!strcmp(cline, "!ins_names")) {
        state->smap = state->amap->ins_names;
        state->ident = 1;
    } else if (!strcmp(cline, "!gvar_names")) {
        state->smap = state->amap->gvar_names;
        state->ident = 1;
    } else if (!strcmp(cline, "!gvar_types")) {
        state->smap = state->amap->gvar_types;
        state->ident = 0;
    } else {
        fprintf(stderr, "%s:%s:%u: unknown control line '%s'\n", argv0, state->fn, linenum, cline);
        return 1;
    }
    return 0;
}

static int
is_keyword(
    const char* value)
{
    static const char* keywords[] = {
        "script", "entry", "global",
        "timeof", "offsetof", "scriptof", "spriteof",
        NULL
    };
    const char** kwp = keywords;
    while (*kwp) {
        if (!strcmp(value, *kwp++)) {
            return 1;
        }
    }
    return 0;
}

static int
validate_ident(
    state_t* state,
    int linenum,
    const char* value)
{
    const char* ptr = value;
    if (ptr[0] >= '0' && ptr[0] <= '9') { /* first character must not be digit */
        fprintf(stderr, "%s:%s:%u: '%s' isn't valid identifier\n", argv0, state->fn, linenum, value);
        return 1;
    }
    while (*ptr) {
        if (!(*ptr >= '0' && *ptr <= '9' || *ptr >= 'a' && *ptr <= 'z' || *ptr >= 'A' && *ptr <= 'Z' || *ptr == '_')) {
            break;
        }
        ptr++;
    }
    if (*ptr) {
        fprintf(stderr, "%s:%s:%u: '%s' isn't valid identifier\n", argv0, state->fn, linenum, value);
        return 1;
    }
    if (!util_strcmp_ref(value, stringref("ins_"))) {
        fprintf(stderr, "%s:%s:%u: mnemonic can't start with 'ins_'\n", argv0, state->fn, linenum);
        return 1;
    }
    else if (is_keyword(value)) {
        fprintf(stderr, "%s:%s:%u: '%s' is a keyword, ignoring\n", argv0, state->fn, linenum, value);
        return 1;
    }
    return 0;
}

static int
validate_type(
    state_t* state,
    int linenum,
    const char* ptr)
{
    if (ptr[0] != '$' && ptr[0] != '%' || ptr[0] && ptr[1]) {
        fprintf(stderr, "%s:%s:%u: unknown type '%s'\n", argv0, state->fn, linenum, ptr);
        return 1;
    }
    return 0;
}

static int
set(
    state_t* state,
    int linenum,
    const seqmap_entry_t* ent
) {
    if (state->ident) {
        if (validate_ident(state, linenum, ent->value))
            return 1;
    } else if (state->smap == state->amap->gvar_types) {
        if (validate_type(state, linenum, ent->value))
            return 1;
    }
    seqmap_set(state->smap, ent);
    return 0;
}

void
anmmap_load(
    anmmap_t* map,
    FILE* file,
    const char* fn
) {
    state_t state;
    state.amap = map;
    state.fn = fn;
    control(&state, 0, "!ins_names");
    seqmap_load("!anmmap", &state, (seqmap_setfunc_t)set, (seqmap_controlfunc_t)control, file, fn);
}
