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
#include <stdarg.h>
#include <thtk/error.h>

struct thtk_error_t {
    char* message;
};

void
thtk_error_func_new(
    thtk_error_t** error,
    const char* function,
    const char* message,
    ...)
{
    va_list ap;
    int funlen, msglen;
    char *buf;
    if (error) {
        funlen = strlen(function);
        va_start(ap, message);
        msglen = vsnprintf(NULL, 0, message, ap);
        va_end(ap);

        buf = malloc(funlen + 2 + msglen + 1);
        sprintf(buf, "%s: ", function);
        va_start(ap, message);
        vsnprintf(buf + funlen + 2, msglen + 1, message, ap);
        va_end(ap);

        *error = malloc(sizeof(**error));
        (*error)->message = buf;
    }
}

const char*
thtk_error_message(
    thtk_error_t* error)
{
    if (!error)
        return "(error is NULL)";
    return error->message;
}

void
thtk_error_free(
    thtk_error_t** error)
{
    if (error && *error)
        free((*error)->message);
    if (error) {
        free(*error);
        *error = NULL;
    }
}
