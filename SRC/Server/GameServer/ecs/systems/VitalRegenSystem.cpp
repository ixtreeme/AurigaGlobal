#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "../AIHelpers.hpp"

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

#if defined(DEBUG_STATS_DRIFT)
namespace {

void DebugStatsDriftCheck(entt::registry& reg)
{
    static uint32_t s_lastCheckPulse = 0;
    const uint32_t now = get_dword_time();
    if (s_lastCheckPulse != 0 && now - s_lastCheckPulse < 1000) {
        return;
    }
    s_lastCheckPulse = now;

    static constexpr uint32_t kCheckPoints[] = {
        POINT_ST, POINT_HT, POINT_DX, POINT_IQ,
        POINT_ATT_GRADE, POINT_DEF_GRADE,
        POINT_ATT_SPEED, POINT_MOV_SPEED,
        POINT_CRITICAL_PCT, POINT_PENETRATE_PCT,
        POINT_STAT, POINT_SKILL,
    };

    auto view = reg.view<ecs::LegacyCharPtr, ecs::CharacterStatsComponent>();
    for (auto entity : view) {
        auto* ch = view.get<ecs::LegacyCharPtr>(entity).ptr;
        auto& stats = view.get<ecs::CharacterStatsComponent>(entity);
        if (!ch) {
            continue;
        }

        for (uint32_t point : kCheckPoints) {
            if (!ecs::IsStatArrayPoint(static_cast<uint8_t>(point))) {
                continue;
            }

            if (stats.points[point] != ch->GetPoint(static_cast<uint8_t>(point))) {
                LOG_ERROR("STATS_DRIFT pt={} ecs={} legacy={} name={}", point, static_cast<long long>(stats.points[point]), static_cast<long long>(ch->GetPoint(static_cast<uint8_t>(point))), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
            }
        }
    }
}

} // namespace
#endif

void VitalRegenSystem_Update(entt::registry& reg, uint32_t tick)
{
    (void)tick;

#if defined(DEBUG_STATS_DRIFT)
    DebugStatsDriftCheck(reg);
#endif

    auto view = reg.view<ecs::LegacyCharPtr, ecs::Health, ecs::Mana>();
    view.each([&](const entt::entity entity, const ecs::LegacyCharPtr& legacy, ecs::Health& health, ecs::Mana& mana) {
        LPCHARACTER ch = legacy.ptr;
        if (!ch) {
            return;
        }

        if (ch->IsDead()) {
            return;
        }

        if (!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) && !ecs::PlayerRuntime::IsNPC(AIHelpers::EcsOf(ch))) {
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
