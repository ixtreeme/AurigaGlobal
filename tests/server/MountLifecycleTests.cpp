#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/PetSystem.h"
#include "../../SRC/Server/GameServer/New_PetSystem.h"
#include "../../SRC/Server/GameServer/db.h"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/constants.h"
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

// Compile the complete production PetSystem.cpp/New_PetSystem.cpp. External
// services are doubled; no CHARACTER/CItem is created or attached to the
// entity fixtures. The costume mount runtime is covered by the GameServer
// integration path instead of this harness.
entt::registry g_registry;
entt::dispatcher g_dispatcher;
int passes_per_sec = 25;
const TApplyInfo aApplyInfo[MAX_APPLY_NUM] = {};
const uint32_t Pet_Skill_Table[12][23] = {};
uint32_t testPetExp[121] {};
uint32_t* exppet_table = testPetExp;

namespace {
struct CharacterState {
    int32_t x = 1000, y = 1000, z = 0, map = 1;
    float rotation = 0;
    uint32_t vid = 0, mount = 0;
    uint32_t exp = 0;
    uint8_t level = 1;
    uint8_t empire = 1;
    bool dead = false, war = false, dungeon = false, hasMob = true, walking = false;
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
bool failQuery = false, failPayment = false;
int nullColumn = -1, databaseRows = 1, liveResults = 0, consumed = 0, removedItems = 0;
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
    nullColumn = -1; databaseRows = 1; consumed = removedItems = questDelay = 0;
    queries.clear(); growthBonusOwners.clear();
    std::fill(std::begin(testPetExp), std::end(testPetExp), 1000u);
    databaseRow = {"grown pet","1","0","0","10","20","30","0","0","0","0","-1","0","-1","0","120","200","0","1","1000000"};
}
}

