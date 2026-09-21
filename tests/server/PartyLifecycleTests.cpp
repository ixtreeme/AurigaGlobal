#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/p2p.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/dungeon.h"
#include "../../SRC/Server/GameServer/event.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Real party.cpp with entity-only fixtures. The party state lives on registry
// entities; character, dungeon, packet, event and manager services are doubles.
entt::registry g_registry;
int passes_per_sec = 25;
uint8_t g_bChannel = 1;
namespace { int dbMarker = 0; }
LPCLIENT_DESC db_clientdesc = reinterpret_cast<LPCLIENT_DESC>(&dbMarker);

// Production singletons the manager and walk code resolve.
CPartyManager cparty_manager;
CHARACTER_MANAGER character_manager;
P2P_MANAGER p2p_manager;
SECTREE_MANAGER sectree_manager;

namespace {
int checks = 0;
int dbPackets = 0;
int lastDbHeader = 0;
std::vector<int> dbHeaders;
int liveEvents = 0;
int cancels = 0;
int idleDestroyRequests = 0;
int teardowns = 0;
int skillLevel = 0;
bool canSummon = true;
bool movablePosition = true;
std::function<void(entt::entity, entt::entity)> onSetParty;
std::function<void(entt::entity)> onTeardown;
std::vector<entt::entity> warped, shown, stopped;
std::vector<std::pair<int32_t, int32_t>> gotoRequests;
LPEVENT scheduled;
std::vector<LPEVENT> retainedEvents;
entt::entity trackParty = entt::null;

struct Actor {
    uint32_t pid = 0;
    uint32_t vid = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t mapIndex = 1;
    uint8_t level = 10;
    bool pc = true;
    bool monster = false;
    bool hasDesc = false;
    uint32_t mobVnum = 0;
    std::string name;
};

void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}

int CountDbPacket(int header) {
    int count = 0;
    for (const int sent : dbHeaders)
        if (sent == header) ++count;
    return count;
}

entt::entity ActorEntity(uint32_t pid, uint32_t vid, bool pc = true, bool hasDesc = false) {
    const auto entity = g_registry.create();
    auto& actor = g_registry.emplace<Actor>(entity);
    actor.pid = pid;
    actor.vid = vid;
    actor.pc = pc;
    actor.hasDesc = hasDesc;
    actor.monster = !pc;
    actor.name = pc ? "player" : "mob";
    return entity;
}

entt::entity MobEntity(uint32_t vid, uint32_t vnum) {
    const auto entity = ActorEntity(vid, vid, false, false);
    g_registry.get<Actor>(entity).mobVnum = vnum;
    return entity;
}

Actor* ActorOf(entt::entity e) {
    return e != entt::null && g_registry.valid(e) ? g_registry.try_get<Actor>(e) : nullptr;
}

void Place(entt::entity e, int32_t x, int32_t y, int32_t mapIndex = 1) {
    auto* actor = ActorOf(e);
    Check(actor != nullptr, "placement of an unknown actor");
    actor->x = x;
    actor->y = y;
    actor->mapIndex = mapIndex;
}

void Reset() {
    g_registry.clear();
    scheduled.reset();
    retainedEvents.clear();
    Check(liveEvents == 0, "event leaked");
    onSetParty = {};
    onTeardown = {};
    warped.clear();
    shown.clear();
    stopped.clear();
    gotoRequests.clear();
    dbPackets = lastDbHeader = idleDestroyRequests = teardowns = 0;
    dbHeaders.clear();
    skillLevel = 0;
    canSummon = true;
    movablePosition = true;
    trackParty = entt::null;
}

LPDESC DescToken(entt::entity e) { return reinterpret_cast<LPDESC>(static_cast<uintptr_t>(0x2000 + entt::to_entity(e))); }

ecs::PartyState& State(entt::entity party) { return g_registry.get<ecs::PartyState>(party); }

void SetNear(entt::entity a, entt::entity b) {
    Place(a, 1000, 1000);
    Place(b, 1000, 1000);
}
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("party-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("party-test");
    return logger;
}
void DESC::Packet(const void*, int) {}
void CLIENT_DESC::DBPacket(uint8_t header, uint32_t, const void*, uint32_t) {
    ++dbPackets;
    lastDbHeader = header;
    dbHeaders.push_back(header);
}

