#pragma once

namespace AntiHook::ThreadScanner
{
    void SetWatchdogThreadId(unsigned long tid);
    void SetGuardianThreadId(unsigned long tid);

    // IP (EIP/RIP) alapján: ha egy szál IP-je nem modulban van / furcsán RWX-ben → gyanús.
    void ScanThreadsForSuspiciousEip(const char* logFile);

    // Thread start address (Win32StartAddress) alapján – NtQueryInformationThread.
    void ScanThreadStartAddresses(const char* logFile);
}
