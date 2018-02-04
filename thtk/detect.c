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
#include <string.h>
#include <thtk/thtk.h>
#include <stdio.h> /* for SEEK_SET */ 
#include "thcrypt.h"
#include "thlzss.h"

#define DETECT_DEF(x) \
    /* thdat02 */ \
    x(0, 1,1) \
    x(1, 2,2) \
    x(2, 3,3) \
    x(3, 3,4) \
    x(4, 3,5) \
    /* thdat06 */ \
    x(5, 6,6) \
    x(6, 7,7) \
    /* thdat08 */ \
    x(7, 8,8) \
    x(8, 9,9) \
    /* thdat95 */ \
    x(9, 95,95) \
    x(10, 95,10) \
    x(11, 95,11) \
    x(12, 12,12) \
    x(13, 12,125) \
    x(14, 12,128) \
    x(15, 13,13) \
    x(16, 14,14) \
    x(17, 14,143) \
    x(18, 14,15) \
    x(19, 14,16) \
    /* thdat105 */ \
    x(20, 105,105) \
    x(21, 123,123) 
    
static const thdat_detect_entry_t detect_table[] = {
#define x(idx, var, alias) {var,alias},
        DETECT_DEF(x)
        {0,0},       
#undef x
};
#define DETECT_ENTRIES (sizeof(detect_table)/sizeof(detect_table[0]) - 1)

static int detect_ver_to_idx(int ver) {
    switch(ver) {
    default:return -1;
#define x(idx, var, alias) case alias:return idx;
    DETECT_DEF(x)
#undef x
    }
}

#define SET_OUT(x) do { \
   int macrotemp = detect_ver_to_idx(x); \
   out[macrotemp/32] |= 1 << (macrotemp%32); \
} while(0)

int
thdat_detect(
        const char* filename,
	thtk_io_t* input,
	uint32_t out[4],
        unsigned int *heur,
	thtk_error_t** error)
{
    out[0]=out[1]=out[2]=out[3]=0;
    *heur = -1;
    /* TODO: th02dat */
    if(-1 == thtk_io_seek(input, 0, SEEK_SET, error)) {
        return -1;
    }
    
    /* read magic for TSA 06+ */
    char magic[16]; /* 16 for THA1 header */
    if(-1 == thtk_io_read(input, magic, 16, error)) {
        return -1;
    }
    /* th06 */
    if(!memcmp(magic,"PBG3",4))
        SET_OUT(6);
    /* th07*/
    if(!memcmp(magic,"PBG4",4))
        SET_OUT(7);
    /* th08/th09 */
    if(!memcmp(magic,"PBGZ",4)) {
        /* TODO: differentiate */
        SET_OUT(8);
        SET_OUT(9);
    }
    /* th095+ */
    th_decrypt((unsigned char*)magic,16,0x1b,0x37,16,16); /* 16 is sizeof(th95_archive_header_t) */
    if(!memcmp(magic,"THA1",4)) {
        SET_OUT(95);
        SET_OUT(10);
        SET_OUT(11);
        SET_OUT(12);
        SET_OUT(125);
        SET_OUT(128);
        SET_OUT(13);
        SET_OUT(14);
        SET_OUT(143);
        SET_OUT(15);
        SET_OUT(16);
    }
    
    
    /* heur */
    uint32_t out2[4];
    memcpy(out2,out,sizeof(out2));
    const thdat_detect_entry_t* ent;
    /* TODO: filename heur */
    int variant=-1;
    int alias=-1;
    int count = 0;
    while((ent = thdat_detect_iter(out2))) {
        count++;
        alias = ent->alias;
        if(variant == -1) {
            variant = ent->variant;
        }
        else if(variant != ent->variant) {
            variant = -1;
            break;
        }
    }
    if(count == 1) {
        *heur = alias;
    }
    else {
        *heur = variant;
    }
    return 0;
}

const thdat_detect_entry_t*
thdat_detect_iter(
    uint32_t out[4])
{
    for(int i=0;i<4;i++) {
        if(!out[i]) continue;
        uint32_t v = out[i];
        int j=0;
        for(;!(v&1);v>>=1, j++);
        out[i] &= -1 << (j+1);
        int entry_num = i*32 + j;
        if(entry_num >= DETECT_ENTRIES) { /* non-existent entry */
            out[0]=out[1]=out[2]=out[3] = 0;
            return NULL;
        }
        return &detect_table[entry_num];
    }
    return NULL;
}