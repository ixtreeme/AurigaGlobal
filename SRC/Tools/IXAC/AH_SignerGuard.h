#pragma once
#include <string>

namespace AntiHook::SignerGuard
{
    // Tiltott aláíró subject neve (pl. L"BOOST - NET KRZYSZTOF ZAGÓRSKI")
    void SetBlockedSigner(const std::wstring& subject);

    // Processzek szkennelése: ha ilyen aláíróval fut bármi, log + TerminateProcess.
    void ScanProcesses(const char* logFile);
}
