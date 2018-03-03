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
#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "program.h"
#include "util.h"
#include "mygetopt.h"

const char* argv0 = NULL;
const char* current_input = NULL;
const char* current_output = NULL;

/* Returns a pointer to after the last directory separator in path. */
/* TODO: Use util_basename if it can be made to return a pointer to inside of
 * path. */
const char*
util_shortname(
    const char* path)
{
    const char* ret;
    if (!path) {
        fprintf(stderr, "%s: NULL path passed to short_name\n", argv0);
        abort();
    }
#ifdef WIN32
    ret = MAX(strrchr(path, '/'), strrchr(path, '\\'));
#else
    ret = strrchr(path, '/');
#endif
    return ret ? ret + 1 : path;
}

void
util_getopt_default(
    int *ind,
    char **argv,
    int opt,
    void (*usage)(void))
{
    switch(opt) {
    case 'V':
        util_print_version();
        exit(0);
    case ':':
        fprintf(stderr,"%s: Missing required argument for option '%c'\n",argv0,util_optopt);
        usage();
        exit(1);
    case '?':
        fprintf(stderr,"%s: Unknown option '%c'\n",argv0,util_optopt);
        usage();
        exit(1);
    case -1:
        if(!strcmp(argv[util_optind-1], "--")) {
            while(argv[util_optind]) {
                argv[(*ind)++] = argv[util_optind++];
            }
        }
        else {
            argv[(*ind)++] = argv[util_optind++];
        }
        break;
    }
}
