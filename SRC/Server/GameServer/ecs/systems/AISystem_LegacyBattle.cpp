#include "../../stdafx.h"
#include "MovementSystem.hpp"
#include "PlayerRuntimeSystem.hpp"

#include "AISystem.hpp"

#include "../../config.h"
#include "../../char_interface.hpp"
#include "../../char_manager.h"
#include "../../packet.h"
#include "../../party.h"
#include "../../sectree_manager.h"
#include "../../motion.h"
#include "../../vector.h"
#include "../CharacterAccessors.hpp"
#include "../SpatialHelpers.hpp"
#include "../AIHelpers.hpp"
#include "../components/ai_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/identity_components.hpp"
#include <Core/Logging.hpp>
#include "CombatSystem.hpp"

// What is left of the AI on CHARACTER. StateIdle moved to AISystem.cpp as
// entity-native code; StateBattle follows in its own step, and this file goes
// with it. Keeping it apart is what lets AISystem.cpp be linked into a test
// without dragging in attack, mob skills and the motion manager.


namespace {

bool LegacyGotoNearTarget(LPCHARACTER self, LPCHARACTER victim)
{
    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(self->GetEntityHandle()), AIFLAG_NOMOVE)) {
        return false;
    }

    switch (self->GetMobBattleType()) {
    case BATTLE_TYPE_RANGE:
    case BATTLE_TYPE_MAGIC:
        if (self->Follow(victim ? victim->GetEntityHandle() : entt::null, self->GetMobAttackRange() * 8 / 10)) {
            return true;
        }
        break;

    default:
        if (self->Follow(victim ? victim->GetEntityHandle() : entt::null, self->GetMobAttackRange() * 9 / 10)) {
            return true;
        }
        break;
    }

    return self->Follow(victim ? victim->GetEntityHandle() : entt::null, self->GetMobAttackRange() * 9 / 10);
}

} // namespace

