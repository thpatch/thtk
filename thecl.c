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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "program.h"
#include "thecl.h"
#include "util.h"

extern const thecl_module_t th06_ecl;
extern const thecl_module_t th10_ecl;

thecl_t*
thecl_new(
    void)
{
    thecl_t* ecl = calloc(1, sizeof(thecl_t));
    list_init(&ecl->subs);
    list_init(&ecl->local_data);
    return ecl;
}

static void
thecl_free(
    thecl_t* ecl)
{
    for (size_t a = 0; a < ecl->anim_count; ++a)
        free(ecl->anim_names[a]);
    free(ecl->anim_names);

    for (size_t e = 0; e < ecl->ecli_count; ++e)
        free(ecl->ecli_names[e]);
    free(ecl->ecli_names);

    thecl_sub_t* sub;
    list_for_each(&ecl->subs, sub) {
        free(sub->name);

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr)
            thecl_instr_free(instr);
        list_free_nodes(&sub->instrs);

        for (size_t v = 0; v < sub->var_count; ++v)
            free(sub->vars[v]);
        free(sub->vars);

        thecl_label_t* label;
        list_for_each(&sub->labels, label)
            free(label);
        list_free_nodes(&sub->labels);

        free(sub);
    }
    list_free_nodes(&ecl->subs);

    thecl_local_data_t* local_data;
    list_for_each(&ecl->local_data, local_data)
        free(local_data);
    list_free_nodes(&ecl->local_data);

    free(ecl);
}

thecl_instr_t*
thecl_instr_new(void)
{
    thecl_instr_t* instr = calloc(1, sizeof(thecl_instr_t));
    instr->type = THECL_INSTR_INSTR;
    list_init(&instr->params);
    return instr;
}

thecl_instr_t*
thecl_instr_time(unsigned int time)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->type = THECL_INSTR_TIME;
    instr->time = time;
    return instr;
}

thecl_instr_t*
thecl_instr_rank(unsigned int rank)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->type = THECL_INSTR_RANK;
    instr->rank = rank;
    return instr;
}

thecl_instr_t*
thecl_instr_label(unsigned int offset)
{
    thecl_instr_t* instr = thecl_instr_new();
    instr->type = THECL_INSTR_LABEL;
    instr->offset = offset;
    return instr;
}

void
thecl_instr_free(thecl_instr_t* instr)
{
    free(instr->string);

    thecl_param_t* param;
    list_for_each(&instr->params, param) {
        value_free(&param->value);
        free(param);
    }
    list_free_nodes(&instr->params);

    free(instr);
}

thecl_param_t*
param_new(
    int type)
{
    thecl_param_t* param = calloc(1, sizeof(thecl_param_t));
    param->type = type;
    param->value.type = type;
    return param;
}

void
param_free(
    thecl_param_t* param)
{
    value_free(&param->value);
    free(param);
}

static void
print_usage(void)
{
    printf("Usage: %s COMMAND[OPTION...] [INPUT [OUTPUT]]\n"
           "COMMAND can be:\n"
           "  c  create ECL file\n"
           "  d  dump ECL file\n"
           "  h  display this help and exit\n"
           "  V  display version information and exit\n"
           "OPTION can be:\n"
           "  #  # can be 6, 7, 8, 9, 95, 10, 11, 12, 125, 128, or 13 (required)\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

int
main(int argc, char* argv[])
{
    FILE* in = stdin;
    FILE* out = stdout;
    unsigned int version = 0;
    int mode;
    const thecl_module_t* module = NULL;

    current_input = "(stdin)";
    current_output = "(stdout)";

    mode = parse_args(argc, argv, print_usage, "cdhV", "", &version);

    switch (version) {
    case 6:
    case 7:
    case 8:
    case 9:
    case 95:
        module = &th06_ecl;
        break;
    case 10:
    case 11:
    case 12:
    case 125:
    case 128:
    case 13:
        module = &th10_ecl;
        break;
    default:
        if (version == 0)
            fprintf(stderr, "%s: version must be specified\n", argv0);
        else
            fprintf(stderr, "%s: version %u is unsupported\n", argv0, version);
        exit(1);
    }

    switch (mode)
    {
    case 'c':
    case 'd': {
        if (argc > 2) {
            current_input = argv[2];
            in = fopen(argv[2], "rb");
            if (!in) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, argv[2], strerror(errno));
                exit(1);
            }
            if (argc > 3) {
                current_output = argv[3];
                out = fopen(argv[3], "wb");
                if (!out) {
                    fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                        argv0, argv[3], strerror(errno));
                    fclose(in);
                    exit(1);
                }
            }
        }

        if (mode == 'c') {
            thecl_t* ecl = module->parse(in, version);
            if (!ecl)
                exit(1);
            module->compile(ecl, out);
            thecl_free(ecl);
        } else if (mode == 'd') {
            thecl_t* ecl = module->open(in, version);
            if (!ecl)
                exit(1);
            module->trans(ecl);
            module->dump(ecl, out);
            thecl_free(ecl);
        }
        fclose(in);
        fclose(out);

        exit(0);
    }
    case 'V':
        util_print_version();
        exit(0);
    case 'h':
        print_usage();
        exit(0);
    default:
        exit(1);
    }
}
