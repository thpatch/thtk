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
#include <stdlib.h>
#include <thtk/thtk.h>
#include "thcrypt.h"
#include "thdat.h"
#include "thlzss.h"
#include "util.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    /* "PBGZ" */
    char magic[4];
    /* The following entries are also encrypted. */
    /* Stored + 123456. */
    uint32_t count;
    /* Stored + 345678. */
    uint32_t offset;
    /* Stored + 567891. */
    uint32_t size;
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th08_archive_header_t;

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

static int
th08_open(
    thdat_t* thdat,
    thtk_error_t** error)
{
    unsigned char* data;
    unsigned char* zdata;
    off_t filesize = thtk_io_seek(thdat->stream, 0, SEEK_END, error);
    if (filesize == -1)
        return 0;
    if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
        return 0;

    th08_archive_header_t header;

    if (thtk_io_read(thdat->stream, &header, sizeof(header), error) == -1)
        return 0;

    if (strncmp(header.magic, "PBGZ", 4)) {
        thtk_error_new(error, "magic not PBGZ");
        return 0;
    }

    th_decrypt((unsigned char*)&header + 4, sizeof(header) - 4, 0x1b, 0x37, sizeof(header) - 4, 0x400);

    header.count -= 123456;
    header.offset -= 345678;
    header.size -= 567891;

    if (thtk_io_seek(thdat->stream, header.offset, SEEK_SET, error) == -1)
        return 0;

    unsigned int zsize = filesize - header.offset;
    zdata = malloc(zsize);

    if (thtk_io_read(thdat->stream, zdata, zsize, error) == -1)
        return 0;

    th_decrypt(zdata, zsize, 0x3e, 0x9b, 0x80, 0x400);

    thtk_io_t* compressed_data = thtk_io_open_memory(zdata, zsize, error);
    if (!compressed_data)
        return 0;
    thtk_io_t* raw_data = thtk_io_open_growing_memory(error);
    if (!raw_data)
        return 0;
    if (th_unlzss(compressed_data, raw_data, header.size, error) == -1)
        return 0;

    thtk_io_close(compressed_data);

    data = malloc(header.size);
    if (thtk_io_seek(raw_data, 0, SEEK_SET, error) == -1)
        return 0;
    if (thtk_io_read(raw_data, data, header.size, error) != header.size)
        return 0;
    thtk_io_close(raw_data);

    const uint32_t* ptr = (uint32_t*)data;
    for (unsigned int i = 0; i < header.count; ++i) {
        thdat_entry_t* entry;
        ARRAY_GROW(thdat->entry_count, thdat->entries, entry);
        thdat_entry_init(entry);

        strcpy(entry->name, (char*)ptr);
        ptr = (uint32_t*)((char*)ptr + strlen(entry->name) + 1);
        entry->offset = *ptr++;
        entry->size = *ptr++;
        entry->extra = *ptr++;
    }

    if (header.count) {
        thdat_entry_t* prev = NULL;
        for (unsigned int i = 0; i < header.count; ++i) {
            thdat_entry_t* entry = &thdat->entries[i];
            if (prev)
                prev->zsize = entry->offset - prev->offset;
            prev = entry;
        }
        prev->zsize = (filesize - zsize) - prev->offset;
    }

    free(data);

    return 1;
}

static ssize_t
th08_read(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    const crypt_params* current_crypt_params = thdat->version == 8 ?
        th08_crypt_params : th09_crypt_params;
    unsigned int i = 0;
    int type = -1;

    thtk_io_t* raw_entry = thtk_io_open_growing_memory(error);
    if (!raw_entry)
        return -1;
    unsigned char* zdata = malloc(entry->zsize);

    int failed = 0;
#pragma omp critical
    {
        failed = (thtk_io_seek(thdat->stream, entry->offset, SEEK_SET, error) == -1) ||
                 (thtk_io_read(thdat->stream, zdata, entry->zsize, error) != entry->zsize);
    }

    if (failed)
        return -1;

    thtk_io_t* zdata_stream = thtk_io_open_memory(zdata, entry->zsize, error);
    if (!zdata_stream)
        return -1;

    if (th_unlzss(zdata_stream, raw_entry, entry->size, error) == -1)
        return -1;
    thtk_io_close(zdata_stream);
    if (thtk_io_seek(raw_entry, 0, SEEK_SET, error) == -1)
        return -1;

    char magic[3];
    char entry_type;
    if (thtk_io_read(raw_entry, magic, 3, error) != 3)
        return -1;
    if (thtk_io_read(raw_entry, &entry_type, 1, error) != 1)
        return -1;

    if (strncmp(magic, "edz", 3)) {
        thtk_error_new(error, "incorrect entry magic");
        return -1;
    }

    entry->size -= 4;

    for (i = 0; i < 7; ++i) {
        if (current_crypt_params[i].type == entry_type) {
            type = i;
            break;
        }
    }

    if (type == -1) {
        thtk_error_new(error, "unsupported entry key");
        return -1;
    }

    unsigned char* data = thtk_io_map(raw_entry, 4, entry->size, error);
    if (!data)
        return -1;

    th_decrypt(data,
               entry->size,
               current_crypt_params[type].key,
               current_crypt_params[type].step,
               current_crypt_params[type].block,
               current_crypt_params[type].limit);

    if (thtk_io_write(output, data, entry->size, error) == -1)
        return -1;

    thtk_io_unmap(raw_entry, data);

    thtk_io_close(raw_entry);

    return entry->size;
}

