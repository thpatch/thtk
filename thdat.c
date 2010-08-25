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
#include <string.h>
#include <errno.h>
#include "args.h"
#include "program.h"
#include "util.h"
#include "thdat.h"
#include "thrle.h"
#include "thlzss.h"

extern const archive_module_t archive_th02;
extern const archive_module_t archive_th03;
extern const archive_module_t archive_th06;
extern const archive_module_t archive_th75;
extern const archive_module_t archive_th08;
extern const archive_module_t archive_th95;

archive_t*
thdat_open(
    FILE* stream,
    unsigned int version)
{
    archive_t* archive = malloc(sizeof(archive_t));

    archive->version = version;
    archive->stream = stream;
    archive->offset = 0;
    archive->count = 0;
    archive->entries = NULL;

    return archive;
}

archive_t*
archive_create(
    FILE* stream,
    uint32_t version,
    uint32_t offset,
    unsigned int count)
{
    archive_t* archive;

    /* Reserve some space for the header. */
    if (!util_seek(stream, offset))
        return NULL;

    archive = malloc(sizeof(archive_t));
    archive->entries = malloc(count * sizeof(entry_t));

    archive->version = version;
    archive->stream = stream;
    archive->offset = offset;
    archive->count = 0;

    return archive;
}

unsigned char*
thdat_read_file(
    const entry_t* entry,
    FILE* stream)
{
    unsigned char* data = malloc(entry->size);

    if (!util_read(stream, data, entry->size)) {
        free(data);
        return NULL;
    }

    return data;
}

unsigned char*
thdat_read_file_lzss(
    entry_t* entry,
    FILE* stream)
{
    return th_lz_file(stream, &entry->zsize);
}

unsigned char*
thdat_rle(
    entry_t* entry,
    unsigned char* data)
{
    unsigned char* zdata = th_rle(data, entry->size, &entry->zsize);

    if (entry->zsize >= entry->size) {
        entry->zsize = entry->size;
        free(zdata);
        zdata = data;
    } else {
        free(data);
    }

    return zdata;
}

entry_t*
thdat_add_entry(
    archive_t* archive)
{
    entry_t* e;

    archive->count++;
    archive->entries =
        realloc(archive->entries, archive->count * sizeof(entry_t));

    e = &archive->entries[archive->count - 1];

    memset(e->name, 0, 255);
    e->size = 0;
    e->zsize = 0;
    e->offset = 0;
    e->extra = 0;

    return e;
}

int
thdat_write_entry(
    archive_t* archive,
    entry_t* entry,
    unsigned char* data)
{
    int ret;

#pragma omp critical
{
    ret = util_write(archive->stream, data, entry->zsize);
    entry->offset = archive->offset;
    archive->offset += entry->zsize;
}

    free(data);
    return ret;
}

static int
entry_compar(const void* a, const void* b)
{
    entry_t* ea = (entry_t*)a;
    entry_t* eb = (entry_t*)b;
    if (ea->offset < eb->offset)
        return -1;
    else if (ea->offset > eb->offset)
        return 1;
    else
        return 0;
}

void
thdat_sort(
    archive_t* archive)
{
    qsort(archive->entries, archive->count, sizeof(entry_t), entry_compar);
}

static entry_t*
archive_add_entry(
    archive_t* archive,
    FILE* stream,
    const char* filename,
    unsigned int flags)
{
    entry_t* e;

    /* TODO: Pass the index to add_entry instead. */
#pragma omp critical
    e = &archive->entries[archive->count++];

    memset(e->name, 0, 256);
    e->size = util_fsize(stream);
    e->zsize = 0;
    e->offset = 0;
    e->extra = 0;

    if (flags & THDAT_BASENAME)
        util_basename(e->name, 255, filename);
    else
        strncpy(e->name, filename, 255);

    if ((int)e->size == -1)
        return NULL;

    return e;
}

static void
archive_check_duplicates(
    const archive_t* archive)
{
    unsigned int i, j;

    for (i = 0; i < archive->count; ++i) {
        for (j = 0; j < archive->count; ++j) {
            if (i == j)
                continue;
            if (strcmp(archive->entries[i].name, archive->entries[j].name) == 0)
                fprintf(stderr, "%s: duplicate entry: %s\n",
                    argv0, archive->entries[i].name);
        }
    }
}

static void
archive_free(
    archive_t* archive)
{
    if (archive) {
        free(archive->entries);
        free(archive);
    }
}

