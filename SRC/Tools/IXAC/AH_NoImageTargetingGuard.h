#pragma once

namespace AntiHook::NoImageTargetingGuard
{
    void Scan(const char* logFile, unsigned minDangerousHandles = 3);
}
