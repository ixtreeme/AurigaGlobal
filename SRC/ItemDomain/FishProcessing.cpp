#include <item/FishProcessing.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
namespace
{
using E=FishProcessingError;
constexpr std::size_t MaxUncertainFishRewards=8;
std::expected<FishProcessingPlan,E> Build(const ItemCatalog& c,std::array<std::uint8_t,16> op,
    ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,
    FishProcessingRequest request,std::uint32_t consume,std::span<const FishingYield> yields,std::span<const ItemInstanceId> ids)
{
    std::vector<PersistentItem> working(items.begin(),items.end());
    auto source=std::ranges::find(working,request.source,&PersistentItem::id);
    if(source==working.end() || !consume || consume>source->count)return std::unexpected(E::Invalid);
    source->count-=consume;if(!source->count){source->destroyed=true;source->placement.reset();}
    std::size_t used=0;
    for(const auto& reward:yields)
    {
        auto grant=MakeGrantCommit(c,op,fence,layouts,working,{reward.item,0,reward.count},ids.subspan(used));
        if(!grant)return std::unexpected(grant.error()==ItemPersistenceError::InvalidPlacement ? E::NoRoom :
            grant.error()==ItemPersistenceError::LimitExceeded ? E::Unavailable : E::Unsupported);
        for(const auto& change:grant->items)
        {
            if(change.before)*std::ranges::find(working,change.after.id,&PersistentItem::id)=change.after;
            else{working.push_back(change.after);++used;}
        }
    }
    FishProcessingPlan plan;plan.yields.assign(yields.begin(),yields.end());auto& out=plan.commit;
    out.operationId=op;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;
    out.owners={fence};out.outcomeCode=request.action==FishAction::Use ? 0x2501 : 0x2502;out.result={'F','S','H',1};
    const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.result.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    put(static_cast<unsigned>(request.action),1);put(request.source.Value(),8);put(consume,4);put(yields.size(),2);
    for(const auto& reward:yields){put(static_cast<unsigned>(reward.item.Category()),2);put(reward.item.LocalId(),4);put(reward.count,4);}
    for(std::size_t i=0;i<items.size();++i)if(working[i]!=items[i])
    {if(items[i].revision==UINT64_MAX)return std::unexpected(E::Invalid);working[i].revision=items[i].revision+1;out.items.push_back({items[i],working[i]});}
    for(std::size_t i=items.size();i<working.size();++i){working[i].revision=1;out.items.push_back({{},working[i]});}
    if(out.items.size()>MaxItemCommitItems)return std::unexpected(E::Unsupported);
    for(const auto& change:out.items)for(const auto* item:{change.before ? &*change.before : nullptr,&change.after})if(item && item->placement)
    {
        const ItemStorage storage{item->owner,item->placement->kind,item->placement->storageId};
        if(std::ranges::find(out.storages,storage,&ItemStorageLayout::storage)!=out.storages.end())continue;
        const auto layout=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);
        if(layout==layouts.end())return std::unexpected(E::Invalid);out.storages.push_back(*layout);
    }
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);return plan;
}
}
std::expected<FishProcessingQuote,E> CheckFishProcessing(const ItemCatalog& c,ItemOwner owner,
    std::span<const PersistentItem> items,FishProcessingRequest request)
{
    const auto source=std::ranges::find(items,request.source,&PersistentItem::id);
    if(owner.kind!=ItemOwnerKind::Character || source==items.end() || !PlainInventoryItem(*source,owner) ||
       !ValidatePersistentItem(c,*source) || request.actor.characterClass>3 || request.actor.sex>1)return std::unexpected(E::Invalid);
    const auto* entry=c.Find(source->key);const auto* row=std::get_if<ItemFishingRow>(&entry->category);
    if(!row)return std::unexpected(E::WrongAction);
    const auto* use=c.FindUsePolicy(entry->base.usePolicy);
    if(!request.actor.admitted || entry->base.retired || !use || !(use->allowedClasses&(1u<<request.actor.characterClass)) ||
       !(use->allowedSexes&(1u<<request.actor.sex)) || (request.actor.inCombat && !use->useDuringCombat))return std::unexpected(E::Forbidden);
    if(request.actor.level<entry->base.minimumLevel)return std::unexpected(E::Level);
    FishProcessingQuote quote;quote.kind=row->kind;
    if(request.action==FishAction::Use && row->kind==FishingKind::LiveFish && row->useLootProfile)
    {
        const auto* loot=c.FindLoot(*row->useLootProfile);if(!loot)return std::unexpected(E::Unsupported);
        quote.consume=1;quote.mode=loot->Data().mode;quote.possibilities=loot->Data().entries;
    }
    else if(request.action==FishAction::Grill && row->kind==FishingKind::DeadFish && row->grillResult)
    {
        if(!request.actor.campfireAvailable)return std::unexpected(E::CampfireRequired);
        quote.consume=source->count;quote.possibilities={{1,LootRewardKind::Item,row->grillResult,source->count,10000,0}};
    }
    else return std::unexpected(E::WrongAction);
    std::size_t uncertain=0;
    for(const auto& reward:quote.possibilities)
    {
        const auto* target=reward.item ? c.Find(*reward.item) : nullptr;
        if(reward.kind!=LootRewardKind::Item || reward.rareAttributeChanceBp || !reward.amount || reward.amount>UINT32_MAX ||
           !target || target->base.retired || !SupportsPersistentItem(c,*target))return std::unexpected(E::Unsupported);
        if(reward.weight<10000)++uncertain;
    }
    if(quote.mode==LootPickMode::IndependentChance && (uncertain>MaxUncertainFishRewards || quote.possibilities.size()+1>MaxItemCommitItems))return std::unexpected(E::Unsupported);
    return quote;
}
std::expected<FishProcessingPlan,E> MakeFishProcessingCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,
    ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,
    FishProcessingRequest request,std::span<const ItemInstanceId> ids,const LootDraw& draw)
{
    if(items.size()>MaxLoadedOwnerItems)return std::unexpected(E::Invalid);
    std::set<ItemInstanceId> unique;for(const auto& item:items)if(item.owner!=fence.owner || !unique.insert(item.id).second || !ValidatePersistentItem(c,item))return std::unexpected(E::Invalid);
    const auto quote=CheckFishProcessing(c,fence.owner,items,request);if(!quote)return std::unexpected(quote.error());
    const auto candidate=[&](std::span<const LootEntry> entries)->std::expected<void,E>{
        std::vector<FishingYield> yields;for(const auto& reward:entries)yields.push_back({*reward.item,static_cast<std::uint32_t>(reward.amount)});
        const auto built=Build(c,op,fence,layouts,items,request,quote->consume,yields,ids);
        if(!built)return std::unexpected(built.error());return {};
    };
    // Do not let a full bag or missing ID reservation select a favourable roll.
    if(quote->mode==LootPickMode::WeightedOne)
    {for(const auto& reward:quote->possibilities)if(auto allowed=candidate({&reward,1});!allowed)return std::unexpected(allowed.error());}
    else if(quote->mode==LootPickMode::IndependentChance)
    {
        std::size_t uncertain=0;for(const auto& reward:quote->possibilities)if(reward.weight<10000)++uncertain;
        for(std::size_t mask=0;mask<(std::size_t{1}<<uncertain);++mask)
        {
            std::vector<LootEntry> possible;std::size_t bit=0;
            for(const auto& reward:quote->possibilities)if(reward.weight==10000 || (mask&(std::size_t{1}<<bit++)))possible.push_back(reward);
            if(auto allowed=candidate(possible);!allowed)return std::unexpected(allowed.error());
        }
    }
    else if(auto allowed=candidate(quote->possibilities);!allowed)return std::unexpected(allowed.error());
    const auto& source=*std::ranges::find(items,request.source,&PersistentItem::id);
    GatheringContext context;context.actorId=fence.owner.id;context.rulesetRevision=1;context.operationId=op;
    context.characterLevel=request.actor.level;context.admittedByServer=request.actor.admitted;context.inCombat=request.actor.inCombat;
    const FishProcessRequest domain{context,{source.key,source.id.Value(),source.revision,source.owner.id,source.refinementLevel,source.count,false,false,false},request.action,request.actor.campfireAvailable};
    const auto plan=PlanFishProcess(c,domain,draw);if(!plan)return std::unexpected(plan.error()==GatheringError::InvalidDecision ? E::InvalidDecision : E::Invalid);
    return Build(c,op,fence,layouts,items,request,plan->consumeUnits,plan->yields,ids);
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeFishProcessing(const ItemCatalog& c,
    FishProcessingStatus status,std::uint8_t error,std::uint64_t session,const PersistentItem* source,FishAction action,
    std::uint32_t campfire,const FishProcessingQuote& quote,std::span<const FishingYield> yields)
{
    using W=TypedInventoryWireError;
    if(static_cast<unsigned>(status)>8 || (error!=255 && error>8) || quote.possibilities.size()>MaxLootEntries || yields.size()>MaxLootEntries ||
       (source ? !session || source->key.Category()!=ItemCategory::Fishing || (action!=FishAction::Use && action!=FishAction::Grill) :
        session || static_cast<unsigned>(action) || campfire || !quote.possibilities.empty() || !yields.empty() || status==FishProcessingStatus::Ready || status==FishProcessingStatus::Pending || status==FishProcessingStatus::Applied))return std::unexpected(W::BadValue);
    std::vector<ItemGrant> extra;for(const auto& entry:quote.possibilities)if(entry.item)extra.push_back({*entry.item,0,1});
    for(const auto& reward:yields)extra.push_back({reward.item,0,1});
    const auto details=EncodeTypedItemDetails(c,source ? std::span(source,1) : std::span<const PersistentItem>{},false,extra);if(!details)return std::unexpected(details.error());
    std::vector<std::byte> out;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(std::byte((v>>(8*i))&255));};
    const auto key=[&](ItemKey k){put(static_cast<unsigned>(k.Category()),2);put(k.LocalId(),4);};
    put(254,1);put(0,4);put(1,1);put(static_cast<unsigned>(status),1);put(error,1);put(session,8);put(source ? source->id.Value() : 0,8);put(source ? source->revision : 0,8);
    put(source ? 12 : 0,2);put(source ? source->key.LocalId() : 0,4);put(source ? source->count : 0,4);put(static_cast<unsigned>(action),1);put(static_cast<unsigned>(quote.kind),1);put(quote.consume,4);put(campfire,4);put(quote.mode ? static_cast<unsigned>(*quote.mode) : 0,1);
    put(quote.possibilities.size(),2);for(const auto& r:quote.possibilities){if(!r.item || r.amount>UINT32_MAX)return std::unexpected(W::BadValue);key(*r.item);put(r.amount,4);put(r.weight,4);}
    put(yields.size(),2);for(const auto& r:yields){key(r.item);put(r.count,4);}put(details->size(),4);out.insert(out.end(),details->begin(),details->end());
    if(out.size()>1u<<20)return std::unexpected(W::TooLarge);for(unsigned i=0;i<4;++i)out[i+1]=std::byte((out.size()>>(8*i))&255);return out;
}
}
