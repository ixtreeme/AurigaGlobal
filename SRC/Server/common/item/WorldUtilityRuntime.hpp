#pragma once
#include "PersistentItems.hpp"

namespace auriga::item {
inline constexpr std::uint16_t WorldUtilityOutcome=0x2A01;
enum class WorldUtilityAction : std::uint8_t { Stamp=1,WarpTown=2,WarpRecall=3,ClearStamp=4,Detect=5,Campfire=6 };
enum class WorldUtilityError : std::uint8_t { NotStateful,InvalidSource,Forbidden,HealthDenied,ZoneDenied,TooNear,DungeonDenied,TargetUnavailable,LevelDenied,OnCooldown,NoRoom,NoIds,InvalidState,SpawnUnprepared };
struct WorldUtilityFacts {
    EquipContext character;
    QuestItemContext context=QuestItemContext::World;
    RecallPosition position;
    std::uint8_t empire=0;
    std::uint64_t hp=0,maximumHp=0;
    bool admitted=false,alive=false,mapLoaded=false,arena=false,inDungeon=false,warping=false;
    bool warpReady=false,summonZoneAllowed=false,pvpPotionsBlocked=false;
    std::optional<RecallPosition> town;
    bool savedDestinationAvailable=false;
    bool campfireSpotAllowed=false,campfirePrepared=false;
    std::uint32_t stoneVid=0;
    std::uint16_t stoneBearing=0;
    std::uint8_t stoneRange=0;
};
struct WorldUtilityQuote {
    WorldUtilityAction action{};
    std::optional<RecallPosition> destination,stamp;
    bool consume=false,split=false,needsWrite=true;
    std::uint64_t hpCost=0;
    std::uint16_t remainingUses=0;
    std::uint32_t stoneVid=0;
    std::uint16_t stoneBearing=0;
    std::uint8_t stoneRange=0;
};
bool StatefulWorldUtility(const ItemCatalog&,ItemKey);
std::uint64_t WorldUtilityCooldownRemaining(const ItemCatalog&,const RecoverySchedule&,std::uint64_t now);
std::expected<WorldUtilityQuote,WorldUtilityError> QuoteWorldUtility(const ItemCatalog&,ItemOwner,const PersistentItem&,
    const RecoverySchedule&,const WorldUtilityFacts&,std::uint64_t now);
struct WorldUtilityPlan {WorldUtilityQuote quote;std::optional<ItemCommit> commit;};
std::expected<WorldUtilityPlan,WorldUtilityError> MakeWorldUtilityCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const RecoverySchedule&,ItemInstanceId,
    const WorldUtilityFacts&,std::uint64_t elapsed,std::uint64_t now,std::optional<ItemInstanceId> splitId={});
}
