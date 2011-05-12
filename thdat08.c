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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "program.h"
#include "thcrypt.h"
#include "thdat.h"
#include "thlzss.h"
#include "util.h"

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
#define TYPE_MSG 4
#define TYPE_TXT 5
#define TYPE_WAV 6

/* XXX: { '*', 0x99, 0x37, 0x400, 0x1000 } is listed by Brightmoon. */
static const crypt_params
th08_crypt_params[] = {
    { '-', 0x35, 0x97,   0x80, 0x2800 }, /* .*   */
    { 'A', 0xc1, 0x51, 0x1400, 0x2000 }, /* .anm */
    { 'E', 0xab, 0xcd,  0x200, 0x1000 }, /* .ecl */
    { 'J', 0x03, 0x19, 0x1400, 0x7800 }, /* .jpg */
    { 'M', 0x1b, 0x37,   0x40, 0x2000 }, /* .msg */
    { 'T', 0x51, 0xe9,   0x40, 0x3000 }, /* .txt */
    { 'W', 0x12, 0x34,  0x400, 0x2800 }, /* .wav */
};

static const crypt_params
th09_crypt_params[] = {
    { '-', 0x35, 0x97,  0x80, 0x2800 },
    { 'A', 0xc1, 0x51, 0x400,  0x400 },
    { 'E', 0xab, 0xcd, 0x200, 0x1000 },
    { 'J', 0x03, 0x19, 0x400,  0x400 },
    { 'M', 0x1b, 0x37,  0x40, 0x2800 },
    { 'T', 0x51, 0xe9,  0x40, 0x3000 },
    { 'W', 0x12, 0x34, 0x400,  0x400 }
};

static void
tolowerstr(
    char* str)
{
    while (*str) {
        *str = tolower(*str);
        ++str;
    }
}

