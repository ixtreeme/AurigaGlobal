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
            if (LPCHARACTER ch = ecs::LegacyCharOf(e); ch && ch->Follow(protege, number(100, 300)))
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

    // Berserk and godspeed still live on CMobInstance, which has no component
    // yet. Resolving a character is their cost alone, so it happens here and
    // not as a precondition for the whole body - a mob with no legacy object
    // still wanders and still drops a dead target.
    if (CombatSystem::IsBerserker(e) || CombatSystem::IsGodSpeeder(e)) {
        if (LPCHARACTER ch = ecs::LegacyCharOf(e)) {
            if (CombatSystem::IsBerserker(e) && ch->IsBerserk())
                ch->SetBerserk(false);
            if (CombatSystem::IsGodSpeeder(e) && ch->IsGodSpeed())
                ch->SetGodSpeed(false);
        }
    }

    entt::entity victim = CombatSystem::GetVictim(e);
    if (victim == entt::null || CombatSystem::IsDead(victim)) {
        CombatSystem::SetVictim(e, entt::null);
        victim = entt::null;
        AIHelpers::SetStateDuration(e, PASSES_PER_SEC(1));
    }

    if (victim == entt::null || ecs::PlayerRuntime::IsBuilding(victim)) {
        if (const entt::entity stone = CombatSystem::GetStone(e); stone != entt::null) {
            victim = CombatSystem::GetNearestVictim(stone, stone);
        } else if (!no_wander && AIHelpers::IsAggressive(e)) {
            // Target search is step 2; it still takes a character.
            const TMobTable* table = ecs::PlayerRuntime::GetMobTable(e);
            LPCHARACTER self = table ? ecs::LegacyCharOf(e) : nullptr;
            LPCHARACTER found = self ? FindVictim(self, table->wAggressiveSight) : nullptr;
            victim = found ? found->GetEntityHandle() : entt::null;
        }
    }

    if (victim != entt::null && !CombatSystem::IsDead(victim)) {
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
            LPCHARACTER self = ecs::LegacyCharOf(e);
            if (self && self->Follow(protege, number(150, 400))) {
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
        LPCHARACTER ch = LegacyCharBoundary(reg, entity);
        if (!ch || ecs::PlayerRuntime::IsPC(entity)) {
            continue;
        }

        bool changed = SyncAIFlags(reg, entity, ch);

        // CombatTarget is authoritative; do not mirror it through CHARACTER.

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
