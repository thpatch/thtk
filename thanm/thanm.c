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
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "file.h"
#include "image.h"
#include "thanm.h"
#include "program.h"
#include "util.h"
#include "value.h"
#include "mygetopt.h"
#include "reg.h"

anmmap_t* g_anmmap = NULL;
unsigned int option_force;

/* SPECIAL FORMATS:
 * 'o' - offset (for labels)
 * 't' - time (to be read from a label)
 * 'n' - sprite number, dumped as sprite name string
 * 'N' - script number, dumped as script name string
*/

static const id_format_pair_t formats_v0[] = {
    { 0, "" },
    { 1, "n" },
    { 2, "ff" },
    { 3, "S" },
    { 4, "S" },
    { 5, "o" },
    { 6, "" },
    { 7, "" },
    { 8, "" },
    { 9, "fff" },
    { 10, "fff" },
    { 11, "ff" },
    { 12, "SS" },
    { 13, "" },
    { 14, "" },
    { 15, "" },
    { 16, "nS" },
    { 17, "fff" },
    { 18, "fffS" },
    { 19, "fffS" },
    { 20, "fffS" },
    { 21, "" },
    { 22, "S" },
    { 23, "" },
    { 24, "" },
    { 25, "S" },
    { 26, "S" },
    { 27, "f" },
    { 28, "f" },
    { 29, "S" },
    { 30, "ffS" },
    { 31, "S" },
    { 0, NULL }
};

static const id_format_pair_t formats_v2[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "n" },
    { 4, "ot" },
    { 5, "Sot" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "S" },
    { 9, "S" },
    { 10, "" },
    { 11, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "SS" },
    { 16, "S" },
    { 17, "fffS" },
    { 18, "fffS" },
    { 19, "fffS" },
    { 20, "" },
    { 21, "S" },
    { 22, "" },
    { 23, "" },
    { 24, "S" },
    { 25, "S" },
    { 26, "f" },
    { 27, "f" },
    { 28, "S" },
    { 29, "ffS" },
    { 30, "S" },
    { 31, "S" },
    { 32, "SSfff" },
    { 33, "SSS" },
    { 34, "SSS" },
    { 35, "SSfff" },
    { 36, "SSff" },
    { 37, "SS" },
    { 38, "ff" },
    { 39, "SS" },
    { 40, "ff" },
    { 41, "SS" },
    { 42, "ff" },
    { 43, "SS" },
    { 44, "ff" },
    { 45, "SS" },
    { 46, "ff" },
    { 47, "SS" },
    { 48, "ff" },
    { 49, "SSS" },
    { 50, "fff" },
    { 51, "SSS" },
    { 52, "fff" },
    { 53, "SSS" },
    { 54, "fff" },
    { 55, "SSS" },
    { 56, "fff" },
    { 57, "SSS" },
    { 58, "fff" },
    { 59, "SS" },
    { 60, "ff" },
    { 61, "ff" },
    { 62, "ff" },
    { 63, "ff" },
    { 64, "ff" },
    { 65, "ff" },
    { 66, "f" },
    { 67, "SSot" },
    { 68, "ffot" },
    { 69, "SSot" },
    { 70, "ffot" },
    { 71, "SSot" },
    { 72, "ffot" },
    { 73, "SSot" },
    { 74, "ffot" },
    { 75, "SSot" },
    { 76, "ffot" },
    { 77, "SSot" },
    { 78, "ffot" },
    { 79, "S" },
    { 80, "f" },
    { 81, "f" },
    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v3[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "n" },
    { 4, "ot" },
    { 5, "Sot" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "S" },
    { 9, "SSS" },
    { 10, "" },
    { 11, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "SS" },
    { 16, "S" },
    { 17, "fffS" },
    { 18, "fffS" },
    { 19, "fffS" },
    { 20, "" },
    { 21, "S" },
    { 22, "" },
    { 23, "" },
    { 24, "S" },
    { 25, "S" },
    { 26, "f" },
    { 27, "f" },
    { 28, "S" },
    { 29, "ffS" },
    { 30, "S" },
    { 31, "S" },
    { 32, "SSfff" },
    { 33, "SSSSS" },
    { 34, "SSS" },
    { 35, "SSfff" },
    { 36, "SSff" },
    { 37, "SS" },
    { 38, "ff" },
    { 39, "SS" },
    { 40, "ff" },
    { 41, "SS" },
    { 42, "ff" },
    { 43, "SS" },
    { 44, "ff" },
    { 45, "SS" },
    { 46, "ff" },
    { 47, "SS" },
    { 48, "ff" },
    { 49, "SSS" },
    { 50, "fff" },
    { 51, "SSS" },
    { 52, "fff" },
    { 53, "SSS" },
    { 54, "fff" },
    { 55, "SSS" },
    { 56, "fff" },
    { 57, "SSS" },
    { 58, "fff" },
    { 59, "SS" },
    { 60, "ff" },
    { 61, "ff" },
    { 62, "ff" },
    { 63, "ff" },
    { 64, "ff" },
    { 65, "ff" },
    { 66, "f" },
    { 67, "SSot" },
    { 68, "ffot" },
    { 69, "SSot" },
    { 70, "ffot" },
    { 71, "SSot" },
    { 72, "ffot" },
    { 73, "SSot" },
    { 74, "ffot" },
    { 75, "SSot" },
    { 76, "ffot" },
    { 77, "SSot" },
    { 78, "ffot" },
    { 79, "S" },
    { 80, "f" },
    { 81, "f" },
    { 82, "S" },
    { 83, "S" },
    { 84, "SSS" },
    { 85, "S" },
    { 86, "SSSSS" },
    { 87, "SSS" },
    { 88, "S" },
    { 89, "" },
    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v4p[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "n" },
    { 4, "ot" },
    { 5, "Sot" },
    { 6, "SS" },
    { 7, "ff" },
    { 8, "SS" },
    { 9, "ff" },
    { 10, "SS" },
    { 11, "ff" },
    { 12, "SS" },
    { 13, "ff" },
    { 14, "SS" },
    { 15, "ff" },
    { 16, "SS" },
    { 17, "ff" },
    { 18, "SSS" },
    { 19, "fff" },
    { 20, "SSS" },
    { 21, "fff" },
    { 22, "SSS" },
    { 23, "fff" },
    { 24, "SSS" },
    { 25, "fff" },
    { 26, "SSS" },
    { 27, "fff" },
    { 28, "SSot" },
    { 29, "ffot" },
    { 30, "SSot" },
    { 31, "ffot" },
    { 32, "SSot" },
    { 33, "ffot" },
    { 34, "SSot" },
    { 35, "ffot" },
    { 36, "SSot" },
    { 37, "ffot" },
    { 38, "SSot" },
    { 39, "ffot" },
    { 40, "SS" },
    { 41, "ff" },
    { 42, "ff" },
    { 43, "ff" },
    { 44, "ff" },
    { 45, "ff" },
    { 46, "ff" },
    { 47, "f" },
    { 48, "fff" },
    { 49, "fff" },
    { 50, "ff" },
    { 51, "S" },
    { 52, "SSS" },
    { 53, "fff" },
    { 54, "ff" },
    { 55, "SS" },
    { 56, "SSfff" },
    { 57, "SSSSS" },
    { 58, "SSS" },
    { 59, "SSfff" },
    { 60, "SSff" },
    { 61, "" },
    { 62, "" },
    { 63, "" },
    { 64, "S" },
    { 65, "ss" },
    { 66, "S" },
    { 67, "S" },
    { 68, "S" },
    { 69, "" },
    { 70, "f" },
    { 71, "f" },
    { 72, "S" },
    { 73, "S" },
    { 74, "S" },
    { 75, "S" },
    { 76, "SSS" },
    { 77, "S" },
    { 78, "SSSSS" },
    { 79, "SSS" },
    { 80, "S" },
    { 81, "" },
    { 82, "S" },
    { 83, "" },
    { 84, "S" },
    { 85, "S" },
    { 86, "S" },
    { 87, "S" },
    { 88, "N" },
    { 89, "S" },
    { 90, "N" },
    { 91, "N" },
    { 92, "N" },
    { 93, "SSf" },
    { 94, "SSf" },
    { 95, "N" },
    { 96, "Nff" },
    { 97, "Nff" },
    { 98, "" },
    { 99, "S" },
    { 100, "Sfffffffff" },
    { 101, "S" },
    { 102, "nS" },
    { 103, "ff" },
    { 104, "fS" },
    { 105, "fS" },
    { 106, "ff" },
    { 107, "SSff" },
    { 108, "ff" },
    { 109, "ff" },
    { 110, "ff" },
    { 111, "S" },
    { 112, "S" },
    { 113, "SSf" },
    { 114, "S" },
    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v8[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "" },
    { 4, "" },
    { 5, "S" },
    { 6, "S" },
    { 7, "" },
    { 100, "SS" },
    { 101, "ff" },
    { 102, "SS" },
    { 103, "ff" },
    { 104, "SS" },
    { 105, "ff" },
    { 106, "SS" },
    { 107, "ff" },
    { 108, "SS" },
    { 109, "ff"},
    { 110, "SS"},
    { 111, "ff"},
    { 112, "SSS" },
    { 113, "fff" },
    { 114, "SSS" },
    { 115, "fff" },
    { 116, "SSS" },
    { 117, "fff" },
    { 118, "SSS" },
    { 119, "fff" },
    { 120, "SSS" },
    { 121, "fff" },
    { 122, "SS" },
    { 123, "ff" },
    { 124, "ff" },
    { 125, "ff" },
    { 126, "ff" },
    { 127, "ff" },
    { 128, "ff" },
    { 129, "f" },
    { 130, "ffff" },
    { 131, "ffff" },
    { 200, "ot" },
    { 201, "Sot" },
    { 202, "SSot" },
    { 203, "ffot" },
    { 204, "SSot" },
    { 205, "ffot" },
    { 206, "SSot" },
    { 207, "ffot" },
    { 208, "SSot" },
    { 209, "ffot" },
    { 210, "SSot" },
    { 211, "ffot" },
    { 212, "SSot" },
    { 213, "ffot" },
    { 300, "n" },
    { 301, "nS" },
    { 302, "S" },
    { 303, "S" },
    { 304, "S" },
    { 305, "S" },
    { 306, "S" },
    { 307, "S" },
    { 308, "" },
    { 309, "" },
    { 310, "S" },
    { 311, "S" },
    { 312, "SS" },
    { 313, "S" },
    { 314, "S" },
    { 315, "S" },
    { 316, "" },
    { 317, "" },
    { 400, "fff" },
    { 401, "fff" },
    { 402, "ff" },
    { 403, "S" },
    { 404, "SSS" },
    { 405, "S" },
    { 406, "SSS" },
    { 407, "SSfff" },
    { 408, "SSSSS" },
    { 409, "SSS" },
    { 410, "SSfff" },
    { 411, "SSf" },
    { 412, "SSff" },
    { 413, "SSSSS" },
    { 414, "SSS" },
    { 415, "fff" },
    { 416, "ff" },
    { 417, "SS" },
    { 418, "" },
    { 419, "S" },
    { 420, "Sfffffffff" },
    { 421, "ss" },
    { 422, "" },
    { 423, "S" },
    { 424, "S" },
    { 425, "f" },
    { 426, "f" },
    { 427, "SSf" },
    { 428, "SSf" },
    { 429, "ff" },
    { 430, "SSff" },
    { 431, "S" },
    { 432, "S" },
    { 433, "SSff" },
    { 434, "ff" },
    { 435, "SSff" },
    { 436, "ff" },
    { 437, "S" },
    { 438, "S" },
    { 439, "S" },
    { 500, "N" },
    { 501, "N" },
    { 502, "N" },
    { 503, "N" },
    { 504, "N" },
    { 505, "Nff" },
    { 506, "Nff" },
    { 507, "S" },
    { 508, "S" },
    { 509, "" },
    { 600, "S" },
    { 601, "S" },
    { 602, "S" },
    { 603, "ff" },
    { 604, "fS" },
    { 605, "fS" },
    { 606, "ff" },
    { 607, "ff" },
    { 608, "ff" },
    { 609, "S" },
    { 610, "S" },
    { 611, "ffS" },
    { 612, "ff" },
    { 613, "ff" },
    { 614, "ff" },
    { 0xffff, "" },
    { 0, NULL }
};

