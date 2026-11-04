#include <item/ToolRuntime.hpp>
#include <algorithm>
namespace auriga::item
{
namespace
{
using E=GatheringError;
const PersistentItem* Find(std::span<const PersistentItem> items,ItemInstanceId id)
{const auto it=std::ranges::find(items,id,&PersistentItem::id);return it==items.end() ? nullptr : &*it;}
bool Worn(const PersistentItem& i){return i.placement && *i.placement==ItemPlacement{ItemStorageKind::Equipment,1,0};}
GatheringItemSnapshot Snapshot(const PersistentItem& i)
{return {i.key,i.id.Value(),i.revision,i.owner.id,i.refinementLevel,i.count,Worn(i),false,false};}
GatheringContext Context(ItemOwner owner,ToolRuntimeActor a,std::array<std::uint8_t,16> op)
{GatheringContext c;c.actorId=owner.id;c.rulesetRevision=1;c.operationId=op;c.characterLevel=a.level;c.admittedByServer=a.admitted;c.inCombat=a.inCombat;return c;}
std::expected<void,E> Allowed(const ItemCatalog& c,const PersistentItem& i,ItemOwner owner,ToolRuntimeActor a)
{
    if(i.owner!=owner || i.destroyed || !i.placement || !ValidatePersistentItem(c,i) || (i.boundTo && *i.boundTo!=owner) ||
       (!PlainInventoryItem(i,owner) && !Worn(i)) || a.characterClass>3 || a.sex>1)return std::unexpected(E::InvalidSnapshot);
    const auto& base=c.Find(i.key)->base;const auto* use=c.FindUsePolicy(base.usePolicy);
    if(!a.admitted || base.retired || !use || a.level<base.minimumLevel || !(use->allowedClasses&(1u<<a.characterClass)) || !(use->allowedSexes&(1u<<a.sex)) || (a.inCombat && !use->useDuringCombat))return std::unexpected(E::Forbidden);
    return {};
}
}
std::expected<void,E> CheckToolRuntime(const ItemCatalog& c,ItemOwner owner,std::span<const PersistentItem> items,ToolRuntimeRequest r)
{
    if(owner.kind!=ItemOwnerKind::Character)return std::unexpected(E::InvalidSnapshot);
    const auto* source=Find(items,r.source);if(!source)return std::unexpected(E::MissingItem);
    if(auto allowed=Allowed(c,*source,owner,r.actor);!allowed)return allowed;
    if(r.action==ToolRuntimeAction::Bait)
    {
        const auto* target=r.target ? Find(items,*r.target) : nullptr;if(!target)return std::unexpected(E::NotReady);
        if(auto allowed=Allowed(c,*target,owner,r.actor);!allowed)return allowed;
        if(!target->tool)return std::unexpected(E::NotTool);
        const auto plan=PlanApplyBait(c,{Context(owner,r.actor,{1}),Snapshot(*source),Snapshot(*target),target->tool->baitPower,r.actor.fishingActive});
        if(!plan)return std::unexpected(plan.error());return {};
    }
    const auto* row=std::get_if<ItemToolRow>(&c.Find(source->key)->category);if(!row || !source->tool)return std::unexpected(E::NotTool);
    if(r.target)return std::unexpected(E::InvalidOperation);
    if(r.action==ToolRuntimeAction::Info)return {};
    if(r.actor.fishingActive)return std::unexpected(E::FishingActive);
    if(r.action==ToolRuntimeAction::Refine && !r.actor.serviceAvailable)return std::unexpected(E::ServiceRequired);
    if(r.action==ToolRuntimeAction::Practice && !r.actor.completedResourceAction)return std::unexpected(E::ResourceCompletionRequired);
    if(r.action!=ToolRuntimeAction::Refine && r.action!=ToolRuntimeAction::Practice)return std::unexpected(E::WrongAction);
    ToolActionRequest request{Context(owner,r.actor,{1}),Snapshot(*source),r.action==ToolRuntimeAction::Refine ? ToolAction::Refine : ToolAction::Practice,source->tool->trainingPoints,0};
    auto plan=PlanToolAction(c,request);if(!plan && plan.error()==E::InvalidDecision){request.recordedRoll=1;plan=PlanToolAction(c,request);}
    if(!plan)return std::unexpected(plan.error());return {};
}
std::expected<ToolRuntimePlan,E> MakeToolRuntimeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ToolRuntimeRequest r)
{
    if(auto checked=CheckToolRuntime(c,fence.owner,items,r);!checked)return std::unexpected(checked.error());
    if(r.action==ToolRuntimeAction::Info)return std::unexpected(E::WrongAction);
    const auto* source=Find(items,r.source);const auto* tool=r.action==ToolRuntimeAction::Bait ? Find(items,*r.target) : source;
    if(!tool || !tool->tool || tool->revision==UINT64_MAX)return std::unexpected(E::InvalidSnapshot);
    ToolRuntimePlan out;out.level=tool->refinementLevel;out.state=*tool->tool;auto& commit=out.commit;
    if(r.action==ToolRuntimeAction::Bait)
    {
        if(r.roll)return std::unexpected(E::InvalidDecision);
        const auto plan=PlanApplyBait(c,{Context(fence.owner,r.actor,op),Snapshot(*source),Snapshot(*tool),tool->tool->baitPower,r.actor.fishingActive});if(!plan)return std::unexpected(plan.error());
        auto consumed=MakeConsumeCommit(c,op,fence,layouts,items,source->id,plan->consumeUnits);if(!consumed)return std::unexpected(E::InvalidState);commit=std::move(*consumed);out.state.baitPower=plan->afterBaitPower;out.outcome=3;
    }
    else
    {
        const auto plan=PlanToolAction(c,{Context(fence.owner,r.actor,op),Snapshot(*tool),r.action==ToolRuntimeAction::Refine ? ToolAction::Refine : ToolAction::Practice,tool->tool->trainingPoints,r.roll});if(!plan)return std::unexpected(plan.error());
        out.level=plan->afterLevel;out.state.trainingPoints=plan->afterPoints;out.outcome=r.action==ToolRuntimeAction::Practice ? 4 : plan->success ? 1 : 2;
        commit.operationId=op;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();commit.kind=ItemOperationKind::Service;commit.owners={fence};
    }
    auto after=*tool;++after.revision;after.refinementLevel=out.level;after.tool=out.state;commit.items.push_back({*tool,after});
    const ItemStorage storage{tool->owner,tool->placement->kind,tool->placement->storageId};
    if(std::ranges::find(commit.storages,storage,&ItemStorageLayout::storage)==commit.storages.end())
    {const auto l=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);if(l==layouts.end())return std::unexpected(E::InvalidSnapshot);commit.storages.push_back(*l);}
    commit.kind=ItemOperationKind::Service;commit.outcomeCode=0x2510+out.outcome;commit.result={'T','L','R',1,out.outcome};
    const auto put=[&](std::uint64_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)commit.result.push_back(static_cast<std::uint8_t>(n>>(8*i)));};put(r.roll,4);put(out.level,2);put(out.state.trainingPoints,4);put(out.state.baitPower,4);
    if(!ValidateItemCommit(c,commit))return std::unexpected(E::InvalidState);return out;
}
}
