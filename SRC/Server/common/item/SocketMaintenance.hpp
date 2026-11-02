#pragma once
#include "PersistentItems.hpp"
#include "TypedItemDetails.hpp"

namespace auriga::item
{
enum class SocketMaintenanceStatus : std::uint8_t { Closed,Ready,Applied,Pending,Busy,Invalid,Denied,NoEffect,NoRoom,Stale,Failed,Level,NoTarget,Broken,NoSocket,Conflict,Incompatible };
using SocketMaintenanceError=SocketMaintenanceStatus;
struct SocketMaintenanceActor {std::uint16_t level=0;std::uint8_t characterClass=0,sex=0;bool inCombat=false;};
struct SocketMaintenanceReturn {std::uint8_t socket;ItemKey key;std::uint16_t level;bool operator==(const SocketMaintenanceReturn&)const=default;};
struct SocketMaintenanceQuote
{
    std::uint8_t kind=0;std::uint16_t units=0;std::uint32_t available=0;
    std::array<StoneSocketSlot,EquipmentStoneSockets> before{},after{};
    std::vector<SocketMaintenanceReturn> returns;
    // v2: insertion previews both possible outcomes before any server draw.
    std::uint16_t chance=0;std::uint8_t slot=255;
    std::array<StoneSocketSlot,EquipmentStoneSockets> failure{};
    // v3 accessory operations (5 unlock, 6 charge), disjoint from stone slots.
    AccessorySocketState accessoryBefore{},accessoryAfter{},accessoryFailure{};
    std::uint8_t maximum=0;
    std::uint32_t increaseBp=0;
    std::int64_t minimumRaw=0;
    std::int32_t minimumBp=0,minimumInteger=0;
    bool operator==(const SocketMaintenanceQuote&)const=default;
};
struct SocketMaintenancePlan {ItemCommit commit;SocketMaintenanceQuote quote;bool succeeded=true;};
inline constexpr std::uint16_t SocketMaintenanceOutcome=0x1501;
// Narrow exception to the no-new-retired-items rule: verifies an exact
// extraction debit and complete outputs against the embedded before image.
bool IsSocketMaintenanceRecovery(const ItemCatalog&,const ItemCommit&);
bool SocketMaintenanceTarget(const ItemCatalog&,const PersistentItem&,ItemOwner);
std::expected<SocketMaintenancePlan,SocketMaintenanceError> MakeSocketMaintenanceCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    ItemInstanceId source,ItemInstanceId target,SocketMaintenanceActor,std::span<const ItemInstanceId> outputs);
// GC235 v2, 162-byte terminal envelope. v1 golden fixtures remain readable.
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeSocketMaintenance(const ItemCatalog&,
    std::uint64_t,const PersistentItem*,const PersistentItem*,std::uint64_t,SocketMaintenanceStatus,
    const SocketMaintenanceQuote&,std::span<const std::uint64_t> targets);
}
