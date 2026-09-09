#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/shutdown_manager.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/VIDRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/PIDRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/transform_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AISystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
int test_server = 0, passes_per_sec = 25;
uint8_t g_bChannel = 1;

namespace {
int checks = 0, destroys = 0, views = 0, resets = 0, updates = 0, creates = 0, saves = 0, frees = 0;
bool rejectItem = false;
std::function<void(entt::entity)> onView, onDestroy, onUpdate, onSave;
std::vector<int> randomBounds;
struct ActorData { int level = 50; uint8_t job = 0, group = 0; uint32_t race = 0; };
struct ItemData { uint32_t vnum, count; };
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy/live service"); }

entt::entity Actor(CHARACTER_MANAGER& manager, uint32_t id, bool pc = true) {
    const auto e = g_registry.create();
    g_registry.emplace<ActorData>(e);
    g_registry.emplace<ecs::VIDComponent>(e, id);
    g_registry.emplace<ecs::PlayerName>(e, "player" + std::to_string(id));
    g_registry.emplace<ecs::MapIndex>(e, 1);
    CVIDRegistry::Instance().Register(id, e);
    if (pc) {
        g_registry.emplace<ecs::TagPC>(e);
        g_registry.emplace<ecs::PlayerID>(e, id);
        CPIDRegistry::Instance().Register(id, e);
        manager.GetPCMap()[g_registry.get<ecs::PlayerName>(e).value] = e;
    } else g_registry.emplace<ecs::TagMonster>(e);
    return e;
}
entt::entity Item(uint32_t vnum, uint32_t count = 1) {
    const auto e = g_registry.create();
    g_registry.emplace<ItemData>(e, vnum, count);
    return e;
}
void Reset(CHARACTER_MANAGER& manager) {
    onView = onDestroy = onUpdate = onSave = {};
    manager.ClearEventData();
    manager.Destroy();
    g_registry.clear();
    destroys = views = resets = updates = creates = saves = frees = 0;
    rejectItem = false; randomBounds.clear();
}
void Event(CHARACTER_MANAGER& manager, std::initializer_list<uint8_t> types) {
    std::vector<TEventManagerData> events;
    for (const uint8_t type : types) {
        TEventManagerData ev {};
        ev.eventIndex = type; ev.eventStatus = true; ev.value[3] = 10000;
        events.push_back(ev);
    }
    manager.SetEventData(0, events);
}
void LookupChecks(CHARACTER_MANAGER& manager) {
    Reset(manager);
    const auto first = Actor(manager, 1), second = Actor(manager, 2);
    Check(manager.FindEntity(1) == first && manager.FindEntityByPID(2) == second, "native numeric indices");
    Check(manager.FindPCEntity("PLAYER1") == first && manager.FindPCEntity(nullptr) == entt::null, "native name index");
    Check(manager.Find(1) == nullptr && manager.FindPC("player1") == nullptr, "entity-only actor has no shell");
    const auto chosen = manager.FindSpecifyPC(0, 1);
    Check(chosen != entt::null && randomBounds == std::vector<int>{2}, "reservoir counts first candidate");
    Check(manager.FindSpecifyPC(0, 1, first) == second, "excluded PC");
    Check(manager.FindSpecifyPC(0, 2) == entt::null, "map filtering");
    Check(manager.FindSpecifyPC(0, 1, entt::null, 60, 100) == entt::null, "level filtering");
    g_registry.get<ActorData>(first).job = 255;
    Check(manager.FindSpecifyPC(1u << 3, 1) == second, "job shift bounds");
    g_registry.destroy(first);
    const auto recycled = Actor(manager, 3);
    Check(first != recycled && manager.FindEntity(1) == entt::null &&
        manager.FindEntityByPID(1) == entt::null && manager.FindPCEntity("player1") == entt::null, "stale generation lookup");
    manager.DestroyCharacter(first);
    Check(g_registry.valid(recycled) && CVIDRegistry::Instance().Find(1) == entt::null, "stale cleanup touched replacement");
}
void DestroyChecks(CHARACTER_MANAGER& manager) {
    Reset(manager);
    const auto a = Actor(manager, 1), b = Actor(manager, 2);
    Check(manager.BeginPendingDestroy() && !manager.BeginPendingDestroy(), "pending scope nesting");
    manager.DestroyCharacter(a); manager.DestroyCharacter(a); manager.DestroyCharacter(b);
    Check(g_registry.valid(a) && g_registry.valid(b) && destroys == 0, "deferred deletion ran early");
    onView = [&](auto e) { manager.DestroyCharacter(e); };
    onDestroy = [&](auto e) { manager.DestroyCharacter(e == a ? b : a); };
    manager.FlushPendingDestroy();
    Check(!g_registry.valid(a) && !g_registry.valid(b) && destroys == 2 && views == 2, "nested/double destruction");
    onView = onDestroy = {};
    const auto stale = Actor(manager, 10);
    manager.RegisterRaceNum(100);
    g_registry.get<ActorData>(stale).race = 100;
    manager.RegisterRaceNumMap(stale);
    manager.BeginPendingDestroy(); manager.DestroyCharacter(stale);
    g_registry.destroy(stale);
    const auto replacement = Actor(manager, 10);
    manager.FlushPendingDestroy();
    Check(g_registry.valid(replacement) && manager.FindEntity(10) == replacement, "queued recycled handle");
    std::vector<entt::entity> race;
    manager.GetCharactersByRaceNum(100, race);
    Check(race.empty(), "stale race membership");
    g_registry.destroy(replacement);
    CVIDRegistry::Instance().Register(999, entt::null);
    manager.Destroy();
    Check(CVIDRegistry::Instance().Snapshot().empty() && CPIDRegistry::Instance().Snapshot().empty(), "shutdown stale-index progress");
    const auto old = Actor(manager, 22), current = Actor(manager, 22);
    manager.DestroyCharacter(old);
    Check(manager.FindPCEntity("player22") == current && manager.FindEntity(22) == current &&
        manager.FindEntityByPID(22) == current, "old actor erased replacement index");
    g_registry.remove<ecs::PlayerName>(current);
    Check(manager.FindPCEntity("player22") == entt::null, "incomplete name component");
    manager.DestroyCharacter(current);
    Check(!g_registry.valid(current) && manager.GetPCMap().empty(), "incomplete actor teardown");
}
void SaveChecks(CHARACTER_MANAGER& manager) {
    Reset(manager);
    const auto a = Actor(manager, 1), b = Actor(manager, 2);
    for (auto e : {a, b}) {
        auto* shell = new CHARACTER;
        shell->SetEntityHandle(e);
        g_registry.emplace<ecs::LegacyCharPtr>(e, shell);
        manager.DelayedSave(e);
    }
    // Saving the first entry destroys the other entry in the same batch.
    // Destruction must still flush that entry once before freeing its shell.
    bool removedOther = false;
    onSave = [&](auto e) {
        if (!removedOther) {
            removedOther = true;
            manager.DestroyCharacter(e == a ? b : a);
        }
    };
    manager.ProcessDelayedSave();
    Check(saves == 2 && frees == 1, "nested save/destruction lost or duplicated a save");
    const auto survivor = g_registry.valid(a) ? a : b;
    onSave = [&](auto e) { manager.DelayedSave(e); };
    manager.DelayedSave(survivor); manager.ProcessDelayedSave();
    Check(saves == 3, "requeued save ran in same batch");
    onSave = {};
    Check(manager.FlushDelayedSave(survivor) && saves == 4, "save callback requeue was lost");
    Check(!manager.FlushDelayedSave(survivor), "same save flushed twice");
    manager.DelayedSave(survivor); manager.DestroyCharacter(survivor);
    Check(saves == 5 && frees == 2, "final save must precede shell teardown");
    manager.DelayedSave(survivor); manager.DelayedSave(entt::null);
    manager.ProcessDelayedSave();
    Check(saves == 5, "stale delayed save");
}
void UpdateChecks(CHARACTER_MANAGER& manager) {
    Reset(manager);
    const auto a = Actor(manager, 1), b = Actor(manager, 2);
    onUpdate = [&](auto e) {
        const auto other = e == a ? b : a;
        manager.DestroyCharacter(other);
    };
    manager.Update(PASSES_PER_SEC(5));
    Check(updates == 1 && resets == 1 && destroys == 1, "queued PC updated from stale snapshot");
    onUpdate = {};
    Reset(manager);
    const auto pc = Actor(manager, 3);
    Check(manager.BeginPendingDestroy(), "outer update scope");
    manager.DestroyCharacter(pc); manager.Update(1);
    Check(g_registry.valid(pc) && updates == 0, "update flushed outer pending scope");
    manager.FlushPendingDestroy();
    Check(!g_registry.valid(pc), "outer scope drain");
    Reset(manager);
    const auto x = Actor(manager, 4, false), y = Actor(manager, 5, false);
    manager.AddToStateList(x); manager.AddToStateList(y);
    onUpdate = [&](auto e) { manager.RemoveFromStateList(e == x ? y : x); };
    manager.Update(1);
    Check(updates == 1, "state snapshot ignored removed membership");
}
void DropChecks(CHARACTER_MANAGER& manager) {
    Reset(manager);
    const auto victim = Actor(manager, 1, false), killer = Actor(manager, 2);
    Event(manager, {DOUBLE_MISSION_BOOK_EVENT, DUNGEON_TICKET_LOOT_EVENT});
    std::vector<entt::entity> items;
    for (int i = 0; i < 128; ++i) items.push_back(Item(i % 2 ? 50300 : 71201, 7));
    items.shrink_to_fit();
    manager.CheckEventForDrop(victim, killer, items);
    Check(items.size() == 256 && creates == 128, "drop clones count / vector growth");
    Check(std::unordered_set<entt::entity>(items.begin(), items.end()).size() == items.size(), "duplicate item handles");
    for (auto e : items) Check(g_registry.get<ItemData>(e).count == 7, "cloned item count");
    creates = 0;
    const auto stale = Item(50301); g_registry.destroy(stale);
    std::vector<entt::entity> mixed{stale, entt::null, Item(50302), Item(777)};
    manager.CheckEventForDrop(victim, killer, mixed);
    Check(mixed.size() == 5 && creates == 1, "invalid/nonmatching drops");
    rejectItem = true;
    manager.CheckEventForDrop(victim, killer, mixed);
    Check(mixed.size() == 5, "failed item creation appended");
    rejectItem = false;
    Event(manager, {DOUBLE_METIN_LOOT_EVENT, DOUBLE_MISSION_BOOK_EVENT});
    g_registry.emplace<ecs::TagStone>(victim);
    std::vector<entt::entity> stacking{Item(50300)};
    manager.CheckEventForDrop(victim, killer, stacking);
    Check(stacking.size() == 4, "stacked events changed");
    manager.CheckEventForDrop(entt::null, killer, stacking);
    manager.CheckEventForDrop(victim, entt::null, stacking);
    Check(stacking.size() == 4, "invalid drop actors");
    Event(manager, {DUPLA_SZILI_EVENT});
    std::vector<entt::entity> soul{Item(30271), Item(50300), Item(71201)};
    manager.CheckEventForDrop(victim, killer, soul);
    Check(soul.size() == 4 && ItemSystem::GetItemVnum(soul.back()) == 30271, "soul event must not clone unrelated drops");
    g_registry.remove<ecs::TagStone>(victim);
    for (const auto [race, type] : {std::pair<uint32_t, uint8_t>{491, DOUBLE_BOSS_LOOT_EVENT}, {693, BUPLA_RUN_BOSS_LOOT_EVENT}}) {
        g_registry.get<ActorData>(victim).race = race;
        Event(manager, {type});
        std::vector<entt::entity> bossItems{Item(777), Item(888)};
        manager.CheckEventForDrop(victim, killer, bossItems);
        Check(bossItems.size() == 4, "boss event clone behavior");
    }
    manager.ClearEventData();
    std::vector<entt::entity> unchanged{Item(50300)};
    manager.CheckEventForDrop(victim, killer, unchanged);
    Check(unchanged.size() == 1, "inactive event cloned drops");
    manager.LoadItemShopData(entt::null, true);
    manager.LoadItemShopData(killer);
    manager.LoadItemShopBuy(entt::null, 1, 1);
    manager.LoadItemShopBuy(killer, 1, 1);
    manager.LoadItemShopBuyReal(killer, nullptr);
}
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("character-manager-error-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("character-manager-test"); return logger;
}
int number_ex(int from, int to, const char*, int) { randomBounds.push_back(to); return to; }
namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) { return g_registry.valid(e); }
bool IsPC(entt::entity e) { return IsValid(e) && g_registry.all_of<ecs::TagPC>(e); }
bool IsStone(entt::entity e) { return IsValid(e) && g_registry.all_of<ecs::TagStone>(e); }
bool IsNPC(entt::entity e) { return IsValid(e) && g_registry.all_of<ecs::TagNPC>(e); }
bool IsPet(entt::entity) { return false; }
bool IsNewPet(entt::entity) { return false; }
uint32_t GetRaceNum(entt::entity e) { return IsValid(e) ? g_registry.get<ActorData>(e).race : 0; }
uint32_t GetPacketVID(entt::entity e) { return IsValid(e) ? g_registry.get<ecs::VIDComponent>(e).value : 0; }
uint32_t GetPlayerID(entt::entity e) { const auto* p = IsValid(e) ? g_registry.try_get<ecs::PlayerID>(e) : nullptr; return p ? p->pid : 0; }
std::string_view GetName(entt::entity e) {
    const auto* name = IsValid(e) ? g_registry.try_get<ecs::PlayerName>(e) : nullptr;
    return name ? name->value : std::string_view{};
}
int32_t GetMapIndex(entt::entity e) { return IsValid(e) ? g_registry.get<ecs::MapIndex>(e).value : 0; }
uint8_t GetEmpire(entt::entity) { return 1; }
uint8_t GetJob(entt::entity e) { return g_registry.get<ActorData>(e).job; }
int32_t GetX(entt::entity) { return 0; }
int32_t GetY(entt::entity) { return 0; }
int32_t GetZ(entt::entity) { return 0; }
bool IsDungeonTicketExtraMetin(entt::entity) { return false; }
}
namespace ecs::PointSystem {
int32_t GetLevel(entt::entity e) { return g_registry.get<ActorData>(e).level; }
}
namespace SkillSystem {
uint8_t GetSkillGroup(entt::entity e) { return g_registry.get<ActorData>(e).group; }
}
namespace ecs::ViewSystem {
void ViewCleanup(entt::entity e) { ++views; if (onView) onView(e); }
}
void EntityFactory::Destroy(entt::registry& reg, entt::entity e) {
    ++destroys;
    if (onDestroy) onDestroy(e);
    CVIDRegistry::Instance().UnregisterEntity(e);
    CPIDRegistry::Instance().UnregisterEntity(e);
    reg.destroy(e);
}
namespace AISystem {
void UpdateStateMachine(entt::entity e) { ++updates; if (onUpdate) onUpdate(e); }
}
namespace CombatSystem {
void ResetChatCounter(entt::entity) { ++resets; }
}
namespace ItemSystem {
bool IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ItemData>(e); }
uint32_t GetItemVnum(entt::entity e) { return g_registry.get<ItemData>(e).vnum; }
uint32_t GetItemCount(entt::entity e) { return g_registry.get<ItemData>(e).count; }
}
ITEM_MANAGER::ITEM_MANAGER() {}
ITEM_MANAGER::~ITEM_MANAGER() {}
entt::entity ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t, bool, int, bool) {
    ++creates; return rejectItem ? entt::null : Item(vnum, count);
}
CShutdownManager::CShutdownManager() {}
CShutdownManager::~CShutdownManager() {}
void CShutdownManager::Update() {}


