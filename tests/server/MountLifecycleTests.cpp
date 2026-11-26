#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/pet/PetSystem.h"
#include "../../SRC/Server/GameServer/network/db.h"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/entity/char_manager.h"
#include "../../SRC/Server/GameServer/network/desc.h"
#include "../../SRC/Server/GameServer/core/constants.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/components/pet_mount_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountFlightSystem.hpp"
#include <iostream>
#include <stdexcept>
#include <functional>
#include <regex>
#include <Core/Logging.hpp>

std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("growth-pet-test");
    return logger;
}

std::shared_ptr<spdlog::logger> logging::GetLogger() {
    return logging::GetErrorLogger();
}

// Compile the complete production typed PetSystem.cpp. External
// services are doubled; no CHARACTER/CItem is created or attached to the
// entity fixtures. The costume mount runtime is covered by the GameServer
// integration path instead of this harness.
entt::registry g_registry;
entt::dispatcher g_dispatcher;
int passes_per_sec = 25;
const TApplyInfo aApplyInfo[MAX_APPLY_NUM] = {};

namespace {
struct CharacterState {
    int32_t x = 1000, y = 1000, z = 0, map = 1;
    float rotation = 0;
    uint32_t vid = 0, mount = 0;
    uint32_t exp = 0;
    uint8_t level = 1;
    uint8_t empire = 1;
    bool dead = false, war = false, dungeon = false, hasMob = true, walking = false, affectsLoaded = true;
    int64_t petBonus = 0;
    entt::entity wear = entt::null, skin = entt::null;
    std::string name = "owner";
};
struct ItemState {
    entt::entity owner = entt::null;
    uint32_t id = 0, vid = 0, vnum = 71100;
    bool unlimited = true, hasProto = true, locked = false;
    int attributeBonus = 0;
    uint32_t count = 1;
    uint16_t cell = 0;
    bool equipped = false;
    int64_t price = 100;
    std::array<int16_t, ITEM_ATTRIBUTE_MAX_NUM> attributes {};
    TItemTable proto {};
    int64_t sockets[ITEM_SOCKET_MAX_NUM] {};
};
int checks = 0, destroyed = 0, spawned = 0, shown = 0, moves = 0, packets = 0;
int affects = 0, cancels = 0, lastDuration = 0, liveEvents = 0;
uint32_t tick = 1000, nextVID = 1, spawnedVnum = 0;
bool failSpawn = false, failShow = false;
int computes = 0, modifications = 0, mobQueries = 0, clearCalls = 0, lastClearValue = 0;
std::function<void()> onCompute;
LPEVENT scheduled;
std::vector<LPEVENT> retainedEvents;
std::vector<std::string> databaseRow;
std::vector<std::string> queries;
SqlParams lastParams;
bool failQuery = false, failPayment = false;
int nullColumn = -1, databaseRows = 1, liveResults = 0, consumed = 0;
uint32_t nowSeconds = 2000000;
int questDelay = 0;
std::set<entt::entity> growthBonusOwners;
CAffect bonus {};
std::set<entt::entity> bonusOwners;

void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
CharacterState& State(entt::entity e)
{
    Check(g_registry.valid(e) && g_registry.all_of<CharacterState>(e), "service received stale/non-character entity");
    return g_registry.get<CharacterState>(e);
}
entt::entity Character()
{
    const auto e = g_registry.create();
    g_registry.emplace<CharacterState>(e).vid = nextVID++;
    return e;
}
entt::entity Item(entt::entity owner)
{
    const auto e = g_registry.create();
    auto& item = g_registry.emplace<ItemState>(e);
    item.owner = owner;
    item.id = item.vid = nextVID++;
    item.cell = static_cast<uint16_t>(item.id % INVENTORY_MAX_NUM);
    State(owner).wear = e;
    return e;
}
void Reset()
{
    scheduled.reset();
    retainedEvents.clear();
    Check(liveEvents == 0, "event leaked");
    Check(liveResults == 0, "SQL result leaked");
    g_registry.clear();
    bonusOwners.clear();
    destroyed = spawned = shown = moves = packets = affects = cancels = 0;
    failSpawn = failShow = false;
    computes = modifications = mobQueries = clearCalls = lastClearValue = 0;
    onCompute = {};
    failQuery = failPayment = false;
    nullColumn = -1; databaseRows = 1; consumed = questDelay = 0;
    queries.clear(); lastParams.clear(); growthBonusOwners.clear();
    databaseRow = {"grown pet","1","0","0","10","20","30","0","0","0","0","-1","0","-1","0","120","200","0","1","1000000"};
}
}

