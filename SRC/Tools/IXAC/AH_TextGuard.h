#pragma once
#include "AH_Core.h"

namespace AntiHook::TextGuard
{
    void Init();

    bool Verify(const char* logFile);
}