static int
th08_create(
    thdat_t* thdat,
    thtk_error_t** error)
{
    thdat->offset = sizeof(th08_archive_header_t);
    if (thtk_io_seek(thdat->stream, thdat->offset, SEEK_SET, error) == -1)
        return 0;
    return 1;
}

static const crypt_params*
find_crypt_params(
    unsigned int version,
    const char* name)
{
    const crypt_params* current_crypt_params = version == 8 ?
        th08_crypt_params : th09_crypt_params;
    char ext[5];
    unsigned int type;

    if (strlen(name) < 4)
        strcpy(ext, "");
    else
        strcpy(ext, name + strlen(name) - 4);

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

    return &current_crypt_params[type];
}

static ssize_t
th08_write(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error)
{
    thdat_entry_t* entry = &thdat->entries[entry_index];
    const crypt_params* crypt_params = find_crypt_params(thdat->version, entry->name);
    entry->size = input_length + 4;
    unsigned char* data = malloc(entry->size);

    data[0] = 'e';
    data[1] = 'd';
    data[2] = 'z';
    data[3] = crypt_params->type;
    if (thtk_io_read(input, data + 4, input_length, error) != (ssize_t)input_length)
        return -1;

    th_encrypt(data + 4, input_length, crypt_params->key, crypt_params->step, crypt_params->block, crypt_params->limit);

    thtk_io_t* data_stream = thtk_io_open_memory(data, entry->size, error);
    if (!data_stream)
        return -1;

    thtk_io_t* zdata_stream = thtk_io_open_growing_memory(error);
    if (!zdata_stream)
        return -1;
    entry->zsize = th_lzss(data_stream, entry->size, zdata_stream, error);
    thtk_io_close(data_stream);
    if (entry->zsize == -1)
        return -1;
    unsigned char* zdata = thtk_io_map(zdata_stream, 0, entry->zsize, error);
    if (!zdata)
        return -1;

#pragma omp critical
    {
        /* TODO: Handle error. */
        thtk_io_write(thdat->stream, zdata, entry->zsize, error);
        entry->offset = thdat->offset;
        thdat->offset += entry->zsize;
    }

    thtk_io_unmap(zdata_stream, zdata);
    thtk_io_close(zdata_stream);

    return entry->zsize;
}

static int
th08_close(
    thdat_t* thdat,
    thtk_error_t** error)
{
    unsigned char* buffer;
    unsigned char* buffer_ptr;
    unsigned int i;
    unsigned char* zbuffer;
    uint32_t header[4];
    ssize_t list_size = 0;
    ssize_t list_zsize = 0;
    const uint32_t zero = 0;

    for (i = 0; i < thdat->entry_count; ++i)
        list_size += strlen(thdat->entries[i].name) + 1 + (sizeof(uint32_t) * 3);

    /* XXX: I'm adding some padding here to satisfy pbgzmlt.
     * The games work fine without it. */
    list_size += 4;
    buffer = malloc(list_size);
    memset(buffer, 0, list_size);

    buffer_ptr = buffer;
    for (i = 0; i < thdat->entry_count; ++i) {
        thdat_entry_t* entry = &thdat->entries[i];
        buffer_ptr = mempcpy(buffer_ptr, entry->name, strlen(entry->name) + 1);
        buffer_ptr = mempcpy(buffer_ptr, &entry->offset, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &entry->size, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &zero, sizeof(uint32_t));
    }

    memcpy(buffer_ptr, &zero, sizeof(uint32_t));

    thtk_io_t* buffer_stream = thtk_io_open_memory(buffer, list_size, error);
    if (!buffer_stream)
        return 0;

    thtk_io_t* zbuffer_stream = thtk_io_open_growing_memory(error);
    if (!zbuffer_stream)
        return 0;
    if ((list_zsize = th_lzss(buffer_stream, list_size, zbuffer_stream, error)) == -1)
        return 0;
    thtk_io_close(buffer_stream);
    if (thtk_io_seek(zbuffer_stream, 0, SEEK_SET, error) == -1)
        return 0;

    zbuffer = malloc(list_zsize);
    if (thtk_io_read(zbuffer_stream, zbuffer, list_zsize, error) == -1)
        return 0;
    thtk_io_close(zbuffer_stream);

    th_encrypt(zbuffer, list_zsize, 0x3e, 0x9b, 0x80, 0x400);

    if (thtk_io_write(thdat->stream, zbuffer, list_zsize, error) == -1) {
        free(zbuffer);
        return 0;
    }
    free(zbuffer);

    header[0] = 0x5a474250; /* ZGBP */
    header[1] = thdat->entry_count + 123456;
    header[2] = thdat->offset + 345678;
    header[3] = list_size + 567891;

    th_encrypt((unsigned char*)&header[1], sizeof(uint32_t) * 3, 0x1b, 0x37,
        sizeof(uint32_t) * 3, 0x400);

    if (thtk_io_seek(thdat->stream, 0, SEEK_SET, error) == -1)
        return 0;

    if (thtk_io_write(thdat->stream, header, sizeof(header), error) == -1)
        return 0;

    return 1;
}

const thdat_module_t archive_th08 = {
    THDAT_BASENAME,
    th08_open,
    th08_create,
    th08_close,
    th08_read,
    th08_write
};
