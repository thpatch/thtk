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
#include "cp932.h"
#include "cp932tab.h"
char *
cp932_to_utf8(
    char *out,
    const char *in)
{
    char *origout = out;
    while (*in) {
        unsigned short w;
        int ch = *in & 0xff;
        if (cp932_inrange1(ch)) {
            w = cp932_range1tab(ch)[in[1] & 0xff];
            in += 2;
        } else if (cp932_inrange2(ch)) {
            w = cp932_range2tab(ch)[in[1] & 0xff];
            in += 2;
        } else {
            w = cp932_to_ucs2_tab[ch];
            in += 1;
        }
        if (w & 0xF800) {
            *out++ = 0xE0 | w>>12;
            *out++ = 0x80 | w>>6 & 0x3F;
            *out++ = 0x80 | w & 0x3F;
        } else if (w & 0x0780) {
            *out++ = 0xC0 | w>>6;
            *out++ = 0x80 | w & 0x3F;
        } else {
            *out++ = w;
        }
    }
    *out++ = 0;
    return origout;
}
size_t cp932_to_utf8_len(
    const char *in)
{
    size_t len = 0;
    while (*in) {
        unsigned short w;
        int ch = *in & 0xff;
        if (cp932_inrange1(ch)) {
            w = cp932_range1tab(ch)[in[1] & 0xff];
            in += 2;
        } else if (cp932_inrange2(ch)) {
            w = cp932_range2tab(ch)[in[1] & 0xff];
            in += 2;
        } else {
            w = cp932_to_ucs2_tab[ch];
            in += 1;
        }
        if (w & 0xF800)
            len += 3;
        else if (w & 0x0780)
            len += 2;
        else
            len += 1;
    }
    return len;
}
#define utf8_cont_byte(ch) ((ch & 0xC0) == 0x80)
char *
utf8_to_cp932(
    char *out,
    const char *in)
{
    char *origout = out;
    while (*in) {
        unsigned short w;
        if (!(*in & 0x80))  {
            w = *in++ & 0xFF;
        } else if (!(*in & 0x40))  {
skip_cont_bytes:
            w = '?';
            while (utf8_cont_byte(*++in))
                ;
        } else if (!(*in & 0x20))  {
            if (!utf8_cont_byte(in[1]))
                goto skip_cont_bytes;
            w = in[0]<<6 & 0x0EC0 | in[1] & 0x003F;
            in += 2;
        } else if (!(*in & 0x10))  {
            if (!utf8_cont_byte(in[1]) || !utf8_cont_byte(in[2]))
                goto skip_cont_bytes;
            w = in[0]<<12 & 0xF000 | in[1]<<6 & 0x0FC0 | in[2] & 0x003F;
            in += 3;
        } else if ((*in & 0xF0) == 0xF0) {
            goto skip_cont_bytes;
        }
        w = ucs2_to_cp932_tab[w];
        if (w & 0xFF00)
            *out++ = w>>8;
        *out++ = w;
    }
    *out++ = 0;
    return origout;
}
size_t utf8_to_cp932_len(
    const char *in)
{
    size_t len = 0;
    while (*in) {
        unsigned short w;
        if (!(*in & 0x80))  {
            w = *in++ & 0xFF;
        } else if (!(*in & 0x40))  {
skip_cont_bytes:
            w = '?';
            while (utf8_cont_byte(*++in))
                ;
        } else if (!(*in & 0x20))  {
            if (!utf8_cont_byte(in[1]))
                goto skip_cont_bytes;
            w = in[0]<<6 & 0x0EC0 | in[1] & 0x003F;
            in += 2;
        } else if (!(*in & 0x10))  {
            if (!utf8_cont_byte(in[1]) || !utf8_cont_byte(in[2]))
                goto skip_cont_bytes;
            w = in[0]<<12 & 0xF000 | in[1]<<6 & 0x0FC0 | in[2] & 0x003F;
            in += 3;
        } else if ((*in & 0xF0) == 0xF0) {
            goto skip_cont_bytes;
        }
        w = ucs2_to_cp932_tab[w];
        if (w & 0xFF00)
            len++;
        len++;
    }
    return len;
}
