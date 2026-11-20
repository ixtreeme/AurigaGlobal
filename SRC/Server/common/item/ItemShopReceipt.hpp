#pragma once

#include <cstddef>
#include <cstdint>

namespace auriga::item
{
class ItemCatalog;
class ItemKey;

inline constexpr std::uint16_t ItemShopPurchaseOutcome = 0x5345;
inline constexpr std::uint16_t ItemShopCostumeBirthOutcome = 0x5346;
inline constexpr bool IsItemShopPurchaseOutcome(std::uint16_t outcome) noexcept
{
    return outcome == ItemShopPurchaseOutcome || outcome == ItemShopCostumeBirthOutcome;
}

bool UsesItemShopCostumeBirthMagic(const ItemCatalog&, ItemKey, std::uint16_t level) noexcept;
// Receipt v1: account:u32, shop:u32, units:u16, paid:u64, issued_at_ms:u64, title:text16,
// member count:u8, then {key:u16+u32, level:u16, quantity:u32} per member.
inline constexpr unsigned ItemShopIssuedAtOffset = 18;
inline constexpr std::size_t ItemShopReceiptPrefix = ItemShopIssuedAtOffset + 8;
inline constexpr unsigned MaxItemShopReceipt = 1024;
}
