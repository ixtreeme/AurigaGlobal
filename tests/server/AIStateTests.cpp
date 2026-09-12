// Idle and battle AI, driven through the entity seam. The cases here were
// unreachable while the state bodies were CHARACTER methods: a victim that
// died, a victim whose entity was destroyed and its slot handed to somebody
// else, and the AI entity being retired from inside a callback the state body
// invokes.
#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/AIHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/CharacterAccessors.hpp"
#include "../../SRC/Server/GameServer/ecs/SpatialHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/components/ai_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/combat_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AISystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/motion.h"
#include "../../SRC/Server/GameServer/party.h"

#include <functional>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& what)
{
    if (condition)
        return;
    std::cerr << "FAIL: " << what << std::endl;
    ++g_failures;
}

struct Recorder {
    std::vector<entt::entity> victimClears;
    std::vector<entt::entity> gotoCalls;
    entt::entity victim { entt::null };
    bool victimDead { false };
    // What the target search hands back, and whether it was consulted.
    entt::entity searchResult { entt::null };
    std::vector<entt::entity> dead;
    int searchCalls { 0 };
    std::vector<entt::entity> fightsBegun;
    std::vector<entt::entity> attacked;
    uint32_t nowMs { 1000 };
    uint32_t lastAttackTime { 0 };
    int attackPeriodMs { 0 };
    std::function<void(entt::entity)> onAttack;
    // Fires inside Goto, standing in for a packet callback that retires the
    // entity while the state body is still running.
    std::function<void(entt::entity)> onGoto;
};

Recorder g_rec;

entt::entity MakeMonster(int32_t x = 1000, int32_t y = 1000)
{
    const entt::entity e = g_registry.create();
    g_registry.emplace<ecs::TagMonster>(e);
    g_registry.emplace<ecs::AIFlags>(e);
    g_registry.emplace<ecs::AIState>(e);
    g_registry.emplace<ecs::Position>(e, ecs::Position { x, y });
    return e;
}

} // namespace

// --- service doubles --------------------------------------------------------

namespace ecs::PlayerRuntime {
bool IsStone(entt::entity) { return false; }
bool IsMonster(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ecs::TagMonster>(e); }
bool IsPC(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ecs::TagPC>(e); }
bool IsWarp(entt::entity) { return false; }
bool IsGoto(entt::entity) { return false; }
bool IsBuilding(entt::entity) { return false; }
bool IsMount(entt::entity) { return false; }
bool IsPet(entt::entity) { return false; }
bool IsNewPet(entt::entity) { return false; }
uint32_t GetAIFlag(entt::entity) { return 0; }
int32_t GetX(entt::entity e) { const auto* p = g_registry.try_get<ecs::Position>(e); return p ? p->x : 0; }
int32_t GetY(entt::entity e) { const auto* p = g_registry.try_get<ecs::Position>(e); return p ? p->y : 0; }
int32_t GetMapIndex(entt::entity) { return 1; }
float GetRotation(entt::entity) { return 0.0f; }
const TMobTable* GetMobTable(entt::entity)
{
    static TMobTable table {};
    table.wAggressiveSight = 5000;
    return &table;
}
void MonsterLog(entt::entity, const char*) {}
std::string_view GetName(entt::entity) { return "test-mob"; }
int GetHPPct(entt::entity) { return 100; }
uint32_t GetRaceNum(entt::entity) { return 101; }
void SetPosition(entt::entity, int) {}
bool IsGuardNPC(entt::entity) { return false; }
} // namespace ecs::PlayerRuntime

