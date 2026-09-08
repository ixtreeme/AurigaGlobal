#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/dragon_soul_table.h"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/inventory_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/DragonSoulSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
namespace {
int checks = 0, packets = 0;
bool connected = true;
std::function<void()> onPacket;
bool deckTest = false;
int activations = 0, deactivations = 0, itemSaves = 0, itemUpdates = 0;
uint32_t rejectedAffect = 0;
std::map<std::pair<entt::entity, uint16_t>, entt::entity> wear;
std::map<std::pair<entt::entity, uint32_t>, int> affects;
std::map<entt::entity, int> bonuses;
std::function<void(entt::entity)> onActivate, onDeactivate, onSave, onUpdate;
std::function<void(entt::entity, uint32_t, bool)> onAffect;
entt::entity signalOwner = entt::null, signalReplacement = entt::null;
void DestroyOnConstruct(entt::registry& registry, entt::entity entity) {
    if (entity != signalOwner) return;
    registry.destroy(entity);
    signalReplacement = registry.create();
}
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected deck/item/live service"); }
// Opaque token consumed only by the Packet service double, never dereferenced.
LPDESC DescriptorToken() { static int token; return reinterpret_cast<LPDESC>(&token); }
void Reset() {
    g_registry.clear(); connected = true; packets = 0; onPacket = {};
    deckTest = false; activations = deactivations = itemSaves = itemUpdates = 0;
    rejectedAffect = 0; wear.clear(); affects.clear(); bonuses.clear();
    onActivate = onDeactivate = onSave = onUpdate = {}; onAffect = {};
    signalOwner = signalReplacement = entt::null;
}
void LifecycleChecks() {
    Reset();
    const auto owner = g_registry.create(), npc = g_registry.create(), other = g_registry.create();
    Check(!DragonSoulSystem::CanRefine(owner), "window initially open");
    Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent>(owner), "read created window state");
    Check(!DragonSoulSystem::OpenRefineWindow(entt::null, npc), "null owner");
    Check(!DragonSoulSystem::OpenRefineWindow(owner, entt::null), "null opener");
    connected = false;
    Check(!DragonSoulSystem::OpenRefineWindow(owner, npc), "disconnected open");
    Check(!DragonSoulSystem::CanRefine(owner) && packets == 0, "failed open granted refine permission");
    Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent>(owner), "failed open created state");
    connected = true;
    Check(DragonSoulSystem::OpenRefineWindow(owner, npc), "entity-only open requires CHARACTER");
    Check(DragonSoulSystem::CanRefine(owner) && DragonSoulSystem::GetRefineWindowOpener(owner) == npc,
        "native opener identity");
    Check(g_registry.all_of<ecs::DirtyTag>(owner), "open not dirty");
    Check(DragonSoulSystem::OpenRefineWindow(owner, other) &&
        DragonSoulSystem::GetRefineWindowOpener(owner) == npc, "active opener replaced");
    Check(DragonSoulSystem::CloseRefineWindow(owner) && !DragonSoulSystem::CanRefine(owner), "close");
    Check(DragonSoulSystem::CloseRefineWindow(owner), "repeated close");
    Check(DragonSoulSystem::OpenRefineWindow(owner, owner), "self-open alchemy command");
    Check(DragonSoulSystem::GetRefineWindowOpener(owner) == owner, "self opener");
    Check(DragonSoulSystem::CloseRefineWindow(owner), "self-close");
    Check(DragonSoulSystem::OpenRefineWindow(owner, npc), "reopen");
    g_registry.destroy(npc);
    const auto replacement = g_registry.create();
    Check(entt::to_entity(npc) == entt::to_entity(replacement) && npc != replacement, "fixture did not recycle index");
    Check(!DragonSoulSystem::CanRefine(owner) &&
        DragonSoulSystem::GetRefineWindowOpener(owner) == entt::null, "stale opener inherited replacement");
    const int sent = packets;
    Check(!DragonSoulSystem::OpenRefineWindow(owner, npc) && packets == sent, "stale open sent packet");
    Check(DragonSoulSystem::OpenRefineWindow(owner, replacement) &&
        DragonSoulSystem::GetRefineWindowOpener(owner) == replacement, "stale slot not replaced by live opener");
    g_registry.destroy(owner);
    const auto newOwner = g_registry.create();
    Check(owner != newOwner && !DragonSoulSystem::CanRefine(owner), "stale owner");
    Check(!DragonSoulSystem::CloseRefineWindow(owner), "closed stale owner");
    Check(!DragonSoulSystem::OpenRefineWindow(owner, replacement), "opened recycled owner");
    Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent>(newOwner), "replacement owner mutated");
}
void CallbackChecks() {
    for (int mode = 0; mode < 4; ++mode) {
        Reset();
        const auto owner = g_registry.create(), npc = g_registry.create();
        entt::entity replacement = entt::null;
        onPacket = [&] {
            Check(DragonSoulSystem::CanRefine(owner), "packet published before state");
            if (mode == 0) {
                g_registry.destroy(owner); replacement = g_registry.create();
            } else if (mode == 1) {
                g_registry.destroy(npc); replacement = g_registry.create();
            } else if (mode == 2) {
                DragonSoulSystem::CloseRefineWindow(owner);
            } else {
                // Force component pool growth while publishing the packet.
                for (int i = 0; i < 4096; ++i)
                    g_registry.emplace<ecs::DragonSoulRuntimeStateComponent>(g_registry.create());
            }
        };
        Check(DragonSoulSystem::OpenRefineWindow(owner, npc) == (mode == 3), "callback invalidation ignored");
        if (replacement != entt::null)
            Check(!g_registry.all_of<ecs::DragonSoulRuntimeStateComponent, ecs::DirtyTag>(replacement),
                "callback replacement inherited state");
        if (mode == 3) Check(DragonSoulSystem::GetRefineWindowOpener(owner) == npc, "pool growth lost opener");
    }
}
}
namespace ecs::PlayerRuntime {
LPDESC GetDesc(entt::entity e) { Check(g_registry.valid(e), "invalid descriptor lookup"); return connected ? DescriptorToken() : nullptr; }
}
void DESC::Packet(const void* data, int size) {
    Check(this == DescriptorToken() && data && size == sizeof(TPacketGCDragonSoulRefine), "unexpected packet");
    const auto& packet = *static_cast<const TPacketGCDragonSoulRefine*>(data);
    Check(packet.header == HEADER_GC_DRAGON_SOUL_REFINE && packet.bSubType == DS_SUB_HEADER_OPEN, "wrong open packet");
    ++packets;
    if (onPacket) onPacket();
}
namespace ItemSystem {
entt::entity GetInventoryItem(entt::entity owner, uint16_t cell) {
    Check(deckTest && g_registry.valid(owner), "invalid deck inventory lookup");
    const auto found = wear.find({owner, cell});
    return found == wear.end() ? entt::null : found->second;
}
bool IsValidItem(entt::entity item) { return g_registry.valid(item) && g_registry.all_of<ecs::ItemIdentity>(item); }
bool IsDragonSoulItem(entt::entity item) { return IsValidItem(item); }
entt::entity GetItemOwner(entt::entity item) { return g_registry.get<ecs::ItemOwner>(item).owner; }
bool IsItemEquipped(entt::entity item) { return g_registry.get<ecs::ItemEquipped>(item).equipped; }
uint16_t GetItemCell(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).cell; }
void SaveItem(entt::entity item) {
    Check(deckTest && IsValidItem(item), "stale deck save"); ++itemSaves;
    if (onSave) onSave(item);
}
bool SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected(); }
uint32_t GetItemVnum(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).vnum; }
}
namespace AffectSystem {
CAffect* FindAffect(entt::entity owner, uint32_t type, uint8_t) {
    if (!deckTest) Unexpected();
    static CAffect marker {};
    return affects.contains({owner, type}) ? &marker : nullptr;
}
bool AddAffect(entt::entity owner, uint32_t type, uint8_t, int32_t value, uint32_t, int32_t, int32_t, bool, bool) {
    Check(deckTest && g_registry.valid(owner), "stale deck affect addition");
    if (rejectedAffect == type) return false;
    affects[{owner, type}] = value;
    if (onAffect) onAffect(owner, type, true);
    return true;
}
bool RemoveAffect(entt::entity owner, uint32_t type) {
    Check(deckTest && g_registry.valid(owner), "stale deck affect removal");
    const bool removed = affects.erase({owner, type}) != 0;
    if (removed && onAffect) onAffect(owner, type, false);
    return removed;
}
}
DSManager::DSManager() = default;
DSManager::~DSManager() = default;
DragonSoulTable::~DragonSoulTable() = default;
bool DSManager::ActivateDragonSoul(entt::entity item) {
    Check(deckTest && ItemSystem::IsValidItem(item), "stale deck stone activation");
    const auto owner = ItemSystem::GetItemOwner(item);
    const int cell = ItemSystem::GetItemCell(item);
    Check(DragonSoulSystem::GetActiveDeck(owner) == (cell - DRAGON_SOUL_EQUIP_SLOT_START) / DS_SLOT_MAX,
        "stone activated before deck state was committed");
    if (!IsTimeLeftDragonSoul(item)) return false;
    auto& flag = g_registry.get<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX];
    if (!flag) { flag = 1; ++activations; ++bonuses[owner]; }
    if (onActivate) onActivate(item);
    return ItemSystem::IsValidItem(item) && IsActiveDragonSoul(item);
}
bool DSManager::DeactivateDragonSoul(entt::entity item, bool skipRefresh) {
    Check(deckTest && ItemSystem::IsValidItem(item) && skipRefresh, "stale/recursive deck deactivation");
    auto& flag = g_registry.get<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX];
    if (!flag) return false;
    flag = 0; ++deactivations; --bonuses[ItemSystem::GetItemOwner(item)];
    if (onDeactivate) onDeactivate(item);
    return true;
}
bool DSManager::IsActiveDragonSoul(entt::entity item) const {
    return ItemSystem::IsValidItem(item) && g_registry.get<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX] != 0;
}
bool DSManager::IsTimeLeftDragonSoul(entt::entity item) const {
    return ItemSystem::IsValidItem(item) && g_registry.get<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_REMAIN_SEC] > 0;
}
void ecs::ItemNetworkSystem::SendItemUpdate(entt::registry&, entt::entity item) {
    Check(deckTest && ItemSystem::IsValidItem(item), "stale deck item publication"); ++itemUpdates;
    if (onUpdate) onUpdate(item);
}
time_t get_global_time() { return 1000; }


