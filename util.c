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
#include <errno.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#include "program.h"
#include "util.h"

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

void
util_basename(
    char* dst,
    size_t dstlen,
    const char* src)
{
#ifdef WIN32
    char filename[_MAX_FNAME];
    char ext[_MAX_EXT];
    _splitpath(src, NULL, NULL, filename, ext);
    snprintf(dst, dstlen, "%s%s", filename, ext);
#else
    char* tmp = strdup(src);
    strncpy(dst, basename(tmp), dstlen);
    free(tmp);
#endif
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

#ifdef WIN32
        if (CreateDirectory(name, NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS) {
            fprintf(stderr, "%s: couldn't create directory %s\n",
                argv0, name);
            abort();
        }
#else
        if (mkdir(name, 0755) == -1
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

#ifndef HAVE_MEMPCPY
void*
mempcpy(
    void* dest,
    const void* src,
    size_t n)
{
    memcpy(dest, src, n);
    return (void*)((size_t)dest + n);
}
#endif

void
util_xor(
    unsigned char* data,
    size_t data_length,
    unsigned char key,
    unsigned char step,
    unsigned char step2)
{
    size_t i;

    for (i = 0; i < data_length; ++i) {
        const int ip = i - 1;
        data[i] ^= key + i * step + (ip * ip + ip) / 2 * step2;
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
util_strpcmp(
    const void* a,
    const void* b)
{
    return strcmp(*(const char**)a, *(const char**)b);
}