namespace CombatSystem {
bool IsStun(entt::entity) { return false; }
bool IsDead(entt::entity e)
{
    // A destroyed entity is not dead, it is gone. Answering anything else here
    // would hide the very bug these cases are looking for.
    if (!g_registry.valid(e))
        return false;
    for (const entt::entity d : g_rec.dead)
        if (d == e)
            return true;
    return e == g_rec.victim && g_rec.victimDead;
}
entt::entity GetVictim(entt::entity) { return g_rec.victim; }
void SetVictim(entt::entity attacker, entt::entity victim)
{
    if (victim == entt::null)
        g_rec.victimClears.push_back(attacker);
    g_rec.victim = victim;
}
bool CanBeginFight(entt::entity) { return true; }
void BeginFight(entt::entity, entt::entity victim) { g_rec.fightsBegun.push_back(victim); }
entt::entity FindVictim(entt::entity, int)
{
    ++g_rec.searchCalls;
    return g_rec.searchResult;
}
entt::entity GetNearestVictim(entt::entity, entt::entity) { return entt::null; }
entt::entity GetStone(entt::entity) { return entt::null; }
entt::entity GetProtege(entt::entity) { return entt::null; }
void CowardEscape(entt::entity) {}
bool IsBerserker(entt::entity) { return false; }
bool IsDeathBlow(entt::entity) { return false; }
uint16_t GetMobAttackRange(entt::entity) { return 100; }
uint8_t GetMobBattleType(entt::entity) { return 0; }
uint32_t GetLastAttackTime(entt::entity) { return g_rec.lastAttackTime; }
bool Attack(entt::entity attacker, entt::entity victim, uint8_t)
{
    g_rec.attacked.push_back(victim);
    g_rec.lastAttackTime = g_rec.nowMs;
    if (g_rec.onAttack)
        g_rec.onAttack(attacker);
    return true;
}
bool IsGodSpeeder(entt::entity) { return false; }
bool IsBerserk(entt::entity) { return false; }
void SetBerserk(entt::entity, bool) {}
bool IsGodSpeed(entt::entity) { return false; }
void SetGodSpeed(entt::entity, bool) {}
bool IsRevive(entt::entity) { return false; }
bool IsStoneSkinner(entt::entity) { return false; }
bool Return(entt::entity) { return false; }
bool Follow(entt::entity, entt::entity, float) { return false; }
} // namespace CombatSystem

namespace ecs::MovementSystem {
bool CanMove(entt::entity) { return true; }
bool Goto(entt::entity e, int32_t, int32_t)
{
    g_rec.gotoCalls.push_back(e);
    if (g_rec.onGoto)
        g_rec.onGoto(e);
    return true;
}
void SetNowWalking(entt::entity, bool) {}
void SetRotationToXY(entt::entity, int32_t, int32_t) {}
void SetRotation(entt::entity, float, bool) {}
void SendMovePacket(entt::entity, uint8_t, uint8_t, uint32_t, uint32_t,
    uint32_t, uint32_t, float) {}
} // namespace ecs::MovementSystem


namespace SkillSystem {
bool HasMobSkill(entt::entity) { return false; }
bool CanUseMobSkill(entt::entity, unsigned int) { return false; }
bool UseMobSkill(entt::entity, unsigned int) { return false; }
} // namespace SkillSystem

namespace ecs::SocialSystem {
CParty* GetParty(entt::entity) { return nullptr; }
entt::entity GetPartyLeader(entt::entity) { return entt::null; }
} // namespace ecs::SocialSystem

namespace ecs::PointSystem {
int GetLimitPoint(entt::entity, uint8_t) { return 100; }
} // namespace ecs::PointSystem

// --- globals and free functions the pump touches ----------------------------

bool no_wander = false;
int passes_per_sec = 25;
int test_server = 0;

int number_ex(int from, int to, const char*, int)
{
    // Deterministic and inclusive: the wander roll is one in seven, so a
    // fixed 0 keeps the movement branch reachable on every pass.
    return from > to ? from : from;
}

void GetDeltaByDegree(float, float distance, float* x, float* y)
{
    if (x) *x = distance;
    if (y) *y = 0.0f;
}

bool SECTREE_MANAGER::IsMovablePosition(int, int, int) { return true; }


// Motion, party and attack-speed services are controlled at the AI boundary.
float CMotionManager::GetMotionDuration(uint32_t, uint32_t) { return 0.0f; }
void CParty::SendMessage(entt::entity, uint8_t, uint32_t, uint32_t) {}
int CalculateDuration(int, int) { return g_rec.attackPeriodMs; }
int MAX(int a, int b) { return a > b ? a : b; }

namespace logging {
std::shared_ptr<spdlog::logger> GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("test-null");
    return logger;
}
} // namespace logging
uint32_t get_dword_time() { return g_rec.nowMs; }

// The pump reads the pulse, so the cases below drive it.
int g_pulse = 0;
int thecore_pulse() { return g_pulse; }
bool CHARACTER_MANAGER::AddToStateList(entt::entity) { return true; }

// --- the cases --------------------------------------------------------------

