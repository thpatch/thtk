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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#ifdef HAVE_LIBPNG
#include <png.h>
#endif
#include "thanm.h"
#include "program.h"
#include "util.h"

static unsigned int option_force;

static unsigned int
format_Bpp(format_t format)
{
    switch (format) {
    case FORMAT_RGBA8888:
    case FORMAT_BGRA8888:
        return 4;
    case FORMAT_BGRA4444:
    case FORMAT_BGR565:
        return 2;
    case FORMAT_GRAY8:
        return 1;
    default:
        fprintf(stderr, "%s: unknown format: %u\n", argv0, format);
        if (!option_force) abort();
        return 1;
    }
}

static unsigned char*
rgba_to_fmt(const uint32_t* data, unsigned int pixels, format_t format)
{
    unsigned int i;
    unsigned char* out = NULL;

    if (format == FORMAT_GRAY8) {
        out = malloc(pixels);
        for (i = 0; i < pixels; ++i) {
            out[i] = data[i] & 0xff;
        }
    } else if (format == FORMAT_BGRA8888) {
        const unsigned char* data8 = (const unsigned char*)data;
        out = malloc(sizeof(uint32_t) * pixels);
        memcpy(out, data, pixels * sizeof(uint32_t));
        for (i = 0; i < pixels; ++i) {
            out[i * sizeof(uint32_t) + 0] = data8[i * sizeof(uint32_t) + 2];
            out[i * sizeof(uint32_t) + 2] = data8[i * sizeof(uint32_t) + 0];
        }
    } else if (format == FORMAT_BGRA4444) {
        out = malloc(sizeof(uint16_t) * pixels);
        for (i = 0; i < pixels; ++i) {
            const unsigned char r = (((data[i] & 0xff000000) >> 24) + 8) / 17;
            const unsigned char g = (((data[i] &   0xff0000) >> 16) + 8) / 17;
            const unsigned char b = (((data[i] &     0xff00) >>  8) + 8) / 17;
            const unsigned char a = (((data[i] &       0xff)      ) + 8) / 17;

            out[i * sizeof(uint16_t) + 0] = (b << 4) | g;
            out[i * sizeof(uint16_t) + 1] = (r << 4) | a;
        }
    } else if (format == FORMAT_BGR565) {
        uint16_t* out16;
        out = malloc(sizeof(uint16_t) * pixels);
        out16 = (uint16_t*)out;
        for (i = 0; i < pixels; ++i) {
            out16[i] = ((data[i] &     0xf8) << 8)  /* 00000000 00000000 11111000 -> 00000000 00011111 */
                     | ((data[i] &   0xfc00) >> 5)  /* 00000000 11111100 00000000 -> 00000111 11100000 */
                     | ((data[i] & 0xf80000) >>19); /* 11111000 00000000 00000000 -> 11111000 00000000 */
        }
    } else {
        fprintf(stderr, "%s: unknown format: %u\n", argv0, format);
        abort();
    }

    return out;
}

static char*
fmt_to_rgba(const char* data, unsigned int pixels, format_t format)
{
    unsigned int i;
    uint32_t* out = malloc(sizeof(uint32_t) * pixels);

    if (format == FORMAT_GRAY8) {
        for (i = 0; i < pixels; ++i) {
            out[i] = 0xff000000
                   | (data[i] << 16 & 0xff0000)
                   | (data[i] <<  8 &   0xff00)
                   | (data[i] <<  0 &     0xff);
        }
    } else if (format == FORMAT_BGRA8888) {
        char* out8 = (char*)out;
        memcpy(out, data, pixels * sizeof(uint32_t));
        for (i = 0; i < pixels; ++i) {
            out8[i * sizeof(uint32_t) + 0] = data[i * sizeof(uint32_t) + 2];
            out8[i * sizeof(uint32_t) + 2] = data[i * sizeof(uint32_t) + 0];
        }
    } else if (format == FORMAT_BGRA4444) {
        for (i = 0; i < pixels; ++i) {
            /* Extends like this: 0x0 -> 0x00, 0x3 -> 0x33, 0xf -> 0xff.
             * It's required for proper alpha. */
            out[i] = ((data[i * sizeof(uint16_t) + 1] & 0xf0) << 24 & 0xf0000000)
                   | ((data[i * sizeof(uint16_t) + 1] & 0xf0) << 20 & 0x0f000000)
                   | ((data[i * sizeof(uint16_t) + 0] & 0x0f) << 20 &   0xf00000)
                   | ((data[i * sizeof(uint16_t) + 0] & 0x0f) << 16 &   0x0f0000)
                   | ((data[i * sizeof(uint16_t) + 0] & 0xf0) <<  8 &     0xf000)
                   | ((data[i * sizeof(uint16_t) + 0] & 0xf0) <<  4 &     0x0f00)
                   | ((data[i * sizeof(uint16_t) + 1] & 0x0f) <<  4 &       0xf0)
                   | ((data[i * sizeof(uint16_t) + 1] & 0x0f) <<  0 &       0x0f);
        }
    } else if (format == FORMAT_BGR565) {
        uint16_t* u16 = (uint16_t*)data;
        for (i = 0; i < pixels; ++i) {
            /* Bit-extends channels: 00001b -> 00001111b. */
            out[i] = 0xff000000
                   | ((u16[i] & 0x001f) << 19 & 0xf80000)
                   | ((u16[i] & 0x0001) << 16 & 0x040000)
                   | ((u16[i] & 0x0001) << 16 & 0x020000)
                   | ((u16[i] & 0x0001) << 16 & 0x010000)

                   | ((u16[i] & 0x07e0) <<  5 & 0x00fc00)
                   | ((u16[i] & 0x0020) <<  4 & 0x000200)
                   | ((u16[i] & 0x0020) <<  3 & 0x000100)

                   | ((u16[i] & 0xf800) >>  8 & 0x0000f8)
                   | ((u16[i] & 0x0800) >>  9 & 0x000004)
                   | ((u16[i] & 0x0800) >> 10 & 0x000002)
                   | ((u16[i] & 0x0800) >> 11 & 0x000001);
        }
    } else {
        fprintf(stderr, "%s: unknown format: %u\n", argv0, format);
        abort();
    }

    return (char*)out;
}