void CHARACTER::StateBattle()
{
    if (IsStone()) {
        LOG_ERROR("Stone must not use battle state (name {})", GetName());
        return;
    }

    if (IsPC() || !ecs::MovementSystem::CanMove(GetEntityHandle()) || IsStun()) {
        return;
    }

    LPCHARACTER victim = GetVictim();

    if (AIHelpers::IsCoward(GetEntityHandle())) {
        if (IsDead()) {
            return;
        }

        SetVictim(entt::null);
        if (number(1, 50) != 1) {
            SetPosition(POS_STANDING);
            AIHelpers::SetStateDuration(GetEntityHandle(), 1);
        } else {
            CombatSystem::CowardEscape(GetEntityHandle());
        }
        return;
    }

    if (!victim || (victim->IsStun() && ecs::PlayerRuntime::IsGuardNPC(GetEntityHandle())) || victim->IsDead()) {
        LPCHARACTER newVictim = nullptr;
        if (victim && victim->IsDead() && !no_wander && AIHelpers::IsAggressive(GetEntityHandle()) && (!GetParty() || GetParty()->GetLeader() == this)) {
            newVictim = ecs::LegacyCharOf(
                CombatSystem::FindVictim(GetEntityHandle(), m_pkMobData->m_table.wAggressiveSight));
        }

        if (newVictim) {
            SetVictim(newVictim ? newVictim->GetEntityHandle() : entt::null);
            AIHelpers::SetStateDuration(GetEntityHandle(), PASSES_PER_SEC(1));
            return;
        }

        SetVictim(entt::null);
        if (ecs::PlayerRuntime::IsGuardNPC(GetEntityHandle())) {
            Return();
        } else {
            SetPosition(POS_STANDING);
        }
        AIHelpers::SetStateDuration(GetEntityHandle(), PASSES_PER_SEC(1));
        return;
    }

    const entt::entity protegeEntity = CombatSystem::GetProtege(GetEntityHandle());
    LPCHARACTER protege = ecs::LegacyCharOf(protegeEntity);
    const entt::entity victimEntity = victim->GetEntityHandle();
    const float dist = static_cast<float>(DISTANCE_APPROX(
        GetX() - ecs::PlayerRuntime::GetX(victimEntity),
        GetY() - ecs::PlayerRuntime::GetY(victimEntity)));

    if (dist >= 4000.0f) {
        SetVictim(entt::null);
        if (protege && DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX(protege->GetEntityHandle()), GetY() - ecs::PlayerRuntime::GetY(protege->GetEntityHandle())) > 1000) {
            Follow(protege ? protege->GetEntityHandle() : entt::null, number(150, 400));
        } else {
            SetPosition(POS_STANDING);
        }
        return;
    }

    if (dist >= GetMobAttackRange() * 1.15f) {
        if (LegacyGotoNearTarget(this, victim)) {
            AIHelpers::SetStateDuration(GetEntityHandle(), 1);
        }
        return;
    }

    if (m_pkParty) {
        m_pkParty->SendMessage(GetEntityHandle(), PM_ATTACKED_BY, 0, 0);
    }

    const uint32_t curTime = get_dword_time();
    const uint32_t duration = CalculateDuration(GetLimitPoint(POINT_ATT_SPEED), 2000);
    if ((curTime - GetLastAttackTime()) < duration) {
        AIHelpers::SetStateDuration(GetEntityHandle(), MAX(1, (passes_per_sec * (duration - (curTime - GetLastAttackTime())) / 1000)));
        return;
    }

    if (CombatSystem::IsBerserker(GetEntityHandle()) && GetHPPct() < m_pkMobData->m_table.bBerserkPoint && !IsBerserk()) {
        SetBerserk(true);
    }

    if (CombatSystem::IsGodSpeeder(GetEntityHandle()) && GetHPPct() < m_pkMobData->m_table.bGodSpeedPoint && !IsGodSpeed()) {
        SetGodSpeed(true);
    }

    if (HasMobSkill()) {
        for (unsigned int skillIdx = 0; skillIdx < MOB_SKILL_MAX_NUM; ++skillIdx) {
            if (!CanUseMobSkill(skillIdx)) {
                continue;
            }

            SetRotationToXY(ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity));
            if (UseMobSkill(skillIdx)) {
                ecs::MovementSystem::SendMovePacket(GetEntityHandle(), FUNC_MOB_SKILL, skillIdx, GetX(), GetY(), 0, curTime);

                const float motionDuration = CMotionManager::instance().GetMotionDuration(
                    GetRaceNum(),
                    MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + skillIdx));
                AIHelpers::SetStateDuration(GetEntityHandle(), static_cast<uint32_t>(
                    motionDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(motionDuration)));
                return;
            }
        }
    }

    if (!IsPC()) {
        const int32_t vnum = GetRaceNum();
#ifdef ENABLE_MELEY_LAIR
        if (vnum == 6193) {
            return;
        }
#endif
#ifdef ENABLE_ANCIENT_PYRAMID
        if (vnum == PYRAMID_BOSSVNUM) {
            return;
        }
#endif
#ifdef __DEFENSE_WAVE__
        if (vnum >= 3960 && vnum <= 3962) {
            return;
        }
#endif
    }

    if (!Attack(victim ? victim->GetEntityHandle() : entt::null, 0)) {
        AIHelpers::SetStateDuration(GetEntityHandle(), passes_per_sec / 2);
        return;
    }

    SetRotationToXY(ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity));
    ecs::MovementSystem::SendMovePacket(GetEntityHandle(), FUNC_ATTACK, 0, GetX(), GetY(), 0, curTime);

    const float motionDuration = CMotionManager::instance().GetMotionDuration(
        GetRaceNum(),
        MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK));
    AIHelpers::SetStateDuration(GetEntityHandle(), static_cast<uint32_t>(
        motionDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(motionDuration)));
}

