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

// --- New instance IDs and lost commit acknowledgements (F4.4, IC-034) ----
// Reserves `count` fresh IDs from player.item_instance_id_allocator
// (db/migrations/item_instance/002). The range [first, first+count) is used
// by nobody else; an unused rest is skipped, never reissued.
struct ItemInstanceIdRange
{
    std::uint64_t first=0;
    std::uint32_t count=0;
    bool operator==(const ItemInstanceIdRange&) const = default;
};
inline constexpr std::uint32_t MaxReservedItemInstanceIds=65536;
std::expected<SqlTransaction,ItemPersistenceError> PrepareReserveItemInstanceIds(const ItemCatalog&,std::uint32_t count);
std::expected<ItemInstanceIdRange,ItemPersistenceError> ReserveItemInstanceIdsCompleted(const ItemCatalog&,std::uint32_t count,_SQLMsg&);
std::expected<ItemInstanceIdRange,ItemPersistenceError> ReserveItemInstanceIds(CAsyncSQL&,const ItemCatalog&,std::uint32_t count);

// Decides a commit whose COMMIT acknowledgement was lost (CommitUnknown).
// Locks every owner fence first, so it waits until the original transaction
// has finished on the server; the receipt is then read as a current read.
// A receipt means committed (exactly once); none means it never committed.
std::expected<SqlTransaction,ItemPersistenceError> PrepareReconcileItemCommit(const ItemCatalog&,const ItemCommit&);
std::expected<std::optional<ItemReceipt>,ItemPersistenceError> ReconcileItemCommitCompleted(const ItemCatalog&,const ItemCommit&,_SQLMsg&);
std::expected<std::optional<ItemReceipt>,ItemPersistenceError> ReconcileItemCommit(CAsyncSQL&,const ItemCatalog&,const ItemCommit&);
} // namespace auriga::item::persistence
