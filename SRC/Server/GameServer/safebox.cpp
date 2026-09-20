#include "stdafx.h"
#include <algorithm>
#include <utility>
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "constants.h"
#include "safebox.h"
#include "packet.h"
#include "ecs/Registry.hpp"
#include "ecs/components/inventory_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "desc_client.h"
#include "config.h"

namespace
{
using Storage = ecs::SafeboxStorageComponent;
constexpr int GridWidth = Storage::Width;
constexpr int MaxHeight = Storage::MaxHeight;

void EnsureLifecycle();

Storage* FindStorage(entt::entity storage)
{
    if (storage == entt::null || !g_registry.valid(storage))
        return nullptr;
    return g_registry.try_get<Storage>(storage);
}

// Operations must never act on a storage whose teardown has begun.
Storage* LiveStorage(entt::entity storage)
{
    auto* state = FindStorage(storage);
    return state && !state->destroying ? state : nullptr;
}

bool OwnsItem(entt::entity owner, uint8_t windowMode, entt::entity item, uint32_t cell)
{
    if (!ItemSystem::IsValidItem(item))
        return false;
    const auto* itemOwner = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    // Compare the stored versioned owner even during teardown after logout.
    // GetItemOwner intentionally hides invalid owners and cannot do this check.
    return itemOwner && itemOwner->owner == owner && location &&
        location->window == windowMode && location->cell == cell;
}

bool OwnsItem(const Storage& state, entt::entity item, uint32_t cell)
{
    return OwnsItem(state.owner, state.windowMode, item, cell);
}

uint32_t GridSize(const Storage& state)
{
    return state.size > 0 ? static_cast<uint32_t>(GridWidth * state.size) : 0;
}

bool GridIsEmpty(const Storage& state, uint32_t cell, uint8_t size)
{
    if (state.size <= 0 || size == 0)
        return false;
    if (cell >= static_cast<uint32_t>(GridWidth * state.size))
        return false;
    const int row = static_cast<int>(cell) / GridWidth;
    const int column = static_cast<int>(cell) % GridWidth;
    if (row + size > state.size || column + 1 > GridWidth)
        return false;
    for (int y = 0; y < size; ++y)
        if (state.occupied[cell + y * GridWidth])
            return false;
    return true;
}

bool GridPut(Storage& state, uint32_t cell, uint8_t size)
{
    if (!GridIsEmpty(state, cell, size))
        return false;
    for (int y = 0; y < size; ++y)
        state.occupied[cell + y * GridWidth] = 1;
    return true;
}

void GridGet(Storage& state, uint32_t cell, uint8_t size)
{
    if (state.size <= 0 || size == 0 || cell >= static_cast<uint32_t>(GridWidth * state.size))
        return;
    for (int y = 0; y < size; ++y)
        state.occupied[cell + y * GridWidth] = 0;
}

bool FitsGrid(const Storage& state, uint32_t cell, uint8_t size)
{
    return cell < state.items.size() && cell < GridSize(state) && size > 0 &&
        size <= state.size - static_cast<int>(cell / GridWidth);
}

struct ClosingStorageGuard {
    entt::entity owner;
    uint8_t window;
    ~ClosingStorageGuard() {
        if (!g_registry.valid(owner)) return;
        if (auto* refs = g_registry.try_get<ecs::SafeboxRef>(owner))
            (window == SAFEBOX ? refs->closingSafebox : refs->closingMall) = false;
    }
};

// Counts operations that hold the storage across service callbacks. An owner
// destruction during such a callback only marks the storage for retirement:
// the operation finishes (and can still roll back), then the last lease
// retires the storage. This is the replacement for the shared_ptr lease that
// kept a closing container alive.
struct StorageLease {
    entt::entity entity { entt::null };
    Storage* state { nullptr };

