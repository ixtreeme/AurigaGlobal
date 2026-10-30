#pragma once
// F4.4 (IC-034): basic inventory operations as atomic write sets. Pure: no
// SQL, RNG, ECS or vnum. Each builder reads one owner's current state and
// returns an ItemCommit that ValidateItemCommit accepts, or an error.
#include "PersistentItems.hpp"
#include <array>
#include <span>

namespace auriga::item
{
using ItemOperationId = std::array<std::uint8_t, 16>;

// One owner's current, live state (destroyed items are not part of it).
struct ItemOwnerView
{
    ItemOwnerFence fence;
    std::span<const ItemStorageLayout> storages;
    std::span<const PersistentItem> items;
};

// Move to free cells, or swap with the one item whose anchor is the target,
// when each fits the other's place. Entering Equipment binds an OnEquip item.
std::expected<ItemCommit, ItemPersistenceError> MakeMoveCommit(const ItemCatalog&, const ItemOwnerView&, const ItemOperationId&,
                                                               ItemInstanceId item, ItemPlacement target);
// Moves `count` units of `source` onto the compatible stack `target`. A source
// that becomes empty is destroyed (tombstone).
std::expected<ItemCommit, ItemPersistenceError> MakeMergeCommit(const ItemCatalog&, const ItemOwnerView&, const ItemOperationId&,
                                                                ItemInstanceId source, ItemInstanceId target, std::uint32_t count);
// Splits `count` units of `source` into a new stack `created` at `target`.
std::expected<ItemCommit, ItemPersistenceError> MakeSplitCommit(const ItemCatalog&, const ItemOwnerView&, const ItemOperationId&,
                                                                ItemInstanceId source, std::uint32_t count, ItemPlacement target,
                                                                ItemInstanceId created);
// Removes the whole item (tombstone with its last complete state).
std::expected<ItemCommit, ItemPersistenceError> MakeDestroyCommit(const ItemCatalog&, const ItemOwnerView&, const ItemOperationId&,
                                                                  ItemInstanceId item);
// Uses up `count` units; the last unit destroys the item.
std::expected<ItemCommit, ItemPersistenceError> MakeConsumeCommit(const ItemCatalog&, const ItemOwnerView&, const ItemOperationId&,
                                                                  ItemInstanceId item, std::uint32_t count);
} // namespace auriga::item
