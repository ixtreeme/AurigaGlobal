#include "../../stdafx.h"

#include "AISystem.hpp"

#include <algorithm>
#include <cmath>

#include "../components/ai_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/status_components.hpp"
#include "../components/transform_components.hpp"
#include "../../config.h"

namespace {

constexpr uint32_t GUARD_SEARCH_INTERVAL = 1000;

enum EAIState : uint8_t
{
    AI_STATE_IDLE = 0,
    AI_STATE_CHASE = 1,
    AI_STATE_ATTACK = 2,
    AI_STATE_RETURN = 3,
};

void StepTowards(ecs::Position& from, const ecs::Position& to, int32_t maxStep)
{
    const int32_t dx = to.x - from.x;
    const int32_t dy = to.y - from.y;
    if (dx == 0 && dy == 0) {
        return;
    }

    const double distance = std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
    if (distance <= maxStep) {
        from.x = to.x;
        from.y = to.y;
        return;
    }

    const double ratio = static_cast<double>(maxStep) / distance;
    from.x += static_cast<int32_t>(std::round(dx * ratio));
    from.y += static_cast<int32_t>(std::round(dy * ratio));
}

bool CanGuardAttack(const ecs::AIFlags* flags, const ecs::EmpireComponent* empire)
{
    if (!flags || !empire) {
        return true;
    }

    if (flags->isNoAttackShinsu && empire->value == 1) {
        return false;
    }

    if (flags->isNoAttackChunjo && empire->value == 2) {
        return false;
    }

    if (flags->isNoAttackJinno && empire->value == 3) {
        return false;
    }

    return true;
}

void AISystem_UpdateGuard(entt::registry& reg, uint32_t tick)
{
    // migrated from CHARACTER::__StateIdle_NPC guard search
    auto view = reg.view<ecs::GuardState, ecs::Position, ecs::AIState>(entt::exclude<ecs::DeadTag>);

    view.each([&](const entt::entity entity, ecs::GuardState& guard, const ecs::Position& guardPosition, ecs::AIState& aiState) {
        if (tick - guard.lastSearchTime < GUARD_SEARCH_INTERVAL) {
            return;
        }

        guard.lastSearchTime = tick;

        if (guard.victim != entt::null && (!reg.valid(guard.victim) || reg.all_of<ecs::DeadTag>(guard.victim))) {
            guard.victim = entt::null;
        }

        if (guard.victim != entt::null) {
            return;
        }

        entt::entity closestVictim = entt::null;
        int32_t bestDistance = std::numeric_limits<int32_t>::max();
        const auto* flags = reg.try_get<ecs::AIFlags>(entity);

        auto pcView = reg.view<ecs::TagPC, ecs::Position, ecs::EmpireComponent>(entt::exclude<ecs::DeadTag, ecs::StunTag>);
        pcView.each([&](const entt::entity candidate, const ecs::Position& candidatePosition, const ecs::EmpireComponent& candidateEmpire) {
            if (!CanGuardAttack(flags, &candidateEmpire)) {
                return;
            }

            const int32_t distance = DISTANCE_APPROX(candidatePosition.x - guardPosition.x, candidatePosition.y - guardPosition.y);
            if (distance > static_cast<int32_t>(guard.searchRadius) || distance >= bestDistance) {
                return;
            }

            closestVictim = candidate;
            bestDistance = distance;
        });

        if (closestVictim != entt::null) {
            guard.victim = closestVictim;
            reg.emplace_or_replace<ecs::CombatTarget>(entity, closestVictim, tick);
            reg.emplace_or_replace<ecs::CombatActiveTag>(entity);
            aiState.currentState = AI_STATE_CHASE;
            aiState.nextStatePulse = tick + PASSES_PER_SEC(1);
        }
    });
}

void AISystem_UpdateWarFlag(entt::registry& reg, uint32_t tick)
{
    // migrated from CHARACTER::StateFlag / CHARACTER::StateFlagBase
    auto view = reg.view<ecs::WarFlagState, ecs::Position>(entt::exclude<ecs::DeadTag>);
    view.each([&](const entt::entity entity, const ecs::WarFlagState& flagState, const ecs::Position& flagPosition) {
        (void)entity;
        (void)tick;
        (void)flagState;
        (void)flagPosition;
        // TODO Phase 8: move war-flag capture/return logic from char_state.cpp.
    });
}

void AISystem_UpdatePassiveTags(entt::registry& reg)
{
    // migrated from CHARACTER::__StateIdle_Stone / CHARACTER::StateHorse
    auto stoneView = reg.view<ecs::StoneAITag, ecs::AIState>(entt::exclude<ecs::DeadTag>);
    stoneView.each([](const entt::entity, ecs::AIState& aiState) {
        aiState.currentState = AI_STATE_IDLE;
    });

    auto horseView = reg.view<ecs::HorseAITag, ecs::AIState>(entt::exclude<ecs::DeadTag>);
    horseView.each([](const entt::entity, ecs::AIState& aiState) {
        aiState.currentState = AI_STATE_IDLE;
    });
}

} // namespace