static const opcode_fmt_t formats_v0[] = {
    { 0, "" },
    { 1, "i" },
    { 2, "ff" },
    { 3, "i" },
    { 4, "i" },
    { 5, "i" },
    { 7, "" },
    { 9, "fff" },
    { 10, "fif" },
    { 11, "ff" },
    { 12, "ii" },
    { 13, "" },
    { 14, "" },
    { 15, "" },
    { 16, "ii" },
    { 17, "fff" },
    { 18, "ffii" },
    { 19, "ffii" },
    { 20, "fffi" },
    { 21, "" },
    { 22, "i" },
    { 23, "" },
    { 24, "" },
    { 25, "i" },
    { 26, "i" },
    { 27, "f" },
    { 28, "f" },
    { 29, "i" },
    { 30, "ffi" },
    { 31, "i" },
};

static const opcode_fmt_t formats_v2[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "i" },
    { 4, "ii" },
    { 5, "iii" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "i" },
    { 9, "i" },
    { 10, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "ii" },
    { 16, "i" },
    { 17, "ffii" },
    { 18, "ffii" },
    { 19, "fffi" },
    { 20, "" },
    { 21, "i" },
    { 22, "" },
    { 23, "" },
    { 24, "i" },
    { 25, "i" },
    { 26, "f" },
    { 27, "f" },
    { 28, "i" },
    { 29, "ffi" },
    { 30, "i" },
    { 31, "i" },
    { 32, "iiffi" },
    { 33, "iii" },
    { 34, "iii" },
    { 35, "iiiif" },
    { 36, "iiff" },
    { 37, "ii" },
    { 38, "ff" },
    { 42, "ff" },
    { 50, "fff" },
    { 52, "fff" },
    { 55, "iii" },
    { 59, "ii" },
    { 60, "ff" },
    { 69, "iiii" },
    { 79, "i" },
    { 80, "i" },
    { 0xffff, "" }
};

static const opcode_fmt_t formats_v3[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "i" },
    { 4, "ii" },
    { 5, "iii" },
    { 6, "fff" },
    { 7, "ff" },
    { 8, "i" },
    { 9, "iii" },
    { 10, "" },
    { 12, "fff" },
    { 13, "fff" },
    { 14, "ff" },
    { 15, "ii" },
    { 16, "i" },
    { 17, "ffii" },
    { 18, "ffii" },
    { 20, "" },
    { 21, "i" },
    { 22, "" },
    { 23, "" },
    { 24, "i" },
    { 25, "i" },
    { 26, "f" },
    { 27, "f" },
    { 28, "i" },
    { 30, "i" },
    { 31, "i" },
    { 32, "iifff" },
    { 33, "iiiii" },
    { 34, "iii" },
    { 35, "iiiif" },
    { 36, "iiff" },
    { 37, "ii" },
    { 38, "ff" },
    { 40, "ff" },
    { 42, "ff" },
    { 44, "ff" },
    { 49, "iii" },
    { 50, "fff" },
    { 52, "fff" },
    { 54, "fff" },
    { 55, "iii" },
    { 56, "fff" },
    { 59, "ii" },
    { 60, "ff" },
    { 69, "iiii" },
    { 79, "i" },
    { 80, "f" },
    { 81, "f" },
    { 82, "i" },
    { 83, "i" },
    { 85, "i" },
    { 86, "iiiii" },
    { 87, "iii" },
    { 89, "" },
    { 0xffff, "" }
};

static const opcode_fmt_t formats_v4p[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "i" },
    { 4, "ii" },
    { 5, "iii" },
    { 6, "ii" },
    { 7, "ff" },
    { 8, "ii" },
    { 9, "ff" },
    { 11, "ff" },
    { 13, "ff" },
    { 18, "iii" },
    { 19, "fff" },
    { 21, "fff" },
    { 22, "iii" },
    { 23, "fff" },
    { 24, "iii" },
    { 25, "fff" },
    { 26, "iii" },
    { 27, "fff" },
    { 30, "iiii" },
    { 40, "ii" },
    { 42, "ff" },
    { 43, "ff" },
    { 48, "fff" },
    { 49, "fff" },
    { 50, "ff" },
    { 51, "i" },
    { 52, "iii" },
    { 53, "fff" },
    { 56, "iifff" },
    { 57, "iiiii" },
    { 58, "iii" },
    { 59, "iifif" },
    { 60, "iiff" },
    { 61, "" },
    { 63, "" },
    { 64, "i" },
    { 65, "i" },
    { 66, "i" },
    { 67, "i" },
    { 68, "i" },
    { 69, "" },
    { 70, "f" },
    { 71, "f" },
    { 73, "i" },
    { 74, "i" },
    { 75, "i" },
    { 76, "iii" },
    { 77, "i" },
    { 78, "iiiii" },
    { 79, "iii" },
    { 80, "i" },
    { 81, "" },
    { 82, "i" },
    { 83, "" },
    { 84, "i" },
    { 85, "i" },
    { 86, "i" },
    { 87, "i" },
    { 88, "i" },
    { 89, "i" },
    { 90, "i" },
    { 91, "i" },
    { 92, "i" },
    { 93, "iif" },
    { 94, "iif" },
    { 95, "i" },
    { 96, "iff" },
    { 100, "ifffffiffi" },
    { 101, "i" },
    { 102, "ii" },
    { 103, "ff" },
    { 104, "fi" },
    { 105, "fi" },
    { 106, "fi" },
    { 107, "iiff" },
    { 108, "ff" },
    { 110, "ff" },
    { 111, "i" },
    { 112, "i" },
    { 0xffff, "" }
};

/* The order and sizes of fields changed for TH11. */
static void
convert_header_to_old(anm_header_t* header)
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