// Database/SQL client doubles never connect or start worker threads.
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() : m_queueLane(0) {}
CAsyncSQL::~CAsyncSQL() = default;
DBManager::DBManager() = default;
DBManager::~DBManager() = default;
void DESC::Packet(const void*, int) {}
namespace {
struct FakeResult {
    std::vector<std::string> columns;
    std::vector<char*> row;
    explicit FakeResult(const std::vector<std::string>& values) : columns(values) {
        for (auto& column : columns) row.push_back(column.data());
        if (nullColumn >= 0 && nullColumn < row.size()) row[nullColumn] = nullptr;
    }
};
}
extern "C" MYSQL_ROW STDCALL mysql_fetch_row(MYSQL_RES* result) { return reinterpret_cast<FakeResult*>(result)->row.data(); }
extern "C" unsigned int STDCALL mysql_num_fields(MYSQL_RES* result) { return static_cast<unsigned int>(reinterpret_cast<FakeResult*>(result)->row.size()); }
extern "C" void STDCALL mysql_free_result(MYSQL_RES* result) { --liveResults; delete reinterpret_cast<FakeResult*>(result); }
MYSQL_ROW SQLResult::FetchRow() { return pSQLResult ? mysql_fetch_row(pSQLResult) : nullptr; }
// The rename binds its values; the statement and its parameters are recorded.
SQLMsg* DBManager::DirectQueryPrepared(const char* query, const SqlParams& params) {
    queries.emplace_back(query);
    lastParams = params;
    if (failQuery) return nullptr;
    auto message = std::make_unique<SQLMsg>();
    auto result = std::make_unique<SQLResult>();
    result->bPrepared = true;
    result->uiAffectedRows = 1;
    message->vec_pkResult.push_back(result.release());
    return message.release();
}
SQLMsg* DBManager::DirectQuery(const char* format, ...) {
    char query[4096];
    va_list args; va_start(args, format); vsnprintf(query, sizeof(query), format, args); va_end(args);
    queries.emplace_back(query);
    if (failQuery) return nullptr;
    if (queries.back().starts_with("UPDATE new_petsystem SET")) {
        static const std::array<std::string, 20> names = {"name","level","exp","expi","bonus0","bonus1","bonus2",
            "skill0","skill0lv","skill1","skill1lv","skill2","skill2lv","skill3","skill3lv","duration","tduration",
            "evolution","evocation","minAge"};
        for (size_t i = 1; i < names.size(); ++i) {
            const std::regex assignment("(?:SET |,)" + names[i] + "=(-?[0-9]+)");
            std::smatch match;
            if (std::regex_search(queries.back(), match, assignment) && i < databaseRow.size())
                databaseRow[i] = match[1].str();
        }
    }
    auto message = std::make_unique<SQLMsg>();
    auto result = std::make_unique<SQLResult>();
    if (queries.back().starts_with("SELECT")) {
        auto rows = std::make_unique<FakeResult>(databaseRow);
        result->uiNumRows = databaseRows;
        result->pSQLResult = reinterpret_cast<MYSQL_RES*>(rows.release());
        ++liveResults;
    }
    result->uiAffectedRows = 1;
    message->vec_pkResult.push_back(result.release());
    return message.release();
}
uint32_t DBManager::EscapeString(char* dst, uint64_t capacity, const char* src, uint32_t size) {
    uint32_t used = 0;
    for (uint32_t i = 0; i < size; ++i) {
        Check(used + 2 < capacity, "escape buffer too small");
        if (src[i] == '\'' || src[i] == '\\') dst[used++] = '\\';
        dst[used++] = src[i];
    }
    dst[used] = 0;
    return used;
}

CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
entt::entity CHARACTER_MANAGER::SpawnMobEntity(uint32_t vnum, int32_t map, int32_t x, int32_t y,
    int32_t z, bool, int rotation, bool)
{
    ++spawned;
    spawnedVnum = vnum;
    if (failSpawn)
        return entt::null;
    const auto e = Character();
    auto& state = State(e);
    state.map = map; state.x = x; state.y = y; state.z = z; state.rotation = rotation;
    return e;
}
void intrusive_ptr_add_ref(EVENT* e) { ++e->ref_count; }
void intrusive_ptr_release(EVENT* e) { if (--e->ref_count == 0) { --liveEvents; delete e; } }
LPEVENT event_create_ex(TEVENTFUNC func, event_info_data* info, int32_t)
{
    LPEVENT result(new EVENT);
    ++liveEvents;
    result->func = func; result->info = info;
    scheduled = result;
    retainedEvents.push_back(result);
    return result;
}
void event_cancel(LPEVENT* event)
{
    if (*event) { ++cancels; (*event)->is_force_to_end = true; }
    event->reset();
}
uint32_t get_dword_time() { return tick; }
time_t get_global_time() { return nowSeconds; }
int number_ex(int from, int, const char*, int) { return from; }
float GetDegreeFromPositionXY(int32_t, int32_t, int32_t, int32_t) { return 0.f; }
void GetDeltaByDegree(float, float distance, float* x, float* y) { *x = distance; *y = 0.f; }