namespace {
struct DeckFixture {
    entt::entity owner;
    std::array<entt::entity, DS_SLOT_MAX * DRAGON_SOUL_DECK_MAX_NUM> items;
    DeckFixture() {
        Reset(); deckTest = true; owner = g_registry.create();
        for (size_t i = 0; i < items.size(); ++i) items[i] = Stone(DRAGON_SOUL_EQUIP_SLOT_START + i);
    }
    entt::entity Stone(uint16_t cell) {
        const auto item = g_registry.create();
        const uint32_t vnum = 110000 +
#ifdef ENABLE_DS_GRADE_MYTH
            DRAGON_SOUL_GRADE_MYTH
#else
            DRAGON_SOUL_GRADE_LEGENDARY
#endif
            * 1000 + DRAGON_SOUL_STEP_HIGHEST * 100 + 60;
        g_registry.emplace<ecs::ItemIdentity>(item, ecs::ItemIdentity{static_cast<uint32_t>(cell), vnum, vnum});
        g_registry.emplace<ecs::ItemOwner>(item, ecs::ItemOwner{owner});
        g_registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation{EQUIPMENT, cell});
        g_registry.emplace<ecs::ItemEquipped>(item).equipped = true;
        g_registry.emplace<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_REMAIN_SEC] = 120;
        wear[{owner, cell}] = item;
        return item;
    }
    void Remove(entt::entity item) {
        if (DSManager::instance().IsActiveDragonSoul(item)) DSManager::instance().DeactivateDragonSoul(item, true);
        wear.erase({owner, ItemSystem::GetItemCell(item)}); g_registry.destroy(item);
    }
    bool Has(uint32_t affect) { return affects.contains({owner, affect}); }
};

