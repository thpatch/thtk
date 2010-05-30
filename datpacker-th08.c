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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "datpacker.h"
#include "thcrypt.h"
#include "thlzss.h"
#include "util.h"
#include "thdat.h"

typedef struct {
    unsigned char type;
    unsigned char key;
    unsigned char step;
    unsigned int block;
    unsigned int limit;
} crypt_params;

/* Indices into th??_crypt_params. */
#define TYPE_ETC 0
#define TYPE_ANM 1
#define TYPE_ECL 2
#define TYPE_JPG 3
#define TYPE_TXT 4
#define TYPE_WAV 5
#define TYPE_MSG 6

static const crypt_params
th08_crypt_params[] = {
    { 0x2d, 0x35, 0x97, 0x80,   0x2800 }, /* .rpy, .end, .mid, .dat, .sht, .png, .std, .ver, .fmt */
    { 0x41, 0xc1, 0x51, 0x1400, 0x2000 }, /* .anm */
    { 0x45, 0xab, 0xcd, 0x200,  0x1000 }, /* .ecl */
    { 0x4a, 0x03, 0x19, 0x1400, 0x7800 }, /* .jpg */
    { 0x54, 0x51, 0xe9, 0x40,   0x3000 }, /* .txt */
    { 0x57, 0x12, 0x34, 0x400,  0x2800 }, /* .wav */
    { 0x2d, 0x35, 0x97, 0x80,   0x2800 }, /* TYPE_MSG, same as TYPE_MSG here. */
};

static const crypt_params
th09_crypt_params[] = {
    { 0x2a, 0x99, 0x37, 0x400, 0x1000 }, /* .rpy, .end, .mid, .sht, .png, .std, .ver, .fmt */
    { 0x41, 0xc1, 0x51, 0x400, 0x400  }, /* .anm */
    { 0x45, 0xab, 0xcd, 0x200, 0x1000 }, /* .ecl */
    { 0x4a, 0x03, 0x19, 0x400, 0x400  }, /* .jpg */
    { 0x54, 0x51, 0xe9, 0x40,  0x3000 }, /* .txt */
    { 0x57, 0x12, 0x34, 0x400, 0x400  }, /* .wav */
    { 0x4d, 0x1b, 0x37, 0x40,  0x2800 }, /* .msg */
};

static void
tolowerstr(char* str)
{
    while (*str) {
        *str = tolower(*str);
        ++str;
    }
}

static archive_t*
th08_create(FILE* stream, unsigned int version, unsigned int count)
{
    return archive_create(stream, version, 16, count);
}

static unsigned char*
th08_read_file(entry_t* entry, FILE* stream)
{
    unsigned char* data = malloc(entry->size + 4);

    if (fread(data + 4, entry->size, 1, stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't read: %s", strerror(errno));
        free(data);
        return NULL;
    }

    return data;
}

static int
th08_encrypt(archive_t* archive, entry_t* entry, unsigned char* data)
{
    const crypt_params* current_crypt_params = archive->version == 8 ? th08_crypt_params : th09_crypt_params;
    unsigned int type;
    char ext[5];

    /* TODO: Move all of this to one or two functions. */
    if (strlen(entry->name) < 4) {
        strcpy(ext, "");
    } else {
        strcpy(ext, entry->name + strlen(entry->name) - 4);
    }
    tolowerstr(ext);

    if (strcmp(ext, ".anm") == 0) {
        type = TYPE_ANM;
    } else if (strcmp(ext, ".ecl") == 0) {
        type = TYPE_ECL;
    } else if (strcmp(ext, ".jpg") == 0) {
        type = TYPE_JPG;
    } else if (strcmp(ext, ".msg") == 0) {
        type = TYPE_MSG;
    } else if (strcmp(ext, ".txt") == 0) {
        type = TYPE_TXT;
    } else if (strcmp(ext, ".wav") == 0) {
        type = TYPE_WAV;
    } else {
        type = TYPE_ETC;
    }

    data[0] = 'e';
    data[1] = 'd';
    data[2] = 'z';
    data[3] = current_crypt_params[type].type;

    if (th_encrypt(data + 4,
                   entry->size,
                   current_crypt_params[type].key,
                   current_crypt_params[type].step,
                   current_crypt_params[type].block,
                   current_crypt_params[type].limit) == -1) {
        free(data);
        return -1;
    }

    return 0;
}

static unsigned char*
th08_lzss(entry_t* entry, unsigned char* data)
{
    unsigned char* zdata;
    zdata = th_lz_mem(data, entry->size + 4, &entry->zsize);
    free(data);
    return zdata;
}

static int
th08_write(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned char* data;

    data = th08_read_file(entry, stream);
    if (!data)
        return -1;

    if (th08_encrypt(archive, entry, data) == -1)
        return -1;

    data = th08_lzss(entry, data);

    /* XXX: There doesn't seem to be an option for non-compressed data here. */

    return thdat_write_entry(archive, entry, data);
}

static int
th08_close(archive_t* archive)
{
    unsigned char* buffer;
    unsigned char* buffer_ptr;
    unsigned int i;
    unsigned char* zbuffer;
    uint32_t header[4];
    unsigned int list_size = 0;
    unsigned int list_zsize = 0;
    const uint32_t zero = 0;

    thdat_sort(archive);

    for (i = 0; i < archive->count; ++i)
        list_size += strlen(archive->entries[i].name) + 1 + (sizeof(uint32_t) * 3);

    /* XXX: I'm adding some padding here to satisfy pbgzmlt; th08 has 1248 zero
     * bytes and th09 1200 bytes.  The games work fine without it. */
    list_size += 4;
    buffer = malloc(list_size);
    memset(buffer, 0, list_size);

    buffer_ptr = buffer;
    for (i = 0; i < archive->count; ++i) {
        entry_t* entry = &archive->entries[i];
        buffer_ptr = mempcpy(buffer_ptr, entry->name, strlen(entry->name) + 1);
        buffer_ptr = mempcpy(buffer_ptr, &entry->offset, sizeof(uint32_t));
        entry->size += 4; /* XXX: Only OK if I don't use it later on. */
        buffer_ptr = mempcpy(buffer_ptr, &entry->size, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &zero, sizeof(uint32_t));
    }

    buffer_ptr = mempcpy(buffer_ptr, &zero, sizeof(uint32_t));

    zbuffer = th_lz_mem(buffer, list_size, &list_zsize);
    free(buffer);

    if (th_encrypt(zbuffer, list_zsize, 0x3e, 0x9b, 0x80, 0x400) == -1) {
        free(zbuffer);
        return -1;
    }

    if (fwrite(zbuffer, list_zsize, 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        free(zbuffer);
        return -1;
    }
    free(zbuffer);

    header[0] = 0x5a474250; /* ZGBP */
    header[1] = archive->count + 123456;
    header[2] = archive->offset + 345678;
    header[3] = list_size + 567891;

    if (th_encrypt((unsigned char*)&header[1], sizeof(uint32_t) * 3, 0x1b, 0x37, sizeof(uint32_t) * 3, 0x400) == -1) {
        return -1;
    }

    if (!util_seek(archive->stream, 0, NULL))
        return -1;

    if (fwrite(&header, sizeof(header), 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        return -1;
    }

    return 0;
}

const archive_module_t archive_th08 = {
    THDAT_BASENAME,
    th08_create,
    th08_write,
    th08_close,
    NULL,
    NULL
};