/* The order and sizes of fields changed for TH11. */
static void
convert_header_to_old(
    anm_header06_t* header)
{
    anm_header11_t th11 = *(anm_header11_t*)header;
    header->sprites = th11.sprites;
    header->scripts = th11.scripts;
    header->rt_textureslot = 0;
    header->w = th11.w;
    header->h = th11.h;
    header->format = th11.format;
    header->colorkey = 0;
    header->nameoffset = th11.nameoffset;
    header->x = th11.x;
    header->y = th11.y;
    header->version = th11.version;
    header->memorypriority = th11.memorypriority;
    header->thtxoffset = th11.thtxoffset;
    header->hasdata = th11.hasdata;
    header->lowresscale = th11.lowresscale;
    header->nextoffset = th11.nextoffset;
    header->zero3 = 0;
}

#ifdef HAVE_LIBPNG
static void
convert_header_to_11(
    anm_header06_t* oldheader)
{
    anm_header06_t header = *oldheader;
    anm_header11_t* th11 = (anm_header11_t*)oldheader;
    memset(th11, 0, sizeof(anm_header11_t));
    th11->sprites = header.sprites;
    th11->scripts = header.scripts;
    th11->zero1 = 0;
    th11->w = header.w;
    th11->h = header.h;
    th11->format = header.format;
    th11->nameoffset = header.nameoffset;
    th11->x = header.x;
    th11->y = header.y;
    th11->version = header.version;
    th11->memorypriority = header.memorypriority;
    th11->thtxoffset = header.thtxoffset;
    th11->hasdata = header.hasdata;
    th11->lowresscale = header.lowresscale;
    th11->nextoffset = header.nextoffset;
}
#endif

anm_script_t* anm_script_new() {
    anm_script_t* script = (anm_script_t*)malloc(sizeof(anm_script_t));
    list_init(&script->labels);
    list_init(&script->instrs);
    list_init(&script->raw_instrs);
    list_init(&script->vars);
    script->offset = NULL;
    return script;
}

var_t* var_new(
    char* name,
    int type
) {
    var_t* var = (var_t*)malloc(sizeof(var_t));
    var->name = name;
    var->type = type;
    var->reg = NULL;
    return var;
}

void var_free(
    var_t* var
) {
    free(var->name);
    free(var);
}

const id_format_pair_t*
anm_get_formats(
    uint32_t version
) {
    switch (version) {
    case 0:
        return formats_v0;
    case 2:
        return formats_v2;
    case 3:
        return formats_v3;
    case 4:
    case 7:
        return formats_v4p;
    case 8:
        return formats_v8;
    default:
        fprintf(stderr,
            "%s:%s: could not find a format description for version %u\n",
            argv0, current_input, version);
        abort();
    }
}

static char*
anm_get_name(
    anm_archive_t* archive,
    const char* name)
{
    char* other_name;
    list_for_each(&archive->names, other_name) {
        if (strcmp(name, other_name) == 0)
            return other_name;
    }

    other_name = strdup(name);
    list_append_new(&archive->names, other_name);
    return other_name;
}

thanm_param_t*
thanm_param_new(
    int type
) {
    thanm_param_t* ret = (thanm_param_t*)util_malloc(sizeof(thanm_param_t));
    ret->is_var = 0;
    ret->type = type;
    ret->expr = NULL;
    ret->val = NULL;
    return ret;
}

void
thanm_param_free(
    thanm_param_t* param
) {
    if (param->val)
        value_free(param->val);
    free(param);
}

static void
thanm_make_params(
    anm_instr_t* raw_instr,
    list_t* param_list,
    const char* format
) {
    size_t i = 0;
    size_t v = 0;
    while(i < raw_instr->length - sizeof(anm_instr_t)) {
        value_t* value = (value_t*)malloc(sizeof(value_t));
        ssize_t read;
        char c = format ? format[v] : 'S';
        switch(c) {
            case 'o':
            case 't':
            case 'n':
            case 'N':
                read = value_from_data((const unsigned char*)&raw_instr->data[i],
                    raw_instr->length - sizeof(anm_instr_t) - i, 'S', value);
                break;
            default:
                read = value_from_data((const unsigned char*)&raw_instr->data[i],
                    raw_instr->length - sizeof(anm_instr_t) - i, c, value);
                break;
        }

        if (read == -1) {
            fprintf(stderr, "%s: value read error\n", argv0);
            abort();
        }

        i += read;
        thanm_param_t* param = thanm_param_new(c);
        if (raw_instr->param_mask & 1 << v) {
            param->is_var = 1;
        }

        param->val = value;

        list_append_new(param_list, param);
        ++v;
    }
}