namespace ecs::PlayerRuntime {
bool IsPC(entt::entity e){return IsValid(e);}
bool IsObserverMode(entt::entity){return false;}
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
LPDESC GetDesc(entt::entity) { return nullptr; }
uint32_t GetPacketVID(entt::entity e) { return State(e).vid; }
std::string_view GetName(entt::entity e) { return State(e).name; }
void SetName(entt::entity e, std::string_view name) { State(e).name = name; }
int32_t GetX(entt::entity e) { return State(e).x; }
int32_t GetY(entt::entity e) { return State(e).y; }
int32_t GetZ(entt::entity e) { return State(e).z; }
int32_t GetMapIndex(entt::entity e) { return State(e).map; }
float GetRotation(entt::entity e) { return State(e).rotation; }
uint8_t GetEmpire(entt::entity e) { return State(e).empire; }
void SetEmpire(entt::entity e, uint8_t empire) { State(e).empire = empire; }
void SetLevel(entt::entity e, uint8_t level) { State(e).level = level; }
void SetExp(entt::entity e, uint32_t exp) { State(e).exp = exp; }
const TMobTable* GetMobTable(entt::entity e) { ++mobQueries; static TMobTable table{}; return State(e).hasMob ? &table : nullptr; }
void DestroyCharacter(entt::entity e) { State(e); ++destroyed; g_registry.destroy(e); }
}
namespace ItemSystem {
bool IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<ItemState>(e); }
entt::entity GetItemOwner(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).owner : entt::null; }
uint32_t GetItemID(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).id : 0; }
uint32_t GetItemVID(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).vid : 0; }
uint32_t GetItemVnum(entt::entity e) { return IsValidItem(e) ? g_registry.get<ItemState>(e).vnum : 0; }
const TItemTable* GetItemProto(entt::entity e) { const auto& item = g_registry.get<ItemState>(e); return item.hasProto ? &item.proto : nullptr; }
bool IsUnlimitedTimeUnique(entt::entity e) { return g_registry.get<ItemState>(e).unlimited; }
uint32_t GetItemSocket(entt::entity e, int i) { return static_cast<uint32_t>(g_registry.get<ItemState>(e).sockets[i]); }
bool SetItemSocket(entt::entity e, int i, uint32_t value, bool) { g_registry.get<ItemState>(e).sockets[i] = value; return true; }
int32_t GetItemValue(entt::entity e, uint32_t i) { return g_registry.get<ItemState>(e).proto.alValues[i]; }
bool LockItem(entt::entity e, bool locked) { Check(IsValidItem(e), "locking stale item"); g_registry.get<ItemState>(e).locked = locked; return true; }
bool UnlockItem(entt::entity e) { return LockItem(e, false); }
bool IsItemLocked(entt::entity e) { Check(IsValidItem(e), "locking invalid item"); return g_registry.get<ItemState>(e).locked; }
bool IsItemEquipped(entt::entity e) { return g_registry.get<ItemState>(e).equipped; }
uint32_t GetItemCount(entt::entity e) { return g_registry.get<ItemState>(e).count; }
uint8_t GetItemType(entt::entity e) { return g_registry.get<ItemState>(e).proto.bType; }
int64_t GetItemShopBuyPrice(entt::entity e) { return g_registry.get<ItemState>(e).price; }
bool ConsumeItemEcs(entt::entity e, uint32_t count) {
    if (failPayment || !IsValidItem(e) || GetItemCount(e) < count) return false;
    ++consumed;
    if ((g_registry.get<ItemState>(e).count -= count) == 0) g_registry.destroy(e);
    return true;
}
bool DestroyItemEntityEcs(entt::entity e, const char*) {
    if (failPayment || !IsValidItem(e)) return false;
    g_registry.destroy(e);
    return true;
}
entt::entity GetInventoryItem(entt::entity owner, uint16_t cell) {
    State(owner);
    for (auto [e, item] : g_registry.view<ItemState>().each())
        if (item.owner == owner && item.cell == cell) return e;
    return entt::null;
}
entt::entity GetItem(entt::entity owner, TItemPos cell) { return GetInventoryItem(owner, cell.cell); }
bool SetItemForceAttributeEcs(entt::entity e, int index, uint8_t, double value) {
    Check(IsValidItem(e) && index >= 0 && index < ITEM_ATTRIBUTE_MAX_NUM, "invalid pet attribute write");
    g_registry.get<ItemState>(e).attributes[index] = value;
    return true;
}
void ModifyPoints(entt::entity e, bool add) {
    Check(IsValidItem(e) && add, "pet buff used invalid item/removal path");
    const auto& item = g_registry.get<ItemState>(e);
    ++modifications;
    for (const auto& apply : item.proto.aApplies)
        if (apply.bType != APPLY_NONE) State(item.owner).petBonus += apply.lValue;
    State(item.owner).petBonus += item.attributeBonus;
}
entt::entity GetWearItem(entt::entity owner, uint8_t slot) {
    return slot == WEAR_COSTUME_MOUNT ? State(owner).wear : State(owner).skin;
}
}
namespace MountSystem {
bool IsHorseRiding(entt::entity e) { State(e); return false; }
bool StopRiding(entt::entity) { throw std::runtime_error("unexpected horse riding"); }
entt::entity GetSummonedHorse(entt::entity e) { State(e); return entt::null; }
void SummonHorse(entt::entity, bool, bool, uint32_t, const char*) { throw std::runtime_error("unexpected horse summon"); }
uint32_t GetMountVnum(entt::entity e) { return State(e).mount; }
void SetMountVnum(entt::entity e, uint32_t vnum) { State(e).mount = vnum; }
}
std::optional<bool> TypedMountFlightPermission(entt::entity) { return std::nullopt; }
void ReplayTypedCostumeAppearances(entt::entity) {}

