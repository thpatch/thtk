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
#include <stdlib.h>
#include <thtk/thtk.h>
#include "thcrypt.h"
#include "thdat.h"
#include "thlzss.h"
#include "util.h"
#include "dattypes.h"

static unsigned int
th95_get_crypt_param_index(
    const char *name)
{
    char index = 0;
    while (*name) index += *name++;
    return index & 7;
}

static const crypt_params_t *
th95_get_crypt_param(
    unsigned int version,
    const char *name)
{
    const unsigned int i = th95_get_crypt_param_index(name);
    switch (version) {
        case 95:
        case 10:
        case 103:
        case 11:
            return &th95_crypt_params[i];
        case 12:
        case 125:
        case 128:
            return &th12_crypt_params[i];
        case 13:
            return &th13_crypt_params[i];
        case 14:
        case 143:
        case 15:
        case 16:
        case 165:
        case 17:
        case 18:
        /* NEWHU: */
        default:
            return &th14_crypt_params[i];
    }
}

static int
th95_open(
    thdat_t* thdat,
    thtk_error_t** error)
{
    th95_archive_header_t header;

    if (thtk_io_read(thdat->stream, &header, sizeof(header), error) == -1)
        return 0;

    th_decrypt((unsigned char*)&header, sizeof(header), 0x1b, 0x37,
        sizeof(header), sizeof(header));

    if (strncmp((const char*)header.magic, "THA1", 4) != 0) {
        thtk_error_new(error, "wrong magic for archive");
        return 0;
    }

    header.size -= 123456789;
    header.zsize -= 987654321;
    header.entry_count -= 135792468;

    if (thtk_io_seek(thdat->stream, -(off_t)header.zsize, SEEK_END, error) == -1)
        return 0;

    unsigned char* zdata = malloc(header.zsize);
    if (thtk_io_read(thdat->stream, zdata, header.zsize, error) != header.zsize) {
        free(zdata);
        return 0;
    }

    th_decrypt(zdata, header.zsize, 0x3e, 0x9b, 0x80, header.zsize);

    thtk_io_t* zdata_stream = thtk_io_open_memory(zdata, header.zsize, error);
    if (!zdata_stream)
        return 0;
    thtk_io_t* data_stream = thtk_io_open_growing_memory(error);
    if (!data_stream)
        return 0;
    if (th_unlzss(zdata_stream, data_stream, header.size, error) == -1)
        return 0;
    thtk_io_close(zdata_stream);
    unsigned char* data = malloc(header.size);
    if (thtk_io_seek(data_stream, 0, SEEK_SET, error) == -1)
        return 0;
    if (thtk_io_read(data_stream, data, header.size, error) != header.size)
        return 0;
    thtk_io_close(data_stream);

    thdat->entry_count = header.entry_count;
    thdat->entries = calloc(header.entry_count, sizeof(thdat_entry_t));

    if (header.entry_count) {
        thdat_entry_t* prev = NULL;
        const uint32_t* ptr = (uint32_t*)data;
        for (uint32_t i = 0; i < header.entry_count; ++i) {
            thdat_entry_t* entry = &thdat->entries[i];
            thdat_entry_init(entry);

            strcpy(entry->name, (char*)ptr);
            ptr = (uint32_t*)((char*)ptr + strlen(entry->name) + (4 - strlen(entry->name) % 4));
            entry->offset = *ptr++;
            entry->size = *ptr++;
            /* Zero. */
            entry->extra = *ptr++;

            if (prev)
                prev->zsize = entry->offset - prev->offset;
            prev = entry;
        }
        off_t filesize = thtk_io_seek(thdat->stream, 0, SEEK_END, error);
        if (filesize == -1)
            return 0;
        prev->zsize = (filesize - header.zsize) - prev->offset;
    }

    free(data);

    return 1;
}

static ssize_t
th95_read(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    unsigned char* data;
    unsigned char* zdata = malloc(entry->zsize);

    if ((thtk_io_seek(thdat->stream, entry->offset, SEEK_SET, error) == -1) ||
            (thtk_io_read(thdat->stream, zdata, entry->zsize, error) != entry->zsize))
        return -1;

    const crypt_params_t* crypt_params = th95_get_crypt_param(thdat->version, entry->name);
    th_decrypt(zdata, entry->zsize, crypt_params->key, crypt_params->step,
        crypt_params->block, crypt_params->limit);

    if (entry->zsize == entry->size) {
        data = zdata;
    } else {
        thtk_io_t* zdata_stream = thtk_io_open_memory(zdata, entry->zsize, error);
        if (!zdata_stream)
            return -1;
        thtk_io_t* data_stream = thtk_io_open_growing_memory(error);
        if (!data_stream)
            return -1;
        if (th_unlzss(zdata_stream, data_stream, entry->size, error) == -1)
            return -1;
        thtk_io_close(zdata_stream);

        if (thtk_io_seek(data_stream, 0, SEEK_SET, error) == -1)
            return -1;
        data = malloc(entry->size);
        if (thtk_io_read(data_stream, data, entry->size, error) != entry->size)
            return -1;
        thtk_io_close(data_stream);
    }

    if (thtk_io_write(output, data, entry->size, error) == -1)
        return -1;

    free(data);

    return 1;
}

