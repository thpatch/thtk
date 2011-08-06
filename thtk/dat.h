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
#ifndef THTK_DAT_H_
#define THTK_DAT_H_

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <thtk/error.h>
#include <thtk/io.h>

#ifndef API_SYMBOL
#define API_SYMBOL /* */
#endif

typedef struct thdat_t thdat_t;

/* Opens an existing archive file read from the input stream.  The stream has
 * its reading position reset to zero.
 *
 * This function generally sets the entry names, along with other fields
 * depending on the format.  Fields not set here are set by
 * thdat_entry_read_data.
 *
 * A new thdat_t object is returned on success, NULL indicates an error. */
API_SYMBOL thdat_t* thdat_open(
    unsigned int version,
    thtk_io_t* input,
    thtk_error_t** error);

/* Creates an archive with entry_count empty entries.
 *
 * The stream has its reading position reset to zero before writing starts.
 * NULL indicates an error. */
API_SYMBOL thdat_t* thdat_create(
    unsigned int version,
    thtk_io_t* output,
    size_t entry_count,
    thtk_error_t** error);

/* Writes out the final pieces of data for a created archive.  The stream is
 * not closed.  0 indicates an error. */
API_SYMBOL int thdat_close(
    thdat_t* thdat,
    thtk_error_t** error);

/* Frees the memory associated with the archive, but doesn't close the stream. */
API_SYMBOL void thdat_free(
    thdat_t* thdat);

/* Returns the number of entries in in the archive.  -1 indicates an error. */
API_SYMBOL ssize_t thdat_entry_count(
    thdat_t* thdat,
    thtk_error_t** error);

/* Returns the index of the named entry.  -1 indicates an error. */
API_SYMBOL ssize_t thdat_entry_by_name(
    thdat_t* thdat,
    const char* name,
    thtk_error_t** error);

/* Sets the name of an entry, names are limited to 256 characters at most, and
 * less for certain formats.  0 indicates an error. */
API_SYMBOL int thdat_entry_set_name(
    thdat_t* thdat,
    int entry_index,
    const char* name,
    thtk_error_t** error);

/* Returns the entry's name.  NULL indicates an error. */
API_SYMBOL const char* thdat_entry_get_name(
    thdat_t* thdat,
    int entry_index,
    thtk_error_t** error);

/* Returns the size of the entry's uncompressed data.  -1 indicates an error. */
API_SYMBOL ssize_t thdat_entry_get_size(
    thdat_t* thdat,
    int entry_index,
    thtk_error_t** error);

/* Returns the size of the entry's compressed data.  -1 indicates an error. */
API_SYMBOL ssize_t thdat_entry_get_zsize(
    thdat_t* thdat,
    int entry_index,
    thtk_error_t** error);

/* TODO: Make sure functions implement these specs. */
/* Reads no more bytes than the limit from the input stream, converts the data
 * as needed, and writes it to the archive's current offset using the specified
 * index.  The number of bytes read from the input stream is returned.  -1
 * indicates an error. */
API_SYMBOL ssize_t thdat_entry_write_data(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* input,
    size_t input_length,
    thtk_error_t** error);

/* Reads all the data for the specified entry, converts it to its uncompressed
 * form, and writes all of it to output.  The number of bytes written to the
 * output stream is returned.  -1 indicates an error. */
API_SYMBOL ssize_t thdat_entry_read_data(
    thdat_t* thdat,
    int entry_index,
    thtk_io_t* output,
    thtk_error_t** error);

#ifdef __cplusplus
}
#endif

#endif
