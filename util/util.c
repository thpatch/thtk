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
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(HAVE_FSTAT) && defined(HAVE_SCANDIR)
#include <sys/stat.h>
#include <dirent.h>
#else
#error "port util_scan_files"
#endif
#if defined(HAVE_CHDIR)
#include <unistd.h>
#define _chdir chdir
#elif defined(HAVE__CHDIR)
#include <direct.h>
#else
#error "port util_chdir"
#endif
#include "program.h"
#include "util.h"

int
util_strcmp_ref(
    const char *str,
    const stringref_t ref)
{
    return strncmp(str, ref.str, ref.len);
}

void*
util_malloc(
    size_t size)
{
    void* ret = malloc(size);
    if (!ret) {
        fprintf(stderr, "%s: allocation of %zu bytes failed: %s\n",
            argv0, size, strerror(errno));
        abort();
    }
    return ret;
}

void
util_print_version(
    void)
{
    printf(PACKAGE_NAME " release " PACKAGE_VERSION "\n");
}

/* XXX: Win32 has MakeSureDirectoryPathExists in dbghelp.dll. */
void
util_makepath(
    const char* path)
{
    char* name;
    char* filename;

    /* TODO: Implement something like this for Windows. */
    if (!path || path[0] == '/') {
        fprintf(stderr, "%s: invalid path: %s\n", argv0, path);
        abort();
    }

    name = strdup(path);
    filename = name;

    for (;;) {
        filename = strchr(filename, '/');
        if (!filename)
            break;
        *filename = '\0';

#ifdef _WIN32
        if (CreateDirectory(name, NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS) {
            fprintf(stderr, "%s: couldn't create directory %s\n",
                argv0, name);
            abort();
        }
#else
        if (mkdir(name, 0777) == -1
#ifdef EEXIST
            && errno != EEXIST
#endif
            ) {
            fprintf(stderr, "%s: couldn't create directory %s: %s\n",
                argv0, name, strerror(errno));
            abort();
        }
#endif

        *filename = '/';
        ++filename;
    }

    free(name);
}

/* Typical amortized O(1) dynamic array resize function */
int
util_vec_ensure(
    void *data, /* It's actually void **. This is done to avoid casts. */
    size_t *cap,
    size_t size,
    size_t element_size)
{
    if (size <= *cap)
        return 0;

    size_t ncap = *cap ? *cap : 1;
    while (ncap < size) {
        ncap <<= 1;
        if (ncap < *cap) /* overflow check */
            ncap = size;
    }
    if (element_size > SIZE_MAX/ncap) /* overflow check */
        return -1;

    void *ndata = realloc(*(void **)data, ncap*element_size);
    if (!ndata)
        return -1;

    *(void **)data = ndata;
    *cap = ncap;
    return 0;
}

#ifdef _WIN32
int
util_scan_files(
    const char* dir,
    char*** result)
{
    WIN32_FIND_DATA wfd;
    HANDLE h;

    char* search_query = malloc(strlen(dir)+3);
    strcpy(search_query, dir);
    char t = dir[strlen(dir)-1];
    if (t != '/' && t != '\\')
        strcat(search_query, "/");
    strcat(search_query, "*");
    h = FindFirstFile(search_query, &wfd);
    if (h == INVALID_HANDLE_VALUE) {
        free(search_query);
        return -1;
    }
    search_query[strlen(search_query)-1] = 0;

    char** filelist = NULL;
    size_t size = 0, capacity = 0;
    if (util_vec_ensure(&filelist, &capacity, 8, sizeof(char*)))
        return -1;

    BOOL bResult = TRUE;
    while (bResult) {
        const char* name = wfd.cFileName;
        char* fullpath = malloc(strlen(search_query)+strlen(name)+3);
        strcpy(fullpath, dir);
        t = dir[strlen(dir)-1];
        if (t != '/' || t != '\\') strcat(fullpath, "/");
        strcat(fullpath, name);
        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char** subdirs;
            // Ignore ".", "..", or hidden files
            if (wfd.cFileName[0] == '.') {
                bResult = FindNextFile(h, &wfd);
                continue;
            }
            t = name[strlen(name)-1];
            if (t != '/' || t != '\\') strcat(fullpath, "/");
            int new_cnt = util_scan_files(fullpath, &subdirs);
            for (int j = 0; j < new_cnt; j++) {
                filelist[size] = malloc(strlen(subdirs[j])+1);
                strcpy(filelist[size++], subdirs[j]);
                free(subdirs[j]);

                if (util_vec_ensure(&filelist, &capacity, size+1, sizeof(char*)))
                    goto err;
            }
            free(subdirs);
            continue;
        }
        filelist[size++] = fullpath;
        if (util_vec_ensure(&filelist, &capacity, size+1, sizeof(char*)))
            goto err;

        bResult = FindNextFile(h, &wfd);
    }
    FindClose(h);

    *result = filelist;
    return size;
err:
    if (filelist) {
        while (size--)
            free(filelist[size]);
        free(filelist);
    }
    return -1;
}
#else
static int
util_scan_files_comp(
    const struct dirent* file)
{
    // Ignore ".", "..", or hidden files
    if (file->d_name[0] == '.')
        return 0;
    return 1;
}

