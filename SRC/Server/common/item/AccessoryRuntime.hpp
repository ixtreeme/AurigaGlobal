#pragma once
#include "SocketMaintenance.hpp"
#include "SocketServices.hpp"

namespace auriga::item
{
bool AccessoryMaterial(const ItemCatalog&,ItemKey);
bool AccessoryTarget(const ItemCatalog&,const PersistentItem&,ItemOwner);
std::expected<SocketMaintenancePlan,SocketMaintenanceError> MakeAccessorySocketCommit(const ItemCatalog&,
    std::array<std::uint8_t,16>,ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,
    ItemInstanceId,ItemInstanceId,SocketMaintenanceActor,AccessoryServiceKind,std::uint16_t);
// Add elapsed equipped time to the same atomic operation, retaining its exact
// before images and incrementing each changed instance only once.
std::expected<void,ItemPersistenceError> SettleAccessoryTime(const ItemCatalog&,ItemCommit&,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,std::uint64_t);
std::expected<void,ItemPersistenceError> ProjectAccessoryTime(const ItemCatalog&,std::span<PersistentItem>,std::uint64_t);
}
