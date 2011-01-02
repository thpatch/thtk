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
#include "file.h"
#include "instr.h"
#include "program.h"
#include "util.h"

extern int compile_ecs(
    FILE* in,
    FILE* out,
    unsigned int version);

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
open_ecl(
    ecl_t* ecl,
    FILE* f)
{
    unsigned int i;
    char magic[5] = { 0 };

    if (!file_seekable(f)) {
        fprintf(stderr, "%s: input is not seekable\n", argv0);
        return 0;
    }

    if (!file_read(f, magic, 4))
        return 0;
    if (strncmp(magic, "SCPT", 4) != 0) {
        fprintf(stderr, "%s:%s: SCPT signature missing\n",
            argv0, current_input);
        return 0;
    }

    if (!file_read(f, &ecl->scpt, sizeof(header_scpt_t)))
        return 0;
    assert(ecl->scpt.unknown1 == 1);
    assert(ecl->scpt.include_offset == 0x24);
    assert(ecl->scpt.zero1 == 0);
    assert(ecl->scpt.zero2[0] == 0);
    assert(ecl->scpt.zero2[1] == 0);
    assert(ecl->scpt.zero2[2] == 0);
    assert(ecl->scpt.zero2[3] == 0);

    if (!file_seek(f, ecl->scpt.include_offset))
        return 0;

    if (!file_read(f, magic, 4))
        return 0;
    if (strncmp(magic, "ANIM", 4) != 0) {
        fprintf(stderr, "%s:%s: ANIM signature missing\n",
            argv0, current_input);
        return 0;
    }

    if (!file_read(f, &ecl->anim_cnt, sizeof(uint32_t)))
        return 0;
    ecl->anim = malloc(sizeof(char*) * ecl->anim_cnt);
    for (i = 0; i < ecl->anim_cnt; ++i) {
        char buffer[256];
        file_read_asciiz(f, buffer, 256);
        ecl->anim[i] = strdup(buffer);
    }

    while (file_tell(f) % 4 != 0) {
        if (getc_unlocked(f) == EOF)
            break;
    }

    if (!file_read(f, magic, 4))
        return 0;
    if (strncmp(magic, "ECLI", 4) != 0) {
        fprintf(stderr, "%s:%s: ECLI signature missing\n",
            argv0, current_input);
        return 0;
    }

    if (!file_read(f, &ecl->ecli_cnt, sizeof(uint32_t)))
        return 0;
    ecl->ecli = malloc(sizeof(char*) * ecl->ecli_cnt);
    for (i = 0; i < ecl->ecli_cnt; ++i) {
        char buffer[256];
        file_read_asciiz(f, buffer, 256);
        ecl->ecli[i] = strdup(buffer);
    }

    ecl->sub_cnt = ecl->scpt.sub_cnt;
    ecl->subs = calloc(ecl->sub_cnt, sizeof(sub_t));

    while (file_tell(f) % 4 != 0) {
        if (getc_unlocked(f) == EOF)
            break;
    }

    for (i = 0; i < ecl->sub_cnt; ++i) {
        if (!file_read(f, &ecl->subs[i].offset, sizeof(uint32_t)))
            return 0;
    }

    for (i = 0; i < ecl->sub_cnt; ++i) {
        /* TODO: Find out the maximum length. */
        char buffer[256];
        file_read_asciiz(f, buffer, 256);
        ecl->subs[i].name = strdup(buffer);
    }

    for (i = 0; i < ecl->sub_cnt; ++i) {
        header_eclh_t eclh;
        sub_t* sub;

        if (!file_seek(f, ecl->subs[i].offset))
            return 0;

        if (!file_read(f, magic, 4))
            return 0;
        if (strncmp(magic, "ECLH", 4) != 0) {
            fprintf(stderr, "%s:%s: ECLH signature missing\n",
                argv0, current_input);
            return 0;
        }

        if (!file_read(f, &eclh, sizeof(header_eclh_t)))
            return 0;
        assert(eclh.unknown1 == 0x10);
        assert(eclh.zero[0] == 0);
        assert(eclh.zero[1] == 0);

        sub = &ecl->subs[i];
        sub->instr_cnt = 0;
        sub->raw_instrs = NULL;
        sub->stack = -1;
        sub->arity = -1;

        /* The instruction count is unfortunately not provided. */
        while (1) {
            raw_instr_t rins;
            memset(&rins, 0, sizeof(raw_instr_t));
            rins.offset = file_tell(f);

            /* EOF is expected for the last instruction in the file. */
            if (fread_unlocked(&rins, 16, 1, f) != 1)
                break;

            if (i + 1 != ecl->sub_cnt && file_tell(f) > ecl->subs[i + 1].offset)
                break;

            assert(rins.zero == 0);
            assert(rins.size >= 16);

            rins.data_size = rins.size - 16;
            if (rins.data_size) {
                rins.data = malloc(rins.data_size);
                if (!file_read(f, rins.data, rins.data_size))
                    return 0;
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
set_arity(
    ecl_t* ecl,
    const char* subroutine,
    unsigned int arity)
{
    unsigned int i;

    for (i = 0; i < ecl->sub_cnt; ++i) {
        if (strcmp(ecl->subs[i].name, subroutine) == 0) {
            if (   ecl->subs[i].arity != -1
                && (unsigned int)ecl->subs[i].arity != arity) {
                fprintf(stderr, "%s:%s: arity mismatch %u %u for %s\n",
                    argv0, current_input,
                    ecl->subs[i].arity, arity, subroutine);
            } else {
                ecl->subs[i].arity = arity;
            }
            return;
        }
    }
}

static void
ecldump_translate(
    ecl_t* ecl,
    unsigned int version)
{
    unsigned int s, r, p;

    for (s = 0; s < ecl->sub_cnt; ++s) {
        sub_t* sub = &ecl->subs[s];

        sub->instrs = malloc(sizeof(instr_t) * sub->instr_cnt);

        for (r = 0; r < sub->instr_cnt; ++r) {
            const raw_instr_t* rinstr = &sub->raw_instrs[r];
            instr_t* instr = &sub->instrs[r];
            if (!instr_parse(version, rinstr, instr))
                exit(1);

            for (p = 0; p < instr->param_cnt; ++p) {
                const param_t* param = &instr->params[p];
                if (param->type == 's') {
                    if (instr->id == 11 || instr->id == 15)
                        set_arity(ecl, param->value.s.data, instr->param_cnt - 1);
                    else
                        set_arity(ecl, param->value.s.data, 0);
                }
            }

            instr->offset = rinstr->offset - sub->offset;

            if (instr->id == 40) {
                sub->stack = instr->params[0].value.i;
                --sub->instr_cnt;
                free(sub->raw_instrs[r].data);
                free(sub->instrs[r].params);
                sub->instrs = realloc(sub->instrs, sizeof(instr_t) * sub->instr_cnt);
                memmove(sub->raw_instrs, sub->raw_instrs + 1, sub->instr_cnt * sizeof(raw_instr_t));
                sub->raw_instrs = realloc(sub->raw_instrs, sub->instr_cnt * sizeof(raw_instr_t));
                --r;
            }
        }
    }
}

static void
ecldump_display_param(
    char* output,
    unsigned int output_length,
    const sub_t* sub,
    const instr_t* instr,
    unsigned int i,
    const param_t* param,
    unsigned int version)
{
    unsigned int c;
    /* Allow the input to be overridden for nested data. */
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
            if (param->value.i >= 0) {
                snprintf(output, output_length, "$%c",
                    'A' + (param->value.i / 4));
            } else {
                snprintf(output, output_length, "[%d]", param->value.i);
            }
        } else
            snprintf(output, output_length, "%d", param->value.i);
        break;
    case 'o':
        snprintf(output, output_length, "%s_%u",
            sub->name, instr->offset + param->value.i);
        break;
    case 'f': {
        const char* floatb = util_printfloat(&param->value.f);
        if (instr->param_mask & (1 << i)) {
            int tempint = param->value.f;
            if (floor(param->value.f) != param->value.f)
                fprintf(stderr, "%s:%s: non-integral float: %s\n",
                    argv0, current_input, floatb);
            if (tempint >= 0 && tempint % 4) {
                fprintf(stderr, "%s:%s: strange stack offset: %s\n",
                    argv0, current_input, floatb);
            }
            if (param->value.f >= 0) {
                snprintf(output, output_length, "%%%c",
                    'A' + (int)(param->value.f / 4));
            } else {
                snprintf(output, output_length, "[%sf]", floatb);
            }
        } else
            snprintf(output, output_length, "%sf", floatb);
        break;
    }
    case 'c':
        *output++ = 'C';
    case 's':
        *output++ = '"';
        for (c = 0; param->value.s.data[c]; ++c) {
            if (param->value.s.data[c] == '"')
                *output++ = '\\';
            *output++ = param->value.s.data[c];
        }
        *output++ = '"';
        *output++ = '\0';
        break;
    case 'D': {
        param_t newparam = *param;
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
        ecldump_display_param(output + strlen(output),
            output_length - strlen(output), sub, instr, i, &newparam, version);
    }
        break;
    default:
        break;
    }
}

static void
ecldump_render_instr(
    const sub_t* sub,
    instr_t* instr,
    instr_t** stack,
    int* stack_top,
    unsigned int version)
{
    int i;
    const op_pair_t* op;

    op = op_find_instr(version, instr->id);
    if (!op)
        return;

    if (*stack_top < op->arity)
        return;

    for (i = 0; i < op->arity; ++i) {
        if (!stack[*stack_top - 2 - i]->type)
            return;
        if (stack[*stack_top - 2 - i]->time != instr->time)
            return;
        if (stack[*stack_top - 2 - i]->rank_mask != instr->rank_mask)
            return;
        /* Only the edge entry may have a label. */
        if (i < op->arity - 1)
            if (stack[*stack_top - 2 - i]->label)
                return;
    }

    if (   op->token == IF
        || op->token == UNLESS
        || op->token == GOTO) {
        if (op->token == IF)
            snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "if %s ", stack[*stack_top - 2]->string);
        else if (op->token == UNLESS)
            snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "unless %s ", stack[*stack_top - 2]->string);
        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "goto ");
        ecldump_display_param(instr->string + strlen(instr->string), 1024 - strlen(instr->string), sub, instr, 0, NULL, version);
        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), " @ ");
        ecldump_display_param(instr->string + strlen(instr->string), 1024 - strlen(instr->string), sub, instr, 1, NULL, version);
    } else if (op->token == LOAD) {
        ecldump_display_param(instr->string, 1024, sub, instr, 0, NULL, version);
        instr->type = instr->params[0].type;
    } else if (op->token == ASSIGN) {
        ecldump_display_param(instr->string, 1024, sub, instr, 0, NULL, version);
        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), " %s %s", op->symbol, stack[*stack_top - 2]->string);
    } else if (op->arity == 1) {
        snprintf(instr->string, 1024, "%s%s", op->symbol, stack[*stack_top - 2]->string);
        instr->type = instr->id == op->op_1.instr ? op->op_1.result_type : op->op_2.result_type;
    } else if (op->arity == 2) {
        const char* symbol;
        instr->type = instr->id == op->op_1.instr ? op->op_1.result_type : op->op_2.result_type;

        if (instr->id == op->op_2.instr && (stack[*stack_top - 2]->type == 'f' || stack[*stack_top - 3]->type == 'f'))
            symbol = op->symbol;
        else if (instr->id == op->op_1.instr && (stack[*stack_top - 2]->type == 'i' && stack[*stack_top - 3]->type == 'i'))
            symbol = op->symbol;
        else
            symbol = instr->id == op->op_1.instr ? op->op_1.symbol : op->op_2.symbol;

        snprintf(instr->string, 1024, "(%s %s %s)", stack[*stack_top - 3]->string, symbol, stack[*stack_top - 2]->string);
    } else {
        return;
    }

    if (op->arity) {
        instr->label = stack[*stack_top - 1 - op->arity]->label;
        instr->offset = stack[*stack_top - 1 - op->arity]->offset;

        *stack_top -= op->arity;
        stack[*stack_top - 1] = instr;
    }
}

