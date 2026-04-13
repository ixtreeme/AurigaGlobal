#pragma once

namespace AntiHook::NoImageTargetingGuard
{
    // Csak azt szkenneljük: ki nyit ERŐS handle-t a játék processzére
    void Scan(const char* logFile, unsigned minDangerousHandles = 3);
}
