#include "stdafx.h"
#include "crash_report.h"

#ifdef _WIN32

#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

#pragma comment(lib, "dbghelp.lib")

namespace
{
    void Emit(FILE* fp, const char* fmt, ...)
    {
        char line[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(line, sizeof(line), fmt, args);
        va_end(args);

        fputs(line, stderr);
        fflush(stderr);
        if (fp)
        {
            fputs(line, fp);
            fflush(fp);
        }
    }

    void DescribeAddress(FILE* fp, HANDLE process, DWORD64 address, int frame)
    {
        // SYMBOL_INFO carries the name past the end of the struct.
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        const char* name = SymFromAddr(process, address, &displacement, symbol)
            ? symbol->Name : "<no symbol>";

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisplacement = 0;

        if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line))
            Emit(fp, "  [%02d] %s  (%s:%lu)\n", frame, name, line.FileName, line.LineNumber);
        else
            Emit(fp, "  [%02d] %s + 0x%llx\n", frame, name, displacement);
    }

    LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info)
    {
        FILE* fp = fopen("crash.txt", "a");
        const HANDLE process = GetCurrentProcess();

        const DWORD64 faultPC = reinterpret_cast<DWORD64>(info->ExceptionRecord->ExceptionAddress);
        const DWORD64 moduleBase = reinterpret_cast<DWORD64>(GetModuleHandleA(nullptr));

        Emit(fp, "\n=== CRASH: exception 0x%08lX at 0x%llx (module + 0x%llx) ===\n",
            info->ExceptionRecord->ExceptionCode, faultPC, faultPC - moduleBase);

        if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            info->ExceptionRecord->NumberParameters >= 2)
        {
            Emit(fp, "    %s address 0x%llx\n",
                info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                static_cast<DWORD64>(info->ExceptionRecord->ExceptionInformation[1]));
        }

        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, nullptr, TRUE);

        // StackWalk64 writes to the context, so it gets a copy.
        CONTEXT context = *info->ContextRecord;
        STACKFRAME64 frame = {};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        for (int i = 0; i < 48; ++i)
        {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame,
                    &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
                break;

            if (frame.AddrPC.Offset == 0)
                break;

            DescribeAddress(fp, process, frame.AddrPC.Offset, i);
        }

        SymCleanup(process);
        if (fp)
            fclose(fp);

        return EXCEPTION_EXECUTE_HANDLER;
    }
}

void InstallCrashReporter()
{
    SetUnhandledExceptionFilter(OnUnhandledException);
}

#else

void InstallCrashReporter() {}

#endif
