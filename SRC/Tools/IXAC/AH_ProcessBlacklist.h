#pragma once

namespace AntiHook::ProcessBlacklist
{
    void Scan(const char* logFile);

    bool IsDebuggerAttached();
    void ScanProcesses(const char* logFile);
    void ScanWindows(const char* logFile);
    void ScanHollowedProcesses(const char* logFile);
}