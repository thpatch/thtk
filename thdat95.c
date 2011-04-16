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
#include "file.h"
#include "program.h"
#include "thcrypt.h"
#include "thdat.h"
#include "thlzss.h"
#include "util.h"

static unsigned int
th95_get_crypt_param_index(
    const char *name)
{
    char index = 0;
    while (*name) index += *name++;
    return index & 7;
}

typedef struct {
    unsigned char key;
    unsigned char step;
    unsigned int block;
    unsigned int limit;
} crypt_params_t;

static const crypt_params_t th95_crypt_params[] = {
    /* key  step  block  limit */
    { 0x1b, 0x37, 0x40,  0x2800 },
    { 0x51, 0xe9, 0x40,  0x3000 },
    { 0xc1, 0x51, 0x80,  0x3200 },
    { 0x03, 0x19, 0x400, 0x7800 },
    { 0xab, 0xcd, 0x200, 0x2800 },
    { 0x12, 0x34, 0x80,  0x3200 },
    { 0x35, 0x97, 0x80,  0x2800 },
    { 0x99, 0x37, 0x400, 0x2000 }
};

static const crypt_params_t th12_crypt_params[] = {
    /* key  step  block  limit */
    { 0x1b, 0x73, 0x40,  0x3800 },
    { 0x51, 0x9e, 0x40,  0x4000 },
    { 0xc1, 0x15, 0x400, 0x2c00 },
    { 0x03, 0x91, 0x80,  0x6400 },
    { 0xab, 0xdc, 0x80,  0x6e00 },
    { 0x12, 0x43, 0x200, 0x3c00 },
    { 0x35, 0x79, 0x400, 0x3c00 },
    { 0x99, 0x7d, 0x80,  0x2800 }
};

static const crypt_params_t th13_crypt_params[] = {
    /* key  step  block  limit */
    { 0x1b, 0x73, 0x100, 0x3800 }, /* aa */
    { 0x12, 0x43, 0x200, 0x3e00 }, /* ff */
    { 0x35, 0x79, 0x400, 0x3c00 }, /* 11 */
    { 0x03, 0x91, 0x80,  0x6400 }, /* dd */
    { 0xab, 0xdc, 0x80,  0x6e00 }, /* ee */
    { 0x51, 0x9e, 0x100, 0x4000 }, /* bb */
    { 0xc1, 0x15, 0x400, 0x2c00 }, /* cc */
    { 0x99, 0x7d, 0x80,  0x4400 }  /* 77 */
};

