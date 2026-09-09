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
#include "../VIDRegistry.hpp"
#include "../CharacterAccessors.hpp"
#include "../SpatialHelpers.hpp"
#include "../components/ai_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include <Core/Logging.hpp>
#include "CombatSystem.hpp"
#include "../../utils.h"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "SkillSystem.hpp"

namespace {

constexpr uint8_t AI_STATE_IDLE = 0;
constexpr uint8_t AI_STATE_CHASE = 1;
constexpr uint8_t AI_STATE_ATTACK = 2;
constexpr uint8_t AI_STATE_RETURN = 3;

uint8_t ObserveAIState(entt::registry& reg, entt::entity entity)
{
    if (ecs::PlayerRuntime::IsPC(entity)) {
        return AI_STATE_IDLE;
    }

    // The victim comes back as a handle and stays one; asking the character for
    // it only to convert the answer back was a round trip with a dangling
    // pointer in the middle of it.
    if (const entt::entity victim = CombatSystem::GetVictim(entity);
        victim != entt::null && reg.valid(victim)) {
        const int32_t dx = ecs::PlayerRuntime::GetX(victim) - ecs::PlayerRuntime::GetX(entity);
        const int32_t dy = ecs::PlayerRuntime::GetY(victim) - ecs::PlayerRuntime::GetY(entity);
        const int32_t distance = DISTANCE_APPROX(dx, dy);
        const int32_t attackRange =
            static_cast<int32_t>(CombatSystem::GetMobAttackRange(entity)) * 100;
        return distance <= attackRange ? AI_STATE_ATTACK : AI_STATE_CHASE;
    }

    if (const auto* spawn = reg.try_get<ecs::SpawnInfo>(entity)) {
        if (ecs::PlayerRuntime::GetX(entity) != spawn->x || ecs::PlayerRuntime::GetY(entity) != spawn->y) {
            return AI_STATE_RETURN;
        }
    }

    return AI_STATE_IDLE;
}

bool SyncAIFlags(entt::registry& reg, entt::entity entity)
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
        CombatSystem::IsBerserk(entity),
        ecs::PlayerRuntime::IsGuardNPC(entity),
        false,
        CombatSystem::IsStoneSkinner(entity),
        CombatSystem::IsGodSpeed(entity),
        CombatSystem::IsDeathBlow(entity),
        CombatSystem::IsRevive(entity),
        flags.isNoMove,
    };

    if (std::memcmp(&flags, &desired, sizeof(ecs::AIFlags)) == 0) {
        return false;
    }

    flags = desired;
    return true;
}

} // namespace


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

    // CFSM ran the old End hook then the new Begin hook on a transition.
    // EndStateEmpty was genuinely empty; BeginStateEmpty was a single
    // MonsterLog call, which the entity can make for itself - so the pump no
    // longer resolves a character at all.
    if (transitioned)
        ecs::PlayerRuntime::MonsterLog(e, "!");

    switch (fsm.current) {
    case ecs::AIFSMState::Battle:
        StateBattle(e);
        break;
    case ecs::AIFSMState::Idle:
        StateIdle(e);
        break;
    case ecs::AIFSMState::Initial:
        break;  // CFSM's m_stateInitial had empty hooks on CHARACTER
    }
}

} // namespace AISystem

namespace AISystem {

// Idle, entity-native. Wandering, protege-following and target acquisition all
// read components; the two calls left on a character are named where they are.

namespace {

void StateIdle_NPC(entt::entity e)
{
    AIHelpers::SetStateDuration(e, PASSES_PER_SEC(5));

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    if (ecs::PlayerRuntime::IsMount(e))
        return;
#endif

#ifdef __NEWPET_SYSTEM__
    if (ecs::PlayerRuntime::IsPet(e) || ecs::PlayerRuntime::IsNewPet(e))
        return;
#else
    if (ecs::PlayerRuntime::IsPet(e))
        return;
#endif

    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(e), AIFLAG_NOMOVE))
        return;

    const int32_t x = ecs::PlayerRuntime::GetX(e);
    const int32_t y = ecs::PlayerRuntime::GetY(e);

    if (const entt::entity protege = CombatSystem::GetProtege(e); protege != entt::null) {
        const int32_t dx = x - ecs::PlayerRuntime::GetX(protege);
        const int32_t dy = y - ecs::PlayerRuntime::GetY(protege);
        if (DISTANCE_APPROX(dx, dy) > 500) {
            // Follow is 146 lines of legacy pathing and is its own migration
            // unit; this is the one operation idle still needs a character for.
            if (CombatSystem::Follow(e, protege, number(100, 300)))
                return;
        }
    }

