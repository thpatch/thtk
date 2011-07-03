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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "file.h"
#include "image.h"
#include "thanm.h"
#include "program.h"
#include "util.h"
#include "value.h"

unsigned int option_force;

static const id_format_pair_t formats_v0[] = {
    { 0, "" },
    { 1, "S" },
    { 2, "ff" },
    { 3, "S" },
    { 4, "S" },
    { 5, "S" },
    { 7, "" },
    { 9, "fff" },
    { 10, "fSf" },
    { 11, "ff" },
    { 12, "SS" },
    { 13, "" },
    { 14, "" },
    { 15, "" },
    { 16, "SS" },
    { 17, "fff" },
    { 18, "ffSS" },
    { 19, "ffSS" },
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
    { 3, "S" },
    { 4, "SS" },
    { 5, "SSS" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "S" },
    { 9, "S" },
    { 10, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "SS" },
    { 16, "S" },
    { 17, "ffSS" },
    { 18, "ffSS" },
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
    { 32, "SSffS" },
    { 33, "SSS" },
    { 34, "SSS" },
    { 35, "SSSSf" },
    { 36, "SSff" },
    { 37, "SS" },
    { 38, "ff" },
    { 42, "ff" },
    { 50, "fff" },
    { 52, "fff" },
    { 55, "SSS" },
    { 59, "SS" },
    { 60, "ff" },
    { 69, "SSSS" },
    { 79, "S" },
    { 80, "S" },
    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v3[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "S" },
    { 4, "SS" },
    { 5, "SSS" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "S" },
    { 9, "SSS" },
    { 10, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "SS" },
    { 16, "S" },
    { 17, "ffSS" },
    { 18, "ffSS" },
    { 20, "" },
    { 21, "S" },
    { 22, "" },
    { 23, "" },
    { 24, "S" },
    { 25, "S" },
    { 26, "f" },
    { 27, "f" },
    { 28, "S" },
    { 30, "S" },
    { 31, "S" },
    { 32, "SSfff" },
    { 33, "SSSSS" },
    { 34, "SSS" },
    { 35, "SSSSf" },
    { 36, "SSff" },
    { 37, "SS" },
    { 38, "ff" },
    { 40, "ff" },
    { 42, "ff" },
    { 44, "ff" },
    { 49, "SSS" },
    { 50, "fff" },
    { 52, "fff" },
    { 54, "fff" },
    { 55, "SSS" },
    { 56, "fff" },
    { 59, "SS" },
    { 60, "ff" },
    { 69, "SSSS" },
    { 79, "S" },
    { 80, "f" },
    { 81, "f" },
    { 82, "S" },
    { 83, "S" },
    { 85, "S" },
    { 86, "SSSSS" },
    { 87, "SSS" },
    { 89, "" },
    { 0xffff, "" },
    { 0, NULL }
};

