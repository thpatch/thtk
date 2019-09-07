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
#include <string.h>
#include <stdlib.h>
#include "thecl.h"
#include "path.h"

char*
path_get_full(
    parser_state_t* state,
    char* path)
{
    char* current_dir = state->path_stack[state->path_cnt - 1];
    char* ret;

    if (current_dir[0] != '\0') {
        ret = malloc(strlen(current_dir) + strlen(path) + 2);
        strcpy(ret, current_dir);
        strcat(ret, "/");
        strcat(ret, path);
    } else {
        ret = malloc(strlen(path) + 1);
        strcpy(ret, path);
    }

    return ret;
}

void
path_add(
    parser_state_t* state,
    char* path)
{
    char* dir_path;
    char* tmp = strrchr(path, '\\');
    char* tmp2 = strrchr(path, '/');
    if (tmp != NULL || tmp2 != NULL) {
        /* Since these can be mixed, we need to get the later one.
           They are pointing somewhere in the same string (or one of them is NULL),
           so it's best to just compare them */
        if (tmp2 > tmp) tmp = tmp2;

        size_t len = strlen(path) - strlen(tmp);
        dir_path = malloc(len + 1);
        strncpy(dir_path, path, len);
        dir_path[len] = '\0';
    }
    else {
        dir_path = malloc(sizeof(""));
        strcpy(dir_path, "");
    }

    ++state->path_cnt;
    state->path_stack = realloc(state->path_stack, sizeof(char*) * state->path_cnt);
    state->path_stack[state->path_cnt - 1] = dir_path;
}

void
path_remove(
    parser_state_t* state)
{
    --state->path_cnt;
    free(state->path_stack[state->path_cnt]);
}
