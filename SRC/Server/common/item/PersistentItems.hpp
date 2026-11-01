#pragma once
#include "ItemCatalog.hpp"
#include "ItemInstanceId.hpp"
#include <deque>
#include <optional>
#include <span>
#include <variant>

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
inline constexpr std::uint64_t OwnedGroundStorageId = 0xfffffffffffffffeULL;
struct ItemGroundChange
{
    std::string core;
    std::uint64_t run = 0, storedRun = 0, instance = 0;
    bool create = false;
};
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
    // Ground registry and the concrete item are written in the same SQL transaction.
    std::optional<ItemGroundChange> ground;
};
inline constexpr std::size_t MaxItemCommitItems=32, MaxItemCommitOwners=8,
    MaxItemCommitStorages=16, MaxItemCommitResult=4096, MaxLoadedOwnerItems=4096;
enum class ItemPersistenceError
{
    InvalidOwner, InvalidStorage, InvalidItem, MissingItem, UnsupportedState,
    InvalidLevel, InvalidAttributes, InvalidSockets, InvalidPlacement,
    InvalidOperation, InvalidFence, InvalidChange, InvalidWallet, LimitExceeded,
    CatalogMismatch, HashFailure, SqlFailure, Conflict, CommitUnknown,
    ReceiptMismatch, InvalidStoredData, MissingOwner, RequirementNotMet, TransferDenied,
};
bool ValidItemOwner(ItemOwner) noexcept;
bool ValidItemStorage(const ItemStorageLayout&) noexcept;
// Phase-one persistence deliberately refuses categories whose additional
// lifetime/pet/tool/etc. state is not represented here. Never zero-fill them.
// Recovery consumables carry none, except the automatic kind's charge.
bool SupportsPersistentItem(const ItemCatalog&,const ItemCatalogEntry&) noexcept;
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
// Moves one placed item of the owner to another anchor, inside the same or
// another inventory storage of that owner (F4 C3). Every target cell must be
// free: no swap, no stack merge and no equipping here (each a later slice).
std::expected<ItemCommit,ItemPersistenceError> MakeMoveCommit(const ItemCatalog&,std::array<std::uint8_t,16> operationId,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,ItemPlacement target);
// The character facts the equip rules read; the owner thread supplies them.
struct EquipContext
{
    std::uint16_t level=0;
    std::uint8_t characterClass=0; // 0..3: warrior, assassin, sura, shaman (use policy bits)
    std::uint8_t sex=0;            // 0 male, 1 female
};
// Equips one inventory item into its own slot of the owner's equipment
// storage 1 (weapon/armor/accessory row decides; F4 C4). An item already in
// that slot swaps back into the freed inventory place when it fits there.
// An OnEquip item binds to the owner. RequirementNotMet: level, class or sex.
// Costumes and other categories are later slices (UnsupportedState).
std::expected<ItemCommit,ItemPersistenceError> MakeEquipCommit(const ItemCatalog&,std::array<std::uint8_t,16> operationId,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,EquipContext);
// Takes an equipped item off into free inventory cells.
std::expected<ItemCommit,ItemPersistenceError> MakeUnequipCommit(const ItemCatalog&,std::array<std::uint8_t,16> operationId,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,ItemPlacement target);
// Uses up `units` of one inventory item (F4 C5). The last unit destroys it:
// a tombstone with no count and no placement. The effect of the use is the
// caller's own plan (e.g. PlanInstantRecovery); this only writes the item.
std::expected<ItemCommit,ItemPersistenceError> MakeConsumeCommit(const ItemCatalog&,std::array<std::uint8_t,16> operationId,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,std::uint32_t units);
// Gives new units of one item state to the owner's inventory (F4 C7a): a drop,
// a purchase or a reward. Stackable units first top up the owner's matching
// inventory stacks (same key, level and binding, no instance state), in storage
// and cell order; the rest become new items in the first free cells, taking
// IDs from the front of `ids` (the commit's new items say how many). All or
// nothing: InvalidPlacement when it does not fit, LimitExceeded when `ids`
// runs short. An OnAcquire item binds to the owner.
struct ItemGrant
{
    ItemKey key;
    std::uint16_t refinementLevel=0;
    std::uint32_t count=1;
};
std::expected<ItemCommit,ItemPersistenceError> MakeGrantCommit(const ItemCatalog&,std::array<std::uint8_t,16> operationId,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const ItemGrant&,std::span<const ItemInstanceId> ids);
// Grants that wait while an earlier typed operation of the owner is in the
// store (C7b-3 monster drops). A grant joins the first waiting grant of the
// same item state whose sum stays within `mergeLimit` units (one stack), else
// it queues behind. False, and nothing changes, for zero units or a queue
// already holding MaxPendingGrants grants.
inline constexpr std::size_t MaxPendingGrants=256;
bool QueueGrant(std::deque<ItemGrant>& pending,const ItemGrant& grant,std::uint32_t mergeLimit);
// Refines one inventory item of the owner with one catalog recipe (F4 C6).
// Only the owner's live inventory items take part: an equipped item is never
// the source or a material. The Gold wallet pays. The server supplies the
// recorded roll, and `admitted` is its service, distance and state decision;
// this layer does not decide them. The plan is returned for the outcome.
struct OwnerRefinementRequest
{
    std::array<std::uint8_t,16> operationId{};
    ItemInstanceId source;
    std::uint32_t recipeId=0;
    std::uint16_t recordedRoll=0;
    bool admitted=false;
};
struct OwnerRefinement { RefinementPlan plan; ItemCommit commit; };
using OwnerRefinementError=std::variant<RefinementUseError,ItemPersistenceError>;
std::expected<OwnerRefinement,OwnerRefinementError> MakeOwnerRefinement(const ItemCatalog&,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::span<const ItemWallet>,const OwnerRefinementRequest&);
} // namespace auriga::item
