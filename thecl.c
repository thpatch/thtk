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
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "ecl.h"
#include "ecsparse.h"
#include "instr.h"
#include "program.h"
#include "util.h"

extern int compile_ecs(FILE* in, FILE* out, unsigned int version);

static FILE* in;
static FILE* out;

static void
print_usage(void)
{
    printf("Usage: %s COMMAND[OPTION...] [INPUT [OUTPUT]]\n"
           "COMMAND can be:\n"
           "  c  create ECL file\n"
           "  d  dump ECL file\n"
           "  r  raw ECL file dump\n"
           "  p  generate parameter table\n"
           "  h  display this help and exit\n"
           "  V  display version information and exit\n"
           "OPTION can be:\n"
           "  #  # can be 10, 11, 12, 125, or 128 (required)\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

/* It would probably be easier to read the entire file at once and use pointers
 * to access the data. */
static int
open_ecl(ecl_t* ecl, FILE* f)
{
    unsigned int i;
    char magic[5] = { 0 };

    if (!util_seekable(f)) {
        fprintf(stderr, "%s: input is not seekable\n", argv0);
        return 0;
    }

    if (fread(magic, 4, 1, f) != 1) {
        fprintf(stderr, "%s:%s: couldn't read: %s\n",
            argv0, current_input, strerror(errno));
        return 0;
    }
    if (strncmp(magic, "SCPT", 4) != 0) {
        fprintf(stderr, "%s:%s: SCPT signature missing\n", argv0, current_input);
        return 0;
    }

    if (fread(&ecl->scpt, sizeof(header_scpt_t), 1, f) != 1) {
        fprintf(stderr, "%s:%s: couldn't read: %s\n",
            argv0, current_input, strerror(errno));
        return 0;
    }
    assert(ecl->scpt.unknown1 == 1);
    assert(ecl->scpt.include_offset == 0x24);
    assert(ecl->scpt.zero1 == 0);
    assert(ecl->scpt.zero2[0] == 0);
    assert(ecl->scpt.zero2[1] == 0);
    assert(ecl->scpt.zero2[2] == 0);
    assert(ecl->scpt.zero2[3] == 0);

    if (fseek(f, ecl->scpt.include_offset, SEEK_SET) == -1) {
        fprintf(stderr, "%s:%s: couldn't seek: %s\n",
            argv0, current_input, strerror(errno));
        return 0;
    }

    if (fread(magic, 4, 1, f) != 1) {
        fprintf(stderr, "%s:%s: couldn't read: %s\n",
            argv0, current_input, strerror(errno));
        return 0;
    }
    if (strncmp(magic, "ANIM", 4) != 0) {
        fprintf(stderr, "%s:%s: ANIM signature missing\n", argv0, current_input);
        return 0;
    }

    if (fread(&ecl->anim_cnt, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "%s:%s: couldn't read: %s\n",
            argv0, current_input, strerror(errno));
        return 0;
    }
    ecl->anim = malloc(sizeof(char*) * ecl->anim_cnt);
    for (i = 0; i < ecl->anim_cnt; ++i) {
        char buffer[256];
        util_read_asciiz(buffer, 256, f);
        ecl->anim[i] = strdup(buffer);
    }

    while (ftell(f) % 4 != 0) {
        if (fgetc(f) == EOF)
            break;
    }

    if (fread(magic, 4, 1, f) != 1) {
        fprintf(stderr, "%s:%s: couldn't read: %s\n",
            argv0, current_input, strerror(errno));
        return 0;
    }
    if (strncmp(magic, "ECLI", 4) != 0) {
        fprintf(stderr, "%s:%s: ECLI signature missing\n", argv0, current_input);
        return 0;
    }

    if (fread(&ecl->ecli_cnt, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "%s:%s: couldn't read: %s\n",
            argv0, current_input, strerror(errno));
        return 0;
    }
    ecl->ecli = malloc(sizeof(char*) * ecl->ecli_cnt);
    for (i = 0; i < ecl->ecli_cnt; ++i) {
        char buffer[256];
        util_read_asciiz(buffer, 256, f);
        ecl->ecli[i] = strdup(buffer);
    }

    ecl->sub_cnt = ecl->scpt.sub_cnt;
    ecl->subs = calloc(ecl->sub_cnt, sizeof(sub_t));

    while (ftell(f) % 4 != 0) {
        if (fgetc(f) == EOF)
            break;
    }

    for (i = 0; i < ecl->sub_cnt; ++i) {
        if (fread(&ecl->subs[i].offset, sizeof(uint32_t), 1, f) != 1) {
            fprintf(stderr, "%s:%s: couldn't read: %s\n",
                argv0, current_input, strerror(errno));
            return 0;
        }
    }

    for (i = 0; i < ecl->sub_cnt; ++i) {
        /* XXX: Maximum length? */
        char buffer[256];
        util_read_asciiz(buffer, 256, f);
        ecl->subs[i].name = strdup(buffer);
    }

    for (i = 0; i < ecl->sub_cnt; ++i) {
        header_eclh_t eclh;
        sub_t* sub;

        if (fseek(f, ecl->subs[i].offset, SEEK_SET) == -1) {
            fprintf(stderr, "%s:%s: couldn't seek: %s\n",
                argv0, current_input, strerror(errno));
            return 0;
        }

        if (fread(magic, 4, 1, f) != 1) {
            fprintf(stderr, "%s:%s: couldn't read: %s\n",
                argv0, current_input, strerror(errno));
            return 0;
        }
        if (strncmp(magic, "ECLH", 4) != 0) {
            fprintf(stderr, "%s:%s: ECLH signature missing\n",
                argv0, current_input);
            return 0;
        }

        if (fread(&eclh, sizeof(header_eclh_t), 1, f) != 1) {
            fprintf(stderr, "%s:%s: couldn't read: %s\n",
                argv0, current_input, strerror(errno));
            return 0;
        }

        assert(eclh.unknown1 == 0x10);
        assert(eclh.zero[0] == 0);
        assert(eclh.zero[1] == 0);

        sub = &ecl->subs[i];
        sub->instr_cnt = 0;
        sub->raw_instrs = NULL;

        /* Unfortunately the instruction count is not provided. */
        while (1) {
            raw_instr_t rins;
            memset(&rins, 0, sizeof(raw_instr_t));
            rins.offset = ftell(f);

            /* EOF is expected for the last instruction. */
            if (fread(&rins, 16, 1, f) != 1)
                break;

            if (i + 1 != ecl->sub_cnt && ftell(f) > ecl->subs[i + 1].offset)
                break;

            assert(rins.zero == 0);
            assert(rins.size >= 16);

            rins.data_size = rins.size - 16;
            if (rins.data_size) {
                rins.data = malloc(rins.data_size);
                if (fread(rins.data, rins.data_size, 1, f) != 1) {
                    fprintf(stderr, "%s:%s: couldn't read: %s\n",
                        argv0, current_input, strerror(errno));
                    return 0;
                }
            }

            sub->instr_cnt++;
            sub->raw_instrs = realloc(sub->raw_instrs,
                                      sub->instr_cnt * sizeof(raw_instr_t));
            sub->raw_instrs[sub->instr_cnt - 1] = rins;
        }
    }

    return 1;
}

static void
ecldump_list_params(ecl_t* ecl)
{
    unsigned int i;

    char instr_params[1024][16];
    memset(instr_params, -1, 1024 * 16);

    for (i = 0; i < ecl->sub_cnt; ++i) {
        unsigned int j;
        for (j = 0; j < ecl->subs[i].instr_cnt; ++j) {
            unsigned int k;
            raw_instr_t* rins = &ecl->subs[i].raw_instrs[j];

            assert(rins->id <= 1024);
            assert(rins->param_cnt <= 15);

            switch (instr_params[rins->id][0]) {
            case -2: /* Broken. */
                break;
            case -1: /* New. */
                memset(instr_params[rins->id], 0, 16);
                if (rins->param_cnt != rins->data_size >> 2) {
                    instr_params[rins->id][0] = -2;
                    break;
                } else {
                    instr_params[rins->id][0] = rins->param_cnt;
                }
            default:
                for (k = 0; k < rins->param_cnt; ++k) {
                    if (instr_params[rins->id][k + 1] != 'i') {
                        /* TODO: This float checking stuff is pretty ugly,
                         * move it to util.c so it can be fixed more easily. */
                        int32_t integer = 0;
                        float floating = 0;
                        char buffer[128];
                        memcpy(&integer,
                               rins->data + k * sizeof(int32_t),
                               sizeof(int32_t));
                        memcpy(&floating,
                               rins->data + k * sizeof(int32_t),
                               sizeof(int32_t));
                        sprintf(buffer, "%.9f", floating);
                        if (((integer & 0xfffff000) == 0xfffff000) ||
                            (strcmp("nan", buffer) == 0) ||
                            (integer != 0 && strcmp("0.000000000", buffer) == 0))
                            instr_params[rins->id][k + 1] = 'i';
                        else
                            instr_params[rins->id][k + 1] = 'f';
                    }
                }
                break;
            }
        }
    }

    for (i = 0; i < 1024; ++i) {
        if (instr_params[i][0] >= 0) {
            fprintf(out, "    { %u, \"%s\" },\n", i, instr_params[i] + 1);
        } else if (instr_params[i][0] == -2) {
            fprintf(out, "    /*{ %u, \"%s\" },*/\n", i, instr_params[i] + 1);
        }
    }
}

static void
ecldump_rawdump(ecl_t* ecl)
{
    unsigned int i;

    for (i = 0; i < ecl->sub_cnt; ++i) {
        unsigned int j;

        fprintf(out, "Subroutine %s\n", ecl->subs[i].name);

        for (j = 0; j < ecl->subs[i].instr_cnt; ++j) {
            raw_instr_t* rins = &ecl->subs[i].raw_instrs[j];
            unsigned int k;

            fprintf(out, "Instruction %u %u %u 0x%02x %u %u: ",
                rins->time, rins->id, rins->size, rins->param_mask,
                rins->rank_mask, rins->param_cnt);

            for (k = 0; k < rins->data_size; k += sizeof(uint32_t)) {
                fprintf(out, "0x%08x ", *(uint32_t*)(rins->data + k));
            }

            fprintf(out, "\n");
        }

        fprintf(out, "\n");
    }
}

static void
ecldump_translate(ecl_t* ecl, unsigned int version)
{
    unsigned int i;

    for (i = 0; i < ecl->sub_cnt; ++i) {
        unsigned int j;

        ecl->subs[i].instrs = malloc(sizeof(instr_t) * ecl->subs[i].instr_cnt);

        for (j = 0; j < ecl->subs[i].instr_cnt; ++j) {
            if (!instr_parse(&ecl->subs[i].raw_instrs[j],
                             &ecl->subs[i].instrs[j], version)) {
                exit(1);
            }
            ecl->subs[i].instrs[j].offset =
                ecl->subs[i].raw_instrs[j].offset - ecl->subs[i].offset;
        }
    }
}

static void
ecldump_display_param(char* output, unsigned int output_length,
                      const sub_t* sub, const instr_t* instr,
                      unsigned int i, const param_t* param,
                      unsigned int version)
{
    const char* floatb;
    param_t newparam;

    if (param == NULL)
        param = &instr->params[i];

    switch (param->type) {
    case 'i':
        if (instr->param_mask & (1 << i)) {
            if (param->value.i >= 0 &&
                param->value.i % 4) {
                fprintf(stderr, "%s:%s: strange stack offset: %d\n",
                    argv0, current_input, param->value.i);
                abort();
            }
            snprintf(output, output_length, "[%d]", param->value.i);
        } else
            snprintf(output, output_length, "%d", param->value.i);
        break;
    case 'o':
        snprintf(output, output_length, "%s_%u",
            sub->name, instr->offset + param->value.i);
        break;
    case 'f':
        floatb = util_printfloat(&param->value.f);
        if (instr->param_mask & (1 << i)) {
            int tempint = param->value.f;
            /* XXX: Exactly how well does this work? */
            if (floor(param->value.f) != param->value.f)
                fprintf(stderr, "%s:%s: non-integral float: %s\n",
                    argv0, current_input, floatb);
            if (tempint >= 0 && tempint % 4) {
                fprintf(stderr, "%s:%s: strange stack offset: %s\n",
                    argv0, current_input, floatb);
            }
            snprintf(output, output_length, "[%sf]", floatb);
        } else
            snprintf(output, output_length, "%sf", floatb);
        break;
    case 's':
        snprintf(output, output_length, "\"%s\"", param->value.s.data);
        break;
    case 'c':
        snprintf(output, output_length, "C\"%s\"", param->value.s.data);
        break;
    case 'D':
        memcpy(&newparam, param, sizeof(param_t));
        if (param->value.D[0] == 0x6666) {
            snprintf(output, output_length, "(float)");
            newparam.type = 'f';
            memcpy(&newparam.value.f,
                   &param->value.D[1],
                   sizeof(float));
        } else if (param->value.D[0] == 0x6669) {
            snprintf(output, output_length, "(float)");
            newparam.type = 'i';
            memcpy(&newparam.value.i,
                   &param->value.D[1],
                   sizeof(int32_t));
        } else if (param->value.D[0] == 0x6966) {
            snprintf(output, output_length, "(int)");
            newparam.type = 'f';
            memcpy(&newparam.value.f,
                   &param->value.D[1],
                   sizeof(float));
        } else if (param->value.D[0] == 0x6969) {
            snprintf(output, output_length, "(int)");
            newparam.type = 'i';
            memcpy(&newparam.value.i,
                   &param->value.D[1],
                   sizeof(int32_t));
        } else {
            fprintf(stderr, "%s:%s: unknown value: %u\n",
                argv0, current_input, param->value.D[0]);
            abort();
        }
        ecldump_display_param(output + strlen(output), output_length - strlen(output), sub, instr, i, &newparam, version);
        break;
    default:
        break;
    }
}

static void
ecldump_render_instr(const sub_t* sub, instr_t* instr, instr_t** stack, unsigned int* stack_top, unsigned int version)
{
    const stackinstr_t* i;
    unsigned int j;

    for (i = get_stackinstrs(version); i->type; ++i) {
        if (i->instr == instr->id &&
            strlen(i->params) == instr->param_cnt &&
            *stack_top > strlen(i->stack)) {
            int skip = 0;
            const char* format = NULL;

            for (j = 0; j < strlen(i->stack); ++j) {
                if (stack[*stack_top - strlen(i->stack) - 1 + j]->type != i->stack[j])
                    skip = 1;
                if (instr->time != stack[*stack_top - strlen(i->stack) - 1 + j]->time)
                    skip = 1;
                if (j != 0 && stack[*stack_top - strlen(i->stack) - 1 + j]->label)
                    skip = 1;
            }

            for (j = 0; j < instr->param_cnt; ++j) {
                if (instr->params[j].type != i->params[j])
                    skip = 1;
            }

            if (skip)
                continue;

            /* Make sure the label and its offset is maintained. */
            if (strlen(i->stack) && (instr->label || ((*stack_top > 1) && (stack[*stack_top - 2]->time != instr->time))))
                continue;

            switch (i->type) {
            case ADD:      format = "(%s + %s)";  break;
            case SUBTRACT: format = "(%s - %s)";  break;
            case MULTIPLY: format = "(%s * %s)";  break;
            case DIVIDE:   format = "(%s / %s)";  break;
            case MODULO:   format = "(%s %% %s)"; break;
            case EQUAL:    format = "(%s == %s)"; break;
            case INEQUAL:  format = "(%s != %s)"; break;
            case LT:       format = "(%s < %s)";  break;
            case LTEQ:     format = "(%s <= %s)"; break;
            case GT:       format = "(%s > %s)";  break;
            case GTEQ:     format = "(%s >= %s)"; break;
            case AND:      format = "(%s & %s)";  break;
            case OR:       format = "(%s | %s)";  break;
            case XOR:      format = "(%s ^ %s)";  break;
            default: break;
            }

            /* TODO: Make 1024 a constant. */
            if (format) {
                snprintf(instr->string, 1024, format, stack[*stack_top - 3]->string, stack[*stack_top - 2]->string);
            } else {
                if (i->type == GOTO || i->type == UNLESS || i->type == IF) {
                    char target[256];
                    char newtime[256];
                    ecldump_display_param(target, 256, sub, instr, 0, NULL, version);
                    ecldump_display_param(newtime, 256, sub, instr, 1, NULL, version);
                    if (i->type == GOTO) {
                        snprintf(instr->string, 1024, "goto %s @ %s",
                            target, newtime);
                    } else if (i->type == UNLESS) {
                        snprintf(instr->string, 1024, "unless %s goto %s @ %s",
                            stack[*stack_top - 2]->string, target, newtime);
                    } else if (i->type == IF) {
                        snprintf(instr->string, 1024, "if %s goto %s @ %s",
                            stack[*stack_top - 2]->string, target, newtime);
                    }
                } else if (i->type == LOAD || i->type == ASSIGN) {
                    /* ecldump_display_param(instr->string + strlen(instr->string), 1024 - strlen(instr->string), &ecl->subs[i], instr, k, NULL, version); */
                    if (instr->params[0].type == 'i') {
                        if (instr->param_mask & 1) {
                            snprintf(instr->string, 1024, "[%d]", instr->params[0].value.i);
                        } else {
                            snprintf(instr->string, 1024, "%d", instr->params[0].value.i);
                        }
                    } else if (instr->params[0].type == 'f') {
                        const char* floatb = util_printfloat(&instr->params[0].value.f);
                        if (instr->param_mask & 1) {
                            snprintf(instr->string, 1024, "[%sf]", floatb);
                        } else
                            snprintf(instr->string, 1024, "%sf", floatb);
                    }

                    if (i->type == ASSIGN) {
                        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), " = %s", stack[*stack_top - 2]->string);
                    }
                } else if (i->type == NOT) {
                    snprintf(instr->string, 1024, "(!%s)", stack[*stack_top - 2]->string);
                }
            }

            if (strlen(i->stack)) {
                instr->label = stack[*stack_top - strlen(i->stack) - 1]->label;
                instr->offset = stack[*stack_top - strlen(i->stack) - 1]->offset;
            }

            instr->type = i->value;
            *stack_top -= strlen(i->stack);
            stack[*stack_top - 1] = instr;
            return;
        }
    }
}

