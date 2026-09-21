#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/dungeon.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/sectree.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/affect.h"
#include "../../SRC/Server/GameServer/event.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include <Core/Logging.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Real dungeon.cpp with entity-only fixtures. The private map, the character
// services, the party relation, packets and the event scheduler are doubles.
entt::registry g_registry;
int passes_per_sec = 25;
uint8_t g_bChannel = 1;
entt::dispatcher g_dispatcher;

// Production singletons the manager resolves.
CDungeonManager dungeon_manager;
CHARACTER_MANAGER character_manager;
SECTREE_MANAGER sectree_manager;
quest::CQuestManager quest_manager;

namespace {
int checks = 0;
int liveEvents = 0;
int cancels = 0;
int privateMapsCreated = 0;
int privateMapsDestroyed = 0;
int canceledTimers = 0;
int nextPrivateMap = 50000;
std::vector<int32_t> destroyedPrivateMaps;
std::vector<entt::entity> deadChecks;
std::vector<entt::entity> warped;
std::vector<entt::entity> destroyedCharacters;
std::vector<std::pair<entt::entity, entt::entity>> partyDungeonSets;
std::vector<entt::entity> affected;
std::vector<entt::entity> collectedEntities;
LPEVENT scheduled;
std::vector<LPEVENT> retainedEvents;

struct Actor {
    uint32_t pid = 0;
    uint32_t vid = 0;
    int32_t mapIndex = 1;
    uint8_t charType = 0;
    bool pc = true;
    bool monster = false;
    bool stone = false;
    std::string name;
};

void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}

Actor* ActorOf(entt::entity e) {
    return e != entt::null && g_registry.valid(e) ? g_registry.try_get<Actor>(e) : nullptr;
}

entt::entity ActorEntity(uint32_t pid, uint32_t vid, bool pc) {
    const auto entity = g_registry.create();
    auto& actor = g_registry.emplace<Actor>(entity);
    actor.pid = pid;
    actor.vid = vid;
    actor.pc = pc;
    actor.monster = !pc;
    actor.name = pc ? "player" : "mob";
    g_registry.emplace<ecs::TagCharacter>(entity);
    return entity;
}

entt::entity Player(uint32_t pid) { return ActorEntity(pid, pid, true); }
entt::entity Monster(uint32_t vid) {
    const auto entity = ActorEntity(vid, vid, false);
    g_registry.get<Actor>(entity).charType = CHAR_TYPE_MONSTER;
    return entity;
}

ecs::DungeonState& State(entt::entity dungeon) { return g_registry.get<ecs::DungeonState>(dungeon); }

void Reset() {
    g_registry.clear();
    scheduled.reset();
    retainedEvents.clear();
    Check(liveEvents == 0, "event leaked");
    deadChecks.clear();
    warped.clear();
    destroyedCharacters.clear();
    partyDungeonSets.clear();
    affected.clear();
    collectedEntities.clear();
    destroyedPrivateMaps.clear();
    privateMapsCreated = privateMapsDestroyed = canceledTimers = 0;
    nextPrivateMap = 50000;
}