    if (number(0, 6))
        return;

    ecs::MovementSystem::SetRotation(e, number(0, 359));

    float fx = 0.0f;
    float fy = 0.0f;
    GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(e), number(200, 400), &fx, &fy);

    const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(e);
    const int32_t destX = x + static_cast<int>(fx);
    const int32_t destY = y + static_cast<int>(fy);
    if (!(ecs::IsMovablePosition(mapIndex, destX, destY) &&
          ecs::IsMovablePosition(mapIndex, x + static_cast<int>(fx) / 2, y + static_cast<int>(fy) / 2)))
        return;

    ecs::MovementSystem::SetNowWalking(e, true);
    if (ecs::MovementSystem::Goto(e, destX, destY))
        ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 0);
}

void StateIdle_Monster(entt::entity e)
{
    if (CombatSystem::IsStun(e) || !ecs::MovementSystem::CanMove(e))
        return;

    if (AIHelpers::IsCoward(e)) {
        if (!CombatSystem::IsDead(e))
            CombatSystem::CowardEscape(e);
        return;
    }

    if (CombatSystem::IsBerserker(e) && CombatSystem::IsBerserk(e))
        CombatSystem::SetBerserk(e, false);

    if (CombatSystem::IsGodSpeeder(e) && CombatSystem::IsGodSpeed(e))
        CombatSystem::SetGodSpeed(e, false);

    entt::entity victim = CombatSystem::GetVictim(e);
    if (victim == entt::null || !g_registry.valid(victim) || CombatSystem::IsDead(victim)) {
        CombatSystem::SetVictim(e, entt::null);
        victim = entt::null;
        AIHelpers::SetStateDuration(e, PASSES_PER_SEC(1));
    }

    if (victim == entt::null || ecs::PlayerRuntime::IsBuilding(victim)) {
        if (const entt::entity stone = CombatSystem::GetStone(e); stone != entt::null) {
            victim = CombatSystem::GetNearestVictim(stone, stone);
        } else if (!no_wander && AIHelpers::IsAggressive(e)) {
            const TMobTable* table = ecs::PlayerRuntime::GetMobTable(e);
            victim = table ? CombatSystem::FindVictim(e, table->wAggressiveSight)
                           : entt::null;
        }
    }

    // The search result gets the same treatment: it was chosen during a
    // sectree scan, and a callback in between can have retired it.
    if (victim != entt::null && g_registry.valid(victim) && !CombatSystem::IsDead(victim)) {
        if (CombatSystem::CanBeginFight(e))
            CombatSystem::BeginFight(e, victim);
        return;
    }

    AIHelpers::SetStateDuration(e, AIHelpers::IsAggressive(e) && victim == entt::null
        ? PASSES_PER_SEC(number(1, 3))
        : PASSES_PER_SEC(number(3, 5)));

    const int32_t x = ecs::PlayerRuntime::GetX(e);
    const int32_t y = ecs::PlayerRuntime::GetY(e);

    if (const entt::entity protege = CombatSystem::GetProtege(e); protege != entt::null) {
        const int32_t dx = x - ecs::PlayerRuntime::GetX(protege);
        const int32_t dy = y - ecs::PlayerRuntime::GetY(protege);
        if (DISTANCE_APPROX(dx, dy) > 1000) {
            if (CombatSystem::Follow(e, protege, number(150, 400))) {
                ecs::PlayerRuntime::MonsterLog(e, "[IDLE] returning to protege");
                return;
            }
        }
    }

    if (no_wander || IS_SET(ecs::PlayerRuntime::GetAIFlag(e), AIFLAG_NOMOVE) || number(0, 6))
        return;

    ecs::MovementSystem::SetRotation(e, number(0, 359));

    float fx = 0.0f;
    float fy = 0.0f;
    GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(e), number(300, 700), &fx, &fy);

    const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(e);
    const int32_t destX = x + static_cast<int>(fx);
    const int32_t destY = y + static_cast<int>(fy);
    if (!(ecs::IsMovablePosition(mapIndex, destX, destY) &&
          ecs::IsMovablePosition(mapIndex, x + static_cast<int>(fx) / 2, y + static_cast<int>(fy) / 2)))
        return;

    if (test_server)
        ecs::MovementSystem::SetNowWalking(e, number(0, 100) >= 60);

    if (ecs::MovementSystem::Goto(e, destX, destY))
        ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 0);
}

} // namespace

