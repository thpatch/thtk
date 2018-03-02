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
#include <Windows.h>
#include <thtk/thtk.h>
#include <stdexcept>

#define ENUM_THTK_FUNCS(x) \
    /* error.h */ \
    x(const char*,thtk_error_message,(thtk_error_t* a),(a)) \
    x(void,thtk_error_free,(thtk_error_t** a),(a))\
    /* io.h */ \
    x(ssize_t,thtk_io_read,(thtk_io_t* a, void* b, size_t c, thtk_error_t** d),(a,b,c,d)) \
    x(ssize_t,thtk_io_write,(thtk_io_t* a, const void* b, size_t c, thtk_error_t** d),(a,b,c,d)) \
    x(off_t,thtk_io_seek,(thtk_io_t* a, off_t b, int c, thtk_error_t** d),(a,b,c,d)) \
    x(unsigned char*,thtk_io_map,(thtk_io_t* a, off_t b, size_t c, thtk_error_t** d),(a,b,c,d)) \
    x(void,thtk_io_unmap,(thtk_io_t* a, unsigned char* b),(a,b)) \
    x(int,thtk_io_close,(thtk_io_t* a),(a)) \
    x(thtk_io_t*,thtk_io_open_file,(const char* a, const char* b, thtk_error_t** c),(a,b,c)) \
    x(thtk_io_t*,thtk_io_open_file_w,(const wchar_t* a, const wchar_t* b, thtk_error_t** c),(a,b,c)) \
    x(thtk_io_t*,thtk_io_open_memory,(void* a, size_t b, thtk_error_t** c),(a,b,c)) \
    x(thtk_io_t*,thtk_io_open_growing_memory,(thtk_error_t** a),(a)) \
    /* dat.h */ \
    x(thdat_t*,thdat_open,(unsigned int a,thtk_io_t* b,thtk_error_t** c),(a,b,c)) \
    x(thdat_t*,thdat_create,(unsigned int a,thtk_io_t* b,size_t c,thtk_error_t** d),(a,b,c,d)) \
    x(int,thdat_init,(thdat_t* a,thtk_error_t** b),(a,b)) \
    x(int,thdat_close,(thdat_t* a,thtk_error_t** b),(a,b)) \
    x(void,thdat_free,(thdat_t* a),(a)) \
    x(ssize_t,thdat_entry_count,(thdat_t* a,thtk_error_t** b),(a,b)) \
    x(ssize_t,thdat_entry_by_name,(thdat_t* a,const char* b,thtk_error_t** c),(a,b,c)) \
    x(int,thdat_entry_set_name,(thdat_t* a,int b,const char* c,thtk_error_t** d),(a,b,c,d)) \
    x(const char*,thdat_entry_get_name,(thdat_t* a,int b,thtk_error_t** c),(a,b,c)) \
    x(ssize_t,thdat_entry_get_size,(thdat_t* a,int b,thtk_error_t** c),(a,b,c)) \
    x(ssize_t,thdat_entry_get_zsize,(thdat_t* a,int b,thtk_error_t** c),(a,b,c)) \
    x(ssize_t,thdat_entry_write_data,(thdat_t* a,int b,thtk_io_t* c,size_t d,thtk_error_t** e),(a,b,c,d,e)) \
    x(ssize_t,thdat_entry_read_data,(thdat_t* a,int b,thtk_io_t* c,thtk_error_t** d),(a,b,c,d)) \
    /* detect.h */ \
    x(int,thdat_detect_filename,(const char* a),(a)) \
    x(int,thdat_detect_filename_w,(const wchar_t* a),(a)) \
    x(int,thdat_detect,(const char* a,thtk_io_t* b, uint32_t c[4],unsigned int *d, thtk_error_t** e),(a,b,c,d,e)) \
    x(int,thdat_detect_w,(const wchar_t* a,thtk_io_t* b,uint32_t c[4],unsigned int *d, thtk_error_t** e),(a,b,c,d,e)) \
    x(const thdat_detect_entry_t*,thdat_detect_iter,(uint32_t a[4]),(a))

#define x(ret,name,arg,arg2) static decltype(name)* wrap_##name = nullptr;
ENUM_THTK_FUNCS(x)
#undef x

int thtk_wrapper_init(HMODULE dll) {
    int status = 0;
#define x(ret,name,arg,arg2) if(!(wrap_##name = (decltype(name)*)GetProcAddress(dll,#name))) status = -1;
    ENUM_THTK_FUNCS(x)
#undef x
    return status;
}

void thtk_wrapper_auto_load();

extern "C" {
#define x(ret,name,arg,arg2) ret name arg { if(!wrap_##name) thtk_wrapper_auto_load(); return wrap_##name arg2; }
    ENUM_THTK_FUNCS(x)
#undef x
}

static HMODULE thismodule = nullptr, thtkmodule = nullptr;

void thtk_wrapper_preinit(HMODULE mod) {
    thismodule = mod;
}
void thtk_wrapper_auto_load() {
    if (!thismodule) throw std::runtime_error("thtk_wrapper: didn't preinit");
    if (thtkmodule) throw std::runtime_error("thtk_wrapper: called twice");

    thtkmodule = GetModuleHandleW(L"thtk.dll"); // See if thtk.dll is already in process memory
    if (!thtkmodule) {
        wchar_t path[MAX_PATH];
        DWORD status = GetModuleFileNameW(thismodule, path, MAX_PATH);
        if (status == 0 || status == MAX_PATH)
            throw std::runtime_error("thtk_wrapper: GetModuleFileNameW error");
        wchar_t* path2 = wcsrchr(path, '\\');
        if (!path2)
            throw std::runtime_error("thtk_wrapper: couldn't find parent directory of current dll");
        ++path2;
        if (wcscpy_s(path2, MAX_PATH-(path2-path), L"thtk.dll"))
            throw std::runtime_error("thtk_wrapper: MAX_PATH");
        thtkmodule = LoadLibraryW(path);
        if (!thtkmodule)
            throw std::runtime_error("thtk_wrapper: couldn't load thtk.dll");
    }
    if (thtk_wrapper_init(thtkmodule)) {
        throw std::runtime_error("thtk_wrapper: couldn't load all thtk procs");
    }
}