// Event scheduler doubles: the test drives callbacks explicitly.
void intrusive_ptr_add_ref(EVENT* e) { ++e->ref_count; }
void intrusive_ptr_release(EVENT* e) { if (--e->ref_count == 0) { --liveEvents; delete e; } }
LPEVENT event_create_ex(TEVENTFUNC func, event_info_data* info, int32_t) {
    LPEVENT result(new EVENT);
    ++liveEvents;
    result->func = func;
    result->info = info;
    scheduled = result;
    retainedEvents.push_back(result);
    return result;
}
void event_cancel(LPEVENT* event) {
    if (*event) { ++cancels; (*event)->is_force_to_end = true; }
    event->reset();
}

int number_ex(int from, int to, const char*, int) {
    if (from > to) { const int tmp = from; from = to; to = tmp; }
    return from;
}
int MAX(int a, int b) { return a > b ? a : b; }
int MIN(int a, int b) { return a < b ? a : b; }
uint32_t get_dword_time() { return 100000; }
const int CHN_aiPartyBonusExpPercentByMemberCount[9] = { 0, 0, 12, 18, 26, 40, 53, 70, 100 };

// Dungeon members only reach these on the services the doubles never set.
void CDungeon::QuitParty(entt::entity) {}
void CDungeon::SetPartyNull() {}

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
P2P_MANAGER::P2P_MANAGER() = default;
P2P_MANAGER::~P2P_MANAGER() = default;
SECTREE_MANAGER::SECTREE_MANAGER() = default;
SECTREE_MANAGER::~SECTREE_MANAGER() = default;

entt::entity CHARACTER_MANAGER::FindEntity(uint32_t vid) {
    for (const auto e : g_registry.view<Actor>())
        if (g_registry.get<Actor>(e).vid == vid) return e;
    return entt::null;
}
entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t pid) {
    for (const auto e : g_registry.view<Actor>())
        if (g_registry.get<Actor>(e).pid == pid) return e;
    return entt::null;
}
CCI* P2P_MANAGER::FindByPID(uint32_t) { return nullptr; }
bool SECTREE_MANAGER::GetMovablePosition(int32_t, int32_t x, int32_t y, PIXEL_POSITION& pos) {
    pos.x = x;
    pos.y = y;
    pos.z = 0;
    return movablePosition;
}

namespace ecs::PlayerRuntime {
bool IsPC(entt::entity e) { const auto* a = ActorOf(e); return a && a->pc; }
bool IsMonster(entt::entity e) { const auto* a = ActorOf(e); return a && a->monster; }
bool IsStone(entt::entity) { return false; }
uint32_t GetPlayerID(entt::entity e) { const auto* a = ActorOf(e); return a ? a->pid : 0; }
uint32_t GetPacketVID(entt::entity e) { const auto* a = ActorOf(e); return a ? a->vid : 0; }
std::string_view GetName(entt::entity e) { const auto* a = ActorOf(e); return a ? a->name : "unknown"; }
LPDESC GetDesc(entt::entity e) { const auto* a = ActorOf(e); return a && a->pc && a->hasDesc ? DescToken(e) : nullptr; }
int32_t GetMapIndex(entt::entity e) { const auto* a = ActorOf(e); return a ? a->mapIndex : 0; }
int32_t GetX(entt::entity e) { const auto* a = ActorOf(e); return a ? a->x : 0; }
int32_t GetY(entt::entity e) { const auto* a = ActorOf(e); return a ? a->y : 0; }
int64_t GetHP(entt::entity) { return 100; }
int64_t GetSP(entt::entity) { return 100; }
int GetSkillPowerByLevel(entt::entity, int, bool) { return 100; }
const TMobTable* GetMobTable(entt::entity e) {
    static TMobTable table {};
    const auto* a = ActorOf(e);
    if (!a || !a->monster) return nullptr;
    table.dwVnum = a->mobVnum;
    return &table;
}
void StartDestroyWhenIdleEvent(entt::entity) { ++idleDestroyRequests; }
}