// A fake private map: for_each asks each registered tree for its entities and
// the test decides who is standing there.
SECTREE_MAP testMap;
int fakeTreeMarker = 0;
const SECTREE* FakeTree() { return reinterpret_cast<const SECTREE*>(&fakeTreeMarker); }
void EnsureFakeTreeRegistered() {
    static bool registered = false;
    if (!registered) {
        registered = testMap.Add(1u, reinterpret_cast<LPSECTREE>(&fakeTreeMarker));
    }
}
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("dungeon-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("dungeon-test");
    return logger;
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

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
SECTREE_MANAGER::SECTREE_MANAGER() = default;
SECTREE_MANAGER::~SECTREE_MANAGER() = default;
quest::CQuestManager::CQuestManager() = default;
quest::CQuestManager::~CQuestManager() = default;
quest::PC::~PC() = default;
quest::NPC::~NPC() = default;

SECTREE_MAP::SECTREE_MAP() = default;
SECTREE_MAP::~SECTREE_MAP() = default;
SECTREE_MAP::SECTREE_MAP(SECTREE_MAP&) = default;

void SECTREE::Collect(FCollectEntity& out) const {
    for (const auto entity : collectedEntities)
        out.Add(entity, this);
}
bool SectreeMember(entt::entity entity, const SECTREE*) {
    return entity != entt::null && g_registry.valid(entity);
}

int32_t SECTREE_MANAGER::CreatePrivateMap(int32_t) {
    ++privateMapsCreated;
    return nextPrivateMap++;
}
void SECTREE_MANAGER::DestroyPrivateMap(int32_t index) {
    ++privateMapsDestroyed;
    destroyedPrivateMaps.push_back(index);
}
LPSECTREE_MAP SECTREE_MANAGER::GetMap(int32_t) { return &testMap; }

entt::entity CHARACTER_MANAGER::SpawnMobEntity(uint32_t vnum, int32_t mapIndex, int32_t, int32_t, int32_t, bool, int, bool) {
    const auto mob = Monster(vnum);
    g_registry.get<Actor>(mob).mapIndex = mapIndex;
    return mob;
}
void CHARACTER_MANAGER::DestroyCharacter(entt::entity e) {
    destroyedCharacters.push_back(e);
    if (g_registry.valid(e))
        g_registry.destroy(e);
}

void quest::CQuestManager::CancelServerTimers(uint32_t) { ++canceledTimers; }

bool regen_do(const char*, int32_t, int, int, entt::entity, bool) { return true; }

namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
bool IsPC(entt::entity e) { const auto* a = ActorOf(e); return a && a->pc; }
bool IsMonster(entt::entity e) { const auto* a = ActorOf(e); return a && a->monster; }
bool IsStone(entt::entity e) { const auto* a = ActorOf(e); return a && a->stone; }
bool IsPet(entt::entity) { return false; }
bool IsNewPet(entt::entity) { return false; }
bool IsMount(entt::entity) { return false; }
uint8_t GetCharType(entt::entity e) { const auto* a = ActorOf(e); return a ? a->charType : 0; }
uint32_t GetRaceNum(entt::entity e) { const auto* a = ActorOf(e); return a ? a->vid : 0; }
std::string_view GetName(entt::entity e) { const auto* a = ActorOf(e); return a ? a->name : "unknown"; }
int32_t GetMapIndex(entt::entity e) { const auto* a = ActorOf(e); return a ? a->mapIndex : 0; }
uint32_t GetPacketVID(entt::entity e) { const auto* a = ActorOf(e); return a ? a->vid : 0; }
int64_t GetHP(entt::entity) { return 100; }
void SetHP(entt::entity, int64_t) {}
entt::entity FindByVID(uint32_t vid) {
    for (const auto e : g_registry.view<Actor>())
        if (g_registry.get<Actor>(e).vid == vid) return e;
    return entt::null;
}
}

namespace ecs {
void ChatSystem::Send(entt::entity, uint8_t, const char*, ...) {}
void ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) {}
}

namespace AffectSystem {
bool AddAffect(entt::entity e, uint32_t, uint8_t, int32_t, uint32_t, int32_t, int32_t, bool, bool) {
    affected.push_back(e);
    return true;
}
CAffect* FindAffect(entt::entity, uint32_t, uint8_t) { return nullptr; }
}

namespace CombatSystem {
void Dead(entt::entity victim, entt::entity, bool) { deadChecks.push_back(victim); }
bool IsDead(entt::entity) { return false; }
}

namespace ecs::MovementSystem {
void SaveExitLocation(entt::entity e) { warped.push_back(e); }
bool WarpSet(entt::entity e, int32_t, int32_t, int32_t) { warped.push_back(e); return true; }
bool Show(entt::entity e, int32_t, int32_t, int32_t, int32_t, bool) { warped.push_back(e); return true; }
void Stop(entt::entity) {}
}

namespace ItemSystem {
bool IsValidItem(entt::entity) { return false; }
bool DestroyItemEntityEcs(entt::entity, const char*) { return true; }
}

namespace PartySystem {
void SetDungeon(entt::entity party, entt::entity dungeon) {
    partyDungeonSets.emplace_back(party, dungeon);
}
}

namespace ecs::SocialSystem {
entt::entity GetParty(entt::entity) { return entt::null; }
entt::entity GetDungeon(entt::entity e) { return DungeonSystem::GetMemberDungeon(e); }
void SetDungeon(entt::entity e, entt::entity dungeon) { DungeonSystem::SetMemberDungeon(e, dungeon); }
void ClearDungeonIfOtherMap(entt::entity e, int32_t mapIndex) {
    DungeonSystem::ClearMemberDungeonIfOtherMap(e, mapIndex);
}
}

