#pragma once
#include "PersistentItems.hpp"
namespace auriga::item
{
const RecoveryProfile* AutomaticProfile(const ItemCatalog&,ItemKey);
AutomaticRecoveryAdmission AutomaticActor(const ItemCatalog&,std::span<const PersistentItem>,const PersistentItem&,
    const InstantRecoveryAdmission&,bool stunned,bool allowed);
std::expected<void,ItemPersistenceError> ChangeAutomatic(ItemCommit&,std::span<const ItemStorageLayout>,const PersistentItem&,AutomaticRecoveryState,bool destroy=false);
// Timer-only settlement never spends charge or invents catch-up ticks.
std::expected<void,ItemPersistenceError> SettleAutomaticTime(ItemCommit&,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::uint64_t elapsed);
std::expected<void,ItemPersistenceError> TickAutomaticOwner(const ItemCatalog&,ItemCommit&,std::span<const ItemStorageLayout>,
    std::span<const PersistentItem>,const InstantRecoveryAdmission&,RecoverySchedulePlan&,std::uint64_t elapsed,bool paused,bool allowed);
}
