#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/safebox.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <type_traits>

// Real safebox.cpp and CGrid, with entity-only items/owners. No CItem,
// CHARACTER, descriptor or database connection is instantiated.
entt::registry g_registry;
LPCLIENT_DESC db_clientdesc = nullptr;
int g_bItemCountLimit = 200;

namespace {
int checks = 0, saves = 0, flushes = 0, removed = 0, destroyed = 0;
int accountLookups = 0, descriptorLookups = 0;
bool rejectConsumption = false, rejectSave = false, rejectRemoval = false;
std::function<void(entt::entity)> onSave, onFlush, onRemove, onDestroy;
struct Player { bool pc = true; };
struct ItemData { uint8_t size = 1; };
void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
entt::entity PlayerEntity() {
    const auto entity = g_registry.create();
    g_registry.emplace<Player>(entity);
    return entity;
}
entt::entity Item(uint32_t count = 1, uint8_t size = 1) {
    const auto item = g_registry.create();
    g_registry.emplace<ecs::ItemIdentity>(item).vnum = 100;
    g_registry.emplace<ecs::ItemOwner>(item);
    g_registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation {RESERVED_WINDOW, 0});
    g_registry.emplace<ecs::ItemCount>(item).count = static_cast<int>(count);
    g_registry.emplace<ecs::ItemFlags>(item).flags = ITEM_FLAG_STACKABLE;
    g_registry.emplace<ItemData>(item).size = size;
    return item;
}
void Reset() {
    g_registry.clear();
    saves = flushes = removed = destroyed = accountLookups = descriptorLookups = 0;
    onSave = onFlush = onRemove = onDestroy = {};
    rejectConsumption = rejectSave = rejectRemoval = false;
    g_bItemCountLimit = 200;
}
class TestSafebox : public CSafebox {
public:
    using CSafebox::CSafebox;
    using CSafebox::__Destroy;
};
static_assert(!std::is_copy_constructible_v<CSafebox>);
static_assert(!std::is_copy_assignable_v<CSafebox>);
static_assert(!std::is_move_constructible_v<CSafebox>);
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger() {
    static auto logger = std::make_shared<spdlog::logger>("safebox-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("safebox-test");
    return logger;
}
void DESC::Packet(const void*, int) { throw std::runtime_error("unexpected live descriptor packet"); }
void CLIENT_DESC::DBPacket(uint8_t, uint32_t, const void*, uint32_t) { throw std::runtime_error("unexpected live DB packet"); }

namespace ecs::PlayerRuntime {
bool IsPC(entt::entity entity) {
    const auto* player = g_registry.valid(entity) ? g_registry.try_get<Player>(entity) : nullptr;
    return player && player->pc;
}
LPDESC GetDesc(entt::entity entity) {
    Check(IsPC(entity), "descriptor lookup used stale/non-player owner");
    ++descriptorLookups;
    return nullptr;
}
uint32_t GetAccountID(entt::entity entity) {
    Check(IsPC(entity), "account lookup used stale/non-player owner");
    ++accountLookups;
    return 0;
}
std::string_view GetName(entt::entity entity) {
    Check(IsPC(entity), "name lookup used stale/non-player owner");
    return "test-owner";
}
}

namespace ItemSystem {
bool IsValidItem(entt::entity item) { return g_registry.valid(item) && g_registry.all_of<ecs::ItemIdentity>(item); }
uint32_t GetItemID(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).id; }
uint32_t GetItemVnum(entt::entity item) { return g_registry.get<ecs::ItemIdentity>(item).vnum; }
uint8_t GetItemSize(entt::entity item) { return g_registry.get<ItemData>(item).size; }
uint32_t GetItemCount(entt::entity item) { return g_registry.get<ecs::ItemCount>(item).count; }
const char* GetItemName(entt::entity item) { Check(IsValidItem(item), "name read from destroyed item"); return "item"; }
int32_t GetItemFlags(entt::entity item) { return g_registry.get<ecs::ItemFlags>(item).flags; }
uint32_t GetItemAntiFlags(entt::entity) { return 0; }
uint32_t GetItemSocket(entt::entity, int) { return 0; }
TPlayerItemAttribute GetItemAttribute(entt::entity, int) { return {}; }
int16_t GetItemLockedAttributeIndex(entt::entity) { return -1; }
bool GetItemSkipSave(entt::entity item) { return g_registry.get<ecs::ItemFlags>(item).skipSave; }
bool SetItemSkipSave(entt::entity item, bool flag) { g_registry.get<ecs::ItemFlags>(item).skipSave = flag; return true; }
bool SetItemWindow(entt::entity item, uint8_t window) { g_registry.get<ecs::ItemLocation>(item).window = window; return true; }
uint8_t GetItemWindow(entt::entity item) { return g_registry.get<ecs::ItemLocation>(item).window; }
bool SetItemCell(entt::entity item, entt::entity owner, uint16_t cell) {
    g_registry.get<ecs::ItemLocation>(item).cell = cell;
    g_registry.get<ecs::ItemOwner>(item).owner = owner;
    return true;
}
bool SaveItemEcs(entt::entity item, bool) {
    Check(IsValidItem(item), "save received invalid item");
    ++saves;
    if (onSave) onSave(item);
    return !rejectSave;
}
bool FlushDelayedSaveEcs(entt::entity item) {
    Check(IsValidItem(item) && GetItemSkipSave(item), "teardown flush not protected from DB deletion");
    ++flushes;
    if (onFlush) onFlush(item);
    return true;
}
bool RemoveItemEcs(entt::entity item) {
    Check(IsValidItem(item), "detach received invalid item");
    if (rejectRemoval) return false;
    ++removed;
    g_registry.get<ecs::ItemOwner>(item).owner = entt::null;
    g_registry.get<ecs::ItemLocation>(item) = {RESERVED_WINDOW, 0};
    if (onRemove) onRemove(item);
    return true;
}
bool DestroyItemEntityEcs(entt::entity item, const char*) {
    Check(IsValidItem(item), "duplicate/stale destruction");
    Check(GetItemSkipSave(item) && g_registry.get<ecs::ItemOwner>(item).owner == entt::null,
        "teardown destroyed attached/persistent item");
    ++destroyed;
    if (onDestroy) onDestroy(item);
    if (IsValidItem(item)) g_registry.destroy(item);
    return true;
}
bool IsItemExchanging(entt::entity item) { return g_registry.get<ecs::ItemFlags>(item).exchanging; }
bool IsItemLocked(entt::entity item) { return g_registry.get<ecs::ItemFlags>(item).isLocked; }
bool ConsumeItemEcs(entt::entity item, uint32_t amount) {
    if (rejectConsumption) return false;
    auto& count = g_registry.get<ecs::ItemCount>(item).count;
    Check(amount > 0 && amount <= static_cast<uint32_t>(count), "invalid stack debit");
    count -= static_cast<int>(amount);
    if (!count) g_registry.destroy(item);
    return true;
}
bool AddItemCountEcs(entt::entity item, int amount) {
    g_registry.get<ecs::ItemCount>(item).count += amount;
    return true;
}
}