static void
ecldump_translate_print(
    ecl_t* ecl,
    unsigned int version)
{
    unsigned int s, p;

    if (ecl->anim_cnt) {
        unsigned int i;
        fprintf(out, "anim { ");
        for (i = 0; i < ecl->anim_cnt; ++i)
            fprintf(out, "\"%s\"; ", ecl->anim[i]);
        fprintf(out, "}\n");
    }

    if (ecl->ecli_cnt) {
        unsigned int i;
        fprintf(out, "ecli { ");
        for (i = 0; i < ecl->ecli_cnt; ++i)
            fprintf(out, "\"%s\"; ", ecl->ecli[i]);
        fprintf(out, "}\n");
    }

    for (s = 0; s < ecl->sub_cnt; ++s) {
        sub_t* sub = &ecl->subs[s];
        unsigned int j, k;
        unsigned int time;
        int stack_top = 0;
        unsigned int sub_arity = sub->arity == -1 ? 0 : sub->arity;

        instr_t** stack = malloc(sub->instr_cnt * sizeof(instr_t*));

        for (j = 0; j < sub->instr_cnt; ++j) {
            instr_t* instr = &sub->instrs[j];
            instr->string = malloc(1024);
            instr->string[0] = '\0';
            instr->type = 0;
            instr->label = 0;
            for (k = 0; k < sub->instr_cnt; ++k) {
                for (p = 0; p < sub->instrs[k].param_cnt; ++p) {
                    const param_t* param = &sub->instrs[k].params[p];
                    if (param->type == 'o' &&
                        instr->offset == sub->instrs[k].offset + param->value.i) {
                        instr->label = 1;

                        k = sub->instr_cnt;
                        break;
                    }
                }
            }
        }

        for (j = 0; j < sub->instr_cnt; ++j) {
            instr_t* instr = &sub->instrs[j];

            ++stack_top;
            stack[stack_top - 1] = instr;

            ecldump_render_instr(sub, instr, stack, &stack_top, version);

            if (!instr->string[0]) {
                snprintf(instr->string, 1024, "ins_%u", instr->id);

                for (p = 0; p < instr->param_cnt; ++p) {
                    param_t* param = &instr->params[p];
                    snprintf(instr->string + strlen(instr->string),
                        1024 - strlen(instr->string), " ");
                    if (   (instr->param_mask & (1 << p))
                        && (   ((param->type == 'i') && (param->value.i == -1))
                            || ((param->type == 'f') && (param->value.f == -1.0f)))
                        && instr->time == stack[stack_top - 2]->time
                        && stack[stack_top - 2]->type) {
                        snprintf(instr->string + strlen(instr->string), 1024 - strlen(instr->string), "%c(%s)",
                            param->type == 'i' ? '$' : '%',
                            stack[stack_top - 2]->string);
                        instr->label = stack[stack_top - 2]->label;
                        instr->offset = stack[stack_top - 2]->offset;
                        --stack_top;
                        stack[stack_top - 1] = instr;
                    } else {
                        ecldump_display_param(instr->string + strlen(instr->string),
                            1024 - strlen(instr->string), sub, instr, p,
                            NULL, version);
                    }
                }
            }
        }

        fprintf(out, "\nsub %s(", sub->name);
        for (p = 0; p < sub_arity; ++p) {
            if (p != 0)
                fprintf(out, " ");
            fprintf(out, "%c", 'A' + p);
        }
        fprintf(out, ")\n{\n");

        fprintf(out, "    var");
        for (p = sub_arity * 4; p < (unsigned int)sub->stack; p += 4) {
            fprintf(out, " %c", 'A' + (p / 4));
        }
        fprintf(out, ";\n");

        time = 0;
        for (j = 0; j < (unsigned int)stack_top; ++j) {
            if (stack[j]->time != time) {
                time = stack[j]->time;
                fprintf(out, "%u:\n", time);
            }

            if (stack[j]->label) {
                fprintf(out, "%s_%u:\n",
                    sub->name, stack[j]->offset);
            }

            if (stack[j]->id == 40)
                continue;

            fprintf(out, "    ");

            if (stack[j]->rank_mask != 0xff) {
                fprintf(out, "!");
                if (stack[j]->rank_mask & RANK_EASY)
                    fprintf(out, "E");
                if (stack[j]->rank_mask & RANK_NORMAL)
                    fprintf(out, "N");
                if (stack[j]->rank_mask & RANK_HARD)
                    fprintf(out, "H");
                if (stack[j]->rank_mask & RANK_LUNATIC)
                    fprintf(out, "L");
                fprintf(out, " ");
            }

            fprintf(out, "%s;\n", stack[j]->string);
        }
        free(stack);

        fprintf(out, "}\n");
    }
}

