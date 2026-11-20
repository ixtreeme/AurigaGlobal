#pragma once

#include "PersistentItems.hpp"

namespace auriga::item
{
inline constexpr std::uint16_t NameScrollOutcome = 0x3B02;
inline constexpr std::uint16_t EmpireScrollOutcome = 0x3B03;
inline constexpr std::uint16_t SexScrollOutcome = 0x3B04;
inline constexpr std::uint16_t GuildMasterScrollOutcome = 0x3B05;

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

struct SexScrollChange
{
    std::uint8_t before;
    std::uint8_t after;
};

struct GuildMasterScrollChange
{
    std::uint32_t guild;
    std::uint32_t successor;
};

// The four player classes occupy the same slots in both halves of the race table.
constexpr std::optional<std::uint8_t> OppositeSexRace(std::uint32_t race)
{
    if (race >= 8) return {};
    return static_cast<std::uint8_t>(race ^ 4);
}

std::optional<NameScrollChange> ReadNameScrollChange(const ItemCommit&);
std::optional<EmpireScrollChange> ReadEmpireScrollChange(const ItemCommit&);
std::optional<SexScrollChange> ReadSexScrollChange(const ItemCommit&);
std::optional<GuildMasterScrollChange> ReadGuildMasterScrollChange(const ItemCommit&);
bool ValidateIdentityScrollWrite(const ItemCatalog&, const ItemCommit&);
std::expected<ItemCommit, ItemPersistenceError> MakeNameScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, NameScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeEmpireScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, EmpireScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeSexScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, SexScrollChange);
std::expected<ItemCommit, ItemPersistenceError> MakeGuildMasterScrollCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16>, ItemOwnerFence, std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>, ItemInstanceId, std::uint64_t, GuildMasterScrollChange);
}