/* Used to make sure that the string contains a valid identifier after
 * using sprintf to put a number in it. */
static void
replace_minus(
    char* str
) {
    for (size_t i = 0; str[i] != '\0'; ++i) {
        if (str[i] == '-')
            str[i] = 'M';
    }
}

static int
anm_is_valid_script_index(
    const anm_archive_t* anm,
    int32_t index
) {
    anm_entry_t* entry;
    anm_script_t* script;
    list_for_each(&anm->entries, entry) {
        list_for_each(&entry->scripts, script) {
            if (script->real_index == index)
                return 1;
        }
    }
    return 0;
}

static void
anm_stringify_param(
    FILE* stream,
    thanm_param_t* param,
    thanm_instr_t* instr,
    const anm_archive_t* anm,
    int32_t scriptn
) {
    char* disp = NULL;
    char* dest = NULL;
    char buf[256];

    switch(param->type) {
        case 'o':
            sprintf(buf, "offset%d", param->val->val.S);
            replace_minus(buf);
            dest = buf;
            break;
        case 'n':
            /* Sprite -1 is actually sometimes used to indicate no sprite. */
            if (param->val->val.S < 0)
                sprintf(buf, "%d", param->val->val.S);
            else
                sprintf(buf, "sprite%d", param->val->val.S);
            dest = buf;
            break;
        case 'N':
            if (anm_is_valid_script_index(anm, param->val->val.S)) {
                sprintf(buf, "script%d", param->val->val.S);
                replace_minus(buf);
            } else {
                sprintf(buf, "%d", param->val->val.S);
            }
            dest = buf;
            break;
        default:
            disp = value_to_text(param->val);
            dest = disp;
            break;
    }

    if (param->is_var) {
        int val;
        if (param->val->type == 'f')
            val = floorf(param->val->val.f);
        else if (param->val->type == 'S')
            val = param->val->val.S;
        else if (param->val->type == 's')
            val = param->val->val.s;

        seqmap_entry_t* ent = seqmap_get(g_anmmap->gvar_names, val);
        if (ent) {
            fprintf(stream, "%c%s", param->val->type == 'f' ? '%' : '$', ent->value);
        }
        else {
            fprintf(stream, "[%s]", dest);
        }
    }
    else {
        fprintf(stream, "%s", dest);
    }

    if (dest == disp)
        free(disp);
}

thanm_instr_t*
thanm_instr_new() {
    thanm_instr_t* ret = (thanm_instr_t*)util_malloc(sizeof(thanm_instr_t));
    list_init(&ret->params);
    return ret;
}

uint32_t
instr_get_size(
    thanm_instr_t* instr,
    int32_t version
) {
    uint32_t size = version == 0 ? sizeof(anm_instr0_t) : sizeof(anm_instr_t);
    thanm_param_t* param;
    list_for_each(&instr->params, param) {
        switch(param->type) {
            case 's':
                size += sizeof(int16_t);
                break;
            default:
                size += 4;
        }
    }

    return size;
}

thanm_instr_t*
instr_new(
    parser_state_t* state,
    uint16_t id,
    list_t* params
) {
    thanm_instr_t* instr = thanm_instr_new();
    instr->type = THANM_INSTR_INSTR;
    instr->time = state->time;
    instr->offset = state->offset;
    instr->id = id;
    instr->params = *params;
    free(params);
    instr->size = instr_get_size(instr, state->current_version);
    state->offset += instr->size;
    return instr;
}

static thanm_instr_t*
thanm_instr_new_raw(
    anm_instr_t* raw_instr,
    const char* format
) {
    thanm_instr_t* ret = thanm_instr_new();
    ret->type = THANM_INSTR_INSTR;
    ret->time = raw_instr->time;
    ret->id = raw_instr->type;
    ret->size = raw_instr->length;
    ret->param_mask = raw_instr->param_mask;
    thanm_make_params(raw_instr, &ret->params, format);
    return ret;
}

static thanm_instr_t*
thanm_instr_new_time(
    int16_t time
) {
    thanm_instr_t* ret = thanm_instr_new();
    ret->type = THANM_INSTR_TIME;
    ret->time = time;
    ret->id = -1;
    return ret;
}

static thanm_instr_t*
thanm_instr_new_label() {
    thanm_instr_t* ret = thanm_instr_new();
    ret->type = THANM_INSTR_LABEL;
    ret->time = 0;
    ret->id = -1;
    return ret;
}

static void
thanm_instr_free(
    thanm_instr_t* instr
) {
    thanm_param_t* param;
    list_for_each(&instr->params, param)
        thanm_param_free(param);

    free(instr);
}

static void
anm_insert_labels(
    anm_script_t* script,
    int32_t scriptn
) {
    thanm_instr_t* instr;
    list_for_each(&script->instrs, instr) {
        if (instr->type != THANM_INSTR_INSTR)
            continue;

        thanm_param_t* param;
        list_for_each(&instr->params, param) {
            if (param->type == 'o') {
                uint32_t offset = param->val->val.S;
                thanm_instr_t* search_instr;
                list_node_t* node;
                list_node_t* instr_node;
                list_for_each_node(&script->instrs, node) {
                    thanm_instr_t* iter_instr = (thanm_instr_t*)node->data;
                    if (iter_instr->type == THANM_INSTR_LABEL && iter_instr->offset == offset) {
                        /* Label already exists. */
                        break;
                    }
                    if (iter_instr->type == THANM_INSTR_INSTR)  {
                        search_instr = iter_instr;
                        instr_node = node;
                        if (search_instr-> type == THANM_INSTR_INSTR && search_instr->offset == offset) {
                            thanm_instr_t* instr_label = thanm_instr_new_label();
                            instr_label->offset = offset;
                            list_prepend_to(&script->instrs, instr_label, instr_node);
                            break;
                        }
                    }
                }
                /* There is a possibility that the label has to be inserted after the last instruction,
                 * and we can know that we need to do that if the loop didn't end with a break (when node is NULL) */
                if (node == NULL && search_instr->offset + search_instr->size == offset) {
                    thanm_instr_t* instr_label = thanm_instr_new_label();
                    instr_label->offset = offset;
                    list_append_to(&script->instrs, instr_label, instr_node);
                }
            }
        }
    }
}