static void
ecldump_guess_params(
    const ecl_t* ecl)
{
    unsigned int s, r, p;

    char instr_params[1024][16];
    memset(instr_params, -1, 1024 * 16);

    for (s = 0; s < ecl->sub_cnt; ++s) {
        for (r = 0; r < ecl->subs[s].instr_cnt; ++r) {
            const raw_instr_t* rins = &ecl->subs[s].raw_instrs[r];

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
                for (p = 0; p < rins->param_cnt; ++p) {
                    if (instr_params[rins->id][p + 1] != 'i') {
                        /* TODO: This float checking stuff is pretty ugly,
                         * move it to util.c so it can be fixed more easily. */
                        int32_t integer = 0;
                        float floating = 0;
                        char buffer[128];
                        memcpy(&integer,
                               rins->data + p * sizeof(int32_t),
                               sizeof(int32_t));
                        memcpy(&floating,
                               rins->data + p * sizeof(int32_t),
                               sizeof(int32_t));
                        sprintf(buffer, "%.9f", floating);
                        if (((integer & 0xfffff000) == 0xfffff000) ||
                            (strcmp("nan", buffer) == 0) ||
                            (integer != 0 && strcmp("0.000000000", buffer) == 0))
                            instr_params[rins->id][p + 1] = 'i';
                        else
                            instr_params[rins->id][p + 1] = 'f';
                    }
                }
                break;
            }
        }
    }

    for (p = 0; p < 1024; ++p) {
        if (instr_params[p][0] >= 0) {
            fprintf(out, "    { %u, \"%s\" },\n", p, instr_params[p] + 1);
        } else if (instr_params[p][0] == -2) {
            fprintf(out, "    /*{ %u, \"%s\" },*/\n", p, instr_params[p] + 1);
        }
    }
}

