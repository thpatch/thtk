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
#include "thtkpp.hh"
#include "wcxhead.h"
#include <wchar.h>
#include <string.h>
#include <memory>

template<typename CTYPE> struct Helper {};
template<>
struct Helper<char> {
    constexpr static char*rb = "rb";
    constexpr static char*wb = "wb";
    static void cpy_s(char *dest, size_t destsz, const char *src) {
        strcpy_s(dest,destsz,src);
    }
    static void cpy_widen_s(char *dest, size_t destsz, const char *src) {
        strcpy_s(dest,destsz,src);
    }
};
template<>
struct Helper<wchar_t> {
    constexpr static wchar_t*rb = L"rb";
    constexpr static wchar_t*wb = L"wb";
    static void cpy_s(wchar_t *dest, size_t destsz, const wchar_t *src) {
        wcscpy_s(dest,destsz,src);
    }
    static void cpy_widen_s(wchar_t *dest, size_t destsz, const char *src) {
        for (int i = 0; i < destsz && *src; i++, src++) {
            dest[i] = (wchar_t)(unsigned char)*src; // treat name as Latin1
        }
    }
};

template<typename CTYPE>
Thtk::Dat* open_and_detect(const CTYPE* filename, Thtk::Io& io) {
    int version = Thtk::Dat::detect(filename, io);
    if (-1 == version) {
        thtk_error_t* err;
        thtk_error_new(&err, "couldn't detect version");
        throw Thtk::Error(err);
    }
    return new Thtk::Dat(version, io);
}

struct DeletableBase {
    virtual ~DeletableBase() {}
};

template<typename CTYPE>
struct ThdatContext : DeletableBase {
    CTYPE arcname[MAX_PATH];

    int openmode;
    std::unique_ptr<Thtk::Io> io;
    std::unique_ptr<Thtk::Dat> dat;
    int cur_entry;
    int entries;
    int size;
};


template<typename CTYPE, typename ADATA>
HANDLE OpenArchiveTemplate(ADATA* ArchiveData) {
    std::unique_ptr<Thtk::Io> io;
    try {
        io.reset(new Thtk::Io(ArchiveData->ArcName, Helper<CTYPE>::rb));
    }
    catch (...) {
        ArchiveData->OpenResult = E_EOPEN;
        return nullptr;
    }
    std::unique_ptr<Thtk::Dat> dat;
    try {
        dat.reset(open_and_detect(ArchiveData->ArcName, *io));
    }
    catch (...) {
        ArchiveData->OpenResult = E_BAD_ARCHIVE;
        return nullptr;
    }

    ThdatContext<CTYPE>* ctx = new ThdatContext<CTYPE>;
    Helper<CTYPE>::cpy_s(ctx->arcname, MAX_PATH, ArchiveData->ArcName);
    ctx->openmode = ArchiveData->OpenMode;
    ctx->io.reset(io.release());
    ctx->dat.reset(dat.release());
    ctx->cur_entry = -1;
    ctx->entries = ctx->dat->entry_count();

    return ctx;
}

template<typename CTYPE, typename HDATA>
int ReadHeaderTemplate(HANDLE hArcData, HDATA *HeaderData) {
    ThdatContext<CTYPE>* ctx = (ThdatContext<CTYPE>*)hArcData;
    memset(HeaderData, 0, sizeof(*HeaderData));

    ++ctx->cur_entry;
    if (ctx->cur_entry == ctx->entries) return E_END_ARCHIVE;

    thtk_error_t* err = nullptr;
    Helper<CTYPE>::cpy_s(HeaderData->ArcName, MAX_PATH, ctx->arcname);
    try {
        Thtk::Entry e = ctx->dat->entry(ctx->cur_entry);
        const char* name = e.name();
        Helper<CTYPE>::cpy_widen_s(HeaderData->FileName, sizeof(HeaderData->FileName) / sizeof(CTYPE), name);
        HeaderData->PackSize = e.zsize();
        HeaderData->UnpSize = e.size();;
    }
    catch (...) {
        return E_BAD_ARCHIVE;
    }
    return 0;
}

