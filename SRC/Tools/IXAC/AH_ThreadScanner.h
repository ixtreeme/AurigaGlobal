#pragma once

namespace AntiHook::ThreadScanner
{
    void SetWatchdogThreadId(unsigned long tid);
    void SetGuardianThreadId(unsigned long tid);

    void ScanThreadsForSuspiciousEip(const char* logFile);

    void ScanThreadStartAddresses(const char* logFile);
}
