#pragma once
#include "PersistentItems.hpp"
#include "TypedInventoryWire.hpp"

namespace auriga::item
{
inline constexpr std::size_t MaxPlayerTradeItems=8;
struct TradeSelection { ItemInstanceId id; std::uint32_t count=0; };
struct PlayerTradeOffer
{
    ItemOwnerFence owner;
    std::vector<TradeSelection> items;
    std::uint64_t gold=0;
};
enum class PlayerTradeError { Invalid, Denied, NoMoney, NoRoom, WalletFull, Unavailable };
// Both complete owner snapshots. Whole instances retain their IDs; splits
// copy the complete payload to reserved IDs. Incoming instances never merge.
// Remove all outgoing whole footprints before placing either incoming side.
std::expected<ItemCommit,PlayerTradeError> MakePlayerTradeCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,const std::array<PlayerTradeOffer,2>&,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::span<const ItemWallet>,
    std::span<const ItemInstanceId>);

enum class PlayerTradeStatus : std::uint8_t
{ Closed, Invited, Updated, Ready, Pending, Completed, Cancelled, Denied, Stale, NoRoom, NoMoney, WalletFull, Invalid, Failed };
enum class PlayerTradePhase : std::uint8_t { Invited, Editing, Confirming, Committing };
struct PlayerTradeActorView
{
    std::uint32_t vid=0,pid=0;
    std::string name;
    PlayerTradeOffer offer;
};
struct PlayerTradeView
{
    std::uint64_t token=0,revision=0;
    PlayerTradePhase phase{};
    std::uint8_t readyMask=0,confirmedMask=0;
    std::array<PlayerTradeActorView,2> actors;
};
// GC229 v1: header,size:u32,version,status,token:u64,request:u64,revision:u64,
// side:u8,phase:u8,ready:u8,confirmed:u8. Terminal packet ends here (35 bytes).
// Active: two {vid:u32,pid:u32,name:text16,gold:u64}, eligible:u16 {id:u64},
// two {size:u32,GC225},
// then {size:u32,GC227}. Embedded inventories reveal offered instances only.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodePlayerTrade(const ItemCatalog&,
    const PlayerTradeView*,std::uint8_t side,std::uint64_t request,PlayerTradeStatus,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>);
}
