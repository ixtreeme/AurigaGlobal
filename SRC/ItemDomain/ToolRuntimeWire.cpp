#include <item/ToolRuntime.hpp>
#include <item/TypedItemDetails.hpp>
namespace auriga::item
{
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeToolRuntime(const ItemCatalog& c,const ToolRuntimeView& v)
{
    using E=TypedInventoryWireError;
    if(v.status>8 || (v.error!=255 && v.error>17) || v.outcome>4 || (v.source ? !v.session || static_cast<unsigned>(v.action)<1 || static_cast<unsigned>(v.action)>3 : v.session || v.tool || static_cast<unsigned>(v.action) || v.serviceVid || v.outcome || v.roll))return std::unexpected(E::BadValue);
    const auto* row=v.tool ? std::get_if<ItemToolRow>(&c.Find(v.tool->key)->category) : nullptr;
    const auto* level=row && v.tool->refinementLevel<c.FindToolProfile(row->profile)->levels.size() ? &c.FindToolProfile(row->profile)->levels[v.tool->refinementLevel] : nullptr;
    if(v.tool && (!row || !level || !v.tool->tool))return std::unexpected(E::BadValue);
    std::vector<PersistentItem> metadata;if(v.source)metadata.push_back(*v.source);if(v.tool && (!v.source || v.tool->id!=v.source->id))metadata.push_back(*v.tool);
    const auto details=EncodeTypedItemDetails(c,metadata);if(!details)return std::unexpected(details.error());
    std::vector<std::byte> out;const auto put=[&](std::uint64_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)out.push_back(std::byte((n>>(8*i))&255));};
    const auto key=[&](const PersistentItem* i){put(i ? static_cast<unsigned>(i->key.Category()) : 0,2);put(i ? i->key.LocalId() : 0,4);};
    put(254,1);put(0,4);put(2,1);put(v.status,1);put(v.error,1);put(v.session,8);put(v.source ? v.source->id.Value() : 0,8);put(v.source ? v.source->revision : 0,8);key(v.source);put(v.source ? v.source->count : 0,4);put(static_cast<unsigned>(v.action),1);
    put(v.tool ? v.tool->id.Value() : 0,8);put(v.tool ? v.tool->revision : 0,8);key(v.tool);put(v.tool ? v.tool->refinementLevel : 0,2);put(row ? static_cast<unsigned>(row->kind) : 0,1);
    put(level ? level->power : 0,4);put(level ? level->gatherBonusBp : 0,2);put(v.tool ? v.tool->tool->trainingPoints : 0,4);put(level ? level->requiredPoints : 0,4);put(v.tool ? v.tool->tool->baitPower : 0,4);put(row ? static_cast<unsigned>(row->fishingPool) : 0,1);
    put(level ? level->practiceNumerator : 0,4);put(level ? level->practiceDenominator : 0,4);put(level ? level->refineChanceBp : 0,2);put(level ? static_cast<unsigned>(level->failure) : 0,1);put(level ? level->failureLevel.value_or(65535) : 65535,2);put(level ? level->failureLossBp : 0,2);
    const auto* bait=v.source ? std::get_if<ItemFishingRow>(&c.Find(v.source->key)->category) : nullptr;
    put(v.tool && v.tool->placement && v.tool->placement->kind==ItemStorageKind::Equipment,1);put(v.serviceVid,4);put(bait && bait->kind==FishingKind::Bait ? bait->baitPower : 0,4);put(v.outcome,1);put(v.roll,4);put(v.afterLevel,2);put(v.after.trainingPoints,4);put(v.after.baitPower,4);put(details->size(),4);out.insert(out.end(),details->begin(),details->end());
    if(out.size()>1u<<20)return std::unexpected(E::TooLarge);for(unsigned i=0;i<4;++i)out[i+1]=std::byte((out.size()>>(8*i))&255);return out;
}
}
