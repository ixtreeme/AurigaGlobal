#pragma once

#include "ItemMissions.hpp"

namespace auriga::item
{
enum class MarriageItemAction : std::uint8_t { Engage = 1, MutualDivorce, UnilateralDivorce };
inline constexpr std::uint16_t MarriageItemOutcome = 0x3B01;

struct MarriageItemChange
{
    MarriageItemAction action;
    std::uint32_t initiator;
    std::uint32_t partner;
};

struct MarriageItemParticipant
{
    ItemOwnerFence fence;
    std::span<const ItemStorageLayout> storages;
    std::span<const PersistentItem> items;
    std::span<const ItemWallet> wallets;
};

std::optional<MarriageItemChange> ReadMarriageItemChange(const ItemCommit&);
bool ValidateMarriageItemWrite(const ItemCatalog&, const ItemCommit&);

std::expected<ItemCommit, ItemMissionError> MakeMarriageItemCommit(const ItemCatalog&,
    std::array<std::uint8_t, 16> operation, MarriageItemChange,
    std::span<const MarriageItemParticipant>, std::span<const ItemInstanceId>, std::uint64_t nowUtcMs);
}
