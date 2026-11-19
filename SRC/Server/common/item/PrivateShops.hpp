#pragma once
#include "PersistentItems.hpp"
namespace auriga::item {
enum class PrivateShopPermitKind:std::uint8_t {None,Consumed,Silk};
PrivateShopPermitKind PrivateShopPermit(const ItemCatalog&,ItemKey);
std::uint64_t PrivateShopPermitDuration(const ItemCatalog&,ItemKey);
bool ValidPrivateShopSign(std::string_view);
bool ValidPrivateShopHeader(const PrivateShopHeader&);
bool ValidPrivateShopListing(const PrivateShopListing&);
std::optional<std::vector<std::uint16_t>> PrivateShopDisplayCells(const ItemCatalog&,ItemKey,std::uint16_t cell);
bool PrivateShopListable(const ItemCatalog&,const PersistentItem&,ItemOwner,std::uint64_t nowAtMs,bool allowListed=false);
std::uint64_t PrivateShopItemDeadline(const PersistentItem&);
bool ValidPrivateShopWrite(const ItemCatalog&,const ItemCommit&);
std::vector<std::uint8_t> EncodePrivateShopMutation(const PrivateShopMutation&);
struct PrivateShopOffer {ItemInstanceId id;std::uint64_t price=0;std::uint16_t cell=0;};
enum class PrivateShopError {Invalid,Denied,Unavailable,Expired,NoMoney,WalletFull,NoRoom};
std::expected<ItemCommit,PrivateShopError> MakePrivateShopOpen(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const ItemWallet&,
    ItemInstanceId permit,std::string_view sign,std::span<const PrivateShopOffer>,std::uint64_t nowAtMs,std::uint64_t cap);
std::expected<ItemCommit,PrivateShopError> MakePrivateShopBuy(const ItemCatalog&,std::array<std::uint8_t,16>,
    const PrivateShopHeader&,ItemOwnerFence seller,ItemOwnerFence buyer,std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>,std::span<const ItemWallet>,ItemInstanceId,
    std::uint8_t taxPercent,std::uint64_t nowAtMs,std::uint64_t cap);
std::expected<ItemCommit,PrivateShopError> MakePrivateShopClose(const ItemCatalog&,std::array<std::uint8_t,16>,
    const PrivateShopHeader&,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>);
std::expected<ItemCommit,PrivateShopError> MakePrivateShopPermitExpiry(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,std::uint64_t nowAtMs);
}
