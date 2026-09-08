#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/ecs/SpatialHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/ecs/services/VisibilityService.hpp"
#include "../../SRC/Server/GameServer/ecs/services/EntityNetworkDispatch.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/VisibilitySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AISystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/movement_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_runtime_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/combat_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/ai_components.hpp"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/dungeon.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/motion.h"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/item_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/ecs/ItemRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/CBuildingRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/OfflineShopEntityRegistry.hpp"
#include <Base/attribute.h>
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
entt::dispatcher g_dispatcher;
int VIEW_RANGE = 5000;
int VIEW_BONUS_RANGE = 500;
namespace {
int checks = 0;
void Check(bool good, const char* why) { ++checks; if (!good) throw std::runtime_error(why); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy spatial service"); }
std::map<std::tuple<int, int, int>, SECTREE*> sectors;
std::unordered_set<entt::entity> awake;
struct Packet { bool add; entt::entity source, viewer; };
std::vector<Packet> packets;
std::function<void(Packet)> onPacket;
std::function<void(entt::entity)> onRetire;
int retired = 0;
std::vector<TPacketGCMove> movementPackets;
std::vector<std::pair<entt::entity, ecs::AIFSMState>> transitions;

struct MapFixture {
    int index;
    SECTREE_MAP map;
    explicit MapFixture(int id = 1) : index(id) {
        for (int x = 0; x < 4; ++x) for (int y = 0; y < 2; ++y) {
            auto* tree = new SECTREE;
            map.Add(uint32_t(x + y * 4), tree);
            sectors[{id, x, y}] = tree;
        }
        map.Build();
    }
    ~MapFixture() {
        // Drain while lookup and every neighbor are live. The map owns deletion.
        auto drain = [&](entt::entity e) { ecs::SpatialService::RemoveEntity(g_registry, e); };
        map.for_each(drain);
        for (auto it = sectors.begin(); it != sectors.end();)
            if (std::get<0>(it->first) == index) it = sectors.erase(it); else ++it;
    }
    SECTREE* At(int x = 100, int y = 100) { return SECTREE_MANAGER::instance().Get(index, x, y); }
};

entt::entity Entity(ecs::SpatialKind kind = ecs::SpatialKind::Item) {
    const auto e = g_registry.create();
    g_registry.emplace<ecs::SpatialKindTag>(e, kind);
    g_registry.emplace<ecs::VIDComponent>(e, uint32_t(entt::to_integral(e) + 1));
    if (kind == ecs::SpatialKind::Item) {
        g_registry.emplace<ecs::ItemIdentity>(e);
        g_registry.emplace<ecs::ItemLocation>(e, ecs::ItemLocation {GROUND, 0});
    }
    if (kind == ecs::SpatialKind::Character) {
        g_registry.emplace<ecs::CharacterType>(e, uint8_t(CHAR_TYPE_PC));
        g_registry.emplace<ecs::TagPC>(e);
    }
    return e;
}
bool Spawn(entt::entity e, int map = 1, int x = 100, int y = 100) {
    if (!ecs::SpatialService::InsertEntity(g_registry, e, map, x, y, 0)) return false;
    ecs::SpatialService::UpdateSectree(g_registry, e);
    return true;
}
bool Visible(entt::entity source, entt::entity viewer) {
    return g_registry.valid(viewer) && g_registry.all_of<ecs::ViewMap>(viewer) &&
        g_registry.get<ecs::ViewMap>(viewer).visible.contains(source);
}
void Reset() {
    onPacket = {}; onRetire = {}; packets.clear(); awake.clear(); retired = 0;
    movementPackets.clear(); transitions.clear(); g_registry.clear();
}
struct Callback {
    std::function<void(entt::registry&, entt::entity)> fn;
    void Run(entt::registry& reg, entt::entity e) { fn(reg, e); }
};
}

std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto log = std::make_shared<spdlog::logger>("spatial-test"); return log;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
SECTREE_MANAGER::SECTREE_MANAGER() {}
SECTREE_MANAGER::~SECTREE_MANAGER() {}
SECTREE* SECTREE_MANAGER::Get(int32_t map, int32_t x, int32_t y) {
    if (x < 0 || y < 0) return nullptr;
    auto it = sectors.find({map, x / SECTREE_SIZE, y / SECTREE_SIZE});
    return it == sectors.end() ? nullptr : it->second;
}
// Grid loading is a fixture, not a second production spatial implementation.
SECTREE_MAP::SECTREE_MAP() { m_setting = {}; }
SECTREE_MAP::~SECTREE_MAP() {
    for (auto [id, tree] : map_) tree->Destroy();
    for (auto [id, tree] : map_) delete tree;
}
void SECTREE_MAP::Build() {
    for (auto [id, tree] : map_) {
        tree->m_neighbor_list.clear();
        for (auto [otherID, other] : map_)
            if (std::abs(int(id % 4) - int(otherID % 4)) <= 1 &&
                std::abs(int(id / 4) - int(otherID / 4)) <= 1)
                tree->m_neighbor_list.push_back(other);
    }
}
CAttribute::~CAttribute() {}
uint32_t CAttribute::Get(uint32_t, uint32_t) { Unexpected(); }
void CAttribute::Set(uint32_t, uint32_t, uint32_t) { Unexpected(); }
void CAttribute::Remove(uint32_t, uint32_t, uint32_t) { Unexpected(); }
CHARACTER_MANAGER::CHARACTER_MANAGER() {}
CHARACTER_MANAGER::~CHARACTER_MANAGER() {}
bool CHARACTER_MANAGER::AddToStateList(entt::entity e) { Check(g_registry.valid(e), "wake stale NPC"); return awake.insert(e).second; }
void CHARACTER_MANAGER::RemoveFromStateList(entt::entity e) { awake.erase(e); }
void CHARACTER_MANAGER::DestroyCharacter(entt::entity e) { g_registry.destroy(e); }
DESC_MANAGER::DESC_MANAGER() {}
DESC_MANAGER::~DESC_MANAGER() {}
void DESC_MANAGER::DestroyDesc(LPDESC, bool) { Unexpected(); }
DESC* ecs::PlayerRuntime::GetDesc(entt::entity) { return nullptr; }
// Gameplay leaf services are isolated here; movement, packet encoding,
// SetPosition, sectree relocation and visibility run their production code.
uint32_t get_dword_time() { return 123456; }
bool ecs::PlayerRuntime::IsPC(entt::entity e) { return g_registry.all_of<ecs::TagPC>(e); }
bool ecs::PlayerRuntime::IsStone(entt::entity e) { return g_registry.all_of<ecs::StoneAITag>(e); }
void ecs::PlayerRuntime::MonsterLog(entt::entity, const char*) {}
void ecs::PlayerRuntime::CancelCharEvent(entt::entity e, CharEvent) { Check(g_registry.valid(e), "cancel event on stale entity"); }
float ecs::PlayerRuntime::GetRotation(entt::entity e) {
    const auto* runtime = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
    return runtime ? runtime->rotation : 0.0f;
}
void AISystem::GotoState(entt::entity e, ecs::AIFSMState state) {
    Check(g_registry.valid(e), "AI transition on stale entity"); transitions.emplace_back(e, state);
}
entt::entity CombatSystem::GetVictim(entt::entity e) {
    const auto* target = g_registry.try_get<ecs::CombatTarget>(e);
    return target && g_registry.valid(target->target) &&
        g_registry.all_of<ecs::CharacterType>(target->target) ? target->target : entt::null;
}
void ecs::ViewSystem::PacketView(entt::entity e, const void* data, int size, entt::entity except) {
    Check(g_registry.valid(e) && e == except && size == sizeof(TPacketGCMove), "invalid movement broadcast");
    movementPackets.push_back(*static_cast<const TPacketGCMove*>(data));
}
// Fail-fast link seams for the still-unmigrated functions in MovementSystem.cpp.
// None may be reached by the native tick or packet tests.
void intrusive_ptr_add_ref(event*) { Unexpected(); }
void intrusive_ptr_release(event*) { Unexpected(); }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { Unexpected(); }
void ecs::ChatSystem::Send(entt::entity, uint8_t, const char*, ...) { Unexpected(); }
bool AffectSystem::IsAffectFlag(entt::entity, uint32_t) { Unexpected(); }
bool AffectSystem::IsPolymorphed(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetPacketVID(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetRaceNum(entt::entity) { Unexpected(); }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetMapIndex(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetX(entt::entity) { Unexpected(); }
int32_t ecs::PlayerRuntime::GetY(entt::entity) { Unexpected(); }
LPEVENT ecs::PlayerRuntime::GetCharEvent(entt::entity, CharEvent) { Unexpected(); }
void ecs::PlayerRuntime::SetCharEvent(entt::entity, CharEvent, LPEVENT) { Unexpected(); }
int ecs::PlayerRuntime::GetPosition(entt::entity) { Unexpected(); }
void CombatSystem::CheckTarget(entt::entity) { Unexpected(); }
bool CombatSystem::IsStun(entt::entity) { Unexpected(); }
bool CombatSystem::IsDead(entt::entity) { Unexpected(); }
int32_t CEntity::GetX() const { Unexpected(); }
int32_t CEntity::GetY() const { Unexpected(); }
int32_t CEntity::GetZ() const { Unexpected(); }
LPSECTREE CEntity::GetSectree() const { Unexpected(); }
int CalculateDuration(int, int) { Unexpected(); }
uint16_t CHARACTER::GetRaceNum() const { Unexpected(); }
void CHARACTER::Save() { Unexpected(); }
void CHARACTER::FlushDelayedSaveItem() { Unexpected(); }
const char* CHARACTER::GetName(uint8_t) const { Unexpected(); }
uint32_t CHARACTER::GetPacketVID() const { Unexpected(); }
void CHARACTER::DistributeSP(entt::entity, int) { Unexpected(); }
int64_t CHARACTER::GetHP() const { Unexpected(); }
int CHARACTER::GetStamina() const { Unexpected(); }
int CHARACTER::GetLimitPoint(uint8_t) const { Unexpected(); }
const TMobTable& CHARACTER::GetMobTable() const { Unexpected(); }
void CHARACTER::PointChange(uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
bool CHARACTER::Show(int32_t, int32_t, int32_t, int32_t, bool) { Unexpected(); }
void CHARACTER::OnMove(bool) { Unexpected(); }
bool CHARACTER::WarpSet(int32_t, int32_t, int32_t) { Unexpected(); }
void CHARACTER::SaveExitLocation() { Unexpected(); }
void CHARACTER::ExitToSavedLocation() { Unexpected(); }
bool CHARACTER::IsAffectFlag(uint32_t) const { Unexpected(); }
bool CHARACTER::IsEquipUniqueItem(uint32_t) const { Unexpected(); }
void CHARACTER::Dead(entt::entity, bool) { Unexpected(); }
void CHARACTER::UpdateKillerMode() { Unexpected(); }
LPCHARACTER CHARACTER::GetVictim() const { Unexpected(); }
void CHARACTER::MonsterLog(const char*, ...) { Unexpected(); }
uint8_t CHARACTER::GetEmpire() const { Unexpected(); }
int CDungeon::GetFlag(std::string) { Unexpected(); }
float CMotion::GetDuration() const { Unexpected(); }
const D3DXVECTOR3& CMotion::GetAccumVector() const { Unexpected(); }
const CMotion* CMotionManager::GetMotion(uint32_t, uint32_t) { Unexpected(); }
float GetDegreeFromPositionXY(int32_t, int32_t, int32_t, int32_t) { Unexpected(); }
void quest::CQuestManager::AttrIn(uint32_t, LPCHARACTER, int) { Unexpected(); }
void quest::CQuestManager::AttrOut(uint32_t, LPCHARACTER, int) { Unexpected(); }
entt::entity ItemSystem::GetWearItem(entt::entity, uint8_t) { Unexpected(); }
bool ItemSystem::IsValidItem(entt::entity) { Unexpected(); }
const TItemTable* ItemSystem::GetItemProto(entt::entity) { Unexpected(); }
uint32_t CParty::GetLeaderPID() { Unexpected(); }
int64_t ecs::PointSystem::Get(entt::entity, uint8_t) { Unexpected(); }
int ecs::PointSystem::GetMaxHP(entt::entity) { Unexpected(); }
void ecs::PointSystem::Change(entt::entity, uint8_t, int64_t, bool, bool, bool) { Unexpected(); }
uint32_t g_start_position[4][2] {};
int passes_per_sec = 25;
int save_event_second_cycle = 60;
int test_server = 0;
int CEntity::GetType() const { Unexpected(); }
CItem* ITEM_MANAGER::Find(uint32_t) { Unexpected(); }
CItem* ITEM_MANAGER::FindByVID(uint32_t) { Unexpected(); }
CItemRegistry& CItemRegistry::Instance() { Unexpected(); }
entt::entity CItemRegistry::Find(uint32_t) const { Unexpected(); }
entt::entity CItemRegistry::FindByVID(uint32_t) const { Unexpected(); }
entt::entity ecs::CBuildingRegistry::FindByID(uint32_t) { Unexpected(); }
building::CObject* ecs::CBuildingRegistry::FindLegacyByEntity(entt::entity) { Unexpected(); }
entt::entity ecs::OfflineShopEntityRegistry::FindByVID(uint32_t) { Unexpected(); }
offlineshop::ShopEntity* ecs::OfflineShopEntityRegistry::FindLegacyByEntity(entt::entity) { Unexpected(); }
bool ItemSystem::DestroyItemEntityEcs(entt::entity e, const char*) {
    ++retired;
    if (onRetire) { auto callback = onRetire; callback(e); }
    if (g_registry.valid(e)) g_registry.destroy(e);
    return true;
}
namespace ecs::EntityNetworkDispatch {
void SendInsert(entt::registry& reg, entt::entity source, entt::entity viewer) {
    Check(reg.valid(source) && reg.valid(viewer), "insert stale identity");
    Check(source == viewer || Visible(source, viewer), "insert before edge commit");
    Packet packet {true, source, viewer}; packets.push_back(packet);
    if (onPacket) { auto callback = onPacket; callback(packet); }
}
void SendRemove(entt::registry& reg, entt::entity source, entt::entity viewer) {
    Check(reg.valid(source) && reg.valid(viewer), "remove stale identity");
    Packet packet {false, source, viewer}; packets.push_back(packet);
    if (onPacket) { auto callback = onPacket; callback(packet); }
}
}

namespace {
void MembershipAndSnapshots() {
    Reset(); MapFixture map, privateMap(10001);
    const auto a = Entity(), b = Entity();
    Check(Spawn(a) && Spawn(b, 1, 6500), "native item insertion failed");
    Check(ecs::SpatialService::GetSectree(g_registry, a) == map.At(), "native sector lookup failed");
    std::vector<entt::entity> nearby;
    auto collect = [&](entt::entity e) { nearby.push_back(e); };
    map.At()->ForEachAround(collect);
    Check(nearby.size() == 2, "neighbor snapshot omitted entity-only item");
    const auto snapshot = map.At()->SnapshotAround();
    ecs::SpatialService::RemoveEntity(g_registry, b);
    Check(Spawn(b, 10001, 6500), "private-map relocation failed");
    nearby.clear(); auto saved = snapshot; saved.ForEach(collect);
    Check(nearby.size() == 1 && nearby[0] == a, "snapshot visited moved member");
    const auto stale = a; ecs::SpatialService::RemoveEntity(g_registry, a); g_registry.destroy(a);
    const auto recycled = Entity(); Check(Spawn(recycled), "recycled insertion failed");
    nearby.clear(); saved.ForEach(collect);
    Check(nearby.empty() && stale != recycled, "snapshot visited recycled generation");
    Check(!Spawn(recycled) && !Spawn(Entity(), 99), "duplicate/missing-map insertion accepted");
}
void VisibilityRoundTrip() {
    Reset(); MapFixture map;
    const auto item = Entity(), player = Entity(ecs::SpatialKind::Character);
    Check(Spawn(item) && Spawn(player), "item-first spawn failed");
    Check(Visible(item, player) && !Visible(player, item), "late viewer cannot see entity-only item");
    Check(packets.size() == 1 && packets[0].add && packets[0].source == item && packets[0].viewer == player,
        "incorrect directed item insertion");
    ecs::VisibilitySystem::Refresh(g_registry, player);
    Check(packets.size() == 1, "refresh resent unchanged item");
    g_registry.get<ecs::Position>(player).x = 15000;
    Check(map.At(15000)->InsertEntity(player), "player sector move failed");
    ecs::VisibilitySystem::Refresh(g_registry, player);
    Check(!Visible(item, player) && !packets.back().add && packets.back().source == item, "item leaving remove reversed/missed");
    g_registry.get<ecs::Position>(player).x = 100;
    Check(map.At()->InsertEntity(player), "player return failed");
    ecs::VisibilitySystem::Refresh(g_registry, player);
    Check(Visible(item, player), "returning player missed item");
    ecs::SpatialService::RemoveEntity(g_registry, item);
    Check(!map.At()->Contains(item) && !Visible(item, player) &&
        !g_registry.any_of<ecs::SpatialEntity, ecs::SectorPlacement, ecs::ViewActiveTag>(item),
        "item removal left spatial state");
    Check(!packets.back().add && packets.back().source == item, "ground remove not broadcast");
}
void ViewCallbacks() {
    for (int action = 0; action < 3; ++action) {
        Reset(); MapFixture map;
        const auto player = Entity(ecs::SpatialKind::Character), other = Entity(ecs::SpatialKind::Character);
        Check(Spawn(player) && Spawn(other), "viewer setup");
        const auto item = Entity();
        onPacket = [&](Packet packet) {
            if (!packet.add || packet.source != item) return;
            onPacket = {};
            ecs::SpatialService::RemoveEntity(g_registry, item);
            if (action == 0) g_registry.destroy(item);
            if (action == 1) Check(Spawn(item, 1, 15000), "callback relocation failed");
            if (action == 2) Check(Spawn(item), "callback same-location respawn failed");
        };
        Check(Spawn(item), "committed callback spawn reported failure");
        if (action == 0) Check(!g_registry.valid(item), "callback deletion missed");
        if (action == 1) Check(!Visible(item, player) && !Visible(item, other), "outer spawn republished relocated item");
        if (action == 2) Check(Visible(item, player) && Visible(item, other), "same-location respawn lost visibility");
    }
}
void PreparationAndPCs() {
    Reset(); MapFixture map;
    const auto npc = Entity(ecs::SpatialKind::Character);
    g_registry.remove<ecs::TagPC>(npc);
    g_registry.get<ecs::CharacterType>(npc).value = CHAR_TYPE_MONSTER;
    Check(Spawn(npc) && !awake.contains(npc), "NPC active without PCs");
    const auto pc = Entity(ecs::SpatialKind::Character);
    Check(Spawn(pc) && awake.contains(npc), "PC arrival failed to wake existing NPC");
    ecs::SpatialService::RemoveEntity(g_registry, pc);
    Check(!awake.contains(npc), "last PC departure failed to stop NPC");
    const auto item = Entity();
    Callback callback {[&](entt::registry& reg, entt::entity e) { if (e == item) reg.destroy(e); }};
    entt::scoped_connection connection = g_registry.on_construct<ecs::SectorPlacement>().connect<&Callback::Run>(callback);
    Check(!Spawn(item) && !g_registry.valid(item), "construction deletion ignored");
}

void LifetimeAndObservers() {
    Reset(); MapFixture map; MapFixture privateMap(10001);
    const auto a = Entity(ecs::SpatialKind::Character), b = Entity(ecs::SpatialKind::Character);
    Check(Spawn(a) && Spawn(b), "character spawn");
    Check(Visible(a, b) && Visible(b, a), "character edges are not mutual");
    const auto count = packets.size();
    for (int i = 0; i < 10; ++i) {
        ++g_registry.get<ecs::Position>(a).x;
        g_dispatcher.trigger<ecs::PositionChangedEvent>(ecs::PositionChangedEvent {a});
    }
    Check(packets.size() == count, "movement resent existing character insert");
    g_registry.emplace<ecs::StatusFlags>(a).isObserverMode = true;
    ecs::VisibilitySystem::Refresh(g_registry, a);
    Check(!Visible(a, b) && Visible(b, a), "observer directions wrong");
    g_registry.get<ecs::StatusFlags>(a).isObserverMode = false;
    ecs::VisibilitySystem::Refresh(g_registry, a);
    Check(Visible(a, b), "observer exit stayed hidden");
    const auto building = Entity(ecs::SpatialKind::Building), shop = Entity(ecs::SpatialKind::OfflineShop);
    Check(Spawn(building, 1, 12000) && Spawn(shop), "non-character spawn");
    Check(Visible(building, a) && Visible(shop, a), "building range exception / shop hidden");
    const auto beforeRename = packets.size();
    ecs::VisibilitySystem::Reencode(g_registry, shop);
    Check(packets.size() == beforeRename + 4 && packets.back().source == shop,
        "shop reencode failed to update both viewers");
    const auto foreign = Entity();
    Check(Spawn(foreign, 10001) && !Visible(foreign, a), "cross-map visibility leak");

    const auto npc = Entity(ecs::SpatialKind::Character);
    g_registry.remove<ecs::TagPC>(npc);
    g_registry.get<ecs::CharacterType>(npc).value = CHAR_TYPE_MONSTER;
    Check(Spawn(npc) && awake.contains(npc), "NPC did not wake");
    g_registry.destroy(a); g_registry.destroy(b);
    Check(!map.At()->Contains(a) && !map.At()->Contains(b) && !awake.contains(npc),
        "registry destruction left membership/PC count");
    const auto replacement = Entity(ecs::SpatialKind::Character);
    Check(Spawn(replacement) && awake.contains(npc), "recycled PC failed to wake NPC");
    ecs::SpatialService::RemoveEntity(g_registry, replacement);
    Check(map.At()->InsertEntity(replacement), "native character reinsert failed");
    ecs::VisibilitySystem::Refresh(g_registry, replacement);
    Check(g_registry.all_of<ecs::SpatialEntity, ecs::ViewActiveTag>(replacement) &&
        Visible(shop, replacement), "character reinsert did not restore visibility tags");
}

void RemovalCallbacksAndTeardown() {
    Reset(); MapFixture map;
    const auto player = Entity(ecs::SpatialKind::Character), item = Entity();
    Check(Spawn(player) && Spawn(item), "despawn setup");
    Callback callback {[&](entt::registry& reg, entt::entity e) {
        if (e != item) return;
        Check(!ecs::SpatialService::InsertEntity(reg, e, 1, 100, 100, 0),
            "on_destroy reinserted tag being destroyed");
    }};
    entt::scoped_connection connection =
        g_registry.on_destroy<ecs::SpatialEntity>().connect<&Callback::Run>(callback);
    onPacket = [&](Packet p) {
        if (p.add || p.source != item) return;
        onPacket = {};
        Check(Spawn(item), "despawn packet callback cannot respawn");
    };
    ecs::SpatialService::RemoveEntity(g_registry, item);
    Check(map.At()->Contains(item) && Visible(item, player) &&
        g_registry.all_of<ecs::SpatialEntity, ecs::ViewActiveTag>(item),
        "older despawn removed new spawn");
    connection.release();
    onPacket = {};
    const auto second = Entity();
    Check(Spawn(second), "teardown item spawn");
    onRetire = [&](entt::entity e) {
        Check(!Spawn(e), "destroying sector accepted reinsert");
        map.At()->Destroy(); // Reentrant region teardown is idempotent.
    };
    map.At()->Destroy();
    Check(!g_registry.valid(item) && !g_registry.valid(second) && retired == 2,
        "sector teardown skipped/doubled item retirement");
}

void PreparationMutationAndIteration() {
    Reset(); MapFixture map;
    const auto item = Entity();
    Callback callback {[&](entt::registry& reg, entt::entity e) {
        if (e == item) reg.get<ecs::ItemLocation>(e).window = INVENTORY;
    }};
    entt::scoped_connection connection =
        g_registry.on_construct<ecs::SectorPlacement>().connect<&Callback::Run>(callback);
    Check(!Spawn(item) && !map.At()->Contains(item) &&
        !g_registry.all_of<ecs::SectorPlacement>(item) &&
        g_registry.get<ecs::ItemLocation>(item).window == INVENTORY,
        "failed preparation overwrote storage or left phantom placement");
    connection.release();
    std::vector<entt::entity> original;
    for (int i = 0; i < 8; ++i) {
        auto e = Entity(); Check(Spawn(e), "map snapshot setup"); original.push_back(e);
    }
    int visits = 0;
    ecs::SpatialService::ForEachInMap(g_registry, 1, [&](entt::entity e) {
        Check(g_registry.valid(e), "map snapshot delivered stale handle");
        ++visits;
        if (visits != 1) return;
        for (auto old : original) if (old != e) g_registry.destroy(old);
        auto replacement = Entity(); Check(Spawn(replacement), "map callback replacement");
    });
    Check(visits == 1, "map iteration included deleted/replacement entities");
}

struct MovementProbe {
    std::vector<ecs::PositionChangedEvent> positions;
    std::vector<ecs::EvEntityMoved> moved;
    std::function<void(const ecs::PositionChangedEvent&)> onPosition;
    std::function<void(const ecs::EvEntityMoved&)> onMoved;
    entt::scoped_connection positionConnection, movedConnection;
    MovementProbe() :
        positionConnection(g_dispatcher.sink<ecs::PositionChangedEvent>().connect<&MovementProbe::Position>(*this)),
        movedConnection(g_dispatcher.sink<ecs::EvEntityMoved>().connect<&MovementProbe::Moved>(*this)) {}
    void Position(const ecs::PositionChangedEvent& event) {
        Check(g_registry.valid(event.entity), "position event has stale identity");
        const auto* tree = ecs::SectorOf(g_registry, event.entity);
        Check(tree && tree->Contains(event.entity) &&
            tree == ecs::SectorAt(event.newMapIndex, event.newX, event.newY),
            "position event preceded native sector commit");
        positions.push_back(event);
        if (onPosition) onPosition(event);
    }
    void Moved(const ecs::EvEntityMoved& event) {
        Check(g_registry.valid(event.entity), "movement event has stale identity");
        moved.push_back(event);
        if (onMoved) onMoved(event);
    }
};
entt::entity Moving(int x, int y, int targetX, int targetY, int speed = 200, bool npc = false) {
    const auto e = Entity(ecs::SpatialKind::Character);
    if (npc) {
        g_registry.remove<ecs::TagPC>(e);
        g_registry.emplace<ecs::TagNPC>(e);
        g_registry.get<ecs::CharacterType>(e).value = CHAR_TYPE_MONSTER;
    }
    g_registry.emplace<ecs::MovementState>(e);
    g_registry.emplace<ecs::MovementSpeed>(e, 100, speed);
    g_registry.emplace<ecs::CharacterRuntimeFlagsComponent>(e);
    Check(Spawn(e, 1, x, y), "native mover spawn");
    g_registry.emplace<ecs::MovementDestination>(e, targetX, targetY);
    Check(!g_registry.all_of<ecs::LegacyCharPtr>(e), "movement fixture acquired legacy character");
    return e;
}
void NativeMovement() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto e = Moving(6300, 100, 6700, 100);
    auto* oldTree = map.At(6300, 100);
    const auto revision = g_registry.get<ecs::SpatialRevision>(e).value;
    g_registry.get<ecs::Position>(e).z = 77;
    g_registry.get<ecs::PositionZ>(e).z = 77;
    g_registry.get<ecs::MovementState>(e).walkPreference = true;
    g_registry.remove<ecs::VIDComponent>(e); // Tick has no VID-index dependency.
    MovementSystem_Update(g_registry, 10);
    Check(g_registry.get<ecs::Position>(e).x == 6500 && !oldTree->Contains(e) &&
        map.At(6500, 100)->Contains(e), "native tick did not migrate sector");
    Check(g_registry.get<ecs::SpatialRevision>(e).value == revision + 1, "sector revision not advanced");
    Check(probe.positions.size() == 1 && probe.moved.size() == 1 &&
        probe.positions[0].oldX == 6300 && probe.positions[0].newX == 6500,
        "movement old/new event values or count wrong");
    MovementSystem_Update(g_registry, 20);
    const auto& state = g_registry.get<ecs::MovementState>(e);
    Check(g_registry.get<ecs::Position>(e).x == 6700 && !g_registry.all_of<ecs::MovementDestination>(e) &&
        state.lastMoveTime == 20 && state.stopTime == 20 && !state.isWalking &&
        !state.isNowWalking && state.moveDuration == 0 && state.walkPreference, "arrival state incorrect");
    Check(probe.positions.size() == 2 && probe.moved.size() == 2 && transitions.empty(),
        "arrival duplicated publication or transitioned a PC");
    Check(g_registry.get<ecs::Position>(e).z == 77 && g_registry.get<ecs::PositionZ>(e).z == 77,
        "movement changed altitude");
    MovementSystem_Update(g_registry, 30);
    Check(probe.positions.size() == 2, "stopped entity moved again");
}
void MovementVisibilityAndBounds() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto oldViewer = Entity(ecs::SpatialKind::Character), newViewer = Entity(ecs::SpatialKind::Character);
    Check(Spawn(oldViewer, 1, 500, 100) && Spawn(newViewer, 1, 10500, 100), "movement viewers");
    const auto e = Moving(1000, 100, 10000, 100, 20000);
    Check(Visible(e, oldViewer) && !Visible(e, newViewer), "initial movement visibility");
    packets.clear();
    MovementSystem_Update(g_registry, 50);
    Check(!Visible(e, oldViewer) && Visible(e, newViewer), "movement did not reconcile visibility");
    Check(std::count_if(packets.begin(), packets.end(), [&](const Packet& p) {
        return p.source == e && p.viewer == newViewer && p.add;
    }) == 1, "arrival duplicated insert packet");

    const auto edge = Moving(25500, 100, 25700, 100);
    const auto overflow = Moving(100, 500, INT32_MIN, 500, 1);
    const auto diagonal = Moving(500, 500, 800, 900, 100);
    const auto zeroSpeed = Moving(100, 700, 110, 700, 0);
    const auto detached = Moving(100, 900, 110, 900);
    ecs::SpatialService::RemoveEntity(g_registry, detached);
    const auto dead = Moving(100, 1100, 110, 1100);
    g_registry.emplace<ecs::DeadTag>(dead);
    MovementSystem_Update(g_registry, 60);
    Check(g_registry.get<ecs::Position>(edge).x == 25500 &&
        g_registry.get<ecs::MovementState>(edge).lastMoveTime == 0 &&
        map.At(25500, 100)->Contains(edge), "out-of-map step partially committed");
    Check(g_registry.get<ecs::Position>(overflow).x == 99, "destination subtraction overflowed");
    Check(g_registry.get<ecs::Position>(diagonal).x == 560 &&
        g_registry.get<ecs::Position>(diagonal).y == 580, "diagonal interpolation changed");
    Check(g_registry.get<ecs::Position>(zeroSpeed).x == 101, "nonpositive speed did not clamp");
    Check(g_registry.get<ecs::Position>(detached).x == 100 && !ecs::SectorOf(g_registry, detached),
        "tick respawned detached entity");
    Check(g_registry.get<ecs::Position>(dead).x == 100 && g_registry.all_of<ecs::DeadTag>(dead),
        "tick moved or revived dead character");
}
void MovementCallbackLifetime() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto e = Moving(100, 100, 300, 100);
    entt::entity replacement = entt::null;
    probe.onPosition = [&](const ecs::PositionChangedEvent& event) {
        if (event.entity != e) return;
        g_registry.destroy(e);
        replacement = Moving(500, 100, 700, 100);
    };
    MovementSystem_Update(g_registry, 80);
    Check(!g_registry.valid(e) && g_registry.valid(replacement) && probe.moved.empty() &&
        g_registry.get<ecs::Position>(replacement).x == 500,
        "tick used retired identity or advanced callback-created replacement");
    probe.onPosition = {};

    // A visibility packet can also retire the mover, after PositionChangedEvent.
    const auto viewer = Entity(ecs::SpatialKind::Character); Check(Spawn(viewer, 1, 10000, 100), "callback viewer");
    const auto packetMover = Moving(1000, 100, 10000, 100, 20000);
    onPacket = [&](Packet p) {
        if (p.add && p.source == packetMover && p.viewer == viewer) g_registry.destroy(packetMover);
    };
    MovementSystem_Update(g_registry, 90);
    Check(!g_registry.valid(packetMover) && std::none_of(probe.moved.begin(), probe.moved.end(),
        [&](const auto& event) { return event.entity == packetMover; }), "packet callback left stale tick continuation");
    onPacket = {};
}
void MovementCallbackRetarget() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto e = Moving(100, 100, 300, 100, 200, true);
    probe.onPosition = [&](const ecs::PositionChangedEvent& event) {
        if (event.entity != e) return;
        g_registry.emplace<ecs::MovementDestination>(e, 1000, 100);
        g_registry.get<ecs::MovementState>(e).moveDuration = 777;
    };
    MovementSystem_Update(g_registry, 100);
    Check(g_registry.get<ecs::MovementDestination>(e).x == 1000 &&
        g_registry.get<ecs::MovementState>(e).moveDuration == 777 &&
        transitions.empty() && probe.moved.empty(), "old arrival overrode callback retarget");

    probe.onPosition = {};
    probe.onMoved = [&](const ecs::EvEntityMoved& event) {
        if (event.entity != e) return;
        ecs::SpatialService::RemoveEntity(g_registry, e);
        Check(Spawn(e, 1, event.newX, event.newY), "same-location callback respawn");
    };
    g_registry.get<ecs::MovementDestination>(e).x = 500;
    MovementSystem_Update(g_registry, 110);
    Check(transitions.empty() && map.At(500, 100)->Contains(e), "old arrival transitioned new spawn");

    probe.onMoved = {};
    std::vector<entt::entity> original;
    for (int i = 0; i < 6; ++i) original.push_back(Moving(2000, 2000 + i * 100, 2200, 2000 + i * 100));
    bool changed = false;
    probe.onPosition = [&](const ecs::PositionChangedEvent& event) {
        if (changed) return;
        changed = true;
        for (auto other : original) {
            if (other == event.entity) continue;
            g_registry.get<ecs::MovementDestination>(other).x = 4000;
        }
    };
    const auto before = probe.moved.size();
    MovementSystem_Update(g_registry, 120);
    Check(changed && probe.moved.size() == before + 1, "tick advanced later retargeted snapshot entries");
}
void MovementArrivalAndPackets() {
    Reset(); MapFixture map; MovementProbe probe;
    const auto victim = Entity(ecs::SpatialKind::Character);
    const auto fighter = Moving(100, 100, 100, 100, 200, true);
    g_registry.emplace<ecs::CombatTarget>(fighter, victim, 0u);
    MovementSystem_Update(g_registry, 130);
    Check(g_registry.all_of<ecs::CombatActiveTag>(fighter) &&
        g_registry.get<ecs::CharacterRuntimeFlagsComponent>(fighter).position == POS_FIGHTING &&
        transitions.size() == 1 && transitions.back().second == ecs::AIFSMState::Battle &&
        probe.positions.empty() && probe.moved.empty(), "zero-distance native combat arrival");
    const auto coward = Moving(200, 100, 300, 100, 200, true);
    g_registry.emplace<ecs::CombatTarget>(coward, victim, 0u);
    g_registry.emplace<ecs::AIFlags>(coward).isCoward = true;
    MovementSystem_Update(g_registry, 140);
    Check(!g_registry.all_of<ecs::CombatTarget>(coward) &&
        g_registry.get<ecs::CharacterRuntimeFlagsComponent>(coward).position == POS_STANDING &&
        transitions.back().second == ecs::AIFSMState::Idle, "coward arrival did not enter idle");

    const auto e = Moving(1000, 1500, 1200, 1600);
    g_registry.get<ecs::MovementState>(e).moveDuration = 444;
    g_registry.get<ecs::CharacterRuntimeFlagsComponent>(e).rotation = 225;
    ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 2, 9, 9, 9);
    Check(movementPackets.size() == 1 && movementPackets.back().dwVID == g_registry.get<ecs::VIDComponent>(e).value &&
        movementPackets.back().lX == 1200 && movementPackets.back().lY == 1600 &&
        movementPackets.back().dwDuration == 444 && movementPackets.back().bRot == 45 &&
        movementPackets.back().dwTime == 123456, "entity movement packet lost destination/timing/rotation");
    g_registry.remove<ecs::MovementDestination>(e);
    g_registry.get<ecs::MovementState>(e).moveDuration = 0;
    ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 9, 42, 10);
    Check(movementPackets.back().lX == 1000 && movementPackets.back().lY == 1500 &&
        movementPackets.back().dwDuration == 0 && movementPackets.back().dwTime == 42 &&
        movementPackets.back().bRot == 10, "stopped movement packet fallback");
    ecs::MovementSystem::SendMovePacket(e, FUNC_ATTACK, 3, 2000, 2500, 99, 43, 12);
    Check(movementPackets.back().bHeader == HEADER_GC_MOVE && movementPackets.back().bFunc == FUNC_ATTACK &&
        movementPackets.back().bArg == 3 && movementPackets.back().lX == 2000 &&
        movementPackets.back().lY == 2500 && movementPackets.back().dwDuration == 99,
        "explicit movement packet fields changed");
    g_registry.destroy(e);
    ecs::MovementSystem::SendMovePacket(e, FUNC_WAIT, 0, 0, 0, 0);
    ecs::MovementSystem::SendMovePacket(entt::null, FUNC_WAIT, 0, 0, 0, 0);
    ecs::MovementSystem::SendMovePacket(Entity(), FUNC_WAIT, 0, 0, 0, 0);
    Check(movementPackets.size() == 3, "invalid/non-character entity emitted movement packet");
}

}
int main() {
    try {
        SECTREE_MANAGER maps; CHARACTER_MANAGER characters; DESC_MANAGER descriptors;
        ecs::VisibilitySystem::Init(g_registry);
        MembershipAndSnapshots(); VisibilityRoundTrip(); ViewCallbacks(); PreparationAndPCs();
        LifetimeAndObservers(); RemovalCallbacksAndTeardown(); PreparationMutationAndIteration();
        NativeMovement(); MovementVisibilityAndBounds(); MovementCallbackLifetime();
        MovementCallbackRetarget(); MovementArrivalAndPackets();
        ecs::VisibilitySystem::Shutdown(g_registry);
        std::cout << "Spatial checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
