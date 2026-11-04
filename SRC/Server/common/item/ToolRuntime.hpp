#pragma once
#include "PersistentItems.hpp"
#include "TypedInventoryWire.hpp"
namespace auriga::item
{
enum class ToolRuntimeAction : std::uint8_t { Info=1, Refine=2, Bait=3, Practice=4 };
struct ToolRuntimeActor
{
    std::uint16_t level=0;std::uint8_t characterClass=0,sex=0;
    bool admitted=false,inCombat=false,fishingActive=false,serviceAvailable=false,completedResourceAction=false;
};
struct ToolRuntimeRequest
{
    ItemInstanceId source;std::optional<ItemInstanceId> target;
    ToolRuntimeAction action{};ToolRuntimeActor actor;std::uint32_t roll=0;
};
struct ToolRuntimePlan { ItemCommit commit;std::uint8_t outcome=0;std::uint16_t level=0;ToolInstanceState state; };
std::expected<void,GatheringError> CheckToolRuntime(const ItemCatalog&,ItemOwner,std::span<const PersistentItem>,ToolRuntimeRequest);
std::expected<ToolRuntimePlan,GatheringError> MakeToolRuntimeCommit(const ItemCatalog&,std::array<std::uint8_t,16>,ItemOwnerFence,
    std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ToolRuntimeRequest);
// Practice is deliberately absent from CG254. Only a completed server-owned
// resource operation may set completedResourceAction and reuse its operation ID.
struct ToolRuntimeView
{
    std::uint8_t status=0,error=255,outcome=0;std::uint64_t session=0;
    const PersistentItem* source=nullptr;const PersistentItem* tool=nullptr;ToolRuntimeAction action{};
    std::uint32_t serviceVid=0,roll=0;std::uint16_t afterLevel=0;ToolInstanceState after;
};
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeToolRuntime(const ItemCatalog&,const ToolRuntimeView&);
}