namespace CombatSystem {
bool IsDead(entt::entity e) { return State(e).dead; }
void SetLastAttacked(entt::entity e, uint32_t) { State(e); }
}
namespace ecs::MovementSystem {
bool CanMove(entt::entity e){return !State(e).dead;}
bool Show(entt::entity e, int32_t map, int32_t x, int32_t y, int32_t z, bool) {
    ++shown;
    auto& s = State(e);
    if (const auto* level = g_registry.try_get<ecs::TypedPetLevel>(e))
        Check(level->value == 41, "initial typed growth insert lacks the authoritative level");
    if (failShow) return false;
    s.map = map; s.x = x; s.y = y; s.z = z;
    return true;
}
void Stop(entt::entity e) { State(e); }
void SendMovePacket(entt::entity e, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t, uint32_t, float) { State(e); ++packets; }
bool Goto(entt::entity e, int32_t, int32_t) { State(e); ++moves; return true; }
void SetRotation(entt::entity e, float r
#ifdef ENABLE_ANCIENT_PYRAMID
    , bool
#endif
) { State(e).rotation = r; }
void SyncWalkingWrite(entt::entity e, bool) { State(e); }
void SetNowWalking(entt::entity e, bool walking) { State(e).walking = walking; }
}
bool AffectSystem::IsLoaded(entt::entity e) { return State(e).affectsLoaded; }
CAffect* AffectSystem::FindAffect(entt::entity e, uint32_t, uint8_t) { return bonusOwners.contains(e) ? &bonus : nullptr; }
bool AffectSystem::AddAffect(entt::entity e, uint32_t type, uint8_t, double value, uint32_t,
    int32_t duration, int32_t, bool, bool)
{
    State(e); ++affects; lastDuration = duration;
    if (type == AFFECT_MOUNT) State(e).mount = value;
    if (type == AFFECT_MOUNT_BONUS) bonusOwners.insert(e);
    if (type == AFFECT_NEW_PET) growthBonusOwners.insert(e);
    return true;
}
bool AffectSystem::RemoveAffect(entt::entity e, uint32_t type) {
    State(e);
    if (type == AFFECT_MOUNT_BONUS) bonusOwners.erase(e);
    if (type == AFFECT_MOUNT) State(e).mount = 0;
    if (type == AFFECT_NEW_PET) growthBonusOwners.erase(e);
    return true;
}
void ecs::PointSystem::Change(entt::entity e, uint8_t, int64_t, bool, bool
#ifdef __ENABLE_BLOCK_EXP__
    , bool
#endif
) { State(e); }
void ecs::PointSystem::Compute(entt::entity e) {
    State(e).petBonus = 0;
    ++computes;
    if (onCompute) onCompute();
}
void ecs::PointSystem::ApplyPoint(entt::entity e, uint8_t, double value) {
    State(e).petBonus += value;
    ++clearCalls;
    lastClearValue = value;
}
CWarMap* ecs::SocialSystem::GetWarMap(entt::entity e) {
    static int marker;
    return State(e).war ? reinterpret_cast<CWarMap*>(&marker) : nullptr;
}
entt::entity ecs::SocialSystem::GetDungeon(entt::entity e) {
    // A live stand-in handle; the pet code only compares it against null.
    return State(e).dungeon ? static_cast<entt::entity>(0x1234u) : entt::null;
}
bool ecs::SocialSystem::HasExchange(entt::entity e) { State(e); return false; }
entt::entity ecs::SocialSystem::GetMyShop(entt::entity e) { State(e); return entt::null; }
entt::entity ecs::SocialSystem::GetShopOwner(entt::entity e) { State(e); return entt::null; }
int64_t ecs::QuestSystem::GetFlag(entt::entity e, std::string_view) { State(e); return questDelay; }
void ecs::QuestSystem::SetFlag(entt::entity e, std::string_view, int64_t time) { State(e); questDelay = time; }
void ecs::ViewSystem::PacketView(entt::entity e, const void*, int, entt::entity) { State(e); ++packets; }
void ecs::ViewSystem::ViewReencode(entt::entity e) { State(e); ++packets; }
void ecs::ChatSystem::Send(entt::entity e, uint8_t, const char*, ...) { State(e); }
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity e, uint8_t, uint32_t, const char*, ...) { State(e); }
#endif

