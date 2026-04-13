#pragma once

namespace AntiHook::SyscallScanner
{
    // Baseline hash-ek inicializálása (NtOpenProcess, NtReadVirtualMemory, stb.)
    void InitSyscallBaseline();

    // Syscall stubok aktuális hash ellenõrzése – ha nem egyezik, log + TerminateProcess.
    void ScanSyscallStubs(const char* logFile);
}