static anm_archive_t*
anm_read_file(
    FILE* in)
{
    anm_archive_t* archive = malloc(sizeof(*archive));
    list_init(&archive->names);
    list_init(&archive->entries);

    long file_size;
    unsigned char* map_base;
    unsigned char* map;

    archive->map_size = file_size = file_fsize(in);
    archive->map = map_base = file_mmap(in, file_size);
    map = map_base;

    int32_t scriptn = 0;
    for (;;) {
        anm_entry_t* entry = malloc(sizeof(*entry));
        anm_header06_t* header = (anm_header06_t*)map;

        list_append_new(&archive->entries, entry);

        if(header->rt_textureslot != 0) {
            header = malloc(sizeof(*header));
            memcpy(header, map, sizeof(*header));
            convert_header_to_old(header);
        }

        entry->header = header;

        assert(
            header->version == 0 ||
            header->version == 2 ||
            header->version == 3 ||
            header->version == 4 ||
            header->version == 7 ||
            header->version == 8);

        assert(header->hasdata == 0 || header->hasdata == 1);
        assert(header->rt_textureslot == 0);
        assert(header->zero3 == 0);

        if(header->version == 8)
            assert(header->lowresscale == 0 || header->lowresscale == 1);

        /* Lengths, including padding, observed are: 16, 32, 48. */
        entry->name = anm_get_name(archive, (const char*)map + header->nameoffset);
        if (header->version == 0 && header->y != 0)
            entry->name2 = (char*)map + header->y;
        else
            entry->name2 = NULL;

        assert(
            (header->hasdata == 0 || (entry->name && entry->name[0] == '@')) ==
            (header->thtxoffset == 0));

        list_init(&entry->sprites);
        if (header->sprites) {
            uint32_t* sprite_offsets = (uint32_t*)(map + sizeof(*header));
            for (uint32_t s = 0; s < header->sprites; ++s) {
                list_append_new(&entry->sprites, (sprite_t*)(map + sprite_offsets[s]));
            }
        }

        id_format_pair_t* formats = anm_get_formats(header->version);

        list_init(&entry->scripts);
        if (header->scripts) {
            anm_offset_t* script_offsets = 
                (anm_offset_t*)(map + sizeof(*header) + header->sprites * sizeof(uint32_t));
            for (uint32_t s = 0; s < header->scripts; ++s) {
                anm_script_t* script = anm_script_new();
                script->real_index = scriptn;
                script->offset = &(script_offsets[s]);

                unsigned char* limit = map;
                if (s < header->scripts - 1)
                    limit += script_offsets[s + 1].offset;
                else if (header->thtxoffset)
                    limit += header->thtxoffset;
                else if (header->nextoffset)
                    limit += header->nextoffset;
                else
                    limit += file_size;

                unsigned char* instr_ptr = map + script->offset->offset;
                anm_instr_t* instr;
                size_t len;
                int16_t time = 0;
                for (;;) {
                    if (header->version == 0) {
                        anm_instr0_t* temp_instr = (anm_instr0_t*)instr_ptr;

                        if (instr_ptr + sizeof(anm_instr0_t) > limit ||
                            (temp_instr->type == 0 && temp_instr->time == 0) ||
                            instr_ptr + sizeof(anm_instr0_t) + temp_instr->length > limit)
                            break;

                        instr = util_malloc(sizeof(anm_instr_t) + temp_instr->length);
                        instr->type = temp_instr->type;
                        instr->length = sizeof(anm_instr_t) + temp_instr->length;
                        instr->time = temp_instr->time;
                        instr->param_mask = 0;
                        memcpy(instr->data, temp_instr->data, temp_instr->length);
                        
                        len = sizeof(anm_instr0_t) + temp_instr->length;
                    } else {
                        instr = (anm_instr_t*)instr_ptr;

                        if (instr_ptr + sizeof(anm_instr_t) > limit ||
                            instr->type == 0xffff ||
                            instr_ptr + instr->length > limit)
                            break;

                        len = instr->length;
                    }

                    if (instr->time != time) {
                        thanm_instr_t* time_instr = thanm_instr_new_time(instr->time);
                        list_append_new(&script->instrs, time_instr);
                        time = instr->time;
                    }

                    const char* format = find_format(formats, instr->type);
                    if (!format) {
                        fprintf(stderr, "id %d was not found in the format table (total parameter size was %d)\n",
                            instr->type, (int)(instr->length - sizeof(anm_instr_t)));
                    }
                    thanm_instr_t* thanm_instr = thanm_instr_new_raw(instr, format);
                    thanm_instr->offset = (uint32_t)((ptrdiff_t)instr_ptr - (ptrdiff_t)(map + script->offset->offset));
                    list_append_new(&script->instrs, thanm_instr);

                    if (header->version == 0)
                        free(instr);

                    instr_ptr += len;
                }

                anm_insert_labels(script, scriptn);
                list_append_new(&entry->scripts, script);
                ++scriptn;
            }
        }

        if (header->hasdata) {
            thtx_header_t* thtx = entry->thtx =
                (thtx_header_t*)(map + header->thtxoffset);
            assert(util_strcmp_ref(thtx->magic, stringref("THTX")) == 0);
            assert(thtx->zero == 0);
            assert(thtx->w * thtx->h * format_Bpp(thtx->format) <= thtx->size);
            assert(
                thtx->format == FORMAT_BGRA8888 ||
                thtx->format == FORMAT_RGB565 ||
                thtx->format == FORMAT_ARGB4444 ||
                thtx->format == (uint16_t)FORMAT_RGBA8888 ||
                thtx->format == FORMAT_GRAY8);

            entry->data = thtx->data;
        }

        if (!header->nextoffset)
            break;

        map = map + header->nextoffset;
    }

    return archive;
}

static void
anm_dump_old(
    FILE* stream,
    const anm_archive_t* anm)
{
    unsigned int entry_num = 0;
    anm_entry_t* entry;

    list_for_each(&anm->entries, entry) {
        const id_format_pair_t* formats = anm_get_formats(entry->header->version);

        fprintf(stream, "ENTRY #%u, VERSION %u\n", entry_num++, entry->header->version);
        fprintf(stream, "Name: %s\n", entry->name);
        if (entry->name2)
            fprintf(stream, "Name2: %s\n", entry->name2);
        fprintf(stream, "Format: %u\n", entry->header->format);
        fprintf(stream, "Width: %u\n", entry->header->w);
        fprintf(stream, "Height: %u\n", entry->header->h);
        if (entry->header->x != 0)
            fprintf(stream, "X-Offset: %u\n", entry->header->x);
        if (!entry->name2 && entry->header->y != 0)
            fprintf(stream, "Y-Offset: %u\n", entry->header->y);
        if (entry->header->version < 7) {
            fprintf(stream, "ColorKey: %08x\n", entry->header->colorkey);
        }
        if (entry->header->zero3 != 0)
            fprintf(stream, "Zero3: %u\n", entry->header->zero3);
        if (entry->header->version >= 1)
            fprintf(stream, "MemoryPriority: %u\n", entry->header->memorypriority);
        if (entry->header->version >= 8)
            fprintf(stream, "LowResScale: %u\n", entry->header->lowresscale);
        if (entry->header->hasdata) {
            fprintf(stream, "HasData: %u\n", entry->header->hasdata);
            fprintf(stream, "THTX-Size: %u\n", entry->thtx->size);
            fprintf(stream, "THTX-Format: %u\n", entry->thtx->format);
            fprintf(stream, "THTX-Width: %u\n", entry->thtx->w);
            fprintf(stream, "THTX-Height: %u\n", entry->thtx->h);
            fprintf(stream, "THTX-Zero: %u\n", entry->thtx->zero);
        }

        fprintf(stream, "\n");

        sprite_t* sprite;
        list_for_each(&entry->sprites, sprite) {
            fprintf(stream, "Sprite: %u %.f*%.f+%.f+%.f\n",
                sprite->id,
                sprite->w, sprite->h,
                sprite->x, sprite->y);
        }

        fprintf(stream, "\n");

        anm_script_t* script;
        list_for_each(&entry->scripts, script) {

            fprintf(stream, "Script: %d\n", script->offset->id);

            unsigned int instr_num = 0;
            thanm_instr_t* instr;
            list_for_each(&script->instrs, instr) {
                if (instr->type != THANM_INSTR_INSTR)
                    continue;
                
                fprintf(stream, "Instruction #%u: %hd %hu %hu",
                    instr_num++, instr->time, instr->param_mask, instr->id);

                thanm_param_t* param;
                list_for_each(&instr->params, param) {
                    char* disp = value_to_text(param->val);
                    fprintf(stream, " %s", disp);
                    free(disp);
                }

                fprintf(stream, "\n");
            }

            fprintf(stream, "\n");
        }

        fprintf(stream, "\n");
    }
}

static void
anm_stringify_instr(
    FILE* stream,
    thanm_instr_t* instr,
    const anm_archive_t* anm,
    int32_t scriptn
) {
    seqmap_entry_t* ent = seqmap_get(g_anmmap->ins_names, instr->id);

    if (ent)
        fprintf(stream, "%s(", ent->value);
    else
        fprintf(stream, "ins_%d(", instr->id);

    thanm_param_t* param;
    list_for_each(&instr->params, param) {
        anm_stringify_param(stream, param, instr, anm, scriptn);
        if (!list_is_last_iteration()) {
            fprintf(stream, ", ");
        }
    }

    fprintf(stream, ");\n");
}

