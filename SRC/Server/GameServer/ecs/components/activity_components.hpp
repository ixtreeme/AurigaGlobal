#pragma once

#include <cstdint>
#include <list>

#include "../../world/event.h"

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
    // Whether the list above came back from the database. It was a CHARACTER
    // field beside the list it describes.
    bool loaded { false };

    BattlePassMissions() = default;
    BattlePassMissions(const BattlePassMissions&) = default;
    BattlePassMissions& operator=(const BattlePassMissions&) = default;
    BattlePassMissions(BattlePassMissions&&) = default;

    // entt's swap-and-pop erases the last pool element by doing
    // `elem = std::move(elem)`. libc++ implements list move assignment with
    // splice(end(), other) and its hardening traps on the self-splice (SIGILL
    // on FreeBSD); MSVC tolerates the self-move. Guard it like AffectList.
    BattlePassMissions& operator=(BattlePassMissions&& other) noexcept {
        if (this != &other) {
            missions = std::move(other.missions);
            loaded = other.loaded;
        }
        return *this;
    }
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