namespace {

// Close the distance. Ranged and magic mobs stop a little further out; the
// repeated fallback call is the original one, kept as it was.
bool GotoNearTarget(entt::entity self, entt::entity victim)
{
    if (IS_SET(ecs::PlayerRuntime::GetAIFlag(self), AIFLAG_NOMOVE))
        return false;

    const uint16_t range = CombatSystem::GetMobAttackRange(self);

    switch (CombatSystem::GetMobBattleType(self)) {
    case BATTLE_TYPE_RANGE:
    case BATTLE_TYPE_MAGIC:
        if (CombatSystem::Follow(self, victim, range * 8 / 10))
            return true;
        break;

    default:
        if (CombatSystem::Follow(self, victim, range * 9 / 10))
            return true;
        break;
    }

    return CombatSystem::Follow(self, victim, range * 9 / 10);
}

} // namespace

void StateBattle(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (ecs::PlayerRuntime::IsStone(e)) {
        LOG_ERROR("Stone must not use battle state (name {})", ecs::PlayerRuntime::GetName(e));
        return;
    }

    if (ecs::PlayerRuntime::IsPC(e) || !ecs::MovementSystem::CanMove(e) || CombatSystem::IsStun(e))
        return;

    entt::entity victim = CombatSystem::GetVictim(e);
    if (victim != entt::null && !g_registry.valid(victim))
        victim = entt::null;

    if (AIHelpers::IsCoward(e)) {
        if (CombatSystem::IsDead(e))
            return;

        CombatSystem::SetVictim(e, entt::null);
        if (number(1, 50) != 1) {
            ecs::PlayerRuntime::SetPosition(e, POS_STANDING);
            AIHelpers::SetStateDuration(e, 1);
        } else {
            CombatSystem::CowardEscape(e);
        }
        return;
    }

    const bool guard = ecs::PlayerRuntime::IsGuardNPC(e);
    if (victim == entt::null ||
        (CombatSystem::IsStun(victim) && guard) ||
        CombatSystem::IsDead(victim)) {
        entt::entity replacement = entt::null;
        if (victim != entt::null && CombatSystem::IsDead(victim) && !no_wander &&
            AIHelpers::IsAggressive(e)) {
            const entt::entity leader = ecs::SocialSystem::GetPartyLeader(e);
            if (leader == entt::null || leader == e) {
                if (const TMobTable* table = ecs::PlayerRuntime::GetMobTable(e))
                    replacement = CombatSystem::FindVictim(e, table->wAggressiveSight);
            }
        }

        if (replacement != entt::null && g_registry.valid(replacement)) {
            CombatSystem::SetVictim(e, replacement);
            AIHelpers::SetStateDuration(e, PASSES_PER_SEC(1));
            return;
        }

        CombatSystem::SetVictim(e, entt::null);
        if (guard) {
            CombatSystem::Return(e);
        } else {
            ecs::PlayerRuntime::SetPosition(e, POS_STANDING);
        }
        AIHelpers::SetStateDuration(e, PASSES_PER_SEC(1));
        return;
    }

    const int32_t x = ecs::PlayerRuntime::GetX(e);
    const int32_t y = ecs::PlayerRuntime::GetY(e);
    const entt::entity protege = CombatSystem::GetProtege(e);
    const float distance = static_cast<float>(DISTANCE_APPROX(
        x - ecs::PlayerRuntime::GetX(victim),
        y - ecs::PlayerRuntime::GetY(victim)));

    if (distance >= 4000.0f) {
        CombatSystem::SetVictim(e, entt::null);
        const bool farFromProtege = protege != entt::null &&
            DISTANCE_APPROX(x - ecs::PlayerRuntime::GetX(protege),
                            y - ecs::PlayerRuntime::GetY(protege)) > 1000;
        if (farFromProtege) {
            CombatSystem::Follow(e, protege, number(150, 400));
        } else {
            ecs::PlayerRuntime::SetPosition(e, POS_STANDING);
        }
        return;
    }

    if (distance >= CombatSystem::GetMobAttackRange(e) * 1.15f) {
        if (GotoNearTarget(e, victim))
            AIHelpers::SetStateDuration(e, 1);
        return;
    }

    if (LPPARTY party = ecs::SocialSystem::GetParty(e))
        party->SendMessage(e, PM_ATTACKED_BY, 0, 0);

    const uint32_t curTime = get_dword_time();
    const uint32_t duration = CalculateDuration(
        ecs::PointSystem::GetLimitPoint(e, POINT_ATT_SPEED), 2000);
    const uint32_t sinceLastAttack = curTime - CombatSystem::GetLastAttackTime(e);
    if (sinceLastAttack < duration) {
        AIHelpers::SetStateDuration(e,
            MAX(1, (passes_per_sec * (duration - sinceLastAttack) / 1000)));
        return;
    }

    // The berserk and godspeed switches are CMobInstance state; the thresholds
    // they compare against come off the table, which the entity exposes.
    if (const TMobTable* table = ecs::PlayerRuntime::GetMobTable(e)) {
        const bool wantsBerserk = CombatSystem::IsBerserker(e) &&
            ecs::PlayerRuntime::GetHPPct(e) < table->bBerserkPoint;
        const bool wantsGodSpeed = CombatSystem::IsGodSpeeder(e) &&
            ecs::PlayerRuntime::GetHPPct(e) < table->bGodSpeedPoint;
        if (wantsBerserk && !CombatSystem::IsBerserk(e))
            CombatSystem::SetBerserk(e, true);
        if (wantsGodSpeed && !CombatSystem::IsGodSpeed(e))
            CombatSystem::SetGodSpeed(e, true);
    }

    if (SkillSystem::HasMobSkill(e)) {
        for (unsigned int skillIdx = 0; skillIdx < MOB_SKILL_MAX_NUM; ++skillIdx) {
            if (!SkillSystem::CanUseMobSkill(e, skillIdx))
                continue;

            ecs::MovementSystem::SetRotationToXY(e,
                ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim));

            if (!SkillSystem::UseMobSkill(e, skillIdx))
                continue;

            ecs::MovementSystem::SendMovePacket(e, FUNC_MOB_SKILL, skillIdx,
                ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), 0, curTime);

            const float motionDuration = CMotionManager::instance().GetMotionDuration(
                ecs::PlayerRuntime::GetRaceNum(e),
                MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + skillIdx));
            AIHelpers::SetStateDuration(e, static_cast<uint32_t>(
                motionDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(motionDuration)));
            return;
        }
    }

    {
        const int32_t vnum = ecs::PlayerRuntime::GetRaceNum(e);
#ifdef ENABLE_MELEY_LAIR
        if (vnum == 6193)
            return;
#endif
#ifdef ENABLE_ANCIENT_PYRAMID
        if (vnum == PYRAMID_BOSSVNUM)
            return;
#endif
#ifdef __DEFENSE_WAVE__
        if (vnum >= 3960 && vnum <= 3962)
            return;
#endif
    }

    if (!CombatSystem::Attack(e, victim, 0)) {
        AIHelpers::SetStateDuration(e, passes_per_sec / 2);
        return;
    }

    ecs::MovementSystem::SetRotationToXY(e,
        ecs::PlayerRuntime::GetX(victim), ecs::PlayerRuntime::GetY(victim));
    ecs::MovementSystem::SendMovePacket(e, FUNC_ATTACK, 0,
        ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), 0, curTime);

    const float motionDuration = CMotionManager::instance().GetMotionDuration(
        ecs::PlayerRuntime::GetRaceNum(e),
        MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK));
    AIHelpers::SetStateDuration(e, static_cast<uint32_t>(
        motionDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(motionDuration)));
}