static const id_format_pair_t formats_v4p[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "S" },
    { 4, "SS" },
    { 5, "SSS" },
    { 6, "SS" },
    { 7, "ff" },
    { 8, "SS" },
    { 9, "ff" },
    { 11, "ff" },
    { 13, "ff" },
    { 18, "SSS" },
    { 19, "fff" },
    { 21, "fff" },
    { 22, "SSS" },
    { 23, "fff" },
    { 24, "SSS" },
    { 25, "fff" },
    { 26, "SSS" },
    { 27, "fff" },
    { 30, "SSSS" },
    { 40, "SS" },
    { 42, "ff" },
    { 43, "ff" },
    { 48, "fff" },
    { 49, "fff" },
    { 50, "ff" },
    { 51, "S" },
    { 52, "SSS" },
    { 53, "fff" },
    { 56, "SSfff" },
    { 57, "SSSSS" },
    { 58, "SSS" },
    { 59, "SSfSf" },
    { 60, "SSff" },
    { 61, "" },
    { 63, "" },
    { 64, "S" },
    { 65, "S" },
    { 66, "S" },
    { 67, "S" },
    { 68, "S" },
    { 69, "" },
    { 70, "f" },
    { 71, "f" },
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
    { 88, "S" },
    { 89, "S" },
    { 90, "S" },
    { 91, "S" },
    { 92, "S" },
    { 93, "SSf" },
    { 94, "SSf" },
    { 95, "S" },
    { 96, "Sff" },
    { 100, "SfffffSffS" },
    { 101, "S" },
    { 102, "SS" },
    { 103, "ff" },
    { 104, "fS" },
    { 105, "fS" },
    { 106, "fS" },
    { 107, "SSff" },
    { 108, "ff" },
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
    { 107, "ff" },
    { 112, "SSS" },
    { 113, "fff" },
    { 115, "fff" },
    { 117, "fff" },
    { 118, "SSS" },
    { 119, "fff" },
    { 120, "SSS" },
    { 121, "fff" },
    { 122, "SS" },
    { 124, "ff" },
    { 125, "ff" },
    { 131, "ffff" },
    { 200, "SS" },
    { 201, "SSS" },
    { 204, "SSSS" },
    { 300, "S" },
    { 301, "SS" },
    { 302, "S" },
    { 303, "S" },
    { 304, "S" },
    { 305, "S" },
    { 306, "S" },
    { 307, "S" },
    { 308, "" },
    { 311, "S" },
    { 312, "SS" },
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
    { 412, "SSff" },
    { 413, "SSSSS" },
    { 414, "SSS" },
    { 415, "fff" },
    { 420, "SffSSSSffS" },
    { 421, "S" }, /* ss */
    { 424, "S" },
    { 425, "f" },
    { 426, "f" },
    { 429, "Sf" },
    { 430, "SSff" },
    { 431, "S" },
    { 433, "SSff" },
    { 500, "S" },
    { 501, "S" },
    { 502, "S" },
    { 503, "S" },
    { 504, "S" },
    { 505, "Sff" },
    { 508, "S" },
    { 600, "S" },
    { 602, "S" },
    { 603, "ff" },
    { 604, "fS" },
    { 605, "fS" },
    { 606, "ff" },
    { 608, "ff" },
    { 0xffff, "" },
    { 0, NULL }
};

/* The order and sizes of fields changed for TH11. */
static void
convert_header_to_old(
    anm_header_t* header)
{
    anm_header11_t th11 = *(anm_header11_t*)header;
    header->sprites = th11.sprites;
    header->scripts = th11.scripts;
    header->zero1 = th11.zero1;
    header->w = th11.w;
    header->h = th11.h;
    header->format = th11.format;
    header->zero2 = 0;
    header->nameoffset = th11.nameoffset;
    header->x = th11.x;
    header->y = th11.y;
    header->version = th11.version;
    header->unknown1 = th11.unknown1;
    header->thtxoffset = th11.thtxoffset;
    header->hasdata = th11.hasdata;
    header->nextoffset = th11.nextoffset;
    header->zero3 = 0;
}