void DeckTransitions() {
    DeckFixture f;
    Check(DragonSoulSystem::GetActiveDeck(f.owner) == -1 && !DragonSoulSystem::IsDeckActivated(f.owner) &&
        !g_registry.all_of<ecs::DragonSoulRuntimeStateComponent>(f.owner), "deck query created component");
    for (int deck : std::array<int, 4>{-1, DRAGON_SOUL_DECK_MAX_NUM, INT_MIN, INT_MAX})
        Check(!DragonSoulSystem::ActivateDeck(f.owner, deck), "invalid deck accepted");
    Check(DragonSoulSystem::ActivateDeck(f.owner, 0) && bonuses[f.owner] == DS_SLOT_MAX &&
        f.Has(AFFECT_DRAGON_SOUL_DECK_0), "first deck activation");
#ifdef ENABLE_DS_SET
    Check(affects[{f.owner, AFFECT_DS_SET}] == 1 && f.Has(AFFECT_DS_BNS3), "complete set bonus missing");
#endif
    Check(DragonSoulSystem::ActivateDeck(f.owner, 0) && activations == DS_SLOT_MAX, "repeated deck applied twice");
    Check(DragonSoulSystem::ActivateDeck(f.owner, 1) && bonuses[f.owner] == DS_SLOT_MAX &&
        deactivations == DS_SLOT_MAX && !f.Has(AFFECT_DRAGON_SOUL_DECK_0), "deck switch kept old bonuses");
    DragonSoulSystem::CleanUp(f.owner);
    Check(bonuses[f.owner] == 0 && DragonSoulSystem::GetActiveDeck(f.owner) == 1 &&
        f.Has(AFFECT_DRAGON_SOUL_DECK_1), "logout lost saved deck selection");
    DragonSoulSystem::CleanUp(f.owner);
    Check(deactivations == 2 * DS_SLOT_MAX, "cleanup deducted twice");
    DragonSoulSystem::DeactivateAll(f.owner);
    Check(DragonSoulSystem::GetActiveDeck(f.owner) == -1 && !f.Has(AFFECT_DS_BNS3) &&
        !f.Has(AFFECT_DRAGON_SOUL_DECK_1), "explicit shutdown did not clear affects");
}

