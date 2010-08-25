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
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#include "program.h"
#include "util.h"

void
util_print_version(
    const char* name,
    const char* version)
{
    printf("%s %s (part of " PACKAGE_NAME " release " PACKAGE_VERSION ")\n"
#ifdef PACKAGE_URL
           "The latest version is available at <" PACKAGE_URL ">\n",
#endif
           name, version);
}

int
util_seek(
    FILE* stream,
    long offset)
{
    if (fseek(stream, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed seeking to %lu: %s\n",
            argv0, offset, strerror(errno));
        return 0;
    } else
        return 1;
}

int
util_seekable(
    FILE* stream)
{
    return ftell(stream) != -1;
}

long
util_tell(
    FILE* stream)
{
    long pos = ftell(stream);
    if (pos == -1)
        fprintf(stderr, "%s: ftell failed: %s\n", argv0, strerror(errno));
    return pos;
}

/* Might be better to use stat, but it's probably less cross-platform. */
/* This will fail for 2GB+ files, but who cares. */
/* TODO: Implement stat() version. */
long
util_fsize(
    FILE* stream)
{
    long prev, end;

    if ((prev = util_tell(stream)) == -1)
        return -1;

    if (fseek(stream, 0, SEEK_END) == -1) {
        fprintf(stderr, "%s: failed seeking to end: %s\n",
            argv0, strerror(errno));
        return -1;
    }

    if ((end = util_tell(stream)) == -1)
        return -1;

    if (!util_seek(stream, prev))
        return -1;

    return end;
}

int
util_read(
    FILE* stream,
    void* buffer,
    size_t size)
{
    if (fread_unlocked(buffer, size, 1, stream) != 1 && size != 0) {
        if (feof_unlocked(stream)) {
            fprintf(stderr,
                "%s: failed reading %lu bytes: unexpected end of file\n",
                argv0, (long unsigned int)size);
        } else {
            fprintf(stderr, "%s: failed reading %lu bytes: %s\n",
                argv0, (long unsigned int)size, strerror(errno));
        }
        return 0;
    } else
        return 1;
}

int
util_write(
    FILE* stream,
    const void* buffer,
    size_t size)
{
    if (fwrite_unlocked(buffer, size, 1, stream) != 1 && size != 0) {
        fprintf(stderr, "%s: failed writing %lu bytes: %s\n",
            argv0, (long unsigned int)size, strerror(errno));
        return 0;
    } else
        return 1;
}

ssize_t
util_read_asciiz(
    char* buffer,
    size_t buffersize,
    FILE* stream)
{
    unsigned int i;
    int c;
    for (i = 0; i < buffersize; ++i) {
        if (i == buffersize - 1) {
            return -1;
        }
        c = fgetc_unlocked(stream);
        if (c == EOF) {
            return -1;
        }
        buffer[i] = c;
        if (!c)
            break;
    }
    return i + 1;
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
    _snprintf(dst, dstlen, "%s%s", filename, ext);
#else
    char* tmp = strdup(src);
    strncpy(dst, basename(tmp), dstlen);
    free(tmp);
#endif
}

/* TODO: Use util_basename if it can be made to return a pointer to inside of
 * path. */
