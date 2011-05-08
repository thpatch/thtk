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
#include <errno.h>
#ifdef HAVE_LIBPNG
#include <png.h>
#endif
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
#include "args.h"
#include "file.h"
#include "thanm.h"
#include "program.h"
#include "util.h"
#include "value.h"

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

#ifdef HAVE_LIBPNG
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
        for (i = 0; i < pixels; ++i) {
            out[i * sizeof(uint32_t) + 0] = data8[i * sizeof(uint32_t) + 2];
            out[i * sizeof(uint32_t) + 1] = data8[i * sizeof(uint32_t) + 1];
            out[i * sizeof(uint32_t) + 2] = data8[i * sizeof(uint32_t) + 0];
            out[i * sizeof(uint32_t) + 3] = data8[i * sizeof(uint32_t) + 3];
        }
    } else if (format == FORMAT_BGRA4444) {
        out = malloc(sizeof(uint16_t) * pixels);
        for (i = 0; i < pixels; ++i) {
            /* Use the extra precision for rounding. */
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
                     /* 00000000 00000000 11111000 -> 00000000 00011111 */
            out16[i] = ((data[i] &     0xf8) << 8)
                     /* 00000000 11111100 00000000 -> 00000111 11100000 */
                     | ((data[i] &   0xfc00) >> 5)
                     /* 11111000 00000000 00000000 -> 11111000 00000000 */
                     | ((data[i] & 0xf80000) >>19);
        }
    } else if (format == FORMAT_RGBA8888) {
        out = malloc(sizeof(uint32_t) * pixels);
        memcpy(out, data, sizeof(uint32_t) * pixels);
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
        for (i = 0; i < pixels; ++i) {
            out8[i * sizeof(uint32_t) + 0] = data[i * sizeof(uint32_t) + 2];
            out8[i * sizeof(uint32_t) + 1] = data[i * sizeof(uint32_t) + 1];
            out8[i * sizeof(uint32_t) + 2] = data[i * sizeof(uint32_t) + 0];
            out8[i * sizeof(uint32_t) + 3] = data[i * sizeof(uint32_t) + 3];
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
    } else if (format == FORMAT_RGBA8888) {
        memcpy(out, data, sizeof(uint32_t) * pixels);
    } else {
        fprintf(stderr, "%s: unknown format: %u\n", argv0, format);
        abort();
    }

    return (char*)out;
}
#endif

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

