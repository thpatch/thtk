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
                SET_ARGV((*ind)++, util_optind++);
            }
        }
        else {
            SET_ARGV((*ind)++, util_optind++);
        }
        break;
    }
}

unsigned int parse_version(char *str) {
    struct {
        unsigned int version;
        char name[6];
    } static const vers[] = {
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
        {0}
    }, *vp = vers;

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

#ifdef _WIN32
// I don't want to deal with improper UTF-16, so surrogate codepoints are left as-is.
static size_t utf8len(wchar_t *str) {
    size_t sz = 0;
    do {
        if (*str & 0xF800) sz++;
        if (*str & 0xFF80) sz++;
        sz++;
    } while (*str++);
    return sz;
}
static void utf8cpy(char *out, wchar_t *str) {
    do {
        if (*str & 0xF800) {
            *out++ = 0xE0 | 0x0F & (*str >> 12);
            *out++ = 0x80 | 0x3F & (*str >> 6);
            *out++ = 0x80 | 0x3F & (*str);
        }
        else if (*str & 0xFF80) {
            *out++ = 0xC0 | 0x1F & (*str >> 6);
            *out++ = 0x80 | 0x3F & (*str);
        }
        else {
            *out++ = (char)*str;
        }
    } while (*str++);
}
#define UTF8CPLEN(init) (\
    ((init)&0x80) == 0x00 ? 1 : \
    ((init)&0xE0) == 0xC0 ? 2 : \
    ((init)&0xF0) == 0xE0 ? 3 : 4)
fnchar *mkfnchar(const char* str) {
    int len = 0;
    for(;;) {
        int x = UTF8CPLEN(*str);
        len += x;
        str += x;
        if (!*(str - x)) break;
    }
    str -= len; // rewind
    wchar_t *wcs = malloc(sizeof(wchar_t)*len);
    for (int i = 0; i < len; i++) {
        switch (UTF8CPLEN(*str)) {
        case 1:
            *wcs++ = (wchar_t)str[0];
            str += 1;
            break;
        case 2:
            *wcs++ = (wchar_t)(0x7FF & (str[0] << 6 | str[1] & 0x3f));
            str += 2;
            break;
        case 3:
            *wcs++ = (wchar_t)(0xFFFF & (str[0] << 12 | (str[1] & 0x3f) << 6 | str[2] & 0x3f));
            str += 3;
            break;
        default: abort();
        }
    }
    return wcs;
}
#pragma comment( linker, "/entry:\"wmainCRTStartup\"" )
wchar_t **wargv = NULL;
int wmain(int argc, wchar_t **_wargv) {
    wargv = _wargv;

    char **argv = malloc(sizeof(char*) * (argc + 1));
    argv[argc] = NULL;

    for (int i = 0; i < argc; i++) {
        int len = utf8len(wargv[i]);
        argv[i] = malloc(len);
        utf8cpy(argv[i], wargv[i]);
    }

    int rv = main(argc, argv);
    return rv;
}
#endif
