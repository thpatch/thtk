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
static unsigned int option_verbose;
static unsigned int option_filemap;
filemap_t filemap;

static unsigned int
format_Bpp(format_t format)
{
    switch (format) {
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

static char*
rgba_to_fmt(const uint32_t* data, unsigned int pixels, format_t format)
{
    unsigned int i;
    char* out = NULL;

    if (format == FORMAT_GRAY8) {
        out = malloc(pixels);
        for (i = 0; i < pixels; ++i) {
            out[i] = data[i] & 0xff;
        }
    } else if (format == FORMAT_BGRA8888) {
        const char* data8 = (const char*)data;
        out = malloc(sizeof(uint32_t) * pixels);
        memcpy(out, data, pixels * sizeof(uint32_t));
        for (i = 0; i < pixels; ++i) {
            out[i * sizeof(uint32_t) + 0] = data8[i * sizeof(uint32_t) + 2];
            out[i * sizeof(uint32_t) + 2] = data8[i * sizeof(uint32_t) + 0];
        }
    } else if (format == FORMAT_BGRA4444) {
        out = malloc(sizeof(uint16_t) * pixels);
        for (i = 0; i < pixels; ++i) {
            out[i * sizeof(uint16_t) + 0] = ((data[i] &   0xf00000) >> 20)
                                          | ((data[i] &     0xf000) >> 8);
            out[i * sizeof(uint16_t) + 1] = ((data[i] &       0xf0) >> 4)
                                          | ((data[i] & 0xf0000000) >> 24);
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
    { 80, "i" }
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
    { 89, "" }
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
    { 112, "i" }
};

/* The order and sizes of fields changed for TH11. */
static void
convert_header(anm_header_t* header)
{
    anm_header11_t th11 = *(anm_header11_t*)header;
    header->sprites = th11.sprites;
    header->animations = th11.animations;
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
    header->unknown2 = th11.unknown2;
    header->nextoffset = th11.nextoffset;
    header->zero3 = 0;
}

static anm_t*
anm_read_file(const char* filename)
{
    anm_t* anm;
    uint32_t offset = 0;
    long filesize;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "%s: couldn't open %s: %s\n", argv0, filename, strerror(errno));
        exit(1);
    }

    filesize = util_fsize(f);

    anm = malloc(sizeof(anm_t));
    anm->name_count = 0;
    anm->names = NULL;
    anm->entry_count = 0;
    anm->entries = NULL;

    if (option_filemap) {
        filemap.data = malloc(filesize);
        memset(filemap.data, ' ', filesize);
        filemap.pos = 0;
    }

    for (;;) {
        entry_t* entry;
        char name[256];
        unsigned int i;

        ++anm->entry_count;
        anm->entries = realloc(anm->entries, sizeof(entry_t) * anm->entry_count);

        entry = &anm->entries[anm->entry_count - 1];
        memset(&entry->thtx, 0, sizeof(thtx_header_t));
        entry->name = NULL;
        entry->sprite_count = 0;
        entry->sprites = NULL;
        entry->animation_count = 0;
        entry->animations = NULL;
        entry->data_size = 0;
        entry->data = NULL;

        util_seek(f, offset, NULL);
        util_read(f, &entry->header, sizeof(anm_header_t), 'H', NULL);

        /* XXX: This is not a particularly good way of detecting this. */
        if (entry->header.zero1 != 0)
            convert_header(&entry->header);

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
        if (entry->header.unknown2 == 0 &&
            entry->header.thtxoffset != 0) {
            fprintf(stderr, "%s:%s:%u: unknown2 and thtxoffset do not match: %u, %u\n",
                argv0, current_input, anm->entry_count, entry->header.unknown2, entry->header.thtxoffset);
            if (!option_force) abort();
        }
        if (entry->header.unknown2 != 0 &&
            entry->header.unknown2 != 1) {
            fprintf(stderr, "%s:%s:%u: unknown value for unknown2: %u\n",
                argv0, current_input, anm->entry_count, entry->header.unknown2);
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
        util_seek(f, offset + entry->header.nameoffset, NULL);
        fgets(name, 256, f);
        if (option_filemap) {
            memset(filemap.data + filemap.pos, 'N', strlen(name) + 1);
            filemap.pos += strlen(name) + 1;
        }

        for (i = 0; i < anm->name_count; ++i) {
            if (strcmp(name, anm->names[i]) == 0) {
                entry->name = anm->names[i];
            }
        }

        if (!entry->name) {
            ++anm->name_count;
            anm->names = realloc(anm->names, sizeof(char*) * anm->name_count);
            anm->names[anm->name_count - 1] = strdup(name);
            entry->name = anm->names[anm->name_count - 1];
        }

        util_seek(f, offset + sizeof(anm_header_t), NULL);

        /* Parse any sprites in the entry. */
        if (entry->header.sprites) {
            uint32_t* offsets = NULL;
            int sequential = 1;

            offsets = malloc(sizeof(uint32_t) * entry->header.sprites);
            entry->sprites = malloc(sizeof(sprite_t) * entry->header.sprites);
            entry->sprite_count = entry->header.sprites;
            util_read(f, offsets, sizeof(uint32_t) * entry->header.sprites, 'o', NULL);

            /* Check that the sprites are stored packed. */
            for (i = 1; i < entry->header.sprites; ++i) {
                if (offsets[i] - offsets[i - 1] != sizeof(sprite_t)) {
                    /* This should never happen. */
                    sequential = 0;
                    break;
                }
            }

            if (sequential) {
                util_seek(f, offset + offsets[0], NULL);
                util_read(f, entry->sprites, sizeof(sprite_t) * entry->header.sprites, 'R', NULL);
            } else {
                for (i = 0; i < entry->header.sprites; ++i) {
                    util_seek(f, offset + offsets[i], NULL);
                    util_read(f, &entry->sprites[i], sizeof(sprite_t), 'R', NULL);
                }
            }

            free(offsets);
        }

        /* Read in all the animation scripts. */
        /* XXX: TH06 is not yet supported. */
        if (entry->header.animations && entry->header.version != 0) {
            animation_offset_t* offsets;

            util_seek(f, offset + sizeof(anm_header_t) + sizeof(uint32_t) * entry->header.sprites, NULL);

            offsets = malloc(sizeof(animation_offset_t) * entry->header.animations);
            util_read(f, offsets, sizeof(animation_offset_t) * entry->header.animations, 'O', NULL);

            entry->animations = NULL;
            entry->animation_count = 0;

            for (i = 0; i < entry->header.animations; ++i) {
                long limit = 0;

                util_seek(f, offset + offsets[i].offset, NULL);

                if (i < entry->header.animations - 1)
                    limit = offset + offsets[i + 1].offset;
                else if (entry->header.thtxoffset)
                    limit = offset + entry->header.thtxoffset;
                else
                    limit = filesize;

                for (;;) {
                    animation_t anim;

                    if (ftell(f) + 8 > limit) {
                        fprintf(stderr, "%s:%s:%s:%d: would read past limit\n",
                            argv0, current_input, entry->name, offsets[i].id);
                        break;
                    }

                    util_read(f, &anim, 8, 'a', NULL);

                    /* End of this animation. */
                    if (anim.type == 0xffff)
                        break;

                    /* This shouldn't happen. */
                    if (anim.length == 0) {
                        fprintf(stderr, "%s:%s:%s:%d: animation length is zero: %u %u %u\n",
                            argv0, current_input, entry->name, offsets[i].id, anim.type, anim.length, anim.time);
                        if (!option_force) abort();
                        break;
                    }

                    if (anim.length % 4 != 0) {
                        fprintf(stderr, "%s:%s:%s:%d: length is not a multiple of four: %u\n",
                            argv0, current_input, entry->name, offsets[i].id, anim.length);
                        if (!option_force) abort();
                        break;
                    }

                    ++entry->animation_count;
                    entry->animations = realloc(entry->animations, sizeof(animation_t) * entry->animation_count);

                    entry->animations[entry->animation_count - 1].time = anim.time;
                    entry->animations[entry->animation_count - 1].type = anim.type;
                    entry->animations[entry->animation_count - 1].length = anim.length;
                    entry->animations[entry->animation_count - 1].id = offsets[i].id;
                    if (anim.length > 8) {
                        entry->animations[entry->animation_count - 1].data = malloc(anim.length - 8);
                        util_read(f, entry->animations[entry->animation_count - 1].data, anim.length - 8, 'A', NULL);
                    } else {
                        entry->animations[entry->animation_count - 1].data = NULL;
                    }
                }
            }

            free(offsets);
        }

        /* TH06 doesn't store entry data. */
        if (entry->header.thtxoffset) {
            char* data = NULL;
            char magic[5] = { 0 };

            util_seek(f, offset + entry->header.thtxoffset, NULL);

            util_read(f, magic, 4, 't', NULL);
            if (strcmp(magic, "THTX") != 0) {
                fprintf(stderr, "%s:%s:%s: unknown thtx magic: %s\n",
                    argv0, current_input, entry->name, magic);
                if (!option_force) abort();
            }

            util_read(f, &entry->thtx, sizeof(thtx_header_t), 'T', NULL);
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

            /* XXX: There are three known entries which don't match.
             *      What they have in common is that their sizes are odd,
             *      and that the extra data makes up two rows.
             *
             *      09/pl02.anm  : data/pl/pl02/eff09sk1.png
             *      09/pl02b.anm : data/pl/pl02b/eff09sk1.png
             *      95/enm11.anm : data/cdbg/cdbg11.png
             *
             *      The extra lines are neither written out nor read in. */
            /* if (entry->thtx.w * entry->thtx.h * format_Bpp(entry->thtx.format) != entry->thtx.size)
                fprintf(stderr, "%s: %u != %u\n", entry->name, entry->thtx.w * entry->thtx.h * format_Bpp(entry->thtx.format), entry->thtx.size); */

            data = malloc(entry->thtx.size);
            util_read(f, data, entry->thtx.w * entry->thtx.h * format_Bpp(entry->thtx.format), 'D', NULL);

            entry->data_size = entry->thtx.w * entry->thtx.h * 4;
            entry->data = fmt_to_rgba(data, entry->thtx.w * entry->thtx.h, entry->thtx.format);

            free(data);
        }

        if (!entry->header.nextoffset)
            break;

        offset += entry->header.nextoffset;
    }

    fclose(f);

    if (option_filemap) {
        f = fopen("filemap.dat", "wb");
        if (!f) {
            fprintf(stderr, "%s: couldn't open filemap.dat for writing: %s\n", argv0, strerror(errno));
        } else {
            if (fwrite(filemap.data, filesize, 1, f) != 1)
                fprintf(stderr, "%s: couldn't write to filemap.dat: %s\n", argv0, strerror(errno));
            free(filemap.data);
            fclose(f);
        }
    }

    qsort(anm->names, anm->name_count, sizeof(char*), util_strpcmp);

    return anm;
}

static void
anm_list(const anm_t* anm)
{
    unsigned int i;

    if (!option_verbose) {
        for (i = 0; i < anm->name_count; ++i)
            puts(anm->names[i]);
    } else {
        for (i = 0; i < anm->entry_count; ++i) {
            unsigned int j;
            printf("%s %ux%u+%u+%u %ux%u %u %u %u %u\n",
                anm->entries[i].name,
                anm->entries[i].header.w, anm->entries[i].header.h, anm->entries[i].header.x, anm->entries[i].header.y,
                anm->entries[i].thtx.w, anm->entries[i].thtx.h,
                anm->entries[i].header.version,
                anm->entries[i].header.format,
                anm->entries[i].header.unknown1,
                anm->entries[i].header.unknown2);

            if (option_verbose > 1) {
                for (j = 0; j < anm->entries[i].sprite_count; ++j) {
                    printf("  Sprite #%u: %.fx%.f+%.f+%.f\n",
                        anm->entries[i].sprites[j].id,
                        anm->entries[i].sprites[j].w,
                        anm->entries[i].sprites[j].h,
                        anm->entries[i].sprites[j].x,
                        anm->entries[i].sprites[j].y);
                }
            }

            if (option_verbose > 2 && anm->entries[i].animation_count) {
                const opcode_fmt_t* formats = NULL;
                unsigned int format_count = 0;
                if (anm->entries[i].header.version == 2) {
                    formats = formats_v2;
                    format_count = sizeof(formats_v2) / sizeof(formats_v2[0]);
                } else if (anm->entries[i].header.version == 3) {
                    formats = formats_v3;
                    format_count = sizeof(formats_v3) / sizeof(formats_v3[0]);
                } else if (anm->entries[i].header.version == 4 || anm->entries[i].header.version == 7) {
                    formats = formats_v4p;
                    format_count = sizeof(formats_v4p) / sizeof(formats_v4p[0]);
                } else {
                    fprintf(stderr, "%s:%s:%s: unknown version: %u\n", argv0, current_input, anm->entries[i].name, anm->entries[i].header.version);
                    continue;
                }
                for (j = 0; j < anm->entries[i].animation_count; ++j) {
                    unsigned int k;
                    unsigned int done = 0;

                    if (j == 0 || anm->entries[i].animations[j].id != anm->entries[i].animations[j - 1].id)
                        printf("  Animation #%i:\n", anm->entries[i].animations[j].id);

                    printf("    @%-5u %-5u", anm->entries[i].animations[j].time, anm->entries[i].animations[j].type);

                    for (k = 0; k < format_count; ++k) {
                        if (formats[k].type == anm->entries[i].animations[j].type && formats[k].format) {
                            unsigned int m;
                            if ((int)strlen(formats[k].format) * 4 != anm->entries[i].animations[j].length - 8) {
                                fprintf(stderr, "%s:%s:%d: animation format length does not match data length: %lu != %u\n",
                                    argv0,
                                    current_input,
                                    anm->entries[i].animations[j].id,
                                    (long unsigned int)strlen(formats[k].format) * 4,
                                    anm->entries[i].animations[j].length - 8);
                                break;
                            }
                            for (m = 0; m < strlen(formats[k].format); ++m) {
                                int32_t int1;
                                switch (formats[k].format[m]) {
                                case 'i':
                                    memcpy(&int1, anm->entries[i].animations[j].data + m * sizeof(int32_t), sizeof(int32_t));
                                    printf(" %i", int1);
                                    break;
                                case 'f':
                                    printf(" %sf", util_printfloat(anm->entries[i].animations[j].data + m * sizeof(int32_t)));
                                    break;
                                default:
                                    fprintf(stderr, "%s: unknown animation format specifier: %c\n", argv0, formats[k].format[m]);
                                    if (!option_force) abort();
                                    break;
                                }
                            }
                            printf("\n");
                            done = 1;
                            break;
                        }
                    }
                    if (!done) {
                        uint32_t* array = (uint32_t*)anm->entries[i].animations[j].data;
                        printf("  ? @%-5u %-5u", anm->entries[i].animations[j].time, anm->entries[i].animations[j].type);
                        for (k = 0; (int)k < (anm->entries[i].animations[j].length - 8) >> 2; ++k) {
                            printf(" 0x%08x", *(array + k));
                        }
                        printf("\n");
                    }
                }
            }
        }
    }
}

static void
util_total_entry_size(const anm_t* anm, const char* name, unsigned int* widthptr, unsigned int* heightptr)
{
    unsigned int i;
    unsigned int width = 0;
    unsigned int height = 0;

    for (i = 0; i < anm->entry_count; ++i) {
        if (anm->entries[i].name == name) {
            if (!anm->entries[i].header.thtxoffset)
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
static void
anm_replace(const anm_t* anm, const char* name, const char* filename)
{
    unsigned int i;
    unsigned int y;
    unsigned int width = 0, height = 0;
    char* image_data = NULL;

    unsigned char header[8] = { 0 };
    FILE* pngfd;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytepp row_pointers;

    pngfd = fopen(filename, "rb");
    if (!pngfd) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n", argv0, filename, strerror(errno));
        exit(1);
    }

    if (fread(header, 8, 1, pngfd) != 1) {
        fprintf(stderr, "%s: couldn't read from %s: %s\n", argv0, filename, strerror(errno));
        exit(1);
    }

    if (png_sig_cmp(header, 0, 8) != 0) {
        fprintf(stderr, "%s: invalid PNG signature for %s\n", argv0, filename);
        exit(1);
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);

    png_init_io(png_ptr, pngfd);
    png_set_sig_bytes(png_ptr, 8);

    png_read_png(png_ptr, info_ptr, 0, NULL);

    row_pointers = png_get_rows(png_ptr, info_ptr);
    fclose(pngfd);

    util_total_entry_size(anm, name, &width, &height);

    if (width != info_ptr->width || height != info_ptr->height) {
        fprintf(stderr, "%s:%s:%s: wrong image dimensions for %s: %u, %u instead of %u, %u\n", argv0, current_input, name, filename, (unsigned int)info_ptr->width, (unsigned int)info_ptr->height, width, height);
        exit(1);
    }

    if (info_ptr->color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
        fprintf(stderr, "%s: %s must be RGBA\n", argv0, filename);
        exit(1);
    }

    image_data = malloc(width * height * 4);
    for (y = 0; y < info_ptr->height; ++y)
        memcpy(image_data + y * info_ptr->width * 4, row_pointers[y], info_ptr->width * 4);

    for (i = 0; i < anm->entry_count; ++i) {
        /* XXX: Try to avoid doing the conversion for every part. */
        if (anm->entries[i].name == name && anm->entries[i].header.thtxoffset) {
            char* converted_data = rgba_to_fmt((uint32_t*)image_data, width * height, anm->entries[i].header.format);

            for (y = anm->entries[i].header.y; y < anm->entries[i].header.y + anm->entries[i].thtx.h; ++y) {
                memcpy(anm->entries[i].data + (y - anm->entries[i].header.y) * anm->entries[i].thtx.w * format_Bpp(anm->entries[i].header.format),
                       converted_data + y * width * format_Bpp(anm->entries[i].header.format) + anm->entries[i].header.x * format_Bpp(anm->entries[i].header.format),
                       anm->entries[i].thtx.w * format_Bpp(anm->entries[i].header.format));
            }

            free(converted_data);
        }
    }

    free(image_data);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
}

static void
anm_extract(const anm_t* anm, const char* name)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_byte** imagep;
    unsigned int i;
    unsigned int y;
    char* data;
    FILE* fp;
    unsigned int width = 0;
    unsigned int height = 0;

    util_total_entry_size(anm, name, &width, &height);

    if (width == 0 || height == 0) {
        fprintf(stderr, "%s: calculated width or height is zero\n", argv0);
        if (!option_force) abort();
        return;
    }

    util_makepath(name);

    fp = fopen(name, "wb");
    if (!fp) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n", argv0, name, strerror(errno));
        return;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "%s: png_create_write_struct failed\n", argv0);
        abort();
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "%s: png_create_info_struct failed\n", argv0);
        abort();
    }
    png_init_io(png_ptr, fp);

    data = malloc(width * height * 4);
    memset(data, 0xff, width * height * 4);

    for (i = 0; i < anm->entry_count; ++i) {
        if (anm->entries[i].name == name) {
            for (y = anm->entries[i].header.y; y < anm->entries[i].header.y + anm->entries[i].thtx.h; ++y) {
                memcpy(data + y * width * 4 + anm->entries[i].header.x * 4,
                       anm->entries[i].data + (y - anm->entries[i].header.y) * anm->entries[i].thtx.w * 4,
                       anm->entries[i].thtx.w * 4);
            }
        }
    }

    png_set_IHDR(png_ptr, info_ptr,
        width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    imagep = malloc(sizeof(png_byte*) * height);
    for (y = 0; y < height; ++y)
        imagep[y] = (png_byte*)(data + y * width * 4);

    png_write_image(png_ptr, imagep);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    free(imagep);
    free(data);

    fclose(fp);
}
#endif

static void
anm_free(anm_t* anm)
{
    unsigned int i, j;

    if (!anm)
        return;

    if (anm->entries) {
        for (i = 0; i < anm->entry_count; ++i) {
            if (anm->entries[i].sprites)
                free(anm->entries[i].sprites);
            if (anm->entries[i].animations) {
                for (j = 0; j < anm->entries[i].animation_count; ++j) {
                    if (anm->entries[i].animations[j].data)
                        free(anm->entries[i].animations[j].data);
                }
                free(anm->entries[i].animations);
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
print_usage()
{
    printf("Usage: %s [OPTION]... MODE ARCHIVE [FILE]...\n"
           "MODE can be:\n"
           "  -l    list archive\n"
#ifdef HAVE_LIBPNG
           "  -x    extract archive\n"
           "  -r    replace entries in archive\n"
#endif
           "OPTION can be:\n"
           "  -v N  verbosity level\n"
           "  -f    ignore errors when possible\n"
           "  -d    write filemap.dat\n"
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
        } else if (strcmp(argv[i], "-v") == 0) {
            ++i;
            if (i >= argc) {
                print_usage();
                exit(1);
            }
            option_verbose = strtol(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "-d") == 0) {
            option_filemap = 1;
        } else if (strcmp(argv[i], "-f") == 0) {
            option_force = 1;
#ifdef HAVE_LIBPNG
        } else if (strcmp(argv[i], "-x") == 0) {
            mode = MODE_EXTRACT;
        } else if (strcmp(argv[i], "-r") == 0) {
            mode = MODE_REPLACE;
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
        anm_list(anm);
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
            fprintf(stderr, "%s:%s: couldn't open for writing: %s\n", argv0, current_input, strerror(errno));
            abort();
        }

        for (i = 0; i < (int)anm->entry_count; ++i) {
            unsigned int nextoffset = anm->entries[i].header.nextoffset;
            if (strcmp(argv[filestart + 1], anm->entries[i].name) == 0 && anm->entries[i].header.thtxoffset) {
                fseek(anmfp, offset + anm->entries[i].header.thtxoffset + 4 + sizeof(thtx_header_t), SEEK_SET);
                fwrite(anm->entries[i].data, anm->entries[i].thtx.w * anm->entries[i].thtx.h * format_Bpp(anm->entries[i].thtx.format), 1, anmfp);
            }
            offset += nextoffset;
        }

        fclose(anmfp);

        anm_free(anm);
#endif
    } else {
        fprintf(stderr, "%s: no MODE specified\n", argv0);
        print_usage();
        exit(1);
    }

    exit(0);
}
