#include <item/PolymorphRuntime.hpp>
#include <item/EffectRuntime.hpp>
#include <algorithm>
namespace auriga::item
{
const ItemPolymorphRow* PolymorphItem(const ItemCatalog& c,ItemKey key)
{const auto* e=c.Find(key);return e ? std::get_if<ItemPolymorphRow>(&e->category) : nullptr;}
namespace
{
PolymorphRequest Request(const PersistentItem& source,const RecoverySchedule& schedule,PolymorphRequest actor,std::uint64_t now)
{
    actor.item=source.key;actor.state=source.polymorph.value_or(PolymorphInstanceState{});
    actor.instanceId=source.id.Value();actor.revision=source.revision;actor.ownerId=source.owner.id;
    actor.count=source.count;actor.refinementLevel=source.refinementLevel;actor.nowUtcMs=now;
    actor.alreadyPolymorphed|=schedule.polymorph.has_value();return actor;
}
}
std::expected<PolymorphPlan,PolymorphError> QuotePolymorphRuntime(const ItemCatalog& c,const PersistentItem& source,const RecoverySchedule& schedule,PolymorphRequest actor,std::uint64_t now)
{
    if(!ValidatePersistentItem(c,source) || !source.polymorph || !PlainInventoryItem(source,{ItemOwnerKind::Character,actor.actorId}) || !ValidateRecoverySchedule(c,schedule))return std::unexpected(PolymorphError::InvalidState);
    actor=Request(source,schedule,actor,now);actor.recordedRoll=0;
    auto plan=PlanPolymorph(c,actor);
    if(!plan && plan.error()==PolymorphError::InvalidDecision){actor.recordedRoll=1;plan=PlanPolymorph(c,actor);}return plan;
}
std::expected<PolymorphRuntimePlan,PolymorphError> MakePolymorphRuntimeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const RecoverySchedule& before,ItemInstanceId id,PolymorphRequest actor,std::uint64_t elapsed,std::uint64_t now,std::uint16_t roll)
{
    using E=PolymorphError;
    const auto source=std::ranges::find(items,id,&PersistentItem::id);
    if(source==items.end() || fence.owner.kind!=ItemOwnerKind::Character || actor.actorId!=fence.owner.id || !PlainInventoryItem(*source,fence.owner) || !ValidatePersistentItem(c,*source) || !source->polymorph)return std::unexpected(E::InvalidSnapshot);
    auto state=AdvanceRecoverySchedule(c,before,elapsed);if(!state || before.revision==UINT64_MAX || before.generation==UINT64_MAX)return std::unexpected(E::InvalidState);
    NormalizeTimedItems(c,*state,now);actor=Request(*source,*state,actor,now);actor.operationId=op;actor.recordedRoll=roll;
    auto plan=PlanPolymorph(c,actor);if(!plan)return std::unexpected(plan.error());
    ItemCommit commit;
    if(plan->consumeUnits){auto consumed=MakeConsumeCommit(c,op,fence,layouts,items,id,plan->consumeUnits);if(!consumed)return std::unexpected(E::InvalidState);commit=std::move(*consumed);}
    else
    {
        commit.operationId=op;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();commit.kind=ItemOperationKind::Service;commit.owners={fence};
        const auto layout=std::ranges::find(layouts,ItemStorage{fence.owner,source->placement->kind,source->placement->storageId},&ItemStorageLayout::storage);
        if(layout==layouts.end())return std::unexpected(E::InvalidSnapshot);commit.storages={*layout};auto after=*source;++after.revision;after.polymorph=plan->after;commit.items={{*source,after}};
    }
    if(plan->effect){state->polymorph=ActivePolymorph{source->key,*plan->effect};++state->generation;}
    ++state->revision;commit.recovery=ItemRecoveryChange{fence.owner,before,*state,elapsed};
    const unsigned outcome=actor.action==PolymorphAction::UpgradeBook ? 3 : plan->success ? 2 : 1;
    commit.outcomeCode=0x2400+outcome;commit.result={'P','L','R',1,static_cast<std::uint8_t>(outcome),static_cast<std::uint8_t>(roll),static_cast<std::uint8_t>(roll>>8)};
    if(!ValidateItemCommit(c,commit))return std::unexpected(E::InvalidState);return PolymorphRuntimePlan{std::move(commit),*plan};
}
std::expected<ItemCommit,PolymorphError> MakePolymorphCancel(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,const RecoverySchedule& before,std::uint64_t elapsed,std::uint64_t now,std::uint64_t generation)
{
    using E=PolymorphError;if(generation!=before.generation || before.generation==UINT64_MAX || before.revision==UINT64_MAX)return std::unexpected(E::InvalidSnapshot);
    auto state=AdvanceRecoverySchedule(c,before,elapsed);if(!state)return std::unexpected(E::InvalidState);NormalizeTimedItems(c,*state,now);
    if(!state->polymorph)return std::unexpected(E::NotReady);state->polymorph.reset();++state->revision;++state->generation;
    ItemCommit commit;commit.operationId=op;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();commit.kind=ItemOperationKind::Service;commit.owners={fence};commit.outcomeCode=0x2404;commit.result={'P','L','R',1,4};commit.recovery=ItemRecoveryChange{fence.owner,before,*state,elapsed};
    if(!ValidateItemCommit(c,commit))return std::unexpected(E::InvalidState);return commit;
}
}
