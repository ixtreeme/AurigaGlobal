#pragma once
#include "PersistentItems.hpp"
#include "TypedItemDetails.hpp"

namespace auriga::item
{
struct MissionItemAmount { ItemKey key;std::uint16_t level=0;std::uint32_t count=0;bool operator==(const MissionItemAmount&) const=default; };
struct ItemMission
{
    std::uint32_t id=0,version=0;std::string code,name,description;std::uint16_t minimumLevel=0;
    std::vector<MissionItemAmount> requirements,costs,rewards;
    std::uint64_t goldCost=0,goldReward=0;
};
struct ItemMissionBoard { std::uint32_t npcRace=0;std::string name;std::vector<ItemMission> missions; };
inline constexpr std::size_t MaxItemMissionBoards=256,MaxItemMissions=32,MaxMissionItemEntries=16;
inline constexpr std::uint16_t ItemMissionOutcome=0x1201;
enum class ItemMissionError { Invalid, Requirements, NoMoney, NoRoom, Unavailable, Limit, Level };
enum class ItemMissionStatus : std::uint8_t { Closed,Ready,Completed,AlreadyCompleted,Pending,Busy,Invalid,Denied,Requirements,NoMoney,NoRoom,Failed,Stale,Level };
struct ItemMissionProof { ItemOwner owner;ItemCatalogDigest action{}; };
bool MissionItemUsable(const PersistentItem&,ItemOwner);
std::uint64_t CountMissionItems(std::span<const PersistentItem>,ItemOwner,ItemKey,std::uint16_t);
std::expected<void,ItemMissionError> ValidateItemMission(const ItemCatalog&,const ItemMission&);
// Immutable economic contract. Presentation names may change without enabling
// another reward. Economic changes require an explicitly new mission version.
std::expected<ItemCatalogDigest,ItemMissionError> ItemMissionDigest(const ItemMission&);
std::expected<std::array<std::uint8_t,16>,ItemMissionError> ItemMissionOperation(ItemMissionProof);
std::expected<std::vector<std::uint8_t>,ItemMissionError> ItemMissionReceipt(const ItemMission&,ItemMissionProof);
std::expected<ItemCommit,ItemMissionError> MakeItemMissionCommit(const ItemCatalog&,const ItemMission&,ItemMissionProof,
    std::uint16_t level,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    std::span<const ItemWallet>,std::span<const ItemInstanceId>);
// GC232 v1: header,size:u32,version,status,npc:u32,session:u64,request:u64,
// selected:u32,name:text16,missions:u8 {id:u32,version:u32,code/name/description:text16,
// minLevel:u16,completed:u8,goldCost/goldReward:u64,3x (count:u8,{key,level:u16,units:u32})},
// detailsSize:u32,GC227. Empty closed envelope: 38 bytes.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeItemMissionBoard(const ItemCatalog&,const ItemMissionBoard*,
    std::uint32_t npc,std::uint64_t session,std::uint64_t request,ItemMissionStatus,std::uint32_t selected,
    std::span<const std::uint32_t> completed);
}
