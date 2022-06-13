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
#ifndef THTK_DETECT_H_
#define THTK_DETECT_H_

#ifndef THTK_EXPORT
#define THTK_EXPORT /* */
#endif

#include <inttypes.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Detect table is guaranteed to be grouped by variant */
typedef struct {
    unsigned int variant; /* the smallest version which is equvivalent to alias */
    unsigned int alias;
    const char* filename; /* NULL if there are multiple filenames, or if filename is non-ascii */
} thdat_detect_entry_t;

/* Detects version of archive, based on its filename
 *
 * Returns -1 if unconclusive. */
THTK_EXPORT int thdat_detect_filename(
    const char* filename);

#ifdef _WIN32
THTK_EXPORT int thdat_detect_filename_w(
    const wchar_t* filename);
#endif

/* Detects version of archive.
 *
 * Returns 0 if successful, and -1 if io error happened (check thtk_error_t)
 *
 * Filename heur is optional, use NULL as filename to disable it.
 *
 * out is bits corresponding to entries in detect_table.
 * Note that out array is only based on file contents.
 *
 * heur is set in following cases:
 * - Only one bit of out is set
 * - Filename heuristic matches one of the bits
 * - All entries specified in out have the same variant
 *   (in that case heur is set to variant number)
 * Otherwise it is set to -1, and the detection shall be considered unconclusive */
THTK_EXPORT int thdat_detect(
    const char* filename,
    thtk_io_t* input,
    uint32_t out[4],
    unsigned int *heur,
    thtk_error_t** error);

#ifdef _WIN32
THTK_EXPORT int thdat_detect_w(
    const wchar_t* filename,
    thtk_io_t* input,
    uint32_t out[4],
    unsigned int *heur,
    thtk_error_t** error);
#endif

/* Iterate through detect bits
 *
 * Returns NULL when no bits left, otherwise returns entry from detect table
 *
 * Note that it will modify the out array.
 *
 * Usage: while((ent = thdat_detect_iter(out))) ...; */
THTK_EXPORT const thdat_detect_entry_t* thdat_detect_iter(
    uint32_t out[4]);
#ifdef __cplusplus
}
#endif

#endif