void StateIdle(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    if (ecs::PlayerRuntime::IsStone(e)) {
        AIHelpers::SetStateDuration(e, PASSES_PER_SEC(1));
        return;
    }

    if (ecs::PlayerRuntime::IsWarp(e) || ecs::PlayerRuntime::IsGoto(e)) {
        AIHelpers::SetStateDuration(e, 60 * passes_per_sec);
        return;
    }

    if (ecs::PlayerRuntime::IsPC(e))
        return;

    if (!ecs::PlayerRuntime::IsMonster(e)) {
        StateIdle_NPC(e);
        return;
    }

    StateIdle_Monster(e);
}

} // namespace AISystem

void AISystem_Update(entt::registry& reg, uint32_t tick)
{
    (void)tick;

    auto view = reg.view<ecs::VIDComponent>();

    for (auto entity : view) {
        // "Has a legacy object and is not a PC" was the old gate. The first
        // half is a component test now, not a character the loop goes on to use.
        const auto* legacy = reg.try_get<ecs::LegacyCharPtr>(entity);
        if (!legacy || !legacy->ptr || ecs::PlayerRuntime::IsPC(entity)) {
            continue;
        }

        bool changed = SyncAIFlags(reg, entity);

        // CombatTarget is authoritative; do not mirror it through CHARACTER.

        auto& aiState = reg.get_or_emplace<ecs::AIState>(entity);
        const uint8_t observedState = ObserveAIState(reg, entity);
        if (aiState.currentState != observedState) {
            aiState.currentState = observedState;
            changed = true;
        }

        if (changed) {
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
        }
    }
}
