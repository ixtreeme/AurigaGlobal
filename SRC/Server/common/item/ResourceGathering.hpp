#pragma once
#include "ToolRuntime.hpp"
#include "FishingChallenge.hpp"
#include <string>
namespace auriga::item
{
struct GatheringReward { ItemKey key;std::uint32_t count=0,weight=0; };
// World content, like typed crafting workshops. Item references are ItemKeys.
struct GatheringSite
{
    std::uint32_t id=0,map=0,race=0;std::string name;ToolKind kind{};FishingPool pool=FishingPool::None;
    std::uint32_t range=0,durationMs=0,minimumPower=0,powerStep=1;
    std::uint16_t baseChance=0,powerChance=0,baitChance=0;
    std::vector<GatheringReward> rewards;
};
enum class ResourceError:std::uint8_t { Invalid,Forbidden,WrongTool,NoBait,NoRoom,Unavailable,InvalidDecision,NotCompleted };
struct ResourceRequest { ItemInstanceId tool;ToolRuntimeActor actor;bool completed=false,eligible=true; };
struct ResourcePlan { ItemCommit commit;bool success=false;std::uint32_t points=0,bait=0;std::vector<FishingYield> yields; };
std::expected<void,ResourceError> ValidateGatheringSites(const ItemCatalog&,std::span<const GatheringSite>);
std::expected<std::uint16_t,ResourceError> CheckResourceGathering(const ItemCatalog&,const GatheringSite&,ItemOwner,std::span<const PersistentItem>,ResourceRequest);
// All possible reward footprints and reserved IDs are admitted before any RNG.
std::expected<ResourcePlan,ResourceError> MakeResourceGatheringCommit(const ItemCatalog&,const GatheringSite&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ResourceRequest,std::span<const ItemInstanceId>,const LootDraw&);
enum class ResourceStatus:std::uint8_t { Closed,Ready,Running,Pending,Applied,Denied,Busy,Stale,Cancelled,Failed };
struct ResourceView
{
    ResourceStatus status=ResourceStatus::Closed;std::uint8_t error=255;std::uint64_t nonce=0;
    const PersistentItem* tool=nullptr;const GatheringSite* site=nullptr;std::uint32_t target=0,remainingMs=0;
    std::uint16_t chance=0;const ResourcePlan* result=nullptr;
    const FishingRuntimeView* fishing=nullptr;
};
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeResourceGathering(const ItemCatalog&,const ResourceView&);
}
