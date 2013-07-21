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
#include <errno.h>
#include <string.h>
#include "program.h"
#include "thmsg.h"
#include "util.h"

int thmsg_opt_end = 0;

extern const thmsg_module_t th06_msg;
extern const thmsg_module_t th95_msg;

static void
print_usage(void)
{
    printf("Usage: %s COMMAND[OPTION...] [INPUT [OUTPUT]]\n"
           "COMMAND can be:\n"
           "  c  create a dialogue file\n"
           "  d  dump a dialogue file\n"
           "  V  display version information and exit\n"
           "OPTION can be:\n"
           "  e  extract or create ending dialogue\n"
           "  #  # can be 6, 7, 8, 9, 95, 10, 11, 12, 125, 128, 13, 14 (required)\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

int
main(int argc, char* argv[])
{
    FILE* in = stdin;
    FILE* out = stdout;
    unsigned int version = 0;
    int mode;
    const thmsg_module_t* module = NULL;
    char options[] = "e";

    current_input = "(stdin)";
    current_output = "(stdout)";

    mode = parse_args(argc, argv, print_usage, "cdV", options, &version);

    if (!mode)
        return 1;

    if (!strchr(options, 'e'))
        thmsg_opt_end = 1;

    switch (version) {
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 128:
    case 13:
    case 14:
        module = &th06_msg;
        break;
    case 95:
        module = &th95_msg;
        break;
    case 125:
        if (thmsg_opt_end)
            module = &th95_msg;
        else
            module = &th06_msg;
        break;
    default:
        if (mode == 'c' || mode == 'd') {
            if (version == 0)
                fprintf(stderr, "%s: version must be specified\n", argv0);
            else
                fprintf(stderr, "%s: version %u is unsupported\n", argv0, version);
            return 1;
        }
    }

    switch (mode)
    {
    case 'c':
    case 'd': {
        int ret;

        if (argc > 2) {
            current_input = argv[2];
            in = fopen(argv[2], "rb");
            if (!in) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, argv[2], strerror(errno));
                return 1;
            }
            if (argc > 3) {
                current_output = argv[3];
                out = fopen(argv[3], "wb");
                if (!out) {
                    fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                        argv0, argv[3], strerror(errno));
                    fclose(in);
                    return 1;
                }
            }
        }

        if (mode == 'c')
            ret = module->write(in, out, version);
        else
            ret = module->read(in, out, version);

        fclose(in);
        fclose(out);

        if (!ret)
            return 1;

        return 0;
    }
    case 'V':
        util_print_version();
        return 0;
    default:
        return 1;
    }
}
