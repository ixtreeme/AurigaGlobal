#include "../../stdafx.h"
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
#include "../VIDRegistry.hpp"
#include "../CharacterAccessors.hpp"
#include "../components/ai_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include <Core/Logging.hpp>
#include "CombatSystem.hpp"

namespace {

constexpr uint8_t AI_STATE_IDLE = 0;
constexpr uint8_t AI_STATE_CHASE = 1;
constexpr uint8_t AI_STATE_ATTACK = 2;
constexpr uint8_t AI_STATE_RETURN = 3;

LPCHARACTER LegacyCharBoundary(entt::registry& reg, entt::entity entity)
{
    if (entity == entt::null || !reg.valid(entity)) {
        return nullptr;
    }

    const auto* legacy = reg.try_get<ecs::LegacyCharPtr>(entity);
    return legacy ? legacy->ptr : nullptr;
}

uint8_t ObserveAIState(entt::registry& reg, entt::entity entity, LPCHARACTER ch)
{
    if (!ch || ecs::PlayerRuntime::IsPC(entity)) {
        return AI_STATE_IDLE;
    }

    if (LPCHARACTER victim = ch->GetVictim()) {
        const entt::entity victimEntity = victim->GetEntityHandle();
        const int32_t dx = ecs::PlayerRuntime::GetX(victimEntity) - ecs::PlayerRuntime::GetX(entity);
        const int32_t dy = ecs::PlayerRuntime::GetY(victimEntity) - ecs::PlayerRuntime::GetY(entity);
        const int32_t distance = DISTANCE_APPROX(dx, dy);
        const int32_t attackRange = static_cast<int32_t>(ch->GetMobAttackRange()) * 100;
        return distance <= attackRange ? AI_STATE_ATTACK : AI_STATE_CHASE;
    }

    if (const auto* spawn = reg.try_get<ecs::SpawnInfo>(entity)) {
        if (ecs::PlayerRuntime::GetX(entity) != spawn->x || ecs::PlayerRuntime::GetY(entity) != spawn->y) {
            return AI_STATE_RETURN;
        }
    }

    return AI_STATE_IDLE;
}

bool SyncAIFlags(entt::registry& reg, entt::entity entity, LPCHARACTER ch)
{
    auto& flags = reg.get_or_emplace<ecs::AIFlags>(entity);
    const uint32_t aiFlags = ecs::PlayerRuntime::GetAIFlag(entity);
    const ecs::AIFlags desired {
        IS_SET(aiFlags, AIFLAG_AGGRESSIVE) != 0,
        IS_SET(aiFlags, AIFLAG_COWARD) != 0,
        IS_SET(aiFlags, AIFLAG_ATTACKMOB) != 0,
        IS_SET(aiFlags, AIFLAG_NOATTACKSHINSU) != 0,
        IS_SET(aiFlags, AIFLAG_NOATTACKCHUNJO) != 0,
        IS_SET(aiFlags, AIFLAG_NOATTACKJINNO) != 0,
        ch->IsBerserk(),
        ecs::PlayerRuntime::IsGuardNPC(ch->GetEntityHandle()),
        false,
        ch->IsStoneSkinner(),
        ch->IsGodSpeed(),
        CombatSystem::IsDeathBlow(ch->GetEntityHandle()),
        ch->IsRevive(),
        flags.isNoMove,
    };

    if (std::memcmp(&flags, &desired, sizeof(ecs::AIFlags)) == 0) {
        return false;
    }

    flags = desired;
    return true;
}

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

extern LPCHARACTER FindVictim(LPCHARACTER pkChr, int iMaxDistance);

namespace AISystem {

void GotoState(entt::entity e, ecs::AIFSMState state)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& fsm = g_registry.get_or_emplace<ecs::AIStateMachine>(e);

    // CFSM::GotoState returned early when the machine was already in the state
    // and that same state was queued; anything else just replaced the pending
    // slot, so a transition never took effect before the next Update.
    if (fsm.current == state && fsm.hasPending && fsm.pending == state)
        return;

