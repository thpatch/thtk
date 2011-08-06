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
#include <thtk/thtk.h>
#include "program.h"
#include "util.h"

static void
print_usage(
    void)
{
    printf("Usage: %s COMMAND[OPTION...] [ARCHIVE [FILE...]]\n"
           "COMMAND can be:\n"
           "  c  create an archive\n"
           "  l  list the contents of an archive\n"
           "  x  extract an archive\n"
           "  V  display version information and exit\n"
           "OPTION can be:\n"
           "  #  # can be 2, 3, 4, 5, 6, 7, 8, 9, 95, 10, 11, 12, 125, 128, or 13 defaults to the latest\n\n"
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n", argv0);
}

static void
print_error(
    thtk_error_t* error)
{
    fprintf(stderr, "%s:%s\n", argv0, thtk_error_message(error));
}

typedef struct {
    thdat_t* thdat;
    thtk_io_t* stream;
} thdat_state_t;

static thdat_state_t*
thdat_state_alloc(void)
{
    thdat_state_t* state = malloc(sizeof(*state));
    state->thdat = NULL;
    state->stream = NULL;
    return state;
}

static void
thdat_state_free(
    thdat_state_t* state)
{
    if (state) {
        if (state->thdat)
            thdat_free(state->thdat);
        if (state->stream)
            thtk_io_close(state->stream);
        free(state);
    }
}

static thdat_state_t*
thdat_open_file(
    unsigned int version,
    const char* path,
    thtk_error_t** error)
{
    thdat_state_t* state = thdat_state_alloc();

    if (!(state->stream = thtk_io_open_file(path, "rb", error))) {
        thdat_state_free(state);
        return NULL;
    }

    if (!(state->thdat = thdat_open(version, state->stream, error))) { 
        thdat_state_free(state);
        return NULL;
    }

    return state;
}

static int
thdat_extract_file(
    thdat_state_t* state,
    size_t entry_index,
    thtk_error_t** error)
{
    const char* entry_name;
    thtk_io_t* entry_stream;

    if (!(entry_name = thdat_entry_get_name(state->thdat, entry_index, error)))
        return 0;

    if (!(entry_stream = thtk_io_open_file(entry_name, "wb", error)))
        return 0;

    if (thdat_entry_read_data(state->thdat, entry_index, entry_stream, error) == -1) {
        thtk_io_close(entry_stream);
        return 0;
    }

    printf("%s\n", entry_name);

    thtk_io_close(entry_stream);

    return 1;
}

static int
thdat_list(
    unsigned int version,
    const char* path,
    thtk_error_t** error)
{
    thdat_state_t* state = thdat_open_file(version, path, error);
    ssize_t entry_count;
    struct {
        const char* name;
        ssize_t size;
        ssize_t zsize;
    }* entries;
    ssize_t e;
    int name_width = 4;

    if ((entry_count = thdat_entry_count(state->thdat, error)) == -1) {
        thdat_state_free(state);
        return 0;
    }

    entries = malloc(entry_count * sizeof(*entries));

#pragma omp parallel /* reduction(max:name_width) */
    {
#pragma omp for
        for (e = 0; e < entry_count; ++e) {
            thtk_error_t* error = NULL;
            entries[e].name = thdat_entry_get_name(state->thdat, e, &error);
            entries[e].size = thdat_entry_get_size(state->thdat, e, &error);
            entries[e].zsize = thdat_entry_get_zsize(state->thdat, e, &error);
            if (!entries[e].name || entries[e].size == -1 || entries[e].zsize == -1) {
                print_error(error);
                thtk_error_free(&error);
                continue;
            }
            int entry_name_width = strlen(entries[e].name);
#pragma omp critical
            if (entry_name_width > name_width)
                name_width = entry_name_width;
        }
    }

    printf("%-*s  %7s  %7s\n", name_width, "Name", "Size", "Stored");
    for (e = 0; e < entry_count; ++e) {
        printf("%-*s  %7zd  %7zd\n", name_width, entries[e].name, entries[e].size, entries[e].zsize);
    }

    free(entries);
    thdat_state_free(state);

    return 1;
}

static int
thdat_create_wrapper(
    unsigned int version,
    const char* path,
    const char** paths,
    size_t entry_count,
    thtk_error_t** error)
{
    thdat_state_t* state = thdat_state_alloc();

    if (!(state->stream = thtk_io_open_file(path, "wb", error))) {
        thdat_state_free(state);
        exit(1);
    }

    if (!(state->thdat = thdat_create(version, state->stream, entry_count, error))) { 
        thdat_state_free(state);
        exit(1);
    }

    /* TODO: Properly indicate when insertion fails. */
#pragma omp parallel for
    for (size_t i = 0; i < entry_count; ++i) {
        thtk_error_t* error = NULL;
        thtk_io_t* entry_stream;
        off_t entry_size;

        printf("%s...\n", paths[i]);

        if (!(entry_stream = thtk_io_open_file(paths[i], "rb", &error))) {
            print_error(error);
            thtk_error_free(&error);
            continue;
        }

        if ((entry_size = thtk_io_seek(entry_stream, 0, SEEK_END, &error)) == -1) {
            print_error(error);
            thtk_error_free(&error);
            continue;
        }

        if (thtk_io_seek(entry_stream, 0, SEEK_SET, &error) == -1) {
            print_error(error);
            thtk_error_free(&error);
            continue;
        }

        if (!thdat_entry_set_name(state->thdat, i, paths[i], &error)) {
            print_error(error);
            thtk_error_free(&error);
            continue;
        }

        if (thdat_entry_write_data(state->thdat, i, entry_stream, entry_size, &error) == -1) {
            print_error(error);
            thtk_error_free(&error);
            continue;
        }

        thtk_io_close(entry_stream);
    }

    int ret = 1;

    if (!thdat_close(state->thdat, error))
        ret = 0;

    thdat_state_free(state);

    return ret;
}

/* TODO: Make sure errors are printed in all cases. */
int
main(
    int argc,
    char* argv[])
{
    thtk_error_t* error = NULL;
    unsigned int version = 13;
    int mode;

    if (!(mode = parse_args(argc, argv, print_usage, "clxV", "", &version)))
        exit(1);

    switch (mode) {
    case 'V':
        util_print_version();
        exit(0);
    case 'l': {
        if (argc < 3) {
            print_usage();
            exit(1);
        }

        if (!thdat_list(version, argv[2], &error)) {
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }

        exit(0);
    }
    case 'c': {
        if (argc < 4) {
            print_usage();
            exit(1);
        }

        if (!thdat_create_wrapper(version, argv[2], (const char**)&argv[3], argc - 3, &error)) {
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }

        exit(0);
    }
    case 'x': {
        if (argc < 3) {
            print_usage();
            exit(1);
        }

        thdat_state_t* state = thdat_open_file(version, argv[2], &error);
        if (!state) {
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }

        if (argc > 3) {
#pragma omp parallel for
            for (int a = 3; a < argc; ++a) {
                thtk_error_t* error = NULL;
                int entry_index;

                if ((entry_index = thdat_entry_by_name(state->thdat, argv[a], &error)) == -1) {
                    print_error(error);
                    thtk_error_free(&error);
                    continue;
                }

                if (!thdat_extract_file(state, entry_index, &error)) {
                    print_error(error);
                    thtk_error_free(&error);
                    continue;
                }
            }
        } else {
            ssize_t entry_count;
            if ((entry_count = thdat_entry_count(state->thdat, &error)) == -1) {
                print_error(error);
                thtk_error_free(&error);
                exit(1);
            }

#pragma omp parallel for
            for (ssize_t entry_index = 0; entry_index < entry_count; ++entry_index) {
                thtk_error_t* error = NULL;
                if (!thdat_extract_file(state, entry_index, &error)) {
                    print_error(error);
                    thtk_error_free(&error);
                    continue;
                }
            }
        }

        thdat_state_free(state);
        exit(0);
    }
    default:
        exit(1);
    }
}