namespace {
void TypedFollowerLifecycle()
{
    Reset();
    const auto owner = Character();
    const auto other = Character();
    const auto seal = UINT64_C(0x10000000017);
    PetSystem::SetUpdatePeriod(owner, 0);
    Check(PetSystem::SummonTyped(owner, 32500, seal, false), "typed pet spawn failed");
    const auto pet = PetSystem::FindActor(owner, 32500)->character;
    Check(g_registry.get<ecs::StatusFlags>(pet).isPet && PetSystem::LiveTypedSeal(owner) == seal,
        "typed pet lost identity or pet presentation");
    Check(!PetSystem::SummonTyped(owner, 32000, seal + 1, true), "second typed follower was admitted");
    PetSystem::RecallTyped(other, seal);
    PetSystem::RecallTyped(owner, seal + 1);
    State(pet).x += 1000;
    Check(PetSystem::Update(owner, 0) && moves == 1 && PetSystem::LiveTypedSeal(owner) == seal,
        "typed follower required a legacy item or accepted an unrelated recall");
    PetSystem::RecallTyped(owner, seal);
    Check(!g_registry.valid(pet) && !PetSystem::LiveTypedSeal(owner) && !modifications,
        "typed pet recall failed or applied legacy item bonuses");
    Check(PetSystem::SummonTyped(owner, 32000, seal + 1, true), "typed mount follower spawn failed");
    const auto mount = PetSystem::FindActor(owner, 32000)->character;
    Check(g_registry.get<ecs::StatusFlags>(mount).isMount && !g_registry.get<ecs::StatusFlags>(mount).isPet,
        "typed mount used the pet presentation");
    PetSystem::DestroyRuntime(owner);
    Check(!g_registry.valid(mount) && !PetSystem::LiveTypedSeal(owner), "logout left a typed follower in the world");
}

void TypedFollowerFailuresAndReplacement()
{
    Reset();
    const auto owner = Character();
    constexpr auto seal = UINT64_C(0x10000000021);
    Check(!PetSystem::SummonTyped(entt::null, 34001, seal, false), "null owner admitted");
    Check(!PetSystem::SummonTyped(owner, 34001, 0, false), "missing typed seal admitted");
    Check(!PetSystem::SummonTyped(owner, 0, seal, false), "missing follower form admitted");
    failSpawn = true;
    Check(!PetSystem::SummonTyped(owner, 34001, seal, false) && !scheduled,
        "failed typed spawn started a timer");
    failSpawn = false;
    failShow = true;
    Check(!PetSystem::SummonTyped(owner, 34001, seal, false) && destroyed == 1 &&
        !PetSystem::CountSummoned(owner) && !PetSystem::LiveTypedSeal(owner) && !scheduled,
        "failed typed show leaked a follower, seal or timer");
    failShow = false;
    Check(PetSystem::SummonTyped(owner, 34001, seal, false, "Named pet"), "typed retry failed");
    const auto pet = PetSystem::FindActor(owner, 34001)->character;
    Check(State(pet).name == "Named pet" && !modifications && !computes,
        "typed summon used legacy item bonuses or lost its name");
    const auto firstTimer = scheduled;
    PetSystem::DestroyRuntime(owner);
    PetSystem::DestroyRuntime(owner);
    Check(!g_registry.valid(pet) && firstTimer->func(firstTimer, 0) == 0,
        "typed teardown was not idempotent");
    Check(PetSystem::SummonTyped(owner, 34002, seal + 1, false), "replacement typed runtime failed");
    Check(firstTimer->func(firstTimer, 0) == 0 && scheduled->func(scheduled, 0) != 0,
        "old timer entered replacement typed runtime");
    PetSystem::DestroyRuntime(owner);
}

void TypedFollowerFollowingAndDeath()
{
    Reset();
    const auto owner = Character();
    constexpr auto seal = UINT64_C(0x10000000031);
    PetSystem::SetUpdatePeriod(owner, 0);
    Check(PetSystem::SummonTyped(owner, 34001, seal, false), "typed follow fixture failed");
    auto* actor = PetSystem::FindActor(owner, 34001);
    const auto pet = actor->character;
    State(pet).x = State(owner).x + 500;
    Check(PetSystem::Update(owner, 0) && State(pet).walking && moves == 1,
        "near typed follower did not walk");
    State(pet).x = State(owner).x + 1500;
    Check(PetSystem::Update(owner, 0) && !State(pet).walking && moves == 2,
        "far typed follower did not run");
    State(pet).map = 2;
    Check(PetSystem::Update(owner, 0) && State(pet).map == State(owner).map,
        "typed follower did not follow the map change");
    State(owner).dead = true;
    Check(PetSystem::Update(owner, 0) && PetSystem::LiveTypedSeal(owner) == seal,
        "owner death changed the typed follower survival rule");
    State(owner).dead = false;
    actor->options = 0;
    const auto queries = mobQueries;
    Check(PetSystem::Update(owner, 0) && mobQueries == queries,
        "disabled follow option still ran movement");
    PetSystem::SetUpdatePeriod(owner, 1000);
    State(pet).dead = true;
    Check(PetSystem::Update(owner, 0) && g_registry.valid(pet), "typed update period ignored");
    tick += 1000;
    Check(PetSystem::Update(owner, 0) && !g_registry.valid(pet) && !PetSystem::LiveTypedSeal(owner),
        "dead typed follower retained the creature or seal");
    Check(PetSystem::SummonTyped(owner, 34001, seal, false), "typed follower could not respawn");
    PetSystem::DestroyRuntime(owner);
}

void TypedFollowerStaleEntities()
{
    Reset();
    const auto owner = Character();
    constexpr auto seal = UINT64_C(0x10000000041);
    PetSystem::SetUpdatePeriod(owner, 0);
    Check(PetSystem::SummonTyped(owner, 34001, seal, false), "typed stale fixture failed");
    const auto pet = PetSystem::FindActor(owner, 34001)->character;
    g_registry.destroy(pet);
    const auto replacement = Character();
    Check(pet != replacement && entt::to_entity(pet) == entt::to_entity(replacement),
        "typed creature generation was not recycled");
    Check(PetSystem::Update(owner, 0) && g_registry.valid(replacement) &&
        !PetSystem::LiveTypedSeal(owner), "stale follower destroyed a recycled creature");
    Check(PetSystem::SummonTyped(owner, 34002, seal + 1, false), "typed respawn after stale creature failed");
    const auto timer = scheduled;
    PetSystem::DestroyRuntime(owner);
    g_registry.destroy(owner);
    const auto newOwner = Character();
    Check(newOwner != owner && entt::to_entity(newOwner) == entt::to_entity(owner),
        "typed owner generation was not recycled");
    Check(timer->func(timer, 0) == 0 && !g_registry.try_get<ecs::PetRuntime>(newOwner),
        "stale pet timer entered a recycled owner");
}

} // namespace

