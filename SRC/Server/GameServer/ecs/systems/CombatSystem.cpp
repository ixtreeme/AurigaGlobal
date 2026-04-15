#include "../../stdafx.h"

#include "CombatSystem.hpp"

#include <algorithm>

#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/status_components.hpp"
#include "../components/vital_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include "../../config.h"

void CombatSystem_Update(entt::registry& reg, uint32_t tick)
{
    // migrated from CHARACTER::Attack
    auto view = reg.view<ecs::CombatActiveTag, ecs::CombatTarget, ecs::CombatStats, ecs::AttackCooldown, ecs::Health>();

    view.each([&](const entt::entity entity,
                  ecs::CombatTarget& combatTarget,
                  ecs::CombatStats& combatStats,
                  ecs::AttackCooldown& attackCooldown,
                  ecs::Health& attackerHealth) {
        (void)combatStats;
        (void)attackerHealth;

        if (combatTarget.target == entt::null || !reg.valid(combatTarget.target) ||
            !reg.all_of<ecs::Health>(combatTarget.target) ||
            reg.all_of<ecs::DeadTag>(combatTarget.target))
        {
            combatTarget.target = entt::null;
            reg.remove<ecs::CombatActiveTag>(entity);
            return;
        }

        const uint32_t attackPeriod = PASSES_PER_SEC(1);
        if (tick < attackCooldown.lastAttackTime || (tick - attackCooldown.lastAttackTime) < attackPeriod) {
            return;
        }

        auto& victimHealth = reg.get<ecs::Health>(combatTarget.target);
        const int32_t damage = 1;
        victimHealth.current = std::max<int32_t>(0, victimHealth.current - damage);
        attackCooldown.lastAttackTime = tick;

        reg.emplace_or_replace<ecs::DirtyTag>(combatTarget.target);
        g_dispatcher.trigger(ecs::EvEntityDamaged { entity, combatTarget.target, damage, DAMAGE_TYPE_NORMAL });

        if (victimHealth.current > 0) {
            return;
        }

        reg.emplace_or_replace<ecs::DeadTag>(combatTarget.target);
        if (auto* statusFlags = reg.try_get<ecs::StatusFlags>(combatTarget.target)) {
            statusFlags->isDead = true;
        }

        combatTarget.target = entt::null;
        reg.remove<ecs::CombatActiveTag>(entity);
        g_dispatcher.trigger(ecs::EvEntityDied { entity, combatTarget.target });
    });
}
