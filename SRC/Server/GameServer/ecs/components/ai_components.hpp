#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

#include <common/tables.h>

#include "../../mob_manager.h"

namespace ecs {

// The two-state machine CHARACTER used to inherit from CFSM. Transitions
// are deferred by one update, which is what CFSM did through m_pNewState;
// its Begin/End hooks were BeginStateEmpty/EndStateEmpty on CHARACTER, so
// nothing else is lost. Initial was a no-op state and stays one.
enum class AIFSMState : uint8_t { Initial = 0, Idle, Battle };

struct AIStateMachine {
    AIFSMState current { AIFSMState::Initial };
    AIFSMState pending { AIFSMState::Initial };
    bool hasPending { false };
};

struct AIFlags {
    bool isAggressive : 1;
    bool isCoward : 1;
    bool isAttackMob : 1;
    bool isNoAttackShinsu : 1;
    bool isNoAttackChunjo : 1;
    bool isNoAttackJinno : 1;
    bool isBerserk : 1;
    bool isGuard : 1;
    bool isDeadFly : 1;
    bool isStoneSkinner : 1;
    bool isGodSpeed : 1;
    bool isDeathBlower : 1;
    bool isReviver : 1;
    bool isNoMove : 1;
};

struct AIState {
    uint8_t currentState;
    uint32_t nextStatePulse;
    uint32_t stateDuration;
};

struct GuardState {
    entt::entity victim { entt::null };
    uint32_t searchRadius;
    uint32_t lastSearchTime;
};

struct WarFlagState {
    uint8_t flagEmpire;
    bool isBaseFlag;
};

struct AggroTable {
    std::vector<std::pair<entt::entity, int32_t>> entries;
    entt::entity stoneOwner { entt::null };
};

struct SpawnInfo {
    int32_t x, y;
    uint32_t mapIndex;
    uint32_t respawnTime;
    std::size_t regenId;
};

struct MobDataRef {
    const CMob* data { nullptr };
    CMobInstance* instance { nullptr };
};

struct FlyTargets {
    uint32_t primary;
    std::vector<uint32_t> list;
};

struct HorseAITag {};
struct StoneAITag {};

} // namespace ecs
