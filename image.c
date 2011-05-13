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
#include <inttypes.h>
#ifdef HAVE_LIBPNG
#include <png.h>
#endif
#include <stdlib.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
#include "image.h"
#include "program.h"
#include "thanm.h"

unsigned int
format_Bpp(
    format_t format)
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
unsigned char*
rgba_to_fmt(
    const uint32_t* data,
    unsigned int pixels,
    format_t format)
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

char*
fmt_to_rgba(
    const char* data,
    unsigned int pixels,
    format_t format)
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

image_t*
png_read(
    FILE* stream,
    format_t format)
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

void
png_write(
    FILE* stream,
    image_t* image)
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
#endif