const char*
util_shortname(
    const char* path)
{
    const char* ret;
    if (!path) {
        fprintf(stderr, "%s: NULL path passed to short_name\n", argv0);
        abort();
    }
#ifdef WIN32
    ret = MAX(strrchr(path, '/'), strrchr(path, '\\'));
#else
    ret = strrchr(path, '/');
#endif
    return ret ? ret + 1 : path;
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
        if (
#ifdef WIN32
            mkdir(name)
#else
            mkdir(name, 0755)
#endif
            == -1
#ifdef EEXIST
            && errno != EEXIST
#endif
            ) {
            fprintf(stderr, "%s: couldn't create directory %s: %s\n",
                argv0, name, strerror(errno));
            abort();
        }

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
util_sillyxor(
    const unsigned char* in,
    unsigned char* out,
    int size,
    unsigned char key,
    unsigned char step,
    const unsigned char step2)
{
    const unsigned char* iend = in + size;
    while (in < iend) {
        *out = *in ^ key;
        key += step;
        step += step2;
        in++;
        out++;
    }
}

#ifdef WIN32
unsigned char*
util_iconv(
    const char* to,
    const char* from,
    unsigned char* in,
    size_t insize,
    size_t* outsize)
{
    wchar_t* temp;
    int temp_len;
    unsigned char* out;
    int out_len;
    UINT from_n;
    UINT to_n;

    if (strcmp(from, "CP932") == 0) {
        from_n = 932;
    } else if (strcmp(from, "UTF-8") == 0) {
        from_n = CP_UTF8;
    } else {
        fprintf(stderr, "%s:util_iconv: Unsupported conversion specifier %s\n",
            argv0, from);
        return NULL;
    }

    if (strcmp(to, "CP932") == 0) {
        to_n = 932;
    } else if (strcmp(to, "UTF-8") == 0) {
        to_n = CP_UTF8;
    } else {
        fprintf(stderr, "%s:util_iconv: Unsupported conversion specifier %s\n",
            argv0, to);
        return NULL;
    }

    temp_len = MultiByteToWideChar(from_n, MB_ERR_INVALID_CHARS, (LPCSTR)in,
        insize, NULL, 0);
    if (!temp_len) {
        fprintf(stderr, "%s:MultiByteToWideChar: %lu\n", argv0, GetLastError());
        return NULL;
    }
    temp = malloc(temp_len * sizeof(wchar_t));
    temp_len = MultiByteToWideChar(from_n, MB_ERR_INVALID_CHARS, (LPCSTR)in,
        insize, temp, temp_len);
    if (!temp_len) {
        fprintf(stderr, "%s:MultiByteToWideChar: %lu\n", argv0, GetLastError());
        return NULL;
    }

    free(in);

    out_len = WideCharToMultiByte(to_n, 0, temp, temp_len, NULL, 0, NULL, NULL);
    if (!out_len) {
        fprintf(stderr, "%s:WideCharToMultiByte: %lu\n", argv0, GetLastError());
        return NULL;
    }
    out = malloc(out_len);
    out_len = WideCharToMultiByte(to_n, 0, temp, temp_len, (LPSTR)out, out_len,
        NULL, NULL);
    if (!out_len) {
        fprintf(stderr, "%s:WideCharToMultiByte: %lu\n", argv0, GetLastError());
        return NULL;
    }

    free(temp);

    *outsize = out_len;
    return out;
}
#else
#ifdef HAVE_ICONV_H
unsigned char*
util_iconv(
    const char* to,
    const char* from,
    unsigned char* in,
    size_t insize,
    size_t* outsize)
{
    size_t ret;
    char* inp;
    char* outp;
    size_t outsize2 = insize * 2;
    size_t outsize3 = outsize2;
    char* out = malloc(outsize2);
    iconv_t ic = iconv_open(to, from);

    if (ic == (iconv_t)-1) {
        fprintf(stderr, "%s:iconv_open: %s\n", argv0, strerror(errno));
        return NULL;
    }

    memset(out, 0, outsize2);

    inp = (char*)in;
    outp = out;

    ret = iconv(ic, &inp, &insize, &outp, &outsize2);
    if (ret == (size_t)-1) {
        fprintf(stderr, "%s:iconv: %s\n", argv0, strerror(errno));
        return NULL;
    }

    *outsize = (size_t)(outsize3 - outsize2);
    free(in);

    if (iconv_close(ic) == -1) {
        fprintf(stderr, "%s:iconv_close: %s\n", argv0, strerror(errno));
        return NULL;
    }

    return (unsigned char*)out;
}
#endif
#endif

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
