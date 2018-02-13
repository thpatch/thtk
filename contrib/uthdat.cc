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
#include <time.h>
#include <utility>
#include <memory>
#include "thtkpp.hh"

Thtk::Dat* open_and_detect(const char* filename, Thtk::Io& io) {
    int version = Thtk::Dat::detect(filename, io);
    if(-1 == version) {
        thtk_error_t* err;
        thtk_error_new(&err, "couldn't detect version");
        throw Thtk::Error(err);
    }
    return new Thtk::Dat(version, io);
}

int main(int argc, char** argv) {
    try {
        if(argc < 3) return 1;
    
        if(!strcmp(argv[1],"list")) {
            char date[13];
            time_t t = time(NULL);
            struct tm tm;
            localtime_r(&t,&tm);
            strftime(date,sizeof(date),"%b %d %H:%M",&tm);
            
            Thtk::Io io(argv[2],"rb");
            std::unique_ptr<Thtk::Dat> dat(open_and_detect(argv[2],io));
            ssize_t count = dat-> entry_count();
            for(int i=0;i<count;i++) {
                Thtk::Entry e = dat->entry(i);
                printf("-r--r--r-- 1 0 0 %d %s %s\n",e.size(), date, e.name());
            }
        } else if(!strcmp(argv[1],"copyout")) {
            if(argc < 5) return 1;
            Thtk::Io io(argv[2],"rb");
            std::unique_ptr<Thtk::Dat> dat(open_and_detect(argv[2],io));
	    Thtk::Entry e = dat->entry(argv[3]);
            Thtk::Io io2(argv[4],"wb");
            e.read(io2);
        }
        else {
            return 1;
        }
    } catch(Thtk::Error& err) {
        fprintf(stderr, "%s\n", err.what());
    }
}