// Database/SQL client doubles never connect or start worker threads.
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
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
bool IsValid(entt::entity e) { return e != entt::null && g_registry.valid(e); }
LPDESC GetDesc(entt::entity) { return nullptr; }
uint32_t GetPacketVID(entt::entity e) { return State(e).vid; }
std::string_view GetName(entt::entity e) { return State(e).name; }
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
    ++removedItems;
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
bool SetItemForceAttributeEcs(entt::entity e, int index, uint8_t, int16_t value) {
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
namespace CombatSystem {
bool IsDead(entt::entity e) { return State(e).dead; }
void SetLastAttacked(entt::entity e, uint32_t) { State(e); }
}
namespace ecs::MovementSystem {
bool Show(entt::entity e, int32_t map, int32_t x, int32_t y, int32_t z, bool) {
    ++shown;
    auto& s = State(e);
    if (const auto* status = g_registry.try_get<ecs::StatusFlags>(e); status && status->isNewPet)
        Check(g_registry.all_of<ecs::GrowthPetComponent>(e)
            && g_registry.get<ecs::GrowthPetComponent>(e).level == s.level,
            "initial growth insert packet lacks creature-side level");
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
CAffect* AffectSystem::FindAffect(entt::entity e, uint32_t, uint8_t) { return bonusOwners.contains(e) ? &bonus : nullptr; }
bool AffectSystem::AddAffect(entt::entity e, uint32_t type, uint8_t, int32_t value, uint32_t,
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
    PetSystem::RefreshBuff(e);
}
void ecs::PointSystem::ApplyPoint(entt::entity e, uint8_t, int value) {
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
int32_t ecs::QuestSystem::GetFlag(entt::entity e, std::string_view) { State(e); return questDelay; }
void ecs::QuestSystem::SetFlag(entt::entity e, std::string_view, int32_t time) { State(e); questDelay = time; }
void ecs::ViewSystem::PacketView(entt::entity e, const void*, int, entt::entity) { State(e); ++packets; }
void ecs::ViewSystem::ViewReencode(entt::entity e) { State(e); ++packets; }
void ecs::ChatSystem::Send(entt::entity e, uint8_t, const char*, ...) { State(e); }
#ifdef TEXTS_IMPROVEMENT
void ecs::ChatSystem::SendNew(entt::entity e, uint8_t, uint32_t, const char*, ...) { State(e); }
#endif

namespace {
entt::entity PetItem(entt::entity owner, int value = 10)
{
    const auto item = Item(owner);
    auto& material = g_registry.get<ItemState>(item);
    material.proto.aApplies[0] = { APPLY_MOV_SPEED, value };
    material.attributeBonus = 7;
    return item;
}

void PetLifecycle()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner);
    PetSystem::SetUpdatePeriod(owner, 0);
    auto* actor = PetSystem::Summon(owner, 34001, item, "", false);
    Check(actor && actor->character != entt::null, "pet native summon failed");
    const auto pet = actor->character;
    Check(g_registry.get<ecs::StatusFlags>(pet).isPet, "pet marker missing");
    Check(g_registry.get<ecs::PlayerName>(pet).value == "owner's Pet", "pet name missing");
    Check(State(owner).petBonus == 17 && modifications == 1, "pet bonuses not rebuilt on summon");
    Check(g_registry.get<ItemState>(item).locked && ItemSystem::GetItemSocket(item, 2) == 1, "pet item not locked");
    Check(g_registry.get<ecs::PetComponent>(owner).item == item
        && g_registry.get<ecs::PetComponent>(owner).sockets[2] == 1, "pet component not synchronized");
    Check(PetSystem::FindActorByVID(owner, ecs::PlayerRuntime::GetPacketVID(pet)) == actor
        && !PetSystem::FindActorByVID(owner, 0), "pet VID lookup failed");
    const auto timer = scheduled;
    State(pet).x = State(owner).x + 500;
    Check(PetSystem::Update(owner, 0) && State(pet).walking && moves == 1, "near pet did not walk");
    State(pet).x += 1000;
    Check(PetSystem::Update(owner, 0) && !State(pet).walking && moves == 2, "far pet did not run");
    State(pet).map = 2;
    Check(PetSystem::Update(owner, 0) && State(pet).map == State(owner).map, "pet cross-map return failed");
    State(owner).dead = true;
    Check(PetSystem::Update(owner, 0) && actor->character != entt::null, "owner death changed pet survival rule");
    Check(timer->func(timer, 0) != 0, "pet owner-entity timer rejected");
    onCompute = [&] { Check(actor->character == entt::null && actor->summonItem == entt::null,
        "unsummon reentered with still-published handles"); };
    PetSystem::Unsummon(owner, 34001);
    onCompute = {};
    Check(!g_registry.valid(pet) && State(owner).petBonus == 0 && clearCalls == 1, "pet cleanup lost bonuses/character");
    Check(!g_registry.get<ItemState>(item).locked && ItemSystem::GetItemSocket(item, 2) == 0, "pet item remained locked");
    Check(g_registry.get<ecs::PetComponent>(owner).item == entt::null, "pet component not cleared");
    const auto computed = computes;
    PetSystem::Unsummon(owner, 34001);
    PetSystem::DestroyRuntime(owner);
    Check(computes == computed && destroyed == 1, "pet teardown repeated side effects");
    Check(timer->func(timer, 0) == 0 && !g_registry.try_get<ecs::PetRuntime>(owner), "pet timer/runtime outlived system");
}

void PetMultipleAndReentrantDeletion()
{
    Reset();
    const auto owner = Character(), first = PetItem(owner), second = PetItem(owner, 20);
    PetSystem::SetUpdatePeriod(owner, 0);
    auto* a = PetSystem::Summon(owner, 34001, first, "", false);
    auto* b = PetSystem::Summon(owner, 34002, second, "", false);
    Check(a && b && State(owner).petBonus == 44, "multiple-pet bonuses incorrect");
    State(a->character).hasMob = State(b->character).hasMob = false;
    mobQueries = 0;
    Check(!PetSystem::Update(owner, 0) && mobQueries == 2, "failed pet AI skipped another pet");
    onCompute = [&] { Check(!PetSystem::FindActor(owner, 34001), "deleting actor still visible during RefreshBuff"); };
    PetSystem::DeleteActor(owner, a->vnum);
    onCompute = {};
    Check(PetSystem::CountSummoned(owner) == 1 && State(owner).petBonus == 27, "single deletion removed remaining bonus");
    Check(!g_registry.get<ItemState>(first).locked && g_registry.get<ItemState>(second).locked, "single deletion unlocked wrong item");
    a = PetSystem::Summon(owner, 34001, first, "", false);
    Check(a && PetSystem::CountSummoned(owner) == 2, "pet resummon failed");
    PetSystem::UnsummonAll(owner);
    Check(PetSystem::CountSummoned(owner) == 0 && State(owner).petBonus == 0, "UnsummonAll left a pet/bonus behind");
    Check(!g_registry.get<ItemState>(first).locked && !g_registry.get<ItemState>(second).locked, "UnsummonAll left item locks");
    Check(PetSystem::FindActor(owner, 34001) && PetSystem::FindActor(owner, 34002), "UnsummonAll unexpectedly deleted actors");
    Check(scheduled->func(scheduled, 0) == 0, "UnsummonAll left active timer");
    a = PetSystem::Summon(owner, 34001, first, "", false);
    b = PetSystem::Summon(owner, 34002, second, "", false);
    int callbacks = 0;
    onCompute = [&] {
        ++callbacks;
        Check(PetSystem::CountSummoned(owner) == static_cast<size_t>(2 - callbacks), "destructor published a dying actor");
        Check(!PetSystem::Summon(owner, 34003, first, "", false), "summon reentered Destroy");
        PetSystem::DestroyRuntime(owner); // Recursive teardown is harmless.
    };
    PetSystem::DestroyRuntime(owner);
    onCompute = {};
    Check(callbacks == 2 && State(owner).petBonus == 0 && PetSystem::CountSummoned(owner) == 0,
        "multi-actor Destroy failed during reentrant point calculation");
    Check(!PetSystem::FindActor(owner, 34001) && !PetSystem::FindActor(owner, 34002), "Destroy retained actors");
}

void PetStaleHandles()
{
    for (int which = 0; which < 4; ++which)
    {
        Reset();
        const auto owner = Character(), item = PetItem(owner);
        PetSystem::SetUpdatePeriod(owner, 0);
        auto* actor = PetSystem::Summon(owner, 34001, item, "", false);
        const auto timer = scheduled;
        const auto pet = actor->character;
        const auto old = which == 0 ? owner : which == 1 ? item : pet;
        if (which == 0)
            PetSystem::DestroyRuntime(owner);
        if (which == 3)
            g_registry.get<ItemState>(item).owner = Character();
        else
            g_registry.destroy(old);
        const auto replacement = Character();
        Check(which == 3 || (entt::to_entity(old) == entt::to_entity(replacement) && old != replacement),
            "pet fixture did not recycle an entity generation");
        if (which == 0)
        {
            Check(timer->func(timer, 0) == 0, "stale-owner timer reached recycled character");
            Check(!g_registry.valid(pet) && !g_registry.get<ItemState>(item).locked
                && ItemSystem::GetItemSocket(item, 2) == 0, "stale pet owner teardown not cleaned");
        }
        else
        {
            Check(PetSystem::Update(owner, 0) && actor->character == entt::null && actor->summonItem == entt::null,
                "stale pet owner/item/follower not cleaned");
        }
        Check(g_registry.valid(replacement), "pet cleanup destroyed recycled entity");
        if (which != 0)
            Check(State(owner).petBonus == 0, "stale pet retained owner bonus");
        if (which == 3)
        {
            const auto newOwner = ItemSystem::GetItemOwner(item);
            Check(State(newOwner).petBonus == 0 && g_registry.get<ItemState>(item).locked,
                "pet cleanup modified transferred item's new owner or lock");
        }
        PetSystem::DestroyRuntime(owner);
        Check(g_registry.valid(replacement), "pet destructor destroyed recycled entity");
    }
}

void PetFailuresSkinsAndBonuses()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner), other = Character();
    PetSystem::SetUpdatePeriod(owner, 0);
    Check(!PetSystem::Summon(owner, 34004, entt::null, "", false), "null pet item accepted");
    g_registry.get<ItemState>(item).owner = other;
    Check(!PetSystem::Summon(owner, 34004, item, "", false) && spawned == 0, "foreign pet item spawned");
    g_registry.get<ItemState>(item).owner = owner;
    g_registry.get<ItemState>(item).hasProto = false;
    Check(!PetSystem::Summon(owner, 34004, item, "", false) && spawned == 0, "pet missing proto accepted");
    g_registry.get<ItemState>(item).hasProto = true;
    failSpawn = true;
    Check(!PetSystem::Summon(owner, 34004, item, "", false) && !scheduled, "failed pet spawn started timer");
    failSpawn = false; failShow = true;
    Check(!PetSystem::Summon(owner, 34004, item, "", false) && destroyed == 1 && !scheduled,
        "failed pet show leaked follower or timer");
    Check(!g_registry.get<ItemState>(item).locked && State(owner).petBonus == 0, "failed pet show applied side effects");
    failShow = false;
    const auto skin = Item(owner);
    State(owner).skin = skin;
    g_registry.get<ItemState>(skin).proto.alValues[0] = 34500;
    auto* actor = PetSystem::Summon(owner, 34004, item, "", false);
    Check(actor && actor->vnum == 34004 && spawnedVnum == 34500, "skin replaced pet's stable identity");
    Check(State(owner).petBonus == 0, "skin bypassed dungeon-only pet bonus restriction");
    State(owner).dungeon = true;
    ecs::PointSystem::Compute(owner);
    Check(State(owner).petBonus == 17, "dungeon pet bonus missing");
    const auto second = PetItem(owner);
    const auto pet = actor->character;
    Check(!PetSystem::Summon(owner, 34004, second, "", false) && actor->summonItem == item,
        "active pet rebound to a different item");
    Check(!PetSystem::Summon(owner, 34001, item, "", false), "one item became owned by two pet actors");
    Check(PetSystem::Summon(owner, 34004, item, "", true) == actor && actor->character == pet,
        "repeat summon duplicated active follower");
    State(owner).skin = entt::null;
    PetSystem::UpdatePetSkin(owner);
    actor = PetSystem::FindActor(owner, 34004);
    Check(actor && actor->vnum == 34004 && spawnedVnum == 34004 && actor->summonItem == item,
        "removing pet skin lost base identity/item");
    State(owner).dungeon = false;
    const auto beforeClear = clearCalls;
    PetSystem::Unsummon(owner, actor->vnum, true);
    Check(!PetSystem::FindActor(owner, 34004) && clearCalls == beforeClear + 1 && lastClearValue == -10,
        "dungeon exit prevented removal of previously applied bonus");
    Check(State(owner).petBonus == 0, "pet attributes survived removal");
    auto& material = g_registry.get<ItemState>(item);
    material.proto.aApplies[0].bType = MAX_APPLY_NUM;
    actor = PetSystem::Summon(owner, 34001, item, "", false);
    Check(actor && State(owner).petBonus == 0, "malformed pet apply entered point system");
    PetSystem::Unsummon(owner, 34001);
    material.proto.aApplies[0] = { APPLY_MOV_SPEED, std::numeric_limits<int>::min() };
    actor = PetSystem::Summon(owner, 34001, item, "", false);
    Check(actor && State(owner).petBonus == 0, "unnegatable pet bonus accepted");
    PetSystem::Unsummon(owner, 34001);
    material.proto.aApplies[0] = { APPLY_SKILL, 0x00800020 };
    PetSystem::Summon(owner, 34001, item, "", false);
    PetSystem::Unsummon(owner, 34001);
    Check(lastClearValue == 0x20, "skill bonus removal did not toggle the add bit");
}

void PetReplacementRuntimeAndItem()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner);
    PetSystem::Summon(owner, 34001, item, "", false);
    const auto oldTimer = scheduled;
    PetSystem::DestroyRuntime(owner);
    PetSystem::DestroyRuntime(owner);
    Check(oldTimer->func(oldTimer, 0) == 0, "old pet timer entered replacement runtime");
    auto* actor = PetSystem::Summon(owner, 34001, item, "", false);
    Check(actor && scheduled->func(scheduled, 0) != 0, "new pet timer rejected");
    const auto oldVID = ItemSystem::GetItemVID(item);
    g_registry.destroy(item);
    const auto replacement = PetItem(owner);
    Check(item != replacement && entt::to_entity(item) == entt::to_entity(replacement), "item generation not recycled");
    g_registry.get<ItemState>(replacement).vid = oldVID;
    g_registry.get<ItemState>(replacement).locked = true;
    g_registry.get<ItemState>(replacement).sockets[2] = 1;
    auto& state = g_registry.get<ecs::PetComponent>(owner);
    state.item = replacement;
    state.itemVID = oldVID;
    PetSystem::Unsummon(owner, actor->vnum);
    Check(g_registry.get<ItemState>(replacement).locked && ItemSystem::GetItemSocket(replacement, 2) == 1,
        "stale pet unlocked another item with same VID");
    Check(state.item == replacement && state.itemVID == oldVID, "pet cleared replacement ECS state");
    PetSystem::DestroyRuntime(owner);
}

