#pragma once
#include "SocketMaintenance.hpp"

namespace auriga::item
{
inline constexpr std::uint16_t SocketInsertionSuccess=0x1601,SocketInsertionFailure=0x1602;
// Explicit server chance, one recorded draw (0 at 0/100% endpoints). No RNG.
// Quote includes both outcomes and is independent of the recorded decision.
std::expected<SocketMaintenancePlan,SocketMaintenanceError> MakeSocketInsertionCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    ItemInstanceId source,ItemInstanceId target,SocketMaintenanceActor,std::uint16_t chance,std::uint16_t roll);
}
