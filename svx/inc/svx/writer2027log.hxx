/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027LOG_HXX
#define INCLUDED_SVX_WRITER2027LOG_HXX

#include <com/sun/star/uno/Exception.hpp>

#include <osl/file.hxx>
#include <rtl/string.hxx>
#include <rtl/textenc.h>
#include <rtl/ustring.hxx>
#include <sal/log.hxx>

#include <exception>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// winuser.h maps DrawText -> DrawTextA; that would break every VCL
// OutputDevice::DrawText call in any TU including this header.
#ifdef DrawText
#undef DrawText
#endif
#endif

namespace svx::writer2027
{

/** Append one line to the Writer 2027 diagnostics log.

    The log file lives in the user's temp directory (%TEMP%/writer2027.log on
    Windows) so it survives application crashes and does not depend on the
    LibreOffice user profile. Best-effort: never throws, never asserts. */
inline void Writer2027AppendLogLine(const OUString& rLine)
{
    OUString aDir;
    if (osl::FileBase::getTempDirURL(aDir) != osl::FileBase::E_None)
        return;
    const OUString aUrl = aDir + "/writer2027.log";
    osl::File aFile(aUrl);
    if (aFile.open(osl_File_OpenFlag_Write | osl_File_OpenFlag_Create) != osl::FileBase::E_None)
        return;
    (void)aFile.setPos(osl_Pos_End, 0);
    const OUString aLine = rLine + u"\n"_ustr;
    const OString aUtf8 = OUStringToOString(aLine, RTL_TEXTENCODING_UTF8);
    sal_uInt64 nWritten = 0;
    (void)aFile.write(aUtf8.getStr(), aUtf8.getLength(), nWritten);
    aFile.close();
}

/** Log a UNO exception with a context label (also to SAL). */
inline void Writer2027LogException(const char* pContext, const css::uno::Exception& rEx)
{
    const OUString aLine = OUString::Concat("[") + OUString::createFromAscii(pContext)
                           + u"] UNO exception: "_ustr + rEx.Message;
    SAL_WARN("svx.writer2027", aLine);
    Writer2027AppendLogLine(aLine);
}

/** Log a C++ exception with a context label (also to SAL). */
inline void Writer2027LogException(const char* pContext, const std::exception& rEx)
{
    const OUString aLine
        = OUString::Concat("[") + OUString::createFromAscii(pContext)
          + u"] C++ exception: "_ustr + OUString::createFromAscii(rEx.what());
    SAL_WARN("svx.writer2027", aLine);
    Writer2027AppendLogLine(aLine);
}

/** Log an unknown (non-standard, non-UNO) exception with a context label. */
inline void Writer2027LogUnknownException(const char* pContext)
{
    const OUString aLine = OUString::Concat("[") + OUString::createFromAscii(pContext)
                           + u"] unknown exception"_ustr;
    SAL_WARN("svx.writer2027", aLine);
    Writer2027AppendLogLine(aLine);
}

/** Generic message log line (also to SAL). */
inline void Writer2027LogMessage(const char* pContext, const OUString& rMessage)
{
    const OUString aLine = OUString::Concat("[") + OUString::createFromAscii(pContext)
                           + u"] "_ustr + rMessage;
    SAL_WARN("svx.writer2027", aLine);
    Writer2027AppendLogLine(aLine);
}

#ifdef _WIN32

#include <cstdio>
#include <cstring>
#include <string>

// CaptureStackBackTrace lives in kernel32 (forwarded to RtlCaptureStackBackTrace);
// declared here to avoid a dbghelp.lib dependency in the crash path.
extern "C" __declspec(dllimport) USHORT WINAPI
    CaptureStackBackTrace(DWORD nFramesToSkip, DWORD nFramesToCapture, PVOID* pBackTrace,
                          PDWORD pBackTraceHash);

namespace
{
// Internal-linkage state for the crash handler (per-TU copies are fine: the
// handler is installed once, from the TU that calls Writer2027InstallCrashCapture).
[[maybe_unused]] HANDLE g_hWriter2027CrashFile = INVALID_HANDLE_VALUE;
[[maybe_unused]] bool g_bWriter2027CrashCaptured = false;
[[maybe_unused]] char g_aWriter2027CrashBuf[4096];
[[maybe_unused]] size_t g_nWriter2027CrashUsed = 0;

[[maybe_unused]] void lcl_Writer2027CrashAppend(const char* pText)
{
    const size_t nLen = strlen(pText);
    if (g_nWriter2027CrashUsed + nLen < sizeof(g_aWriter2027CrashBuf))
    {
        memcpy(g_aWriter2027CrashBuf + g_nWriter2027CrashUsed, pText, nLen);
        g_nWriter2027CrashUsed += nLen;
    }
}

[[maybe_unused]] void lcl_Writer2027CrashAppendHex(sal_uIntPtr nValue)
{
    char aHex[32];
    snprintf(aHex, sizeof(aHex), "0x%llX", static_cast<unsigned long long>(nValue));
    lcl_Writer2027CrashAppend(aHex);
}

[[maybe_unused]] void lcl_Writer2027CrashAppendModule(sal_uIntPtr nAddr)
{
    HMODULE hMod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(nAddr), &hMod)
        || !hMod)
    {
        lcl_Writer2027CrashAppend("<unknown>");
        return;
    }
    wchar_t aPath[MAX_PATH] = {};
    if (GetModuleFileNameW(hMod, aPath, MAX_PATH) == 0)
    {
        lcl_Writer2027CrashAppend("<unknown>");
        return;
    }
    const wchar_t* pSlash = wcsrchr(aPath, L'\\');
    const wchar_t* pBase = pSlash ? pSlash + 1 : aPath;
    const sal_uIntPtr nBase = reinterpret_cast<sal_uIntPtr>(hMod);

