#pragma once
#include "PersistentItems.hpp"
#include "TypedItemDetails.hpp"

namespace auriga::item
{
struct NpcShopOffer { std::uint32_t id=0; ItemGrant grant; };
struct NpcShop { std::uint32_t npcRace=0; std::string name; std::vector<NpcShopOffer> offers; };
inline constexpr std::size_t MaxNpcShops=256, MaxNpcShopOffers=128;
inline constexpr std::uint32_t MaxShopQuantity=10000;
enum class NpcShopError { Invalid, Denied, NoMoney, NoRoom, WalletFull, Unavailable };
enum class NpcShopStatus : std::uint8_t
{ Closed=0, Ready=1, Bought=2, Sold=3, Busy=4, Invalid=5, Denied=6, NoMoney=7, NoRoom=8, WalletFull=9, Failed=10, Stale=11 };
std::expected<void,NpcShopError> ValidateNpcShops(const ItemCatalog&,std::span<const NpcShop>);
std::expected<ItemCommit,NpcShopError> MakeNpcBuyCommit(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::span<const ItemWallet>,
    const NpcShopOffer&,std::uint32_t quantity,std::span<const ItemInstanceId>);
std::expected<ItemCommit,NpcShopError> MakeNpcSellCommit(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::span<const ItemWallet>,
    ItemInstanceId,std::uint32_t count);
std::uint64_t NpcSellUnitPrice(const ItemCatalog&,const PersistentItem&);
// GC 228 v1: header:u8,size:u32,version:u8,status:u8,npc:u32,session:u64,request:u64,
// name:text16,offers:u16 {id:u32,key:6,level:u16,units:u32,unitPrice:u64},
// sales:u16 {instance:u64,unitPrice:u64},detailsSize:u32,GC227 bytes.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeNpcShop(const ItemCatalog&,const NpcShop*,
    std::uint32_t npc,std::uint64_t session,std::uint64_t request,NpcShopStatus,std::span<const PersistentItem>);
} // namespace auriga::item