namespace ecs::SocialSystem {
entt::entity GetParty(entt::entity e) { return PartySystem::GetCharacterParty(e); }
void SetParty(entt::entity e, entt::entity party) {
    PartySystem::SetCharacterParty(e, party);
    if (onSetParty) onSetParty(e, party);
    if (party == entt::null && onTeardown) onTeardown(e);
}
LPDUNGEON GetDungeon(entt::entity) { return nullptr; }
void SetDungeon(entt::entity, LPDUNGEON) {}
}

namespace ecs::PointSystem {
int64_t Get(entt::entity, uint8_t) { return 0; }
int GetLevel(entt::entity e) { const auto* a = ActorOf(e); return a ? a->level : 0; }
int32_t GetMaxHP(entt::entity) { return 1000; }
int32_t GetMaxSP(entt::entity) { return 1000; }
void Change(entt::entity, uint8_t, int64_t, bool, bool, bool) {}
void Compute(entt::entity) {}
void ComputeBattlePoints(entt::entity) {}
}

namespace ecs {
void ChatSystem::Send(entt::entity, uint8_t, const char*, ...) {}
void ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) {}
}

int SkillSystem::GetSkillLevel(entt::entity, uint32_t) { return skillLevel; }

namespace CombatSystem {
void SetLastAttacked(entt::entity, uint32_t) {}
void SetVictim(entt::entity, entt::entity) {}
entt::entity GetVictim(entt::entity) { return entt::null; }
bool CanBeginFight(entt::entity) { return false; }
void BeginFight(entt::entity, entt::entity) {}
bool CanSummon(entt::entity, int) { return canSummon; }
void UpdateAggrPoint(entt::entity, entt::entity, EDamageType, int) {}
}

namespace ecs::MovementSystem {
void SetRotationToXY(entt::entity, int32_t, int32_t) {}
bool Goto(entt::entity e, int32_t x, int32_t y) {
    gotoRequests.emplace_back(x, y);
    return true;
}
void SendMovePacket(entt::entity, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, float) {}
void ExitToSavedLocation(entt::entity e) { warped.push_back(e); }
bool Show(entt::entity e, int32_t, int32_t, int32_t, int32_t, bool) { shown.push_back(e); return true; }
void Stop(entt::entity e) { stopped.push_back(e); }
}

namespace ItemSystem {
bool IsEquipUniqueItem(entt::entity, uint32_t) { return false; }
bool IsEquipUniqueGroup(entt::entity, uint32_t) { return false; }
}

bool NetworkSyncSystem::BuildPartyUpdatePacket(entt::registry&, entt::entity, TPacketGCPartyUpdate& packet) {
    packet = {};
    packet.header = HEADER_GC_PARTY_UPDATE;
    return true;
}

