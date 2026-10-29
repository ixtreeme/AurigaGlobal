#include <item/SoulItems.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>
#include <limits>

namespace auriga::item
{
namespace
{
constexpr auto MaxTime=std::uint64_t{INT64_MAX};
bool ValidState(const SoulLevel& level,const SoulState& state)
{
    return state.charge<=level.chargeCapacity && state.strikes>0 && state.strikes<=level.strikesPerCycle &&
        state.rechargeRemainderMs<level.rechargeIntervalMs &&
        (state.charge!=level.chargeCapacity || !state.rechargeRemainderMs) &&
        (!state.active || state.charge>=level.cycleChargeCost);
}
SoulState Initial(const SoulLevel& level) { return {level.initialCharge,level.initialStrikes,0,false}; }
}
bool ValidSoulProfile(const SoulProfile& profile) noexcept
{
    if(profile.levels.empty() || profile.levels.size()>256)return false;
    for(std::size_t i=0;i<profile.levels.size();++i)
    {
        const auto& r=profile.levels[i];
        if(r.level!=i || !r.cycleChargeCost || r.chargeCapacity<r.cycleChargeCost ||
           std::uint64_t{r.chargeCapacity}>std::uint64_t{r.cycleChargeCost}*3 || r.chargeCapacity%r.cycleChargeCost ||
           !r.strikesPerCycle || !r.initialStrikes || r.initialStrikes>r.strikesPerCycle || r.initialCharge>r.chargeCapacity ||
           !r.rechargeIntervalMs || r.rechargeIntervalMs>MaxTime)return false;
        for(auto multiplier:{r.firstMultiplierBp,r.secondMultiplierBp,r.thirdMultiplierBp})
            if(multiplier<10000 || multiplier>1000000)return false;
    }
    return true;
}
std::expected<SoulState,SoulError> MakeSoulState(const ItemCatalog& catalog,ItemKey key,std::uint16_t level)
{
    const auto* item=catalog.Find(key);if(!item)return std::unexpected(SoulError::MissingItem);
    const auto* row=std::get_if<ItemSoulRow>(&item->category);
    if(!row || row->kind!=SoulKind::Core)return std::unexpected(SoulError::NotCore);
    if(item->base.retired)return std::unexpected(SoulError::RetiredItem);
    const auto* profile=catalog.FindSoulProfile(*row->profile);
    if(level>=profile->levels.size())return std::unexpected(SoulError::InvalidLevel);
    return Initial(profile->levels[level]);
}
std::expected<SoulPlan,SoulError> PlanSoul(const ItemCatalog& catalog,const SoulRequest& r)
{
    using E=SoulError;
    if(!r.admittedByServer)return std::unexpected(E::Forbidden);
    if(!r.actorId || !r.rulesetRevision || std::ranges::all_of(r.operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    if(!r.instanceId || !r.revision || r.revision==UINT64_MAX || r.ownerId!=r.actorId || r.count!=1 ||
       r.lockedByOther || r.exchanging || r.equipped)return std::unexpected(E::InvalidSnapshot);
    const auto* item=catalog.Find(r.key);if(!item)return std::unexpected(E::MissingItem);
    const auto* row=std::get_if<ItemSoulRow>(&item->category);
    if(!row || row->kind!=SoulKind::Core)return std::unexpected(E::NotCore);
    const auto* profile=catalog.FindSoulProfile(*row->profile);
    if(r.refinementLevel>=profile->levels.size())return std::unexpected(E::InvalidLevel);
    const auto& level=profile->levels[r.refinementLevel];
    if(!ValidState(level,r.state))return std::unexpected(E::InvalidState);
    if(r.action<SoulAction::Activate || r.action>SoulAction::Refine)return std::unexpected(E::InvalidAction);
    if((r.action!=SoulAction::Recharge && r.elapsedOwnerMs) || (r.action!=SoulAction::Strike && r.inputDamageRaw) ||
       (r.action!=SoulAction::Refine && (r.recordedRoll || r.scroll)))return std::unexpected(E::InvalidOperation);
    SoulPlan plan{r,r.state,r.refinementLevel,0,0,0,0,false,catalog.Revision(),catalog.ContentHash()};
    // Deactivation is cleanup; a retired prototype or changed policy must not trap a live effect.
    if(r.action==SoulAction::Deactivate){plan.after.active=false;plan.success=true;return plan;}
    if(item->base.retired)return std::unexpected(E::RetiredItem);
    if(r.action==SoulAction::Recharge)
    {
        if(r.elapsedOwnerMs>MaxTime)return std::unexpected(E::InvalidOperation);
        if(plan.after.charge<level.chargeCapacity)
        {
            const auto sum=r.state.rechargeRemainderMs+r.elapsedOwnerMs; // <=2*INT64_MAX, fits uint64.
            const auto ticks=sum/level.rechargeIntervalMs;
            const auto missing=level.chargeCapacity-r.state.charge;
            plan.after.charge+=static_cast<std::uint32_t>(std::min<std::uint64_t>(ticks,missing));
            plan.after.rechargeRemainderMs=plan.after.charge==level.chargeCapacity ? 0 : sum%level.rechargeIntervalMs;
        }
        plan.success=true;return plan;
    }
    const auto* policy=catalog.FindUsePolicy(item->base.usePolicy);
    if(r.characterLevel<1 || r.characterLevel>255 || r.characterLevel<item->base.minimumLevel ||
       r.characterClass<1 || r.characterClass>4 || r.sex<1 || r.sex>2 ||
       !(policy->allowedClasses&(1u<<(r.characterClass-1))) || !(policy->allowedSexes&(1u<<(r.sex-1))) ||
       (r.inCombat && !policy->useDuringCombat))return std::unexpected(E::Forbidden);
    if(r.action==SoulAction::Activate)
    {
        if(r.state.active)return std::unexpected(E::AlreadyActive);
        if(r.channelOccupiedByOther)return std::unexpected(E::ChannelOccupied);
        if(r.state.charge<level.cycleChargeCost)return std::unexpected(E::NotReady);
        plan.after.active=true;plan.success=true;return plan;
    }
    if(r.action==SoulAction::Refine)
    {
        if(r.state.active)return std::unexpected(E::AlreadyActive);
        if(std::size_t(r.refinementLevel)+1==profile->levels.size())return std::unexpected(E::AtMaximum);
        if(!r.scroll)return std::unexpected(E::MissingScroll);
        const auto& scroll=*r.scroll;const auto* target=catalog.Find(scroll.key);
        const auto* definition=target ? std::get_if<ItemSoulRow>(&target->category) : nullptr;
        if(!definition || definition->kind!=SoulKind::RefinementScroll || target->base.retired ||
           !scroll.instanceId || scroll.instanceId==r.instanceId || !scroll.revision || scroll.revision==UINT64_MAX ||
           scroll.ownerId!=r.actorId || scroll.locked || !scroll.count || scroll.count>target->base.maxStack)return std::unexpected(E::InvalidScroll);
        const auto* scrollPolicy=catalog.FindUsePolicy(target->base.usePolicy);
        if(r.characterLevel<target->base.minimumLevel || !(scrollPolicy->allowedClasses&(1u<<(r.characterClass-1))) ||
           !(scrollPolicy->allowedSexes&(1u<<(r.sex-1))) || (r.inCombat && !scrollPolicy->useDuringCombat))return std::unexpected(E::Forbidden);
        const auto chance=definition->refineChanceBp;
        if(chance==0 || chance==10000){if(r.recordedRoll)return std::unexpected(E::InvalidDecision);plan.success=chance==10000;}
        else {if(!r.recordedRoll || r.recordedRoll>10000)return std::unexpected(E::InvalidDecision);plan.success=r.recordedRoll<=chance;}
        plan.consumeScrollUnits=1;
        if(plan.success){++plan.afterLevel;plan.after=Initial(profile->levels[plan.afterLevel]);}
        return plan;
    }
    if(!r.state.active)return std::unexpected(E::NotActive);
    if(!r.damageEventId || !r.targetId || !r.targetRevision || r.targetRevision==UINT64_MAX)return std::unexpected(E::InvalidOperation);
    if(!r.actorIsPlayer || !r.targetIsNpc || r.polymorphed || r.channelOccupiedByOther || r.damageChannel!=*row->channel)return std::unexpected(E::IneligibleTarget);
    if(r.inputDamageRaw<=0)return std::unexpected(E::InvalidDamage);
    const auto tier=std::min<std::uint32_t>(3,r.state.charge/level.cycleChargeCost);
    plan.multiplierBp=tier==1 ? level.firstMultiplierBp : tier==2 ? level.secondMultiplierBp : level.thirdMultiplierBp;
    const auto damage=static_cast<std::uint64_t>(r.inputDamageRaw),q=damage/10000,rem=damage%10000;
    if(q>MaxTime/plan.multiplierBp)return std::unexpected(E::DamageOverflow);
    const auto whole=q*plan.multiplierBp,fraction=rem*plan.multiplierBp/10000;
    if(fraction>MaxTime-whole)return std::unexpected(E::DamageOverflow);
    plan.outputDamageRaw=static_cast<std::int64_t>(whole+fraction);
    plan.additionalDamageRaw=plan.outputDamageRaw-r.inputDamageRaw;
    if(!--plan.after.strikes)
    {
        plan.after.charge-=level.cycleChargeCost;plan.after.strikes=level.strikesPerCycle;
        if(plan.after.charge<level.cycleChargeCost)plan.after.active=false;
    }
    plan.success=true;return plan;
}
}