static void
ecldump_rawdump(
    const ecl_t* ecl)
{
    unsigned int s, r, d;

    for (s = 0; s < ecl->sub_cnt; ++s) {
        fprintf(out, "Subroutine %s\n", ecl->subs[s].name);

        for (r = 0; r < ecl->subs[s].instr_cnt; ++r) {
            const raw_instr_t* rins = &ecl->subs[s].raw_instrs[r];

            fprintf(out, "Instruction %u %u %u 0x%02x %u %u: ",
                rins->time, rins->id, rins->size, rins->param_mask,
                rins->rank_mask, rins->param_cnt);

            for (d = 0; d < rins->data_size; d += sizeof(uint32_t)) {
                fprintf(out, "0x%08x ", *(uint32_t*)(rins->data + d));
            }

            fprintf(out, "\n");
        }

        fprintf(out, "\n");
    }
}

static void
ecldump_free(
    ecl_t* ecl)
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

    in = stdin;
    current_input = "(stdin)";
    out = stdout;
    current_output = "(stdout)";

    mode = parse_args(argc, argv, print_usage, "cdrphV", "", &version);

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
        if (!version) {
            print_usage();
            exit(1);
        }
        if (!compile_ecs(in, out, version))
            exit(0);
        fclose(in);
        fclose(out);
        exit(0);
    case 'd': {
        ecl_t ecl;
        if (!version) {
            print_usage();
            exit(1);
        }
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
        ecldump_guess_params(&ecl);
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