int
util_scan_files(
    const char* dir,
    char*** result)
{
    struct stat stat_buf;
    if (stat(dir, &stat_buf) == -1)
        return -1;
    // We can only scan directories
    if (!S_ISDIR(stat_buf.st_mode))
        return -1;

    struct dirent **files;
    int n;
    n = scandir(dir, &files, util_scan_files_comp, alphasort);
    if (n < 0)
        return -1;

    char** filelist = NULL;
    size_t size = 0, capacity = 0;
    if (util_vec_ensure(&filelist, &capacity, 8, sizeof(char*)))
        return -1;
    for (int i = 0; i < n; ++i) {
        const char* name = files[i]->d_name;
        char* fullpath = malloc(strlen(dir)+strlen(name)+3);
        strcpy(fullpath, dir);
        if (dir[strlen(dir)-1] != '/') strcat(fullpath, "/");
        strcat(fullpath, name);
        if (stat(fullpath, &stat_buf) == -1) {
            free(files[i]);
            free(fullpath);
            continue;
        }
        if (S_ISDIR(stat_buf.st_mode)) {
            char** subdirs;
            if (name[strlen(name)-1] != '/') strcat(fullpath, "/");
            int new_cnt = util_scan_files(fullpath, &subdirs);
            for (int j = 0; j < new_cnt; j++) {
                filelist[size] = malloc(strlen(subdirs[j])+1);
                strcpy(filelist[size++], subdirs[j]);
                free(subdirs[j]);

                if (util_vec_ensure(&filelist, &capacity, size+1, sizeof(char*)))
                    goto err;
            }
            free(subdirs);
            free(files[i]);
            free(fullpath);
            continue;
        }
        filelist[size++] = fullpath;
        if (util_vec_ensure(&filelist, &capacity, size+1, sizeof(char*)))
            goto err;
        free(files[i]);
    }
    *result = filelist;
    return size;
err:
    if (filelist) {
        while (size--)
            free(filelist[size]);
        free(filelist);
    }
    return -1;
}
#endif // _WIN32

void
util_xor(
    unsigned char* data,
    size_t size,
    unsigned char key,
    unsigned char step1,
    unsigned char step2)
{
    while (size-- > 0) {
        *data++ ^= key;
        key += step1;
        step1 += step2;
    }
}

const char*
util_printfloat(
    const void* data)
{
    static char buf[256];
    float f;
    unsigned int i;

    memcpy(&f, data, sizeof(float));

    for (i = 1; i < 50; ++i) {
        snprintf(buf, 256, "%.*f", i, f);

#ifdef HAVE_STRTOF
        if (f == strtof(buf, NULL))
            break;
#else
        {
            float g = 0.0f;
            if (sscanf(buf, "%f", &g) == 1 && f == g)
                break;
        }
#endif
    }

    /* XXX: What about errors? */

    return buf;
}

int
util_chdir(
    const char *path)
{
    return _chdir(path);
}
