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
#include "datpacker.h"
#include "thcrypt.h"
#include "thlzss.h"
#include "util.h"
#include "thdat.h"

static unsigned int
th95_get_crypt_param_index(const char *name)
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
    { 0xc1, 0x15, 0x400, 0x2C00 },
    { 0x03, 0x91, 0x80,  0x6400 },
    { 0xab, 0xdc, 0x80,  0x6e00 },
    { 0x12, 0x43, 0x200, 0x3c00 },
    { 0x35, 0x79, 0x400, 0x3c00 },
    { 0x99, 0x7d, 0x80,  0x2800 }
};

static archive_t*
th95_create(FILE* stream, unsigned int version, unsigned int count)
{
    return archive_create(stream, version, 16, count);
}

static int
th95_encrypt_data(archive_t* archive, entry_t* entry, unsigned char* data)
{
    const unsigned int i = th95_get_crypt_param_index(entry->name);
    const crypt_params_t* crypt_params = archive->version >= 12 ? th12_crypt_params : th95_crypt_params;

    if (th_encrypt(data, entry->zsize, crypt_params[i].key, crypt_params[i].step, crypt_params[i].block, crypt_params[i].limit) == -1) {
        free(data);
        return -1;
    }

    return 0;
}

static int
th95_write(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned char* data;

    data = thdat_read_file_lzss(entry, stream);

    if (entry->zsize >= entry->size) {
        free(data);

        if (!util_seek(stream, 0, NULL))
            return -1;

        data = thdat_read_file(entry, stream);
        if (!data)
            return -1;

        entry->zsize = entry->size;
    }

    if (th95_encrypt_data(archive, entry, data) == -1)
        return -1;

    return thdat_write_entry(archive, entry, data);
}

static int
th95_close(archive_t* archive)
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
        buffer_ptr = mempcpy(buffer_ptr, entry->name, namelen + (4 - namelen % 4));
        buffer_ptr = mempcpy(buffer_ptr, &entry->offset, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &entry->size, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &zero, sizeof(uint32_t));
    }

    zbuffer = th_lz_mem(buffer, list_size, &list_zsize);
    free(buffer);

    if (th_encrypt(zbuffer, list_zsize, 0x3e, 0x9b, 0x80, list_size) == -1) {
        free(zbuffer);
        return -1;
    }

    if (fwrite(zbuffer, list_zsize, 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        free(zbuffer);
        return -1;
    }
    free(zbuffer);

    if (!util_seek(archive->stream, 0, NULL))
        return -1;

    header[0] = 0x31414854;
    header[1] = list_size + 123456789;
    header[2] = list_zsize + 987654321;
    header[3] = archive->count - 0xf7e7f8ac;

    if (th_encrypt((unsigned char*)&header, sizeof(header), 0x1b, 0x37, sizeof(header), sizeof(header)) == -1) {
        return -1;
    }

    if (fwrite(&header, sizeof(header), 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        return -1;
    }

    return 0;
}

const archive_module_t archive_th95 = {
    THDAT_BASENAME,
    th95_create,
    th95_write,
    th95_close,
    NULL,
    NULL
};