namespace {
bool PartyHandleClearedTo(entt::entity party) {
    for (const auto& [target, dungeon] : partyDungeonSets)
        if (target == party && dungeon == entt::null)
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// Manager create / find / destroy
// ---------------------------------------------------------------------------
void CreateFindDestroy() {
    Reset();
    const entt::entity first = CDungeonManager::instance().Create(101);
    Check(first != entt::null && DungeonSystem::IsValid(first), "dungeon was not created");
    Check(privateMapsCreated == 1, "private map was not created");
    Check(State(first).mapIndex == 50000 && State(first).originalMapIndex == 101, "dungeon indices wrong");
    Check(CDungeonManager::instance().Find(State(first).id) == first, "id lookup failed");
    Check(CDungeonManager::instance().FindByMapIndex(50000) == first, "map lookup failed");
    Check(CDungeonManager::instance().Find(State(first).id + 99) == entt::null, "unknown id resolved");

    const entt::entity second = CDungeonManager::instance().Create(102);
    Check(second != entt::null && second != first, "second dungeon missing");
    Check(State(second).id != State(first).id, "ids were reused");

    const int32_t firstMap = State(first).mapIndex;
    const uint32_t firstId = State(first).id;
    CDungeonManager::instance().Destroy(firstId);
    Check(CDungeonManager::instance().FindByMapIndex(firstMap) == entt::null, "destroyed map still resolves");
    Check(!g_registry.valid(first), "destroyed dungeon entity survived");
    Check(privateMapsDestroyed == 1 && destroyedPrivateMaps.front() == firstMap, "private map not destroyed");
    Check(canceledTimers == 1, "server timers were not cancelled");
    Check(DungeonSystem::IsValid(second), "destroy removed the wrong dungeon");

    // A directly retired index entry must not resolve and a repeated destroy
    // must be a no-op.
    CDungeonManager::instance().Destroy(firstId);
    const uint32_t secondId = State(second).id;
    g_registry.destroy(second);
    Check(CDungeonManager::instance().Find(secondId) == entt::null, "stale index entry resolved");
    CDungeonManager::instance().Destroy(secondId);
    Check(privateMapsDestroyed == 1, "repeated destroy freed another map");
}

// ---------------------------------------------------------------------------
// Character membership and the dead event
// ---------------------------------------------------------------------------
void MembershipAndDeadEvent() {
    Reset();
    const entt::entity dungeon = CDungeonManager::instance().Create(101);
    const int32_t mapIndex = State(dungeon).mapIndex;

    const entt::entity player = Player(11);
    DungeonSystem::Join_Coords(dungeon, player, 100, 100, mapIndex);
    Check(State(dungeon).members.count(player) == 1, "entry did not register the member");
    Check(!warped.empty() && warped.back() == player, "entry did not warp the member");
    Check(DungeonSystem::GetMemberDungeon(player) == dungeon, "member relation missing");

    // A warp on the dungeon's own map keeps the membership.
    DungeonSystem::ClearMemberDungeonIfOtherMap(player, mapIndex);
    Check(DungeonSystem::GetMemberDungeon(player) == dungeon, "same-map warp dropped the membership");

    // Leaving the map releases it and arms the dead event.
    DungeonSystem::ClearMemberDungeonIfOtherMap(player, 1);
    Check(DungeonSystem::GetMemberDungeon(player) == entt::null, "leave kept the membership");
    Check(State(dungeon).members.empty(), "leave kept the member counted");
    LPEVENT event = State(dungeon).deadEvent;
    Check(event != nullptr && liveEvents >= 1, "dead event was not armed");

    // A live player on the map blocks the teardown and the callback re-arms.
    EnsureFakeTreeRegistered();
    collectedEntities.push_back(Player(12));
    Check(DungeonSystem::HasLivePlayers(dungeon), "live player was not found on the map");
    Check(event->func(event, 0) == PASSES_PER_SEC(3), "teardown ran under a live player");
    Check(DungeonSystem::IsValid(dungeon), "dungeon was destroyed under a live player");
    Check(State(dungeon).deadEvent == event, "re-armed lease was replaced");

    // When the map is empty the callback retires the instance.
    collectedEntities.clear();
    Check(!DungeonSystem::HasLivePlayers(dungeon), "empty map reported a player");
    const uint32_t id = State(dungeon).id;
    Check(event->func(event, 0) == 0, "teardown did not finish");
    Check(CDungeonManager::instance().Find(id) == entt::null, "dead event left the dungeon registered");
    Check(privateMapsDestroyed == 1, "dead event left the private map");
}

// ---------------------------------------------------------------------------
// Monster counting
// ---------------------------------------------------------------------------
void MonsterCounting() {
    Reset();
    const entt::entity dungeon = CDungeonManager::instance().Create(101);
    const entt::entity other = CDungeonManager::instance().Create(102);

    const entt::entity first = Monster(501);
    DungeonSystem::SetMemberDungeon(first, dungeon);
    Check(DungeonSystem::CountMonster(dungeon) == 1, "monster entry was not counted");
    Check(DungeonSystem::CountMonster(other) == 0, "monster was counted into the wrong dungeon");

    const entt::entity second = Monster(502);
    DungeonSystem::SetMemberDungeon(second, dungeon);
    Check(DungeonSystem::CountMonster(dungeon) == 2, "second monster was not counted");

    // Reassignment must not leak into the old dungeon (legacy IncMonster did).
    DungeonSystem::SetMemberDungeon(second, other);
    Check(DungeonSystem::CountMonster(dungeon) == 1, "reassignment left the old count");
    Check(DungeonSystem::CountMonster(other) == 1, "reassignment did not count the new dungeon");

    // A monster destroyed outside the kill path is pruned on read.
    DungeonSystem::RemoveMonster(first);
    Check(DungeonSystem::CountMonster(dungeon) == 0, "explicit removal did not decrement");
    g_registry.destroy(second);
    Check(DungeonSystem::CountMonster(other) == 0, "destroyed monster stayed in the count");
}

// ---------------------------------------------------------------------------
// Unique mobs and flags
// ---------------------------------------------------------------------------
void UniqueMobsAndFlags() {
    Reset();
    const entt::entity dungeon = CDungeonManager::instance().Create(101);
    Check(State(dungeon).mapIndex != 0, "map index missing");

    Check(DungeonSystem::GetFlag(dungeon, "floor") == 0, "missing flag default changed");
    DungeonSystem::SetFlag(dungeon, "floor", 3);
    Check(DungeonSystem::GetFlag(dungeon, "floor") == 3, "flag was not stored");

    const entt::entity boss = DungeonSystem::SpawnMob(dungeon, 700, 10, 20);
    Check(boss != entt::null && DungeonSystem::CountMonster(dungeon) == 1, "spawn did not register the mob");
    Check(affected.size() == 0, "spawn added an affect early");

    DungeonSystem::SetUnique(dungeon, "boss", ecs::PlayerRuntime::GetPacketVID(boss));
    Check(!affected.empty() && affected.back() == boss, "unique affect missing");
    Check(DungeonSystem::GetUniqueVid(dungeon, "boss") == static_cast<int32_t>(ecs::PlayerRuntime::GetPacketVID(boss)),
        "unique vid lookup failed");
    Check(DungeonSystem::GetUniqueVid(dungeon, "missing") == -1, "missing unique did not answer -1");
    Check(!DungeonSystem::IsUniqueDead(dungeon, "boss"), "fresh unique counts as dead");

    DungeonSystem::KillUnique(dungeon, "boss");
    Check(!deadChecks.empty() && deadChecks.back() == boss, "unique was not killed");

    // A retired unique handle answers -1 instead of a recycled mob.
    g_registry.destroy(boss);
    Check(DungeonSystem::GetUniqueVid(dungeon, "boss") == -1, "retired unique handle still resolved");
}

// ---------------------------------------------------------------------------
// Teardown clears every relation
// ---------------------------------------------------------------------------
void DestroyClearsRelations() {
    Reset();
    const entt::entity dungeon = CDungeonManager::instance().Create(101);
    const entt::entity party = g_registry.create();
    const entt::entity player = Player(21);
    const entt::entity mob = Monster(601);

    DungeonSystem::IncPartyMember(dungeon, party, player);
    DungeonSystem::SetMemberDungeon(mob, dungeon);
    partyDungeonSets.clear();

    CDungeonManager::instance().Destroy(State(dungeon).id);

    Check(!g_registry.valid(dungeon), "destroy left the dungeon entity");
    Check(DungeonSystem::GetMemberDungeon(player) == entt::null, "member relation survived destroy");
    Check(ecs::SocialSystem::GetDungeon(mob) == entt::null, "monster relation survived destroy");
    Check(PartyHandleClearedTo(party), "party dungeon handle survived destroy");

    // A stale handle never resolves to a recycled entity.
    Check(DungeonSystem::GetId(dungeon) == 0 && DungeonSystem::GetMapIndex(dungeon) == 0,
        "stale handle answered state");
    Check(CDungeonManager::instance().FindByMapIndex(50000) == entt::null, "stale map index resolved");
}
}

int main() {
    try {
        CreateFindDestroy();
        MembershipAndDeadEvent();
        MonsterCounting();
        UniqueMobsAndFlags();
        DestroyClearsRelations();
        Reset();
        std::cout << "Dungeon lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
