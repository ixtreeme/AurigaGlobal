#pragma once
#include "PersistentItems.hpp"
#include "TypedItemDetails.hpp"

namespace auriga::item
{
enum class TokenRedemptionError { Invalid, Denied, WalletFull, Unsupported, Stale, Level };
enum class TokenRedemptionStatus : std::uint8_t { Closed, Ready, Redeemed, Pending, Busy, Invalid, Denied, WalletFull, Unsupported, Stale, Failed, Level };
struct TokenRedemptionActor { std::uint16_t level=0;std::uint8_t characterClass=0,sex=0;bool inCombat=false; };
struct TokenRedemptionQuote
{
    std::uint32_t available=0,requested=0,consumed=0;
    WalletCurrency currency{};
    std::uint64_t unit=0,credit=0,balance=0,cap=0;
    bool operator==(const TokenRedemptionQuote&) const=default;
};
struct TokenRedemptionPlan { ItemCommit commit;TokenRedemptionQuote quote; };
inline constexpr std::uint16_t TokenRedemptionOutcome=0x1401;
std::expected<TokenRedemptionPlan,TokenRedemptionError> MakeTokenRedemptionCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    std::span<const ItemWallet>,ItemInstanceId,std::uint32_t requested,std::uint64_t cap,TokenRedemptionActor);
// GC234 v1. Empty terminal envelope: 88 bytes. Preview metadata is GC227.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeTokenRedemption(const ItemCatalog&,
    std::uint64_t session,const PersistentItem*,std::uint64_t ownerRevision,TokenRedemptionStatus,const TokenRedemptionQuote&);
}
