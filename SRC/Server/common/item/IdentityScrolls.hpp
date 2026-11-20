#pragma once

#include "PersistentItems.hpp"

namespace auriga::item
{
inline constexpr std::uint16_t NameScrollOutcome = 0x3B02;

struct NameScrollChange
{
    std::string_view before;
    std::string_view after;
};

std::optional<NameScrollChange> ReadNameScrollChange(const ItemCommit&);
bool ValidateNameScrollWrite(const ItemCatalog&, const ItemCommit&);
std::expected<ItemCommit, ItemPersistenceError> MakeNameScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, NameScrollChange);
}
