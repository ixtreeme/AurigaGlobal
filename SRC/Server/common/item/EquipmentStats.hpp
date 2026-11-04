#pragma once
#include "PersistentItems.hpp"
#include <array>

namespace auriga::item
{
struct EquippedStatItem
{
    std::uint8_t slot=0;ItemInstanceId id;ItemKey key;std::uint16_t level=0;
    std::string model;
    bool operator==(const EquippedStatItem&) const = default;
};
struct EquipmentStats
{
    ItemBaseStats flat;
    std::array<std::int64_t,35> bonuses{}; // stable BonusId, in its registry unit
    std::array<std::int64_t,15> resistances{}; // ItemResistanceProfile::Values order, bp
    std::vector<EquippedStatItem> equipped;
    std::optional<WeaponSubtype> weapon;
    std::optional<ToolKind> tool;
    std::uint32_t reachMillimeters=0;
    std::uint16_t automaticHpBonusBp=0,automaticSpBonusBp=0;
    bool operator==(const EquipmentStats&) const = default;
    std::int64_t Bonus(BonusId id) const noexcept {return bonuses[static_cast<unsigned>(id)];}
};
enum class EquipmentStatsError { InvalidItem, InvalidSlot, Duplicate, Overflow, UnsupportedState };
std::expected<EquipmentStats,EquipmentStatsError> BuildEquipmentStats(const ItemCatalog&,ItemOwner,std::span<const PersistentItem>,std::span<const BonusEntry> effects={});
// Checked signed nearest rounding (ties away from zero). No floating point or
// balance clamp. A negative factor is interpreted as zero at the call site.
std::expected<std::int64_t,EquipmentStatsError> ScaleEquipmentValue(std::int64_t value,std::int64_t numerator,std::int64_t denominator=10000);
std::expected<std::int64_t,EquipmentStatsError> AddEquipmentValue(std::int64_t,std::int64_t);
// Owner-only GC231 v1: header,size:u32,version,status,vid:u32,statRevision:u64,
// inventoryRevision:u64,weapon:u8 (0:none),4 flat raw:i64,13 effective points:i64
// (STR,DEX,CON,INT,physical/magic attack,physical/magic defense,attack/move/cast
// speed,max HP,max SP),bonuses:u8 {id:u32,value:i64},15 resistance bp:i64.
std::vector<std::byte> EncodeEquipmentStats(const EquipmentStats*,std::uint32_t vid,std::uint64_t revision,
    std::uint64_t inventoryRevision,const std::array<std::int64_t,13>& points);
}