static void
convert_header_to_11(anm_header_t* oldheader)
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

static char*
anm_get_name(anm_t* anm, const char* name)
{
    unsigned int i;
    for (i = 0; i < anm->name_count; ++i) {
        if (strcmp(name, anm->names[i]) == 0) {
            return anm->names[i];
        }
    }

    ++anm->name_count;
    anm->names = realloc(anm->names, sizeof(char*) * anm->name_count);
    return anm->names[anm->name_count - 1] = strdup(name);
}

static anm_t*
anm_read_file(const char* filename)
{
    anm_t* anm;
    uint32_t offset = 0;
    long filesize;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n", argv0, filename, strerror(errno));
        exit(1);
    }

    filesize = util_fsize(f);

    anm = malloc(sizeof(anm_t));
    anm->name_count = 0;
    anm->names = NULL;
    anm->entry_count = 0;
    anm->entries = NULL;

    for (;;) {
        entry_t* entry;
        char name[256];
        unsigned int i;

        ++anm->entry_count;
        anm->entries = realloc(anm->entries, sizeof(entry_t) * anm->entry_count);

        entry = &anm->entries[anm->entry_count - 1];
        memset(&entry->thtx, 0, sizeof(thtx_header_t));
        entry->name = NULL;
        entry->name2 = NULL;
        entry->sprite_count = 0;
        entry->sprites = NULL;
        entry->script_count = 0;
        entry->scripts = NULL;
        entry->data_size = 0;
        entry->data = NULL;

        util_seek(f, offset);
        util_read(f, &entry->header, sizeof(anm_header_t));

        /* XXX: This is not a particularly good way of detecting this. */
        if (entry->header.zero1 != 0)
            convert_header_to_old(&entry->header);

        if (entry->header.version != 0 &&
            entry->header.version != 2 &&
            entry->header.version != 3 &&
            entry->header.version != 4 &&
            entry->header.version != 7) {
            fprintf(stderr, "%s:%s:%u: unknown version: %u\n",
                argv0, current_input, anm->entry_count, entry->header.version);
            if (!option_force) abort();
        }
        if (entry->header.format != FORMAT_BGRA8888 &&
            entry->header.format != FORMAT_BGR565 &&
            entry->header.format != FORMAT_BGRA4444 &&
            entry->header.format != FORMAT_GRAY8) {
            if (!(entry->header.format == 0 && entry->header.thtxoffset == 0)) {
                fprintf(stderr, "%s:%s:%u: unknown format: %u\n",
                    argv0, current_input, anm->entry_count, entry->header.format);
                if (!option_force) abort();
            }
        }
        if (entry->header.unknown1 != 0 &&
            entry->header.unknown1 != 1 &&
            entry->header.unknown1 != 10 &&
            entry->header.unknown1 != 11) {
            fprintf(stderr, "%s:%s:%u: unknown value for unknown1: %u\n",
                argv0, current_input, anm->entry_count, entry->header.unknown1);
            if (!option_force) abort();
        }
        if (entry->header.hasdata == 0 &&
            entry->header.thtxoffset != 0) {
            fprintf(stderr, "%s:%s:%u: hasdata and thtxoffset do not match: %u, %u\n",
                argv0, current_input, anm->entry_count, entry->header.hasdata, entry->header.thtxoffset);
            if (!option_force) abort();
        }
        if (entry->header.hasdata != 0 &&
            entry->header.hasdata != 1) {
            fprintf(stderr, "%s:%s:%u: unknown value for hasdata: %u\n",
                argv0, current_input, anm->entry_count, entry->header.hasdata);
            if (!option_force) abort();
        }
        if (entry->header.zero1 != 0) {
            fprintf(stderr, "%s:%s:%u: unknown value for zero1: %u\n",
                argv0, current_input, anm->entry_count, entry->header.zero1);
            if (!option_force) abort();
        }
        if (entry->header.zero3 != 0) {
            fprintf(stderr, "%s:%s:%u: unknown value for zero3: %u\n",
                argv0, current_input, anm->entry_count, entry->header.zero3);
            if (!option_force) abort();
        }

        /* Lengths, including padding, observed are: 16, 32, 48. */
        entry->name = NULL;
        util_seek(f, offset + entry->header.nameoffset);
        fgets(name, 256, f);

        entry->name = anm_get_name(anm, name);

        if (entry->header.version == 0 && entry->header.y != 0) {
            util_seek(f, offset + entry->header.y);
            fgets(name, 256, f);

            entry->name2 = strdup(name);
        }

        util_seek(f, offset + sizeof(anm_header_t));

        /* Parse any sprites in the entry. */
        if (entry->header.sprites) {
            uint32_t* offsets = NULL;
            int sequential = 1;

            offsets = malloc(sizeof(uint32_t) * entry->header.sprites);
            entry->sprites = malloc(sizeof(sprite_t) * entry->header.sprites);
            entry->sprite_count = entry->header.sprites;
            util_read(f, offsets, sizeof(uint32_t) * entry->header.sprites);

            /* Check that the sprites are stored packed. */
            for (i = 1; i < entry->header.sprites; ++i) {
                if (offsets[i] - offsets[i - 1] != sizeof(sprite_t)) {
                    /* This should never happen. */
                    sequential = 0;
                    break;
                }
            }

            if (sequential) {
                util_seek(f, offset + offsets[0]);
                util_read(f, entry->sprites, sizeof(sprite_t) * entry->header.sprites);
            } else {
                for (i = 0; i < entry->header.sprites; ++i) {
                    util_seek(f, offset + offsets[i]);
                    util_read(f, &entry->sprites[i], sizeof(sprite_t));
                }
            }

            free(offsets);
        }

        if (entry->header.scripts) {
            util_seek(f, offset + sizeof(anm_header_t) + sizeof(uint32_t) * entry->header.sprites);

            entry->script_count = entry->header.scripts;
            entry->scripts = malloc(entry->script_count * sizeof(anm_script_t));

            for (i = 0; i < entry->script_count; ++i) {
                util_read(f, &entry->scripts[i], ANM_SCRIPT_SIZE);
                entry->scripts[i].instr_count = 0;
                entry->scripts[i].instrs = NULL;
            }

            for (i = 0; i < entry->header.scripts; ++i) {
                long limit = 0;

                util_seek(f, offset + entry->scripts[i].offset);

                if (i < entry->header.scripts - 1)
                    limit = offset + entry->scripts[i + 1].offset;
                else if (entry->header.thtxoffset)
                    limit = offset + entry->header.thtxoffset;
                else if (entry->header.nextoffset)
                    limit = offset + entry->header.nextoffset;
                else
                    limit = filesize;

                for (;;) {
                    anm_instr_t* instr;
                    if (entry->header.version == 0) {
                        anm_instr0_t tempinstr;

                        if (ftell(f) + ANM_INSTR0_SIZE > limit)
                            break;

                        util_read(f, &tempinstr, ANM_INSTR0_SIZE);

                        ++entry->scripts[i].instr_count;
                        entry->scripts[i].instrs = realloc(entry->scripts[i].instrs, entry->scripts[i].instr_count * sizeof(anm_instr_t));
                        instr = &entry->scripts[i].instrs[entry->scripts[i].instr_count - 1];
                        instr->type = tempinstr.type;
                        instr->length = tempinstr.length;
                        instr->time = tempinstr.time;
                        instr->param_mask = 0;

                        if (instr->length) {
                            instr->data = malloc(instr->length);
                            util_read(f, instr->data, instr->length);
                        } else {
                            instr->data = NULL;
                        }

                        if (instr->type == 0 && instr->time == 0)
                            break;
                    } else {
                        anm_instr_t tempinstr;

                        if (ftell(f) + ANM_INSTR_SIZE > limit)
                            break;

                        util_read(f, &tempinstr, ANM_INSTR_SIZE);

                        ++entry->scripts[i].instr_count;
                        entry->scripts[i].instrs = realloc(entry->scripts[i].instrs, entry->scripts[i].instr_count * sizeof(anm_instr_t));
                        instr = &entry->scripts[i].instrs[entry->scripts[i].instr_count - 1];
                        instr->type = tempinstr.type;
                        instr->length = tempinstr.length;
                        instr->time = tempinstr.time;
                        instr->param_mask = tempinstr.param_mask;

                        if (instr->length > ANM_INSTR_SIZE) {
                            instr->data = malloc(instr->length - ANM_INSTR_SIZE);
                            util_read(f, instr->data, instr->length - ANM_INSTR_SIZE);
                        } else {
                            instr->data = NULL;
                        }

                        if (tempinstr.type == 0xffff)
                            break;
                    }
                }
            }
        }

        /* TH06 doesn't store entry data. */
        if (entry->header.hasdata) {
            char* data = NULL;
            char magic[5] = { 0 };

            util_seek(f, offset + entry->header.thtxoffset);

            util_read(f, magic, 4);
            if (strcmp(magic, "THTX") != 0) {
                fprintf(stderr, "%s:%s:%s: unknown thtx magic: %s\n",
                    argv0, current_input, entry->name, magic);
                if (!option_force) abort();
            }

            util_read(f, &entry->thtx, sizeof(thtx_header_t));
            if (entry->thtx.zero != 0) {
                fprintf(stderr, "%s:%s:%s: unknown value for zero: %u\n",
                    argv0, current_input, entry->name, entry->thtx.zero);
                if (!option_force) abort();
            }
            if (entry->header.format != entry->thtx.format) {
                fprintf(stderr, "%s:%s:%s: header format does not match thtx header format: %u != %u\n",
                    argv0, current_input, entry->name,
                    entry->header.format, entry->thtx.format);
                if (!option_force) abort();
            }

            if (entry->thtx.w * entry->thtx.h * format_Bpp(entry->thtx.format) > entry->thtx.size) {
                fprintf(stderr, "%s:%s:%s: w*h*Bpp is greater than data size: %u*%u*%u > %u\n",
                    argv0, current_input, entry->name,
                    entry->thtx.w, entry->thtx.h, format_Bpp(entry->thtx.format), entry->thtx.size);
                if (!option_force) abort();
            }

            data = malloc(entry->thtx.size);
            util_read(f, data, entry->thtx.size);

            entry->data_size = entry->thtx.w * entry->thtx.h * 4;
            entry->data = fmt_to_rgba(data, entry->thtx.w * entry->thtx.h, entry->thtx.format);

            free(data);
        }

        if (!entry->header.nextoffset)
            break;

        offset += entry->header.nextoffset;
    }

    fclose(f);

    qsort(anm->names, anm->name_count, sizeof(char*), util_strpcmp);

    return anm;
}

