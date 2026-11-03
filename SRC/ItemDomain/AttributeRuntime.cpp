#include <item/AttributeRuntime.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
namespace
{
using S=AttributeRuntimeStatus;
S Refusal(AttributeError e)
{
    switch(e){case AttributeError::AdmissionDenied:return S::Denied;case AttributeError::IncompatibleTarget:case AttributeError::TargetDenied:return S::Incompatible;
    case AttributeError::NoEffect:return S::NoEffect;case AttributeError::NotEnoughAttributes:return S::NotEnough;case AttributeError::AlreadyLocked:return S::AlreadyLocked;
    case AttributeError::NotLocked:return S::NotLocked;case AttributeError::NoAlternative:return S::NoAlternative;case AttributeError::NoCandidate:return S::NoCandidate;
    case AttributeError::UnassignedTier:return S::UnassignedTier;case AttributeError::InsufficientUnits:return S::Insufficient;default:return S::Invalid;}
}
const AttributeServiceProfile* Service(const ItemCatalog& c,ItemKey key)
{const auto* e=c.Find(key);const auto* row=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;return row && row->attributeServiceProfile ? c.FindAttributeService(*row->attributeServiceProfile) : nullptr;}
SocketItemPair Pair(const ItemCatalog& c,ItemOwnerFence fence,const PersistentItem& source,const PersistentItem& target,SocketMaintenanceActor actor,std::array<std::uint8_t,16> op={18})
{
    const auto snapshot=[](const PersistentItem& i){return StoneItemSnapshot{i.key,i.id.Value(),i.revision,i.owner.id,i.refinementLevel,i.count};};
    return {{op,fence.owner.id,fence.sessionGeneration,c.Revision(),fence.revision,actor.level,actor.characterClass,actor.sex,true,actor.inCombat,false},snapshot(source),snapshot(target),true,true};
}
}
bool AttributeRuntimeTarget(const ItemCatalog& c,const PersistentItem& i,ItemOwner owner)
{
    return i.owner==owner && !i.destroyed && i.count==1 && i.placement && i.placement->kind==ItemStorageKind::Inventory &&
        (!i.boundTo || i.boundTo==owner) && c.FindItemAttributes(i.key) && ValidatePersistentItem(c,i).has_value();
}
std::expected<AttributeRuntimeQuote,S> PrepareAttributeRuntime(const ItemCatalog& c,ItemOwnerFence fence,std::span<const PersistentItem> items,
    ItemInstanceId sourceId,ItemInstanceId targetId,SocketMaintenanceActor actor)
{
    if(fence.owner.kind!=ItemOwnerKind::Character || items.size()>MaxLoadedOwnerItems)return std::unexpected(S::Invalid);
    std::set<ItemInstanceId> ids;for(const auto& i:items)if(i.owner!=fence.owner || !ids.insert(i.id).second || !ValidatePersistentItem(c,i))return std::unexpected(S::Invalid);
    const auto source=std::ranges::find(items,sourceId,&PersistentItem::id),target=std::ranges::find(items,targetId,&PersistentItem::id);
    if(source==items.end() || target==items.end() || sourceId==targetId || !PlainInventoryItem(*source,fence.owner) || !AttributeRuntimeTarget(c,*target,fence.owner))return std::unexpected(S::Invalid);
    const auto* service=Service(c,source->key);if(!service)return std::unexpected(S::Invalid);
    if(actor.level<c.Find(source->key)->base.minimumLevel)return std::unexpected(S::Level);
    const auto* p=c.FindItemAttributes(target->key);const auto group=std::ranges::find(p->groups,service->pool,&AttributeGroup::pool);if(group==p->groups.end())return std::unexpected(S::Incompatible);
    AttributeRuntimeQuote q;q.operation=static_cast<std::uint8_t>(service->operation);q.pool=static_cast<std::uint8_t>(service->pool);q.units=service->units;q.chance=service->successBp;q.available=source->count;q.consumeFailure=service->consumeOnFailure;q.before=target->attributes;q.capacity=group->capacity;
    const unsigned begin=service->pool==AttributePool::Normal ? 0 : service->pool==AttributePool::Rare ? 5 : 7;
    auto working=q.before;unsigned count=0;for(unsigned i=begin;i<begin+group->capacity;++i)count+=working.slots[i].has_value();
    const bool rolling=service->operation==AttributeOperation::Add || service->operation==AttributeOperation::Reroll;
    if(rolling)
    {
        if(service->operation==AttributeOperation::Add)q.draws=service->addCount;
        else for(unsigned i=begin;i<begin+group->capacity;++i)if(working.slots[i] && working.lockedSlot!=i){++q.draws;working.slots[i].reset();}
        std::set<BonusId> excluded;
        for(unsigned i=begin<5 ? 0 : 5;i<(begin<5 ? 5u : 8u);++i)if(working.slots[i])excluded.insert(working.slots[i]->bonus);
        if(group->excludeStatic)
        {
            const auto profiles=c.BonusProfilesAt(target->key,target->refinementLevel);if(!profiles)return std::unexpected(S::Invalid);
            for(const auto* profile:{profiles->base,profiles->refinement})if(profile)for(const auto& entry:profile->Data().entries)excluded.insert(entry.bonus);
        }
        for(const auto& candidate:p->candidates)if(candidate.pool==service->pool && !excluded.contains(candidate.bonus))
        {AttributeRuntimeChoice choice{candidate.bonus,candidate.weight,{}};for(const auto& value:candidate.values)choice.values.push_back(value.value);q.choices.push_back(std::move(choice));}
        for(const auto& tier:service->tiers)q.tiers.push_back({tier.tier,tier.chanceBp});
    }
    else if(service->operation!=AttributeOperation::Unlock)
        for(std::uint8_t i=0;i<group->capacity;++i)if(working.slots[i] && working.lockedSlot!=i)q.locks.push_back(i);
    // A complete deterministic successful proposal validates every requirement
    // before any entropy is read. Repeated bonusRoll=1 selects without replacement.
    AttributeRequest probe{Pair(c,fence,*source,*target,actor),q.before,service->successBp==10000 ? std::uint16_t{0} : std::uint16_t{1},static_cast<std::uint8_t>(q.locks.empty() ? 0 : 1),{}};
    probe.draws.assign(q.draws,{1,1});const auto valid=PlanAttributeService(c,probe);if(!valid)return std::unexpected(Refusal(valid.error()));
    if(rolling && q.choices.size()<q.draws)return std::unexpected(S::NoCandidate);
    return q;
}
std::expected<AttributeRuntimePlan,S> MakeAttributeRuntimeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ItemInstanceId sourceId,ItemInstanceId targetId,
    SocketMaintenanceActor actor,const AttributeRuntimeQuote& expected,const AttributeRandom& random)
{
    const auto quote=PrepareAttributeRuntime(c,fence,items,sourceId,targetId,actor);if(!quote)return std::unexpected(quote.error());if(*quote!=expected)return std::unexpected(S::Stale);
    const auto& source=*std::ranges::find(items,sourceId,&PersistentItem::id);const auto& target=*std::ranges::find(items,targetId,&PersistentItem::id);
    // Validate the exact operation and storage before randomness, including the
    // eventual writer's structural bound. Layout admission cannot reject only
    // one randomly chosen outcome.
    std::set<ItemStorage> storages;for(const auto& l:layouts)if(l.storage.owner!=fence.owner || !ValidItemStorage(l) || !storages.insert(l.storage).second)return std::unexpected(S::Invalid);
    if(std::ranges::all_of(op,[](auto b){return b==0;}))return std::unexpected(S::Invalid);
    AttributeRequest probe{Pair(c,fence,source,target,actor,op),target.attributes,quote->chance==10000 ? std::uint16_t{0} : std::uint16_t{1},static_cast<std::uint8_t>(quote->locks.empty() ? 0 : 1),{}};
    probe.draws.assign(quote->draws,{1,1});const auto preflight=PlanAttributeService(c,probe);
    if(!preflight || !MakeAttributeCommit(c,*preflight,fence,layouts,items))return std::unexpected(S::Invalid);
    const auto draw=[&](std::uint32_t max)->std::optional<std::uint32_t>{if(!max || !random)return {};auto v=random(max);return v && *v>=1 && *v<=max ? v : std::nullopt;};
    AttributeRequest request{Pair(c,fence,source,target,actor,op),target.attributes};
    if(quote->chance!=10000){const auto roll=draw(10000);if(!roll)return std::unexpected(S::Failed);request.successRoll=static_cast<std::uint16_t>(*roll);}
    const bool succeeded=quote->chance==10000 || request.successRoll<=quote->chance;
    if(succeeded)
    {
        if(!quote->locks.empty()){const auto roll=draw(static_cast<std::uint32_t>(quote->locks.size()));if(!roll)return std::unexpected(S::Failed);request.lockRoll=static_cast<std::uint8_t>(*roll);}
        auto choices=quote->choices;
        for(unsigned i=0;i<quote->draws;++i)
        {
            const auto tier=draw(10000);if(!tier)return std::unexpected(S::Failed);
            unsigned covered=0;for(const auto& t:quote->tiers)covered+=t.chance;
            if(*tier>covered)return std::unexpected(S::UnassignedTier); // explicit no-result range; never reroll
            std::uint32_t total=0;for(const auto& candidate:choices)total+=candidate.weight;const auto type=draw(total);if(!type)return std::unexpected(S::Failed);
            request.draws.push_back({static_cast<std::uint16_t>(*tier),*type});auto remaining=*type;
            for(auto candidate=choices.begin();candidate!=choices.end();++candidate)if(remaining<=candidate->weight){choices.erase(candidate);break;}else remaining-=candidate->weight;
        }
    }
    const auto plan=PlanAttributeService(c,request);if(!plan)return std::unexpected(Refusal(plan.error()));
    auto commit=MakeAttributeCommit(c,*plan,fence,layouts,items);if(!commit)return std::unexpected(S::Invalid);
    return AttributeRuntimePlan{std::move(*commit),plan->after,plan->succeeded};
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeAttributeRuntime(const ItemCatalog& c,std::uint64_t nonce,
    const PersistentItem* source,const PersistentItem* target,std::uint64_t revision,S status,const AttributeRuntimeQuote& q,
    const InstanceAttributes* after,std::span<const std::uint64_t> targets)
{
    using E=TypedInventoryWireError;
    if(static_cast<unsigned>(status)>20 || targets.size()>MaxLoadedOwnerItems || q.choices.size()>MaxAttributeCandidates || q.tiers.size()>MaxAttributeTiers || q.locks.size()>5 ||
       (source ? (!nonce || !revision) : (nonce || target || revision || q!=AttributeRuntimeQuote{} || after || !targets.empty())))return std::unexpected(E::BadValue);
    std::vector<PersistentItem> metadata;if(source)metadata.push_back(*source);if(target)metadata.push_back(*target);
    const auto details=source ? EncodeTypedItemDetails(c,metadata,false) : std::expected<std::vector<std::byte>,E>(std::vector<std::byte>{});if(!details)return std::unexpected(details.error());
    std::vector<std::byte> out;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(std::byte((v>>(i*8))&255));};
    const auto key=[&](const PersistentItem* i){put(i ? static_cast<unsigned>(i->key.Category()) : 0,2);put(i ? i->key.LocalId() : 0,4);put(i ? i->refinementLevel : 0,2);};
    const auto attributes=[&](const InstanceAttributes& a){put(a.lockedSlot.value_or(255),1);for(const auto& entry:a.slots){put(entry ? static_cast<unsigned>(entry->bonus) : 0,4);put(entry ? static_cast<std::uint64_t>(entry->valueScaled) : 0,8);}};
    put(236,1);put(0,4);put(1,1);put(static_cast<unsigned>(status),1);put(nonce,8);put(source ? source->id.Value() : 0,8);put(target ? target->id.Value() : 0,8);put(revision,8);key(source);key(target);
    put(q.operation,1);put(q.pool,1);put(q.units,2);put(q.available,4);put(q.chance,2);put(q.consumeFailure,1);put(q.capacity,1);put(q.draws,1);attributes(q.before);attributes(after ? *after : q.before);
    put(q.tiers.size(),1);for(const auto& tier:q.tiers){put(tier.tier,1);put(tier.chance,2);}
    put(q.choices.size(),1);for(const auto& choice:q.choices){put(static_cast<unsigned>(choice.bonus),4);put(choice.weight,4);put(choice.values.size(),1);for(auto value:choice.values)put(static_cast<std::uint64_t>(value),8);}
    put(q.locks.size(),1);for(auto slot:q.locks)put(slot,1);put(targets.size(),2);for(auto id:targets)put(id,8);put(details->size(),4);out.insert(out.end(),details->begin(),details->end());
    if(out.size()>(1u<<20))return std::unexpected(E::TooLarge);for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(8*i))&255);return out;
}
}
