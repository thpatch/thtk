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
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <thtk/thtk.h>
#include "thdat.h"
#include "thrle.h"

extern const thdat_module_t archive_th02;
extern const thdat_module_t archive_th06;
extern const thdat_module_t archive_th08;
extern const thdat_module_t archive_th95;

static const thdat_module_t*
thdat_version_to_module(
    unsigned int version,
    thtk_error_t** error)
{
    switch (version) {
    case 2:
    case 3:
    case 4:
    case 5:
        return &archive_th02;
    case 6:
    case 7:
        return &archive_th06;
    case 8:
    case 9:
        return &archive_th08;
    case 95:
    case 10:
    case 11:
    case 12:
    case 125:
    case 128:
    case 13:
        return &archive_th95;
    }

    thtk_error_new(error, "no module found for version %u", version);
    return NULL;
}

void
thdat_entry_init(
    thdat_entry_t* entry)
{
    if (entry) {
        memset(entry->name, 0, sizeof(entry->name));
        entry->extra = 0;
        entry->offset = entry->zsize = entry->size = -1;
    }
}

static thdat_t*
thdat_new(
    unsigned int version,
    thtk_io_t* stream,
    thtk_error_t** error)
{
    thdat_t* thdat;
    if (!stream) {
        thtk_error_new(error, "invalid parameter passed");
        return 0;
    }
    thdat = malloc(sizeof(*thdat));
    thdat->version = version;
    if (!(thdat->module = thdat_version_to_module(version, error))) {
        free(thdat);
        return NULL;
    }
    thdat->stream = stream;
    thdat->entry_count = 0;
    thdat->entries = NULL;
    thdat->offset = 0;
    return thdat;
}

thdat_t*
thdat_open(
    unsigned int version,
    thtk_io_t* input,
    thtk_error_t** error)
{
    thdat_t* thdat;
    if (!input) {
        thtk_error_new(error, "invalid parameter passed");
        return 0;
    }
    if (thtk_io_seek(input, 0, SEEK_SET, error) == -1)
        return NULL;
    if (!(thdat = thdat_new(version, input, error)))
        return NULL;
    if (!thdat->module->open(thdat, error)) {
        thdat_free(thdat);
        return NULL;
    }
    return thdat;
}

thdat_t*
thdat_create(
    unsigned int version,
    thtk_io_t* output,
    size_t entry_count,
    thtk_error_t** error)
{
    thdat_t* thdat;
    if (!output) {
        thtk_error_new(error, "invalid parameter passed");
        return 0;
    }
    if (thtk_io_seek(output, 0, SEEK_SET, error) == -1)
        return NULL;
    if (!(thdat = thdat_new(version, output, error)))
        return NULL;
    thdat->entry_count = entry_count;
    thdat->entries = calloc(entry_count, sizeof(thdat_entry_t));
    if (!thdat->module->create(thdat, error)) {
        thdat_free(thdat);
        return NULL;
    }
    return thdat;
}

static int
thdat_entry_compar(
    const void* a,
    const void* b)
{
    const thdat_entry_t* ea = a;
    const thdat_entry_t* eb = b;
    return (int)ea->offset - eb->offset;
}

int
thdat_close(
    thdat_t* thdat,
    thtk_error_t** error)
{
    if (!thdat) {
        thtk_error_new(error, "invalid parameter passed");
        return 0;
    }
    qsort(thdat->entries, thdat->entry_count, sizeof(thdat_entry_t), thdat_entry_compar);
    return thdat->module->close(thdat, error);
}

void
thdat_free(
    thdat_t* thdat)
{
    if (thdat) {
        free(thdat->entries);
        free(thdat);
    }
}

ssize_t
thdat_entry_count(
    thdat_t* thdat,
    thtk_error_t** error)
{
    if (!thdat) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    return thdat->entry_count;
}

ssize_t
thdat_entry_by_name(
    thdat_t* thdat,
    const char* name,
    thtk_error_t** error)
{
    if (!thdat || !name) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    for (size_t e = 0; e < thdat->entry_count; ++e) {
        if (strcmp(name, thdat->entries[e].name) == 0)
            return e;
    }
    return -1;
}

int
thdat_entry_set_name(
    thdat_t* thdat,
    int entry_index,
    const char* name,
    thtk_error_t** error)
{
    if (thdat && name && entry_index >= 0 && entry_index < (int)thdat->entry_count) {
        char temp_name[256];
        strncpy(temp_name, name, 255);

        if (thdat->module->flags & THDAT_BASENAME) {
#ifdef WIN32
            char filename[_MAX_FNAME];
            char ext[_MAX_EXT];
            _splitpath(temp_name, NULL, NULL, filename, ext);
            snprintf(temp_name, 255, "%s%s", filename, ext);
#else
            char temp_name2[256];
            strncpy(temp_name2, temp_name, 255);
            strncpy(temp_name, basename(temp_name2), 255);
#endif
        }

        if (thdat->module->flags & THDAT_UPPERCASE) {
            for (unsigned int i = 0; i < 255 && temp_name[i]; ++i) {
                temp_name[i] = toupper(temp_name[i]);
            }
        }

        if (thdat->module->flags & THDAT_8_3) {
            const char* dotpos = strchr(temp_name, '.');
            size_t name_len = dotpos ? strlen(temp_name) - strlen(dotpos) : strlen(temp_name);
            size_t ext_len = dotpos ? strlen(dotpos + 1) : 0;

            if (name_len > 8 || ext_len > 3) {
                thtk_error_new(error, "name is not 8.3");
                return 0;
            }
        }

        strcpy(thdat->entries[entry_index].name, temp_name);

        return 1;
    }

    thtk_error_new(error, "invalid parameter passed");
    return 0;
}

const char*
thdat_entry_get_name(
    thdat_t* thdat,
    int entry_index,
    thtk_error_t** error)
{
    if (!thdat || entry_index < 0 || entry_index >= (int)thdat->entry_count) {
        thtk_error_new(error, "invalid parameter passed");
        return NULL;
    }
    return thdat->entries[entry_index].name;
}

ssize_t
thdat_entry_get_size(
    thdat_t* thdat,
    int entry_index,
    thtk_error_t** error)
{
    if (!thdat || entry_index < 0 || entry_index >= (int)thdat->entry_count) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    return thdat->entries[entry_index].size;
}

ssize_t
thdat_entry_get_zsize(
    thdat_t* thdat,
    int entry_index,
    thtk_error_t** error)
{
    if (!thdat || entry_index < 0 || entry_index >= (int)thdat->entry_count) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    return thdat->entries[entry_index].zsize;
}

ssize_t
thdat_entry_write_data(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error)
{
    if (!thdat || entry_index < 0 || entry_index >= (int)thdat->entry_count || !input) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    return thdat->module->write(thdat, entry_index, input, input_length, error);
}

ssize_t
thdat_entry_read_data(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error)
{
    if (!thdat || entry_index < 0 || entry_index >= (int)thdat->entry_count || !output) {
        thtk_error_new(error, "invalid parameter passed");
        return -1;
    }
    return thdat->module->read(thdat, entry_index, output, error);
}
