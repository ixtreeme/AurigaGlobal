#pragma once
#include "PersistentItems.hpp"
#include "TypedItemDetails.hpp"

namespace auriga::item
{
enum class ContainerOpeningError { Invalid, Denied, KeyRequired, WrongKey, NoRoom, WalletLimit, Unsupported, Level, Unavailable };
enum class ContainerOpeningStatus : std::uint8_t { Closed, Ready, Opened, Pending, Busy, Invalid, Denied, KeyRequired, WrongKey, NoRoom, WalletLimit, Unsupported, Stale, Failed, Level, Cooldown };
struct ContainerActor { std::uint16_t level=0; std::uint8_t characterClass=0,sex=0; bool inCombat=false; };
struct ContainerOpeningRequest { ItemInstanceId box; std::optional<ItemInstanceId> key; ContainerActor actor; std::uint64_t nowUtcMs=0; };
struct ContainerOpeningPlan { ItemCommit commit; std::vector<std::uint16_t> won; std::uint64_t gold=0; };
inline constexpr std::uint16_t ContainerOpeningOutcome=0x1301;
// Exhaustive admission of all independent item outcomes is bounded. Gold is
// checked at its maximum; guaranteed item rows do not multiply this budget.
inline constexpr std::size_t MaxUncertainContainerItems=8;
bool ContainerItemUsable(const PersistentItem&,ItemOwner);
std::expected<void,ContainerOpeningError> ValidateContainerContents(const ItemCatalog&,ItemKey);
std::expected<ContainerOpenRequest,ContainerOpeningError> CheckContainerOpening(const ItemCatalog&,
    ContainerOpeningRequest,ItemOwner,std::span<const PersistentItem>);
std::expected<ContainerOpeningPlan,ContainerOpeningError> MakeContainerOpeningCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    std::span<const ItemWallet>,ContainerOpeningRequest,std::span<const ItemInstanceId>,const LootDraw&);
// GC233 v1; empty terminal envelope is 69 bytes. Extra metadata is GC227.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeContainerOpening(const ItemCatalog&,
    std::uint64_t session,const PersistentItem* box,const PersistentItem* key,std::uint64_t ownerRevision,
    ContainerOpeningStatus,std::span<const std::uint64_t> keys,std::span<const std::uint16_t> won);
}