static void
ecldump_translate_print(ecl_t* ecl, unsigned int version)
{
    unsigned int i;

    if (ecl->anim_cnt) {
        fprintf(out, "anim { ");
        for (i = 0; i < ecl->anim_cnt; ++i)
            fprintf(out, "\"%s\"; ", ecl->anim[i]);
        fprintf(out, "}\n");
    }

    if (ecl->ecli_cnt) {
        fprintf(out, "ecli { ");
        for (i = 0; i < ecl->ecli_cnt; ++i)
            fprintf(out, "\"%s\"; ", ecl->ecli[i]);
        fprintf(out, "}\n");
    }

    for (i = 0; i < ecl->sub_cnt; ++i) {
        unsigned int j, k;
        unsigned int time;
        unsigned int stack_top = 0;
        instr_t** stack = malloc(ecl->subs[i].instr_cnt * sizeof(instr_t*));

        for (j = 0; j < ecl->subs[i].instr_cnt; ++j) {
            unsigned int m;
            instr_t* instr = &ecl->subs[i].instrs[j];
            instr->string = malloc(1024);
            instr->string[0] = '\0';
            instr->type = 0;
            instr->label = 0;
            for (k = 0; k < ecl->subs[i].instr_cnt; ++k) {
                for (m = 0; m < ecl->subs[i].instrs[k].param_cnt; ++m) {
                    if (ecl->subs[i].instrs[k].params[m].type == 'o' &&
                        instr->offset == ecl->subs[i].instrs[k].offset + ecl->subs[i].instrs[k].params[m].value.i) {
                        instr->label = 1;

                        k = ecl->subs[i].instr_cnt;
                        break;
                    }
                }
            }
        }

        for (j = 0; j < ecl->subs[i].instr_cnt; ++j) {
            instr_t* instr = &ecl->subs[i].instrs[j];

            ++stack_top;
            stack[stack_top - 1] = instr;

            if (instr->rank_mask == 0xff)
                ecldump_render_instr(&ecl->subs[i], instr, stack, &stack_top, version);

            if (!instr->string[0]) {
                snprintf(instr->string, 1024, "ins_%u", instr->id);

                if (instr->rank_mask != 0xff) {
                    snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), " +");
                    if (instr->rank_mask & RANK_EASY)
                        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "E");
                    if (instr->rank_mask & RANK_NORMAL)
                        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "N");
                    if (instr->rank_mask & RANK_HARD)
                        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "H");
                    if (instr->rank_mask & RANK_LUNATIC)
                        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "L");
                }

                for (k = 0; k < instr->param_cnt; ++k) {
                    snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), " ");
                    ecldump_display_param(instr->string + strlen(instr->string), 1024 - strlen(instr->string), &ecl->subs[i], instr, k, &instr->params[k], version);
                }
            }
        }

        fprintf(out, "\nsub %s\n{\n", ecl->subs[i].name);

        time = 0;
        for (j = 0; j < stack_top; ++j) {
            if (stack[j]->time != time) {
                time = stack[j]->time;
                fprintf(out, "%u:\n", time);
            }

            if (stack[j]->label) {
                fprintf(out, "%s_%u:\n",
                    ecl->subs[i].name, stack[j]->offset);
            }

            fprintf(out, "    %s;\n", stack[j]->string);
        }
        free(stack);

        fprintf(out, "}\n");
    }
}