static void
anm_dump(
    FILE* stream,
    const anm_archive_t* anm)
{
    char buf[256];
    unsigned int entry_num = 0;
    anm_entry_t* entry;

    int prev_sprite_id = -1;
    int prev_script_id = -1;
    list_for_each(&anm->entries, entry) {
        const id_format_pair_t* formats = anm_get_formats(entry->header->version);

        fprintf(stream, "entry entry%u {\n", entry_num++);
        fprintf(stream, "    version: %u,\n", entry->header->version);
        fprintf(stream, "    name: \"%s\",\n", entry->name);
        if (entry->name2)
            fprintf(stream, "    name2: \"%s\",\n", entry->name2);
        fprintf(stream, "    format: %u,\n", entry->header->format);
        fprintf(stream, "    width: %u,\n", entry->header->w);
        fprintf(stream, "    height: %u,\n", entry->header->h);
        if (entry->header->x != 0)
            fprintf(stream, "    xOffset: %u,\n", entry->header->x);
        if (!entry->name2 && entry->header->y != 0)
            fprintf(stream, "    yOffset: %u,\n", entry->header->y);
        if (entry->header->version < 7) {
            fprintf(stream, "    colorKey: 0x%08x,\n", entry->header->colorkey);
        }
        if (entry->header->zero3 != 0)
            fprintf(stream, "    zero3: %u,\n", entry->header->zero3);
        if (entry->header->version >= 1)
            fprintf(stream, "    memoryPriority: %u,\n", entry->header->memorypriority);
        if (entry->header->version >= 8)
            fprintf(stream, "    lowResScale: %u,\n", entry->header->lowresscale);

        fprintf(stream, "    hasData: %u,\n", entry->header->hasdata);
        if (entry->header->hasdata) {
            fprintf(stream, "    THTXSize: %u,\n", entry->thtx->size);
            fprintf(stream, "    THTXFormat: %u,\n", entry->thtx->format);
            fprintf(stream, "    THTXWidth: %u,\n", entry->thtx->w);
            fprintf(stream, "    THTXHeight: %u,\n", entry->thtx->h);
            fprintf(stream, "    THTXZero: %u,\n", entry->thtx->zero);
        }

        fprintf(stream, "    sprites: {\n");

        sprite_t* sprite;
        list_for_each(&entry->sprites, sprite) {
            if (prev_sprite_id + 1 != sprite->id) {
                fprintf(stream, "        sprite%u: { x: %.f, y: %.f, w: %.f, h: %.f, id: %d }",
                    sprite->id,
                    sprite->x, sprite->y,
                    sprite->w, sprite->h,
                    sprite->id);
            } else {
                fprintf(stream, "        sprite%u: { x: %.f, y: %.f, w: %.f, h: %.f }",
                    sprite->id,
                    sprite->x, sprite->y,
                    sprite->w, sprite->h);
            }
            if (!list_is_last_iteration())
                fprintf(stream, ",");
            fprintf(stream, "\n");
            prev_sprite_id = sprite->id;
        }

        fprintf(stream, "    }\n}\n\n");

        anm_script_t* script;
        list_for_each(&entry->scripts, script) {
            /* We need to use the index of the script in file for the name, because that's
             * what instructions that refer to scripts use. I'm unsure what's the actual purpose of the ID field.
             * However! I still don't really know how old versions work, and maybe instructions refer to the
             * ID field there? If that's the case, it should be enough to just do a check earlier that sets
             * real_index to offset->id in old versions (and re-add the replace_minus calls).
             * I honestly doubt that this is the case, but I want to leave a note how to change it anyway in case
             * it's needed... */
            if (script->offset->id - 1 != prev_script_id) {
                fprintf(stream, "script %d script%d {\n", script->offset->id, script->real_index);
            } else {
                fprintf(stream, "script script%d {\n", script->real_index);
            }
            prev_script_id = script->offset->id;

            thanm_instr_t* instr;
            int time = 0;
            int is_negative_time = 0;
            list_for_each(&script->instrs, instr) {
                switch(instr->type) {
                    case THANM_INSTR_INSTR:
                        fprintf(stream, "    ");
                        anm_stringify_instr(stream, instr, anm, script->real_index);
                        break;
                    case THANM_INSTR_TIME:
                        if (instr->time < 0)
                            is_negative_time = 1;

                        if (is_negative_time)
                            fprintf(stream, "%d:\n", instr->time);
                        else
                            fprintf(stream, "+%d: // %d\n", instr->time - time, instr->time);

                        time = instr->time;

                        /* This is here so that the instruction after the one with
                         * negative time is also dumped as an absolute label. */
                        if (instr->time >= 0)
                            is_negative_time = 0;
                        break;
                    case THANM_INSTR_LABEL:
                        fprintf(stream, "offset%u:\n", instr->offset);
                        break;
                }
            }

            fprintf(stream, "}\n\n");
        }

        fprintf(stream, "\n");
    }
}

#ifdef HAVE_LIBPNG
static void
util_total_entry_size(
    const anm_archive_t* anm,
    const char* name,
    unsigned int* widthptr,
    unsigned int* heightptr)
{
    unsigned int width = 0;
    unsigned int height = 0;

    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        if (entry->name == name) {
            if (!entry->header->hasdata)
                return;
            if (entry->header->x + entry->thtx->w > width)
                width = entry->header->x + entry->thtx->w;
            if (entry->header->y + entry->thtx->h > height)
                height = entry->header->y + entry->thtx->h;
        }
    }

    *widthptr = width;
    *heightptr = height;
}