static archive_t*
th08_open(
    FILE* stream,
    unsigned int version)
{
    archive_t* archive;
    char magic[4];
    uint32_t header[3];
    entry_t* e;
    unsigned int i;
    unsigned int count, offset, size;
    unsigned char* ptr;
    unsigned char* data;
    unsigned char* zdata;
    long filesize = file_fsize(stream);
    unsigned int zsize;

    if (!file_read(stream, magic, 4))
        return NULL;

    if (strncmp(magic, "PBGZ", 4)) {
        fprintf(stderr, "%s: %s is not a PBGZ archive\n", argv0, current_input);
        return NULL;
    }

    if (!file_read(stream, header, 3 * sizeof(uint32_t)))
        return NULL;

    th_decrypt((unsigned char*)header, 3 * sizeof(uint32_t), 0x1b, 0x37,
        3 * sizeof(uint32_t), 0x400);

    count = header[0] - 123456;
    offset = header[1] - 345678;
    size = header[2] - 567891;

    if (!file_seek(stream, offset))
        return NULL;

    zsize = filesize - offset;
    zdata = malloc(zsize);
    data = malloc(size);

    if (!file_read(stream, zdata, zsize)) {
        free(data);
        free(zdata);
        return NULL;
    }

    th_decrypt(zdata, zsize, 0x3e, 0x9b, 0x80, 0x400);

    th_unlz_mem(zdata, zsize, data, size);
    free(zdata);

    archive = thdat_open(stream, version);

    ptr = data;
    for (i = 0; i < count; ++i) {
        e = thdat_add_entry(archive);

        strcpy(e->name, (char*)ptr);
        ptr += strlen(e->name) + 1;
        memcpy(&e->offset, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(&e->size, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        ptr += sizeof(uint32_t);
    }

    free(data);

    return archive;
}

static int
th08_extract(
    archive_t* archive,
    entry_t* entry,
    FILE* stream)
{
    const crypt_params* current_crypt_params = archive->version == 8 ?
        th08_crypt_params : th09_crypt_params;
    unsigned char* data = malloc(entry->size);
    unsigned int i = 0;
    int type = -1;

#pragma omp critical
{
    if (file_seek(archive->stream, entry->offset)) {
        i = 1;
        th_unlz_file(archive->stream, data, entry->size);
    }
}

    if (!i) {
        free(data);
        return 0;
    }

    if (strncmp((char*)data, "edz", 3)) {
        fprintf(stderr, "%s: entry did not start with \"edz\"\n", argv0);
        free(data);
        return 0;
    }

    entry->size -= 4;

    for (i = 0; i < 7; ++i) {
        if (current_crypt_params[i].type == data[3]) {
            type = i;
            break;
        }
    }

    if (type == -1) {
        fprintf(stderr, "%s: unsupported entry key '%c'\n", argv0, data[3]);
        free(data);
        return 0;
    }

    th_decrypt(data + 4,
               entry->size,
               current_crypt_params[type].key,
               current_crypt_params[type].step,
               current_crypt_params[type].block,
               current_crypt_params[type].limit);

    if (!file_write(stream, data + 4, entry->size)) {
        free(data);
        return 0;
    }

    free(data);

    return 1;
}

static archive_t*
th08_create(
    FILE* stream,
    unsigned int version,
    unsigned int count)
{
    return archive_create(stream, version, 16, count);
}

static unsigned char*
th08_read_file(
    entry_t* entry,
    FILE* stream)
{
    unsigned char* data = malloc(entry->size + 4);

    if (!file_read(stream, data + 4, entry->size)) {
        free(data);
        return NULL;
    }

    return data;
}

static void
th08_encrypt(
    archive_t* archive,
    entry_t* entry,
    unsigned char* data)
{
    const crypt_params* current_crypt_params = archive->version == 8 ?
        th08_crypt_params : th09_crypt_params;
    unsigned int type;
    char ext[5];

    /* TODO: Move all of this to one or two functions. */
    if (strlen(entry->name) < 4)
        strcpy(ext, "");
    else
        strcpy(ext, entry->name + strlen(entry->name) - 4);

    tolowerstr(ext);

    if (strcmp(ext, ".anm") == 0)
        type = TYPE_ANM;
    else if (strcmp(ext, ".ecl") == 0)
        type = TYPE_ECL;
    else if (strcmp(ext, ".jpg") == 0)
        type = TYPE_JPG;
    else if (strcmp(ext, ".msg") == 0)
        type = TYPE_MSG;
    else if (strcmp(ext, ".txt") == 0)
        type = TYPE_TXT;
    else if (strcmp(ext, ".wav") == 0)
        type = TYPE_WAV;
    else
        type = TYPE_ETC;

    data[0] = 'e';
    data[1] = 'd';
    data[2] = 'z';
    data[3] = current_crypt_params[type].type;

    th_encrypt(data + 4,
               entry->size,
               current_crypt_params[type].key,
               current_crypt_params[type].step,
               current_crypt_params[type].block,
               current_crypt_params[type].limit);
}

static unsigned char*
th08_lzss(
    entry_t* entry,
    unsigned char* data)
{
    unsigned char* zdata;
    zdata = th_lz_mem(data, entry->size + 4, &entry->zsize);
    free(data);
    return zdata;
}

static int
th08_write(
    archive_t* archive,
    entry_t* entry,
    FILE* stream)
{
    unsigned char* data;

    data = th08_read_file(entry, stream);
    if (!data)
        return 0;

    th08_encrypt(archive, entry, data);

    data = th08_lzss(entry, data);

    /* XXX: There doesn't seem to be an option for non-compressed data here. */

    return thdat_write_entry(archive, entry, data);
}

static int
th08_close(
    archive_t* archive)
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

    /* XXX: I'm adding some padding here to satisfy pbgzmlt.
     * The games work fine without it. */
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

    th_encrypt(zbuffer, list_zsize, 0x3e, 0x9b, 0x80, 0x400);

    if (!file_write(archive->stream, zbuffer, list_zsize)) {
        free(zbuffer);
        return 0;
    }
    free(zbuffer);

    header[0] = 0x5a474250; /* ZGBP */
    header[1] = archive->count + 123456;
    header[2] = archive->offset + 345678;
    header[3] = list_size + 567891;

    th_encrypt((unsigned char*)&header[1], sizeof(uint32_t) * 3, 0x1b, 0x37,
        sizeof(uint32_t) * 3, 0x400);

    if (!file_seek(archive->stream, 0))
        return 0;

    if (!file_write(archive->stream, header, sizeof(header)))
        return 0;

    return 1;
}

const archive_module_t archive_th08 = {
    THDAT_BASENAME,
    th08_create,
    th08_write,
    th08_close,
    th08_open,
    th08_extract
};
