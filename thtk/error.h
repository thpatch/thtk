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
#ifndef THTK_ERROR_H_
#define THTK_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef API_SYMBOL
#define API_SYMBOL /* */
#endif

typedef struct thtk_error_t thtk_error_t;

/* Creates a new error for the current function. */
#define thtk_error_new(error, ...) thtk_error_func_new(error, __func__, __VA_ARGS__)

/* Stores a pointer to an error structure if error is not NULL. */
API_SYMBOL void thtk_error_func_new(
    thtk_error_t** error,
    const char* function,
    const char* message,
    ...);

/* Extracts the message from an error structure. */
API_SYMBOL const char* thtk_error_message(
    thtk_error_t* error);

/* Frees the error structure and clears the pointer. */
API_SYMBOL void thtk_error_free(
    thtk_error_t** error);

#ifdef __cplusplus
}
#endif

#endif
