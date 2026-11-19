#pragma once
#include "PersistentItems.hpp"
namespace auriga::item {
std::uint64_t DungeonEntryCleanupLimit(const ItemCatalog&,std::string_view gate,ItemKey);
bool ValidDungeonEntryMutation(const ItemCatalog&,const ItemCommit&);
std::vector<std::uint8_t> EncodeDungeonEntryMutation(const DungeonEntryMutation&);
// One existing stack per participant suffices for the authored one-unit entry
// costs; all8 party participants are reserved and debited together.
std::expected<ItemCommit,ItemPersistenceError> MakeDungeonEntryReservation(const ItemCatalog&,std::array<std::uint8_t,16>,
 std::string_view gate,ItemKey ticket,std::uint16_t level,std::uint32_t units,std::uint64_t createdAtMs,
 std::span<const ItemOwnerFence>,std::span<const PersistentItem>);
// Tickets are in preference order. Each participant can supply either exact key.
std::expected<ItemCommit,ItemPersistenceError> MakeDungeonEntryReservation(const ItemCatalog&,std::array<std::uint8_t,16>,
 std::string_view gate,std::span<const ItemKey> tickets,std::uint16_t level,std::uint32_t units,std::uint64_t createdAtMs,
 std::span<const ItemOwnerFence>,std::span<const PersistentItem>);
std::expected<ItemCommit,ItemPersistenceError> MakeDungeonEntryResolution(const ItemCatalog&,std::array<std::uint8_t,16>,
 const DungeonEntryMutation& reservation,bool consume,std::span<const ItemOwnerFence>,
 std::span<const ItemStorageLayout>,std::span<const PersistentItem>);
}
