#pragma once

namespace AntiHook::RwxScanner
{
    // RWX / EXECUTE_WRITECOPY régiók keresése. Találat esetén log + TerminateProcess.
    void ScanForRwxRegions(const char* logFile);
}
