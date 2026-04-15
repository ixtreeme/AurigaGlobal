#pragma once

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
};

} // namespace ecs
