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
#include "util.h"
#include "thdat.h"

static archive_t*
th75_create(FILE* stream, unsigned int version, unsigned int count)
{
    return archive_create(stream, version, 2 + 108 * count, count);
}

static void
th75_process_name(entry_t* entry)
{
    char* temp;

    entry->name[100] = 0;

    /* XXX: This is only needed on some platforms, and should be done in
     * archive_add_entry. */
    temp = entry->name;
    while (*temp) {
        if (*temp == '/') *temp = '\\';
        temp++;
    }
}

/* TODO: Figure out something nice for relative paths. */
static int
th75_write(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned char* data;

    th75_process_name(entry);

    data = thdat_read_file(entry, stream);
    if (!data)
        return -1;

    entry->zsize = entry->size;

    return thdat_write_entry(archive, entry, data);
}

static int
th75_close(archive_t* archive)
{
    unsigned char* buffer;
    unsigned char* buffer_ptr;
    unsigned int i;
    const unsigned int list_size = archive->count * 108;
    uint16_t count = archive->count;
    unsigned char k = 100, t = 100;

    if (!util_seek(archive->stream, 0))
        return -1;

    if (fwrite(&count, sizeof(uint16_t), 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        return -1;
    }

    buffer = malloc(list_size);

    buffer_ptr = buffer;
    for (i = 0; i < archive->count; ++i) {
        const entry_t* entry = &archive->entries[i];
        buffer_ptr = mempcpy(buffer_ptr, entry->name, 100);
        buffer_ptr = mempcpy(buffer_ptr, &entry->size, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &entry->offset, sizeof(uint32_t));
    }

    for (i = 0; i < list_size; ++i) {
        buffer[i] ^= k;
        k += t;
        t += 77;
    }

    if (fwrite(buffer, list_size, 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        free(buffer);
        return -1;
    }
    free(buffer);

    return 0;
}

const archive_module_t archive_th75 = {
    0,
    th75_create,
    th75_write,
    th75_close,
    NULL,
    NULL
};
