#pragma once
#include "PersistentItems.hpp"
#include "TypedItemDetails.hpp"

namespace auriga::item
{
enum class SocketMaintenanceStatus : std::uint8_t { Closed,Ready,Applied,Pending,Busy,Invalid,Denied,NoEffect,NoRoom,Stale,Failed,Level,NoTarget };
using SocketMaintenanceError=SocketMaintenanceStatus;
struct SocketMaintenanceActor {std::uint16_t level=0;std::uint8_t characterClass=0,sex=0;bool inCombat=false;};
struct SocketMaintenanceReturn {std::uint8_t socket;ItemKey key;std::uint16_t level;bool operator==(const SocketMaintenanceReturn&)const=default;};
struct SocketMaintenanceQuote
{
    std::uint8_t kind=0;std::uint16_t units=0;std::uint32_t available=0;
    std::array<StoneSocketSlot,EquipmentStoneSockets> before{},after{};
    std::vector<SocketMaintenanceReturn> returns;
    bool operator==(const SocketMaintenanceQuote&)const=default;
};
struct SocketMaintenancePlan {ItemCommit commit;SocketMaintenanceQuote quote;};
inline constexpr std::uint16_t SocketMaintenanceOutcome=0x1501;
// Narrow exception to the no-new-retired-items rule: verifies an exact
// extraction debit and complete outputs against the embedded before image.
bool IsSocketMaintenanceRecovery(const ItemCatalog&,const ItemCommit&);
bool SocketMaintenanceTarget(const ItemCatalog&,const PersistentItem&,ItemOwner);
std::expected<SocketMaintenancePlan,SocketMaintenanceError> MakeSocketMaintenanceCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    ItemInstanceId source,ItemInstanceId target,SocketMaintenanceActor,std::span<const ItemInstanceId> outputs);
// GC235 v1, 129-byte terminal envelope, GC227 metadata and target candidates.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeSocketMaintenance(const ItemCatalog&,
    std::uint64_t,const PersistentItem*,const PersistentItem*,std::uint64_t,SocketMaintenanceStatus,
    const SocketMaintenanceQuote&,std::span<const std::uint64_t> targets);
}
