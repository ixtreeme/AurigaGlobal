#pragma once
#include "SoulRuntime.hpp"

namespace auriga::item {
inline constexpr std::uint16_t AmmoShotOutcome=0x3101;
struct BowAmmoView {
    ItemInstanceId bow,ammo;
    std::uint32_t count=0,policy=0;
    std::uint16_t units=0;
};
std::optional<BowAmmoView> FindBowAmmo(const ItemCatalog&,ItemOwner,std::span<const PersistentItem>);
// One final, server-accepted ranged hit. Optional Soul debit shares this transaction.
std::expected<SoulCommit,ItemPersistenceError> MakeAmmoShotCommit(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,SoulContext,
    std::optional<ItemInstanceId> soul={},std::uint64_t soulElapsed=0);
bool ValidAmmoShotWrite(const ItemCatalog&,const ItemCommit&);
std::span<const std::uint8_t> AmmoSoulReceipt(const ItemCommit&);
}
