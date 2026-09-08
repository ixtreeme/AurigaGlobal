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
void Reset() { onPacket = {}; onRetire = {}; packets.clear(); awake.clear(); retired = 0; g_registry.clear(); }
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
}
int main() {
    try {
        SECTREE_MANAGER maps; CHARACTER_MANAGER characters; DESC_MANAGER descriptors;
        ecs::VisibilitySystem::Init(g_registry);
        MembershipAndSnapshots(); VisibilityRoundTrip(); ViewCallbacks(); PreparationAndPCs();
        LifetimeAndObservers(); RemovalCallbacksAndTeardown(); PreparationMutationAndIteration();
        ecs::VisibilitySystem::Shutdown(g_registry);
        std::cout << "Spatial checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