static void
util_total_entry_size(const anm_t* anm, const char* name, unsigned int* widthptr, unsigned int* heightptr)
{
    unsigned int i;
    unsigned int width = 0;
    unsigned int height = 0;

    for (i = 0; i < anm->entry_count; ++i) {
        if (anm->entries[i].name == name) {
            if (!anm->entries[i].header.hasdata)
                return;
            if (anm->entries[i].header.x + anm->entries[i].thtx.w > width)
                width = anm->entries[i].header.x + anm->entries[i].thtx.w;
            if (anm->entries[i].header.y + anm->entries[i].thtx.h > height)
                height = anm->entries[i].header.y + anm->entries[i].thtx.h;
        }
    }

    *widthptr = width;
    *heightptr = height;
}

#ifdef HAVE_LIBPNG

typedef struct {
    char* data;
    unsigned int width;
    unsigned int height;
    format_t format;
} image_t;

static image_t*
png_read(FILE* stream, format_t format)
{
    unsigned int y;
    image_t* image;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytepp row_pointers;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, stream);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    /* XXX: Consider just converting everything ... */
    if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGB_ALPHA) {
        /* XXX: current_input? exit(1)? */
        fprintf(stderr, "%s: %s must be RGBA\n", argv0, current_input);
        exit(1);
    }

    row_pointers = png_get_rows(png_ptr, info_ptr);

    image = malloc(sizeof(image_t));
    image->width = png_get_image_width(png_ptr, info_ptr);
    image->height = png_get_image_height(png_ptr, info_ptr);
    image->format = format;
    image->data = malloc(image->width * image->height * format_Bpp(image->format));

    for (y = 0; y < image->height; ++y) {
        if (format == FORMAT_RGBA8888) {
            memcpy(image->data + y * image->width * format_Bpp(image->format), row_pointers[y], image->width * format_Bpp(image->format));
        } else {
            unsigned char* converted_data = rgba_to_fmt((uint32_t*)row_pointers[y], image->width, image->format);
            memcpy(image->data + y * image->width * format_Bpp(image->format), converted_data, image->width * format_Bpp(image->format));
            free(converted_data);
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return image;
}

static void
png_write(FILE* stream, image_t* image)
{
    unsigned int y;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytepp imagep;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, stream);
    png_set_IHDR(png_ptr, info_ptr,
        image->width, image->height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    imagep = malloc(sizeof(png_byte*) * image->height);
    for (y = 0; y < image->height; ++y)
        imagep[y] = (png_byte*)(image->data + y * image->width * 4);

    png_write_image(png_ptr, imagep);
    free(imagep);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
}

static void
anm_replace(const anm_t* anm, const char* name, const char* filename)
{
    const format_t formats[] = { FORMAT_BGRA8888, FORMAT_BGR565, FORMAT_BGRA4444, FORMAT_GRAY8 };
    unsigned int f, i, y;
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
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n", argv0, filename, strerror(errno));
        exit(1);
    }
    image = png_read(stream, FORMAT_RGBA8888);
    fclose(stream);

    if (width != image->width || height != image->height) {
        fprintf(stderr, "%s:%s:%s: wrong image dimensions for %s: %u, %u instead of %u, %u\n", argv0, current_input, name, filename, image->width, image->height, width, height);
        exit(1);
    }

    for (f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {
        unsigned char* converted_data = NULL;
        for (i = 0; i < anm->entry_count; ++i) {
            if (anm->entries[i].name == name &&
                anm->entries[i].header.format == formats[f] &&
                anm->entries[i].header.hasdata) {

                if (!converted_data)
                    converted_data = rgba_to_fmt((uint32_t*)image->data, width * height, formats[f]);

                for (y = anm->entries[i].header.y; y < anm->entries[i].header.y + anm->entries[i].thtx.h; ++y) {
                    memcpy(anm->entries[i].data + (y - anm->entries[i].header.y) * anm->entries[i].thtx.w * format_Bpp(formats[f]),
                           converted_data + y * width * format_Bpp(formats[f]) + anm->entries[i].header.x * format_Bpp(formats[f]),
                           anm->entries[i].thtx.w * format_Bpp(formats[f]));
                }
            }
        }
        free(converted_data);
    }

    free(image->data);
    free(image);
}

static void
anm_extract(const anm_t* anm, const char* name)
{
    const format_t formats[] = { FORMAT_GRAY8, FORMAT_BGRA4444, FORMAT_BGR565, FORMAT_BGRA8888 };
    FILE* stream;
    image_t image;

    unsigned int f, i, y;

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
        for (i = 0; i < anm->entry_count; ++i) {
            if (anm->entries[i].header.hasdata && anm->entries[i].name == name && formats[f] == anm->entries[i].header.format) {
                for (y = anm->entries[i].header.y; y < anm->entries[i].header.y + anm->entries[i].thtx.h; ++y) {
                    memcpy(image.data + y * image.width * 4 + anm->entries[i].header.x * 4,
                           anm->entries[i].data + (y - anm->entries[i].header.y) * anm->entries[i].thtx.w * 4,
                           anm->entries[i].thtx.w * 4);
                }
            }
        }
    }

    util_makepath(name);
    stream = fopen(name, "wb");
    if (!stream) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n", argv0, name, strerror(errno));
        return;
    }
    png_write(stream, &image);
    fclose(stream);

    free(image.data);
}