void PetNullOwnerAndMounting()
{
    Reset();
    PetSystem::UpdatePetSkin(entt::null);
    PetSystem::Unmount(entt::null, 34001);
    PetSystem::DestroyRuntime(entt::null);
    Check(!PetSystem::Summon(entt::null, 34001, entt::null, "", false),
        "null-owner pet entered services");
    const auto owner = Character();
    auto* first = PetSystem::Summon(owner, 34001, PetItem(owner), "", false);
    Check(first && !PetSystem::Mount(owner, 34001), "non-mountable pet mounted");
    auto* mountableFirst = PetSystem::Summon(owner, 34003, PetItem(owner), "", false,
        PetSystem::EPetOption_Mountable);
    auto* mountableSecond = PetSystem::Summon(owner, 34002, PetItem(owner), "", false,
        PetSystem::EPetOption_Mountable);
    PetSystem::Unsummon(owner, 34001);
    Check(mountableFirst && mountableSecond, "entity pet fixture summon failed");
    Check(PetSystem::Mount(owner, 34003) && PetSystem::Mount(owner, 34002), "entity pet mounting failed");
    PetSystem::Unsummon(owner, 34003);
    Check(State(owner).mount == 34002, "old pet actor unmounted replacement");
    PetSystem::Unsummon(owner, 34002);
    Check(State(owner).mount == 0, "pet mount survived actor teardown");
}

