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
#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <config.h>

const char* util_shortname(
        const char* path);

void util_getopt_default(
    int *ind,
    char **argv,
    int opt,
    void (*usage)(void));

unsigned int parse_version(
    char *str);

extern const char* argv0;
extern const char* current_input;
extern const char* current_output;

/**
 * Definitions for argument parsing on Windows
 *
 * The goal is to add ability to access wide arguments (specifically,
 * for filenames,) while leaving the argument parsing code narrow.
 *
 * On Unix, everything is left as-is.
 */
#ifdef _WIN32
extern wchar_t **wargv;
#define main fakemain
int main(int argc, char **argv);

typedef wchar_t fnchar;
fnchar *mkfnchar(const char* str);
#define freefnchar(str) do{ free(str); } while(0)
#define SET_ARGV(_to,_from) do { \
    int to = (_to), from = (_from); \
    argv[to] = argv[from]; \
    wargv[to] = wargv[from]; \
} while(0)
#define fnargv wargv
#define thtk_io_open_file_fn(f,m,e) thtk_io_open_file_w(f,L##m,e)
#define thdat_detect_filename_fn thdat_detect_filename_w
#define thdat_detect_fn thdat_detect_w
#define fnfopen(f,m) _wfopen(f,L##m)
#define PRIfnc "%lc"
#define PRIfns "%ls"
#define FNCHAR(x) L##x
#else /* !_WIN32 */
typedef char fnchar;
#define mkfnchar(str) (str)
#define freefnchar(str) do{ ; } while(0)
#define SET_ARGV(to,from) do { argv[to] = argv[from]; } while(0)
#define fnargv argv
#define thtk_io_open_file_fn(f,m,e) thtk_io_open_file(f,m,e)
#define thdat_detect_filename_fn thdat_detect_filename
#define thdat_detect_fn thdat_detect
#define fnfopen(f,m) fopen(f,m)
#define PRIfnc "%c"
#define PRIfns "%s"
#define FNCHAR(x) x
#endif

#endif