    fsm.pending = state;
    fsm.hasPending = true;
}

void UpdateStateMachine(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    auto& fsm = g_registry.get_or_emplace<ecs::AIStateMachine>(e);
    const bool transitioned = fsm.hasPending;
    if (transitioned) {
        fsm.current = fsm.pending;
        fsm.hasPending = false;
    }

    // The state bodies are still CHARACTER methods. This is the once-per-tick
    // boundary the pump already crosses for everything else, not a wrapper
    // hiding a conversion behind an entity-shaped call.
    LPCHARACTER ch = ecs::LegacyCharOf(e);
    if (!ch)
        return;

    // CFSM ran the old state's End hook then the new state's Begin hook on a
    // transition. EndStateEmpty is genuinely empty; BeginStateEmpty is not -
    // it logs - so it has to keep firing.
    if (transitioned)
        ch->BeginStateEmpty();

    switch (fsm.current) {
    case ecs::AIFSMState::Battle:
        ch->StateBattle();
        break;
    case ecs::AIFSMState::Idle:
        ch->StateIdle();
        break;
    case ecs::AIFSMState::Initial:
        break;  // CFSM's m_stateInitial had empty hooks on CHARACTER
    }
}

} // namespace AISystem

void CHARACTER::StateIdle()
{
    if (IsStone()) {
        m_dwStateDuration = PASSES_PER_SEC(1);
        return;
    }

    if (IsWarp() || IsGoto()) {
        m_dwStateDuration = 60 * passes_per_sec;
        return;
    }

    if (IsPC()) {
        return;
    }

    if (!IsMonster()) {
        __StateIdle_NPC();
        return;
    }

    __StateIdle_Monster();
}

void CHARACTER::__StateIdle_NPC()
{
    m_dwStateDuration = PASSES_PER_SEC(5);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (IsMount()) {
        return;
    }
#endif

#ifdef __NEWPET_SYSTEM__
    if (IsPet() || IsNewPet()) {
        return;
    }
#else
    if (IsPet()) {
        return;
    }
#endif

    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(GetEntityHandle()), AIFLAG_NOMOVE)) {
        return;
    }

    LPCHARACTER protege = GetProtege();
    if (protege && DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX(protege->GetEntityHandle()), GetY() - ecs::PlayerRuntime::GetY(protege->GetEntityHandle())) > 500) {
        if (Follow(protege ? protege->GetEntityHandle() : entt::null, number(100, 300))) {
            return;
        }
    }

    if (number(0, 6)) {
        return;
    }

    SetRotation(number(0, 359));

    float fx = 0.0f;
    float fy = 0.0f;
    const float dist = number(200, 400);
    GetDeltaByDegree(GetRotation(), dist, &fx, &fy);

    if (!(SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + static_cast<int>(fx), GetY() + static_cast<int>(fy)) &&
        SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + static_cast<int>(fx) / 2, GetY() + static_cast<int>(fy) / 2))) {
        return;
    }

    SetNowWalking(true);
    if (Goto(GetX() + static_cast<int>(fx), GetY() + static_cast<int>(fy))) {
        SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
    }
}

void CHARACTER::__StateIdle_Monster()
{
    if (IsStun() || !CanMove()) {
        return;
    }

    if (AIHelpers::IsCoward(GetEntityHandle())) {
        if (!IsDead()) {
            CowardEscape();
        }
        return;
    }

    if (IsBerserker() && IsBerserk()) {
        SetBerserk(false);
    }

    if (IsGodSpeeder() && IsGodSpeed()) {
        SetGodSpeed(false);
    }

    LPCHARACTER victim = GetVictim();
    if (!victim || victim->IsDead()) {
        SetVictim(entt::null);
        victim = nullptr;
        m_dwStateDuration = PASSES_PER_SEC(1);
    }

    if (!victim || victim->IsBuilding()) {
        if (m_pkChrStone) {
            victim = m_pkChrStone->GetNearestVictim((m_pkChrStone ? m_pkChrStone->GetEntityHandle() : entt::null));
        } else if (!no_wander && AIHelpers::IsAggressive(GetEntityHandle())) {
            victim = FindVictim(this, m_pkMobData->m_table.wAggressiveSight);
        }
    }

    if (victim && !victim->IsDead()) {
        if (CanBeginFight()) {
            BeginFight(victim ? victim->GetEntityHandle() : entt::null);
        }
        return;
    }

    m_dwStateDuration = AIHelpers::IsAggressive(GetEntityHandle()) && !victim
        ? PASSES_PER_SEC(number(1, 3))
        : PASSES_PER_SEC(number(3, 5));

    LPCHARACTER protege = GetProtege();
    if (protege && DISTANCE_APPROX(GetX() - ecs::PlayerRuntime::GetX(protege->GetEntityHandle()), GetY() - ecs::PlayerRuntime::GetY(protege->GetEntityHandle())) > 1000) {
        if (Follow(protege ? protege->GetEntityHandle() : entt::null, number(150, 400))) {
            MonsterLog("[IDLE] returning to protege");
            return;
        }
    }

    if (no_wander || IS_SET(ecs::PlayerRuntime::GetAIFlag(GetEntityHandle()), AIFLAG_NOMOVE) || number(0, 6)) {
        return;
    }

    SetRotation(number(0, 359));

    float fx = 0.0f;
    float fy = 0.0f;
    const float dist = number(300, 700);
    GetDeltaByDegree(GetRotation(), dist, &fx, &fy);

    if (!(SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + static_cast<int>(fx), GetY() + static_cast<int>(fy)) &&
        SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + static_cast<int>(fx) / 2, GetY() + static_cast<int>(fy) / 2))) {
        return;
    }

    if (test_server) {
        SetNowWalking(number(0, 100) >= 60);
    }

    if (Goto(GetX() + static_cast<int>(fx), GetY() + static_cast<int>(fy))) {
        SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
    }
}

