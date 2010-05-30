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
#include "datpacker.h"
#include "thdat.h"
#include "program.h"
#include "util.h"

extern const archive_module_t archive_th02;
extern const archive_module_t archive_th06;
extern const archive_module_t archive_th75;
extern const archive_module_t archive_th08;
extern const archive_module_t archive_th95;

char library_error[LIBRARY_ERROR_SIZE] = { 0 };

static void
print_usage()
{
    printf("Usage: %s [OPTION]... ARCHIVE FILE...\n"
           "OPTION can be:\n"
           "  -v VERSION  can be 2, 6, 7, 75, 8, 9, 95, 10, 11, 12, or 125, defaults to 125\n"
           "  -h          display this help and exit\n"
           "  -V          display version information and exit\n\n"
           "Additional documentation might be available at <" PACKAGE_URL ">.\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

static entry_t*
archive_add_entry(archive_t* archive, FILE* stream, const char* filename, unsigned int flags)
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

static int
archive_check_duplicates(archive_t* archive)
{
    unsigned int i, j;

    for (i = 0; i < archive->count; ++i) {
        for (j = 0; j < archive->count; ++j) {
            if (i == j)
                continue;
            if (strcmp(archive->entries[i].name, archive->entries[j].name) == 0)
                snprintf(library_error, LIBRARY_ERROR_SIZE, "duplicate filename ``%s''", archive->entries[i].name);
        }
    }

    return 0;
}

static void
archive_free(archive_t* archive)
{
    if (archive) {
        free(archive->entries);
        free(archive);
    }
}

int
main(int argc, char* argv[])
{
    FILE* archive;
    archive_t* private;
    const archive_module_t* archive_module = NULL;
    unsigned int version = 125;
    int i;
    int argstart;

    argv0 = util_shortname(argv[0]);

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "-V") == 0) {
            util_print_version("datpacker", PACKAGE_THDAT_VERSION);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0) {
            ++i;
            if (i == argc) {
                print_usage();
                return 1;
            }
            version = strtol(argv[i], NULL, 10);
        } else {
            break;
        }
    }

    if ((i + 2) > argc) {
        print_usage();
        return 1;
    }

    switch (version) {
    case 2:
        archive_module = &archive_th02;
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
        archive_module = &archive_th95;
        break;
    }

    if (!archive_module) {
        fprintf(stderr, "%s: unsupported version\n", argv0);
        return 1;
    }

    archive = fopen(argv[i], "wb");
    if (!archive) {
        fprintf(stderr, "%s: couldn't open %s for writing\n", argv0, argv[i]);
        return 1;
    }
    current_output = argv[i];

    private = archive_module->create(archive, version, argc - i - 1);
    if (!private) {
        fprintf(stderr, "%s: %s\n", argv0, library_error);
        fclose(archive);
        return 1;
    }

    argstart = i + 1;
#pragma omp parallel shared(private)
#pragma omp for
    for (i = argstart; i < argc; ++i) {
        entry_t* entry;
        FILE* stream = fopen(argv[i], "rb");
        if (!stream) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv0, argv[i]);
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
        if (!entry || archive_module->write(private, entry, stream) == -1) {
            fprintf(stderr, "%s: %s\n", argv0, library_error);
            fclose(stream);
            fclose(archive);
            archive_free(private);
            exit(1);
        }

        fclose(stream);
    }

    if (archive_check_duplicates(private) || archive_module->close(private) == -1) {
        fprintf(stderr, "%s: %s\n", argv0, library_error);
        fclose(archive);
        archive_free(private);
        return 1;
    }

    archive_free(private);
    fclose(archive);

    return 0;
}
