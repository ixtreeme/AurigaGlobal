#pragma once
#include "PersistentItems.hpp"
#include "TypedInventoryWire.hpp"

namespace auriga::item
{
enum class FishProcessingError : std::uint8_t
{ Invalid, Forbidden, Level, CampfireRequired, WrongAction, Unsupported, NoRoom, Unavailable, InvalidDecision };
enum class FishProcessingStatus : std::uint8_t
{ Closed, Ready, Pending, Applied, Invalid, Denied, Busy, Stale, Failed };
struct FishProcessingActor
{
    std::uint16_t level=0;
    std::uint8_t characterClass=0,sex=0;
    bool admitted=false,inCombat=false,campfireAvailable=false;
};
struct FishProcessingRequest { ItemInstanceId source; FishAction action{}; FishProcessingActor actor; };
struct FishProcessingQuote
{
    FishingKind kind{};
    std::uint32_t consume=0;
    std::optional<LootPickMode> mode; // absent: deterministic grilling
    std::vector<LootEntry> possibilities;
};
struct FishProcessingPlan { ItemCommit commit; std::vector<FishingYield> yields; };
std::expected<FishProcessingQuote,FishProcessingError> CheckFishProcessing(
    const ItemCatalog&,ItemOwner,std::span<const PersistentItem>,FishProcessingRequest);
// Admit every possible output footprint before drawing. Consumption, all
// grants and the replay receipt are one CAS-guarded transaction.
std::expected<FishProcessingPlan,FishProcessingError> MakeFishProcessingCommit(
    const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>,FishProcessingRequest,std::span<const ItemInstanceId>,const LootDraw&);
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeFishProcessing(
    const ItemCatalog&,FishProcessingStatus,std::uint8_t error,std::uint64_t session,const PersistentItem*,
    FishAction,std::uint32_t campfireVid,const FishProcessingQuote&,std::span<const FishingYield>);
}
