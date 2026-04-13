// AH_Heuristics.h
#pragma once

#include <string>
#include <vector>
#include "AH_Core.h" // ebben van: ModuleInfo, BuildModuleMap, IsModuleWhitelisted

namespace AntiHook::Heuristics
{
    struct SuspiciousModule
    {
        Core::ModuleInfo mod;
        int              score;   // veszélyességi pont
        std::string      reason;  // rövid magyarázat lognak
    };

    // Heurisztikusan gyanús modulok a JELENLEGI processben.
    // scoreThreshold: pl. 7 a COM2-szerû dolgokra.
    std::vector<SuspiciousModule> ScanModulesHeuristic(int scoreThreshold);
}
