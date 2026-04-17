#include "../../stdafx.h"

#include "VitalRegenSystem.hpp"

#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/vital_components.hpp"
#include "../../char_interface.hpp"
#include "../../char_manager.h"

namespace
{

inline LPCHARACTER LegacyCharOf(entt::entity e)
{
    auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    if (!vid) {
        return nullptr;
    }

    return CHARACTER_MANAGER::instance().Find(vid->value);
}

} // namespace

void VitalRegenSystem_Update(entt::registry& reg, uint32_t tick)
{
    (void)tick;

    auto view = reg.view<ecs::VIDComponent, ecs::Health, ecs::Mana>();
    view.each([&](const entt::entity entity, const ecs::VIDComponent&, ecs::Health& health, ecs::Mana& mana) {
        LPCHARACTER ch = LegacyCharOf(entity);
        if (!ch) {
            return;
        }

        if (ch->IsDead()) {
            return;
        }

        if (!ch->GetDesc() && !ch->IsNPC()) {
            return;
        }

        const int32_t oldHP = health.current;
        const int32_t oldHPMax = health.max;
        const int32_t oldMana = mana.current;
        const int32_t oldManaMax = mana.max;

        health.current = ch->GetHP();
        health.max = ch->GetMaxHP();
        mana.current = ch->GetSP();
        mana.max = ch->GetMaxSP();

        if (auto* stamina = reg.try_get<ecs::Stamina>(entity)) {
            stamina->current = ch->GetStamina();
            stamina->max = ch->GetMaxStamina();
        }

        if (health.current != oldHP || health.max != oldHPMax ||
            mana.current != oldMana || mana.max != oldManaMax) {
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
        }
    });
}
