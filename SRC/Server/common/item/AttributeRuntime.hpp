#pragma once
#include "SocketMaintenance.hpp"
#include <functional>

namespace auriga::item
{
enum class AttributeRuntimeStatus : std::uint8_t {Closed,Ready,Applied,Pending,Busy,Invalid,Denied,NoEffect,NoTarget,Stale,Failed,Level,Incompatible,Insufficient,NotEnough,AlreadyLocked,NotLocked,NoAlternative,NoCandidate,UnassignedTier,AttemptFailed};
struct AttributeRuntimeTier {std::uint8_t tier;std::uint16_t chance;bool operator==(const AttributeRuntimeTier&)const=default;};
struct AttributeRuntimeChoice {BonusId bonus;std::uint32_t weight;std::vector<std::int64_t> values;bool operator==(const AttributeRuntimeChoice&)const=default;};
struct AttributeRuntimeQuote
{
    std::uint8_t operation=0,pool=0,capacity=0,draws=0;
    std::uint16_t units=0,chance=0;std::uint32_t available=0;bool consumeFailure=false;
    InstanceAttributes before;
    std::vector<AttributeRuntimeTier> tiers;
    std::vector<AttributeRuntimeChoice> choices;
    std::vector<std::uint8_t> locks;
    bool operator==(const AttributeRuntimeQuote&)const=default;
};
struct AttributeRuntimePlan {ItemCommit commit;InstanceAttributes after;bool succeeded=false;};
using AttributeRandom=std::function<std::optional<std::uint32_t>(std::uint32_t)>; // inclusive 1..maximum, no modulo bias
bool AttributeRuntimeTarget(const ItemCatalog&,const PersistentItem&,ItemOwner);
std::expected<AttributeRuntimeQuote,AttributeRuntimeStatus> PrepareAttributeRuntime(const ItemCatalog&,ItemOwnerFence,
    std::span<const PersistentItem>,ItemInstanceId,ItemInstanceId,SocketMaintenanceActor);
std::expected<AttributeRuntimePlan,AttributeRuntimeStatus> MakeAttributeRuntimeCommit(const ItemCatalog&,std::array<std::uint8_t,16>,
    ItemOwnerFence,std::span<const ItemStorageLayout>,std::span<const PersistentItem>,ItemInstanceId,ItemInstanceId,
    SocketMaintenanceActor,const AttributeRuntimeQuote&,const AttributeRandom&);
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeAttributeRuntime(const ItemCatalog&,std::uint64_t,
    const PersistentItem*,const PersistentItem*,std::uint64_t,AttributeRuntimeStatus,const AttributeRuntimeQuote&,
    const InstanceAttributes*,std::span<const std::uint64_t>);
}