static void
anm_replace(
    const anm_archive_t* anm,
    FILE* anmfp,
    const char* name,
    const char* filename)
{
    const format_t formats[] = {
        FORMAT_RGBA8888,
        FORMAT_BGRA8888,
        FORMAT_RGB565,
        FORMAT_ARGB4444,
        FORMAT_GRAY8
    };
    unsigned int f;
    unsigned int width = 0;
    unsigned int height = 0;
    image_t* image;

    util_total_entry_size(anm, name, &width, &height);
    if (width == 0 || height == 0) {
        /* There's nothing to do. */
        return;
    }

    image = png_read(filename);

    if (width != image->width || height != image->height) {
        fprintf(stderr,
            "%s:%s:%s: wrong image dimensions for %s: %u, %u instead of %u, %u\n",
            argv0, current_input, name, filename, image->width, image->height,
            width, height);
        exit(1);
    }

    for (f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {

        long offset = 0;
        anm_entry_t* entry;
        list_for_each(&anm->entries, entry) {
            if (entry->name == name &&
                entry->thtx->format == formats[f] &&
                entry->header->hasdata) {
                unsigned int y;

                unsigned char* converted_data = format_from_rgba((uint32_t*)image->data, width * height, formats[f]);

                if (anmfp) {
                    for (y = entry->header->y; y < entry->header->y + entry->thtx->h; ++y) {
                        if (!file_seek(anmfp,
                            offset + entry->header->thtxoffset + sizeof(thtx_header_t) + (y - entry->header->y) * entry->thtx->w * format_Bpp(formats[f])))
                            exit(1);
                        if (!file_write(anmfp, converted_data + y * width * format_Bpp(formats[f]) + entry->header->x * format_Bpp(formats[f]), entry->thtx->w * format_Bpp(formats[f])))
                            exit(1);
                    }
                } else {
                    for (y = entry->header->y; y < entry->header->y + entry->thtx->h; ++y) {
                        memcpy(entry->data + (y - entry->header->y) * entry->thtx->w * format_Bpp(formats[f]),
                               converted_data + y * width * format_Bpp(formats[f]) + entry->header->x * format_Bpp(formats[f]),
                               entry->thtx->w * format_Bpp(formats[f]));
                    }
                }

                free(converted_data);
            }

            offset += entry->header->nextoffset;
        }
    }

    free(image->data);
    free(image);
}

static void
anm_extract(
    const anm_archive_t* anm,
    const char* name)
{
    const format_t formats[] = {
        FORMAT_GRAY8,
        FORMAT_ARGB4444,
        FORMAT_RGB565,
        FORMAT_BGRA8888,
        FORMAT_RGBA8888
    };
    image_t image;

    unsigned int f, y;

    image.width = 0;
    image.height = 0;
    image.format = FORMAT_RGBA8888;

    util_total_entry_size(anm, name, &image.width, &image.height);

    if (image.width == 0 || image.height == 0) {
        /* Then there's nothing to extract. */
        return;
    }

    image.data = malloc(image.width * image.height * 4);
    /* XXX: Why 0xff? */
    memset(image.data, 0xff, image.width * image.height * 4);

    for (f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {
        anm_entry_t* entry;
        list_for_each(&anm->entries, entry) {
            if (entry->header->hasdata && entry->name == name && formats[f] == entry->thtx->format) {
                unsigned char* temp_data = format_to_rgba(entry->data, entry->thtx->w * entry->thtx->h, entry->thtx->format);
                for (y = entry->header->y; y < entry->header->y + entry->thtx->h; ++y) {
                    memcpy(image.data + y * image.width * 4 + entry->header->x * 4,
                           temp_data + (y - entry->header->y) * entry->thtx->w * 4,
                           entry->thtx->w * 4);
                }
                free(temp_data);
            }
        }
    }

    util_makepath(name);
    png_write(name, &image);

    free(image.data);
}

static char*
filename_cut(
    char *line,
    size_t len)
{
    char *p = line;

    assert(line);

    if(len == 0) {
        return line;
    }

    /* isspace(3) is only asking for trouble; we don't parse Unicode anyway. */
#define is_space(c) (c == ' ' || c == '\t')

    while(len > 0 && is_space(*p)) {
        len--;
        p++;
    }

    char *start = p;
    char *end = p;

    while(len > 0 && *p != '\n') {
        if(!is_space(*p)) {
            end = p;
        }
        len--;
        p++;
    }

#undef is_space

    end[len == 0 ? 0 : 1] = '\0';
    return start;
}

static anm_archive_t*
anm_create_old(
    const char* spec)
{
    FILE* f;
    char line[4096];
    anm_archive_t* anm;
    anm_entry_t* entry = NULL;
    anm_script_t* script = NULL;
    anm_instr_t* instr = NULL;
    unsigned int linenum = 1;

    f = fopen(spec, "r");
    if (!f) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
            argv0, spec, strerror(errno));
        exit(1);
    }

#define ERROR(text, ...) fprintf(stderr, \
    "%s: %s:%d: " text "\n", argv0, spec, linenum, ##__VA_ARGS__)

#define SCAN_DEPRECATED(fieldstr, format, field) \
    if (sscanf(line, fieldstr ": " format, &entry->header->field) > 0) { \
        ERROR("warning: " fieldstr " is an old field written by thanm <= 10; re-dump the spec after ANM creation to remove this warning"); \
    };

    anm = malloc(sizeof(anm_archive_t));
    anm->map = NULL;
    anm->map_size = 0;
    list_init(&anm->names);
    list_init(&anm->entries);

    while (fgets(line, sizeof(line), f)) {
        if (util_strcmp_ref(line, stringref("ENTRY ")) == 0) {
            entry = malloc(sizeof(*entry));
            entry->header = calloc(1, sizeof(*entry->header));
            entry->thtx = calloc(1, sizeof(*entry->thtx));
            entry->thtx->magic[0] = 'T';
            entry->thtx->magic[1] = 'H';
            entry->thtx->magic[2] = 'T';
            entry->thtx->magic[3] = 'X';
            entry->name = NULL;
            entry->name2 = NULL;
            list_init(&entry->sprites);
            list_init(&entry->scripts);
            entry->data = NULL;
            list_append_new(&anm->entries, entry);

            if(sscanf(line, "ENTRY %u", &entry->header->version) > 0) {
                ERROR("warning: No entry number detected. This spec was written by thanm <= 10; re-dump it after ANM creation to remove this warning");
            } else {
                unsigned int temp;
                sscanf(line, "ENTRY #%u, VERSION %u", &temp, &entry->header->version);
            }
        } else {
            if (entry == NULL) {
                ERROR("missing ENTRY\n");
                exit(1);
            }
            if (util_strcmp_ref(line, stringref("Name: ")) == 0) {
                size_t offset = stringref("Name: ").len;
                char *name = filename_cut(line + offset, sizeof(line) - offset);
                entry->name = anm_get_name(anm, name);
            } else if (util_strcmp_ref(line, stringref("Name2: ")) == 0) {
                size_t offset = stringref("Name2: ").len;
                char *name = filename_cut(line + offset, sizeof(line) - offset);
                entry->name2 = strdup(name);
            } else if (util_strcmp_ref(line, stringref("Sprite: ")) == 0) {
                sprite_t* sprite = malloc(sizeof(*sprite));
                list_append_new(&entry->sprites, sprite);

                if (5 != sscanf(line, "Sprite: %u %f*%f+%f+%f",
                             &sprite->id, &sprite->w, &sprite->h,
                             &sprite->x, &sprite->y)) {
                    ERROR("Sprite parsing failed for %s", line);
                    exit(1);
                }
            } else if (util_strcmp_ref(line, stringref("Script: ")) == 0) {
                script = malloc(sizeof(*script));
                script->offset = malloc(sizeof(*script->offset));
                list_init(&script->instrs);
                list_init(&script->raw_instrs);
                list_append_new(&entry->scripts, script);
                if (1 != sscanf(line, "Script: %d", &script->offset->id)) {
                    ERROR("Script parsing failed for %s", line);
                    exit(1);
                }
            } else if (util_strcmp_ref(line, stringref("Instruction")) == 0) {
                char* tmp = line + stringref("Instruction").len;
                char* before;
                char* after = NULL;

                tmp = strchr(tmp, ':');
                if (!tmp) {
                    ERROR("Instruction parsing failed for %s", line);
                    exit(1);
                }
                tmp++;

                instr = malloc(sizeof(*instr));

                instr->length = 0;
                instr->time = (int16_t)strtol(tmp, &tmp, 10);
                instr->param_mask = strtol(tmp, &tmp, 10);
                instr->type = strtol(tmp, &tmp, 10);

                before = tmp;

                for (;;) {
                    int32_t i;
                    float f;

                    i = strtol(before, &after, 10);
                    if (after == before) {
                        break;
                    } else {
                        instr->length += sizeof(int32_t);
                        instr = realloc(instr, sizeof(anm_instr_t) + instr->length);
                        if (*after == 'f' || *after == '.') {
                            f = strtof(before, &after);
                            memcpy(instr->data + instr->length - sizeof(float),
                                &f, sizeof(float));
                            /* Skip 'f'. */
                            ++after;
                        } else {
                            memcpy(instr->data + instr->length - sizeof(int32_t),
                                &i, sizeof(int32_t));
                        }
                    }

                    before = after;
                }

                list_append_new(&script->raw_instrs, instr);
            } else {
                sscanf(line, "Format: %u", &entry->header->format);
                sscanf(line, "Width: %u", &entry->header->w);
                sscanf(line, "Height: %u", &entry->header->h);
                sscanf(line, "X-Offset: %u", &entry->header->x);
                sscanf(line, "Y-Offset: %u", &entry->header->y);

                SCAN_DEPRECATED("Zero2", "%u", colorkey);
                if (sscanf(line, "ColorKey: %08x", &entry->header->colorkey) > 0
                    && entry->header->version >= 7)
                    ERROR("ColorKey is no longer supported in ANM versions >= 7");

                sscanf(line, "Zero3: %u", &entry->header->zero3);

                SCAN_DEPRECATED("Unknown1", "%u", memorypriority);
                if (sscanf(line, "MemoryPriority: %u", &entry->header->memorypriority)
                    && entry->header->version == 0)
                    ERROR("MemoryPriority is ignored in ANM version 0");

                SCAN_DEPRECATED("Unknown2", "%hu", lowresscale);
                if(sscanf(line, "LowResScale: %hu", &entry->header->lowresscale)
                    && entry->header->version < 8)
                    ERROR("LowResScale is ignored in ANM versions < 8");

                sscanf(line, "HasData: %hu", &entry->header->hasdata);
                sscanf(line, "THTX-Size: %u", &entry->thtx->size);
                sscanf(line, "THTX-Format: %hu", &entry->thtx->format);
                sscanf(line, "THTX-Width: %hu", &entry->thtx->w);
                sscanf(line, "THTX-Height: %hu", &entry->thtx->h);
                sscanf(line, "THTX-Zero: %hu", &entry->thtx->zero);
            }
        }
        linenum++;
    }

#undef SCAN_DEPRECATED
#undef ERROR

    fclose(f);

    return anm;
}

label_t*
label_find(
    anm_script_t* script,
    char* name
) {
    label_t* label;
    list_for_each(&script->labels, label) {
        if (strcmp(label->name, name) == 0)
            return label;
    }
    return NULL;
}

static symbol_id_pair_t*
symbol_find(
    list_t* list,
    char* name
) {
    symbol_id_pair_t* symbol;
    list_for_each(list, symbol) {
        if (strcmp(symbol->name, name) == 0)
            return symbol;
    }
    return NULL;
}

