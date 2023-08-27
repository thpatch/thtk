/* This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 **/
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
