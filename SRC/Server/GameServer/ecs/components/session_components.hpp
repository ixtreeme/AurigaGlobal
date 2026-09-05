#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "../../typedef.h"

namespace ecs {

struct NetworkSession {
    LPDESC desc;
};

struct LoginInfo {
    std::string login;
    uint32_t loginPlayTime;
    uint32_t playStartTime;
    std::string mobile;
    uint32_t logOffInterval;
    std::array<int, PREMIUM_MAX_NUM> premiumTimes {};
    bool isPCBang { false };
};

struct AntiFlood {
    int cmdPulse;
    uint32_t cmdCount;
    int itemUsePulse;
    uint32_t itemUseCount;
};

struct DragonSoulState {
    int activeDeck { -1 };
    LPENTITY refineWindowOpener { nullptr };
    int32_t lastRefineTime { 0 };
};

} // namespace ecs
