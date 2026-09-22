#pragma once

namespace AntiHook::SyscallScanner
{
    void InitSyscallBaseline();

    void ScanSyscallStubs(const char* logFile);
}