void PetDeathOptionsAndSkinFailure()
{
    Reset();
    const auto owner = Character(), item = PetItem(owner);
    PetSystem::SetUpdatePeriod(owner, 1000);
    auto* actor = PetSystem::Summon(owner, 34001, item, "", false, PetSystem::EPetOption_Summonable);
    Check(actor && PetSystem::Update(owner, 0) && mobQueries == 0, "non-followable pet ran follow AI");
    State(actor->character).dead = true;
    Check(PetSystem::Update(owner, 0) && actor->character != entt::null, "update period was ignored");
    tick += 1000;
    Check(PetSystem::Update(owner, 0) && actor->character == entt::null && State(owner).petBonus == 0,
        "dead pet was not detached or retained its bonuses");
    Check(!g_registry.get<ItemState>(item).locked, "dead pet left summon item locked");
    Check(PetSystem::Summon(owner, 34001, item, "", false) == actor, "dead actor could not resummon");
    failShow = true;
    PetSystem::UpdatePetSkin(owner);
    Check(actor->character == entt::null && !g_registry.get<ItemState>(item).locked && State(owner).petBonus == 0,
        "failed skin respawn leaked follower/item lock/bonus");
    Check(scheduled->func(scheduled, 0) == 0, "failed skin respawn retained timer");
}

