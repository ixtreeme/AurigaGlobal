#pragma once
#include "TypedItemDetails.hpp"
namespace auriga::item
{
struct RecoveryRuntimeView
{
    std::uint8_t status=0,error=255;
    std::uint64_t control=0,preview=0;
    const PersistentItem* source=nullptr;
    RecoveryPools pools;
    RecoverySchedule state;
    std::uint64_t cooldownRemaining=0;
    std::span<const PersistentItem> items{};
};
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeRecoveryRuntime(const ItemCatalog&,const RecoveryRuntimeView&);
}
