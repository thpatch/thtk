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
#include <string.h>
#include <inttypes.h>
#ifdef HAVE_LIBPNG
#include <png.h>
#endif
#include <stdlib.h>
#include <errno.h>
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
    case FORMAT_ARGB4444:
    case FORMAT_RGB565:
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
format_from_rgba(
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
    } else if (format == FORMAT_ARGB4444) {
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
    } else if (format == FORMAT_RGB565) {
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

unsigned char*
format_to_rgba(
    const unsigned char* data,
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
        unsigned char* out8 = (unsigned char*)out;
        for (i = 0; i < pixels; ++i) {
            out8[i * sizeof(uint32_t) + 0] = data[i * sizeof(uint32_t) + 2];
            out8[i * sizeof(uint32_t) + 1] = data[i * sizeof(uint32_t) + 1];
            out8[i * sizeof(uint32_t) + 2] = data[i * sizeof(uint32_t) + 0];
            out8[i * sizeof(uint32_t) + 3] = data[i * sizeof(uint32_t) + 3];
        }
    } else if (format == FORMAT_ARGB4444) {
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
    } else if (format == FORMAT_RGB565) {
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

    return (unsigned char*)out;
}

image_t*
png_read(
    const char* filename)
{
    FILE* stream;
    image_t* image;
    png_image png = {
        .version = PNG_IMAGE_VERSION,
        .opaque = NULL
    };

    stream = fopen(filename, "rb");
    if(!stream) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
            argv0, filename, strerror(errno));
        exit(1);
    }

#define ERR_WRAP(x) \
    x; \
    if(PNG_IMAGE_FAILED(png)) { \
        fprintf(stderr, "%s: error reading %s: %s\n", \
            argv0, filename, png.message); \
        exit(1); \
    }

    ERR_WRAP(png_image_begin_read_from_stdio(&png, stream));
    png.format = PNG_FORMAT_RGBA;

    image = malloc(sizeof(image_t));
    image->width = png.width;
    image->height = png.height;
    image->format = FORMAT_RGBA8888;
    image->data = malloc(PNG_IMAGE_SIZE(png));

    ERR_WRAP(png_image_finish_read(&png, 0, image->data, 0, NULL));
    fclose(stream);

#undef ERR_WRAP

    return image;
}

void
png_write(
    const char* filename,
    image_t* image)
{
    FILE* stream;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytepp imagep;

    int bit_depth;
    int color_type;
    int bytes_per_pixel;

    stream = fopen(filename, "wb");
    if(!stream) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
            argv0, filename, strerror(errno));
        return;
    }

    if (image->format == FORMAT_GRAY8) {
        bit_depth = 8;
        color_type = PNG_COLOR_TYPE_GRAY;
        bytes_per_pixel = 1;
    } else /* if (image->format == FORMAT_RGBA8888) */ {
        bit_depth = 8;
        color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        bytes_per_pixel = 4;
    } /* else { abort(); } */

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_set_compression_level(png_ptr, 1);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, stream);

    png_set_IHDR(png_ptr, info_ptr,
        image->width, image->height, bit_depth, color_type,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    imagep = malloc(image->height * sizeof(png_byte*));
    for (unsigned int y = 0; y < image->height; ++y)
        imagep[y] = (png_byte*)(image->data + y * image->width * bytes_per_pixel);

    png_write_image(png_ptr, imagep);
    free(imagep);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(stream);
}
#endif
