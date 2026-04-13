#pragma once

namespace AntiHook::ThreadHijackGuard
{
    // Szálak vizsgálata: hijack / injektált szál / context-hijack
    // logFile: pl. "UserData/AC/_log.txt"
    void Scan(const char* logFile);
}
