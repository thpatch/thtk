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
#include "program.h"
#include "thecl.h"
#include "util.h"
#include "mygetopt.h"

extern const thecl_module_t th06_ecl;
extern const thecl_module_t th10_ecl;

eclmap_t *g_eclmap = NULL;
bool g_ecl_rawoutput = false;
bool g_ecl_simplecreate = false;
bool g_ecl_hexdebug = false;
bool g_ecl_encode_cp932 = false;
bool g_was_error = false;

thecl_t*
thecl_new(
    void)
{
    thecl_t* ecl = calloc(1, sizeof(thecl_t));
    list_init(&ecl->subs);
    list_init(&ecl->timelines);
    ecl->no_warn = false;
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
        free(sub->format);

        thecl_instr_t* instr;
        list_for_each(&sub->instrs, instr)
            thecl_instr_free(instr);
        list_free_nodes(&sub->instrs);

        for (size_t v = 0; v < sub->var_count; ++v)
            thecl_variable_free(sub->vars[v]);
        free(sub->vars);

        thecl_label_t* label;
        list_for_each(&sub->labels, label)
            free(label);
        list_free_nodes(&sub->labels);

        free(sub);
    }
    list_free_nodes(&ecl->subs);

    free(ecl);
}

thecl_instr_t*
thecl_instr_new(void)
{
    thecl_instr_t* instr = calloc(1, sizeof(thecl_instr_t));
    instr->type = THECL_INSTR_INSTR;
    instr->flags = 0;
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

void
thecl_variable_free(thecl_variable_t* var) {
    free(var->name);
    free(var);
}

thecl_param_t*
param_new(
    int type)
{
    thecl_param_t* param = calloc(1, sizeof(thecl_param_t));
    param->type = type;
    param->value.type = type;
    param->is_expression_param = 0;
    return param;
}

thecl_param_t*
param_copy(
    thecl_param_t* param
) {
    thecl_param_t* copy = calloc(1, sizeof(thecl_param_t));
    memcpy(copy, param, sizeof(thecl_param_t));
    /* Handle possible pointers in the value. */
    if (copy->value.type == 'z')
        copy->value.val.z = strdup(param->value.val.z);
    else if (copy->value.type == 'm') {
        copy->value.val.m.data = malloc(param->value.val.m.length);
        memcpy(copy->value.val.m.data, param->value.val.m.data, param->value.val.m.length);
    }
    return copy;
}

void
param_free(
    thecl_param_t* param)
{
    value_free(&param->value);
    free(param);
}

int32_t
label_offset(
    thecl_sub_t* sub,
    const char* name)
{
    thecl_label_t* label;
    list_for_each(&sub->labels, label) {
        if (strcmp(label->name, name) == 0)
            return label->offset;
    }
    fprintf(stderr, "%s: label not found: %s\n", argv0, name);
    return 0;
}

int32_t
label_time(
    thecl_sub_t* sub,
    const char* name)
{
    thecl_label_t* label;
    list_for_each(&sub->labels, label) {
        if (strcmp(label->name, name) == 0)
            return label->time;
    }
    fprintf(stderr, "%s: label not found: %s\n", argv0, name);
    return 0;
}

static void
free_globals(void)
{
    eclmap_free(g_eclmap);
}

bool
is_pre_th8(unsigned int version) {
    switch (version) {
        case 6: case 7: return true;
        default: return false;
    }
}

bool
is_post_th10(unsigned int version) {
    switch(version) {
        case 6: case 7:
        case 8: case 9: case 95: return false;
        default: return true;
    }
}

bool
is_post_alcostg(unsigned int version) {
    switch (version) {
        case 6: case 7:
        case 8: case 9: case 95:
        case 10: return false;
        default: return true;
    }
}

bool
is_post_th125(unsigned int version) {
    switch (version) {
        case 6: case 7:
        case 8: case 9: case 95:
        case 10:
        case 103: case 11: case 12: return false;
        default: return true;
    }
}

bool
is_post_th13(unsigned int version) {
    switch(version) {
        case 6: case 7:
        case 8: case 9: case 95:
        case 10:
        case 103: case 11: case 12:
        case 125: case 128: return false;
        default: return true;
    }
}

thecl_engine_version
engine_version(unsigned int version) {
    switch (version) {
        default:
            return VER_POST_TH13;
        case 125: case 128:
            return VER_POST_TH125;
        case 103: case 11: case 12:
            return VER_POST_ALCOSTG;
        case 10:
            return VER_POST_TH10;
        case 8: case 9: case 95:
            return VER_PRE_TH10;
        case 6: case 7:
            return VER_PRE_TH8;
    }
}

bool
is_numeric_difficulty_version(unsigned int version) {
    return version == 185 || version == 19;
}

int32_t
get_default_none_rank(unsigned int version) {
    if (!is_post_th13(version)) {
        return 0xF0;
    } else if (is_numeric_difficulty_version(version)) {
        return 0x00;
    } else {
        return 0xC0;
    }
}

static void
print_usage(void)
{
    printf("Usage: %s [-Vrsxj] [[-c | -h | -d] VERSION] [-m ECLMAP]... [INPUT [OUTPUT]]\n"
           "Options:\n"
           "  -c  create ECL file\n"
           "  -h  create header file\n"
           "  -d  dump ECL file\n"
           "  -V  display version information and exit\n"
           "  -m  use map file for translating mnemonics\n"
           "  -r  output raw ECL opcodes, applying minimal transformations\n"
           "  -s  use simple creation, which doesn't add any instructions automatically\n"
           "  -x  add address information for ECL instructions\n"
           "  -j  convert strings between Shift-JIS and UTF-8\n"
           "VERSION can be:\n"
           "  6, 7, 8, 9, 95, 10, 103 (for Uwabami Breakers), 11, 12, 125, 128, 13, 14, 143, 15, 16, 165, 17, 18, 185, 19, or 20\n"
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
    const thecl_module_t* module = NULL;

    current_input = "(stdin)";
    current_output = "(stdout)";

    g_eclmap = eclmap_new();
    atexit(free_globals);

    argv0 = util_shortname(argv[0]);
    int opt;
    int ind=0;
    while(argv[util_optind]) {
        switch(opt = util_getopt(argc, argv, "+:c:h:d:Vm:rsxj")) {
        case 'c':
        case 'd':
        case 'h':
            if(mode != -1) {
                fprintf(stderr,"%s: More than one mode specified\n", argv0);
                print_usage();
                exit(1);
            }
            mode = opt;
            version = parse_version(util_optarg);
            break;
        case 'm': {
            FILE* map_file = NULL;
            map_file = fopen(util_optarg, "r");
            if (!map_file) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, util_optarg, strerror(errno));
                exit(1);
            }
            eclmap_load(version, g_eclmap, map_file, util_optarg);
            fclose(map_file);
            break;
        }
        case 'r':
            g_ecl_rawoutput = true;
            break;
        case 's':
            g_ecl_simplecreate = true;
            break;
        case 'x':
            g_ecl_hexdebug = true;
            break;
        case 'j':
            g_ecl_encode_cp932 = true;
            break;
        default:
            util_getopt_default(&ind,argv,opt,print_usage);
        }
    }
    argc = ind;
    argv[argc] = NULL;

    eclmap_rebuild(g_eclmap);

    switch (version) {
    case 6:
    case 7:
    case 8:
    case 9:
    case 95:
        module = &th06_ecl;
        break;
    case 10:
    case 103:
    case 11:
    case 12:
    case 125:
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
        module = &th10_ecl;
        break;
    default:
        if (mode == 'c' || mode == 'd' || mode == 'h') {
            if (version == 0)
                fprintf(stderr, "%s: version must be specified\n", argv0);
            else
                fprintf(stderr, "%s: version %u is unsupported\n", argv0, version);
            exit(1);
        }
    }

    switch (mode)
    {
    case 'c':
    case 'h':
    case 'd': {
        if(g_ecl_rawoutput) {
            if (mode != 'd') {
                fprintf(stderr, "%s: 'r' option cannot be used while compiling\n", argv0);
                exit(1);
            }
        }
        if (g_ecl_hexdebug) {
            if (mode != 'd') {
                fprintf(stderr, "%s: 'x' option cannot be used while compiling\n", argv0);
                exit(1);
            }
        }
        if (g_ecl_simplecreate) {
            if (mode != 'c' && mode != 'h') {
                fprintf(stderr, "%s: 's' option cannot be used while dumping\n", argv0);
                exit(1);
            }
        }
        if (mode == 'h' && !is_post_th10(version)) {
            fprintf(stderr, "%s: 'h' option can't be used with a pre-th10 version\n", argv0);
            exit(1);
        }

        if (0 < argc) {
            current_input = argv[0];
            in = fopen(argv[0], "rb");
            if (!in) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, argv[0], strerror(errno));
                exit(1);
            }
            if (1 < argc) {
                current_output = argv[1];
                out = fopen(argv[1], "wb");
                if (!out) {
                    fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                        argv0, argv[1], strerror(errno));
                    fclose(in);
                    exit(1);
                }
            }
        }

        if (mode == 'c') {
#ifdef _WIN32
            (void)_setmode(fileno(stdout), _O_BINARY);
#endif
            thecl_t* ecl = module->parse(in, argv[0], version);
            if (!ecl)
                exit(1);
            module->compile(ecl, out);
            thecl_free(ecl);
        } else if (mode == 'h') {
            thecl_t* ecl = module->parse(in, argv[0], version);
            if (!ecl)
                exit(1);
            module->create_header(ecl, out);
            thecl_free(ecl);
        } else if (mode == 'd') {
#ifdef _WIN32
            (void)_setmode(fileno(stdin), _O_BINARY);
#endif
            thecl_t* ecl = module->open(in, version);
            if (!ecl)
                exit(1);
            module->trans(ecl);
            module->dump(ecl, out);
            thecl_free(ecl);
        }
        fclose(in);
        fclose(out);

        if(g_was_error) {
          printf("%s: %s: there were errors.\n", argv0, argv[0]);
          exit(1);
        }
        exit(0);
    }
    default:
        print_usage();
        exit(1);
    }
}