// Full production char_manager.cpp is linked. Unused engine/DB boundaries
// deliberately fail if a supposedly native test path starts using them.
#include "../../SRC/Server/GameServer/mob_manager.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/db.h"
#include "../../SRC/Server/GameServer/dungeon.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/questlua.h"
#include "../../SRC/Server/GameServer/ecs/AIHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"

LPCLIENT_DESC db_clientdesc = nullptr;
CMobGroup* CMobManager::GetGroup(uint32_t) { Unexpected(); }
uint32_t CMobManager::GetGroupFromGroupGroup(uint32_t) { Unexpected(); }
const CMob* CMobManager::Get(uint32_t) { Unexpected(); }
void ecs::ChatSystem::Send(entt::entity, uint8_t, const char*, ...) { Unexpected(); }
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, const char*, ...) { Unexpected(); }
void ecs::PointSystem::ApplyPoint(entt::entity, uint8_t, int32_t) { Unexpected(); }
void ecs::PointSystem::Compute(entt::entity) { Unexpected(); }
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity) { return nullptr; }
uint32_t GetDragonCoin(entt::entity) { Unexpected(); }
int GetProtectTime(entt::entity, std::string_view) { Unexpected(); }
void SetEmpire(entt::entity, uint8_t) { Unexpected(); }
void SetDungeonTicketExtraMetin(entt::entity, bool) { Unexpected(); }
void SetProto(entt::entity, const CMob*) { Unexpected(); }
void SetDungeon(entt::entity, LPDUNGEON) { Unexpected(); }
int GetPremiumRemainSeconds(entt::entity, uint8_t) { Unexpected(); }
void DestroyCharacter(entt::entity e) { CHARACTER_MANAGER::instance().DestroyCharacter(e); }
}
namespace ecs::MovementSystem {
void SendMovePacket(entt::entity, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, float) { Unexpected(); }
void SetRotation(entt::entity, float, bool) { Unexpected(); }
bool Show(entt::entity, int32_t, int32_t, int32_t, int32_t, bool) { Unexpected(); }
bool Goto(entt::entity, int32_t, int32_t) { Unexpected(); }
}
namespace CombatSystem {
void SetAggressive(entt::entity) { Unexpected(); }
void SetStone(entt::entity, entt::entity) { Unexpected(); }
void SetVictim(entt::entity, entt::entity) { Unexpected(); }
bool IsDead(entt::entity) { return false; }
}
time_t get_global_time() { return 100; }
size_t str_lower(const char* from, char* to, size_t size) {
    size_t i = 0;
    for (; i + 1 < size && from[i]; ++i) to[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(from[i])));
    if (size) to[i] = '\0';
    return i;
}
void DESC::Packet(const void*, int) { Unexpected(); }
LPSECTREE CEntity::GetSectree() const { Unexpected(); }
void BroadcastNotice(const char*, bool) { Unexpected(); }
void SendNoticeMap(const char*, int32_t, bool) { Unexpected(); }
namespace mining { bool IsVeinOfOre(uint32_t) { Unexpected(); } }
CEntity::CEntity() = default;
CEntity::~CEntity() = default;
CHorseRider::CHorseRider() = default;
CHorseRider::~CHorseRider() = default;
bool CHorseRider::ReviveHorse() { Unexpected(); }
void CHorseRider::HorseDie() { Unexpected(); }
void CHorseRider::SetHorseLevel(int) { Unexpected(); }
bool CHorseRider::StartRiding() { Unexpected(); }
bool CHorseRider::StopRiding() { Unexpected(); }
void AISystem::StateBattle(entt::entity) { Unexpected(); }
void AISystem::StateIdle(entt::entity) { Unexpected(); }
void CHARACTER::SetHorseLevel(int) { Unexpected(); }
bool CHARACTER::StartRiding() { Unexpected(); }
bool CHARACTER::StopRiding() { Unexpected(); }
uint32_t CHARACTER::GetMyHorseVnum() const { Unexpected(); }
void CHARACTER::HorseDie() { Unexpected(); }
bool CHARACTER::ReviveHorse() { Unexpected(); }
void CHARACTER::SendHorseInfo() { Unexpected(); }
void CHARACTER::ClearHorseInfo() { Unexpected(); }
void intrusive_ptr_release(event*) { Unexpected(); }
const DESC_MANAGER::DESC_SET& DESC_MANAGER::GetClientSet() { Unexpected(); }
CHARACTER::CHARACTER() = default;
CHARACTER::~CHARACTER() {
    Check(g_registry.valid(GetEntityHandle()), "shell destroyed after its ECS state");
    EntityFactory::Destroy(g_registry, GetEntityHandle()); ++frees;
}
void CHARACTER::Create(const char*, uint32_t, bool) { Unexpected(); }
void CHARACTER::Disconnect(const char*) { Unexpected(); }
void CHARACTER::SetProto(const CMob*) { Unexpected(); }
void CHARACTER::SaveReal() { ++saves; if (onSave) onSave(GetEntityHandle()); }
uint32_t CHARACTER::GetLegacyVID() const { Unexpected(); }
void CHARACTER::ComputePoints() { Unexpected(); }
LPCHARACTER CHARACTER::GetRider() const { return nullptr; }
void CHARACTER::SetEmpire(uint8_t) { Unexpected(); }
void CHARACTER::SetRegen(LPREGEN) { Unexpected(); }
TEMP_BUFFER::TEMP_BUFFER(int, bool) { Unexpected(); }
TEMP_BUFFER::~TEMP_BUFFER() = default;
const void* TEMP_BUFFER::read_peek() { Unexpected(); }
void TEMP_BUFFER::write(const void*, int) { Unexpected(); }
int TEMP_BUFFER::size() { Unexpected(); }
LPPARTY CPartyManager::CreateParty(entt::entity) { Unexpected(); }
void CParty::Link(entt::entity) { Unexpected(); }
uint32_t SECTREE::GetAttribute(int32_t, int32_t) { Unexpected(); }
LPENTITY SectreeLegacyEntity(entt::entity) { Unexpected(); }
bool SectreeMember(entt::entity, const SECTREE*) { Unexpected(); }
void SECTREE::Collect(FCollectEntity&) const { Unexpected(); }
LPSECTREE SECTREE_MAP::Find(uint32_t, uint32_t) { Unexpected(); }
LPSECTREE_MAP SECTREE_MANAGER::GetMap(int32_t) { Unexpected(); }
LPSECTREE SECTREE_MANAGER::Get(int32_t, int32_t, int32_t) { Unexpected(); }
bool SECTREE_MANAGER::GetMapBasePositionByMapIndex(int32_t, PIXEL_POSITION&) { Unexpected(); }
bool SECTREE_MANAGER::GetMovablePosition(int32_t, int32_t, int32_t, PIXEL_POSITION&) { Unexpected(); }
uint8_t SECTREE_MANAGER::GetEmpireFromMapIndex(int32_t) { Unexpected(); }
void CDungeon::DeadCharacter(LPCHARACTER) { Unexpected(); }
LPDUNGEON CDungeonManager::FindByMapIndex(int32_t) { Unexpected(); }
void DBManager::SendMoneyLog(uint8_t, uint32_t, int64_t) { Unexpected(); }
bool map_allow_find(int32_t) { Unexpected(); }
int quest::CQuestManager::GetEventFlag(const std::string&) { Unexpected(); }
void quest::FSendPacket::operator()(LPENTITY) { Unexpected(); }
entt::entity EntityFactory::EnsureLegacyCharacterEntity(entt::registry&, LPCHARACTER, uint32_t) { Unexpected(); }
entt::entity EntityFactory::CreateMonster(entt::registry&, const TMobTable&, int32_t, int32_t, int32_t, uint32_t) { Unexpected(); }
entt::entity EntityFactory::CreateNPC(entt::registry&, const TMobTable&, int32_t, int32_t, int32_t, uint32_t) { Unexpected(); }
entt::entity EntityFactory::CreateStone(entt::registry&, const TMobTable&, int32_t, int32_t, int32_t, uint32_t) { Unexpected(); }
LPDUNGEON ecs::SocialSystem::GetDungeon(entt::entity) { return nullptr; }
void ItemSystem::AutoGiveItem(entt::entity, entt::entity, bool, bool) { Unexpected(); }
const TItemTable* ItemSystem::GetItemProto(entt::entity) { Unexpected(); }
bool ItemSystem::SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected(); }
bool ItemSystem::SetItemForceAttributeEcs(entt::entity, int, uint8_t, int16_t) { Unexpected(); }
void CLIENT_DESC::DBPacketHeader(uint8_t, uint32_t, uint32_t) { Unexpected(); }
void CLIENT_DESC::Packet(const void*, int) { Unexpected(); }


int main() {
    try {
        CHARACTER_MANAGER manager;
        ITEM_MANAGER items;
        CShutdownManager shutdown;
        LookupChecks(manager); DestroyChecks(manager); SaveChecks(manager); UpdateChecks(manager); DropChecks(manager);
        Reset(manager);
        std::cout << "Character manager checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
