#pragma once
#include "PersistentItems.hpp"
#include "TypedInventoryWire.hpp"
namespace auriga::item
{
struct ItemEffectFacts
{
    InstantRecoveryAdmission actor;
    bool alive=false,married=false,partyLeader=false,allowed=false;
    bool polymorphAllowed=true;
};
struct ItemEffectError { std::uint8_t code=255; };
const TimedEffectProfile* TimedItemProfile(const ItemCatalog&,ItemKey);
const ToggleEffectProfile* ToggleItemProfile(const ItemCatalog&,ItemKey);
std::uint64_t TimedItemRemaining(const TimedItemEffect&,std::uint64_t now);
void NormalizeTimedItems(const ItemCatalog&,RecoverySchedule&,std::uint64_t now,const ItemEffectFacts* facts=nullptr);
bool HasDeathItemEffects(const ItemCatalog&,const RecoverySchedule&,std::span<const PersistentItem>);
std::expected<std::vector<PersistentItem>,ItemPersistenceError> ProjectToggleItems(const ItemCatalog&,std::span<const PersistentItem>,std::uint64_t elapsed,const ItemEffectFacts&);
std::expected<void,ItemPersistenceError> SettleToggleTime(const ItemCatalog&,ItemCommit&,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::uint64_t,const ItemEffectFacts&);
std::expected<std::vector<BonusEntry>,ItemPersistenceError> CollectItemEffectBonuses(const ItemCatalog&,const RecoverySchedule&,std::span<const PersistentItem>);
std::expected<ItemCommit,ItemEffectError> MakeItemEffectUse(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const RecoverySchedule&,ItemInstanceId,const ItemEffectFacts&,std::uint64_t elapsed,std::uint64_t now,std::uint64_t effectId);
std::expected<ItemCommit,ItemEffectError> MakeItemEffectCancel(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    const RecoverySchedule&,std::uint64_t elapsed,std::uint64_t now,std::uint64_t generation,std::uint64_t effectId);
struct ItemEffectsRuntimeView
{
    std::uint8_t status=0,error=255;std::uint64_t control=0,preview=0;
    const PersistentItem* source=nullptr;const RecoverySchedule* schedule=nullptr;
    std::span<const PersistentItem> items;std::uint64_t now=0;
};
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeItemEffectsRuntime(const ItemCatalog&,const ItemEffectsRuntimeView&);
}
