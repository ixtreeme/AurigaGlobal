#pragma once
#include "ItemPersistenceSql.hpp"
#include <item/DungeonEntry.hpp>
namespace auriga::item::persistence {
// Before concrete writes: bind/resolve the requested reservation and refuse
// every ordinary mutation of an instance still reserved for a dungeon entry.
void DungeonEntryWriteBefore(SqlTransaction&,const ItemCommit&);
std::expected<std::vector<DungeonEntryMutation>,ItemPersistenceError> LoadPendingDungeonEntries(CAsyncSQL&,const ItemCatalog&,std::uint64_t owner);
std::expected<unsigned,ItemPersistenceError> RecoverDungeonEntries(CAsyncSQL&,const ItemCatalog&,ItemOwner,std::uint64_t session);
}
