#pragma once

#include "PersistentItems.hpp"

namespace auriga::item
{
inline constexpr std::uint16_t NameScrollOutcome = 0x3B02;
inline constexpr std::uint16_t EmpireScrollOutcome = 0x3B03;

struct NameScrollChange
{
    std::string_view before;
    std::string_view after;
};

struct EmpireScrollChange
{
    std::uint32_t account;
    std::uint8_t before;
    std::uint8_t after;
};

std::optional<NameScrollChange> ReadNameScrollChange(const ItemCommit&);
std::optional<EmpireScrollChange> ReadEmpireScrollChange(const ItemCommit&);
bool ValidateIdentityScrollWrite(const ItemCatalog&, const ItemCommit&);
std::expected<ItemCommit, ItemPersistenceError> MakeNameScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, NameScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeEmpireScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, EmpireScrollChange);
}