static void
print_usage(void)
{
    printf("Usage: %s COMMAND[OPTION...] [ARCHIVE [FILE...]]\n"
           "COMMAND can be:\n"
           "  c  create an archive\n"
           "  x  extract an archive\n"
           "  h  display this help and exit\n"
           "  V  display version information and exit\n"
           "OPTION can be:\n"
           "  #  # can be 2, 3, 4, 5, 6, 7, 75, 8, 9, 95, 10, 11, 12, 125, or 128, defaults to the latest\n\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

int
main(
    int argc,
    char* argv[])
{
    FILE* archive;
    archive_t* private;
    const archive_module_t* archive_module = NULL;
    unsigned int version = 125;
    int i;
    int mode;

    mode = parse_args(argc, argv, print_usage, "cxhV", "", &version);

    if (!mode)
        return 1;

    switch (version) {
    case 2:
        archive_module = &archive_th02;
        break;
    case 3:
    case 4:
    case 5:
        archive_module = &archive_th03;
        break;
    case 6:
    case 7:
        archive_module = &archive_th06;
        break;
    case 75:
        archive_module = &archive_th75;
        break;
    case 8:
    case 9:
        archive_module = &archive_th08;
        break;
    case 95:
    case 10:
    case 11:
    case 12:
    case 125:
    case 128:
        archive_module = &archive_th95;
        break;
    }

    if (!archive_module) {
        fprintf(stderr, "%s: unsupported version\n", argv0);
        return 1;
    }

    switch (mode) {
    case 'h':
        print_usage();
        return 0;
    case 'V':
        util_print_version("thdat", PACKAGE_THDAT_VERSION);
        return 0;
    case 'c':
        if (argc < 4) {
            print_usage();
            return 1;
        }
        archive = fopen(argv[2], "wb");
        if (!archive) {
            fprintf(stderr, "%s: couldn't open %s for writing\n",
                argv0, argv[2]);
            return 1;
        }
        current_output = argv[2];

        private = archive_module->create(archive, version, argc - 3);
        if (!private) {
            fclose(archive);
            return 1;
        }

#pragma omp parallel shared(private)
#pragma omp for
        for (i = 3; i < argc; ++i) {
            entry_t* entry;
            FILE* stream = fopen(argv[i], "rb");
            if (!stream) {
                fprintf(stderr, "%s: couldn't open %s for reading\n",
                    argv0, argv[i]);
                fclose(archive);
                exit(1);
            }
            current_input = argv[i];
#pragma omp critical
{
            printf("%s...\n", argv[i]);
            fflush(stdout);
}

            entry = archive_add_entry(private, stream, argv[i], archive_module->flags);
            if (!entry || !archive_module->write(private, entry, stream)) {
                fclose(stream);
                fclose(archive);
                archive_free(private);
                exit(1);
            }

            fclose(stream);
        }

        archive_check_duplicates(private);

        if (!archive_module->close(private)) {
            fclose(archive);
            archive_free(private);
            return 1;
        }

        archive_free(private);
        fclose(archive);
        return 0;
    case 'x':
        if (argc < 3) {
            print_usage();
            return 1;
        }
        if (!archive_module->open || !archive_module->extract) {
            fprintf(stderr,
                "%s: extraction not yet supported for this archive format\n",
                argv0);
            return 1;
        }
        current_input = argv[2];
        archive = fopen(current_input, "rb");
        if (!archive) {
            fprintf(stderr, "%s: couldn't open %s for reading\n",
                argv0, current_input);
            return 1;
        }
        private = archive_module->open(archive, version);
        if (!private)
            return 1;
        if (argc > 3) {
            int j;
#pragma omp parallel shared(private)
#pragma omp for
            for (j = 3; j < argc; ++j) {
                int extracted = 0;
                for (i = 0; i < (int)private->count; ++i) {
                    if (strcmp(argv[j], private->entries[i].name) == 0) {
                        FILE* stream;
                        stream = fopen(private->entries[i].name, "wb");
                        current_output = private->entries[i].name;
                        printf("%s\n", current_output);
                        if (!archive_module->extract(private, &private->entries[i], stream))
                            exit(1);
                        fclose(stream);
                        ++extracted;
                        break;
                    }
                }

                if (!extracted)
                    fprintf(stderr, "%s: entry %s not found\n", argv0, argv[j]);
            }
        } else {
#pragma omp parallel shared(private)
#pragma omp for
            for (i = 0; i < (int)private->count; ++i) {
                /* TODO: Secure the filenames first.
                 *       Don't need to do that when the user selects files
                 *       to extract. */
                FILE* stream = fopen(private->entries[i].name, "wb");
                current_output = private->entries[i].name;

                printf("%s\n", private->entries[i].name);

                if (!archive_module->extract(private, &private->entries[i], stream))
                    exit(1);
                fclose(stream);
            }
        }
        archive_free(private);
        fclose(archive);
        return 0;
    default:
        /* XXX: This shouldn't happen. */
        abort();
        return 1;
    }
}
