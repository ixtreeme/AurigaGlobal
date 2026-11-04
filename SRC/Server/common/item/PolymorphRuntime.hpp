#pragma once
#include "PersistentItems.hpp"
#include "TypedInventoryWire.hpp"
namespace auriga::item
{
const ItemPolymorphRow* PolymorphItem(const ItemCatalog&,ItemKey);
// Quote probes a valid decision without mutating; only Make... accepts the
// separately recorded server roll and produces the receipted write.
std::expected<PolymorphPlan,PolymorphError> QuotePolymorphRuntime(const ItemCatalog&,const PersistentItem&,const RecoverySchedule&,PolymorphRequest,std::uint64_t now);
struct PolymorphRuntimePlan { ItemCommit commit; PolymorphPlan result; };
std::expected<PolymorphRuntimePlan,PolymorphError> MakePolymorphRuntimeCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,const RecoverySchedule&,ItemInstanceId,PolymorphRequest,std::uint64_t elapsed,std::uint64_t now,std::uint16_t roll);
std::expected<ItemCommit,PolymorphError> MakePolymorphCancel(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,const RecoverySchedule&,std::uint64_t elapsed,std::uint64_t now,std::uint64_t generation);
struct PolymorphRuntimeView
{
    std::uint8_t status=0,error=255,purpose=0,outcome=0;std::uint16_t roll=0;
    std::uint64_t control=0,preview=0,now=0;
    const PersistentItem* source=nullptr;const RecoverySchedule* schedule=nullptr;
    const PolymorphRequest* actor=nullptr;std::string formName,activeName;
};
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodePolymorphRuntime(const ItemCatalog&,const PolymorphRuntimeView&);
}