    explicit StorageLease(entt::entity storage) : entity(storage)
    {
        state = LiveStorage(storage);
        if (state)
            ++state->operations;
    }
    ~StorageLease()
    {
        if (!state)
            return;
        state = FindStorage(entity);
        if (!state)
            return;
        if (state->operations > 0)
            --state->operations;
        if (state->operations == 0 && state->retirePending && !state->destroying)
            SafeboxSystem::Destroy(entity);
    }
    StorageLease(const StorageLease&) = delete;
    StorageLease& operator=(const StorageLease&) = delete;
};

// Implicit release: the owner component is going away without a Close. If an
// operation is in flight the storage is retired when that operation ends.
void ReleaseStorage(entt::entity storage)
{
    auto* state = FindStorage(storage);
    if (!state || state->destroying)
        return;
    if (state->operations > 0)
    {
        state->retirePending = true;
        return;
    }
    SafeboxSystem::Destroy(storage);
}

// A destroyed owner must not leave its storage entities behind. This mirrors
// the shared_ptr release that SafeboxRef destruction used to perform.
void OwnerDestroyed(entt::registry& registry, entt::entity owner)
{
    auto& refs = registry.get<ecs::SafeboxRef>(owner);
    const entt::entity storages[2] = {refs.safebox, refs.mall};
    refs.safebox = entt::null;
    refs.mall = entt::null;
    refs.closingSafebox = refs.closingMall = true;
    for (const entt::entity storage : storages)
        ReleaseStorage(storage);
}

void EnsureLifecycle()
{
    struct HooksInstalled {};
    if (g_registry.ctx().contains<HooksInstalled>())
        return;
    g_registry.on_destroy<ecs::SafeboxRef>().connect<&OwnerDestroyed>();
    g_registry.ctx().emplace<HooksInstalled>();
}
} // namespace

namespace SafeboxSystem {
entt::entity Get(entt::entity owner, uint8_t window)
{
    if (!g_registry.valid(owner) || (window != SAFEBOX && window != MALL)) return entt::null;
    const auto* refs = g_registry.try_get<ecs::SafeboxRef>(owner);
    if (!refs) return entt::null;
    const entt::entity storage = window == SAFEBOX ? refs->safebox : refs->mall;
    const auto* state = FindStorage(storage);
    return state && state->owner == owner && !state->destroying ? storage : entt::null;
}

entt::entity Open(entt::entity owner, uint8_t window, int height, uint32_t gold)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || (window != SAFEBOX && window != MALL)) return entt::null;
    EnsureLifecycle();
    auto& refs = g_registry.get_or_emplace<ecs::SafeboxRef>(owner);
    if (window == SAFEBOX ? refs.closingSafebox : refs.closingMall) return entt::null;
    auto& slot = window == SAFEBOX ? refs.safebox : refs.mall;
    if (slot != entt::null)
    {
        const auto* current = FindStorage(slot);
        if (current && current->owner == owner && !current->destroying)
            return slot;
        slot = entt::null;
    }
    const entt::entity storage = g_registry.create();
    auto& state = g_registry.emplace<Storage>(storage);
    state.owner = owner;
    state.windowMode = window;
    state.gold = static_cast<int32_t>(gold);
    ChangeSize(storage, height);
    slot = storage;
    return storage;
}

void Close(entt::entity owner, uint8_t window, bool save)
{
    if (!g_registry.valid(owner) || (window != SAFEBOX && window != MALL)) return;
    auto* refs = g_registry.try_get<ecs::SafeboxRef>(owner);
    if (!refs) return;
    bool& closing = window == SAFEBOX ? refs->closingSafebox : refs->closingMall;
    if (closing) return;
    closing = true;
    ClosingStorageGuard guard {owner, window};
    // Unpublish before saving, then retire contents immediately. Operations
    // already in flight revalidate the entity and become no-ops.
    const entt::entity storage = std::exchange(window == SAFEBOX ? refs->safebox : refs->mall, entt::null);
    if (storage == entt::null) return;
    if (save) Save(storage);
    Destroy(storage);
}

void SetWindowMode(entt::entity storage, uint8_t window)
{
    auto* state = LiveStorage(storage);
    if (!state || (window != SAFEBOX && window != MALL) ||
        std::any_of(state->items.begin(), state->items.end(), [](entt::entity item) { return item != entt::null; }))
        return;
    state->windowMode = window;
}