static int
th95_create(
    thdat_t* thdat,
    thtk_error_t** error)
{
    thdat->offset = 16;
    if (thtk_io_seek(thdat->stream, thdat->offset, SEEK_SET, error) == -1)
        return 0;
    return 1;
}

static ssize_t
th95_write(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    unsigned char* data;

    off_t first_offset = thtk_io_seek(input, 0, SEEK_CUR, error);
    if (first_offset == -1)
        return -1;

    entry->size = input_length;
    thtk_io_t* data_stream = thtk_io_open_growing_memory(error);
    if (!data_stream)
        return -1;
    if ((entry->zsize = th_lzss(input, entry->size, data_stream, error)) == -1)
        return -1;

    if (entry->zsize >= entry->size) {
        thtk_io_close(data_stream);

        if (thtk_io_seek(input, first_offset, SEEK_SET, error) == -1)
            return -1;
        data = malloc(entry->size);
        if (thtk_io_read(input, data, entry->size, error) != entry->size)
            return -1;

        entry->zsize = entry->size;
    } else {
        data = malloc(entry->zsize);
        if (thtk_io_seek(data_stream, 0, SEEK_SET, error) == -1)
            return -1;
        int ret = thtk_io_read(data_stream, data, entry->zsize, error);
        if (ret != entry->zsize)
            return -1;
        thtk_io_close(data_stream);
    }

    const crypt_params_t* crypt_params = th95_get_crypt_param(thdat->version, entry->name);
    th_encrypt(data, entry->zsize, crypt_params->key, crypt_params->step,
        crypt_params->block, crypt_params->limit);

    if (!(thtk_io_write(thdat->stream, data, entry->zsize, error) != entry->zsize)) {
        entry->offset = thdat->offset;
        thdat->offset += entry->zsize;
        free(data);
        return entry->zsize;
    } else {
        free(data);
        return -1;
    }
}

static int
th95_close(
    thdat_t* thdat,
    thtk_error_t** error)
{
    unsigned char* buffer;
    unsigned int i;
    unsigned char* zbuffer;
    uint32_t header[4];
    ssize_t list_size = 0;
    ssize_t list_zsize = 0;

    for (i = 0; i < thdat->entry_count; ++i) {
        const size_t namelen = strlen(thdat->entries[i].name);
        list_size += (sizeof(uint32_t) * 3) + namelen + (4 - namelen % 4);
    }

    if (list_size == 0) {
        thtk_error_new(error, "no entries");
        return 0;
    }

    buffer = malloc(list_size);

    uint32_t* buffer_ptr = (uint32_t*)buffer;
    for (i = 0; i < thdat->entry_count; ++i) {
        const thdat_entry_t* entry = &thdat->entries[i];
        const size_t namelen = strlen(entry->name);
        buffer_ptr = MEMPCPY(buffer_ptr, entry->name,
            namelen + (4 - namelen % 4));
        *buffer_ptr++ = entry->offset;
        *buffer_ptr++ = entry->size;
        *buffer_ptr++ = 0;
    }

    thtk_io_t* buffer_stream = thtk_io_open_memory(buffer, list_size, error);
    if (!buffer_stream)
        return 0;
    thtk_io_t* zbuffer_stream = thtk_io_open_growing_memory(error);
    if (!zbuffer_stream)
        return 0;

    if ((list_zsize = th_lzss(buffer_stream, list_size, zbuffer_stream, error)) == -1)
        return 0;

    thtk_io_close(buffer_stream);
    zbuffer = malloc(list_zsize);
    if (thtk_io_seek(zbuffer_stream, 0, SEEK_SET, error) == -1)
        return 0;
    if (thtk_io_read(zbuffer_stream, zbuffer, list_zsize, error) == -1)
        return 0;
    thtk_io_close(zbuffer_stream);

    th_encrypt(zbuffer, list_zsize, 0x3e, 0x9b, 0x80, list_size);

    if (thtk_io_write(thdat->stream, zbuffer, list_zsize, error) == -1) {
        free(zbuffer);
        return 0;
    }
    free(zbuffer);

    if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
        return 0;

    memcpy(&header[0], "THA1", 4);
    header[1] = list_size + 123456789;
    header[2] = list_zsize + 987654321;
    header[3] = thdat->entry_count + 135792468;

    th_encrypt((unsigned char*)&header, sizeof(header), 0x1b, 0x37,
        sizeof(header), sizeof(header));

    if (thtk_io_write(thdat->stream, &header, sizeof(header), error) == -1)
        return 0;

    return 1;
}

const thdat_module_t archive_th95 = {
    THDAT_BASENAME,
    th95_open,
    th95_create,
    th95_close,
    th95_read,
    th95_write
};