namespace {

void Reset()
{
    g_registry.clear();
    g_rec = Recorder {};
    g_pulse = 0;
}

void SetPulse(int pulse) { g_pulse = pulse; }

// The state bodies ask for a wait by setting AIState::stateDuration. Nothing
// honoured it after the pump became entity-native, so every state ran on every
// pulse; these two cases are what would have caught that.
void AWaitingStateIsNotRunEarly()
{
    Reset();
    SetPulse(100);
    const entt::entity mob = MakeMonster();
    const entt::entity victim = g_registry.create();
    g_rec.victim = victim;
    g_rec.victimDead = true;
    AISystem::GotoState(mob, ecs::AIFSMState::Idle);
    AIHelpers::SetNextStatePulse(mob, 140);

    AISystem::UpdateStateMachine(mob);
    Check(g_rec.victimClears.empty(), "a state inside its wait must not run");

    SetPulse(140);
    AISystem::UpdateStateMachine(mob);
    Check(!g_rec.victimClears.empty(), "a state whose wait is up must run");
    Check(AIHelpers::GetNextStatePulse(mob) == 140u + AIHelpers::GetStateDuration(mob),
        "the pass must book its next one at the duration the state asked for");
}

// A dead character kept running its idle pass for the same reason.
void ADeadCharacterDoesNotRunItsState()
{
    Reset();
    SetPulse(100);
    const entt::entity mob = MakeMonster();
    const entt::entity victim = g_registry.create();
    g_rec.victim = victim;
    g_rec.victimDead = true;
    g_rec.dead.push_back(mob);
    AISystem::GotoState(mob, ecs::AIFSMState::Idle);

    AISystem::UpdateStateMachine(mob);
    Check(g_rec.victimClears.empty(), "a dead character must not run its state");
}

// A victim that is still a live entity but has died: idle drops it and slows
// the state to a one-second retry, as the CHARACTER body did.
void DeadVictimIsReleased()
{
    Reset();
    const entt::entity mob = MakeMonster();
    const entt::entity victim = g_registry.create();
    g_rec.victim = victim;
    g_rec.victimDead = true;

    AISystem::StateIdle(mob);

    Check(!g_rec.victimClears.empty() && g_rec.victimClears.front() == mob,
        "a dead victim is cleared on the mob that held it");
    // The one-second retry the dead-victim branch sets is deliberately
    // overwritten further down, exactly as the CHARACTER body did: it only
    // survives when a replacement target is found and the pass returns early.
    // With no target and no aggression, what is left is the idle window.
    Check(AIHelpers::GetStateDuration(mob) == PASSES_PER_SEC(3),
        "a targetless idle pass ends on the wander window, not the retry");
}

// The victim entity was destroyed outright. The handle the AI still holds is
// stale, and nothing in the idle path may dereference it.
void DestroyedVictimDoesNotCrash()
{
    Reset();
    const entt::entity mob = MakeMonster();
    const entt::entity victim = g_registry.create();
    g_rec.victim = victim;
    g_registry.destroy(victim);

    AISystem::StateIdle(mob);

    Check(g_registry.valid(mob), "the mob survives a destroyed victim");
    Check(g_rec.victim == entt::null || !g_registry.valid(g_rec.victim),
        "a destroyed victim is not resurrected by the idle pass");
}

// EnTT hands a destroyed slot back out with a new generation. The old handle
// must not be mistaken for its replacement.
void RecycledHandleIsNotTheNewEntity()
{
    Reset();
    const entt::entity mob = MakeMonster();
    const entt::entity victim = g_registry.create();
    const entt::entity staleHandle = victim;
    g_registry.destroy(victim);

    const entt::entity reused = g_registry.create();
    Check(entt::to_entity(reused) == entt::to_entity(staleHandle),
        "the case actually reused the slot");
    Check(reused != staleHandle, "the reused slot carries a new generation");
    Check(!g_registry.valid(staleHandle), "the stale handle is not valid");

    g_rec.victim = staleHandle;
    AISystem::StateIdle(mob);

    Check(g_rec.victim != reused, "a stale handle never resolves to its successor");
}

// A packet callback can retire the entity while the state body is mid-flight.
void EntityDestroyedInsideCallback()
{
    Reset();
    g_rec.onGoto = [](entt::entity e) {
        if (g_registry.valid(e))
            g_registry.destroy(e);
    };

    // Wandering is a one-in-seven roll, so drive it until Goto is reached.
    for (int attempt = 0; attempt < 500 && g_rec.gotoCalls.empty(); ++attempt) {
        const entt::entity mob = MakeMonster();
        AISystem::StateIdle(mob);
    }

    Check(!g_rec.gotoCalls.empty(), "the wander path was reached at least once");
}

// The search can hand back a handle that has gone bad between the scan and the
// decision. None of these may reach BeginFight.
void SearchResultThatWentBadIsNotEngaged()
{
    struct Case {
        const char* name;
        bool destroy;
        bool recycle;
        bool dead;
    };
    const Case cases[] = {
        { "a dead search result is not engaged", false, false, true },
        { "a destroyed search result is not engaged", true, false, false },
        { "a recycled slot from the search is not engaged", true, true, false },
    };

    for (const Case& c : cases) {
        Reset();
        const entt::entity mob = MakeMonster();
        g_registry.get<ecs::AIFlags>(mob).isAggressive = true;

        entt::entity found = g_registry.create();
        if (c.dead)
            g_rec.dead.push_back(found);
        if (c.destroy)
            g_registry.destroy(found);
        if (c.recycle) {
            const entt::entity reused = g_registry.create();
            Check(entt::to_entity(reused) == entt::to_entity(found), "the slot was reused");
        }
        g_rec.searchResult = found;

        AISystem::StateIdle(mob);

        Check(g_rec.searchCalls > 0, std::string("the search ran for: ") + c.name);
        const bool engaged = !g_rec.fightsBegun.empty();
        Check(!engaged, c.name);
    }
}

// Battle runs against a stored victim handle, which can go bad between ticks
// exactly as the idle one can.
void BattleDoesNotSwingAtAGoneVictim()
{
    struct Case { const char* name; bool destroy; bool recycle; };
    const Case cases[] = {
        { "battle does not attack a destroyed victim", true, false },
        { "battle does not attack through a recycled slot", true, true },
    };

    for (const Case& c : cases) {
        Reset();
        const entt::entity mob = MakeMonster();
        entt::entity victim = g_registry.create();
        g_registry.emplace<ecs::Position>(victim, ecs::Position { 1000, 1000 });
        if (c.destroy)
            g_registry.destroy(victim);
        if (c.recycle) {
            const entt::entity reused = g_registry.create();
            Check(entt::to_entity(reused) == entt::to_entity(victim), "the slot was reused");
        }
        g_rec.victim = victim;

        AISystem::StateBattle(mob);

        Check(g_rec.attacked.empty(), c.name);
    }
}

// The character-manager pump owns mob attacks. A target alone must not cause
// an extra swing between the state deadline and the next scheduled pass.
void BattlePumpHonoursPulseDeadline()
{
    Reset();
    SetPulse(100);
    const entt::entity mob = MakeMonster();
    const entt::entity victim = MakeMonster();
    g_rec.victim = victim;
    g_rec.nowMs = 10000;
    g_rec.attackPeriodMs = 2000;
    AISystem::GotoState(mob, ecs::AIFSMState::Battle);

    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.size() == 1 && g_rec.attacked.front() == victim,
        "a due battle pass dispatches one entity-native attack");
    Check(g_rec.lastAttackTime == 10000, "the attack service records milliseconds");
    Check(AIHelpers::GetNextStatePulse(mob) == 150,
        "the default attack motion schedules two seconds in pulses");