void Destroy(entt::entity storage)
{
    auto* state = FindStorage(storage);
    if (!state || state->destroying)
        return;
    state->destroying = true;
    const auto items = state->items;
    state->items.fill(entt::null);
    state->occupied.fill(0);
    // The teardown callbacks must not see a partially published container.
    const entt::entity chrOwner = state->owner;
    const uint8_t windowMode = state->windowMode;
    for (size_t cell = 0; cell < items.size(); ++cell)
    {
        const auto item = items[cell];
        if (!OwnsItem(chrOwner, windowMode, item, static_cast<uint32_t>(cell)))
            continue;

        const bool previousSkipSave = ItemSystem::GetItemSkipSave(item);
        ItemSystem::SetItemSkipSave(item, true);
        ItemSystem::FlushDelayedSaveEcs(item);
        if (!OwnsItem(chrOwner, windowMode, item, static_cast<uint32_t>(cell)))
        {
            if (ItemSystem::IsValidItem(item))
                ItemSystem::SetItemSkipSave(item, previousSkipSave);
            continue;
        }
        const bool removed = ItemSystem::RemoveItemEcs(item);
        if (!ItemSystem::IsValidItem(item))
            continue;
        const auto* itemOwner = g_registry.try_get<ecs::ItemOwner>(item);
        if (!removed || !itemOwner || itemOwner->owner != entt::null ||
            ItemSystem::GetItemWindow(item) != RESERVED_WINDOW)
        {
            ItemSystem::SetItemSkipSave(item, previousSkipSave);
            LOG_ERROR("SAFEBOX: item {} changed ownership during teardown; not destroying", ItemSystem::GetItemID(item));
            continue;
        }
        ItemSystem::DestroyItemEntityEcs(item, "SAFEBOX_DESTRUCT");
    }
    if (g_registry.valid(storage))
    {
        const auto* current = g_registry.try_get<Storage>(storage);
        if (current && current->destroying)
            g_registry.destroy(storage);
    }
}

