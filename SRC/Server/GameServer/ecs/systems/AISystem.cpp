#include "../../stdafx.h"

#include "AISystem.hpp"

#include "../../char_interface.hpp"
#include "../../char_manager.h"
#include "../AIHelpers.hpp"
#include "../VIDRegistry.hpp"
#include "../components/ai_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"

namespace {

constexpr uint8_t AI_STATE_IDLE = 0;
constexpr uint8_t AI_STATE_CHASE = 1;
constexpr uint8_t AI_STATE_ATTACK = 2;
constexpr uint8_t AI_STATE_RETURN = 3;

LPCHARACTER LegacyCharOf(entt::registry& reg, entt::entity entity)
{
    if (entity == entt::null || !reg.valid(entity)) {
        return nullptr;
    }

    const auto* vid = reg.try_get<ecs::VIDComponent>(entity);
    if (!vid) {
        return nullptr;
    }

    return CHARACTER_MANAGER::instance().Find(vid->value);
}

uint8_t ObserveAIState(entt::registry& reg, entt::entity entity, LPCHARACTER ch)
{
    if (!ch) {
        return AI_STATE_IDLE;
    }

    if (LPCHARACTER victim = ch->GetVictim()) {
        const int32_t dx = victim->GetX() - ch->GetX();
        const int32_t dy = victim->GetY() - ch->GetY();
        const int32_t distance = DISTANCE_APPROX(dx, dy);
        const int32_t attackRange = static_cast<int32_t>(ch->GetMobAttackRange()) * 100;
        return distance <= attackRange ? AI_STATE_ATTACK : AI_STATE_CHASE;
    }

    if (const auto* spawn = reg.try_get<ecs::SpawnInfo>(entity)) {
        if (ch->GetX() != spawn->x || ch->GetY() != spawn->y) {
            return AI_STATE_RETURN;
        }
    }

    return AI_STATE_IDLE;
}

bool SyncAIFlags(entt::registry& reg, entt::entity entity, LPCHARACTER ch)
{
    auto& flags = reg.get_or_emplace<ecs::AIFlags>(entity);
    const ecs::AIFlags desired {
        ch->IsAggressive(),
        ch->IsCoward(),
        ch->IsAttackMob(),
        ch->IsNoAttackShinsu(),
        ch->IsNoAttackChunjo(),
        ch->IsNoAttackJinno(),
        ch->IsBerserk(),
        ch->IsGuardNPC(),
        false,
        ch->IsStoneSkinner(),
        ch->IsGodSpeed(),
        ch->IsDeathBlow(),
        ch->IsRevive(),
        flags.isNoMove,
    };

    if (std::memcmp(&flags, &desired, sizeof(ecs::AIFlags)) == 0) {
        return false;
    }

    flags = desired;
    return true;
}

} // namespace

void AISystem_Update(entt::registry& reg, uint32_t tick)
{
    (void)tick;

    auto view = reg.view<ecs::VIDComponent>();

    for (auto entity : view) {
        LPCHARACTER ch = LegacyCharOf(reg, entity);
        if (!ch) {
            continue;
        }

        bool changed = SyncAIFlags(reg, entity, ch);

        entt::entity victimEntity = entt::null;
        if (LPCHARACTER victim = ch->GetVictim()) {
            victimEntity = AIHelpers::EcsOf(victim);
        }

        if (victimEntity != entt::null) {
            auto* currentTarget = reg.try_get<ecs::CombatTarget>(entity);
            if (!currentTarget || currentTarget->target != victimEntity) {
                reg.emplace_or_replace<ecs::CombatTarget>(entity, victimEntity, tick);
                changed = true;
            }
        } else if (reg.all_of<ecs::CombatTarget>(entity)) {
            reg.remove<ecs::CombatTarget>(entity);
            changed = true;
        }

        auto& aiState = reg.get_or_emplace<ecs::AIState>(entity);
        const uint8_t observedState = ObserveAIState(reg, entity, ch);
        if (aiState.currentState != observedState) {
            aiState.currentState = observedState;
            changed = true;
        }

        if (changed) {
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
        }
    }
}