#ifdef HAVE_LIBPNG
static void
convert_header_to_11(
    anm_header_t* oldheader)
{
    anm_header_t header = *oldheader;
    anm_header11_t* th11 = (anm_header11_t*)oldheader;
    memset(th11, 0, sizeof(anm_header11_t));
    th11->sprites = header.sprites;
    th11->scripts = header.scripts;
    th11->zero1 = header.zero1;
    th11->w = header.w;
    th11->h = header.h;
    th11->format = header.format;
    th11->nameoffset = header.nameoffset;
    th11->x = header.x;
    th11->y = header.y;
    th11->version = header.version;
    th11->unknown1 = header.unknown1;
    th11->thtxoffset = header.thtxoffset;
    th11->hasdata = header.hasdata;
    th11->nextoffset = header.nextoffset;
}
#endif

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

    for (;;) {
        anm_entry_t* entry = malloc(sizeof(*entry));
        anm_header_t* header = (anm_header_t*)map;

        list_append_new(&archive->entries, entry);

        if (header->zero1 != 0) {
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

        assert(
            header->unknown1 == 0 ||
            header->unknown1 == 1 ||
            header->unknown1 == 10 ||
            header->unknown1 == 11);

        assert((header->hasdata == 0) == (header->thtxoffset == 0));

        assert(header->hasdata == 0 || header->hasdata == 1);
        assert(header->zero1 == 0);
        assert(header->zero3 == 0);

        /* Lengths, including padding, observed are: 16, 32, 48. */
        entry->name = anm_get_name(archive, (const char*)map + header->nameoffset);
        if (header->version == 0 && header->y != 0)
            entry->name2 = (char*)map + header->y;
        else
            entry->name2 = NULL;

        list_init(&entry->sprites);
        if (header->sprites) {
            uint32_t* sprite_offsets = (uint32_t*)(map + sizeof(*header));
            for (uint32_t s = 0; s < header->sprites; ++s) {
                list_append_new(&entry->sprites, (sprite_t*)(map + sprite_offsets[s]));
            }
        }

        list_init(&entry->scripts);
        if (header->scripts) {
            anm_offset_t* script_offsets = 
                (anm_offset_t*)(map + sizeof(*header) + header->sprites * sizeof(uint32_t));
            for (uint32_t s = 0; s < header->scripts; ++s) {
                anm_script_t* script = malloc(sizeof(*script));
                script->offset = &(script_offsets[s]);
                list_init(&script->instrs);

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
                for (;;) {
                    anm_instr_t* instr;
                    if (header->version == 0) {
                        anm_instr0_t* temp_instr = (anm_instr0_t*)instr_ptr;

                        if (instr_ptr + sizeof(anm_instr0_t) > limit ||
                            (temp_instr->type == 0 && temp_instr->time == 0) ||
                            instr_ptr + sizeof(anm_instr0_t) + temp_instr->length > limit)
                            break;

                        instr = malloc(sizeof(anm_instr_t) + temp_instr->length);
                        instr->type = temp_instr->type;
                        instr->length = sizeof(anm_instr_t) + temp_instr->length;
                        instr->time = temp_instr->time;
                        instr->param_mask = 0;
                        memcpy(instr->data, temp_instr->data, temp_instr->length);

                        list_append_new(&script->instrs, instr);

                        instr_ptr += sizeof(anm_instr0_t) + temp_instr->length;
                    } else {
                        anm_instr_t* temp_instr = (anm_instr_t*)instr_ptr;

                        if (instr_ptr + sizeof(anm_instr_t) > limit ||
                            temp_instr->type == 0xffff ||
                            instr_ptr + temp_instr->length > limit)
                            break;

                        list_append_new(&script->instrs, temp_instr);

                        instr_ptr += temp_instr->length;
                    }
                }

                list_append_new(&entry->scripts, script);
            }
        }

        if (header->hasdata) {
            thtx_header_t* thtx = entry->thtx =
                (thtx_header_t*)(map + header->thtxoffset);
            assert(strncmp(thtx->magic, "THTX", 4) == 0);
            assert(thtx->zero == 0);
            assert(thtx->w * thtx->h * format_Bpp(thtx->format) <= thtx->size);
            assert(
                thtx->format == FORMAT_BGRA8888 ||
                thtx->format == FORMAT_BGR565 ||
                thtx->format == FORMAT_BGRA4444 ||
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
anm_dump(
    FILE* stream,
    const anm_archive_t* anm)
{
    anm_entry_t* entry;

    list_for_each(&anm->entries, entry) {
        const id_format_pair_t* formats = NULL;

        if (entry->header->version == 0) {
            formats = formats_v0;
        } else if (entry->header->version == 2) {
            formats = formats_v2;
        } else if (entry->header->version == 3) {
            formats = formats_v3;
        } else if (entry->header->version == 4 || entry->header->version == 7) {
            formats = formats_v4p;
        } else if (entry->header->version == 8) {
            formats = formats_v8;
        } else {
            fprintf(stderr,
                "%s:%s: could not find a format description for version %u\n",
                argv0, current_input, entry->header->version);
            abort();
        }

        fprintf(stream, "ENTRY %u\n", entry->header->version);
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
        if (entry->header->zero1 != 0)
            fprintf(stream, "Zero1: %u\n", entry->header->zero1);
        if (entry->header->zero2 != 0)
            fprintf(stream, "Zero2: %u\n", entry->header->zero2);
        if (entry->header->zero3 != 0)
            fprintf(stream, "Zero3: %u\n", entry->header->zero3);
        if (entry->header->unknown1 != 0)
            fprintf(stream, "Unknown1: %u\n", entry->header->unknown1);
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

            anm_instr_t* instr;
            list_for_each(&script->instrs, instr) {
                const char* format = find_format(formats, instr->type);

                if (!format) {
                    fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, instr->type);
                    abort();
                }

                fprintf(stream, "Instruction: %hu %hu %hu",
                    instr->time, instr->param_mask, instr->type);

                if (instr->length > sizeof(anm_instr_t)) {
                    value_t* values;

                    values = value_list_from_data(value_from_data, (unsigned char*)instr->data, instr->length - sizeof(anm_instr_t), format);
                    if (!values)
                        abort();

                    for (size_t i = 0; values[i].type; ++i) {
                        char* disp;
                        disp = value_to_text(&values[i]);
                        fprintf(stream, " %s", disp);
                        value_free(&values[i]);
                        free(disp);
                    }

                    free(values);
                }

                fprintf(stream, "\n");
            }

            fprintf(stream, "\n");
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
        FORMAT_BGR565,
        FORMAT_BGRA4444,
        FORMAT_GRAY8
    };
    unsigned int f;
    unsigned int width = 0;
    unsigned int height = 0;
    FILE* stream;
    image_t* image;

    util_total_entry_size(anm, name, &width, &height);
    if (width == 0 || height == 0) {
        /* There's nothing to do. */
        return;
    }

    stream = fopen(filename, "rb");
    if (!stream) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
            argv0, filename, strerror(errno));
        exit(1);
    }
    image = png_read(stream, FORMAT_RGBA8888);
    fclose(stream);

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
        FORMAT_BGRA4444,
        FORMAT_BGR565,
        FORMAT_BGRA8888,
        FORMAT_RGBA8888
    };
    FILE* stream;
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
    stream = fopen(name, "wb");
    if (!stream) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
            argv0, name, strerror(errno));
        return;
    }
    png_write(stream, &image);
    fclose(stream);

    free(image.data);
}

