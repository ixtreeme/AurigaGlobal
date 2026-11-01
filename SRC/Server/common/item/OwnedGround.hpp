#pragma once
#include "PersistentItems.hpp"

namespace auriga::item
{
// Own-item transfers (C7d-1). The runtime admits the character and controls
// the ground entry's claim, visibility, lifetime and escrow reservation.
// Storages are pre-provisioned. No runtime world entry is created here.
// Whole drops retain their ID; splits copy the full state to a fresh ID.
// Zero units means all; an explicit amount above the stack is refused.
// Supply complete loaded snapshots for the participating owners. Bound items
// and catalog DROP/GIVE restrictions return TransferDenied.
std::expected<ItemCommit,ItemPersistenceError> MakeGroundDropCommit(const ItemCatalog&,
    std::array<std::uint8_t,16> operationId,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    ItemInstanceId source,ItemPlacement escrow,std::uint32_t units=0,
    std::optional<ItemInstanceId> splitId={});

// Moves the concrete escrow instance to a free inventory position. Both
// owners' fences participate atomically; no merge or ItemGrant regeneration.
std::expected<ItemCommit,ItemPersistenceError> MakeGroundPickupCommit(const ItemCatalog&,
    std::array<std::uint8_t,16> operationId,ItemOwnerFence sourceOwner,ItemOwnerFence receiver,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    ItemInstanceId source,ItemPlacement inventory);

// Only escrow instances may expire here; the caller decides the deadline.
std::expected<ItemCommit,ItemPersistenceError> MakeGroundExpireCommit(const ItemCatalog&,
    std::array<std::uint8_t,16> operationId,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId source);
} // namespace auriga::item