entt::entity GrowthSeal(entt::entity owner)
{
    const auto item = Item(owner);
    auto& seal = g_registry.get<ItemState>(item);
    seal.vnum = 55701;
    seal.sockets[1] = 120;
    seal.sockets[2] = 200;
    return item;
}

void GrowthLifecycleAndExpiry()
{
    Reset();
    const auto owner = Character(), seal = GrowthSeal(owner);
    NewPetSystem::SetUpdatePeriod(owner, 0);
    auto* actor = NewPetSystem::Summon(owner, 34041, seal, nullptr, false);
    Check(actor && NewPetSystem::HasValidSummon(owner, *actor), "growth entity summon failed");
    const auto pet = actor->character;
        Check(g_registry.get<ecs::StatusFlags>(pet).isNewPet,
            "growth follower is marked new pet");
    Check(g_registry.get<ecs::PlayerName>(pet).value == "grown pet"
        && g_registry.get<ecs::GrowthPetComponent>(pet).item == seal, "growth identity component mismatch");
    Check(g_registry.get<ItemState>(seal).locked && ItemSystem::GetItemSocket(seal, 0) == 1, "growth seal not locked");
    Check(ItemSystem::GetItemSocket(seal, 1) == 120, "summon consumed one minute");
    Check(growthBonusOwners.contains(owner), "growth bonuses missing");
    Check(retainedEvents.size() == 2, "growth timers were not scheduled");
    const auto updateEvent = retainedEvents[0], expiryEvent = retainedEvents[1];
    Check(updateEvent->func(updateEvent, 0) != 0 && expiryEvent->func(expiryEvent, 0) != 0, "growth timers rejected live owner");
    for (int i = 1; i < 59; ++i) NewPetSystem::UpdateTime(owner);
    Check(ItemSystem::GetItemSocket(seal, 1) == 120, "growth duration decremented before 60 seconds");
    NewPetSystem::UpdateTime(owner);
    Check(ItemSystem::GetItemSocket(seal, 1) == 119, "growth duration did not decrement");
    State(pet).map = 2;
    Check(NewPetSystem::Update(owner, 0) && State(pet).map == State(owner).map, "growth cross-map follow failed");
    onCompute = [&] { Check(!NewPetSystem::IsSummoned(*actor) && actor->summonItem == entt::null, "growth teardown exposed handles to callback"); };
    NewPetSystem::Unsummon(owner, 34041);
    onCompute = {};
    Check(!g_registry.valid(pet) && !g_registry.get<ItemState>(seal).locked, "growth teardown leaked follower/lock");
    Check(!growthBonusOwners.contains(owner) && databaseRow[15] == "119", "growth teardown lost persistence/bonus cleanup");
    const auto saves = queries.size();
    NewPetSystem::DestroyRuntime(owner);
    Check(queries.size() == saves && !NewPetSystem::FindActor(owner, 34041), "growth destructor repeated save");
    Check(updateEvent->func(updateEvent, 0) == 0 && expiryEvent->func(expiryEvent, 0) == 0, "stale growth timer survived");

    databaseRow[15] = "1";
    actor = NewPetSystem::Summon(owner, 34041, seal, "", false);
    Check(actor && NewPetSystem::IsSummoned(*actor), "one-minute pet could not summon");
    for (int i = 0; i < 60; ++i) NewPetSystem::UpdateTime(owner);
    Check(!NewPetSystem::IsSummoned(*actor) && ItemSystem::GetItemSocket(seal, 1) == 0, "zero duration underflowed or stayed active");
}