static void
ecldump_free(ecl_t* ecl)
{
    unsigned int i, j, k;

    if (ecl->anim) {
        for (i = 0; i < ecl->anim_cnt; ++i)
            free(ecl->anim[i]);
        free(ecl->anim);
    }

    if (ecl->ecli) {
        for (i = 0; i < ecl->ecli_cnt; ++i)
            free(ecl->ecli[i]);
        free(ecl->ecli);
    }

    if (ecl->subs) {
        for (i = 0; i < ecl->sub_cnt; ++i) {
            free(ecl->subs[i].name);

            for (j = 0; j < ecl->subs[i].instr_cnt; ++j) {
                free(ecl->subs[i].raw_instrs[j].data);

                if (ecl->subs[i].instrs && ecl->subs[i].instrs[j].params) {
                    for (k = 0; k < ecl->subs[i].instrs[j].param_cnt; ++k) {
                        /* 'o' might store data, but not in ecldump. */
                        if (ecl->subs[i].instrs[j].params[k].type == 's' ||
                            ecl->subs[i].instrs[j].params[k].type == 'c') {
                            free(ecl->subs[i].instrs[j].params[k].value.s.data);
                        }
                    }

                    free(ecl->subs[i].instrs[j].params);
                }

                if (ecl->subs[i].instrs && ecl->subs[i].instrs[j].string)
                    free(ecl->subs[i].instrs[j].string);
            }

            free(ecl->subs[i].raw_instrs);
            free(ecl->subs[i].instrs);

            for (j = 0; j < ecl->subs[i].label_cnt; ++j)
                free(ecl->subs[i].labels[j].name);

            free(ecl->subs[i].labels);
        }
        free(ecl->subs);
    }
}

