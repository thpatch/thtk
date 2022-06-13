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
#include <ctype.h>
#include "program.h"
#include "util.h"
#include "mygetopt.h"

const char* argv0 = NULL;
const char* current_input = NULL;
const char* current_output = NULL;

/* Returns a pointer to after the last directory separator in path. */
/* TODO: Use util_basename if it can be made to return a pointer to inside of
 * path. */
const char *
util_shortname(
    const char *path)
{
    const char *ret;
    if (!path) {
        fprintf(stderr, "%s: NULL path passed to short_name\n", argv0);
        abort();
    }
    ret = strrchr(path, '/');
#ifdef _WIN32
    {
        const char *p = strrchr(path, '\\');
        ret = MAX(ret, p);
    }
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

unsigned int parse_version(char *str) {
    struct version_abbr {
        unsigned int version;
        char name[6];
    };
    static const struct version_abbr vers[] = {
        {1, "hrtp"},
        {2, "soew"},
        {3, "podd"},
        {4, "lls"},
        {5, "ms"},
        {6, "eosd"},
        {7, "pcb"},
        {75, "iamp"},
        {8, "in"},
        {9, "pofv"},
        {95, "stb"},
        {10, "mof"},
        {103, "alco"},
        {105, "swr"},
        {11, "sa"},
        {12, "ufo"},
        {123, "soku"},
        {125, "ds"},
        {128, "fw"},
        {13, "td"},
        {135, "hm"},
        {14, "ddc"},
        {143, "isc"},
        {145, "ulil"},
        {15, "lolk"},
        {155, "aocf"},
        {16, "hsifs"},
        {165, "vd"},
        {17, "wbawc"},
        {18, "um"},
        /* NEWHU: 18 */
        {0}
    };
    const struct version_abbr *vp = vers;

    if(!str) return 0;
    unsigned int version = strtoul(str, NULL, 10);
    if(version) return version;

    char *str2 = str;
    for(;*str2;str2++) *str2 = tolower(*str2);

    while(vp->version) {
        if(!strcmp(str,vp->name)) break;
        vp++;
    }
    return vp->version;
}