void GrowthDatabaseValidation()
{
    for (int which = 0; which < 13; ++which)
    {
        Reset();
        const auto owner = Character(), seal = GrowthSeal(owner);
        NewPetSystem::SetUpdatePeriod(owner, 0);
        if (which == 0) failQuery = true;
        if (which == 1) databaseRows = 0;
        if (which == 2) databaseRows = 2;
        if (which == 3) databaseRow.pop_back();
        if (which == 4) nullColumn = 0;
        if (which == 5) nullColumn = 7;
        if (which == 6) databaseRow[1] = "121";
        if (which == 7) databaseRow[7] = "13";
        if (which == 8) databaseRow[8] = "11";
        if (which == 9) databaseRow[15] = "-1";
        if (which == 10) databaseRow[17] = "4";
        if (which == 11) databaseRow[2] = "12oops";
        if (which == 12) databaseRow[4] = "999999999999999999999";
        Check(!NewPetSystem::Summon(owner, 34041, seal, nullptr, false), "malformed growth row accepted");
        Check(spawned == 0 && retainedEvents.empty() && !g_registry.get<ItemState>(seal).locked,
            "malformed growth row had spawn/lock/timer side effects");
    }
    Reset();
    const auto owner = Character(), seal = GrowthSeal(owner);
    NewPetSystem::SetUpdatePeriod(owner, 0);
    failSpawn = true;
    Check(!NewPetSystem::Summon(owner, 34041, seal, "", false) && retainedEvents.empty(), "failed growth factory started timers");
    failSpawn = false; failShow = true;
    Check(!NewPetSystem::Summon(owner, 34041, seal, "", false) && destroyed == 1, "failed growth show leaked follower");
    Check(!g_registry.get<ItemState>(seal).locked && !growthBonusOwners.contains(owner), "failed growth show applied side effects");
    failShow = false;
    databaseRow[1] = "40"; databaseRow[2] = "900";
    auto* actor = NewPetSystem::Summon(owner, 34041, seal, "", false);
    Check(actor && actor->level == 40 && actor->evolution == 0, "DB hydration executed a level-up transition");
    Check(NewPetSystem::IncreasePetEvolution(owner) && actor->level == 41 && actor->evolution == 1,
        "native growth evolution failed");
    const auto skin = Item(owner);
    State(owner).skin = skin;
    g_registry.get<ItemState>(skin).proto.alValues[0] = 34500;
    NewPetSystem::UpdatePetSkin(owner);
    actor = NewPetSystem::FindActor(owner, 34041);
    Check(actor && actor->vnum == 34041 && spawnedVnum == 34500 && actor->level == 41
        && ItemSystem::GetItemSocket(seal, 1) == 120, "growth skin changed identity/progression/duration");
    NewPetSystem::ChangeName(owner, "O'Pet");
    Check(g_registry.get<ecs::PlayerName>(actor->character).value == "O'Pet", "native rename did not publish name");
    Check(queries.back().find("name='O\\'Pet' WHERE id=" + std::to_string(ItemSystem::GetItemID(seal))) != std::string::npos,
        "rename was not escaped and keyed by item ID");
    NewPetSystem::Unsummon(owner, 34041);
}