namespace {
void BasicStorage() {
    for (const uint8_t window : {SAFEBOX, MALL}) {
        Reset();
        const auto owner = PlayerEntity();
        TestSafebox box(owner, 3, 0);
        box.SetWindowMode(window);
        const auto item = Item(7, 2);
        Check(box.Add(0, item) && box.Get(0) == item, "entity-only add/get failed");
        Check(g_registry.get<ecs::ItemOwner>(item).owner == owner && ItemSystem::GetItemWindow(item) == window,
            "owner/window not stored as entities");
        Check(!box.IsEmpty(16, 1) && box.IsEmpty(32, 1), "multi-cell footprint incorrect");
        const auto other = Item();
        Check(!box.Add(0, other) && !box.Add(16, other) && !box.Add(1, item), "occupied/duplicate add accepted");
        Check(g_registry.get<ecs::ItemOwner>(other).owner == entt::null && saves == 1, "rejected add changed item");
        box.SetWindowMode(window == SAFEBOX ? MALL : SAFEBOX);
        Check(box.Get(0) == item, "nonempty storage changed window mode");
        Check(box.MoveItem(0, 1, 0) && box.Get(0) == entt::null && box.Get(1) == item, "move failed");
        Check(box.Remove(1) == item && box.Get(1) == entt::null && box.IsEmpty(1, 2), "remove failed");
        Check(box.Add(2, item), "reattachment failed");
        box.__Destroy();
        Check(!g_registry.valid(item) && g_registry.valid(other) && destroyed == 1 && flushes == 1,
            "teardown did not destroy only stored item");
        box.__Destroy();
        Check(destroyed == 1 && !box.Add(0, other), "repeated teardown changed storage");
    }
}

void BoundsAndResize() {
    for (const int height : {-1, 0, 28, INT_MAX}) {
        Reset();
        TestSafebox box(PlayerEntity(), height, 0);
        Check(!box.IsValidPosition(0) && !box.Add(0, Item()), "invalid height created a grid");
    }
    Reset();
    TestSafebox box(PlayerEntity(), 1, 0);
    const auto item = Item();
    Check(box.Add(15, item), "last valid initial cell rejected");
    for (const uint32_t position : {16u, 432u, UINT32_MAX})
        Check(!box.IsValidPosition(position) && box.Get(position) == entt::null &&
            !box.IsEmpty(position, 1) && box.Remove(position) == entt::null, "out-of-range cell accepted");
    box.ChangeSize(INT_MAX);
    Check(!box.IsValidPosition(16) && box.Get(15) == item, "invalid resize changed storage");
    box.ChangeSize(27);
    Check(box.IsValidPosition(431) && !box.IsValidPosition(432) && box.Get(15) == item,
        "max resize lost item or exceeded fixed slot array");
    for (uint32_t cell = 16; cell < SAFEBOX_MAX_NUM; ++cell)
        Check(box.IsEmpty(cell, 1), "expanded grid contained uninitialized cells");
    Check(!box.IsEmpty(431, 0) && !box.IsEmpty(431, 2), "invalid item footprint accepted");
    Check(!box.Add(431, Item(1, 2)) && !box.Add(1, Item(1, 0)), "invalid size item added");
    box.ChangeSize(1);
    Check(box.IsValidPosition(431), "storage unexpectedly shrank");
}

void StaleOwnersAndItems() {
    for (int scenario = 0; scenario < 6; ++scenario) {
        Reset();
        const auto owner = PlayerEntity();
        TestSafebox box(owner, 3, 0);
        const auto item = Item();
        Check(box.Add(0, item), "stale fixture add failed");
        if (scenario == 0) {
            g_registry.destroy(owner);
            const auto replacement = PlayerEntity();
            Check(replacement != owner && entt::to_entity(replacement) == entt::to_entity(owner), "owner not recycled");
            const auto lookups = descriptorLookups;
            Check(!box.Add(1, Item()) && !box.MoveItem(0, 1, 0) && box.Remove(0) == entt::null,
                "stale owner mutated storage");
            box.Save();
            Check(accountLookups == 0 && descriptorLookups == lookups, "replacement owner received old storage access");
        } else if (scenario == 1) {
            g_registry.destroy(item);
            const auto replacement = Item(9);
            Check(replacement != item && entt::to_entity(replacement) == entt::to_entity(item), "item not recycled");
        } else if (scenario == 2) g_registry.get<ecs::ItemOwner>(item).owner = PlayerEntity();
        else if (scenario == 3) g_registry.get<ecs::ItemLocation>(item).window = INVENTORY;
        else if (scenario == 4) g_registry.get<ecs::ItemLocation>(item).cell = 1;
        else g_registry.remove<ecs::ItemOwner>(item);
        if (scenario != 0) Check(box.Get(0) == entt::null && box.Remove(0) == entt::null, "foreign/stale slot returned an item");
        box.__Destroy();
        Check(destroyed == (scenario == 0 ? 1 : 0), "teardown destroyed migrated/recycled item or leaked stale-owner item");
    }
    Reset();
    TestSafebox nullOwner(entt::null, 3, 0);
    Check(!nullOwner.Add(0, Item()), "null owner accepted");
    const auto npc = PlayerEntity();
    g_registry.get<Player>(npc).pc = false;
    TestSafebox npcOwner(npc, 3, 0);
    Check(!npcOwner.Add(0, Item()), "NPC owner accepted");
    npcOwner.Save(); nullOwner.Save();
}

void ReentrantTeardown() {
    for (int stage = 0; stage < 3; ++stage) {
        Reset();
        const auto owner = PlayerEntity();
        TestSafebox box(owner, 3, 0);
        const auto first = Item(), second = Item(), unowned = Item();
        Check(box.Add(0, first) && box.Add(1, second), "reentrant fixture add failed");
        const auto callback = [&](entt::entity) {
            Check(box.Get(0) == entt::null && box.Get(1) == entt::null, "teardown published stale entries");
            box.__Destroy();
            box.ChangeSize(27);
            Check(!box.Add(2, unowned) && box.Remove(1) == entt::null && !box.MoveItem(0, 2, 0),
                "callback mutated retiring storage");
            box.Save();
        };
        if (stage == 0) onFlush = callback;
        else if (stage == 1) onRemove = callback;
        else onDestroy = callback;
        box.__Destroy();
        Check(destroyed == 2 && flushes == 2 && removed == 2 && g_registry.valid(unowned), "reentrant teardown double-destroyed");
    }
    for (int stage = 0; stage < 3; ++stage) {
        Reset();
        const auto owner = PlayerEntity();
        TestSafebox box(owner, 3, 0);
        const auto item = Item();
        Check(box.Add(0, item), "callback migration fixture failed");
        entt::entity replacement {entt::null};
        if (stage == 0) onFlush = [&](entt::entity e) {
            g_registry.destroy(e); replacement = Item(9);
        };
        else if (stage == 1) onFlush = [&](entt::entity e) {
            g_registry.get<ecs::ItemLocation>(e).window = INVENTORY;
        };
        else onRemove = [&](entt::entity e) {
            g_registry.get<ecs::ItemOwner>(e).owner = owner;
            g_registry.get<ecs::ItemLocation>(e).window = INVENTORY;
        };
        box.__Destroy();
        Check(destroyed == 0, "callback-migrated item was destroyed");
        Check(stage == 0 ? g_registry.valid(replacement) && ItemSystem::GetItemCount(replacement) == 9
            : g_registry.valid(item) && !ItemSystem::GetItemSkipSave(item), "migration damaged item/persistence flag");
    }
}

void PublicationAndDetachFailures() {
    for (int scenario = 0; scenario < 4; ++scenario) {
        Reset();
        const auto owner = PlayerEntity();
        TestSafebox box(owner, 3, 0);
        const auto item = Item();
        entt::entity replacement {entt::null};
        onSave = [&](entt::entity current) {
            Check(box.Get(0) == current && !box.IsEmpty(0, 1), "save observed partially attached storage");
            if (scenario == 0) rejectSave = true;
            if (scenario == 1) box.__Destroy();
            if (scenario == 2) {
                g_registry.destroy(current);
                replacement = Item(9);
            }
            if (scenario == 3) g_registry.destroy(owner);
        };
        Check(!box.Add(0, item), "failed/interrupted attachment reported success");
        Check(box.Get(0) == entt::null && descriptorLookups == 0, "interrupted add published stale data");
        if (scenario == 0 || scenario == 3)
            Check(box.IsEmpty(0, 1) && g_registry.get<ecs::ItemOwner>(item).owner == entt::null &&
                ItemSystem::GetItemWindow(item) == RESERVED_WINDOW, "failed add left orphaned ownership/grid cells");
        if (scenario == 2)
            Check(g_registry.valid(replacement) && ItemSystem::GetItemCount(replacement) == 9 && box.IsEmpty(0, 1),
                "add rollback damaged replacement generation");
    }
    for (int scenario = 0; scenario < 4; ++scenario) {
        Reset();
        const auto owner = PlayerEntity();
        TestSafebox box(owner, 3, 0);
        const auto item = Item();
        Check(box.Add(0, item), "remove fixture failed");
        rejectRemoval = scenario == 0;
        onRemove = [&](entt::entity current) {
            if (scenario == 1) g_registry.destroy(current);
            if (scenario == 2) g_registry.destroy(owner);
            if (scenario == 3) box.__Destroy();
        };
        Check(box.Remove(0) == entt::null, "failed/interrupted detach reported success");
        if (scenario == 0)
            Check(box.Get(0) == item && !box.IsEmpty(0, 1), "failed detach lost original slot");
        onRemove = {};
        rejectRemoval = false;
    }
    Reset();
    TestSafebox box(PlayerEntity(), 3, 0);
    const auto source = Item(5), destination = Item(10);
    Check(box.Add(0, source) && box.Add(1, destination), "failed full merge fixture failed");
    rejectConsumption = true;
    Check(!box.MoveItem(0, 1, 0) && box.Get(0) == source && !box.IsEmpty(0, 1) &&
        ItemSystem::GetItemCount(source) == 5 && ItemSystem::GetItemCount(destination) == 10,
        "failed full merge lost detached source");
}

void StackGuards() {
    for (int scenario = 0; scenario < 9; ++scenario) {
        Reset();
        TestSafebox box(PlayerEntity(), 3, 0);
        const auto source = Item(10), destination = Item(195);
        Check(box.Add(0, source) && box.Add(1, destination), "stack fixture add failed");
        if (scenario == 0) g_registry.get<ecs::ItemFlags>(source).isLocked = true;
        if (scenario == 1) g_registry.get<ecs::ItemFlags>(destination).isLocked = true;
        if (scenario == 2) g_registry.get<ecs::ItemFlags>(source).exchanging = true;
        if (scenario == 3) g_registry.get<ecs::ItemFlags>(destination).exchanging = true;
        if (scenario == 4) g_registry.get<ecs::ItemCount>(destination).count = 201;
        if (scenario == 5) g_bItemCountLimit = 0;
        if (scenario == 6) rejectConsumption = true;
        const auto originalDestination = ItemSystem::GetItemCount(destination);
        const bool result = box.MoveItem(0, 1, scenario == 7 ? UINT32_MAX : 0);
        if (scenario == 8)
            Check(result && ItemSystem::GetItemCount(source) == 5 && ItemSystem::GetItemCount(destination) == 200,
                "partial merge failed or exceeded cap");
        else Check(!result && ItemSystem::GetItemCount(source) == 10 &&
            ItemSystem::GetItemCount(destination) == originalDestination, "rejected merge changed counts");
    }
    Reset();
    TestSafebox box(PlayerEntity(), 3, 0);
    const auto source = Item(5), destination = Item(10);
    Check(box.Add(0, source) && box.Add(1, destination) && box.MoveItem(0, 1, 0), "full merge failed");
    Check(!g_registry.valid(source) && box.Get(0) == entt::null && box.IsEmpty(0, 1) &&
        ItemSystem::GetItemCount(destination) == 15, "full merge left a stale source");
    box.Save(); // No DB descriptor: must not dereference it.
}
}

int main() {
    try {
        BasicStorage(); BoundsAndResize(); StaleOwnersAndItems(); ReentrantTeardown();
        PublicationAndDetachFailures(); StackGuards();
        std::cout << "Safebox/mall lifecycle checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