    // Even when the millisecond clock has advanced far enough for a swing,
    // the pulse deadline must still suppress repeated or premature updates.
    g_rec.nowMs = 16000;
    for (const int pulse : {100, 101, 125, 149}) {
        SetPulse(pulse);
        AISystem::UpdateStateMachine(mob);
        Check(g_rec.attacked.size() == 1, "no extra swing before the pulse deadline");
        Check(AIHelpers::GetNextStatePulse(mob) == 150,
            "an early update must not postpone the booked attack");
    }

    SetPulse(150);
    AISystem::UpdateStateMachine(mob);
    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.size() == 2 && AIHelpers::GetNextStatePulse(mob) == 200,
        "the deadline dispatches once, even with two updates in that pulse");
    Check(!g_registry.any_of<ecs::LegacyCharPtr>(mob),
        "scheduled attacks do not require a legacy character component");
}

void BattlePumpConvertsRemainingMillisecondsToPulses()
{
    Reset();
    SetPulse(100);
    const entt::entity mob = MakeMonster();
    g_rec.victim = MakeMonster();
    g_rec.nowMs = 1000;
    g_rec.lastAttackTime = 250;
    g_rec.attackPeriodMs = 2000;
    AISystem::GotoState(mob, ecs::AIFSMState::Battle);

    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.empty() && AIHelpers::GetNextStatePulse(mob) == 131,
        "1250 remaining milliseconds book 31 pulses at 25 Hz");

    SetPulse(130);
    g_rec.nowMs = 2200;
    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.empty() && AIHelpers::GetNextStatePulse(mob) == 131,
        "the booked cooldown is not polled early");

    SetPulse(131);
    g_rec.nowMs = 2240;
    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.empty() && AIHelpers::GetNextStatePulse(mob) == 132,
        "a sub-pulse remainder waits at least one pulse");

    SetPulse(132);
    g_rec.nowMs = 2250;
    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.size() == 1 && g_rec.lastAttackTime == 2250,
        "the attack is dispatched when both clocks permit it");
}

