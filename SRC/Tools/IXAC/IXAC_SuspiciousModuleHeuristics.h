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
        int              score;
        std::string      reason;
    };

    std::vector<SuspiciousModule> ScanModulesHeuristic(int scoreThreshold);
}
