#pragma once
#include "ItemKey.hpp"
#include "BonusProfiles.hpp"
#include <array>
#include <optional>
#include <span>
#include <vector>
namespace auriga::item {
class ItemCatalog;
struct PersistentItem;
struct EnergyCrystalTier {
    std::uint32_t profile=0;
    std::uint8_t percent=0;
    std::uint16_t chanceBp=0;
};
struct EnergyCrystalState {
    std::uint8_t percent=0;
    bool operator==(const EnergyCrystalState&) const=default;
};
bool IsEnergyCrystal(const ItemCatalog&,ItemKey);
bool ValidEnergyCrystalPercent(const ItemCatalog&,ItemKey,unsigned);
// Inclusive, unbiased 1..10000. No draw occurs when loading, moving or using.
std::optional<EnergyCrystalState> RollEnergyCrystal(const ItemCatalog&,ItemKey,unsigned);
std::optional<EnergyCrystalState> InitialEnergyCrystal(const ItemCatalog&,ItemKey,std::span<const std::uint8_t,16>,std::uint64_t instance);
// Aggregate unlocked instance attributes on the original eight ordinary slots,
// truncate in the original APPLY unit, then restore each native registry unit.
std::optional<std::vector<BonusEntry>> EnergyEquipmentBonuses(const ItemCatalog&,std::span<const PersistentItem>,unsigned percent);
}