namespace {
entt::entity CreatePartyFor(entt::entity leader) {
    return CPartyManager::instance().CreateParty(leader);
}

void CreateJoinLink(entt::entity leader, entt::entity member) {
    const entt::entity party = ecs::SocialSystem::GetParty(leader);
    Check(party != entt::null, "party missing for member setup");
    PartySystem::Join(party, ecs::PlayerRuntime::GetPlayerID(member));
    PartySystem::Link(party, member);
}

// ---------------------------------------------------------------------------
// Create, index and teardown
// ---------------------------------------------------------------------------
void CreateLinkAndIndex() {
    Reset();
    const auto leader = ActorEntity(101, 101, true, true);
    const auto party = CreatePartyFor(leader);

    Check(party != entt::null && PartySystem::IsValid(party), "party entity was not created");
    Check(ecs::SocialSystem::GetParty(leader) == party, "leader relation missing");
    Check(PartySystem::GetLeaderPID(party) == 101, "leader pid wrong");
    Check(PartySystem::GetMemberCount(party) == 1, "member count wrong");
    Check(PartySystem::GetLeader(party) == leader, "leader entity wrong");
    Check(PartySystem::GetRole(party, 101) == PARTY_ROLE_LEADER, "leader role wrong");
    Check(State(party).isPCParty, "PC party flag missing");
    Check(CountDbPacket(HEADER_GD_PARTY_CREATE) == 1 && CountDbPacket(HEADER_GD_PARTY_ADD) == 1,
        "party create sequence missing");
    Check(liveEvents == 1 && State(party).updateEvent != nullptr, "update event was not armed");

    // Asking again hands back the same party.
    Check(CreatePartyFor(leader) == party, "second create made another party");

    // A relog with the same pid links the new entity through the manager index.
    const auto relogged = ActorEntity(101, 101, true, true);
    Check(CPartyManager::instance().SetParty(relogged), "manager index did not resolve the party");
    Check(PartySystem::GetLeader(party) == relogged, "relog was not linked");
    Check(ecs::SocialSystem::GetParty(relogged) == party, "relog relation missing");
    Check(CountDbPacket(HEADER_GD_PARTY_SET_MEMBER_LEVEL) == 2, "relog level packet missing");

    // Delete retires the entity, the relation, the index and the event.
    const int cancelsBefore = cancels;
    CPartyManager::instance().DeleteParty(party);
    Check(!g_registry.valid(party), "party entity survived delete");
    Check(ecs::SocialSystem::GetParty(relogged) == entt::null, "member relation survived delete");
    Check(!CPartyManager::instance().SetParty(relogged), "index entry survived delete");
    Check(CountDbPacket(HEADER_GD_PARTY_DELETE) == 1, "party delete packet missing");
    Check(cancels == cancelsBefore + 1, "update event was not cancelled");
}

void StaleIndexAndManagerRecovery() {
    Reset();
    const auto party = CPartyManager::instance().P2PCreateParty(77);
    Check(party != entt::null && PartySystem::IsValid(party), "P2P create failed");
    Check(CPartyManager::instance().P2PCreateParty(77) == party, "duplicate P2P create");

    // A directly retired party must not stay reachable; a new DB create on the
    // same leader pid replaces the stale index entry.
    g_registry.destroy(party);
    const auto replacement = CPartyManager::instance().P2PCreateParty(77);
    Check(replacement != entt::null && replacement != party, "stale index entry was not replaced");

    CPartyManager::instance().P2PDeleteParty(77);
    Check(!g_registry.valid(replacement), "P2P delete did not retire the party");
    CPartyManager::instance().P2PDeleteParty(77); // a second delete is a no-op with a log

    // The character side refuses a handle that is not a party.
    const auto leader = ActorEntity(5, 5, true, true);
    const auto notAParty = g_registry.create();
    ecs::SocialSystem::SetParty(leader, notAParty);
    Check(ecs::SocialSystem::GetParty(leader) == entt::null, "non-party handle entered the relation");
    PartySystem::SetCharacterParty(leader, party);
    Check(ecs::SocialSystem::GetParty(leader) == entt::null, "stale party handle entered the relation");
}

// ---------------------------------------------------------------------------
// Membership, roles and levels
// ---------------------------------------------------------------------------
void JoinQuitRolesAndLevels() {
    Reset();
    const auto leader = ActorEntity(1, 1, true, true);
    const auto member = ActorEntity(2, 2, true, true);
    Place(leader, 1000, 1000);
    Place(member, 1000, 1000);
    g_registry.get<Actor>(leader).level = 50;
    g_registry.get<Actor>(member).level = 40;

    const auto party = CreatePartyFor(leader);
    PartySystem::Join(party, 2);
    Check(CountDbPacket(HEADER_GD_PARTY_ADD) == 2, "party add packet missing");
    PartySystem::Link(party, member);
    Check(PartySystem::GetMemberCount(party) == 2, "member count after join");
    Check(ecs::SocialSystem::GetParty(member) == party, "member relation after link");
    Check(PartySystem::GetMemberCount(party) == 2, "member count after link");

    // The near/exp state is refreshed by Update; leadership unlocks the roles.
    skillLevel = 40;
    PartySystem::Update(party);
    Check(PartySystem::GetNearMemberCount(party) == 2, "near member count wrong");
    Check(PartySystem::GetExpBonusPercent(party) > 0, "party exp bonus missing");
    Check(PartySystem::IsNearLeader(party, 2), "member not marked near");

    Check(PartySystem::SetRole(party, 2, PARTY_ROLE_ATTACKER, true), "attacker role refused");
    Check(PartySystem::GetRole(party, 2) == PARTY_ROLE_ATTACKER, "role not stored");
    Check(!PartySystem::SetRole(party, 2, PARTY_ROLE_TANKER, true), "role change accepted");
    Check(PartySystem::SetRole(party, 2, PARTY_ROLE_ATTACKER, false), "attacker role not removed");
    Check(PartySystem::GetRole(party, 2) == PARTY_ROLE_NORMAL, "role not reset");

    PartySystem::P2PSetMemberLevel(party, 1, 50);
    PartySystem::P2PSetMemberLevel(party, 2, 40);
    PartySystem::RequestSetMemberLevel(party, 2, 40);
    Check(CountDbPacket(HEADER_GD_PARTY_SET_MEMBER_LEVEL) == 3, "member level packet missing");
    Check(PartySystem::GetMemberMaxLevel(party) == 50, "max level wrong");
    Check(PartySystem::GetMemberMinLevel(party) == 40, "min level wrong");

    PartySystem::SetFlag(party, "step", 3);
    Check(PartySystem::GetFlag(party, "step") == 3, "party flag lost");
    Check(PartySystem::GetFlag(party, "missing") == 0, "missing flag read");

    // A non-leader quit keeps the party; the leader quit retires it.
    PartySystem::Quit(party, 2);
    Check(PartySystem::GetMemberCount(party) == 1, "quit did not remove the member");
    Check(ecs::SocialSystem::GetParty(member) == entt::null, "quit left the relation");
    Check(CountDbPacket(HEADER_GD_PARTY_REMOVE) == 1, "party remove packet missing");
    Check(g_registry.valid(party), "member quit retired the party");

    PartySystem::Quit(party, 1);
    Check(!g_registry.valid(party), "leader quit left the party alive");
}

void MobPartyAndVnumCount() {
    Reset();
    const auto leader = MobEntity(2001, 501);
    const auto member = MobEntity(2002, 501);
    const auto other = MobEntity(2003, 502);

    const auto party = CreatePartyFor(leader);
    Check(PartySystem::IsValid(party) && !State(party).isPCParty, "mob party not created");
    Check(PartySystem::GetLeaderPID(party) == 2001, "mob leader key is not the packet vid");
    Check(dbPackets == 0, "mob party sent a DB packet");

    CreateJoinLink(leader, member);
    CreateJoinLink(leader, other);
    Check(PartySystem::GetMemberCount(party) == 3, "mob member count wrong");
    Check(PartySystem::CountMemberByVnum(party, 501) == 2, "vnum member count wrong");
    Check(PartySystem::CountMemberByVnum(party, 502) == 1, "vnum member count wrong");

    CPartyManager::instance().DeleteParty(party);
    Check(!g_registry.valid(party), "mob party survived delete");
    Check(ecs::SocialSystem::GetParty(leader) == entt::null, "mob relation survived delete");
}

// ---------------------------------------------------------------------------
// The update event lease
// ---------------------------------------------------------------------------
void UpdateEventStateMachine() {
    Reset();
    const auto leader = ActorEntity(9, 9, true, true);
    const auto party = CreatePartyFor(leader);
    Check(liveEvents == 1, "update event missing");
    LPEVENT event = State(party).updateEvent;
    Check(event != nullptr, "update event handle missing");

    Check(event->func(event, 0) == PASSES_PER_SEC(3), "live update event did not reschedule");

    // A replaced or cancelled lease is stale: the callback must not act on the
    // party that carries a successor lease.
    State(party).updateEvent = nullptr;
    Check(event->func(event, 0) == 0, "stale update event kept running");
    event_cancel(&event);
    Check(event == nullptr, "stale event handle was not released");

    // Destroy cancels the lease that is registered.
    const auto second = ActorEntity(10, 10, true, true);
    const auto secondParty = CreatePartyFor(second);
    Check(State(secondParty).updateEvent != nullptr, "second party did not arm an event");
    const int cancelsBefore = cancels;
    CPartyManager::instance().DeleteParty(secondParty);
    Check(cancels == cancelsBefore + 1, "live event lease was not cancelled");
    Check(!g_registry.valid(secondParty), "destroy left the party behind");
}

// ---------------------------------------------------------------------------
// Loot ownership round-robin
// ---------------------------------------------------------------------------
void OwnershipRoundRobin() {
    Reset();
    const auto leader = ActorEntity(11, 11, true, true);
    const auto nearMember = ActorEntity(12, 12, true, true);
    const auto farMember = ActorEntity(13, 13, true, true);

    const auto party = CreatePartyFor(leader);
    CreateJoinLink(leader, nearMember);
    CreateJoinLink(leader, farMember);
    SetNear(leader, nearMember);
    Place(farMember, 900000, 900000);

    const auto first = PartySystem::GetNextOwnership(party, entt::null, 1000, 1000);
    Check(first == leader, "ownership did not start at the leader");
    const auto second = PartySystem::GetNextOwnership(party, entt::null, 1000, 1000);
    Check(second == nearMember, "ownership did not advance");
    const auto third = PartySystem::GetNextOwnership(party, entt::null, 1000, 1000);
    Check(third == leader, "distant member was handed ownership");

    // A retired member handle is skipped, not chased.
    g_registry.destroy(nearMember);
    const auto fallback = ActorEntity(99, 99, true, true);
    const auto skipped = PartySystem::GetNextOwnership(party, fallback, 1000, 1000);
    Check(skipped != entt::null, "ownership burned on a stale member");

    CPartyManager::instance().DeleteParty(party);
}

// ---------------------------------------------------------------------------
// Teardown reentrancy
// ---------------------------------------------------------------------------
void DestroyReentrancy() {
    Reset();
    const auto leader = ActorEntity(21, 21, true, false);
    const auto member = ActorEntity(22, 22, true, false);
    Place(leader, 1000, 1000);
    Place(member, 1000, 1000);

    const auto party = CreatePartyFor(leader);
    CreateJoinLink(leader, member);
    trackParty = party;

    // The disconnect path erases the member row while the party teardown runs;
    // the leader's quit retires the party and the outer loop must stop.
    onTeardown = [](entt::entity who) {
        ++teardowns;
        if (!g_registry.valid(trackParty))
            return;
        const auto* actor = ActorOf(who);
        if (actor)
            PartySystem::Quit(trackParty, actor->pid);
    };

    CPartyManager::instance().DeleteParty(party);
    Check(!g_registry.valid(party), "reentrant delete left the party alive");
    Check(teardowns >= 1, "teardown callbacks never ran");
    Check(idleDestroyRequests == 2, "each member was not visited exactly once");
    Check(ecs::SocialSystem::GetParty(leader) == entt::null || !g_registry.valid(leader),
        "leader relation survived teardown");
    onTeardown = {};
    trackParty = entt::null;
}

void DestroyNotifiesOfflineMembers() {
    Reset();
    const auto leader = ActorEntity(31, 31, true, true);
    const auto mob = MobEntity(32, 601);

    const auto party = CreatePartyFor(leader);
    PartySystem::Join(party, 32);
    PartySystem::Link(party, mob);

    const int idleBefore = idleDestroyRequests;
    PartySystem::Destroy(party);
    Check(!g_registry.valid(party), "direct destroy left the party alive");
    Check(idleDestroyRequests == idleBefore + 1, "monster member was not asked to retire");
}

void SummonAndHeal() {
    Reset();
    const auto leader = ActorEntity(41, 41, true, true);
    const auto member = ActorEntity(42, 42, true, true);
    Place(leader, 1000, 1000);
    Place(member, 1000, 1000);

    const auto party = CreatePartyFor(leader);
    CreateJoinLink(leader, member);

    PartySystem::SummonToLeader(party, 42);
    Check(shown.size() == 1 && shown.front() == member && stopped.size() == 1,
        "summon did not move the member");

    canSummon = false;
    PartySystem::SummonToLeader(party, 42);
    Check(shown.size() == 1, "summon ignored the leadership check");
    canSummon = true;

    PartySystem::HealParty(party); // Disabled client flow: must stay a no-op.
    Check(shown.size() == 1, "heal moved a member");

    CPartyManager::instance().DeleteParty(party);
}
}

int main() {
    try {
        CreateLinkAndIndex();
        StaleIndexAndManagerRecovery();
        JoinQuitRolesAndLevels();
        MobPartyAndVnumCount();
        UpdateEventStateMachine();
        OwnershipRoundRobin();
        DestroyReentrancy();
        DestroyNotifiesOfflineMembers();
        SummonAndHeal();
        Reset();
        std::cout << "Party lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
