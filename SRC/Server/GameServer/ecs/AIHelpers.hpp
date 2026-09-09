#pragma once

#include "../typedef.h"
#include "Registry.hpp"
#include "VIDRegistry.hpp"
#include "components/ai_components.hpp"

namespace AIHelpers {

// Authoritative scheduler state; never mirror CHARACTER timing fields.
inline uint32_t GetStateDuration(entt::entity e)
{
    const auto* state = g_registry.valid(e) ? g_registry.try_get<ecs::AIState>(e) : nullptr;
    return state ? state->stateDuration : 1;
}
inline uint32_t GetNextStatePulse(entt::entity e)
{
    const auto* state = g_registry.valid(e) ? g_registry.try_get<ecs::AIState>(e) : nullptr;
    return state ? state->nextStatePulse : 0;
}
inline void SetStateDuration(entt::entity e, uint32_t duration)
{
    if (!g_registry.valid(e)) return;
    if (auto* state = g_registry.try_get<ecs::AIState>(e)) state->stateDuration = duration;
    // No returned reference: a construction observer may retire the entity.
    else g_registry.insert<ecs::AIState>(&e, &e + 1, ecs::AIState {0, 0, duration});
}
inline void SetNextStatePulse(entt::entity e, uint32_t pulse)
{
    if (!g_registry.valid(e)) return;
    if (auto* state = g_registry.try_get<ecs::AIState>(e)) state->nextStatePulse = pulse;
    else g_registry.insert<ecs::AIState>(&e, &e + 1, ecs::AIState {0, pulse, 1});
}

inline ecs::AIFlags* TryGetFlags(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    return g_registry.try_get<ecs::AIFlags>(e);
}

inline bool IsAggressive(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isAggressive;
}

inline bool IsCoward(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isCoward;
}

inline bool IsAttackMob(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isAttackMob;
}

inline bool IsNoAttackShinsu(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isNoAttackShinsu;
}

inline bool IsNoAttackChunjo(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isNoAttackChunjo;
}

inline bool IsNoAttackJinno(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isNoAttackJinno;
}

inline bool IsBerserk(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isBerserk;
}

inline bool IsGuard(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isGuard;
}

inline bool IsDeadFly(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isDeadFly;
}

inline bool IsNoMove(entt::entity e)
{
    auto* flags = TryGetFlags(e);
    return flags && flags->isNoMove;
}

inline void SetAggressive(entt::entity e, bool value)
{
    if (auto* flags = TryGetFlags(e)) {
        flags->isAggressive = value;
    }
}

inline void SetCoward(entt::entity e, bool value)
{
    if (auto* flags = TryGetFlags(e)) {
        flags->isCoward = value;
    }
}

inline void SetAttackMob(entt::entity e, bool value)
{
    if (auto* flags = TryGetFlags(e)) {
        flags->isAttackMob = value;
    }
}

inline void SetNoAttackShinsu(entt::entity e, bool value)
{
    if (auto* flags = TryGetFlags(e)) {
        flags->isNoAttackShinsu = value;
    }
}

inline void SetNoAttackChunjo(entt::entity e, bool value)
{
    if (auto* flags = TryGetFlags(e)) {
        flags->isNoAttackChunjo = value;
    }
}

inline void SetNoAttackJinno(entt::entity e, bool value)
{
    if (auto* flags = TryGetFlags(e)) {
        flags->isNoAttackJinno = value;
    }
}

inline void SetGuard(entt::entity e, bool value)
{
    if (auto* flags = TryGetFlags(e)) {
        flags->isGuard = value;
    }
}

} // namespace AIHelpers
