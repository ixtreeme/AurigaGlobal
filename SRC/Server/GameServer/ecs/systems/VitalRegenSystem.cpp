#include "../../stdafx.h"

#include "VitalRegenSystem.hpp"

#include <algorithm>

#include "../components/dirty_components.hpp"
#include "../components/status_components.hpp"
#include "../components/vital_components.hpp"
#include "../../config.h"

void VitalRegenSystem_Update(entt::registry& reg, uint32_t tick)
{
    if ((tick % PASSES_PER_SEC(1)) != 0) {
        return;
    }

    auto view = reg.view<ecs::Health, ecs::Mana, ecs::Stamina, ecs::LevelComponent>(entt::exclude<ecs::DeadTag, ecs::StunTag>);
    view.each([&](const entt::entity entity, ecs::Health& health, ecs::Mana& mana, ecs::Stamina& stamina, const ecs::LevelComponent& level) {
        const int32_t regenStep = std::max<int32_t>(1, level.value / 10);

        const int32_t oldHP = health.current;
        const int32_t oldMana = mana.current;
        const int32_t oldStamina = stamina.current;

        health.current = std::min(health.max, health.current + regenStep);
        mana.current = std::min(mana.max, mana.current + regenStep);
        stamina.current = std::min(stamina.max, stamina.current + regenStep);

        if (health.current != oldHP || mana.current != oldMana || stamina.current != oldStamina) {
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
        }
    });
}
