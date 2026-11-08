#pragma once
#include "ItemPersistenceSql.hpp"
#include <item/StorageRuntime.hpp>
namespace auriga::item::persistence {
// Authenticated runtime bootstrap, idempotent and preserving every existing row.
std::expected<void,ItemPersistenceError> EnsureRuntimeStorageOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner);
std::expected<LoadedItemOwner,ItemPersistenceError> LoadAuthorizedStorageOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner,
    std::uint64_t session,const StorageAuthorization&);
// Once empty, close the shared gate before handing disband to the legacy guild
// coordinator. All cores' subsequent typed deposits then fail inside SQL.
std::expected<void,ItemPersistenceError> CloseEmptyTypedGuild(CAsyncSQL&,const ItemCatalog&,std::uint32_t guild,std::uint32_t leader);
}