void AISystem_Update(entt::registry& reg, uint32_t tick)
{
    AISystem_UpdateGuard(reg, tick);
    AISystem_UpdateWarFlag(reg, tick);
    AISystem_UpdatePassiveTags(reg);

    // migrated from CHARACTER::StateIdle
    auto view = reg.view<ecs::AIState, ecs::AggroTable, ecs::Position, ecs::MobDataRef>(entt::exclude<ecs::DeadTag, ecs::StunTag>);

    view.each([&](const entt::entity entity, ecs::AIState& aiState, ecs::AggroTable& aggroTable, ecs::Position& position, const ecs::MobDataRef& mobData) {
        (void)mobData;

        if (tick < aiState.nextStatePulse) {
            return;
        }

        aggroTable.entries.erase(
            std::remove_if(aggroTable.entries.begin(), aggroTable.entries.end(), [&](const auto& entry) {
                return entry.first == entt::null || !reg.valid(entry.first) || reg.all_of<ecs::DeadTag>(entry.first);
            }),
            aggroTable.entries.end());

        if (!aggroTable.entries.empty()) {
            const entt::entity target = aggroTable.entries.front().first;
            if (const auto* targetPosition = reg.try_get<ecs::Position>(target)) {
                const double distance = std::sqrt(
                    static_cast<double>(targetPosition->x - position.x) * (targetPosition->x - position.x) +
                    static_cast<double>(targetPosition->y - position.y) * (targetPosition->y - position.y));

                if (distance > 300.0) {
                    aiState.currentState = AI_STATE_CHASE;
                    StepTowards(position, *targetPosition, 200);
                    aiState.nextStatePulse = tick + PASSES_PER_SEC(1);
                } else {
                    aiState.currentState = AI_STATE_ATTACK;
                    reg.emplace_or_replace<ecs::CombatTarget>(entity, target, tick);
                    reg.emplace_or_replace<ecs::CombatActiveTag>(entity);
                    aiState.nextStatePulse = tick + PASSES_PER_SEC(1);
                }
                return;
            }
        }

        if (const auto* spawnInfo = reg.try_get<ecs::SpawnInfo>(entity)) {
            if (position.x != spawnInfo->x || position.y != spawnInfo->y) {
                aiState.currentState = AI_STATE_RETURN;
                const ecs::Position spawnPosition { spawnInfo->x, spawnInfo->y, position.z };
                StepTowards(position, spawnPosition, 200);
                aiState.nextStatePulse = tick + PASSES_PER_SEC(1);
                return;
            }
        }

        aiState.currentState = AI_STATE_IDLE;
        aiState.nextStatePulse = tick + PASSES_PER_SEC(1);
    });
}
