// Crash handler for veltrix-studio smoke diagnostics (TEMPORARY):
// on SEH exception, captures a native stack trace with symbol names via
// dbghelp and writes it to veltrix_crash.log next to the exe.
// This will be removed once the startup crash is fixed.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstdlib>

static void write_stack_log(EXCEPTION_POINTERS* ep) {
    FILE* f = nullptr;
    fopen_s(&f, "veltrix_crash.log", "w");
    if (!f) return;

    fprintf(f, "exception code=%08lx addr=%p\n",
            ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    void* frames[64] = {};
    USHORT captured = CaptureStackBackTrace(0, 64, frames, nullptr);

    SYMBOL_INFOW* symbol = nullptr;
    const size_t sym_size = sizeof(SYMBOL_INFOW) + 256 * sizeof(wchar_t);
    symbol = (SYMBOL_INFOW*)malloc(sym_size);

    IMAGEHLP_LINEW64 line = {};
    line.SizeOfStruct = sizeof(line);

    for (USHORT i = 0; i < captured; ++i) {
        DWORD64 addr = (DWORD64)frames[i];
        fprintf(f, "[%2u] %p", i, frames[i]);
        if (symbol) {
            symbol->SizeOfStruct = sym_size;
            symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            if (SymFromAddrW(GetCurrentProcess(), addr, &displacement, symbol)) {
                fprintf(f, "  %ls+0x%llx", symbol->Name,
                        (unsigned long long)displacement);
                DWORD disp_line = 0;
                if (SymGetLineFromAddrW64(GetCurrentProcess(), addr,
                                          &disp_line, &line)) {
                    fprintf(f, "  (%ls:%lu)", line.FileName, line.LineNumber);
                }
            }
        }
        fprintf(f, "\n");
    }
    free(symbol);
    fclose(f);
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep) {
    write_stack_log(ep);
    // Also write the minidump (kept from the earlier iteration).
    const char* path = "veltrix_crash.dmp";
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                          MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(file);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static void install_crash_handler() {
    SetUnhandledExceptionFilter(crash_filter);
}
