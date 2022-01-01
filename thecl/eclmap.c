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

eclmap_t *
eclmap_new()
{
    eclmap_t *map = (eclmap_t*)malloc(sizeof(eclmap_t));
    if (map) {
        map->ins_names = seqmap_new();
        map->ins_signatures = seqmap_new();
        map->gvar_names = seqmap_new();
        map->gvar_types = seqmap_new();
        map->timeline_ins_names = seqmap_new();
        map->timeline_ins_signatures = seqmap_new();
        map->mnem_set = NULL;
        map->mnem_set_len = 0;
    }
    return map;
}

void
eclmap_free(
    eclmap_t *map)
{
    if (map) {
        seqmap_free(map->ins_names);
        seqmap_free(map->ins_signatures);
        seqmap_free(map->gvar_names);
        seqmap_free(map->gvar_types);
        seqmap_free(map->timeline_ins_names);
        seqmap_free(map->timeline_ins_signatures);
        free(map->mnem_set);
        free(map);
    }
}

typedef struct state_t {
    eclmap_t *emap;
    seqmap_t *smap;
    int ident;
    const char *fn;
} state_t;

static int
control(
    state_t *state,
    int linenum,
    const char *cline)
{
    if (!strcmp(cline, "!ins_names")) {
        state->smap = state->emap->ins_names;
        state->ident = 1;
    } else if (!strcmp(cline, "!ins_signatures")) {
        state->smap = state->emap->ins_signatures;
        state->ident = 0;
    } else if (!strcmp(cline, "!gvar_names")) {
        state->smap = state->emap->gvar_names;
        state->ident = 1;
    } else if (!strcmp(cline, "!gvar_types")) {
        state->smap = state->emap->gvar_types;
        state->ident = 0;
    } else if (!strcmp(cline, "!timeline_ins_names")) {
        state->smap = state->emap->timeline_ins_names;
        state->ident = 1;
    } else if (!strcmp(cline, "!timeline_ins_signatures")) {
        state->smap = state->emap->timeline_ins_signatures;
        state->ident = 0;
    } else {
        fprintf(stderr, "%s:%s:%u: unknown control line '%s'\n",argv0,state->fn,linenum,cline);
        return 1;
    }
    return 0;
}

static int
is_keyword(
    const char *value)
{
    static const char *keywords[] = {
        "anim", "ecli", "sub", "timeline",
        "var", "int", "float", "void",
        "inline", "return", "goto", "unless",
        "if", "else", "do", "while",
        "times", "switch", "case", "default",
        "break", "async", "global", "sin",
        "cos", "sqrt", "rad", "false", "true",
        NULL
    };
    const char **kwp = keywords;
    while (*kwp) {
        if (!strcmp(value, *kwp++)) {
            return 1;
        }
    }
    return 0;
}

static int
validate_ident(
    state_t *state,
    int linenum,
    const char *value)
{
    const char *ptr = value;
    if (ptr[0] >= '0' && ptr[0] <= '9' || ptr[0] == '\0') { /* first character must not be digit */
        fprintf(stderr, "%s:%s:%u: '%s' isn't valid identifier\n", argv0, state->fn, linenum, value);
        return 1;
    }
    while (*ptr) {
        if (!(*ptr >= '0' && *ptr <='9' || *ptr >= 'a' && *ptr <= 'z' || *ptr >= 'A' && *ptr <= 'Z' || *ptr == '_')) {
            break;
        }
        ptr++;
    }
    if (*ptr) {
        fprintf(stderr, "%s:%s:%u: '%s' isn't valid identifier\n",argv0, state->fn, linenum, value);
        return 1;
    }
    if (!util_strcmp_ref(value, stringref("ins_"))) {
        fprintf(stderr, "%s:%s:%u: mnemonic can't start with 'ins_'\n",argv0, state->fn, linenum);
        return 1;
    } else if (is_keyword(value)) {
        fprintf(stderr, "%s:%s:%u: '%s' is a keyword, ignoring\n", argv0, state->fn, linenum, value);
        return 1;
    }
    return 0;
}

static int
validate_type(
    state_t *state,
    int linenum,
    const char *ptr)
{
    if (ptr[0] != '$' && ptr[0] != '%' || ptr[0] && ptr[1]) {
        fprintf(stderr, "%s:%s:%u: unknown type '%s'\n", argv0, state->fn, linenum, ptr);
        return 1;
    }
    return 0;
}

static int
validate_signature(
    state_t *state,
    int linenum,
    const char *value)
{
    (void)value;
    static int warned = 0;
    if (!warned && ++warned)
        fprintf(stderr, "%s:%s:%u: warning: signature validation is not yet implemented\n", argv0, state->fn, linenum);
    return 0;
}

static int
set(
    state_t *state,
    int linenum,
    const seqmap_entry_t* ent)
{
    if (state->ident) {
        if (validate_ident(state, linenum, ent->value))
            return 1;
    } else if (state->smap == state->emap->gvar_types) {
        if (validate_type(state, linenum, ent->value))
            return 1;
    } else {
        if (validate_signature(state, linenum, ent->value))
            return 1;
    }
    seqmap_set(state->smap, ent);
    return 0;
}

void
eclmap_load(
    unsigned int version,
    eclmap_t* emap,
    FILE* f,
    const char* fn)
{
    (void)version;
    state_t state;
    state.emap = emap;
    state.fn = fn;
    control(&state, 0, "!ins_names"); // default section
    seqmap_load("!eclmap", &state, (seqmap_setfunc_t)set, (seqmap_controlfunc_t)control, f, fn);
}

int
strcmp_indirect(
    const void *lhs,
    const void *rhs)
{
    return strcmp(*(const char **)lhs, *(const char **)rhs);
}

void
eclmap_rebuild(
    eclmap_t *emap)
{
    char **p;
    size_t count = 0;
    seqmap_entry_t *ent;
    list_for_each(emap->ins_names, ent)
        ++count;
    list_for_each(emap->timeline_ins_names, ent)
        ++count;

    free(emap->mnem_set);
    emap->mnem_set = p = malloc(count * sizeof(char*));
    emap->mnem_set_len = count;

    list_for_each(emap->ins_names, ent)
        *p++ = ent->value;
    list_for_each(emap->timeline_ins_names, ent)
        *p++ = ent->value;

    qsort(emap->mnem_set, emap->mnem_set_len, sizeof(char*), strcmp_indirect);
}

int
eclmap_is_mnemonic(
    eclmap_t *emap,
    const char *mnem)
{
    return !!bsearch(&mnem, emap->mnem_set, emap->mnem_set_len, sizeof(char*), strcmp_indirect);
}