static void
anm_dump(FILE* stream, const anm_t* anm)
{
    unsigned int i;

    for (i = 0; i < anm->entry_count; ++i) {
        unsigned int j;
        const opcode_fmt_t* formats = NULL;
        unsigned int format_count = 0;
        entry_t* entry = &anm->entries[i];

        if (entry->header.version == 0) {
            formats = formats_v0;
            format_count = sizeof(formats_v0) / sizeof(formats_v0[0]);
        } else if (entry->header.version == 2) {
            formats = formats_v2;
            format_count = sizeof(formats_v2) / sizeof(formats_v2[0]);
        } else if (entry->header.version == 3) {
            formats = formats_v3;
            format_count = sizeof(formats_v3) / sizeof(formats_v3[0]);
        } else if (entry->header.version == 4 || entry->header.version == 7) {
            formats = formats_v4p;
            format_count = sizeof(formats_v4p) / sizeof(formats_v4p[0]);
        } else {
            fprintf(stderr, "%s:%s: could not find a format description for version %u\n", argv0, current_input, entry->header.version);
            abort();
        }

        fprintf(stream, "ENTRY %u\n", entry->header.version);
        fprintf(stream, "Name: %s\n", entry->name);
        if (entry->name2)
            fprintf(stream, "Name2: %s\n", entry->name2);
        fprintf(stream, "Format: %u\n", entry->header.format);
        fprintf(stream, "Width: %u\n", entry->header.w);
        fprintf(stream, "Height: %u\n", entry->header.h);
        if (entry->header.x != 0)
            fprintf(stream, "X-Offset: %u\n", entry->header.x);
        if (!entry->name2 && entry->header.y != 0)
            fprintf(stream, "Y-Offset: %u\n", entry->header.y);
        if (entry->header.zero1 != 0)
            fprintf(stream, "Zero1: %u\n", entry->header.zero1);
        if (entry->header.zero2 != 0)
            fprintf(stream, "Zero2: %u\n", entry->header.zero2);
        if (entry->header.zero3 != 0)
            fprintf(stream, "Zero3: %u\n", entry->header.zero3);
        if (entry->header.unknown1 != 0)
            fprintf(stream, "Unknown1: %u\n", entry->header.unknown1);
        if (entry->header.hasdata) {
            fprintf(stream, "HasData: %u\n", entry->header.hasdata);
            fprintf(stream, "THTX-Size: %u\n", entry->thtx.size);
            fprintf(stream, "THTX-Format: %u\n", entry->thtx.format);
            fprintf(stream, "THTX-Width: %u\n", entry->thtx.w);
            fprintf(stream, "THTX-Height: %u\n", entry->thtx.h);
            fprintf(stream, "THTX-Zero: %u\n", entry->thtx.zero);
        }

        fprintf(stream, "\n");

        for (j = 0; j < entry->sprite_count; ++j) {
            sprite_t* sprite = &entry->sprites[j];
            fprintf(stream, "Sprite: %u %.f*%.f+%.f+%.f\n",
                sprite->id,
                sprite->w, sprite->h,
                sprite->x, sprite->y);
        }

        fprintf(stream, "\n");

        for (j = 0; j < entry->script_count; ++j) {
            unsigned int iter_instrs, iter_formats;
            anm_script_t* scr = &entry->scripts[j];

            fprintf(stream, "Script: %d\n", scr->id);

            /* TODO: Compare format length to data length. */
            for (iter_instrs = 0; iter_instrs < scr->instr_count; ++iter_instrs) {
                int done = 0;
                anm_instr_t* instr = &scr->instrs[iter_instrs];
                fprintf(stream, "Instruction: %hu %hu %hu", instr->time, instr->param_mask, instr->type);
                for (iter_formats = 0; iter_formats < format_count; ++iter_formats) {
                    if (formats[iter_formats].type == instr->type) {
                        const char* data = instr->data;
                        const char* format = formats[iter_formats].format;
                        while (*format) {
                            switch (*format) {
                            case 'i':
                                fprintf(stream, " %i", *(int32_t*)data);
                                data += sizeof(int32_t);
                                break;
                            case 'f':
                                fprintf(stream, " %sf", util_printfloat(data));
                                data += sizeof(float);
                                break;
                            default:
                                fprintf(stderr, "%s: invalid format descriptor `%c'\n", argv0, *format);
                                abort();
                            }
                            ++format;
                        }
                        fprintf(stream, "\n");
                        done = 1;
                        break;
                    }
                }
                if (!done) {
                    fprintf(stderr, "%s: no format descriptor found for %d\n", argv0, instr->type);
                    abort();
                }
            }
        }

        fprintf(stream, "\n");
    }
}