template<typename CTYPE>
int ProcessFileTemplate(HANDLE hArcData, int Operation, CTYPE *DestPath, CTYPE *DestName) {
    ThdatContext<CTYPE>* ctx = (ThdatContext<CTYPE>*)hArcData;
    if (Operation != PK_EXTRACT) return 0;
    if (DestPath == nullptr) DestPath = DestName;

    try {
        Thtk::Io out(DestPath, Helper<CTYPE>::wb);
        ctx->dat->entry(ctx->cur_entry).read(out);
    }
    catch (...) {
        return E_BAD_ARCHIVE;
    }
    return 0;
}

template<typename CTYPE>
BOOL CanYouHandleThisFileTemplate(CTYPE* filename) {
    try {
        Thtk::Io io(filename, Helper<CTYPE>::rb);
        return -1 != Thtk::Dat::detect(filename, io);
    }
    catch (...)
    {
        return FALSE;
    }
}

extern "C" {
    BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
        return TRUE;
    }
    // ANSI functions
    HANDLE API_SYMBOL __stdcall OpenArchive(tOpenArchiveData *ArchiveData) {
        return OpenArchiveTemplate<char, tOpenArchiveData>(ArchiveData);
    }
    int API_SYMBOL __stdcall ReadHeader(HANDLE hArcData, tHeaderData *HeaderData) {
        return ReadHeaderTemplate<char, tHeaderData>(hArcData, HeaderData);
    }
    int API_SYMBOL __stdcall ProcessFile(HANDLE hArcData, int Operation, char *DestPath, char *DestName) {
        return ProcessFileTemplate<char>(hArcData, Operation, DestPath, DestName);
    }
    void API_SYMBOL __stdcall SetChangeVolProc(HANDLE hArcData, tChangeVolProc pChangeVolProc1) {
    }
    void API_SYMBOL __stdcall SetProcessDataProc(HANDLE hArcData, tProcessDataProc pProcessDataProc) {
    }
    BOOL API_SYMBOL __stdcall CanYouHandleThisFile(char* filename) {
        return CanYouHandleThisFileTemplate<char>(filename);
    }
    // Unicode functions
    HANDLE API_SYMBOL __stdcall OpenArchiveW(tOpenArchiveDataW *ArchiveData) {
        return OpenArchiveTemplate<wchar_t, tOpenArchiveDataW>(ArchiveData);
    }
    int API_SYMBOL __stdcall ReadHeaderExW(HANDLE hArcData, tHeaderDataExW *HeaderData) {
        return ReadHeaderTemplate<wchar_t, tHeaderDataExW>(hArcData, HeaderData);
    }
    int API_SYMBOL __stdcall ProcessFileW(HANDLE hArcData, int Operation, wchar_t *DestPath, wchar_t *DestName) {
        return ProcessFileTemplate<wchar_t>(hArcData, Operation, DestPath, DestName);
    }
    void API_SYMBOL __stdcall SetChangeVolProcW(HANDLE hArcData, tChangeVolProcW pChangeVolProc1) {
    }
    void API_SYMBOL __stdcall SetProcessDataProcW(HANDLE hArcData, tProcessDataProcW pProcessDataProc) {
    }
    BOOL API_SYMBOL __stdcall CanYouHandleThisFileW(wchar_t* filename) {
        return CanYouHandleThisFileTemplate<wchar_t>(filename);
    }
    // Misc
    int API_SYMBOL __stdcall GetPackerCaps() {
        return PK_CAPS_BY_CONTENT;
    }
    int API_SYMBOL __stdcall CloseArchive(HANDLE hArcData) {
        DeletableBase* ctx = (DeletableBase*)hArcData;
        delete ctx;
        return 0;
    }
}