bool Add(entt::entity storage, uint32_t cell, entt::entity item)
{
    StorageLease lease {storage};
    auto* state = lease.state;
    const entt::entity chrOwner = state ? state->owner : entt::null;
    const uint8_t windowMode = state ? state->windowMode : static_cast<uint8_t>(SAFEBOX);
    if (!state || !ecs::PlayerRuntime::IsPC(chrOwner) || !IsValidPosition(storage, cell) || !ItemSystem::IsValidItem(item))
    {
        LOG_ERROR("SAFEBOX: item on wrong position at {}", cell);
        return false;
    }
    const auto* itemOwner = g_registry.try_get<ecs::ItemOwner>(item);
    const uint8_t size = ItemSystem::GetItemSize(item);
    if ((itemOwner && itemOwner->owner != entt::null) ||
        std::find(state->items.begin(), state->items.end(), item) != state->items.end() || !IsEmpty(storage, cell, size))
        return false;

    if (!GridPut(*state, cell, size))
        return false;
    const auto oldOwner = itemOwner ? *itemOwner : ecs::ItemOwner {};
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    const auto oldLocation = location ? *location : ecs::ItemLocation {RESERVED_WINDOW, 0};
    ItemSystem::SetItemWindow(item, windowMode);
    ItemSystem::SetItemCell(item, chrOwner, cell);
    state->items[cell] = item;
    const bool saved = ItemSystem::SaveItemEcs(item);
    // Saving can run callbacks that close or replace this storage.
    state = FindStorage(storage);
    if (!saved || !state || state->destroying || !OwnsItem(chrOwner, windowMode, item, cell) || !ecs::PlayerRuntime::IsPC(chrOwner))
    {
        // Never restore over a moved/destroyed entity or a replacement slot.
        if (state && state->items[cell] == item)
        {
            state->items[cell] = entt::null;
            GridGet(*state, cell, size);
        }
        if (OwnsItem(chrOwner, windowMode, item, cell))
        {
            g_registry.get<ecs::ItemOwner>(item) = oldOwner;
            g_registry.get<ecs::ItemLocation>(item) = oldLocation;
        }
        return false;
    }

    TPacketGCItemSet pack{};
    pack.header = windowMode == SAFEBOX ? HEADER_GC_SAFEBOX_SET : HEADER_GC_MALL_SET;
    pack.Cell = TItemPos(windowMode, cell);
    pack.vnum = ItemSystem::GetItemVnum(item);
    pack.count = ItemSystem::GetItemCount(item);
    pack.flags = ItemSystem::GetItemFlags(item);
#ifdef ATTR_LOCK
    pack.lockedattr = ItemSystem::GetItemLockedAttributeIndex(item);
#endif
    pack.anti_flags = ItemSystem::GetItemAntiFlags(item);
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        pack.alSockets[i] = ItemSystem::GetItemSocket(item, i);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        pack.aAttr[i] = ItemSystem::GetItemAttribute(item, i);

    if (LPDESC desc = ecs::PlayerRuntime::GetDesc(chrOwner))
        desc->Packet(&pack, sizeof(pack));
    LOG_INFO("SAFEBOX: ADD {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), ItemSystem::GetItemName(item), ItemSystem::GetItemCount(item));
    return true;
}

entt::entity GetItem(entt::entity storage, uint32_t cell)
{
    const auto* state = LiveStorage(storage);
    if (!state || cell >= state->items.size() || cell >= GridSize(*state))
        return entt::null;

    const entt::entity item = state->items[cell];
    return OwnsItem(*state, item, cell) ? item : entt::null;
}

entt::entity Remove(entt::entity storage, uint32_t cell)
{
    StorageLease lease {storage};
    auto* state = lease.state;
    const entt::entity chrOwner = state ? state->owner : entt::null;
    if (!state || !ecs::PlayerRuntime::IsPC(chrOwner))
        return entt::null;
    const uint8_t windowMode = state->windowMode;
    const entt::entity item = GetItem(storage, cell);
    if (!ItemSystem::IsValidItem(item))
        return entt::null;

    const uint8_t size = ItemSystem::GetItemSize(item);
    if (!FitsGrid(*state, cell, size))
        return entt::null;
    GridGet(*state, cell, size);

    state->items[cell] = entt::null;
    if (!ItemSystem::RemoveItemEcs(item))
    {
        state = FindStorage(storage);
        if (state && !state->destroying && OwnsItem(chrOwner, windowMode, item, cell) &&
            state->items[cell] == entt::null && GridPut(*state, cell, size))
            state->items[cell] = item;
        return entt::null;
    }
    // Detaching can run callbacks that close or replace this storage.
    state = FindStorage(storage);
    if (!state || state->destroying || !ecs::PlayerRuntime::IsPC(chrOwner) || state->items[cell] != entt::null)
        return entt::null;

    TPacketGCItemDel pack{};
    pack.header = windowMode == SAFEBOX ? HEADER_GC_SAFEBOX_DEL : HEADER_GC_MALL_DEL;
    pack.pos = cell;
    if (LPDESC desc = ecs::PlayerRuntime::GetDesc(chrOwner))
        desc->Packet(&pack, sizeof(pack));
    if (!ItemSystem::IsValidItem(item))
        return entt::null;
    const auto* detachedOwner = g_registry.try_get<ecs::ItemOwner>(item);
    if (!detachedOwner || detachedOwner->owner != entt::null || ItemSystem::GetItemWindow(item) != RESERVED_WINDOW)
        return entt::null;
    LOG_INFO("SAFEBOX: REMOVE {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), ItemSystem::GetItemName(item), ItemSystem::GetItemCount(item));
    return item;
}

void Save(entt::entity storage)
{
    const auto* state = LiveStorage(storage);
    const entt::entity chrOwner = state ? state->owner : entt::null;
    if (!state || !ecs::PlayerRuntime::IsPC(chrOwner) || !db_clientdesc)
        return;
    TSafeboxTable t {};
    t.dwID = ecs::PlayerRuntime::GetAccountID(chrOwner);
    if (!t.dwID)
        return;
    t.dwGold = state->gold;

    db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_SAVE, 0, &t, sizeof(TSafeboxTable));
    LOG_INFO("SAFEBOX: SAVE {}", ecs::PlayerRuntime::GetName(chrOwner).data());
}

bool IsEmpty(entt::entity storage, uint32_t cell, uint8_t size)
{
    const auto* state = LiveStorage(storage);
    if (!state || !FitsGrid(*state, cell, size))
        return false;

    return GridIsEmpty(*state, cell, size);
}

void ChangeSize(entt::entity storage, int size)
{
    auto* state = LiveStorage(storage);
    if (!state || size <= state->size || size > MaxHeight)
        return;
    // The occupancy array is already zero beyond the current height, so
    // growing only moves the boundary; no cells are reallocated or lost.
    state->size = size;
}

bool IsValidPosition(entt::entity storage, uint32_t cell)
{
    const auto* state = LiveStorage(storage);
    if (!state || cell >= state->items.size())
        return false;

    return cell < GridSize(*state);
}

bool MoveItem(entt::entity storage, uint32_t cell, uint32_t destCell, uint32_t count)
{
    StorageLease lease {storage};
    auto* state = lease.state;
    const entt::entity chrOwner = state ? state->owner : entt::null;
    if (!state || !ecs::PlayerRuntime::IsPC(chrOwner) ||
        !IsValidPosition(storage, cell) || !IsValidPosition(storage, destCell))
        return false;

    const entt::entity item = GetItem(storage, cell);
    if (!ItemSystem::IsValidItem(item) || ItemSystem::IsItemExchanging(item) || ItemSystem::IsItemLocked(item))
        return false;

    const uint32_t sourceCount = ItemSystem::GetItemCount(item);
    if (sourceCount < count)
        return false;

    const entt::entity destination = GetItem(storage, destCell);
    if (ItemSystem::IsValidItem(destination) &&
        (ItemSystem::IsItemExchanging(destination) || ItemSystem::IsItemLocked(destination)))
        return false;
    if (ItemSystem::IsValidItem(destination) && destination != item &&
        (ItemSystem::GetItemFlags(destination) & ITEM_FLAG_STACKABLE) &&
        !(ItemSystem::GetItemAntiFlags(destination) & ITEM_ANTIFLAG_STACK) &&
        ItemSystem::GetItemVnum(destination) == ItemSystem::GetItemVnum(item))
    {
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
            if (ItemSystem::GetItemSocket(destination, i) != ItemSystem::GetItemSocket(item, i))
                return false;

        if (count == 0)
            count = sourceCount;

        const uint32_t destinationCount = ItemSystem::GetItemCount(destination);
        if (g_bItemCountLimit <= 0 || destinationCount >= static_cast<uint32_t>(g_bItemCountLimit))
            return false;
        count = std::min(static_cast<uint32_t>(g_bItemCountLimit) - destinationCount, count);
        if (count == 0)
            return false;

        if (count >= sourceCount && Remove(storage, cell) == entt::null)
            return false;

        if (!ItemSystem::ConsumeItemEcs(item, count))
        {
            if (count >= sourceCount && ItemSystem::IsValidItem(item) && !Add(storage, cell, item))
                LOG_ERROR("SAFEBOX: failed to restore source {} after rejected consumption", ItemSystem::GetItemID(item));
            return false;
        }
        if (!ItemSystem::AddItemCountEcs(destination, count))
        {
            LOG_ERROR("SAFEBOX: destination credit failed after source debit at {} -> {}", cell, destCell);
            return false;
        }
        LOG_INFO("SAFEBOX: STACK {} {} -> {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), static_cast<int>(cell), static_cast<int>(destCell), ItemSystem::GetItemName(destination), ItemSystem::GetItemCount(destination));
        return true;
    }

    if (!IsEmpty(storage, destCell, ItemSystem::GetItemSize(item)))
        return false;

    LOG_INFO("SAFEBOX: MOVE {} {} -> {} {} count {}", ecs::PlayerRuntime::GetName(chrOwner).data(), static_cast<int>(cell), static_cast<int>(destCell), ItemSystem::GetItemName(item), sourceCount);
    if (Remove(storage, cell) == entt::null)
        return false;
    return Add(storage, destCell, item);
}
} // namespace SafeboxSystem
