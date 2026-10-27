#pragma once
#include "ItemKey.hpp"
#include <cstdint>
#include <expected>

namespace auriga::item
{
// Items that carry wallet money (gold bars, coin vouchers). The per-unit value
// is an explicit field, never the hidden shop price of the old item_proto.
enum class WalletCurrency : std::uint8_t { Gold = 1, DragonCoin = 2 };
inline constexpr std::uint64_t MaxTokenUnitAmount = 9'223'372'036'854'775'807ULL;

class ItemCatalog;
struct TokenRedeemRequest
{
    ItemKey token;
    std::uint32_t unitsInStack = 0;
    std::uint64_t walletBalance = 0;
    std::uint64_t walletCap = 0; // runtime configuration (e.g. GOLD_MAX), not catalog data
};
struct TokenRedeemPlan
{
    ItemKey token;
    WalletCurrency currency{};
    std::uint32_t unitsToConsume = 0;
    std::uint64_t amountToCredit = 0;
};
enum class TokenRedeemError { MissingItem, NotToken, RetiredItem, EmptyStack, InvalidWallet, WalletFull };
// Redeems as many whole units of the stack as fit under the wallet cap and
// consumes only those. Never consumes a unit whose value would be lost.
std::expected<TokenRedeemPlan, TokenRedeemError> PlanTokenRedeem(const ItemCatalog& catalog, const TokenRedeemRequest& request);
} // namespace auriga::item
