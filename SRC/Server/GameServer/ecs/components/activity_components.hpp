#pragma once

#include <cstdint>
#include <list>

#include "../../event.h"

namespace ecs {

struct FishingState {
    LPEVENT fishingNewEvent { nullptr };
    uint32_t fishVnum { 0 };
    uint32_t chance { 0 };
    uint32_t elapsedSeconds { 0 };
    uint8_t catchCount { 0 };
    uint32_t catchFailed { 0 };
    int32_t lastCatchTime { 0 };
};

struct FishingActiveTag {};

// The battle-pass missions this character is working on. It was a raw list
// of pointers on CHARACTER, so nothing entity-native could read a mission.
struct BattlePassMissions {
    std::list<TPlayerBattlePassMission*> missions;
};

// When a box was last opened. It was a CHARACTER field guarding a cooldown
// that only the item code enforced.
// The two anti-flood counters, one for commands and one for item use.
// They were four CHARACTER fields with inline accessors.
struct AntiFloodState {
    int cmdPulse { 0 };
    uint32_t cmdCount { 0 };
    int itemUsePulse { 0 };
    uint32_t itemUseCount { 0 };
};

struct BoxUseTime {
    uint32_t value { 0 };
};

struct MiningState {
    LPEVENT event { nullptr };
    entt::entity load { entt::null };
};

} // namespace ecs
