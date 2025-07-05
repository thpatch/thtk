/* This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 **/
#include <stdio.h> /* for fprintf, stderr, NULL */
#include <stdlib.h> /* for getenv */
#include <ctype.h> /* for isalnum */
#include <string.h> /* for strchr */
#include "mygetopt.h"

struct getopt_state {
    char *arg;
    int err, ind, opt;

    int argc;
    char *const *argv;
    const char *string;

    const char *shortopts;

    unsigned want_permute :1; /* not implemented */
    unsigned want_mixed :1; /* not implemented */
    unsigned want_err_colon :1;

    char *next;
};

static int getopt_internal(struct getopt_state *s)
{
    char *p;
    int rv;
    if (s->ind >= s->argc || !s->argv || !s->shortopts)
        return -1;
    if (s->next == NULL) {
        char *temp = s->argv[s->ind];
        if (temp == NULL)
            return -1;
        if (temp[0] != '-')
            return -1;
        if (temp[1] == '\0')
            return -1;
        if (temp[1] == '-' && temp[2] == '\0') {
            s->ind++;
            return -1;
        }
        s->next = temp+1;
    }

    s->opt = *s->next++;
    if (isalnum(s->opt) && (p = strchr(s->shortopts, s->opt))) {
        rv = s->opt;
        if (p[1] == ':') {
            if (*s->next) {
                /* argument of form -ffilename */
                s->arg = s->next;
                s->next = NULL;
                s->ind++;
            } else {
                /* argument of form -f filename */
                s->arg = s->argv[++s->ind]; /* optind will be increased second time later */
                if (!s->arg) {
                    if (s->err && !s->want_err_colon)
                        fprintf(stderr, "%s: Option -%c requires an operand\n", s->argv[0], s->opt);
                    rv = s->want_err_colon ? ':' : '?';
                }
            }
        }
    } else {
        if (s->err && !s->want_err_colon)
            fprintf(stderr, "%s: Unrecognized option: '-%c'\n", s->argv[0], s->opt);
        rv = '?';
    }

    if (s->next && *s->next == '\0') {
        s->next = NULL;
        s->ind++;
    }

    return rv;
}

static void getopt_parse_optstring(struct getopt_state *s, const char *optstring)
{
    s->string = optstring;
    s->want_permute = 0;
    s->want_mixed = 0;
    s->want_err_colon = 0;
    if (optstring) {
        if (*optstring == '-') {
            s->want_mixed = 1;
            optstring++;
        } else if (*optstring == '+') {
            optstring++;
        } else if (!getenv("POSIXLY_CORRECT")) {
            s->want_permute = 1;
        }

        if (*optstring == ':') {
            s->want_err_colon = 1;
            optstring++;
        }
    }
    s->shortopts = optstring;
}

char *optarg = NULL;
int opterr = 1, optind = 1, optopt = 0;
static struct getopt_state getopt_g_state = {0};
int getopt(int argc, char *const argv[], const char *optstring)
{
    struct getopt_state *s = &getopt_g_state;
    int rv;

    s->err = opterr;
    s->ind = optind;
    s->argc = argc;
    s->argv = argv;
    if (s->string != optstring || !optind)
        getopt_parse_optstring(s, optstring);
    if (!optind) {
        s->ind = 1;
        s->next = 0;
    }
    rv = getopt_internal(s);
    optarg = s->arg;
    optind = s->ind;
    optopt = s->opt;
    return rv;
}
