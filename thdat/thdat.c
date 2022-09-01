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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thtk/thtk.h>
#include "program.h"
#include "util.h"
#include "mygetopt.h"

static const char *dat_chdir = NULL;

static void
print_usage(
    void)
{
    printf("Usage: %s [-Vg] [-C DIR] [[-c | -l | -x] VERSION] [ARCHIVE [FILE...]]\n"
           "Options:\n"
           "  -c  create an archive\n"
           "  -l  list the contents of an archive\n"
           "  -x  extract an archive\n"
           "  -V  display version information and exit\n"
           "  -g  enable glob matching for -x filenames\n"
           "  -C  change directory after opening the archive\n"
           "VERSION can be:\n"
           "  1, 2, 3, 4, 5, 6, 7, 8, 9, 95, 10, 103 (for Uwabami Breakers), 105, 11, 12, 123, 125, 128, 13, 14, 143, 15, 16, 165, 17, 18 or 185\n"
           /* NEWHU: 185 */
       "Specify 'd' as VERSION to automatically detect archive format. (-l and -x only)\n\n"
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

    // For th105: Make sure that the directory exists
    util_makepath(entry_name);

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
    if(!state) {
        return 0;
    }
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

    // th105: Stored = Size
    if (version == 105 || version == 123)
        printf("%-*s  %7s\n", name_width, "Name", "Size");
    else
        printf("%-*s  %7s  %7s\n", name_width, "Name", "Size", "Stored");
    for (e = 0; e < entry_count; ++e) {
        if (version == 105 || version == 123)
            printf("%-*s  %7zd\n", name_width, entries[e].name, entries[e].size);
        else
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
    char*** entries = calloc(entry_count, sizeof(char**));
    char** realpaths;
    int* entries_count = calloc(entry_count, sizeof(int));
    size_t real_entry_count = 0;

    if (!(state->stream = thtk_io_open_file(path, "wb", error))) {
        thdat_state_free(state);
        exit(1);
    }

    if (dat_chdir && util_chdir(dat_chdir) == -1) {
        fprintf(stderr, "%s: couldn't change directory to %s: %s\n",
            argv0, dat_chdir, strerror(errno));
        thdat_state_free(state);
        exit(1);
    }

    for (size_t i = 0; i < entry_count; i++) {
        int n = util_scan_files(paths[i], &entries[i]);
        if (n == -1) {
            entries[i] = calloc(1, sizeof(char*));
            entries[i][0] = malloc(strlen(paths[i])+1);
            strcpy(entries[i][0], paths[i]);
            n = 1;
        }
        entries_count[i] = n;
        real_entry_count += n;
    }

    if (!(state->thdat = thdat_create(version, state->stream, real_entry_count, error))) {
        thdat_state_free(state);
        exit(1);
    }

    // Set entry names first...
    realpaths = calloc(real_entry_count, sizeof(char*));
    size_t k = 0;
    for (size_t i = 0; i < entry_count; ++i) {
        thtk_error_t* error = NULL;
        for (size_t j = 0; j < entries_count[i]; j++) {
            if (!thdat_entry_set_name(state->thdat, k, entries[i][j], &error)) {
                print_error(error);
                thtk_error_free(&error);
                continue;
            }
            realpaths[k] = malloc(strlen(entries[i][j])+1);
            strcpy(realpaths[k], entries[i][j]);
            k++;
            free(entries[i][j]);
        }
        free(entries[i]);
    }
    free(entries);
    free(entries_count);
    // ...and then module->create, if this is th105 archive.
    // This is because the list of entries comes first in th105 archives.
    if (version == 105 || version == 123) {
        if (!thdat_init(state->thdat, error))
        {
            thdat_state_free(state);
            exit(1);
        }
    }

    k = 0;
    /* TODO: Properly indicate when insertion fails. */
    ssize_t i;
#pragma omp parallel for schedule(dynamic)
    for (i = 0; i < real_entry_count; ++i) {
        thtk_error_t* error = NULL;
        thtk_io_t* entry_stream;
        off_t entry_size;

        printf("%s...\n", thdat_entry_get_name(state->thdat, i, &error));

        // Is entry name set?
        if (!(thdat_entry_get_name(state->thdat, i, &error))[0])
            continue;

        if (!(entry_stream = thtk_io_open_file(realpaths[i], "rb", &error))) {
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

        if (thdat_entry_write_data(state->thdat, i, entry_stream, entry_size, &error) == -1) {
            print_error(error);
            thtk_error_free(&error);
            continue;
        }

        thtk_io_close(entry_stream);
        free(realpaths[i]);
    }
    free(realpaths);

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
    unsigned int version = 0;
    int mode = -1;
    int dat_use_glob = 0;

    argv0 = util_shortname(argv[0]);
    int opt;
    int ind=0;
    while(argv[util_optind]) {
        switch(opt = util_getopt(argc, argv, ":c:l:x:VdgC:")) {
        case 'c':
        case 'l':
        case 'x':
        case 'd':
            if(mode != -1) {
                fprintf(stderr,"%s: More than one mode specified\n",argv0);
                print_usage();
                exit(1);
            }
            mode = opt;
            if((opt == 'x' || mode == 'l') && *util_optarg == 'd') {
                version = ~0;
            }
            else if(opt != 'd') version = parse_version(util_optarg);
            break;
        case 'g':
            dat_use_glob = 1;
            break;
        case 'C':
            dat_chdir = util_optarg;
            break;
        default:
            util_getopt_default(&ind,argv,opt,print_usage);
        }
    }
    argc = ind;
    argv[argc] = NULL;

    /* detect version */
    if(argc && (mode == 'x' || mode == 'l') && version == ~0) {
        thtk_io_t* file;
        if(!(file = thtk_io_open_file(argv[0], "rb", &error))) {
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }
        uint32_t out[4];
        unsigned int heur;
        printf("Detecting '%s'...\n",argv[0]);
        if(-1 == thdat_detect(argv[0], file, out, &heur, &error)) {
            thtk_io_close(file);
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }
        if(heur == -1) {
            const thdat_detect_entry_t* ent;
            printf("Couldn't detect version!\nPossible versions: ");
            while((ent = thdat_detect_iter(out))) {
                printf("%d,",ent->alias);
            }
            printf("\n");
            thtk_io_close(file);
            exit(1);
        }
        else {
            printf("Detected version %d\n",heur);
            version = heur;
        }
        thtk_io_close(file);
    }

    switch (mode) {
    case 'd': {
        if (argc < 1) {
            print_usage();
            exit(1);
        }
        for (int i = 0; i < argc; i++) {
            thtk_io_t* file;
            if (!(file = thtk_io_open_file(argv[i], "rb", &error))) {
                print_error(error);
                thtk_error_free(&error);
                exit(1);
            }

            uint32_t out[4];
            unsigned int heur;

            printf("Detecting '%s'... ",argv[i]);
            if (-1 == thdat_detect(argv[i], file, out, &heur, &error)) {
                printf("\n");
                thtk_io_close(file);
                print_error(error);
                thtk_error_free(&error);
                continue;
            }

            const thdat_detect_entry_t* ent;
            printf("%d | possible versions: ", heur);
            while((ent = thdat_detect_iter(out))) {
                printf("%d,",ent->alias);
            }
            printf(" | filename: %d\n", thdat_detect_filename(argv[i]));
            thtk_io_close(file);
        }
        exit(0);
    }
    case 'l': {
        if (argc < 1) {
            print_usage();
            exit(1);
        }

        if (!thdat_list(version, argv[0], &error)) {
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }

        exit(0);
    }
    case 'c': {
        if (argc < 2) {
            print_usage();
            exit(1);
        }

        if (!thdat_create_wrapper(version, argv[0], (const char**)&argv[1], argc - 1, &error)) {
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }

        exit(0);
    }
    case 'x': {
        if (argc < 1) {
            print_usage();
            exit(1);
        }

        thdat_state_t* state = thdat_open_file(version, argv[0], &error);
        if (!state) {
            print_error(error);
            thtk_error_free(&error);
            exit(1);
        }

        if (dat_chdir && util_chdir(dat_chdir) == -1) {
            fprintf(stderr, "%s: couldn't change directory to %s: %s\n",
                argv0, dat_chdir, strerror(errno));
            exit(1);
        }

        if (argc > 1) {
            ssize_t a;
#pragma omp parallel for schedule(dynamic)
            for (a = 1; a < argc; ++a) {
                thtk_error_t* error = NULL;
                int e = -1;

                if (dat_use_glob) {
                    for (;;) {
                        if ((e = thdat_entry_by_glob(state->thdat, argv[a], e+1, &error)) == -1) {
                            if (error) {
                                print_error(error);
                                thtk_error_free(&error);
                            }
                            break;
                        }
                        if (!thdat_extract_file(state, e, &error)) {
                            print_error(error);
                            thtk_error_free(&error);
                        }
                    }
                } else {
                    if ((e = thdat_entry_by_name(state->thdat, argv[a], &error)) == -1) {
                        if (error) {
                            print_error(error);
                            thtk_error_free(&error);
                        } else {
                            fprintf(stderr, "%s:%s not found\n", argv0, argv[a]);
                        }
                        continue;
                    }
                    if (!thdat_extract_file(state, e, &error)) {
                        print_error(error);
                        thtk_error_free(&error);
                    }
                }
            }
        } else {
            ssize_t entry_count;
            if ((entry_count = thdat_entry_count(state->thdat, &error)) == -1) {
                print_error(error);
                thtk_error_free(&error);
                exit(1);
            }

            ssize_t entry_index;
#pragma omp parallel for schedule(dynamic)
            for (entry_index = 0; entry_index < entry_count; ++entry_index) {
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
    print_usage();
        exit(1);
    }
}
