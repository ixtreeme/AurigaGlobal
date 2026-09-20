#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/building.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/guild.h"
#include "../../SRC/Server/GameServer/guild_manager.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/ecs/CBuildingRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/CharacterAccessors.hpp"
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

// Production building.cpp + CBuildingRegistry.cpp are linked unchanged. Only
// spatial/world and guild/character services are controlled seams; the actual
// sector/visibility integration is covered separately by SpatialLifecycleTests.
// Packet buffers use the real Core buffer library; external gameplay calls fail
// unless explicitly modeled below.
entt::registry g_registry;
int test_server = 0;
LPCLIENT_DESC db_clientdesc = nullptr;
namespace {
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected building external service"); }
int checks = 0, spawned = 0, destroyedNPCs = 0, published = 0, bonus = 0;
uint32_t nextVID = 1000;
std::set<entt::entity> placed;
std::vector<EAttrRegionMode> attributes;
std::vector<int> bonuses;
std::function<void(entt::entity)> onRemove, onPublish, onSpawn;
CGuild* activeGuild = nullptr;
SECTREE* activeTree = nullptr;

void Check(bool value, const char* reason) {
    ++checks;
    if (!value) throw std::runtime_error(reason);
}
struct TestManager : building::CManager {
    void Prototypes() {
        m_vec_kObjectProto.assign(2, {});
        m_vec_kObjectProto[0].dwVnum = 14000;
        m_vec_kObjectProto[0].dwGroupVnum = 7;
        m_vec_kObjectProto[0].dwNPCVnum = 20045;
        m_vec_kObjectProto[0].lNPCX = 50;
        m_vec_kObjectProto[0].lNPCY = 100;
        m_vec_kObjectProto[1].dwVnum = 14061;
        for (auto& proto : m_vec_kObjectProto) {
            proto.lRegion[0] = proto.lRegion[1] = -100;
            proto.lRegion[2] = proto.lRegion[3] = 100;
            m_map_pkObjectProto[proto.dwVnum] = &proto;
        }
    }
};
building::TObject Data(uint32_t id = 100, uint32_t vnum = 14000) {
    building::TObject result {};
    result.dwID = id; result.dwLandID = 10; result.dwVnum = vnum;
    result.lMapIndex = 1; result.x = 1000; result.y = 2000;
    result.xRot = 10; result.yRot = 20; result.zRot = 90; result.lLife = 5000;
    return result;
}
void Reset(TestManager& manager, uint32_t guild = 0) {
    onRemove = {}; onPublish = {}; onSpawn = {};
    manager.Destroy();
    Check(g_registry.storage<entt::entity>().each().begin() ==
        g_registry.storage<entt::entity>().each().end(), "building fixture leaked an entity");
    placed.clear(); attributes.clear(); bonuses.clear();
    spawned = destroyedNPCs = published = bonus = 0;
    building::TLand land {};
    land.dwID = 10; land.dwGuildID = guild; land.lMapIndex = 1;
    land.width = land.height = 10000;
    Check(manager.LoadLand(&land), "land setup failed");
}
struct ConstructCallback {
    std::function<void(entt::registry&, entt::entity)> action;
    void Run(entt::registry& registry, entt::entity entity) { action(registry, entity); }
};
}