void GrowthFeedAndSkillInputs()
{
    Reset();
    const auto owner = Character(), seal = GrowthSeal(owner);
    NewPetSystem::SetUpdatePeriod(owner, 0);
    auto* actor = NewPetSystem::Summon(owner, 34041, seal, "", false);
    Check(actor != nullptr, "growth feed fixture summon failed");
    const auto food = Item(owner);
    g_registry.get<ItemState>(food).vnum = 55001;
    const auto cell = g_registry.get<ItemState>(food).cell;
    for (int invalid : {-1,9,180,INT_MAX,INT_MIN}) NewPetSystem::SetItemCube(owner, invalid, cell);
    NewPetSystem::ItemCubeFeed(owner, 1);
    Check(g_registry.valid(food) && removedItems == 0, "out-of-bounds feed selection accepted");
    NewPetSystem::SetItemCube(owner, 0, cell);
    NewPetSystem::SetItemCube(owner, 1, cell);
    failPayment = true;
    NewPetSystem::ItemCubeFeed(owner, 1);
    Check(g_registry.valid(food) && ItemSystem::GetItemSocket(seal, 1) == 120, "failed feed awarded duration");
    failPayment = false;
    NewPetSystem::SetItemCube(owner, 0, cell);
    g_registry.destroy(food);
    const auto replacement = Item(owner);
    g_registry.get<ItemState>(replacement).cell = cell;
    g_registry.get<ItemState>(replacement).vnum = 55001;
    NewPetSystem::ItemCubeFeed(owner, 1);
    Check(g_registry.valid(replacement), "stale feed consumed a replacement entity");
    NewPetSystem::SetItemCube(owner, 0, cell);
    NewPetSystem::ItemCubeFeed(owner, 1);
    NewPetSystem::UpdateTime(owner, true);
    Check(!g_registry.valid(replacement) && removedItems == 1 && ItemSystem::GetItemSocket(seal, 1) == 200,
        "valid growth feeding did not cap duration");
    const auto book = Item(owner);
    auto& material = g_registry.get<ItemState>(book);
    material.proto.bType = ITEM_TYPE_PET; material.proto.alValues[0] = 1; material.count = 5;
    for (int invalid : {-1,4,INT_MAX,INT_MIN})
        Check(!NewPetSystem::IncreasePetSkill(owner, invalid, material.cell), "invalid skill slot accepted");
    failPayment = true;
    Check(!NewPetSystem::IncreasePetSkill(owner, 0, material.cell) && consumed == 0, "failed skill payment advanced skill");
    failPayment = false;
    Check(NewPetSystem::IncreasePetSkill(owner, 0, material.cell) && consumed == 1, "native skill learning failed");
    Check(!NewPetSystem::IncreasePetSkill(owner, 1, material.cell) && consumed == 1, "duplicate growth skill accepted");
    material.owner = Character();
    Check(!NewPetSystem::IncreasePetSkillByBook(owner, book) && consumed == 1, "foreign owner's book consumed");
    material.owner = owner;
    material.locked = true;
    Check(!NewPetSystem::IncreasePetSkillByBook(owner, book) && consumed == 1, "locked skill book consumed");
    NewPetSystem::DoPetSkill(owner, INT_MAX);
    NewPetSystem::DoPetSkill(owner, -1);
    NewPetSystem::Unsummon(owner, 34041);
    Check(databaseRow[7] == "1" && databaseRow[8] == "1", "learned skill not persisted");
}

