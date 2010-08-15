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
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "program.h"
#include "util.h"

char
parse_args(int argc, char* argv[], void (*usage)(void), const char* commands,
    char* options, unsigned int* version)
{
    char command;
    char* argp;

    /* TODO: Some kind of check here first. */
    argv0 = util_shortname(argv[0]);

    if (argc < 2) {
        usage();
        return 0;
    }

    command = argv[1][0];

    if (!command) {
        fprintf(stderr, "%s: command missing\n", argv0);
        return 0;
    }

    if (!strchr(commands, command)) {
        fprintf(stderr, "%s: unknown command '%c'\n", argv0, command);
        usage();
        return 0;
    }

    for (argp = argv[1] + 1; *argp;) {
        char* optionp = strchr(options, *argp);
        if (optionp) {
            *optionp = ' ';
            argp++;
        } else {
            long int parsed_version = strtol(argp, &argp, 10);
            if (!version || parsed_version == 0 || parsed_version == LONG_MIN
                || parsed_version == LONG_MAX) {
                fprintf(stderr, "%s: unrecognized option '%c'\n", argv0, *argp);
                usage();
                return 0;
            } else
                *version = parsed_version;
        }
    }

    return command;
}