void CHARACTER::StateBattle()
{
    if (IsStone()) {
        LOG_ERROR("Stone must not use battle state (name {})", GetName());
        return;
    }

    if (IsPC() || !CanMove() || IsStun()) {
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
            m_dwStateDuration = 1;
        } else {
            CowardEscape();
        }
        return;
    }

    if (!victim || (victim->IsStun() && ecs::PlayerRuntime::IsGuardNPC(GetEntityHandle())) || victim->IsDead()) {
        LPCHARACTER newVictim = nullptr;
        if (victim && victim->IsDead() && !no_wander && AIHelpers::IsAggressive(GetEntityHandle()) && (!GetParty() || GetParty()->GetLeader() == this)) {
            newVictim = FindVictim(this, m_pkMobData->m_table.wAggressiveSight);
        }

        if (newVictim) {
            SetVictim(newVictim ? newVictim->GetEntityHandle() : entt::null);
            m_dwStateDuration = PASSES_PER_SEC(1);
            return;
        }

        SetVictim(entt::null);
        if (ecs::PlayerRuntime::IsGuardNPC(GetEntityHandle())) {
            Return();
        } else {
            SetPosition(POS_STANDING);
        }
        m_dwStateDuration = PASSES_PER_SEC(1);
        return;
    }

    LPCHARACTER protege = GetProtege();
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
            m_dwStateDuration = 1;
        }
        return;
    }

    if (m_pkParty) {
        m_pkParty->SendMessage(GetEntityHandle(), PM_ATTACKED_BY, 0, 0);
    }

    const uint32_t curTime = get_dword_time();
    const uint32_t duration = CalculateDuration(GetLimitPoint(POINT_ATT_SPEED), 2000);
    if ((curTime - m_dwLastAttackTime) < duration) {
        m_dwStateDuration = MAX(1, (passes_per_sec * (duration - (curTime - m_dwLastAttackTime)) / 1000));
        return;
    }

    if (IsBerserker() && GetHPPct() < m_pkMobData->m_table.bBerserkPoint && !IsBerserk()) {
        SetBerserk(true);
    }

    if (IsGodSpeeder() && GetHPPct() < m_pkMobData->m_table.bGodSpeedPoint && !IsGodSpeed()) {
        SetGodSpeed(true);
    }

    if (HasMobSkill()) {
        for (unsigned int skillIdx = 0; skillIdx < MOB_SKILL_MAX_NUM; ++skillIdx) {
            if (!CanUseMobSkill(skillIdx)) {
                continue;
            }

            SetRotationToXY(ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity));
            if (UseMobSkill(skillIdx)) {
                SendMovePacket(FUNC_MOB_SKILL, skillIdx, GetX(), GetY(), 0, curTime);

                const float motionDuration = CMotionManager::instance().GetMotionDuration(
                    GetRaceNum(),
                    MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + skillIdx));
                m_dwStateDuration = static_cast<uint32_t>(
                    motionDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(motionDuration));
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
        m_dwStateDuration = passes_per_sec / 2;
        return;
    }

    SetRotationToXY(ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity));
    SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0, curTime);

    const float motionDuration = CMotionManager::instance().GetMotionDuration(
        GetRaceNum(),
        MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK));
    m_dwStateDuration = static_cast<uint32_t>(
        motionDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(motionDuration));
}

void AISystem_Update(entt::registry& reg, uint32_t tick)
{
    (void)tick;

    auto view = reg.view<ecs::VIDComponent>();

    for (auto entity : view) {
        LPCHARACTER ch = LegacyCharBoundary(reg, entity);
        if (!ch || ecs::PlayerRuntime::IsPC(entity)) {
            continue;
        }

        bool changed = SyncAIFlags(reg, entity, ch);

        entt::entity victimEntity = entt::null;
        if (LPCHARACTER victim = ch->GetVictim()) {
            victimEntity = victim->GetEntityHandle();
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