static archive_t*
th95_open(
    FILE* stream,
    unsigned int version)
{
    archive_t* archive;
    uint32_t header[4];
    unsigned int size, zsize, count;
    long filesize = file_fsize(stream);
    unsigned char* data;
    unsigned char* zdata;
    unsigned char* ptr;
    unsigned int i;
    entry_t* prev = NULL;

    if (!file_read(stream, header, sizeof(header)))
        return NULL;

    th_decrypt((unsigned char*)&header, sizeof(header), 0x1b, 0x37,
        sizeof(header), sizeof(header));

    if (strncmp((const char*)&header[0], "THA1", 4)) {
        fprintf(stderr, "%s: wrong magic for archive\n", argv0);
        return NULL;
    }

    size = header[1] - 123456789;
    zsize = header[2] - 987654321;
    count = header[3] - 135792468;

    if (!file_seek(stream, filesize - zsize))
        return NULL;

    zdata = malloc(zsize);
    if (!file_read(stream, zdata, zsize)) {
        free(zdata);
        return NULL;
    }

    th_decrypt(zdata, zsize, 0x3e, 0x9b, 0x80, zsize);

    data = malloc(size);
    th_unlz_mem(zdata, zsize, data, size);
    free(zdata);

    archive = thdat_open(stream, version);

    ptr = data;
    for (i = 0; i < count; ++i) {
        entry_t* e = thdat_add_entry(archive);

        /* TODO: Use mempcpy or a struct. */
        strcpy(e->name, (char*)ptr);
        ptr += strlen(e->name) + (4 - strlen(e->name) % 4);
        memcpy(&e->offset, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(&e->size, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        ptr += sizeof(uint32_t);
    }

    for (i = 0; i < count; ++i) {
        entry_t* e = &archive->entries[i];
        if (prev)
            prev->zsize = e->offset - prev->offset;
        prev = e;
    }
    prev->zsize = (filesize - zsize) - prev->offset;

    free(data);

    return archive;
}

static void
th95_decrypt_data(
    archive_t* archive,
    entry_t* entry,
    unsigned char* data)
{
    const unsigned int i = th95_get_crypt_param_index(entry->name);
    const crypt_params_t* crypt_params;
    if (archive->version == 95 || archive->version == 10 || archive->version == 11) {
        crypt_params = th95_crypt_params;
    } else if (archive->version == 12 || archive->version == 125 || archive->version == 128) {
        crypt_params = th12_crypt_params;
    } else {
        crypt_params = th13_crypt_params;
    }

    th_decrypt(data, entry->zsize, crypt_params[i].key, crypt_params[i].step,
        crypt_params[i].block, crypt_params[i].limit);
}

static int
th95_extract(
    archive_t* archive,
    entry_t* entry,
    FILE* stream)
{
    unsigned char* data;
    unsigned char* zdata = malloc(entry->zsize);
    int ret;

#pragma omp critical
    ret = file_seek(archive->stream, entry->offset) &&
          file_read(archive->stream, zdata, entry->zsize);

    if (!ret) {
        free(zdata);
        return 0;
    }

    th95_decrypt_data(archive, entry, zdata);

    if (entry->zsize == entry->size) {
        data = zdata;
    } else {
        data = malloc(entry->size);
        th_unlz_mem(zdata, entry->zsize, data, entry->size);

        free(zdata);
    }

    if (!file_write(stream, data, entry->size)) {
        free(data);
        return 0;
    }

    free(data);

    return 1;
}

static archive_t*
th95_create(
    FILE* stream,
    unsigned int version,
    unsigned int count)
{
    return archive_create(stream, version, 16, count);
}

static void
th95_encrypt_data(
    archive_t* archive,
    entry_t* entry,
    unsigned char* data)
{
    const unsigned int i = th95_get_crypt_param_index(entry->name);
    const crypt_params_t* crypt_params;
    if (archive->version == 95 || archive->version == 10 || archive->version == 11) {
        crypt_params = th95_crypt_params;
    } else if (archive->version == 12 || archive->version == 125 || archive->version == 128) {
        crypt_params = th12_crypt_params;
    } else {
        crypt_params = th13_crypt_params;
    }

    th_encrypt(data, entry->zsize, crypt_params[i].key, crypt_params[i].step,
        crypt_params[i].block, crypt_params[i].limit);
}

static int
th95_write(
    archive_t* archive,
    entry_t* entry,
    FILE* stream)
{
    unsigned char* data;

    data = thdat_read_file_lzss(entry, stream);

    if (entry->zsize >= entry->size) {
        free(data);

        if (!file_seek(stream, 0))
            return 0;

        data = thdat_read_file(entry, stream);
        if (!data)
            return 0;

        entry->zsize = entry->size;
    }

    th95_encrypt_data(archive, entry, data);

    return thdat_write_entry(archive, entry, data);
}

static int
th95_close(
    archive_t* archive)
{
    unsigned char* buffer;
    unsigned char* buffer_ptr;
    unsigned int i;
    unsigned char* zbuffer;
    uint32_t header[4];
    unsigned int list_size = 0;
    unsigned int list_zsize = 0;

    thdat_sort(archive);

    for (i = 0; i < archive->count; ++i) {
        const size_t namelen = strlen(archive->entries[i].name);
        list_size += (sizeof(uint32_t) * 3) + namelen + (4 - namelen % 4);
    }

    buffer = malloc(list_size);

    buffer_ptr = buffer;
    for (i = 0; i < archive->count; ++i) {
        const uint32_t zero = 0;
        const entry_t* entry = &archive->entries[i];
        const size_t namelen = strlen(entry->name);
        buffer_ptr = mempcpy(buffer_ptr, entry->name,
            namelen + (4 - namelen % 4));
        buffer_ptr = mempcpy(buffer_ptr, &entry->offset, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &entry->size, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &zero, sizeof(uint32_t));
    }

    zbuffer = th_lz_mem(buffer, list_size, &list_zsize);
    free(buffer);

    th_encrypt(zbuffer, list_zsize, 0x3e, 0x9b, 0x80, list_size);

    if (!file_write(archive->stream, zbuffer, list_zsize)) {
        free(zbuffer);
        return 0;
    }
    free(zbuffer);

    if (!file_seek(archive->stream, 0))
        return 0;

    header[0] = 0x31414854;
    header[1] = list_size + 123456789;
    header[2] = list_zsize + 987654321;
    header[3] = archive->count + 135792468;

    th_encrypt((unsigned char*)&header, sizeof(header), 0x1b, 0x37,
        sizeof(header), sizeof(header));

    if (!file_write(archive->stream, &header, sizeof(header)))
        return 0;

    return 1;
}

const archive_module_t archive_th95 = {
    THDAT_BASENAME,
    th95_create,
    th95_write,
    th95_close,
    th95_open,
    th95_extract
};
