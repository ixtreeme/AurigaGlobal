#include <item/SocketServices.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace auriga::item
{
bool ValidSocketServiceProfile(const SocketServiceProfile& p) noexcept
{
    return p.id && detail::ContentCode(p.code) && p.unitsPerUse &&
        p.kind>=SocketServiceKind::CleanBroken && p.kind<=SocketServiceKind::ExtractLast;
}
using E=SocketServiceError;
std::expected<void,E> ValidateItemOperationPair(const ItemCatalog& catalog,const SocketItemPair& r)
{
    const auto& c=r.context;
    if(!c.admittedByServer || c.observer)return std::unexpected(E::Forbidden);
    if(!c.actorId || !c.sessionGeneration || !c.rulesetRevision || !c.inventoryRevision ||
       c.inventoryRevision==UINT64_MAX || std::ranges::all_of(c.operationId,[](auto b){return b==0;}))
        return std::unexpected(E::InvalidOperation);
    if(!r.sourceInInventory || !r.targetInInventory || r.source.instanceId==r.target.instanceId)
        return std::unexpected(E::InvalidSnapshot);
    for(const auto* snapshot:{&r.source,&r.target})
    {
        if(!snapshot->instanceId || !snapshot->revision || snapshot->revision==UINT64_MAX ||
           snapshot->ownerId!=c.actorId || snapshot->locked || snapshot->equipped || snapshot->exchanging)
            return std::unexpected(E::InvalidSnapshot);
        const auto* entry=catalog.Find(snapshot->item);
        if(!entry)return std::unexpected(E::MissingItem);
        if(entry->base.retired)return std::unexpected(E::RetiredItem);
        if(!snapshot->count || snapshot->count>entry->base.maxStack || (snapshot==&r.target && snapshot->count!=1))
            return std::unexpected(E::InvalidStack);
        if(!catalog.FindRefinement(entry->base.refinementProfile)->FindLevel(snapshot->refinementLevel))
            return std::unexpected(E::InvalidLevel);
    }
    const auto& source=catalog.Find(r.source.item)->base;
    const auto* policy=catalog.FindUsePolicy(source.usePolicy);
    if(c.characterLevel<source.minimumLevel)return std::unexpected(E::InvalidLevel);
    if(c.characterClass>=4 || c.sex>=2 || !(policy->allowedClasses&(1u<<c.characterClass)) ||
       !(policy->allowedSexes&(1u<<c.sex)) || (c.inCombat && !policy->useDuringCombat))
        return std::unexpected(E::Forbidden);
    return {};
}
namespace
{
E AccessoryError(AccessorySocketError e)
{
    switch(e)
    {
    case AccessorySocketError::MaterialNotAllowed:return E::MaterialNotAllowed;
    case AccessorySocketError::CapacityFull:case AccessorySocketError::NoOpenCapacity:return E::NoEffect;
    case AccessorySocketError::InvalidDecision:return E::InvalidDecision;
    case AccessorySocketError::OperationDisabled:return E::OperationDisabled;
    case AccessorySocketError::LifetimeMismatch:return E::LifetimeMismatch;
    default:return E::InvalidState;
    }
}
}
std::expected<SocketServicePlan,SocketServiceError> PlanSocketService(const ItemCatalog& catalog,const SocketServiceRequest& r)
{
    if(auto v=ValidateItemOperationPair(catalog,r.items);!v)return std::unexpected(v.error());
    const auto* source=std::get_if<ItemConsumableRow>(&catalog.Find(r.items.source.item)->category);
    const auto* profile=source && source->socketServiceProfile ? catalog.FindSocketService(*source->socketServiceProfile) : nullptr;
    if(!profile)return std::unexpected(E::WrongSource);
    if(r.items.source.count<profile->unitsPerUse)return std::unexpected(E::InvalidStack);
    auto valid=ValidateEquipmentStoneSockets(catalog,r.items.target.item,r.sockets);
    if(!valid)return std::unexpected(valid.error()==SocketStoneError::IncompatibleTarget ? E::IncompatibleTarget : E::InvalidState);
    std::vector<std::uint8_t> selected;
    for(std::size_t i=0;i<r.sockets.size();++i)
        if(r.sockets[i].state==(profile->kind==SocketServiceKind::CleanBroken ? StoneSocketState::Broken : StoneSocketState::Occupied))
            selected.push_back(static_cast<std::uint8_t>(i));
    if(selected.empty())return std::unexpected(E::NoEffect);
    if(profile->kind==SocketServiceKind::ExtractLast)selected={selected.back()};
    const bool extracting=profile->kind!=SocketServiceKind::CleanBroken;
    if(r.returns.size()!=(extracting ? selected.size() : 0))return std::unexpected(E::InvalidReservation);
    SocketServicePlan plan{r,catalog.Revision(),catalog.ContentHash(),r.sockets,{},profile->unitsPerUse};
    std::set<std::uint64_t> instances{r.items.source.instanceId,r.items.target.instanceId},placements;
    for(const auto slot:selected)
    {
        if(extracting)
        {
            auto found=std::ranges::find(r.returns,slot,&StoneReturnReservation::socket);
            if(found==r.returns.end() || !found->instanceId || !found->placementToken ||
               !instances.insert(found->instanceId).second || !placements.insert(found->placementToken).second)
                return std::unexpected(E::InvalidReservation);
            plan.returns.push_back({slot,*r.sockets[slot].stone,r.sockets[slot].refinementLevel,found->instanceId,found->placementToken});
        }
        plan.after[slot]={profile->kind==SocketServiceKind::ExtractAll ? StoneSocketState::Broken : StoneSocketState::Empty,r.sockets[slot].grade};
    }
    return plan;
}
std::expected<AccessoryServicePlan,SocketServiceError> PlanAccessoryService(const ItemCatalog& catalog,const AccessoryServiceRequest& r)
{
    if(auto v=ValidateItemOperationPair(catalog,r.items);!v)return std::unexpected(v.error());
    if(!std::holds_alternative<ItemMaterialRow>(catalog.Find(r.items.source.item)->category))return std::unexpected(E::WrongSource);
    const auto* target=std::get_if<ItemAccessoryRow>(&catalog.Find(r.items.target.item)->category);
    const auto* p=target && target->socketProfile ? catalog.FindAccessorySocket(*target->socketProfile) : nullptr;
    if(!p)return std::unexpected(E::IncompatibleTarget);
    if(auto v=ValidateAccessorySocketState(*p,r.state);!v)return std::unexpected(E::InvalidState);
    if(r.kind!=AccessoryServiceKind::Unlock && r.kind!=AccessoryServiceKind::Charge)return std::unexpected(E::InvalidOperation);
    const auto index=r.kind==AccessoryServiceKind::Unlock ? r.state.unlockedCapacity : r.state.activeGrade;
    const auto* grade=p->FindGrade(index);
    const auto chance=r.kind==AccessoryServiceKind::Unlock ? grade->unlockChanceBp : grade->chargeChanceBp;
    if((chance==0 || chance==10000) ? r.recordedRoll!=0 : (r.recordedRoll<1 || r.recordedRoll>10000))
        return std::unexpected(E::InvalidDecision);
    const auto decision=chance==10000 || (chance && r.recordedRoll<=chance) ? SocketAttemptOutcome::Succeeded : SocketAttemptOutcome::Failed;
    auto result=r.kind==AccessoryServiceKind::Unlock ? PlanAccessorySocketUnlock(*p,r.state,r.items.source.item,decision)
                                                    : PlanAccessorySocketCharge(*p,r.state,r.items.source.item,decision);
    if(!result)return std::unexpected(AccessoryError(result.error()));
    if(result->materialUnits>r.items.source.count)return std::unexpected(E::InvalidStack);
    return AccessoryServicePlan{r,catalog.Revision(),catalog.ContentHash(),result->state,result->materialUnits,result->succeeded};
}
} // namespace auriga::item
