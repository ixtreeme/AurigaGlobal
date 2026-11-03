#pragma once
#include <item/PersistentItems.hpp>
#include <item/ItemMissions.hpp>
#include <SQL/PreparedTransaction.h>
#include <span>
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
    RecoverySchedule recovery;
};
// Dedicated existing CAsyncSQL direct connection; no second SQL engine.
// Values are bound; all tables are fixed and fully qualified. The returned
// transaction may also be dispatched through ReturnTransactionPrepared.
std::expected<SqlTransaction,ItemPersistenceError> PrepareItemWrite(const ItemCatalog&,const ItemCommit&);
std::expected<ItemReceipt,ItemPersistenceError> ItemWriteCompleted(const ItemCatalog&,const ItemCommit&,_SQLMsg&);
// A locking current read after reconnect: waits for an already inserted receipt
// to commit/roll back. Following CommitUnknown (all write steps completed), a
// confirmed absence allows a retry. It does not settle a request not yet sent.
std::expected<std::optional<ItemReceipt>,ItemPersistenceError> FindItemReceipt(CAsyncSQL&,const ItemCatalog&,const ItemCommit&);
std::expected<ItemWriteResult,ItemPersistenceError> CommitItems(CAsyncSQL&,const ItemCatalog&,const ItemCommit&);
// Historical item-mission completion, authorized by the CURRENT owner's
// session. Match the complete immutable contract, not a newly rebuilt before
// image. A duplicate after relog/catalog balance changes never spends twice.
std::expected<bool,ItemPersistenceError> FindItemMissionReceipt(CAsyncSQL&,const ItemCatalog&,ItemOwnerFence,
    const ItemMission&,ItemMissionProof);
// One consistent, bounded snapshot; rejects corrupt/missing child state,
// checksum differences, wrong footprints, unknown catalog or stale sessions.
std::expected<LoadedItemOwner,ItemPersistenceError> LoadItemOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner,std::uint64_t sessionGeneration);
// The login owner takes a NEW session generation for every login, in one
// transaction. Any commit still carrying an older generation (a previous
// login's late write) then fails its fence. MissingOwner: not provisioned.
std::expected<std::uint64_t,ItemPersistenceError> ClaimOwnerSession(CAsyncSQL&,const ItemCatalog&,ItemOwner);
// Explicit provisioning by the importer, character creation or a test tool
// (never by the item writer). Creates fence (session 1), storages and wallets
// atomically. An owner that already exists is left untouched: false.
std::expected<bool,ItemPersistenceError> ProvisionOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner,
    std::span<const ItemStorageLayout>,std::span<const ItemWallet>);
// Runtime-created items take their IDs from blocks reserved here, so several
// writers (game cores, the test bench) never collide (F4 C7a). One transaction
// moves the shared counter; IDs a writer leaves unused are never handed out
// again. The half-open range [first, end) holds exactly `count` IDs.
struct ItemInstanceIdBlock { std::uint64_t first=0, end=0; };
inline constexpr std::uint32_t MaxItemInstanceIdBlock=1'000'000;
std::expected<ItemInstanceIdBlock,ItemPersistenceError> ReserveItemInstanceIds(CAsyncSQL&,const ItemCatalog&,std::uint32_t count);
std::expected<LoadedItemOwner,ItemPersistenceError> LoadCurrentItemOwner(CAsyncSQL&,const ItemCatalog&,ItemOwner);
// Explicit importer/test-bench provisioning, never called by the runtime writer.
std::expected<void,ItemPersistenceError> ProvisionGroundStorage(CAsyncSQL&,const ItemCatalog&,ItemOwner);
// Fences the prior process of this exact core, then expires its old registry
// entries. Other live cores' rows are never selected. No world respawn.
std::expected<std::uint64_t,ItemPersistenceError> BeginOwnedGroundRun(CAsyncSQL&,const ItemCatalog&,std::string_view core);
} // namespace auriga::item::persistence