static anm_archive_t*
anm_create(
    const char* spec)
{
    FILE* f;
    char line[4096];
    anm_archive_t* anm;
    anm_entry_t* entry = NULL;
    anm_script_t* script = NULL;
    anm_instr_t* instr = NULL;

    f = fopen(spec, "r");
    if (!f) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
            argv0, spec, strerror(errno));
        exit(1);
    }

    anm = malloc(sizeof(anm_archive_t));
    anm->map = NULL;
    anm->map_size = 0;
    list_init(&anm->names);
    list_init(&anm->entries);

    while (fgets(line, 4096, f)) {
        char* linep = line;

        if (strncmp(linep, "ENTRY ", 6) == 0) {
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
            sscanf(linep, "ENTRY %u", &entry->header->version);
        } else if (strncmp(linep, "Name: ", 6) == 0) {
            char name[256];
            sscanf(linep, "Name: %s", name);
            entry->name = anm_get_name(anm, name);
        } else if (strncmp(linep, "Name2: ", 6) == 0) {
            char name[256];
            sscanf(linep, "Name2: %s", name);
            entry->name2 = strdup(name);
        } else if (strncmp(linep, "Sprite: ", 8) == 0) {
            sprite_t* sprite = malloc(sizeof(*sprite));
            list_append_new(&entry->sprites, sprite);

            if (5 != sscanf(linep, "Sprite: %u %f*%f+%f+%f",
                         &sprite->id, &sprite->w, &sprite->h,
                         &sprite->x, &sprite->y)) {
                fprintf(stderr, "%s: Sprite parsing failed for %s\n",
                    argv0, linep);
                exit(1);
            }
        } else if (strncmp(linep, "Script: ", 8) == 0) {
            script = malloc(sizeof(*script));
            script->offset = malloc(sizeof(*script->offset));
            list_init(&script->instrs);
            list_append_new(&entry->scripts, script);
            if (1 != sscanf(linep, "Script: %d", &script->offset->id)) {
                fprintf(stderr, "%s: Script parsing failed for %s\n",
                    argv0, linep);
                exit(1);
            }
        } else if (strncmp(linep, "Instruction: ", 13) == 0) {
            char* tmp = linep + 13;
            char* before;
            char* after = NULL;

            instr = malloc(sizeof(*instr));

            instr->length = 0;
            instr->time = strtol(tmp, &tmp, 10);
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

            list_append_new(&script->instrs, instr);
        } else {
            sscanf(linep, "Format: %u", &entry->header->format);
            sscanf(linep, "Width: %u", &entry->header->w);
            sscanf(linep, "Height: %u", &entry->header->h);
            sscanf(linep, "X-Offset: %u", &entry->header->x);
            sscanf(linep, "Y-Offset: %u", &entry->header->y);
            sscanf(linep, "Zero1: %u", &entry->header->zero1);
            sscanf(linep, "Zero2: %u", &entry->header->zero2);
            sscanf(linep, "Zero3: %u", &entry->header->zero3);
            sscanf(linep, "Unknown1: %u", &entry->header->unknown1);
            sscanf(linep, "HasData: %u", &entry->header->hasdata);
            sscanf(linep, "THTX-Size: %u", &entry->thtx->size);
            sscanf(linep, "THTX-Format: %hu", &entry->thtx->format);
            sscanf(linep, "THTX-Width: %hu", &entry->thtx->w);
            sscanf(linep, "THTX-Height: %hu", &entry->thtx->h);
            sscanf(linep, "THTX-Zero: %hu", &entry->thtx->zero);
        }
    }

    fclose(f);

    return anm;
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
                          sizeof(anm_header_t) +
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
            list_for_each(&script->instrs, instr) {
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
            file_write(stream, entry->header, sizeof(anm_header_t));
            convert_header_to_old(entry->header);
        } else {
            file_write(stream, entry->header, sizeof(anm_header_t));
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

            if (!is_mapped || entry->header->version == 0) {
                anm_instr_t* instr;
                list_for_each(&script->instrs, instr) {
                    free(instr);
                }
            }
            list_free_nodes(&script->instrs);

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
    printf("Usage: %s COMMAND\n"
           "COMMAND can be:\n"
           "  l[OPTION...] ARCHIVE            list archive\n", argv0);
#ifdef HAVE_LIBPNG
    printf("  x[OPTION...] ARCHIVE [FILE...]  extract entries\n"
           "  r[OPTION...] ARCHIVE NAME FILE  replace entry in archive\n"
           "  c[OPTION...] ARCHIVE SPEC       create archive\n");
#endif
    printf("  V                               display version information and exit\n"
           "OPTION can be:\n"
           "  f  ignore errors when possible\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n");
}