static anm_instr_t*
anm_serialize_instr(
    parser_state_t* state,
    thanm_instr_t* instr,
    anm_script_t* script
) {
    /* The size field of an instruction refers to size in its vesion.
     * However, here we are always allocating an anm_instr_t...
     * So we need to get size it would get as if it was anm_instr_t, no matter what. */
    uint32_t size = instr_get_size(instr, 8);

    anm_instr_t* raw = (anm_instr_t*)util_malloc(size);
    raw->type = instr->id;
    raw->length = size - sizeof(anm_instr_t); /* size of parametes. */
    raw->time = instr->time;
    raw->param_mask = 0;
    thanm_param_t* param;
    int i = 0;
    size_t offset = 0;
    list_for_each(&instr->params, param) {
        /* Format checking was done by the parser, no need to do it here again. */
        if (param->is_var)
            raw->param_mask |= 1 << i;

        switch(param->type) {
            case 'S':
                memcpy(&raw->data[offset], &param->val->val.S, sizeof(int32_t));
                offset += sizeof(int32_t);
                break;
            case 's':
                memcpy(&raw->data[offset], &param->val->val.s, sizeof(int16_t));
                offset += sizeof(int16_t);
                break;
            case 'f':
                memcpy(&raw->data[offset], &param->val->val.f, sizeof(float));
                offset += sizeof(float);
                break;
            case 'o': {
                label_t* label = label_find(script, param->val->val.z);
                if (label == NULL) {
                    fprintf(stderr, "%s: label not found: %s\n", argv0, param->val->val.z);
                    break;
                }
                memcpy(&raw->data[offset], &label->offset, sizeof(int32_t));
                offset += sizeof(int32_t);
                break;
            }
            case 't': {
                label_t* label = label_find(script, param->val->val.z);
                if (label == NULL) {
                    fprintf(stderr, "%s: label not found: %s\n", argv0, param->val->val.z);
                    break;
                }
                int32_t time = label->time;
                memcpy(&raw->data[offset], &time, sizeof(int32_t));
                offset += sizeof(int32_t);
                break;
            }
            case 'n': {
                symbol_id_pair_t* symbol = symbol_find(&state->sprite_names, param->val->val.z);
                if (symbol == NULL) {
                    fprintf(stderr, "%s: sprite not found: %s\n", argv0, param->val->val.z);
                    break;
                }
                memcpy(&raw->data[offset], &symbol->id, sizeof(int32_t));
                offset += sizeof(int32_t);
                break;
            }
            case 'N': {
                symbol_id_pair_t* symbol = symbol_find(&state->script_names, param->val->val.z);
                if (symbol == NULL) {
                    fprintf(stderr, "%s: script not found: %s\n", argv0, param->val->val.z);
                    break;
                }
                memcpy(&raw->data[offset], &symbol->id, sizeof(int32_t));
                offset += sizeof(int32_t);
                break;
            }
        }
        ++i;
    }
    return raw;
}

static void
anm_serialize_script(
    parser_state_t* state,
    anm_script_t* script
) {
    thanm_instr_t* instr;
    list_for_each(&script->instrs, instr) {
        anm_instr_t* raw_instr = anm_serialize_instr(state, instr, script);
        if (raw_instr != NULL)
            list_append_new(&script->raw_instrs, raw_instr);
        thanm_instr_free(instr);
    }
    list_free_nodes(&script->instrs);

    label_t* label;
    list_for_each(&script->labels, label) {
        free(label->name);
        free(label);
    }
    list_free_nodes(&script->labels);
}

static anm_archive_t*
anm_create(
    const char* spec,
    FILE* symbolfp
) {
    parser_state_t state;
    state.was_error = 0;
    state.time = 0;
    state.default_version = -1;
    state.current_version = -1;
    state.sprite_id = 0;
    state.script_id = 0;
    state.script_real_index = 0;
    list_init(&state.entries);
    list_init(&state.globals);
    list_init(&state.script_names);
    list_init(&state.sprite_names);
    state.current_entry = NULL;
    state.current_script = NULL;
    state.symbolfp = symbolfp;
    strcpy(state.symbol_prefix, "");

    path_init(&state.path_state, spec, argv0);

    FILE* in = fopen(spec, "r");
    yyin = in;
    if (yyparse(&state) || state.was_error)
        return NULL;

    anm_archive_t* anm = (anm_archive_t*)util_malloc(sizeof(anm_archive_t));
    anm->map = NULL;
    anm->map_size = 0;
    list_init(&anm->names);
    anm->entries = state.entries;

    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        char* name;
        int found = 0;
        list_for_each(&anm->names, name) {
            if (strcmp(entry->name, name) == 0) {
                free(entry->name);
                entry->name = name;
                found = 1;
                break;
            }
        }
        if (!found)
            list_append_new(&anm->names, entry->name);

        anm_script_t* script;
        list_for_each(&entry->scripts, script) {
            anm_serialize_script(&state, script);

            /* Free vars. */
            var_t* var;
            list_for_each(&script->vars, var) {
                var_free(var);
            }
        }
    }

    /* Free stuff. */
    reg_free_user();

    symbol_id_pair_t* symbol;
    list_for_each(&state.sprite_names, symbol) {
        free(symbol->name);
        free(symbol);
    }
    list_free_nodes(&state.sprite_names);

    list_for_each(&state.script_names, symbol) {
        free(symbol->name);
        free(symbol);
    }
    list_free_nodes(&state.script_names);

    global_t* global;
    list_for_each(&state.globals, global) {
        free(global->name);
        thanm_param_free(global->param);
    }
    list_free_nodes(&state.globals);

    return anm;
}

static void
anm_defaults(
    anm_archive_t* anm
) {
    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        if (!entry->header->hasdata)
            continue;

        image_t* img = png_read(entry->name);

        /* header->w/h must be a multiple of 2 */
        if (entry->header->w == DEFAULTVAL) {
            unsigned int n = 1;
            while(img->width > n) {
                n *= 2;
            }
            entry->header->w = n;
        }

        if (entry->header->h == DEFAULTVAL) {
            unsigned int n = 1;
            while (img->height > n) {
                n *= 2;
            }
            entry->header->h = n;
        }

        if (entry->header->hasdata == 1) {
            if (entry->thtx->format == DEFAULTVAL) {
                entry->thtx->format = entry->header->format;
            }

            if (entry->thtx->w == DEFAULTVAL)
                entry->thtx->w = img->width;

            if (entry->thtx->h == DEFAULTVAL)
                entry->thtx->h = img->height;

            if (entry->thtx->size == DEFAULTVAL)
                entry->thtx->size = entry->thtx->w * entry->thtx->h * format_Bpp(entry->thtx->format);
        }

        free(img->data);
        free(img);
    }
}

static void
anm_write(
    anm_archive_t* anm,
    const char* filename)
{
    FILE* stream;

    stream = fopen(filename, "wb");
    if (!stream) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
            argv0, filename, strerror(errno));
        exit(1);
    }

    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        sprite_t* sprite;
        anm_script_t* script;
        long base = file_tell(stream);
        unsigned int namepad = 0;
        char* padding;
        unsigned int j;
        unsigned int spriteoffset;

        namepad = (16 - strlen(entry->name) % 16);

        unsigned int sprite_count = 0;
        list_for_each(&entry->sprites, sprite)
            ++sprite_count;

        unsigned int script_count = 0;
        list_for_each(&entry->scripts, script)
            ++script_count;

        file_seek(stream, base +
                          sizeof(anm_header06_t) +
                          sprite_count * sizeof(uint32_t) +
                          script_count * sizeof(anm_offset_t));

        entry->header->nameoffset = file_tell(stream) - base;
        file_write(stream, entry->name, strlen(entry->name));

        padding = calloc(1, namepad);
        file_write(stream, padding, namepad);
        free(padding);

        if (entry->name2 && entry->header->version == 0) {
            namepad = (16 - strlen(entry->name2) % 16);

            entry->header->y = file_tell(stream) - base;
            file_write(stream, entry->name2, strlen(entry->name2));

            padding = calloc(1, namepad);
            file_write(stream, padding, namepad);
            free(padding);
        }

        spriteoffset = file_tell(stream) - base;

        list_for_each(&entry->sprites, sprite)
            file_write(stream, sprite, sizeof(*sprite));

        list_for_each(&entry->scripts, script) {
            script->offset->offset = file_tell(stream) - base;

            anm_instr_t* instr;
            list_for_each(&script->raw_instrs, instr) {
                if (entry->header->version == 0) {
                    anm_instr0_t new_instr;
                    new_instr.time = instr->time;
                    new_instr.type = instr->type;
                    new_instr.length = instr->length;
                    file_write(stream, &new_instr, sizeof(new_instr));
                    if (new_instr.length) {
                        file_write(stream,
                            instr->data,
                            new_instr.length);
                    }
                } else {
                    if (instr->type == 0xffff) {
                        instr->length = 0;
                        file_write(stream, instr, sizeof(*instr));
                    } else {
                        instr->length += sizeof(*instr);
                        file_write(stream, instr, instr->length);
                    }
                }
            }

            if (entry->header->version == 0) {
                anm_instr0_t sentinel = { 0, 0, 0 };
                file_write(stream, &sentinel, sizeof(sentinel));
            } else {
                anm_instr_t sentinel = { 0xffff, 0, 0, 0 };
                file_write(stream, &sentinel, sizeof(sentinel));
            }
        }

        if (entry->header->hasdata) {
            entry->header->thtxoffset = file_tell(stream) - base;

            file_write(stream, entry->thtx, sizeof(thtx_header_t));
            file_write(stream, entry->data, entry->thtx->size);
        }

        if (list_is_last_iteration())
            entry->header->nextoffset = 0;
        else
            entry->header->nextoffset = file_tell(stream) - base;

        entry->header->sprites = sprite_count;
        entry->header->scripts = script_count;

        file_seek(stream, base);

        if (entry->header->version >= 7) {
            convert_header_to_11(entry->header);
            file_write(stream, entry->header, sizeof(anm_header06_t));
            convert_header_to_old(entry->header);
        } else {
            file_write(stream, entry->header, sizeof(anm_header06_t));
        }

        for (j = 0; j < sprite_count; ++j) {
            uint32_t ofs = spriteoffset + j * sizeof(sprite_t);
            file_write(stream, &ofs, sizeof(uint32_t));
        }

        list_for_each(&entry->scripts, script) {
            file_write(stream, script->offset, sizeof(*script->offset));
        }

        file_seek(stream, base + entry->header->nextoffset);
    }

    fclose(stream);
}
#endif

