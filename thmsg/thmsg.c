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
#include <stdlib.h>
#include "program.h"
#include "thmsg.h"
#include "util.h"
#include "mygetopt.h"

int thmsg_opt_end = 0;

extern const thmsg_module_t th06_msg;
extern const thmsg_module_t th95_msg;

static void
print_usage(void)
{
    printf("Usage: %s [-Ve] [[-c | -d] VERSION] [INPUT [OUTPUT]]\n"
           "Options:\n"
           "  -c  create a dialogue file\n"
           "  -d  dump a dialogue file\n"
           "  -V  display version information and exit\n"
           "  -e  extract or create ending dialogue\n"
           "VERSION can be:\n"
           "  6, 7, 8, 9, 95, 10, 11, 12, 125, 128, 13, 14, 143, 15, 16, 165, 17, 18, 185, 19, or 20\n"
           /* NEWHU: 20 */
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

int
main(int argc, char* argv[])
{
    FILE* in = stdin;
    FILE* out = stdout;
    unsigned int version = 0;
    int mode = -1;
    const thmsg_module_t* module = NULL;

    current_input = "(stdin)";
    current_output = "(stdout)";

    argv0 = util_shortname(argv[0]);
    int opt;
    int ind=0;
    while(argv[util_optind]) {
        switch(opt = util_getopt(argc, argv, ":c:d:Ve")) {
        case 'c':
        case 'd':
            if(mode != -1) {
                fprintf(stderr,"%s: More than one mode specified\n", argv0);
                print_usage();
                exit(1);
            }
            mode = opt;
            version = parse_version(util_optarg);
            break;
        case 'e':
            thmsg_opt_end = 1;
            break;
        default:
            util_getopt_default(&ind,argv,opt,print_usage);
        }
    }
    argc = ind;
    argv[argc] = NULL;

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
    case 143:
    case 15:
    case 16:
    case 165:
    case 17:
    case 18:
    case 185:
    case 19:
    case 20:
    /* NEWHU: 20 */
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

        if (argc > 0) {
            current_input = argv[0];
            in = fopen(argv[0], "rb");
            if (!in) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, argv[0], strerror(errno));
                return 1;
            }
            if (argc > 1) {
                current_output = argv[1];
                out = fopen(argv[1], "wb");
                if (!out) {
                    fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                        argv0, argv[1], strerror(errno));
                    fclose(in);
                    return 1;
                }
            }
        }

        if (mode == 'c') {
#ifdef _WIN32
            _setmode(fileno(stdout), _O_BINARY);
#endif
            ret = module->write(in, out, version);
        } else {
#ifdef _WIN32
            _setmode(fileno(stdin), _O_BINARY);
#endif
            ret = module->read(in, out, version);
        }

        fclose(in);
        fclose(out);

        if (!ret)
            return 1;

        return 0;
    }
    default:
        print_usage();
        return 1;
    }
}