static anm_t*
anm_create(const char* spec)
{
    FILE* f;
    char line[4096];
    anm_t* anm;
    entry_t* entry = NULL;
    anm_script_t* script = NULL;
    anm_instr_t* instr = NULL;

    f = fopen(spec, "r");
    if (!f) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n", argv0, spec, strerror(errno));
        exit(1);
    }

    anm = malloc(sizeof(anm_t));
    anm->name_count = 0;
    anm->names = NULL;
    anm->entry_count = 0;
    anm->entries = NULL;

    while (fgets(line, 4096, f)) {
        char* linep = line;

        if (strncmp(linep, "ENTRY ", 6) == 0) {
            anm->entry_count++;
            anm->entries = realloc(anm->entries, anm->entry_count * sizeof(entry_t));
            entry = &anm->entries[anm->entry_count - 1];
            memset(entry, 0, sizeof(entry_t));
            sscanf(linep, "ENTRY %u", &entry->header.version);
        } else if (strncmp(linep, "Name: ", 6) == 0) {
            char name[256];
            sscanf(linep, "Name: %s", name);
            entry->name = anm_get_name(anm, name);
        } else if (strncmp(linep, "Name2: ", 6) == 0) {
            char name[256];
            sscanf(linep, "Name2: %s", name);
            entry->name2 = strdup(name);
        } else if (strncmp(linep, "Sprite: ", 8) == 0) {
            sprite_t* sprite;
            entry->sprite_count++;
            entry->sprites = realloc(entry->sprites, entry->sprite_count * sizeof(sprite_t));
            sprite = &entry->sprites[entry->sprite_count - 1];

            if (5 != sscanf(linep, "Sprite: %u %f*%f+%f+%f", &sprite->id, &sprite->w, &sprite->h, &sprite->x, &sprite->y)) {
                fprintf(stderr, "%s: Sprite parsing failed for %s\n", argv0, linep);
                exit(1);
            }
        } else if (strncmp(linep, "Script: ", 8) == 0) {
            entry->script_count++;
            entry->scripts = realloc(entry->scripts, entry->script_count * sizeof(anm_script_t));
            script = &entry->scripts[entry->script_count - 1];
            script->offset = 0;
            script->instr_count = 0;
            script->instrs = NULL;
            if (1 != sscanf(linep, "Script: %d", &script->id)) {
                fprintf(stderr, "%s: Script parsing failed for %s\n", argv0, linep);
                exit(1);
            }
        } else if (strncmp(linep, "Instruction: ", 13) == 0) {
            char* tmp = linep + 13;
            char* before;
            char* after = NULL;

            script->instr_count++;
            script->instrs = realloc(script->instrs, script->instr_count * sizeof(anm_instr_t));
            instr = &script->instrs[script->instr_count - 1];
            instr->data = NULL;
            instr->length = ANM_INSTR_SIZE;

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
                    instr->data = realloc(instr->data, instr->length - ANM_INSTR_SIZE);
                    if (*after == 'f' || *after == '.') {
                        f = strtof(before, &after);
                        memcpy(instr->data + instr->length - ANM_INSTR_SIZE - sizeof(float), &f, sizeof(float));
                        /* Skip 'f'. */
                        ++after;
                    } else {
                        memcpy(instr->data + instr->length - ANM_INSTR_SIZE - sizeof(int32_t), &i, sizeof(int32_t));
                    }
                }

                before = after;
            }

            if (instr->type == 0xffff)
                instr->length = 0;
        } else {
            sscanf(linep, "Format: %u", &entry->header.format);
            sscanf(linep, "Width: %u", &entry->header.w);
            sscanf(linep, "Height: %u", &entry->header.h);
            sscanf(linep, "X-Offset: %u", &entry->header.x);
            sscanf(linep, "Y-Offset: %u", &entry->header.y);
            sscanf(linep, "Zero1: %u", &entry->header.zero1);
            sscanf(linep, "Zero2: %u", &entry->header.zero2);
            sscanf(linep, "Zero3: %u", &entry->header.zero3);
            sscanf(linep, "Unknown1: %u", &entry->header.unknown1);
            sscanf(linep, "HasData: %u", &entry->header.hasdata);
            sscanf(linep, "THTX-Size: %u", &entry->thtx.size);
            sscanf(linep, "THTX-Format: %hu", &entry->thtx.format);
            sscanf(linep, "THTX-Width: %hu", &entry->thtx.w);
            sscanf(linep, "THTX-Height: %hu", &entry->thtx.h);
            sscanf(linep, "THTX-Zero: %hu", &entry->thtx.zero);
        }
    }

    fclose(f);

    return anm;
}

