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
// checksum differences, wrong footprints or an unknown catalog. A missing
// owner is MissingOwner, a newer session StaleSession.
std::expected<LoadedItemOwner,ItemPersistenceError> LoadItemOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner,std::uint64_t sessionGeneration);
// The same read as one owned job for the existing async SQL queue. The
// completion must be the exact job that PrepareLoadItemOwner returned.
std::expected<SqlTransaction,ItemPersistenceError> PrepareLoadItemOwner(const ItemCatalog&,ItemOwner,std::uint64_t sessionGeneration);
std::expected<LoadedItemOwner,ItemPersistenceError> LoadItemOwnerCompleted(const ItemCatalog&,ItemOwner,std::uint64_t sessionGeneration,_SQLMsg&);

// --- Owner provisioning and sessions (F4.2, IC-034) ----------------------
// Explicit creation of an empty owner: fence (session 1, revision 1), its
// storages and wallets with the given balances. Never overwrites: an
// existing owner is OwnerExists and nothing changes. Wallet revisions are 1.
struct ItemOwnerProvision
{
    ItemOwner owner;
    std::vector<ItemStorageLayout> storages;
    std::vector<ItemWallet> wallets;
};
std::expected<SqlTransaction,ItemPersistenceError> PrepareProvisionItemOwner(const ItemCatalog&,const ItemOwnerProvision&);
std::expected<ItemOwnerFence,ItemPersistenceError> ProvisionItemOwnerCompleted(const ItemCatalog&,const ItemOwnerProvision&,_SQLMsg&);
std::expected<ItemOwnerFence,ItemPersistenceError> ProvisionItemOwner(CAsyncSQL&,const ItemCatalog&,const ItemOwnerProvision&);

// Takes the owner for a new session: one transaction increments the durable
// session generation and returns it with the current state revision. Every
// write or load of an older session is refused afterwards. The increment
// waits for an in-flight writer of the same owner (fence row lock), so a load
// in a later transaction sees that writer's committed state. A claim is safe
// to repeat after CommitUnknown: the newest claim always wins.
std::expected<SqlTransaction,ItemPersistenceError> PrepareClaimItemOwner(const ItemCatalog&,ItemOwner);
std::expected<ItemOwnerFence,ItemPersistenceError> ClaimItemOwnerCompleted(const ItemCatalog&,ItemOwner,_SQLMsg&);
std::expected<ItemOwnerFence,ItemPersistenceError> ClaimItemOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner);
} // namespace auriga::item::persistence
