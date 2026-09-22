#pragma once
#include <string>

namespace AntiHook::SignerGuard
{
    void SetBlockedSigner(const std::wstring& subject);

    void ScanProcesses(const char* logFile);
}