static void
anm_write(anm_t* anm, const char* filename)
{
    FILE* stream;
    unsigned int i;

    stream = fopen(filename, "wb");
    if (!stream) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n", argv0, filename, strerror(errno));
        exit(1);
    }

    for (i = 0; i < anm->entry_count; ++i) {
        unsigned int base = ftell(stream);
        unsigned int namepad = 0;
        entry_t* entry = &anm->entries[i];
        char* padding;
        unsigned int j;
        unsigned int spriteoffset;

        namepad = (16 - strlen(entry->name) % 16);

        /* TODO: Make sure the correct header version is written, convert when needed. */

        fseek(stream, sizeof(anm_header_t), SEEK_CUR);
        fseek(stream, entry->sprite_count * sizeof(uint32_t), SEEK_CUR);
        fseek(stream, entry->script_count * ANM_SCRIPT_SIZE, SEEK_CUR);

        entry->header.nameoffset = ftell(stream) - base;
        fwrite(entry->name, strlen(entry->name), 1, stream);

        padding = calloc(1, namepad);
        fwrite(padding, namepad, 1, stream);
        free(padding);

        if (entry->name2 && entry->header.version == 0) {
            namepad = (16 - strlen(entry->name2) % 16);

            entry->header.y = ftell(stream) - base;
            fwrite(entry->name2, strlen(entry->name2), 1, stream);

            padding = calloc(1, namepad);
            fwrite(padding, namepad, 1, stream);
            free(padding);
        }

        spriteoffset = ftell(stream) - base;
        fwrite(entry->sprites, sizeof(sprite_t), entry->sprite_count, stream);

        for (j = 0; j < entry->script_count; ++j) {
            unsigned int k;

            entry->scripts[j].offset = ftell(stream) - base;
            for (k = 0; k < entry->scripts[j].instr_count; ++k) {
                if (entry->header.version == 0) {
                    anm_instr0_t instr;
                    instr.time = entry->scripts[j].instrs[k].time;
                    instr.type = entry->scripts[j].instrs[k].type;
                    instr.length = entry->scripts[j].instrs[k].length - ANM_INSTR_SIZE;
                    fwrite(&instr, ANM_INSTR0_SIZE, 1, stream);
                    if (entry->scripts[j].instrs[k].data)
                        fwrite(entry->scripts[j].instrs[k].data, entry->scripts[j].instrs[k].length - ANM_INSTR_SIZE, 1, stream);
                } else {
                    fwrite(&entry->scripts[j].instrs[k], ANM_INSTR_SIZE, 1, stream);
                    if (entry->scripts[j].instrs[k].data)
                        fwrite(entry->scripts[j].instrs[k].data, entry->scripts[j].instrs[k].length - ANM_INSTR_SIZE, 1, stream);
                }
            }
        }

        if (entry->header.hasdata) {
            entry->header.thtxoffset = ftell(stream) - base;

            fputs("THTX", stream);
            fwrite(&entry->thtx, sizeof(thtx_header_t), 1, stream);

            fwrite(entry->data, entry->data_size, 1, stream);
        }

        if (i == anm->entry_count - 1)
            entry->header.nextoffset = 0;
        else
            entry->header.nextoffset = ftell(stream) - base;

        entry->header.sprites = entry->sprite_count;
        entry->header.scripts = entry->script_count;

        fseek(stream, base, SEEK_SET);

        if (entry->header.version >= 7) {
            convert_header_to_11(&entry->header);
            fwrite(&entry->header, sizeof(anm_header_t), 1, stream);
            convert_header_to_old(&entry->header);
        } else {
            fwrite(&entry->header, sizeof(anm_header_t), 1, stream);
        }

        for (j = 0; j < entry->sprite_count; ++j) {
            uint32_t ofs = spriteoffset + j * sizeof(sprite_t);
            fwrite(&ofs, sizeof(uint32_t), 1, stream);
        }

        for (j = 0; j < entry->script_count; ++j) {
            fwrite(&entry->scripts[j], ANM_SCRIPT_SIZE, 1, stream);
        }

        fseek(stream, base + entry->header.nextoffset, SEEK_SET);
    }

    fclose(stream);
}

#endif