void MountFlightChecks()
{
    auto previousTick=tick;Reset();auto owner=Character();auto item=Item(owner);State(owner).mount=32155;
    auto& proto=g_registry.get<ItemState>(item).proto;
    Check(!ecs::MountFlightSystem::CanFly(owner),"unflagged mount can fly");
    TPacketCGMountFlight request{};request.header=HEADER_CG_MOUNT_FLIGHT;
    ecs::MountFlightSystem::Request(owner,request);
    Check(g_registry.all_of<ecs::MountFlightSystem::Supported>(owner),"flight hello was not registered");
    request.action=1;request.altitude=10000;ecs::MountFlightSystem::Request(owner,request);
    Check(!g_registry.get<ecs::MountFlightSystem::State>(owner).active,"unflagged takeoff accepted");
    proto.alValues[5]=1;proto.alValues[1]=32154;
    Check(!ecs::MountFlightSystem::CanFly(owner),"another mount borrowed the worn seal's capability");
    proto.alValues[1]=32155;State(owner).mount=0;
    Check(!ecs::MountFlightSystem::CanFly(owner),"unmounted player can fly");
    State(owner).mount=32155;
    State(owner).affectsLoaded=false;
    ecs::MountFlightSystem::Request(owner,request);
    Check(!ecs::MountFlightSystem::CanFly(owner)&&!g_registry.get<ecs::MountFlightSystem::State>(owner).active,
        "takeoff was accepted before login finished restoring mount affects");
    State(owner).affectsLoaded=true;
    Check(ecs::MountFlightSystem::CanFly(owner),"flagged worn mount rejected");
    tick=1000;ecs::MountFlightSystem::Request(owner,request);
    auto& flight=g_registry.get<ecs::MountFlightSystem::State>(owner);
    Check(flight.active&&flight.altitude==10000,"valid takeoff failed");
    tick+=100;request.altitude=10060;ecs::MountFlightSystem::Request(owner,request);
    Check(flight.altitude==10060,"6m/s ascent failed");
    tick+=100;request.action=3;request.altitude=10120;ecs::MountFlightSystem::Request(owner,request);
    Check(!flight.fast && flight.altitude==10060,"unnegotiated fast flight accepted");
    request.action=0;request.time=1;ecs::MountFlightSystem::Request(owner,request);
    request.action=3;ecs::MountFlightSystem::Request(owner,request);
    Check(flight.active && flight.fast && flight.altitude==10120,"fast flight did not retain the altitude limit");
    request.action=1;ecs::MountFlightSystem::Request(owner,request);
    Check(flight.active && !flight.fast,"fast flight flag did not clear on release");
    request.altitude=20000;ecs::MountFlightSystem::Request(owner,request);
    Check(flight.altitude==10120,"vertical teleport accepted");
    const auto beforeSpam = flight.altitude;
    for(int i=0;i<20;i++){request.altitude=flight.altitude+30;ecs::MountFlightSystem::Request(owner,request);}
    Check(flight.altitude<=beforeSpam+200,"packet spam manufactured vertical speed budget");
    const auto hover=flight.altitude;State(owner).x+=5000;State(owner).y+=5000;
    Check(flight.altitude==hover,"horizontal movement changed absolute altitude");
    tick+=1000;request.action=2;request.altitude=flight.altitude-100;ecs::MountFlightSystem::Request(owner,request);
    Check(!flight.active,"landing did not end flight");
    auto landed=flight.altitude;request.action=1;request.altitude=landed+10000;
    ecs::MountFlightSystem::Request(owner,request);
    Check(!flight.active&&flight.altitude==landed,"landing/takeoff reset the vertical speed budget");
    State(owner).x += 300;
    request.altitude = landed + 1000;
    ecs::MountFlightSystem::Request(owner, request);
    Check(flight.active && flight.altitude == landed + 1000, "ground travel did not allow takeoff from a new terrain height");
    request.action=1;ecs::MountFlightSystem::Request(owner,request);State(owner).dead=true;
    ecs::MountFlightSystem::Request(owner,request);Check(!flight.active,"dead actor retained flight");
    State(owner).dead=false;State(owner).map=2;
    ecs::MountFlightSystem::Request(owner,request);Check(!flight.active,"old-map flight carried across warp");
    request.action=0;ecs::MountFlightSystem::Request(owner,request);
    request.action=1;request.altitude=8000;ecs::MountFlightSystem::Request(owner,request);
    Check(flight.active&&flight.map==2,"new-map hello did not restore flight capability");
    ecs::MountFlightSystem::Reset(owner);Check(!flight.active&&flight.ready,"mount reset lost the protocol handshake");
    Check(HEADER_CG_MOUNT_FLIGHT==221&&HEADER_GC_MOUNT_FLIGHT==221,"flight packet ID changed");
    Check(HEADER_CG_TYPED_ITEM_MOVE==225&&HEADER_GC_TYPED_INVENTORY==225,"flight reused the typed item packet IDs");
    proto.alValues[5]=0;Check(!ecs::MountFlightSystem::CanFly(owner),"revoked proto flag retained permission");
    Reset();tick=previousTick;
}

int main()
{
    try {
        CHARACTER_MANAGER factory;
        DBManager database;
        MountFlightChecks();
        Reset();
        const auto growthOwner = Character();
        Check(PetSystem::SummonTyped(growthOwner, 34041, UINT64_C(0x10000000051), false, "Growth", 41),
            "typed growth follower failed");
        const auto growth = PetSystem::FindActor(growthOwner, 34041)->character;
        Check(g_registry.get<ecs::TypedPetLevel>(growth).value == 41 &&
            g_registry.get<ecs::StatusFlags>(growth).isPet, "typed growth identity/level mismatch");
        PetSystem::DestroyRuntime(growthOwner);
        Check(!g_registry.valid(growth), "typed growth follower survived teardown");
        TypedFollowerLifecycle();
        TypedFollowerFailuresAndReplacement();
        TypedFollowerFollowingAndDeath();
        TypedFollowerStaleEntities();
        Reset();
        std::cout << "Pet lifecycle checks passed: " << checks << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
