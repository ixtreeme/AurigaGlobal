#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/dungeon/dungeon.h"
#include "../../SRC/Server/GameServer/dungeon/PyramidDungeonRazor93.h"
#include "../../SRC/Server/GameServer/dungeon/OrcsDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/TritonTempleDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/ValentineDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/EasterDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/NightmareDungeonRazor93.h"
#include "../../SRC/Server/GameServer/dungeon/RuneDungeon.h"
#include "../../SRC/Server/GameServer/dungeon/Halloween2022Dungeon.h"
#include "../../SRC/Server/GameServer/dungeon/VikingDungeon.h"
#include "../../SRC/Server/GameServer/network/db.h"
#include "../../SRC/Server/GameServer/combat/battle_pass.h"
#include "../../SRC/Server/GameServer/item/item_manager.h"
#include "../../SRC/Server/GameServer/social/party.h"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/world/sectree_manager.h"
#include "../../SRC/Server/GameServer/world/sectree.h"
#include "../../SRC/Server/GameServer/quest/questmanager.h"
#include "../../SRC/Server/GameServer/combat/affect.h"
#include "../../SRC/Server/GameServer/world/event.h"
#include "../../SRC/Server/GameServer/core/config.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_runtime_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
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
uint32_t nextSpawnVid = 100000;
std::vector<int32_t> destroyedPrivateMaps;
std::vector<entt::entity> deadChecks;
std::vector<entt::entity> warped;
std::vector<entt::entity> destroyedCharacters;
std::vector<std::pair<entt::entity, entt::entity>> partyDungeonSets;
std::vector<entt::entity> affected;
std::vector<entt::entity> collectedEntities;
LPEVENT scheduled;
std::vector<LPEVENT> retainedEvents;
std::vector<entt::entity> spawned;
std::vector<std::string> notices;
std::vector<std::tuple<int32_t, int32_t, int32_t>> warpTargets;
bool warpResult = true;
int regensLoaded = 0;
bool itemDeliverySucceeds = false;
int rewardDeliveries = 0;
std::map<std::string, int64_t> eventFlags;

struct Actor {
    uint32_t pid = 0;
    uint32_t vid = 0;
    uint32_t race = 0;
    int32_t mapIndex = 1;
    uint8_t charType = 0;
    bool pc = true;
    bool monster = false;
    bool stone = false;
    bool dead = false;
    bool invincible = false;
    int level = 75;
    std::string name;
    entt::entity party = entt::null;
    std::map<std::string, int64_t> flags;
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
    actor.race = vid;
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
    std::vector<uint32_t> ids;
    for (auto e:g_registry.view<ecs::DungeonState>()) ids.push_back(State(e).id);
    for (auto id:ids) CDungeonManager::instance().Destroy(id);
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
    spawned.clear(); notices.clear(); eventFlags.clear(); regensLoaded = 0;
    itemDeliverySucceeds=false;rewardDeliveries=0;
    warpTargets.clear(); warpResult = true;
    destroyedPrivateMaps.clear();
    privateMapsCreated = privateMapsDestroyed = canceledTimers = 0;
    nextPrivateMap = 50000;
    nextSpawnVid = 100000;
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
entt::entity CHARACTER_MANAGER::FindEntity(uint32_t vid) { return ecs::PlayerRuntime::FindByVID(vid); }
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
    CDungeonManager::instance().FindByMapIndex(nextPrivateMap); // prune a retired fixture index
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
    g_registry.get<Actor>(mob).vid = nextSpawnVid++;
    spawned.push_back(mob);
    return mob;
}
void CHARACTER_MANAGER::DestroyCharacter(entt::entity e) {
    destroyedCharacters.push_back(e);
    if (g_registry.valid(e))
        g_registry.destroy(e);
}

void quest::CQuestManager::CancelServerTimers(uint32_t) { ++canceledTimers; }

bool regen_do(const char* filename, int32_t, int, int, entt::entity dungeon, bool) {
    if (std::string(filename) == "data/dungeon/pyramide/regen3.json") {
        ++regensLoaded;
        for (auto race : {4621, 4620, 4618, 4619}) DungeonSystem::SpawnMob(dungeon, race, 1, 1);
    }
    return true;
}

namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
bool IsPC(entt::entity e) { const auto* a = ActorOf(e); return a && a->pc; }
bool CanWarp(entt::entity e) { return IsPC(e); }
uint32_t GetPlayerID(entt::entity e) { const auto* a = ActorOf(e); return a ? a->pid : 0; }
bool IsMonster(entt::entity e) { const auto* a = ActorOf(e); return a && a->monster; }
bool IsStone(entt::entity e) { const auto* a = ActorOf(e); return a && a->stone; }
bool IsPet(entt::entity) { return false; }
bool IsNewPet(entt::entity) { return false; }
bool IsMount(entt::entity) { return false; }
uint8_t GetCharType(entt::entity e) { const auto* a = ActorOf(e); return a ? a->charType : 0; }
uint32_t GetRaceNum(entt::entity e) { const auto* a = ActorOf(e); return a ? a->race : 0; }
std::string_view GetName(entt::entity e) { const auto* a = ActorOf(e); return a ? a->name : "unknown"; }
int32_t GetMapIndex(entt::entity e) { const auto* a = ActorOf(e); return a ? a->mapIndex : 0; }
uint32_t GetPacketVID(entt::entity e) { const auto* a = ActorOf(e); return a ? a->vid : 0; }
uint64_t GetHP(entt::entity) { return 100; }
void SetHP(entt::entity, uint64_t) {}
void SetMaxHP(entt::entity, uint64_t) {}
int GetHPPct(entt::entity) { return 100; }
int GetX(entt::entity) { return 0; }
int GetY(entt::entity) { return 0; }
int GetZ(entt::entity) { return 0; }
uint8_t GetEmpire(entt::entity) { return 1; }
uint8_t GetGMLevel(entt::entity) { return 0; }
DESC* GetDesc(entt::entity) { return nullptr; }
int64_t GetRankPoints(entt::entity, int) { return 0; }
bool SetRankPoints(entt::entity, int, int64_t) { return true; }
uint8_t GetBattlePassId(entt::entity) { return 0; }
uint32_t GetMissionProgress(entt::entity, uint32_t, uint32_t) { return 0; }
bool UpdateMissionProgress(entt::entity, uint32_t, uint32_t, uint32_t, uint32_t, bool) { return true; }
uint64_t GetQuestDamage(entt::entity, int) { return 0; }
entt::entity FindByVID(uint32_t vid) {
    for (const auto e : g_registry.view<Actor>())
        if (g_registry.get<Actor>(e).vid == vid) return e;
    return entt::null;
}
}

namespace ecs {
void ChatSystem::Send(entt::entity, uint8_t, const char* text, ...) { notices.push_back(text); }
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
void Dead(entt::entity victim, entt::entity, bool) {
    auto* a=ActorOf(victim); if (!a || a->dead || a->invincible) return;
    deadChecks.push_back(victim); a->dead=true; DungeonSystem::RemoveMonster(victim);
}
bool IsDead(entt::entity e) { const auto* a=ActorOf(e); return a && a->dead; }
bool SetInvincible(entt::entity e, bool value) { auto* a=ActorOf(e); if (!a) return false; a->invincible=value;return true; }
void SetAttackMultiplier(entt::entity, float) {}
void SetDamageMultiplier(entt::entity, float) {}
}

namespace ecs::MovementSystem {
void SaveExitLocation(entt::entity e) { warped.push_back(e); }
bool WarpSet(entt::entity e, int32_t x, int32_t y, int32_t map) {
    warped.push_back(e); warpTargets.emplace_back(x,y,map); return warpResult;
}
bool Show(entt::entity e, int32_t, int32_t, int32_t, int32_t, bool) { warped.push_back(e); return true; }
void Stop(entt::entity) {}
void SetWarpLocation(entt::entity, int32_t, int32_t, int32_t) {}
void ExitToSavedLocation(entt::entity e) { warped.push_back(e); }
}

