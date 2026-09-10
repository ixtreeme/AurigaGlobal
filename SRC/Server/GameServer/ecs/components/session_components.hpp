#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "../../typedef.h"

namespace ecs {

struct NetworkSession {
    LPDESC desc;
};

// Set when a character must not be written back - a deleted or duplicated
// login. It was a CHARACTER field only the save path read.
struct SkipSave {
    bool value { false };
};

// The phone number the account registered with and the code sent to it. Two
// CHARACTER fields the save path was the only reader of; the code has not been
// written since the constructor cleared it, so the guard it forms is always
// true. Kept as it stands rather than folded away.
struct MobileAuth {
    std::string phone;
    std::string code;
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