int
main(int argc, char* argv[])
{
    char mode;
    unsigned int version = 0;

    argv0 = util_shortname(argv[0]);
    in = stdin;
    current_input = "(stdin)";
    out = stdout;
    current_output = "(stdout)";

    mode = parse_args(argc, argv, print_usage, "cdrphV", "", &version);

    if (argc > 2) {
        current_input = argv[2];
        in = fopen(argv[2], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading: %s\n", argv0, argv[2], strerror(errno));
            exit(1);
        }
        if (argc > 3) {
            current_output = argv[3];
            out = fopen(argv[3], "wb");
            if (!out) {
                fprintf(stderr, "%s: couldn't open %s for writing: %s\n", argv0, argv[3], strerror(errno));
                exit(1);
            }
        }
    }

    switch (mode)
    {
    case 'h':
        print_usage();
        exit(0);
    case 'V':
        util_print_version("thecl", PACKAGE_THECL_VERSION);
        exit(0);
    case 'c':
        if (!compile_ecs(in, out, version))
            exit(0);
        fclose(in);
        fclose(out);
        exit(0);
    case 'd': {
        ecl_t ecl;
        if (!open_ecl(&ecl, in))
            exit(1);
        fclose(in);
        ecldump_translate(&ecl, version);
        ecldump_translate_print(&ecl, version);
        ecldump_free(&ecl);
        fclose(out);
        exit(0);
    }
    case 'p': {
        ecl_t ecl;
        if (!open_ecl(&ecl, in))
            exit(1);
        fclose(in);
        ecldump_list_params(&ecl);
        ecldump_free(&ecl);
        fclose(out);
        exit(0);
    }
    case 'r': {
        ecl_t ecl;
        if (!open_ecl(&ecl, in))
            exit(1);
        fclose(in);
        ecldump_rawdump(&ecl);
        ecldump_free(&ecl);
        fclose(out);
        exit(0);
    }
    default:
        /* Usage will already have been displayed. */
        exit(1);
    }
}