static void
anm_free(anm_t* anm)
{
    unsigned int i, j, k;

    if (!anm)
        return;

    if (anm->entries) {
        for (i = 0; i < anm->entry_count; ++i) {
            if (anm->entries[i].name2)
                free(anm->entries[i].name2);
            if (anm->entries[i].sprites)
                free(anm->entries[i].sprites);
            if (anm->entries[i].scripts) {
                for (j = 0; j < anm->entries[i].script_count; ++j) {
                    if (anm->entries[i].scripts[j].instrs) {
                        for (k = 0; k < anm->entries[i].scripts[j].instr_count; ++k) {
                            if (anm->entries[i].scripts[j].instrs[k].data)
                                free(anm->entries[i].scripts[j].instrs[k].data);
                        }
                        free(anm->entries[i].scripts[j].instrs);
                    }
                }
                free(anm->entries[i].scripts);
            }
            if (anm->entries[i].data)
                free(anm->entries[i].data);
        }
        free(anm->entries);
    }

    if (anm->names) {
        for (i = 0; i < anm->name_count; ++i)
            if (anm->names[i])
                free(anm->names[i]);
        free(anm->names);
    }

    free(anm);
}

static void
print_usage(void)
{
    printf("Usage: %s [OPTION]... MODE ARCHIVE [FILE]...\n"
           "MODE can be:\n"
           "  -l    list archive\n"
#ifdef HAVE_LIBPNG
           "  -x    extract archive\n"
           "  -r    replace entries in archive\n"
           "  -c F  create archive from F\n"
#endif
           "OPTION can be:\n"
           "  -f    ignore errors when possible\n"
           "  -h    display this help and exit\n"
           "  -V    display version information and exit\n\n"
           "Additional documentation might be available at <" PACKAGE_URL ">.\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n"
           , argv0);
}

int
main(int argc, char* argv[])
{
    int i;
    int filestart = 0;
    int mode = 0;
#ifdef HAVE_LIBPNG
    const char* archive_spec_file = NULL;
#endif

    argv0 = util_shortname(argv[0]);
    if (!argv0)
        argv0 = PACKAGE;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            print_usage();
            exit(0);
        } else if (strcmp(argv[i], "-V") == 0) {
            util_print_version("thanm", PACKAGE_THANM_VERSION);
            exit(0);
        } else if (strcmp(argv[i], "-f") == 0) {
            option_force = 1;
#ifdef HAVE_LIBPNG
        } else if (strcmp(argv[i], "-x") == 0) {
            mode = MODE_EXTRACT;
        } else if (strcmp(argv[i], "-r") == 0) {
            mode = MODE_REPLACE;
        } else if (strcmp(argv[i], "-c") == 0) {
            ++i;
            if (i >= argc) {
                print_usage();
                exit(1);
            }
            archive_spec_file = argv[i];
            mode = MODE_CREATE;
#endif
        } else if (strcmp(argv[i], "-l") == 0) {
            mode = MODE_LIST;
        } else {
            filestart = i;
            break;
        }
    }

    if (!filestart) {
        print_usage();
        exit(1);
    }

    current_input = util_shortname(argv[filestart]);

    if (mode == MODE_LIST) {
        anm_t* anm = anm_read_file(argv[filestart]);
        anm_dump(stdout, anm);
        anm_free(anm);
#ifdef HAVE_LIBPNG
    } else if (mode == MODE_EXTRACT) {
        anm_t* anm = anm_read_file(argv[filestart]);

        if (argc == filestart + 1) {
            /* Extract all files. */
            for (i = 0; i < (int)anm->name_count; ++i) {
                puts(anm->names[i]);
                anm_extract(anm, anm->names[i]);
            }
        } else {
            /* Extract all listed files. */
            for (i = filestart + 1; i < argc; ++i) {
                unsigned int j;
                for (j = 0; j < anm->name_count; ++j) {
                    if (strcmp(argv[i], anm->names[j]) == 0) {
                        anm_extract(anm, anm->names[j]);
                        goto extract_next;
                    }
                }
                fprintf(stderr, "%s:%s: %s not found in archive\n", argv0, current_input, argv[i]);
extract_next:
                ;
            }
        }

        anm_free(anm);
    } else if (mode == MODE_REPLACE) {
        FILE* anmfp;
        unsigned int offset = 0;
        anm_t* anm = anm_read_file(argv[filestart]);

        for (i = 0; i < (int)anm->name_count; ++i) {
            if (strcmp(argv[filestart + 1], anm->names[i]) == 0) {
                anm_replace(anm, anm->names[i], argv[filestart + 2]);
                goto replace_done;
            }
        }

        fprintf(stderr, "%s:%s: %s not found in archive\n", argv0, current_input, argv[filestart + 1]);

replace_done:

        anmfp = fopen(argv[filestart], "rb+");
        if (!anmfp) {
            fprintf(stderr, "%s: couldn't open %s for writing: %s\n", argv0, current_input, strerror(errno));
            exit(1);
        }

        for (i = 0; i < (int)anm->entry_count; ++i) {
            unsigned int nextoffset = anm->entries[i].header.nextoffset;
            if (strcmp(argv[filestart + 1], anm->entries[i].name) == 0 && anm->entries[i].header.hasdata) {
                fseek(anmfp, offset + anm->entries[i].header.thtxoffset + 4 + sizeof(thtx_header_t), SEEK_SET);
                fwrite(anm->entries[i].data, anm->entries[i].thtx.size, 1, anmfp);
            }
            offset += nextoffset;
        }

        fclose(anmfp);

        anm_free(anm);
    } else if (mode == MODE_CREATE) {
        anm_t* anm;

        anm = anm_create(archive_spec_file);

        /* Allocate enough space for the THTX data. */
        for (i = 0; i < (int)anm->entry_count; ++i) {
            if (anm->entries[i].header.hasdata) {
                anm->entries[i].data_size = anm->entries[i].thtx.size;;
                /* XXX: There are a few entries with a thtx.size greater than
                 *      w*h*Bpp.  The extra data appears to be all zeroes. */
                anm->entries[i].data = calloc(1, anm->entries[i].data_size);
            }
        }

        for (i = 0; i < (int)anm->name_count; ++i)
            anm_replace(anm, anm->names[i], anm->names[i]);

        anm_write(anm, argv[filestart]);

        anm_free(anm);
#endif
    } else {
        fprintf(stderr, "%s: no MODE specified\n", argv0);
        print_usage();
        exit(1);
    }

    exit(0);
}