namespace ecs::QuestSystem {
int64_t GetFlag(entt::entity e, std::string_view flag) { return g_registry.get<Actor>(e).flags[std::string(flag)]; }
void SetFlag(entt::entity e, std::string_view flag, int64_t value) { g_registry.get<Actor>(e).flags[std::string(flag)] = value; }
}
int ecs::PointSystem::GetLevel(entt::entity e) { const auto* a=ActorOf(e); return a ? a->level : 0; }
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) {}
time_t get_global_time() { return 1000; }
int number_ex(int, int to, const char*, int) { return to; }
void SendNotice(const char*
#ifdef ENABLE_FULL_NOTICE
    , bool
#endif
) {}
void BroadcastNotice(const char*, bool) {}
void BroadcastNoticeNew(uint8_t, uint8_t, int, uint32_t, const char*, ...) {}

// SQL completion is observed at the controller boundary, never sent anywhere.
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
DBManager::DBManager() = default;
DBManager::~DBManager() = default;
DBManager testDbManager;
void DBManager::Query(const char*, ...) {}
void CAsyncSQL::ReturnQuery(const char*, void* data) { delete static_cast<CFuncQueryInfo*>(data); }
MYSQL_ROW SQLResult::FetchRow() { return nullptr; }
bool CBattlePass::BattlePassMissionGetInfo(uint8_t, uint8_t, uint32_t*, uint32_t*) { return false; }
CBattlePass::CBattlePass() = default;
CBattlePass::~CBattlePass() = default;
TItemTable* ITEM_MANAGER::GetTable(uint32_t) { return nullptr; }
ITEM_MANAGER::ITEM_MANAGER() = default;
ITEM_MANAGER::~ITEM_MANAGER() = default;
ITEM_MANAGER testItemManager;
entt::entity ITEM_MANAGER::CreateItem(uint32_t, uint32_t, uint32_t, bool, int, bool) { return entt::null; }
int64_t quest::CQuestManager::GetEventFlag(const std::string& name) { return eventFlags[name]; }
void quest::CQuestManager::SetEventFlag(const std::string& name, int64_t value) { eventFlags[name] = value; }

namespace ItemSystem {
int CountItem(entt::entity, uint32_t) { return 1; }
bool RemoveSpecifyItemEcs(entt::entity, uint32_t, uint32_t, bool) { return true; }
bool IsValidItem(entt::entity) { return false; }
bool DestroyItemEntityEcs(entt::entity, const char*) { return true; }
entt::entity AutoGiveItemEcs(entt::entity, uint32_t, uint32_t, int, bool, bool) {
    if (!itemDeliverySucceeds) return entt::null;
    ++rewardDeliveries;return g_registry.create();
}
uint32_t GetItemVnum(entt::entity) { return 0; }
bool CanConsumeOwnedItem(entt::entity, entt::entity, uint32_t, ItemCostStorage) { return true; }
bool ConsumeOwnedItemCosts(entt::entity, std::span<const ItemCost>) { return true; }
bool PlaceItemOnGround(entt::entity, int, const PIXEL_POSITION&, int) { return true; }
bool SetGroundOwnership(entt::entity, entt::entity, int) { return true; }
}

namespace PartySystem {
uint32_t GetLeaderPID(entt::entity) { return 999; }
entt::entity GetLeader(entt::entity) { return entt::null; }
uint32_t GetMemberCount(entt::entity) { return 2; }
uint32_t GetNearMemberCount(entt::entity) { return 2; }
void SetDungeon(entt::entity party, entt::entity dungeon) {
    partyDungeonSets.emplace_back(party, dungeon);
}
}