    char aName[64];
    size_t n = wcslen(pBase);
    if (n >= sizeof(aName))
        n = sizeof(aName) - 1;
    for (size_t i = 0; i < n; ++i)
        aName[i] = static_cast<char>(pBase[i]);
    aName[n] = '\0';

    lcl_Writer2027CrashAppend(aName);
    lcl_Writer2027CrashAppend("+0x");
    lcl_Writer2027CrashAppendHex(nAddr - nBase);
}

[[maybe_unused]] void lcl_Writer2027CrashFlush()
{
    if (g_nWriter2027CrashUsed == 0)
        return;
    DWORD nWritten = 0;
    WriteFile(g_hWriter2027CrashFile, g_aWriter2027CrashBuf,
              static_cast<DWORD>(g_nWriter2027CrashUsed), &nWritten, nullptr);
    g_nWriter2027CrashUsed = 0;
}

[[maybe_unused]] LONG WINAPI Writer2027CrashHandler(EXCEPTION_POINTERS* pExceptionInfo)
{
    // Capture every exception code EXCEPT the known-benign debugger/trace
    // pseudo-exceptions that helper threads raise during normal startup
    // (DBG_PRINTEXCEPTION_C 0x406D1388 from OutputDebugString, DBG_CONTROL_C
    // 0x40010005). LO's UAE top-level filter swallows genuine faults with no
    // WER dump, so record the code + address + backtrace before dying; return
    // EXCEPTION_CONTINUE_SEARCH to preserve the original crash behaviour.
    // Each fault is captured independently (no single-shot gate): a benign
    // background exception must not exhaust the only capture slot.
    const DWORD nCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
    if (nCode == 0x406D1388 /* DBG_PRINTEXCEPTION_C */ || nCode == 0x40010005 /* Control-C */)
        return EXCEPTION_CONTINUE_SEARCH;

    if (g_hWriter2027CrashFile == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH; // install-time open failed; nothing to write

    const sal_uIntPtr nFaultAddr
        = reinterpret_cast<sal_uIntPtr>(pExceptionInfo->ExceptionRecord->ExceptionAddress);

    g_nWriter2027CrashUsed = 0; // reset per fault
    lcl_Writer2027CrashAppend("==== Writer2027 hard-fault capture ====\n");
    lcl_Writer2027CrashAppend("code: ");
    lcl_Writer2027CrashAppendHex(nCode);
    lcl_Writer2027CrashAppend("\naddress: ");
    lcl_Writer2027CrashAppendHex(nFaultAddr);
    lcl_Writer2027CrashAppend("\nmodule: ");
    lcl_Writer2027CrashAppendModule(nFaultAddr);
    lcl_Writer2027CrashAppend("\nstack:\n");

    void* aFrames[32];
    const USHORT nFrames = CaptureStackBackTrace(0, 32, aFrames, nullptr);
    for (USHORT i = 0; i < nFrames; ++i)
    {
        const sal_uIntPtr nRet = reinterpret_cast<sal_uIntPtr>(aFrames[i]);
        lcl_Writer2027CrashAppend("  ");
        lcl_Writer2027CrashAppendHex(nRet);
        lcl_Writer2027CrashAppend(" (");
        lcl_Writer2027CrashAppendModule(nRet);
        lcl_Writer2027CrashAppend(")\n");
    }
    lcl_Writer2027CrashAppend("==== end capture ====\n");
    lcl_Writer2027CrashFlush();
    // Keep the file handle open so subsequent faults append (the handle is
    // closed at process exit via the OS).

    // Let the normal crash path (UAE filter) proceed unchanged.
    return EXCEPTION_CONTINUE_SEARCH;
}
} // namespace

/** Minimal OS-level crash capture for Writer 2027.

    LibreOffice installs its own top-level filter
    (sal/osl/w32/signal.cxx::signalHandlerFunction) and excludes itself from
    WER (WerAddExcludedApplication), so a hard SEH access violation produces
    NO crash dump and — because the code is compiled with /EHsc — is not
    catchable by catch(...). This vectored handler runs BEFORE the SEH filter
    and records the fault address, the owning module/offset and a raw stack
    backtrace to %TEMP%/writer2027_crash.log, then returns
    EXCEPTION_CONTINUE_SEARCH so the crash behaves exactly as before (this is
    evidence capture, not recovery). */
inline void Writer2027InstallCrashCapture()
{
    static bool bInstalled = false;
    if (bInstalled)
        return;
    bInstalled = true;

    // Pre-open the log file in normal (non-crash) context: the handler itself
    // must never allocate or call loader-lock-sensitive APIs.
    if (g_hWriter2027CrashFile == INVALID_HANDLE_VALUE)
    {
        wchar_t aTempDir[MAX_PATH] = {};
        if (GetTempPathW(MAX_PATH, aTempDir) != 0)
        {
            const std::wstring aLogPath = std::wstring(aTempDir) + L"writer2027_crash.log";
            g_hWriter2027CrashFile = CreateFileW(aLogPath.c_str(), FILE_APPEND_DATA,
                                                 FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        }
    }

    AddVectoredExceptionHandler(1 /*first, runs before the SEH filter*/,
                                &Writer2027CrashHandler);
}

#endif // _WIN32

} // namespace svx::writer2027

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */