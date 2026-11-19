#pragma once
#include "PersistentItems.hpp"
#include "QuestItems.hpp"
namespace auriga::item
{
// Explicit owner facts for compiled utility handlers. No legacy item identity
// is accepted by this planner or emitted in a client action.
struct BuiltinUtilityFacts {
    QuestItemUseRequest use;
    std::uint8_t characterClass=0,sex=0;
    std::int32_t mapIndex=0;
    bool alive=false,pvpPotionsBlocked=false;
    bool summonZoneAllowed=false,warpAllowed=false,marriageAvailable=false,marriageArenaBlocked=false;
    std::uint64_t currentSp=0,maximumSp=0;
};
struct BuiltinUtilityPlan {
    BuiltinQuestItem handler=BuiltinQuestItem::None;
    std::uint16_t consume=0;
    std::uint32_t durationSeconds=0;
    std::uint8_t firstFirework=0,lastFirework=0;
    std::uint64_t spCost=0;
};
std::expected<BuiltinUtilityPlan,QuestItemUseError> PrepareBuiltinUtility(const ItemCatalog&,const PersistentItem&,ItemOwner,const BuiltinUtilityFacts&);
std::expected<ItemCommit,ItemPersistenceError> MakeBuiltinUtilityCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,const BuiltinUtilityFacts&);
}
