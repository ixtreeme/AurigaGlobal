#pragma once
#include <item/PersistentItems.hpp>
#include <SQL/PreparedTransaction.h>
class CAsyncSQL;
struct _SQLMsg;

namespace auriga::item::persistence
{
struct ItemReceipt
{
    ItemCatalogDigest requestHash{};
    std::uint16_t outcomeCode=0;
    std::vector<std::uint8_t> result;
};
struct ItemWriteResult { ItemReceipt receipt; bool replay=false; };
struct LoadedItemOwner
{
    ItemOwnerFence fence;
    std::vector<ItemStorageLayout> storages;
    std::vector<ItemWallet> wallets;
    std::vector<PersistentItem> items;
};
// Dedicated existing CAsyncSQL direct connection; no second SQL engine.
// Values are bound; all tables are fixed and fully qualified. The returned
// transaction may also be dispatched through ReturnTransactionPrepared.
std::expected<SqlTransaction,ItemPersistenceError> PrepareItemWrite(const ItemCatalog&,const ItemCommit&);
std::expected<ItemReceipt,ItemPersistenceError> ItemWriteCompleted(const ItemCatalog&,const ItemCommit&,_SQLMsg&);
// Read after a reconnect to reconcile CommitUnknown. An absent receipt is NOT
// permission to replay while the original owner/transaction is still in flight.
std::expected<std::optional<ItemReceipt>,ItemPersistenceError> FindItemReceipt(CAsyncSQL&,const ItemCatalog&,const ItemCommit&);
std::expected<ItemWriteResult,ItemPersistenceError> CommitItems(CAsyncSQL&,const ItemCatalog&,const ItemCommit&);
// One consistent, bounded snapshot; rejects corrupt/missing child state,
// checksum differences, wrong footprints, unknown catalog or stale sessions.
std::expected<LoadedItemOwner,ItemPersistenceError> LoadItemOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner,std::uint64_t sessionGeneration);
} // namespace auriga::item::persistence