int
main(
    int argc,
    char* argv[])
{
    const char commands[] = "l"
#ifdef HAVE_LIBPNG
                            "xrc"
#endif
                            "V";
    char options[] = "f";
    int command = 0;

    FILE* in;

    anm_archive_t* anm;
#ifdef HAVE_LIBPNG
    anm_entry_t* entry;
    char* name;

    FILE* anmfp;
    int i;
#endif

    command = parse_args(argc, argv, print_usage, commands, options, NULL);

    if (!command)
        exit(1);

    if (!strchr(options, 'f'))
        option_force = 1;

    switch (command) {
    case 'V':
        util_print_version();
        exit(0);
    case 'l':
        if (argc != 3) {
            print_usage();
            exit(1);
        }

        current_input = argv[2];
        in = fopen(argv[2], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[2], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);
        anm_dump(stdout, anm);
        anm_free(anm);
        exit(0);
#ifdef HAVE_LIBPNG
    case 'x':
        if (argc < 3) {
            print_usage();
            exit(1);
        }

        current_input = argv[2];
        in = fopen(argv[2], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[2], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);

        if (argc == 3) {
            /* Extract all files. */
            list_for_each(&anm->names, name) {
                current_output = name;
                puts(name);
                anm_extract(anm, name);
            }
        } else {
            /* Extract all listed files. */
            for (i = 3; i < argc; ++i) {
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
        if (argc != 5) {
            print_usage();
            exit(1);
        }

        current_output = argv[4];
        current_input = argv[2];
        in = fopen(argv[2], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[2], current_input);
            exit(1);
        }
        anm = anm_read_file(in);
        fclose(in);

        anmfp = fopen(argv[2], "rb+");
        if (!anmfp) {
            fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                argv0, current_input, strerror(errno));
            exit(1);
        }

        list_for_each(&anm->names, name) {
            if (strcmp(argv[3], name) == 0) {
                anm_replace(anm, anmfp, name, argv[4]);
                goto replace_done;
            }
        }

        fprintf(stderr, "%s:%s: %s not found in archive\n",
            argv0, current_input, argv[3]);

replace_done:

        fclose(anmfp);

#if 0
        offset = 0;
        list_for_each(&anm->entries, entry) {
            unsigned int nextoffset = entry->header->nextoffset;
            if (strcmp(argv[3], entry->name) == 0 && entry->header->hasdata) {
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
        if (argc != 4) {
            print_usage();
            exit(1);
        }
        current_input = argv[3];
        anm = anm_create(argv[3]);

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

        current_output = argv[2];
        anm_write(anm, argv[2]);

        anm_free(anm);
        exit(0);
#endif
    default:
        abort();
    }
}