void BattlePumpHandlesAttackClockWrap()
{
    Reset();
    SetPulse(100);
    const entt::entity mob = MakeMonster();
    g_rec.victim = MakeMonster();
    g_rec.nowMs = 20;
    g_rec.lastAttackTime = UINT32_MAX - 19;
    g_rec.attackPeriodMs = 80;
    AISystem::GotoState(mob, ecs::AIFSMState::Battle);

    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.empty() && AIHelpers::GetNextStatePulse(mob) == 101,
        "wrapping the millisecond clock leaves 40 milliseconds of cooldown");

    SetPulse(101);
    g_rec.nowMs = 60;
    AISystem::UpdateStateMachine(mob);
    Check(g_rec.attacked.size() == 1 && g_rec.lastAttackTime == 60,
        "a wrapped attack clock does not suppress the next valid attack");
}

void PlayerBattleStateDoesNotAutoAttack()
{
    Reset();
    const entt::entity player = MakeMonster();
    g_registry.remove<ecs::TagMonster>(player);
    g_registry.emplace<ecs::TagPC>(player);
    g_registry.emplace<ecs::CombatActiveTag>(player);
    g_rec.victim = MakeMonster();
    AIHelpers::SetStateDuration(player, 1);
    AISystem::GotoState(player, ecs::AIFSMState::Battle);

    for (int pulse = 100; pulse <= 200; ++pulse) {
        SetPulse(pulse);
        g_rec.nowMs = 10000 + (pulse - 100) * 40;
        AISystem::UpdateStateMachine(player);
    }
    Check(g_rec.attacked.empty(), "a PC with a target still needs client attack input");
}

// The entity can be retired by a callback while battle is still running.
void BattleSurvivesDestructionInsideAttack()
{
    Reset();
    const entt::entity mob = MakeMonster();
    const entt::entity victim = g_registry.create();
    g_registry.emplace<ecs::Position>(victim, ecs::Position { 1000, 1000 });
    g_rec.victim = victim;
    g_rec.onAttack = [](entt::entity e) {
        if (g_registry.valid(e))
            g_registry.destroy(e);
    };

    AISystem::StateBattle(mob);

    Check(!g_rec.attacked.empty(), "the attack path was reached");
    Check(!g_registry.valid(mob), "the callback did retire the entity");
}

} // namespace

int main()
{
    try {
        AWaitingStateIsNotRunEarly();
        ADeadCharacterDoesNotRunItsState();
        DeadVictimIsReleased();
        DestroyedVictimDoesNotCrash();
        RecycledHandleIsNotTheNewEntity();
        EntityDestroyedInsideCallback();
        SearchResultThatWentBadIsNotEngaged();
        BattleDoesNotSwingAtAGoneVictim();
        BattlePumpHonoursPulseDeadline();
        BattlePumpConvertsRemainingMillisecondsToPulses();
        BattlePumpHandlesAttackClockWrap();
        PlayerBattleStateDoesNotAutoAttack();
        BattleSurvivesDestructionInsideAttack();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: threw: " << e.what() << std::endl;
        ++g_failures;
    }

    if (g_failures == 0)
        std::cout << "AI idle and battle states: all cases passed" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