void GrowthStaleAndMultiple()
{
    for (int which = 0; which < 4; ++which)
    {
        Reset();
        const auto owner = Character(), seal = GrowthSeal(owner);
        NewPetSystem::SetUpdatePeriod(owner, 0);
        auto* actor = NewPetSystem::Summon(owner, 34041, seal, "", false);
        Check(actor != nullptr, "growth stale fixture summon failed");
        const auto pet = actor->character;
        const auto old = which == 0 ? owner : which == 1 ? seal : pet;
        if (which == 0)
            NewPetSystem::DestroyRuntime(owner);
        if (which == 3)
            g_registry.get<ItemState>(seal).owner = Character();
        else
            g_registry.destroy(old);
        const auto replacement = Character();
        Check(which == 3 || (entt::to_entity(old) == entt::to_entity(replacement) && old != replacement),
            "growth fixture did not recycle an entity generation");
        if (which == 0)
        {
            Check(!g_registry.valid(pet) && !g_registry.get<ItemState>(seal).locked,
                "stale growth owner teardown not cleaned");
        }
        else
        {
            Check(NewPetSystem::Update(owner, 0) && !NewPetSystem::IsSummoned(*actor)
                && actor->summonItem == entt::null, "growth stale handle cleanup failed");
        }
        Check(g_registry.valid(replacement), "growth cleanup destroyed recycled entity");
        if (which == 1 || which == 3)
            Check(queries.size() == 1, "stale growth handle saved another character's pet data");
        NewPetSystem::DestroyRuntime(owner);
        Check(g_registry.valid(replacement), "growth destructor destroyed recycled entity");
    }
    Reset();
    const auto owner = Character(), first = GrowthSeal(owner), second = GrowthSeal(owner);
    NewPetSystem::SetUpdatePeriod(owner, 0);
    Check(NewPetSystem::Summon(owner, 34041, first, "", false)
        && NewPetSystem::Summon(owner, 34045, second, "", false), "multiple growth pets failed");
    Check(!NewPetSystem::Summon(owner, 34049, first, "", false), "growth seal bound to two actors");
    NewPetSystem::UnsummonAll(owner);
    Check(NewPetSystem::CountSummoned(owner) == 0 && !g_registry.get<ItemState>(first).locked
        && !g_registry.get<ItemState>(second).locked, "growth UnsummonAll stopped after first pet");
    NewPetSystem::Summon(owner, 34041, first, "", false);
    NewPetSystem::Summon(owner, 34045, second, "", false);
    int callbacks = 0;
    onCompute = [&] {
        ++callbacks;
        Check(NewPetSystem::CountSummoned(owner) == static_cast<size_t>(2 - callbacks), "growth destructor exposed dying actor");
        NewPetSystem::DestroyRuntime(owner); // Recursive teardown is harmless.
    };
    const auto oldUpdate = retainedEvents[retainedEvents.size()-2];
    const auto oldExpiry = retainedEvents.back();
    NewPetSystem::DestroyRuntime(owner);
    onCompute = {};
    NewPetSystem::Summon(owner, 34041, first, "", false);
    Check(oldUpdate->func(oldUpdate, 0) == 0 && oldExpiry->func(oldExpiry, 0) == 0, "old growth timer entered replacement");
    Check(NewPetSystem::FindActor(owner, 34041) != nullptr, "replacement growth runtime lost its pet");
    NewPetSystem::DestroyRuntime(owner);
}

void GrowthRecycledSealAndNullOwner()
{
    Reset();
    NewPetSystem::SetUpdatePeriod(entt::null, 0);
    NewPetSystem::ChangeName(entt::null, "Pet");
    NewPetSystem::SetItemCube(entt::null, INT_MIN, INT_MAX);
    NewPetSystem::ItemCubeFeed(entt::null, 1);
    NewPetSystem::RefreshBuff(entt::null);
    NewPetSystem::UpdatePetSkin(entt::null);
    Check(!NewPetSystem::Mount(entt::null, 34041)
        && !NewPetSystem::Summon(entt::null, 34041, entt::null, nullptr, false),
        "null-owner growth pet entered runtime services");
    NewPetSystem::Unsummon(entt::null, 34041);
    NewPetSystem::DestroyRuntime(entt::null);
    const auto owner = Character(), seal = GrowthSeal(owner);
    auto* actor = NewPetSystem::Summon(owner, 34041, seal, "", false);
    Check(actor != nullptr, "growth recycled fixture summon failed");
    const auto id = ItemSystem::GetItemID(seal), vid = ItemSystem::GetItemVID(seal);
    g_registry.destroy(seal);
    const auto replacement = GrowthSeal(owner);
    Check(entt::to_entity(seal) == entt::to_entity(replacement) && seal != replacement, "seal fixture did not recycle index");
    auto& item = g_registry.get<ItemState>(replacement);
    item.id = id; item.vid = vid; item.locked = true; item.sockets[0] = 7; item.attributes[0] = 99;
    const auto before = queries.size();
    NewPetSystem::Unsummon(owner, actor->vnum);
    Check(queries.size() == before && item.locked && item.sockets[0] == 7 && item.attributes[0] == 99,
        "old growth actor modified/saved replacement seal with reused item ID/VID");
}
}

int main()
{
    try {
        CHARACTER_MANAGER factory;
        DBManager database;
        PetLifecycle();
        PetMultipleAndReentrantDeletion();
        PetStaleHandles();
        PetFailuresSkinsAndBonuses();
        PetReplacementRuntimeAndItem();
        PetNullOwnerAndMounting();
        PetDeathOptionsAndSkinFailure();
        GrowthLifecycleAndExpiry();
        GrowthDatabaseValidation();
        GrowthFeedAndSkillInputs();
        GrowthStaleAndMultiple();
        GrowthRecycledSealAndNullOwner();
        Reset();
        std::cout << "Pet lifecycle checks passed: " << checks << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