void DeckSetValidation() {
#ifdef ENABLE_DS_SET
    for (int mode = 0; mode < 4; ++mode) {
        DeckFixture f;
        const auto item = f.items[DS_SLOT_MAX - 1];
        if (mode == 0) f.Remove(item);
        if (mode == 1) g_registry.get<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_REMAIN_SEC] = 0;
        if (mode == 2) g_registry.get<ecs::ItemIdentity>(item).vnum -= 1000;
        if (mode == 3) wear[{f.owner, ItemSystem::GetItemCell(item)}] = entt::null;
        Check(DragonSoulSystem::ActivateDeck(f.owner, 0) && affects[{f.owner, AFFECT_DS_SET}] == 0 &&
            !f.Has(AFFECT_DS_BNS1) && !f.Has(AFFECT_DS_BNS2) && !f.Has(AFFECT_DS_BNS3),
            "partial/expired/invalid set granted complete bonus");
    }
    {
        DeckFixture f;
        onAffect = [&](entt::entity, uint32_t type, bool add) {
            if (add && type == AFFECT_DS_SET) f.Remove(f.items[0]);
        };
        Check(!DragonSoulSystem::ActivateDeck(f.owner, 0) && bonuses[f.owner] == 0 &&
            !f.Has(AFFECT_DS_BNS3) && DragonSoulSystem::GetActiveDeck(f.owner) == -1,
            "set bonus publication ignored removed stone");
    }
#endif
}