static void
anm_free(
    anm_archive_t* anm)
{
    int is_mapped = anm->map != NULL;

    char* name;
    list_for_each(&anm->names, name)
        free(name);
    list_free_nodes(&anm->names);

    anm_entry_t* entry;
    list_for_each(&anm->entries, entry) {
        list_free_nodes(&entry->sprites);

        anm_script_t* script;
        list_for_each(&entry->scripts, script) {
            if (!is_mapped)
                free(script->offset);

            thanm_instr_t* instr;
            list_for_each(&script->instrs, instr) {
                thanm_instr_free(instr);
            }
            if (!is_mapped || entry->header->version == 0) {
                anm_instr_t* instr;
                list_for_each(&script->raw_instrs, instr) {
                    free(instr);
                }
            }
            list_free_nodes(&script->instrs);
            list_free_nodes(&script->raw_instrs);

            free(script);
        }
        list_free_nodes(&entry->scripts);

        if (!is_mapped) {
            free(entry->header);
            free(entry->thtx);
            free(entry->name2);
            free(entry->data);

            sprite_t* sprite;
            list_for_each(&entry->sprites, sprite)
                free(sprite);
        } else if (entry->header->version >= 7) {
            free(entry->header);
        }

        free(entry);
    }
    list_free_nodes(&anm->entries);

    if (is_mapped)
        file_munmap(anm->map, anm->map_size);

    free(anm);
}

static void
print_usage(void)
{
#ifdef HAVE_LIBPNG
#define USAGE_LIBPNGFLAGS " | -x | -r | -c"
#else
#define USAGE_LIBPNGFLAGS ""
#endif
    printf("Usage: %s [-Vf] [-l" USAGE_LIBPNGFLAGS "] [-o] [-m ANMMAP] [-s SYMBOLS] ARCHIVE ...\n"
           "Options:\n"
           "  -l ARCHIVE            list archive\n", argv0);
#ifdef HAVE_LIBPNG
    printf("  -x ARCHIVE [FILE...]  extract entries\n"
           "  -r ARCHIVE NAME FILE  replace entry in archive\n"
           "  -c ARCHIVE SPEC       create archive\n"
           "  -s                    save symbol ids to the given file as globaldefs\n");
#endif
    printf("  -o                    use old spec format\n"
           "  -m                    use map file for translating mnemonics\n"
           "  -V                    display version information and exit\n"
           "  -f                    ignore errors when possible\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n");
}

static void
free_globals(void) {
    anmmap_free(g_anmmap);
}

int
main(
    int argc,
    char* argv[])
{
    g_anmmap = anmmap_new();
    atexit(free_globals);

    const char commands[] = ":lom:"
#ifdef HAVE_LIBPNG
                            "xrcs:"
#endif
                            "Vf";
    int command = -1;

    FILE* in;

    anm_archive_t* anm;
#ifdef HAVE_LIBPNG
    anm_entry_t* entry;
    char* name;

    FILE* anmfp;
    FILE* symbolfp = NULL;
    int i;
#endif

    argv0 = util_shortname(argv[0]);
    int opt;
    int ind = 0;
    int option_old = 0;
    while(argv[util_optind]) {
        switch(opt = util_getopt(argc,argv,commands)) {
        case 'l':
        case 'x':
        case 'r':
        case 'c':
            if(command != -1) {
                fprintf(stderr,"%s: More than one mode specified\n",argv0);
                print_usage();
                exit(1);
            }
            command = opt;
            break;
        case 'o':
            option_old = 1;
            break;
        case 'm': {
            FILE* map_file = NULL;
            map_file = fopen(util_optarg, "r");
            if (!map_file) {
                fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                    argv0, util_optarg, strerror(errno));
            } else {
                anmmap_load(g_anmmap, map_file, util_optarg);
                fclose(map_file);
            }
            break;
        }
        case 's':
            symbolfp = fopen(util_optarg, "w");
            if (!symbolfp) {
                fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                    argv0, util_optarg, strerror(errno));
            }
            break;
        case 'f':
            option_force = 1;
            break;
        default:
            util_getopt_default(&ind,argv,opt,print_usage);
        }
    }
    argc = ind;
    argv[argc] = NULL;

    switch (command) {
    case 'l':
        if (argc != 1) {
            print_usage();
            exit(1);
        }

        current_input = argv[0];
        in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[0], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);
        if (option_old)
            anm_dump_old(stdout, anm);
        else
            anm_dump(stdout, anm);

        anm_free(anm);
        exit(0);
#ifdef HAVE_LIBPNG
    case 'x':
        if (argc < 1) {
            print_usage();
            exit(1);
        }

        current_input = argv[0];
        in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[0], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);

        if (argc == 1) {
            /* Extract all files. */
            list_for_each(&anm->names, name) {
                current_output = name;
                puts(name);
                anm_extract(anm, name);
            }
        } else {
            /* Extract all listed files. */
            for (i = 1; i < argc; ++i) {
                list_for_each(&anm->names, name) {
                    if (strcmp(argv[i], name) == 0) {
                        current_output = name;
                        puts(name);
                        anm_extract(anm, name);
                        goto extract_next;
                    }
                }
                fprintf(stderr, "%s:%s: %s not found in archive\n",
                    argv0, current_input, argv[i]);
extract_next:
                ;
            }
        }

        anm_free(anm);
        exit(0);
    case 'r':
        if (argc != 3) {
            print_usage();
            exit(1);
        }

        current_output = argv[2];
        current_input = argv[0];
        in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[0], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);

        anmfp = fopen(argv[0], "rb+");
        if (!anmfp) {
            fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                argv0, current_input, strerror(errno));
            exit(1);
        }

        list_for_each(&anm->names, name) {
            if (strcmp(argv[1], name) == 0) {
                anm_replace(anm, anmfp, name, argv[2]);
                goto replace_done;
            }
        }

        fprintf(stderr, "%s:%s: %s not found in archive\n",
            argv0, current_input, argv[1]);

replace_done:

        fclose(anmfp);

#if 0
        offset = 0;
        list_for_each(&anm->entries, entry) {
            unsigned int nextoffset = entry->header->nextoffset;
            if (strcmp(argv[1], entry->name) == 0 && entry->header->hasdata) {
                if (!file_seek(anmfp,
                    offset + entry->header->thtxoffset + 4 + sizeof(thtx_header_t)))
                    exit(1);
                if (!file_write(anmfp, entry->data, entry->thtx->size))
                    exit(1);
            }
            offset += nextoffset;
        }
#endif

        anm_free(anm);
        exit(0);
    case 'c':
        if (argc != 2) {
            print_usage();
            exit(1);
        }
        current_input = argv[1];
        if (option_old)
            anm = anm_create_old(argv[1]);
        else
            anm = anm_create(argv[1], symbolfp);

        if (symbolfp)
            fclose(symbolfp);

        if (anm == NULL)
            exit(0);

        anm_defaults(anm);

        /* Allocate enough space for the THTX data. */
        list_for_each(&anm->entries, entry) {
            if (entry->header->hasdata) {
                /* XXX: There are a few entries with a thtx.size greater than
                 *      w*h*Bpp.  The extra data appears to be all zeroes. */
                entry->data = calloc(1, entry->thtx->size);
            }
        }

        list_for_each(&anm->names, name)
            anm_replace(anm, NULL, name, name);

        current_output = argv[0];
        anm_write(anm, argv[0]);

        anm_free(anm);
        exit(0);
#endif
    default:
        print_usage();
        exit(1);
    }
}
