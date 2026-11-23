#pragma once

#include "PersistentItems.hpp"

namespace auriga::item
{
inline constexpr std::uint16_t InventorySortOutcome = 0x4213;
inline constexpr std::uint8_t SortBackpack = 0xff;
inline constexpr std::size_t MaxInventorySortCells = 512;

// Sort every page of the backpack, or every page of one extra inventory category.
// A successful empty plan means the inventory is already arranged.
std::expected<std::optional<ItemCommit>, ItemPersistenceError> MakeInventorySortCommit(
    const ItemCatalog&, std::array<std::uint8_t, 16> operationId, ItemOwnerFence,
    std::span<const ItemStorageLayout>, std::span<const PersistentItem>, std::uint8_t scope);

bool ValidInventorySort(const ItemCatalog&, const ItemCommit&);
}
