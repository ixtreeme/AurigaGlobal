#include "../../stdafx.h"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "CombatSystem.hpp"

#include "VitalRegenSystem.hpp"

#include "../Registry.hpp"
#include "../PointSemantic.hpp"
#include "../components/character_stats_components.hpp"
#include "../VIDRegistry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/vital_components.hpp"
#include "../../char.h"
#include "../../char_interface.hpp"
#include "../../char_manager.h"
#include "../../utils.h"
#include <Core/Logging.hpp>

void VitalRegenSystem_Update(entt::registry& reg, uint32_t tick)
{
    (void)tick;

    auto view = reg.view<ecs::LegacyCharPtr, ecs::Health, ecs::Mana>();
    view.each([&](const entt::entity entity, const ecs::LegacyCharPtr& legacy, ecs::Health& health, ecs::Mana& mana) {
        LPCHARACTER ch = legacy.ptr;
        if (!ch) {
            return;
        }

        if (CombatSystem::IsDead(ch->GetEntityHandle())) {
            return;
        }

        // This mirror was written against the NPC-only answer and there is no
        // class original to check it against, so it keeps what it had.
        if (!ecs::PlayerRuntime::GetDesc(entity) && !ecs::PlayerRuntime::IsNPCType(entity)) {
            return;
        }

        const int32_t oldHP = health.current;
        const int32_t oldHPMax = health.max;
        const int32_t oldMana = mana.current;
        const int32_t oldManaMax = mana.max;

        health.current = ecs::PlayerRuntime::GetHP(ch->GetEntityHandle());
        health.max = ecs::PointSystem::GetMaxHP(entity);
        mana.current = ecs::PlayerRuntime::GetSP(ch->GetEntityHandle());
        mana.max = ecs::PointSystem::GetMaxSP(entity);

        if (auto* stamina = reg.try_get<ecs::Stamina>(entity)) {
            stamina->current = ecs::PlayerRuntime::GetStamina(ch->GetEntityHandle());
            stamina->max = ecs::PlayerRuntime::GetMaxStamina(ch->GetEntityHandle());
        }

        if (health.current != oldHP || health.max != oldHPMax ||
            mana.current != oldMana || mana.max != oldManaMax) {
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
        }
    });
}
