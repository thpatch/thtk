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

#ifndef THTKPP_HH_
#define THTKPP_HH_
#include <exception>
#include <utility>
#include <string.h>
#include <thtk/thtk.h>
namespace Thtk {
    class Error : public std::exception {
        char *_msg;
    public:
        explicit Error(thtk_error_t* err) {
            const char *errm;
            if(!err || !(errm = thtk_error_message(err))) {
                static const char defmsg[] = "No error";
                _msg = new char[sizeof(defmsg)];
                strcpy(_msg, defmsg);
            }
            else {
                _msg = new char[strlen(errm)+1];
                strcpy(_msg, errm);
            }
            if(err) thtk_error_free(&err);
        }
        virtual ~Error() {
            delete[] _msg;
        }
        Error(const Error& other) {
            _msg = new char[strlen(other._msg)+1];
            strcpy(_msg,other._msg);
        }
        Error operator=(const Error& other) {
            delete[] _msg;
            _msg = new char[strlen(other._msg)+1];
            strcpy(_msg,other._msg);
        }
        virtual const char* what() const noexcept{
            return _msg;
        }
    };

    class Dat;
    class Entry;
    class Io {
        thtk_io_t* io;
    public:
        ssize_t read(void* buf, size_t count) {
            thtk_error_t* err;
            ssize_t rv = thtk_io_read(io, buf, count, &err);
            if(rv == -1) throw Thtk::Error(err);
            return rv;
        }
        ssize_t write(const void* buf, size_t count) {
            thtk_error_t* err;
            ssize_t rv = thtk_io_write(io, buf, count, &err);
            if(rv == -1) throw Thtk::Error(err);
            return rv;
        }
        off_t seek(off_t offset, int whence) {
            thtk_error_t* err;
            off_t rv = thtk_io_seek(io, offset, whence, &err);
            if(rv == -1) throw Thtk::Error(err);
            return rv;
        }
        ~Io() {
            if(io) thtk_io_close(io);
        }

        Io(const char* path, const char* mode) {
            thtk_error_t* err;
            io = thtk_io_open_file(path,mode,&err);
            if(!io) throw Thtk::Error(err);
        }
#ifdef _WIN32
        Io(const wchar_t* path, const wchar_t* mode) {
            thtk_error_t* err;
            io = thtk_io_open_file_w(path, mode, &err);
            if (!io) throw Thtk::Error(err);
        }
#endif
        Io(void* buf, size_t size) {
            thtk_error_t* err;
            io = thtk_io_open_memory(buf,size,&err);
            if(!io) throw Thtk::Error(err);
        }
        explicit Io() {
            thtk_error_t* err;
            io = thtk_io_open_growing_memory(&err);
            if(!io) throw Thtk::Error(err);
        }

        Io(const Io&) = delete;
        Io& operator=(const Io&) = delete;
        friend Thtk::Dat;
        friend Thtk::Entry;
    };

    class Entry {
        thdat_t* dat;
        int idx;
        Entry(thdat_t* dat, int idx) :dat(dat), idx(idx){}
    public:
        int index() { return idx; }
        void set_name(const char* name) {
            thtk_error_t* err;
            if(0 == thdat_entry_set_name(dat,idx,name,&err))
                throw Thtk::Error(err);
        }
        const char* name() {
            thtk_error_t* err;
            const char* rv = thdat_entry_get_name(dat,idx,&err);
            if(!rv) throw Thtk::Error(err);
            return rv;
        }
        ssize_t size() {
            thtk_error_t* err;
            ssize_t rv = thdat_entry_get_size(dat,idx,&err);
            if(!rv) throw Thtk::Error(err);
            return rv;
        }
        ssize_t zsize() {
            thtk_error_t* err;
            ssize_t rv = thdat_entry_get_zsize(dat,idx,&err);
            if(-1 == rv) throw Thtk::Error(err);
            return rv;
        }
        ssize_t write(Thtk::Io& input,size_t limit) {
            thtk_error_t* err;
            ssize_t rv = thdat_entry_write_data(dat,idx,input.io,limit,&err);
            if(-1 == rv) throw Thtk::Error(err);
            return rv;
        }
        ssize_t read(Thtk::Io& output) {
            thtk_error_t* err;
            ssize_t rv = thdat_entry_read_data(dat,idx,output.io,&err);
            if(-1 == rv) throw Thtk::Error(err);
            return rv;
        }
        friend Thtk::Dat;
    };
    class Dat {
        thdat_t* dat;
        bool write_mode;
    public:
        Dat(unsigned int version, Thtk::Io& input) {
            write_mode = false;
            thtk_error_t* err;
            dat = thdat_open(version, input.io, &err);
            if(!dat) throw Thtk::Error(err);
        }
        Dat(unsigned int version, Thtk::Io& output, size_t entry_count) {
            write_mode = true;
            thtk_error_t* err;
            dat = thdat_create(version, output.io, entry_count, &err);
            if(!dat) throw Thtk::Error(err);
        }
        ~Dat() {
            if(dat) {
                thtk_error_t* err;
                if(write_mode) thdat_close(dat, &err); // can't throw err inside destructor
                thdat_free(dat);
            }
        }
        Dat(const Dat&) = delete;
        Dat& operator=(const Dat&) = delete;
        ssize_t entry_count() {
            thtk_error_t* err;
            ssize_t rv = thdat_entry_count(dat,&err);
            if(-1 == rv) throw Thtk::Error(err);
            return rv;
        }
        Entry entry(int index) {
            return Entry(dat,index);
        }
        Entry entry(const char* name) {
            thtk_error_t* err;
            ssize_t index = thdat_entry_by_name(dat,name,&err);
            if(-1 == index) throw Thtk::Error(err);
            return entry(index);
        }

        static int detect_filename(const char* filename) {
            return thdat_detect_filename(filename);
        }

        static int detect(const char* filename, Thtk::Io& input) {
            uint32_t out[4];
            unsigned int heur;
            thtk_error_t* err;
            if(-1 == thdat_detect(filename,input.io,out,&heur,&err))
                throw Thtk::Error(err);
            return heur;
        }
#ifdef _WIN32
        static int detect_filename(const wchar_t* filename) {
            return thdat_detect_filename_w(filename);
        }

        static int detect(const wchar_t* filename, Thtk::Io& input) {
            uint32_t out[4];
            unsigned int heur;
            thtk_error_t* err;
            if (-1 == thdat_detect_w(filename, input.io, out, &heur, &err))
                throw Thtk::Error(err);
            return heur;
        }
#endif

        friend Thtk::Entry;
    };
}
#endif