#ifdef HAVE_LIBPNG
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
#endif

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
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
            argv0, filename, strerror(errno));
        exit(1);
    }

    filesize = file_fsize(f);

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

        file_seek(f, offset);
        file_read(f, &entry->header, sizeof(anm_header_t));

        /* XXX: This is not a particularly good way of detecting this. */
        if (entry->header.zero1 != 0)
            convert_header_to_old(&entry->header);

        if (entry->header.version != 0 &&
            entry->header.version != 2 &&
            entry->header.version != 3 &&
            entry->header.version != 4 &&
            entry->header.version != 7 &&
            entry->header.version != 8) {
            fprintf(stderr, "%s:%s:%u: unknown version: %u\n",
                argv0, current_input, anm->entry_count, entry->header.version);
            if (!option_force) abort();
        }
        if (entry->header.format != FORMAT_BGRA8888 &&
            entry->header.format != FORMAT_BGR565 &&
            entry->header.format != FORMAT_BGRA4444 &&
            entry->header.format != FORMAT_RGBA8888 &&
            entry->header.format != FORMAT_GRAY8) {
            if (!(entry->header.format == 0 && entry->header.thtxoffset == 0)) {
                fprintf(stderr, "%s:%s:%u: unknown format: %u\n",
                    argv0, current_input, anm->entry_count,
                    entry->header.format);
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
            fprintf(stderr,
                "%s:%s:%u: hasdata and thtxoffset do not match: %u, %u\n",
                argv0, current_input, anm->entry_count, entry->header.hasdata,
                entry->header.thtxoffset);
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
        file_seek(f, offset + entry->header.nameoffset);
        fgets(name, 256, f);

        entry->name = anm_get_name(anm, name);

        if (entry->header.version == 0 && entry->header.y != 0) {
            file_seek(f, offset + entry->header.y);
            fgets(name, 256, f);

            entry->name2 = strdup(name);
        }

        file_seek(f, offset + sizeof(anm_header_t));

        /* Parse any sprites in the entry. */
        if (entry->header.sprites) {
            uint32_t* offsets = NULL;
            int sequential = 1;

            offsets = malloc(sizeof(uint32_t) * entry->header.sprites);
            entry->sprites = malloc(sizeof(sprite_t) * entry->header.sprites);
            entry->sprite_count = entry->header.sprites;
            file_read(f, offsets, sizeof(uint32_t) * entry->header.sprites);

            /* Check that the sprites are stored packed. */
            for (i = 1; i < entry->header.sprites; ++i) {
                if (offsets[i] - offsets[i - 1] != sizeof(sprite_t)) {
                    /* This should never happen. */
                    sequential = 0;
                    break;
                }
            }

            if (sequential) {
                file_seek(f, offset + offsets[0]);
                file_read(f, entry->sprites,
                    sizeof(sprite_t) * entry->header.sprites);
            } else {
                for (i = 0; i < entry->header.sprites; ++i) {
                    file_seek(f, offset + offsets[i]);
                    file_read(f, &entry->sprites[i], sizeof(sprite_t));
                }
            }

            free(offsets);
        }

        if (entry->header.scripts) {
            file_seek(f, offset + sizeof(anm_header_t) + sizeof(uint32_t)
                         * entry->header.sprites);

            entry->script_count = entry->header.scripts;
            entry->scripts = malloc(entry->script_count * sizeof(anm_script_t));

            for (i = 0; i < entry->script_count; ++i) {
                file_read(f, &entry->scripts[i], ANM_SCRIPT_SIZE);
                entry->scripts[i].instr_count = 0;
                entry->scripts[i].instrs = NULL;
            }

            for (i = 0; i < entry->header.scripts; ++i) {
                unsigned long limit = 0;

                file_seek(f, offset + entry->scripts[i].offset);

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
                        anm_instr0_t temp_instr;

                        if (file_tell(f) + sizeof(anm_instr0_t) > limit)
                            break;

                        file_read(f, &temp_instr, sizeof(anm_instr0_t));

                        if (temp_instr.type == 0 && temp_instr.time == 0)
                            break;

                        ++entry->scripts[i].instr_count;
                        entry->scripts[i].instrs = realloc(
                            entry->scripts[i].instrs,
                            entry->scripts[i].instr_count * sizeof(anm_instr_t*));
                        instr = malloc(sizeof(anm_instr_t) + temp_instr.length);
                        instr->type = temp_instr.type;
                        instr->length = sizeof(anm_instr_t) + temp_instr.length;
                        instr->time = temp_instr.time;
                        instr->param_mask = 0;
                        entry->scripts[i].instrs[entry->scripts[i].instr_count - 1] = instr;

                        if (temp_instr.length)
                            file_read(f, instr->data, temp_instr.length);
                    } else {
                        anm_instr_t temp_instr;

                        if (file_tell(f) + sizeof(anm_instr_t) > limit)
                            break;

                        file_read(f, &temp_instr, sizeof(anm_instr_t));

                        if (temp_instr.type == 0xffff)
                            break;

                        ++entry->scripts[i].instr_count;
                        entry->scripts[i].instrs = realloc(
                            entry->scripts[i].instrs,
                            entry->scripts[i].instr_count * sizeof(anm_instr_t*));
                        instr = malloc(temp_instr.length ? temp_instr.length : sizeof(anm_instr_t));
                        *instr = temp_instr;
                        entry->scripts[i].instrs[entry->scripts[i].instr_count - 1] = instr;

                        if (instr->length > sizeof(anm_instr_t))
                            file_read(f, instr->data, instr->length - sizeof(anm_instr_t));
                    }
                }
            }
        }

        /* TH06 doesn't store entry data. */
        if (entry->header.hasdata) {
#ifdef HAVE_LIBPNG
            char* data = NULL;
#endif
            char magic[5] = { 0 };

            file_seek(f, offset + entry->header.thtxoffset);

            file_read(f, magic, 4);
            if (strcmp(magic, "THTX") != 0) {
                fprintf(stderr, "%s:%s:%s: unknown thtx magic: %s\n",
                    argv0, current_input, entry->name, magic);
                if (!option_force) abort();
            }

            file_read(f, &entry->thtx, sizeof(thtx_header_t));
            if (entry->thtx.zero != 0) {
                fprintf(stderr, "%s:%s:%s: unknown value for zero: %u\n",
                    argv0, current_input, entry->name, entry->thtx.zero);
                if (!option_force) abort();
            }

            if (entry->thtx.w * entry->thtx.h * format_Bpp(entry->thtx.format) >
                entry->thtx.size) {
                fprintf(stderr,
                    "%s:%s:%s: w*h*Bpp is greater than data size: %u*%u*%u > %u\n",
                    argv0, current_input, entry->name,
                    entry->thtx.w, entry->thtx.h,
                    format_Bpp(entry->thtx.format), entry->thtx.size);
                if (!option_force) abort();
            }

            entry->data_size = entry->thtx.w * entry->thtx.h * 4;

#ifdef HAVE_LIBPNG
            data = malloc(entry->thtx.size);
            file_read(f, data, entry->thtx.size);

            entry->data = fmt_to_rgba(
                data, entry->thtx.w * entry->thtx.h, entry->thtx.format);
            free(data);
#else
            entry->data = NULL;
#endif
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
anm_dump(FILE* stream, const anm_t* anm)
{
    unsigned int i;

    for (i = 0; i < anm->entry_count; ++i) {
        unsigned int j;
        const id_format_pair_t* formats = NULL;
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
        } else if (entry->header.version == 8) {
            formats = formats_v8;
            format_count = sizeof(formats_v8) / sizeof(formats_v8[0]);
        } else {
            fprintf(stderr,
                "%s:%s: could not find a format description for version %u\n",
                argv0, current_input, entry->header.version);
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
            unsigned int iter_instrs;
            anm_script_t* scr = &entry->scripts[j];

            fprintf(stream, "Script: %d\n", scr->id);

            for (iter_instrs = 0; iter_instrs < scr->instr_count; ++iter_instrs) {
                anm_instr_t* instr = scr->instrs[iter_instrs];

                fprintf(stream, "Instruction: %hu %hu %hu",
                    instr->time, instr->param_mask, instr->type);

                if (instr->length > sizeof(anm_instr_t)) {
                    const char* format = find_format(formats, instr->type);
                    value_t* values;

                    if (!format) {
                        fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, instr->type);
                        abort();
                    }

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
            memcpy(image->data + y * image->width * format_Bpp(image->format),
                row_pointers[y], image->width * format_Bpp(image->format));
        } else {
            unsigned char* converted_data =
                rgba_to_fmt((uint32_t*)row_pointers[y], image->width, image->format);
            memcpy(image->data + y * image->width * format_Bpp(image->format),
                converted_data, image->width * format_Bpp(image->format));
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
#ifdef HAVE_ZLIB
    png_set_compression_level(png_ptr, Z_BEST_SPEED);
#endif
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
util_total_entry_size(const anm_t* anm, const char* name,
    unsigned int* widthptr, unsigned int* heightptr)
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

static void
anm_replace(const anm_t* anm, const char* name, const char* filename)
{
    const format_t formats[] = { FORMAT_RGBA8888, FORMAT_BGRA8888, FORMAT_BGR565,
                                 FORMAT_BGRA4444, FORMAT_GRAY8 };
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
        unsigned char* converted_data = NULL;
        for (i = 0; i < anm->entry_count; ++i) {
            if (anm->entries[i].name == name &&
                anm->entries[i].header.format == formats[f] &&
                anm->entries[i].header.hasdata) {

                if (!converted_data)
                    converted_data = rgba_to_fmt((uint32_t*)image->data,
                        width * height, formats[f]);

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
    const format_t formats[] = { FORMAT_GRAY8, FORMAT_BGRA4444,
                                 FORMAT_BGR565, FORMAT_BGRA8888, FORMAT_RGBA8888 };
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
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
            argv0, name, strerror(errno));
        return;
    }
    png_write(stream, &image);
    fclose(stream);

    free(image.data);
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
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
            argv0, spec, strerror(errno));
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
            anm->entries =
                realloc(anm->entries, anm->entry_count * sizeof(entry_t));
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
            entry->sprites =
                realloc(entry->sprites, entry->sprite_count * sizeof(sprite_t));
            sprite = &entry->sprites[entry->sprite_count - 1];

            if (5 != sscanf(linep, "Sprite: %u %f*%f+%f+%f",
                         &sprite->id, &sprite->w, &sprite->h,
                         &sprite->x, &sprite->y)) {
                fprintf(stderr, "%s: Sprite parsing failed for %s\n",
                    argv0, linep);
                exit(1);
            }
        } else if (strncmp(linep, "Script: ", 8) == 0) {
            entry->script_count++;
            entry->scripts = realloc(
                entry->scripts, entry->script_count * sizeof(anm_script_t));
            script = &entry->scripts[entry->script_count - 1];
            script->offset = 0;
            script->instr_count = 0;
            script->instrs = NULL;
            if (1 != sscanf(linep, "Script: %d", &script->id)) {
                fprintf(stderr, "%s: Script parsing failed for %s\n",
                    argv0, linep);
                exit(1);
            }
        } else if (strncmp(linep, "Instruction: ", 13) == 0) {
            char* tmp = linep + 13;
            char* before;
            char* after = NULL;

            script->instr_count++;
            script->instrs = realloc(
                script->instrs, script->instr_count * sizeof(anm_instr_t*));
            instr = malloc(sizeof(anm_instr_t));

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

            script->instrs[script->instr_count - 1] = instr;
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
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
            argv0, filename, strerror(errno));
        exit(1);
    }

    for (i = 0; i < anm->entry_count; ++i) {
        long base = file_tell(stream);
        unsigned int namepad = 0;
        entry_t* entry = &anm->entries[i];
        char* padding;
        unsigned int j;
        unsigned int spriteoffset;

        namepad = (16 - strlen(entry->name) % 16);

        /* TODO: Make sure the correct header version is written,
         *       convert when needed. */

        file_seek(stream, base +
                          sizeof(anm_header_t) +
                          entry->sprite_count * sizeof(uint32_t) +
                          entry->script_count * ANM_SCRIPT_SIZE);

        entry->header.nameoffset = file_tell(stream) - base;
        file_write(stream, entry->name, strlen(entry->name));

        padding = calloc(1, namepad);
        file_write(stream, padding, namepad);
        free(padding);

        if (entry->name2 && entry->header.version == 0) {
            namepad = (16 - strlen(entry->name2) % 16);

            entry->header.y = file_tell(stream) - base;
            file_write(stream, entry->name2, strlen(entry->name2));

            padding = calloc(1, namepad);
            file_write(stream, padding, namepad);
            free(padding);
        }

        spriteoffset = file_tell(stream) - base;
        file_write(stream, entry->sprites, entry->sprite_count * sizeof(sprite_t));

        for (j = 0; j < entry->script_count; ++j) {
            unsigned int k;

            entry->scripts[j].offset = file_tell(stream) - base;
            for (k = 0; k < entry->scripts[j].instr_count; ++k) {
                if (entry->header.version == 0) {
                    anm_instr0_t instr;
                    instr.time = entry->scripts[j].instrs[k]->time;
                    instr.type = entry->scripts[j].instrs[k]->type;
                    instr.length = entry->scripts[j].instrs[k]->length;
                    file_write(stream, &instr, sizeof(instr));
                    if (instr.length) {
                        file_write(stream,
                            entry->scripts[j].instrs[k]->data,
                            instr.length);
                    }
                } else {
                    if (entry->scripts[j].instrs[k]->type == 0xffff) {
                        entry->scripts[j].instrs[k]->length = 0;
                        file_write(stream, entry->scripts[j].instrs[k], sizeof(anm_instr_t));
                    } else {
                        entry->scripts[j].instrs[k]->length += sizeof(anm_instr_t);
                        file_write(stream, entry->scripts[j].instrs[k], entry->scripts[j].instrs[k]->length);
                    }
                }
            }

            if (entry->header.version == 0) {
                anm_instr0_t sentinel = { 0, 0, 0 };
                file_write(stream, &sentinel, sizeof(anm_instr0_t));
            } else {
                anm_instr_t sentinel = { 0xffff, 0, 0, 0 };
                file_write(stream, &sentinel, sizeof(anm_instr_t));
            }
        }

        if (entry->header.hasdata) {
            entry->header.thtxoffset = file_tell(stream) - base;

            fputs("THTX", stream);
            file_write(stream, &entry->thtx, sizeof(thtx_header_t));

            file_write(stream, entry->data, entry->data_size);
        }

        if (i == anm->entry_count - 1)
            entry->header.nextoffset = 0;
        else
            entry->header.nextoffset = file_tell(stream) - base;

        entry->header.sprites = entry->sprite_count;
        entry->header.scripts = entry->script_count;

        file_seek(stream, base);

        if (entry->header.version >= 7) {
            convert_header_to_11(&entry->header);
            file_write(stream, &entry->header, sizeof(anm_header_t));
            convert_header_to_old(&entry->header);
        } else {
            file_write(stream, &entry->header, sizeof(anm_header_t));
        }

        for (j = 0; j < entry->sprite_count; ++j) {
            uint32_t ofs = spriteoffset + j * sizeof(sprite_t);
            file_write(stream, &ofs, sizeof(uint32_t));
        }

        for (j = 0; j < entry->script_count; ++j) {
            file_write(stream, &entry->scripts[j], ANM_SCRIPT_SIZE);
        }

        file_seek(stream, base + entry->header.nextoffset);
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
                            free(anm->entries[i].scripts[j].instrs[k]);
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
    printf("Usage: %s COMMAND\n"
           "COMMAND can be:\n"
           "  l[OPTION...] ARCHIVE            list archive\n", argv0);
#ifdef HAVE_LIBPNG
    printf("  x[OPTION...] ARCHIVE [FILE...]  extract entries\n"
           "  r[OPTION...] ARCHIVE NAME FILE  replace entry in archive\n"
           "  c[OPTION...] ARCHIVE SPEC       create archive\n");
#endif
    printf("  h                               display this help and exit\n"
           "  V                               display version information and exit\n"
           "OPTION can be:\n"
           "  f  ignore errors when possible\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n");
}

int
main(int argc, char* argv[])
{
    const char commands[] = "l"
#ifdef HAVE_LIBPNG
                            "xrc"
#endif
                            "hV";
    char options[] = "f";
    int command = 0;
    anm_t* anm;
#ifdef HAVE_LIBPNG
    FILE* anmfp;
    unsigned int offset = 0;
    int i;
#endif

    command = parse_args(argc, argv, print_usage, commands, options, NULL);

    if (!command)
        exit(1);

    if (!strchr(options, 'f'))
        option_force = 1;

    switch (command) {
    case 'h':
        print_usage();
        exit(0);
    case 'V':
        util_print_version();
        exit(0);
    case 'l':
        if (argc != 3) {
            print_usage();
            exit(1);
        }
        current_input = argv[2];
        anm = anm_read_file(argv[2]);
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
        anm = anm_read_file(argv[2]);

        if (argc == 3) {
            /* Extract all files. */
            for (i = 0; i < (int)anm->name_count; ++i) {
                current_output = anm->names[i];
                puts(anm->names[i]);
                anm_extract(anm, anm->names[i]);
            }
        } else {
            /* Extract all listed files. */
            for (i = 3; i < argc; ++i) {
                unsigned int j;
                for (j = 0; j < anm->name_count; ++j) {
                    if (strcmp(argv[i], anm->names[j]) == 0) {
                        current_output = anm->names[i];
                        puts(anm->names[i]);
                        anm_extract(anm, anm->names[j]);
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
        current_output = argv[2];
        current_input = argv[4];
        anm = anm_read_file(argv[2]);

        for (i = 0; i < (int)anm->name_count; ++i) {
            if (strcmp(argv[3], anm->names[i]) == 0) {
                anm_replace(anm, anm->names[i], argv[4]);
                goto replace_done;
            }
        }

        fprintf(stderr, "%s:%s: %s not found in archive\n",
            argv0, current_input, argv[3]);

replace_done:

        anmfp = fopen(argv[2], "rb+");
        if (!anmfp) {
            fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                argv0, current_input, strerror(errno));
            exit(1);
        }

        for (i = 0; i < (int)anm->entry_count; ++i) {
            unsigned int nextoffset = anm->entries[i].header.nextoffset;
            if (strcmp(argv[3], anm->entries[i].name) == 0 && anm->entries[i].header.hasdata) {
                if (!file_seek(anmfp,
                    offset + anm->entries[i].header.thtxoffset + 4 + sizeof(thtx_header_t)))
                    exit(1);
                if (!file_write(anmfp, anm->entries[i].data, anm->entries[i].thtx.size))
                    exit(1);
            }
            offset += nextoffset;
        }

        fclose(anmfp);

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

        current_output = argv[2];
        anm_write(anm, argv[2]);

        anm_free(anm);
        exit(0);
#endif
    default:
        abort();
    }
}