std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("building-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
SECTREE::SECTREE() = default;
SECTREE::~SECTREE() = default;
SECTREE_MANAGER::SECTREE_MANAGER() = default;
SECTREE_MANAGER::~SECTREE_MANAGER() = default;
SECTREE* SECTREE_MANAGER::Get(int32_t map, int32_t, int32_t) { return map == 1 ? activeTree : nullptr; }
bool SECTREE_MANAGER::ForAttrRegion(int32_t map, int32_t, int32_t, int32_t, int32_t,
    int32_t, uint32_t attr, EAttrRegionMode mode) {
    Check(map == 1 && attr == ATTR_OBJECT, "unexpected collision-attribute operation");
    attributes.push_back(mode); return true;
}
bool map_allow_find(int32_t map) { return map == 1; }
CGuildManager::CGuildManager() = default;
CGuildManager::~CGuildManager() = default;
CGuild* CGuildManager::FindGuild(uint32_t id) { return id == 42 ? activeGuild : nullptr; }
void CGuild::Load(uint32_t id) { m_data = {}; m_data.guild_id = id; }
CGuild::~CGuild() = default;
void CGuild::SetMemberCountBonus(int value) { bonus = value; bonuses.push_back(value); }
void CGuild::BroadcastMemberCountBonus() {}
CHARACTER_MANAGER::CHARACTER_MANAGER() = default;
CHARACTER_MANAGER::~CHARACTER_MANAGER() = default;
uint32_t CHARACTER_MANAGER::AllocVID() { return nextVID++; }
entt::entity CHARACTER_MANAGER::SpawnMobEntity(uint32_t vnum, int32_t map, int32_t x,
    int32_t y, int32_t z, bool, int rotation, bool) {
    Check(vnum == 20045 && map == 1 && x == 1100 && y == 1950 && z == 0 && rotation == 90,
        "NPC spawn did not use authoritative building position/rotation/prototype");
    ++spawned;
    const auto npc = g_registry.create();
    g_registry.emplace<ecs::TagCharacter>(npc);
    if (onSpawn) { auto callback = onSpawn; callback(npc); }
    return npc;
}
void CHARACTER_MANAGER::DestroyCharacter(entt::entity entity) {
    Check(ecs::IsCharacter(entity), "destroyed stale or non-character building NPC");
    ++destroyedNPCs; g_registry.destroy(entity);
}
void ecs::SocialSystem::SetGuild(entt::entity npc, CGuild* guild) {
    Check(ecs::IsCharacter(npc) && guild == activeGuild, "invalid building NPC guild assignment");
}
quest::PC* quest::CQuestManager::GetPC(unsigned int) { throw std::runtime_error("unexpected quest service"); }
void quest::PC::SetFlag(const std::string&, int, bool) { throw std::runtime_error("unexpected quest flag"); }
void intrusive_ptr_add_ref(EVENT*) { throw std::runtime_error("unexpected building event"); }
void intrusive_ptr_release(EVENT*) { throw std::runtime_error("unexpected building event"); }
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity) { Unexpected(); }
uint8_t ecs::PlayerRuntime::GetCharType(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetX(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetY(entt::entity) { Unexpected(); }
void ecs::SocialSystem::SendGuildName(entt::entity, CGuild*) { Unexpected(); }
bool SectreeMember(entt::entity, const SECTREE*) { Unexpected(); }
void SECTREE::Collect(FCollectEntity&) const { Unexpected(); }
LPSECTREE_MAP SECTREE_MANAGER::GetMap(int32_t) { Unexpected(); }
const TMapRegion* SECTREE_MANAGER::GetMapRegion(int32_t) { Unexpected(); }
TItemTable* ITEM_MANAGER::GetTable(uint32_t) { Unexpected(); }
bool CHARACTER_MANAGER::GetCharactersByRaceNum(uint32_t, std::vector<entt::entity>&) { Unexpected(); }
void DESC::BufferedPacket(const void*, int) { Unexpected(); }
void DESC::Packet(const void*, int) { Unexpected(); }
const DESC_MANAGER::DESC_SET& DESC_MANAGER::GetClientSet() { Unexpected(); }
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) { Unexpected(); }

namespace ecs::SpatialService {
bool InsertEntity(entt::registry& reg, entt::entity e, uint32_t map, int32_t x, int32_t y, int32_t z) {
    if (!reg.valid(e) || reg.all_of<ecs::SpatialRetiring>(e) || placed.contains(e) || map != 1)
        return false;
    reg.get<ecs::Position>(e) = {x, y, z};
    reg.get<ecs::MapIndex>(e).value = int32_t(map);
    placed.insert(e); return true;
}
void RemoveEntity(entt::registry&, entt::entity e) {
    if (!placed.erase(e)) return;
    if (onRemove) { auto callback = onRemove; callback(e); }
}
void UpdateSectree(entt::registry&, entt::entity e) {
    ++published;
    Check(g_registry.get<ecs::BuildingState>(e).attributesApplied, "published before collision attributes installed");
    if (onPublish) { auto callback = onPublish; callback(e); }
}
LPSECTREE GetSectree(entt::registry&, entt::entity e) { return placed.contains(e) ? activeTree : nullptr; }
}

namespace {
void NativeLifecycle(TestManager& manager) {
    Reset(manager);
    auto data = Data();
    Check(manager.LoadObject(&data, true), "boot load failed");
    const auto object = ecs::CBuildingRegistry::FindByID(data.dwID);
    const auto vid = building::ObjectSystem::GetVID(object);
    auto* land = manager.FindLand(10);
    Check(object != entt::null && building::ObjectSystem::IsValid(object), "boot entity missing");
    Check(!placed.contains(object) && attributes.empty(), "boot object prematurely published");
    Check(g_registry.get<ecs::BuildingState>(object).life == 5000, "persisted life missing");
    Check(land->FindObjectByVID(vid) == object && land->FindObjectByGroup(7) == object &&
        land->FindObjectByVnum(14000) == object, "native land lookups disagree");
    Check(manager.FindObjectByVID(vid) == object, "manager VID index missing");
    Check(!manager.LoadObject(&data, true), "duplicate DB load accepted");
    Check(building::ObjectSystem::Create(data, vid + 20) == entt::null, "duplicate ID accepted");
    auto another = Data(101);
    Check(building::ObjectSystem::Create(another, vid) == entt::null, "duplicate VID accepted");
    Check(building::ObjectSystem::Show(object, 1, 1000, 2000), "native show failed");
    Check(attributes.size() == 1 && attributes[0] == ATTR_REGION_MODE_SET, "collision setup missing");
    Check(!building::ObjectSystem::Show(object, 99, 100, 100), "invalid map accepted");
    Check(placed.contains(object) && attributes.size() == 1, "invalid map destroyed old placement");
    Check(building::ObjectSystem::Show(object, 1, 1100, 2200), "repeat show failed");
    Check(attributes.size() == 3 && attributes[1] == ATTR_REGION_MODE_REMOVE, "old collision not cleared on move");
    manager.DeleteObject(data.dwID);
    Check(!g_registry.valid(object) && !placed.contains(object), "entity survived delete");
    Check(manager.FindObjectByVID(vid) == entt::null && land->FindObject(data.dwID) == entt::null &&
        land->FindObjectByVID(vid) == entt::null, "delete left land/manager VID lookup dangling");
    Check(ecs::CBuildingRegistry::FindByID(data.dwID) == entt::null &&
        ecs::CBuildingRegistry::FindByVID(vid) == entt::null, "delete left registry lookup dangling");
    Check(attributes.size() == 4 && attributes.back() == ATTR_REGION_MODE_REMOVE, "collision teardown missing");
    building::ObjectSystem::Destroy(object);
    Check(attributes.size() == 4, "repeated destroy changed world attributes");
}

void ConstructionRollback(TestManager& manager) {
    Reset(manager);
    auto data = Data();
    entt::entity attempted = entt::null, replacement = entt::null;
    ConstructCallback callback;
    callback.action = [&](entt::registry& reg, entt::entity e) {
        attempted = e; reg.remove<ecs::BuildingState>(e);
    };
    auto connection = g_registry.on_construct<ecs::VIDComponent>().connect<&ConstructCallback::Run>(callback);
    Check(building::ObjectSystem::Create(data, 3000) == entt::null, "missing earlier component accepted");
    Check(!g_registry.valid(attempted), "missing-component rollback leaked entity");
    callback.action = [&](entt::registry& reg, entt::entity e) {
        attempted = e; reg.destroy(e); replacement = reg.create();
    };
    Check(building::ObjectSystem::Create(data, 3000) == entt::null, "destroyed construction accepted");
    Check(g_registry.valid(replacement) && replacement != attempted, "rollback destroyed recycled generation");
    g_registry.destroy(replacement);
    callback.action = [&](entt::registry&, entt::entity e) {
        attempted = e; throw std::runtime_error("construction callback");
    };
    bool threw = false;
    try { building::ObjectSystem::Create(data, 3000); }
    catch (const std::runtime_error&) { threw = true; }
    Check(threw && !g_registry.valid(attempted), "throwing construction did not roll back");
    connection.release();
    const auto retiring = building::ObjectSystem::Create(data, 3000);
    callback.action = [&](entt::registry& reg, entt::entity e) {
        Check(e == retiring, "unexpected retiring signal");
        reg.destroy(e); replacement = reg.create();
    };
    connection = g_registry.on_construct<ecs::SpatialRetiring>().connect<&ConstructCallback::Run>(callback);
    building::ObjectSystem::Destroy(retiring);
    Check(!g_registry.valid(retiring) && g_registry.valid(replacement), "retiring callback touched recycled generation");
    Check(ecs::CBuildingRegistry::FindByID(100) == entt::null, "retiring callback kept old building registered");
    connection.release();
    g_registry.destroy(replacement);
    auto unknown = Data(101, 99999);
    const auto object = building::ObjectSystem::Create(unknown, 3001);
    Check(object != entt::null && !building::ObjectSystem::Show(object, 1, 1000, 2000), "unknown prototype shown");
    building::ObjectSystem::Destroy(object);
    Check(attributes.empty(), "unshown building removed someone else's collision attributes");
}

void GenerationSafeTeardown(TestManager& manager) {
    Reset(manager, 42);
    auto data = Data(100, 14061);
    Check(manager.LoadObject(&data), "bonus building load failed");
    const auto old = ecs::CBuildingRegistry::FindByID(100);
    const auto oldVID = building::ObjectSystem::GetVID(old);
    Check(bonus == 6, "building bonus missing");
    entt::entity replacement = entt::null;
    onRemove = [&](entt::entity e) {
        if (e != old) return;
        Check(!building::ObjectSystem::IsValid(e) && bonus == 0, "teardown published before retiring/effect removal");
        Check(manager.FindObjectByVID(oldVID) == entt::null &&
            manager.FindLand(10)->FindObjectByVID(oldVID) == entt::null, "callback observed old VID index");
        Check(manager.LoadObject(&data), "callback replacement could not reuse database ID");
        replacement = ecs::CBuildingRegistry::FindByID(100);
    };
    building::ObjectSystem::Destroy(old);
    Check(replacement != entt::null && g_registry.valid(replacement) && replacement != old,
        "old teardown destroyed replacement");
    Check(bonus == 6 && placed.contains(replacement), "old teardown reset replacement bonus/placement");
    Check(ecs::CBuildingRegistry::FindByID(100) == replacement, "old teardown unregistered replacement");
    onRemove = [](entt::entity) { throw std::runtime_error("DEL callback"); };
    bool threw = false;
    try { building::ObjectSystem::Destroy(replacement); }
    catch (const std::runtime_error&) { threw = true; }
    Check(threw && !g_registry.valid(replacement), "throwing DEL callback leaked retiring building");
    Check(bonus == 0 && manager.FindLand(10)->FindObject(100) == entt::null,
        "throwing DEL callback left bonus/index");
    onRemove = {};
    Check(manager.LoadObject(&data), "land teardown setup failed");
    onRemove = [&](entt::entity) {
        Check(!manager.LoadObject(&data), "tearing-down land accepted a new building");
    };
    manager.FindLand(10)->Destroy();
    Check(ecs::CBuildingRegistry::FindByID(100) == entt::null && bonus == 0,
        "land teardown retained building or bonus");
    onRemove = {};
}

void LandBatchTeardown(TestManager& manager) {
    Reset(manager);
    auto firstData = Data(100), secondData = Data(101);
    Check(manager.LoadObject(&firstData) && manager.LoadObject(&secondData), "land batch setup failed");
    const auto first = ecs::CBuildingRegistry::FindByID(100);
    const auto second = ecs::CBuildingRegistry::FindByID(101);
    int removals = 0;
    onRemove = [&](entt::entity) {
        if (++removals == 1) throw std::runtime_error("first land DEL callback");
    };
    bool threw = false;
    try { manager.FindLand(10)->Destroy(); }
    catch (const std::runtime_error&) { threw = true; }
    Check(threw && removals == 2, "first failed building aborted land batch retirement");
    Check(!g_registry.valid(first) && !g_registry.valid(second) && placed.empty(), "land batch leaked a live generation");
    Check(ecs::CBuildingRegistry::FindByID(100) == entt::null &&
        ecs::CBuildingRegistry::FindByID(101) == entt::null, "land batch left registry lookups");
    onRemove = {};
}

void NPCOwnership(TestManager& manager) {
    Reset(manager, 42);
    auto data = Data();
    Check(manager.LoadObject(&data), "NPC building load failed");
    const auto object = ecs::CBuildingRegistry::FindByID(100);
    const auto npc = building::ObjectSystem::GetNPCEntity(object);
    Check(ecs::IsCharacter(npc) && spawned == 1, "native building NPC absent");
    Check(manager.FindLand(10)->FindObjectByNPC(npc) == object, "NPC lookup not entity based");
    building::ObjectSystem::RegenNPC(object);
    Check(spawned == 1, "repeated regeneration duplicated live NPC");
    building::ObjectSystem::Destroy(object);
    Check(!g_registry.valid(npc) && destroyedNPCs == 1, "NPC outlived its building");
    Check(manager.LoadObject(&data, true), "second NPC building load failed");
    const auto duringSpawn = ecs::CBuildingRegistry::FindByID(100);
    onSpawn = [&](entt::entity) { building::ObjectSystem::Destroy(duringSpawn); };
    building::ObjectSystem::RegenNPC(duringSpawn);
    Check(!g_registry.valid(duringSpawn) && destroyedNPCs == 2, "spawn callback leaked NPC after owner destruction");
    onSpawn = {};
}
}

int main() {
    try {
        SECTREE_MANAGER sectors;
        SECTREE tree; activeTree = &tree;
        CHARACTER_MANAGER characters;
        CGuildManager guilds;
        CGuild guild(42); activeGuild = &guild;
        TestManager manager;
        manager.Prototypes();
        NativeLifecycle(manager);
        ConstructionRollback(manager);
        GenerationSafeTeardown(manager);
        LandBatchTeardown(manager);
        NPCOwnership(manager);
        Reset(manager);
        manager.Destroy();
        std::cout << "Building lifecycle: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Building lifecycle failure: " << e.what() << '\n'; return 1;
    }
}