void DeckCancellation() {
    for (bool clear : {false, true}) {
        DeckFixture f;
        onActivate = [&](entt::entity) {
            Check(!DragonSoulSystem::ActivateDeck(f.owner, 1), "recursive deck activation accepted");
            if (clear) DragonSoulSystem::DeactivateAll(f.owner);
            else DragonSoulSystem::CleanUp(f.owner);
        };
        Check(!DragonSoulSystem::ActivateDeck(f.owner, 0) && bonuses[f.owner] == 0,
            "cancelled deck kept partial item bonuses");
        Check(DragonSoulSystem::GetActiveDeck(f.owner) == (clear ? -1 : 0) &&
            f.Has(AFFECT_DRAGON_SOUL_DECK_0) == !clear, "cancellation lost logout/disable distinction");
    }
    {
        DeckFixture f;
        rejectedAffect = AFFECT_DRAGON_SOUL_DECK_0;
        Check(!DragonSoulSystem::ActivateDeck(f.owner, 0) && activations == 0 &&
            DragonSoulSystem::GetActiveDeck(f.owner) == -1, "rejected affect left selected deck");
    }
    {
        DeckFixture f;
        onAffect = [&](entt::entity owner, uint32_t type, bool add) {
            if (!add || type != AFFECT_DRAGON_SOUL_DECK_0) return;
            g_registry.remove<ecs::DragonSoulRuntimeStateComponent>(owner);
            g_registry.emplace<ecs::DragonSoulRuntimeStateComponent>(owner).activeDeck = 1;
        };
        Check(!DragonSoulSystem::ActivateDeck(f.owner, 0) && DragonSoulSystem::GetActiveDeck(f.owner) == 1 &&
            activations == 0, "old operation overwrote replacement component");
    }
    {
        DeckFixture f;
        onActivate = [&](entt::entity) { g_registry.destroy(f.owner); };
        Check(!DragonSoulSystem::ActivateDeck(f.owner, 0), "destroyed owner accepted after activation callback");
    }
    {
        DeckFixture f;
        bool once = false; entt::entity replacement = entt::null;
        onActivate = [&](entt::entity) {
            if (once) return; once = true;
            f.Remove(f.items[1]); replacement = f.Stone(DRAGON_SOUL_EQUIP_SLOT_START + 1);
        };
        Check(DragonSoulSystem::ActivateDeck(f.owner, 0) && !DSManager::instance().IsActiveDragonSoul(replacement),
            "old snapshot activated replacement item");
    }
}

void DeckHydrationAndSignals() {
    {
        DeckFixture f;
        affects[{f.owner, AFFECT_DRAGON_SOUL_DECK_0}] = 0;
        for (const auto item : f.items)
            g_registry.get<ecs::ItemSockets>(item).sockets[ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX] = 1;
        onSave = [&](entt::entity) {
            Check(DragonSoulSystem::GetActiveDeck(f.owner) == -1, "hydration published selected deck too early");
            for (const auto item : f.items) Check(!DSManager::instance().IsActiveDragonSoul(item), "partial hydration published");
        };
        DragonSoulSystem::Initialize(f.owner);
        Check(bonuses[f.owner] == DS_SLOT_MAX && DragonSoulSystem::GetActiveDeck(f.owner) == 0 &&
            itemSaves == static_cast<int>(f.items.size()), "login hydration failed");
        DragonSoulSystem::Initialize(f.owner);
        Check(bonuses[f.owner] == DS_SLOT_MAX, "repeated initialization doubled bonuses");
    }
    for (bool dirty : {false, true}) {
        DeckFixture f; signalOwner = f.owner;
        entt::scoped_connection connection = dirty ?
            g_registry.on_construct<ecs::DirtyTag>().connect<&DestroyOnConstruct>() :
            g_registry.on_construct<ecs::DragonSoulRuntimeStateComponent>().connect<&DestroyOnConstruct>();
        Check(!DragonSoulSystem::ActivateDeck(f.owner, 0) && !g_registry.valid(f.owner) &&
            g_registry.valid(signalReplacement), "construction callback destruction ignored");
        Check(!g_registry.any_of<ecs::DragonSoulRuntimeStateComponent, ecs::DirtyTag>(signalReplacement),
            "replacement inherited destroyed deck state");
    }
    {
        Reset(); signalOwner = g_registry.create(); const auto npc = g_registry.create();
        entt::scoped_connection connection = g_registry.on_construct<ecs::DragonSoulRuntimeStateComponent>().connect<&DestroyOnConstruct>();
        Check(!DragonSoulSystem::OpenRefineWindow(signalOwner, npc) && packets == 0,
            "refine window used destroyed construction result");
    }
}
}

int main() {
    try {
        DSManager manager;
        LifecycleChecks(); CallbackChecks();
        DeckTransitions(); DeckSetValidation(); DeckCancellation(); DeckHydrationAndSignals();
        std::cout << "Refine window checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
