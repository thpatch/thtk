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
#include <errno.h>
#include <string.h>
#include "datpacker.h"
#include "thdat.h"
#include "util.h"

static archive_t*
th02_create(FILE* stream, unsigned int version, unsigned int count)
{
    return archive_create(stream, version, (count + 1) * 32, count);
}

/* TODO: Check that filenames are 8.3, make it a THDAT_ flag. */
/* TODO: Find out if lowercase filenames are supported. */
static int
th02_write(archive_t* archive, entry_t* entry, FILE* stream)
{
    unsigned int i;
    unsigned char* data;

    for (i = 0; i < 13; ++i)
        if (entry->name[i])
            entry->name[i] ^= 0xff;

    data = thdat_read_file(entry, stream);
    if (!data)
        return -1;

    data = thdat_rle(entry, data);

    for (i = 0; i < entry->zsize; ++i)
        data[i] ^= 0x12;

    return thdat_write_entry(archive, entry, data);
}

static int
th02_close(archive_t* archive)
{
    unsigned char* buffer;
    unsigned char* buffer_ptr;
    unsigned int i;
    unsigned int list_size = 32 * (archive->count + 1);

    if (!util_seek(archive->stream, 0, NULL))
        return -1;

    buffer = malloc(list_size);
    memset(buffer, 0, list_size);

    buffer_ptr = buffer;
    for (i = 0; i < archive->count; ++i) {
        const uint16_t magic1 = 0xf388;
        const uint16_t magic2 = 0x9595;
        entry_t* entry = &archive->entries[i];

        if (entry->zsize == entry->size)
            buffer_ptr = mempcpy(buffer_ptr, &magic1, sizeof(uint16_t));
        else
            buffer_ptr = mempcpy(buffer_ptr, &magic2, sizeof(uint16_t));

        *buffer_ptr++ = 3;

        buffer_ptr = mempcpy(buffer_ptr, entry->name, 13);
        buffer_ptr = mempcpy(buffer_ptr, &entry->zsize, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &entry->size, sizeof(uint32_t));
        buffer_ptr = mempcpy(buffer_ptr, &entry->offset, sizeof(uint32_t));
        buffer_ptr += 4;
    }

    if (fwrite(buffer, list_size, 1, archive->stream) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        free(buffer);
        return -1;
    }
    free(buffer);

    return 0;
}

const archive_module_t archive_th02 = {
    THDAT_BASENAME,
    th02_create,
    th02_write,
    th02_close,
    NULL,
    NULL
};
