#pragma once
#include "AH_Core.h"

namespace AntiHook::TextGuard
{
    // Inicializálás: .text szekció megkeresése, hash elmentése, PAGE_EXECUTE_READ védelem.
    void Init();

    // Hash ellenõrzése: ha false, akkor módosult (hook / patch gyanú).
    bool Verify(const char* logFile);
}