namespace ecs::SocialSystem {
entt::entity GetParty(entt::entity e) { const auto* actor = ActorOf(e); return actor ? actor->party : entt::null; }
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

void PyramidEntryAndProgression() {
    Reset(); nextPrivateMap = 3570000;
    auto& pyramid = CPyramidDungeonRazor93::instance();
    const auto player = Player(41); g_registry.get<Actor>(player).mapIndex = 219;
    Check(pyramid.OnClickNpc(player), "pyramid entrance was not handled");
    const auto dungeon = CDungeonManager::instance().FindByMapIndex(3570000);
    Check(dungeon != entt::null, "pyramid entrance did not create an instance");
    // The network transfer can take several seconds. Starting on the NPC click
    // sends the one-shot notice to an empty map and marks it already started.
    Check(!scheduled && spawned.empty(), "pyramid prepared before the player arrived");
    g_registry.get<Actor>(player).mapIndex = 3570000; collectedEntities = {player};
    EnsureFakeTreeRegistered();
    pyramid.OnPlayerLogin(player);
    Check(DungeonSystem::GetMemberDungeon(player) == dungeon, "pyramid arrival did not attach the player");
    Check(scheduled && liveEvents == 1, "pyramid arrival did not schedule preparation");
    const auto prepare = scheduled;
    pyramid.OnPlayerLogin(player);
    Check(scheduled == prepare && liveEvents == 1, "repeated login duplicated preparation");
    Check(prepare->func(prepare, 0) == 0, "pyramid preparation did not finish");
    Check(spawned.size() == 7 && DungeonSystem::GetFlag(dungeon, "step") == 7,
        "pyramid did not spawn its seven initial metins");
    Check(!notices.empty() && notices.back() == "[Pyramid] Destroy all metins!",
        "arrived player missed the pyramid start notice");
    const auto unrelated = Monster(8474); g_registry.get<Actor>(unrelated).mapIndex = 3579999;
    pyramid.OnMobKilled(player, unrelated);
    Check(DungeonSystem::GetFlag(dungeon, "step") == 7, "a kill from another private instance advanced the pyramid");
    const auto kill = [&](entt::entity victim) {
        DungeonSystem::RemoveMonster(victim); // Real CombatSystem::Dead does this before Reward.
        pyramid.OnMobKilled(player, victim);
    };
    auto metins = spawned;
    for (auto metin : metins) kill(metin);
    Check(DungeonSystem::GetFlag(dungeon, "step") == 0 && spawned.size() == 8 &&
        ecs::PlayerRuntime::GetRaceNum(spawned.back()) == 4157, "last metin did not spawn the stone");
    kill(spawned.back());
    Check(regensLoaded == 1 && spawned.size() == 12, "stone death did not load the monster wave");
    std::vector<entt::entity> wave(spawned.begin() + 8, spawned.end());
    for (size_t i = 0; i < wave.size(); ++i) {
        kill(wave[i]);
        Check(spawned.size() == (i + 1 == wave.size() ? 13 : 12), "boss spawned before the last monster or not at all");
    }
    Check(ecs::PlayerRuntime::GetRaceNum(spawned.back()) == 4158, "wave spawned the wrong boss");
    kill(spawned.back());
    Check(DungeonSystem::GetFlag(dungeon, "was_completed") == 1 &&
        ecs::PlayerRuntime::GetRaceNum(spawned.back()) == 9331, "boss death did not complete the run/spawn exit NPC");
    const auto count = spawned.size();
    pyramid.OnPlayerLogin(player);
    Check(spawned.size() == count && DungeonSystem::GetFlag(dungeon, "was_completed") == 1,
        "login restarted a completed pyramid");
}

void PyramidFirstPartyArrivalAndMissingInstance() {
    Reset(); nextPrivateMap = 3570001;
    auto& pyramid = CPyramidDungeonRazor93::instance();
    const auto dungeon = CDungeonManager::instance().Create(357), player = Player(42);
    g_registry.get<Actor>(player).mapIndex = 3570001;
    // The leader may load later than another member. The first valid arrival
    // must start exactly once, even when that arrival is not the leader.
    g_registry.get<Actor>(player).party = g_registry.create();
    collectedEntities = {player};
    pyramid.OnPlayerLogin(player);
    Check(scheduled && DungeonSystem::GetFlag(dungeon, "floor") == 2,
        "non-leader first arrival left pyramid uninitialized");
    Check(scheduled->func(scheduled, 0) == 0 && spawned.size() == 7,
        "party arrival failed to spawn the initial metins");
    g_registry.get<Actor>(player).party = entt::null;
    g_registry.get<Actor>(player).mapIndex = 3579999;
    warped.clear(); pyramid.OnPlayerLogin(player);
    Check(!warped.empty() && warped.back() == player, "missing private instance stranded a reconnecting player");
}

void PyramidRejoinCoordinatesAndFailedWarp() {
    Reset(); nextPrivateMap = 3570002;
    const auto dungeon = CDungeonManager::instance().Create(357), player = Player(43);
    DungeonSystem::SetFlag(dungeon, "floor", 2);
    auto& actor = g_registry.get<Actor>(player); actor.mapIndex = 219;
    actor.flags = {{"dungeonpyramid_razor93.disconnect", 1300}, {"dungeonpyramid_razor93.idx", 3570002},
        {"dungeonpyramid_razor93.ch", 1}};
    auto& pyramid = CPyramidDungeonRazor93::instance();
    warpResult = false;
    pyramid.OnClickNpc(player);
    Check(actor.flags["dungeonpyramid_razor93.idx"] == 3570002, "failed pyramid rejoin erased the recovery flags");
    Check(warpTargets.back() == std::tuple<int32_t,int32_t,int32_t>{979600,2366000,3570002},
        "pyramid rejoin used coordinates from a different base map");
    warpResult = true;
    pyramid.OnClickNpc(player);
    Check(actor.flags["dungeonpyramid_razor93.idx"] == 0 && actor.flags["dungeonpyramid_razor93.disconnect"] == 0,
        "successful pyramid rejoin retained consumed recovery flags");
}

struct Controller {
    int base; const char* floor; uint32_t objective;
    std::function<void(entt::entity)> login;
    std::function<void(entt::entity,entt::entity)> kill;
};
std::vector<Controller> Controllers() {
    return {
        {355,"floor",8009,[](auto e){COrcsDungeon::instance().OnPlayerLogin(e);},[](auto a,auto b){COrcsDungeon::instance().OnMobKilled(a,b);}},
        {369,"floor",8054,[](auto e){CTritonTempleDungeon::instance().OnPlayerLogin(e);},[](auto a,auto b){CTritonTempleDungeon::instance().OnMobKilled(a,b);}},
        {377,"val_floor",171,[](auto e){CValentineDungeon::instance().OnPlayerLogin(e);},[](auto a,auto b){CValentineDungeon::instance().OnMobKilled(a,b);}},
        {366,"easter_floor",8461,[](auto e){CEasterDungeon::instance().OnPlayerLogin(e);},[](auto a,auto b){CEasterDungeon::instance().OnMobKilled(a,b);}},
        {373,"floor",8042,[](auto e){CNightmareDungeonRazor93::instance().OnPlayerLogin(e);},[](auto a,auto b){CNightmareDungeonRazor93::instance().OnMobKilled(a,b);}},
        {218,"floor",8202,[](auto e){CRuneDungeon::instance().OnPlayerLogin(e);},[](auto a,auto b){CRuneDungeon::instance().OnMobKilled(a,b);}},
        {161,"hw22_floor",8738,[](auto e){CHalloween2022Dungeon::instance().OnPlayerLogin(e);},[](auto a,auto b){CHalloween2022Dungeon::instance().OnMobKilled(a,b);}},
        {179,"vk_floor",8821,[](auto e){CVikingDungeon::instance().OnPlayerLogin(e);},[](auto a,auto b){CVikingDungeon::instance().OnMobKilled(a,b);}},
        {357,"floor",8474,[](auto e){CPyramidDungeonRazor93::instance().OnPlayerLogin(e);},[](auto a,auto b){CPyramidDungeonRazor93::instance().OnMobKilled(a,b);}}
    };
}

void ControllerInstanceIsolation() {
    for (const auto& c:Controllers()) {
        Reset();nextPrivateMap=c.base*10000;
        const auto dungeon=CDungeonManager::instance().Create(c.base), player=Player(60), victim=Monster(c.objective);
        g_registry.get<Actor>(player).mapIndex=c.base*10000;
        g_registry.get<Actor>(victim).mapIndex=c.base*10000+1;
        DungeonSystem::SetFlag(dungeon,c.floor,c.base==179?3:c.base==377||c.base==366||c.base==218||c.base==161?1:2);
        for (const auto& key:{"step","step2","val_step","easter_step","vk_floor2_remain"}) DungeonSystem::SetFlag(dungeon,key,4);
        DungeonSystem::SetFlag(dungeon,"hw22_can_destroy_first_stone",1);
        const auto before=State(dungeon).flags;
        c.kill(player,victim);
        Check(State(dungeon).flags==before && spawned.empty() && !scheduled,"cross-instance kill changed a dungeon");
        c.kill(entt::null,victim);
        Check(State(dungeon).flags==before,"missing killer changed a dungeon");
    }
}

void PrototypeCountAndProtectedCleanup() {
    Reset();const auto dungeon=CDungeonManager::instance().Create(367);
    auto first=DungeonSystem::SpawnMob(dungeon,5121,1,1),second=DungeonSystem::SpawnMob(dungeon,5122,1,1),boss=DungeonSystem::SpawnMob(dungeon,2832,1,1);
    Check(DungeonSystem::CountMonster(dungeon,5121)==1 && DungeonSystem::CountMonster(dungeon,5122)==1,"prototype filter includes other objectives");
    g_registry.get<Actor>(first).dead=true;
    Check(DungeonSystem::CountMonster(dungeon,5121)==0 && DungeonSystem::CountMonster(dungeon)==2,"dead corpse held wave counter");
    CombatSystem::SetInvincible(boss,true);collectedEntities={second,boss};EnsureFakeTreeRegistered();
    DungeonSystem::KillAll(dungeon);
    Check(CombatSystem::IsDead(boss) && DungeonSystem::CountMonster(dungeon)==0,"cleanup left protected boss in counter");
    Check(IS_SET(g_registry.get<ecs::CharacterRuntimeFlagsComponent>(boss).instantFlag,INSTANT_FLAG_NO_REWARD),"cleanup retained quest reward credit");
}

void DelayedControllerStartupAndCorpseCounters() {
    auto controllers=Controllers();
    for (size_t index=0;index<4;++index) {
        auto& c=controllers[index];Reset();nextPrivateMap=c.base*10000;
        auto player=Player(61);g_registry.get<Actor>(player).mapIndex=219;
        g_registry.get<Actor>(player).level=index==0?50:index==1?65:75;
        const bool entered=index==0?COrcsDungeon::instance().OnClickNpc(player):index==1?CTritonTempleDungeon::instance().OnClickNpc(player):index==2?CValentineDungeon::instance().OnClickNpc(player):CEasterDungeon::instance().OnClickNpc(player);
        const auto dungeon=CDungeonManager::instance().FindByMapIndex(c.base*10000);
        Check(entered && dungeon!=entt::null,"controller entrance failed");
        Check(!scheduled && spawned.empty(),"controller prepared before client arrival");
        g_registry.get<Actor>(player).mapIndex=c.base*10000;collectedEntities={player};EnsureFakeTreeRegistered();
        c.login(player);auto prepare=scheduled;Check(prepare!=nullptr,"arrival did not schedule controller startup");
        prepare->func(prepare,0);
        Check(spawned.size()==(index==0?5:index==1?36:11),"controller did not create initial objectives");
        if (index<2) {
            auto end=scheduled;auto seals=spawned;auto boss=spawned.back();
            for (size_t n=0;n+1<seals.size();++n) {DungeonSystem::RemoveMonster(seals[n]);c.kill(player,seals[n]);}
            Check(!g_registry.get<Actor>(boss).invincible,"last seal did not unlock boss");
            end->func(end,0); // end timer cancels the controller's remaining leases.
        } else {
            auto stones=spawned;
            for(auto stone:stones){DungeonSystem::RemoveMonster(stone);g_registry.get<Actor>(stone).dead=true;c.kill(player,stone);}
            auto transition=scheduled;Check(transition!=prepare,"last stone did not schedule next floor");
            transition->func(transition,0);
            auto wave=std::vector<entt::entity>(spawned.begin()+11,spawned.end());
            Check(wave.size()==17,"floor two objectives missing");
            collectedEntities={player};
            for(auto mob:wave){DungeonSystem::RemoveMonster(mob);g_registry.get<Actor>(mob).dead=true;collectedEntities.push_back(mob);c.kill(player,mob);}
            auto settle=scheduled;settle->func(settle,0);
            Check(DungeonSystem::GetFlag(dungeon,c.floor)==3,"dead corpses prevented floor two boss spawn");
            c.kill(player,spawned.back());auto exit=scheduled;exit->func(exit,0);
        }
    }
}

void RejoinFloorsAndRejectedTransfers() {
    for (int base:{377,366,373,218,161,179}) {
        Reset();nextPrivateMap=base*10000+9;
        const auto dungeon=CDungeonManager::instance().Create(base), player=Player(70);
        auto& actor=g_registry.get<Actor>(player);actor.mapIndex=219;
        const std::string prefix=base==377?"valentine_dungeon":base==366?"easter_dungeon":base==373?"nightmare_razor93":base==218?"rune_zone":base==161?"halloween2022":"vikingdungeon_zone";
        // Nightmare uses the original Lua quest prefix.
        const std::string actualPrefix=base==373?"nightmare_zone_razor93_prepare":prefix;
        actor.flags={{actualPrefix+".disconnect",1300},{actualPrefix+".idx",base*10000+9},{actualPrefix+".ch",1}};
        DungeonSystem::SetFlag(dungeon,base==377?"val_floor":base==366?"easter_floor":base==161?"hw22_floor":base==179?"vk_floor":"floor",2);
        auto npc=Monster(base==161?9475:9615);g_registry.get<Actor>(npc).mapIndex=219;
        auto click=[&]{
            if(base==377)CValentineDungeon::instance().OnClickNpc(player);
            else if(base==366)CEasterDungeon::instance().OnClickNpc(player);
            else if(base==373)CNightmareDungeonRazor93::instance().OnClickNpc(player);
            else if(base==218)CRuneDungeon::instance().OnClickNpc(player);
            else if(base==161)CHalloween2022Dungeon::instance().OnClickNpc(player,npc);
            else CVikingDungeon::instance().OnClickNpc(player,npc);
        };
        warpResult=false;click();
        Check(!warpTargets.empty(),"rejoin did not attempt transfer");
        Check(actor.flags[actualPrefix+".disconnect"]==1300,"rejected rejoin consumed recovery window");
        if(base==377)Check(warpTargets.back()==std::tuple<int32_t,int32_t,int32_t>{1156400,233700,3770009},"Valentine rejoin returned to floor one");
        if(base==366)Check(warpTargets.back()==std::tuple<int32_t,int32_t,int32_t>{747100,61400,3660009},"Easter rejoin returned to floor one");
        warpResult=true;click();
        if(base!=377 && base!=366)Check(actor.flags[actualPrefix+".disconnect"]==0,"successful rejoin retained recovery window");
    }
}

void RuneAndNightmareProgression() {
    for (int base:{218,373}) {
        Reset();nextPrivateMap=base*10000+12;
        const auto dungeon=CDungeonManager::instance().Create(base),player=Player(72);
        g_registry.get<Actor>(player).mapIndex=base*10000+12;
        g_registry.get<Actor>(player).party=g_registry.create();
        collectedEntities={player};EnsureFakeTreeRegistered();
        auto login=[&]{if(base==218)CRuneDungeon::instance().OnPlayerLogin(player);else CNightmareDungeonRazor93::instance().OnPlayerLogin(player);};
        auto kill=[&](auto victim){DungeonSystem::RemoveMonster(victim);g_registry.get<Actor>(victim).dead=true;if(base==218)CRuneDungeon::instance().OnMobKilled(player,victim);else CNightmareDungeonRazor93::instance().OnMobKilled(player,victim);};
        login();auto prepare=retainedEvents.front();
        Check(DungeonSystem::GetMemberDungeon(player)==dungeon,"controller login did not attach party member");
        Check(DungeonSystem::GetFlag(dungeon,"floor")>0,"first non-leader arrival did not start controller");
        const auto n=retainedEvents.size();login();Check(retainedEvents.size()==n,"repeated login duplicated preparation");
        prepare->func(prepare,0);auto end=retainedEvents[1];
        if(base==218) {
            auto check=scheduled;check->func(check,0);
            for(size_t i=0;i<6;++i) {
                entt::entity target=entt::null;
                for(auto e:spawned) if(!g_registry.get<Actor>(e).dead && !g_registry.get<Actor>(e).invincible) {target=e;break;}
                Check(target!=entt::null,"Rune did not unlock next stone");kill(target);
            }
            Check(DungeonSystem::GetFlag(dungeon,"floor")==2,"six stones did not advance Rune");
            const auto before=State(dungeon).flags;
            const auto wrongBoss=Monster(3998);g_registry.get<Actor>(wrongBoss).mapIndex=base*10000+12;
            CRuneDungeon::instance().OnMobKilled(player,wrongBoss);
            Check(State(dungeon).flags==before,"Rune accepted another floor's boss");
        } else {
            for(int wave=0;wave<3;++wave) {
                const auto mobs=spawned;
                for(auto e:mobs) if(!g_registry.get<Actor>(e).dead) kill(e);
            }
            Check(spawned.size()==19 && ecs::PlayerRuntime::GetRaceNum(spawned.back())==4203,"Nightmare waves did not reach boss");
        }
        end->func(end,0);
    }
}

void DungeonChestDelivery() {
    for(int base:{161,179}) {
        Reset();nextPrivateMap=base*10000+15;
        const auto dungeon=CDungeonManager::instance().Create(base),player=Player(73);
        g_registry.get<Actor>(player).mapIndex=base*10000+15;
        DungeonSystem::SetFlag(dungeon,base==161?"hw22_completed":"vk_completed",1);
        auto chest=Monster(base==161?9484:9626);g_registry.get<Actor>(chest).mapIndex=base*10000+15;
        auto click=[&]{return base==161?CHalloween2022Dungeon::instance().OnClickNpc(player,chest):CVikingDungeon::instance().OnClickNpc(player,chest);};
        const char* key=base==161?"hw22_reward_73":"vk_reward_73";
        Check(click() && DungeonSystem::GetFlag(dungeon,key)==0,"failed item delivery consumed chest reward");
        itemDeliverySucceeds=true;Check(click() && rewardDeliveries==1 && DungeonSystem::GetFlag(dungeon,key)==1,"chest did not deliver reward");
        Check(click() && rewardDeliveries==1,"chest gave duplicate reward");
        g_registry.get<Actor>(chest).mapIndex++;
        Check(!click() && rewardDeliveries==1,"chest accepted another private instance");
    }
    Reset();nextPrivateMap=1610016;
    const auto dungeon=CDungeonManager::instance().Create(161),player=Player(74),boss=Monster(4584);
    g_registry.get<Actor>(player).mapIndex=1610016;g_registry.get<Actor>(boss).mapIndex=1610016;
    collectedEntities={player};EnsureFakeTreeRegistered();
    DungeonSystem::SetFlag(dungeon,"hw22_floor",2);DungeonSystem::SetFlag(dungeon,"hw22_f2_final_boss",1);
    CHalloween2022Dungeon::instance().OnMobKilled(player,boss);
    bool chest=false;for(auto e:spawned)chest|=ecs::PlayerRuntime::GetRaceNum(e)==9484;
    Check(chest,"Halloween final boss did not create reward chest");
    auto out=scheduled;out->func(out,0);
}
}

int main() {
    try {
        CreateFindDestroy();
        MembershipAndDeadEvent();
        MonsterCounting();
        UniqueMobsAndFlags();
        DestroyClearsRelations();
        PyramidEntryAndProgression();
        PyramidFirstPartyArrivalAndMissingInstance();
        PyramidRejoinCoordinatesAndFailedWarp();
        ControllerInstanceIsolation();
        PrototypeCountAndProtectedCleanup();
        DelayedControllerStartupAndCorpseCounters();
        RejoinFloorsAndRejectedTransfers();
        RuneAndNightmareProgression();
        DungeonChestDelivery();
        Reset();
        std::cout << "Dungeon lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
