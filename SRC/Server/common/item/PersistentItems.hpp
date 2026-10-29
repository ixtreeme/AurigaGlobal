#pragma once
#include "ItemCatalog.hpp"
#include "ItemInstanceId.hpp"
#include <optional>
#include <span>

namespace auriga::item
{
// Durable ownership is never inferred from a character/account ID collision.
enum class ItemOwnerKind : std::uint8_t { Character=1, Account=2, Guild=3 };
struct ItemOwner
{
    ItemOwnerKind kind{};
    std::uint64_t id=0;
    auto operator<=>(const ItemOwner&) const = default;
};
enum class ItemStorageKind : std::uint8_t
{
    Inventory=1, Equipment=2, Safebox=3, Mall=4, GuildStorage=5,
    MountInventory=6, TradeEscrow=7, Mail=8, PendingAward=9,
};
struct ItemStorage
{
    ItemOwner owner;
    ItemStorageKind kind{};
    std::uint64_t id=0;
    auto operator<=>(const ItemStorage&) const = default;
};
struct ItemStorageLayout
{
    ItemStorage storage;
    std::uint16_t columns=0, rows=0, pages=0;
    bool operator==(const ItemStorageLayout&) const = default;
};
struct ItemPlacement
{
    ItemStorageKind kind{};
    std::uint64_t storageId=0;
    std::uint32_t anchor=0;
    bool operator==(const ItemPlacement&) const = default;
};
struct PersistentItem
{
    ItemInstanceId id;
    ItemKey key;
    ItemOwner owner;
    std::uint64_t revision=1;
    std::uint16_t refinementLevel=0;
    std::uint32_t count=1;
    std::optional<ItemOwner> boundTo;
    // Destruction retains the identity and last complete state as a tombstone.
    bool destroyed=false;
    std::optional<ItemPlacement> placement;
    InstanceAttributes attributes;
    std::array<StoneSocketSlot,EquipmentStoneSockets> stones{};
    std::optional<AccessorySocketState> accessory;
    bool operator==(const PersistentItem&) const = default;
};
struct ItemOwnerFence
{
    ItemOwner owner;
    std::uint64_t sessionGeneration=0, revision=0;
    bool operator==(const ItemOwnerFence&) const = default;
};
struct ItemWallet
{
    ItemOwner owner;
    WalletCurrency currency{};
    std::uint64_t revision=0, balance=0;
    bool operator==(const ItemWallet&) const = default;
};
struct ItemChange { std::optional<PersistentItem> before; PersistentItem after; };
struct ItemWalletChange { ItemWallet before, after; };
enum class ItemOperationKind : std::uint8_t { Grant=1, Move=2, Refine=3, Service=4, Consume=5, Trade=6 };
struct ItemCommit
{
    std::array<std::uint8_t,16> operationId{};
    std::uint64_t catalogRevision=0;
    ItemCatalogDigest catalogHash{};
    ItemOperationKind kind{};
    std::vector<ItemOwnerFence> owners;
    std::vector<ItemStorageLayout> storages;
    std::vector<ItemChange> items;
    std::vector<ItemWalletChange> wallets;
    std::uint16_t outcomeCode=0;
    std::vector<std::uint8_t> result;
};
inline constexpr std::size_t MaxItemCommitItems=32, MaxItemCommitOwners=8,
    MaxItemCommitStorages=16, MaxItemCommitResult=4096, MaxLoadedOwnerItems=4096;
enum class ItemPersistenceError
{
    InvalidOwner, InvalidStorage, InvalidItem, MissingItem, UnsupportedState,
    InvalidLevel, InvalidAttributes, InvalidSockets, InvalidPlacement,
    InvalidOperation, InvalidFence, InvalidChange, InvalidWallet, LimitExceeded,
    CatalogMismatch, HashFailure, SqlFailure, Conflict, CommitUnknown,
    ReceiptMismatch, InvalidStoredData,
};
bool ValidItemOwner(ItemOwner) noexcept;
bool ValidItemStorage(const ItemStorageLayout&) noexcept;
// Phase-one persistence deliberately refuses categories whose additional
// lifetime/pet/tool/etc. state is not represented here. Never zero-fill them.
bool SupportsPersistentItem(const ItemCatalogEntry&) noexcept;
std::expected<void,ItemPersistenceError> ValidatePersistentItem(const ItemCatalog&,const PersistentItem&);
std::expected<std::vector<std::uint32_t>,ItemPersistenceError> OccupiedItemCells(
    const ItemCatalog&,const PersistentItem&,const ItemStorageLayout&);
std::expected<void,ItemPersistenceError> ValidateItemCommit(const ItemCatalog&,const ItemCommit&);
// Canonical LE, including complete expected/after states and the receipt result.
// Sorted sets give the same digest regardless of caller vector ordering.
std::expected<ItemCatalogDigest,ItemPersistenceError> ItemCommitDigest(const ItemCatalog&,const ItemCommit&);
std::expected<ItemCatalogDigest,ItemPersistenceError> PersistentItemDigest(const ItemCatalog&,const PersistentItem&);

// Translates the new native refinement plan directly into an atomic write set.
// No vnum mapping, RNG, SQL execution or ECS mutation. Exact source/material/
// wallet snapshots and every placement must still be checked by the DB writer.
std::expected<ItemCommit,ItemPersistenceError> MakeRefinementCommit(const ItemCatalog&,const RefinementPlan&,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const ItemWallet&);
std::expected<ItemCommit,ItemPersistenceError> MakeAttributeCommit(const ItemCatalog&,const AttributePlan&,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>);
std::expected<ItemCommit,ItemPersistenceError> MakeSocketServiceCommit(const ItemCatalog&,const SocketServicePlan&,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>);
} // namespace auriga::item
