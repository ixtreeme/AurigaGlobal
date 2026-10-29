#include <item/PolymorphItems.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>
#include <limits>

namespace auriga::item
{
namespace { constexpr std::uint64_t MaxTime=std::uint64_t{INT64_MAX}; }
bool ValidPolymorphProfile(const PolymorphProfile& p) noexcept
{
    if(p.kind<PolymorphKind::Orb || p.kind>PolymorphKind::PracticeBook || p.skills.size()!=41 || !p.graceLossDenominator)return false;
    if(p.kind==PolymorphKind::Orb)
    {if(p.maxBookGrade || p.practicePerGrade || p.bookChanceBasePercent || !p.masteries.empty())return false;}
    else
    {
        if(p.mobLevelGrace || p.graceLossNumerator || p.graceLossDenominator!=1 || p.maxBookGrade>255 ||
           std::uint64_t{p.maxBookGrade}*p.practicePerGrade>UINT32_MAX || p.bookChanceBasePercent< -10000 ||
           p.bookChanceBasePercent>10000 || p.masteries.size()!=4)return false;
        for(std::size_t i=0;i<p.masteries.size();++i)
        {const auto& r=p.masteries[i];if(r.mastery!=i || !r.durationMs || r.durationMs>MaxTime || r.bonusDurationMs>r.durationMs)return false;}
    }
    for(std::size_t i=0;i<p.skills.size();++i)
    {
        const auto& r=p.skills[i];
        if(r.skillLevel!=i || r.powerBp>10000 || r.attackBonusBp>1000000 || r.durationMs>MaxTime)return false;
        if(p.kind==PolymorphKind::Orb ? r.durationMs==0 : (r.durationMs!=0 || r.attackBonusBp!=0))return false;
    }
    return true;
}
std::expected<PolymorphPlan,PolymorphError> PlanPolymorph(const ItemCatalog& c,const PolymorphRequest& r)
{
    using E=PolymorphError;
    if(!r.admittedByServer)return std::unexpected(E::Forbidden);
    if(!r.actorId || !r.rulesetRevision || std::ranges::all_of(r.operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    if(!r.instanceId || !r.revision || r.revision==UINT64_MAX || r.ownerId!=r.actorId || r.locked || r.exchanging || r.equipped)return std::unexpected(E::InvalidSnapshot);
    const auto* item=c.Find(r.item);if(!item)return std::unexpected(E::MissingItem);
    const auto* row=std::get_if<ItemPolymorphRow>(&item->category);if(!row)return std::unexpected(E::NotPolymorph);
    if(item->base.retired)return std::unexpected(E::RetiredItem);
    if(!r.count || r.count>item->base.maxStack)return std::unexpected(E::InvalidStack);
    if(r.refinementLevel || r.characterLevel<1 || r.characterLevel>255 || r.skillLevel>40 || r.mastery>3 ||
       r.characterClass<1 || r.characterClass>4 || r.sex<1 || r.sex>2)return std::unexpected(E::InvalidLevel);
    const auto* policy=c.FindUsePolicy(item->base.usePolicy);
    if(r.characterLevel<item->base.minimumLevel || !(policy->allowedClasses&(1u<<(r.characterClass-1))) ||
       !(policy->allowedSexes&(1u<<(r.sex-1))) || (r.inCombat && !policy->useDuringCombat))return std::unexpected(E::Forbidden);
    const auto* p=c.FindPolymorphProfile(row->profile);
    if((row->fixedForm && r.state.boundForm) || (p->kind==PolymorphKind::Orb && (r.state.bookGrade || r.state.practiceRemaining)) ||
       (p->kind==PolymorphKind::PracticeBook && (r.count!=1 || r.state.bookGrade>p->maxBookGrade)))return std::unexpected(E::InvalidState);
    const auto formId=row->fixedForm ? row->fixedForm : r.state.boundForm;
    const auto* form=formId ? c.FindPolymorphForm(*formId) : nullptr;
    if(!form)return std::unexpected(E::MissingForm);
    PolymorphPlan plan{r,r.state,{},0,0,false,c.Revision(),c.ContentHash()};
    if(r.action==PolymorphAction::UpgradeBook)
    {
        if(p->kind!=PolymorphKind::PracticeBook)return std::unexpected(E::WrongAction);
        if(r.recordedRoll)return std::unexpected(E::InvalidDecision);
        if(r.state.bookGrade==p->maxBookGrade)return std::unexpected(E::AtMaximum);
        if(r.state.practiceRemaining)return std::unexpected(E::NotReady);
        ++plan.after.bookGrade;
        plan.after.practiceRemaining=static_cast<std::uint32_t>(std::uint64_t{r.state.bookGrade}*p->practicePerGrade);
        plan.success=true;return plan;
    }
    if(r.action!=PolymorphAction::Use)return std::unexpected(E::WrongAction);
    if(r.riding || r.alreadyPolymorphed || r.duelBlocksPolymorph)return std::unexpected(E::Forbidden);
    if(!r.formAllowedByServer || !r.mobCatalogRevision || r.mobTemplate!=form->mobTemplate || !r.mobLevel || r.mobLevel>255)
        return std::unexpected(E::MobMismatch);
    const auto& skill=p->skills[r.skillLevel];
    std::uint64_t duration=0,bonusDuration=0;PolymorphBonus bonus=PolymorphBonus::None;std::uint32_t magnitude=0;
    if(p->kind==PolymorphKind::Orb)
    {
        const auto loss=(std::uint64_t{r.characterLevel}*p->graceLossNumerator)/p->graceLossDenominator;
        const auto grace=loss>=p->mobLevelGrace ? 0 : p->mobLevelGrace-loss;
        if(r.mobLevel>=r.characterLevel+grace)return std::unexpected(E::MobLevelTooHigh);
        plan.chanceBp=10000;duration=skill.durationMs;bonusDuration=duration;
        if(skill.attackBonusBp){bonus=PolymorphBonus::Attack;magnitude=skill.attackBonusBp;}
    }
    else
    {
        const auto chance=std::int64_t{r.characterLevel}-r.mobLevel+p->bookChanceBasePercent+r.skillLevel+r.state.bookGrade;
        plan.chanceBp=static_cast<std::uint16_t>(std::clamp<std::int64_t>(chance,0,100)*100);
        duration=p->masteries[r.mastery].durationMs;bonusDuration=p->masteries[r.mastery].bonusDurationMs;
        bonus=form->bookBonus;magnitude=(std::uint32_t{r.skillLevel}+r.state.bookGrade)*100;
    }
    if(!plan.chanceBp || plan.chanceBp==10000)
    {if(r.recordedRoll)return std::unexpected(E::InvalidDecision);plan.success=plan.chanceBp!=0;}
    else
    {if(!r.recordedRoll || r.recordedRoll>10000)return std::unexpected(E::InvalidDecision);plan.success=r.recordedRoll<=plan.chanceBp;}
    if(!plan.success)return plan;
    if(r.nowUtcMs>MaxTime || duration>MaxTime-r.nowUtcMs || bonusDuration>MaxTime-r.nowUtcMs)return std::unexpected(E::TimeOverflow);
    if(bonus==PolymorphBonus::None || !bonusDuration || !magnitude){bonus=PolymorphBonus::None;bonusDuration=0;magnitude=0;}
    const auto unit=bonus==PolymorphBonus::None ? PolymorphBonusUnit::None :
        bonus==PolymorphBonus::Movement ? PolymorphBonusUnit::MovementPoints : PolymorphBonusUnit::BasisPoints;
    if(unit==PolymorphBonusUnit::MovementPoints)magnitude/=100; // Original POINT_MOV_SPEED is additive speed points.
    plan.effect=PolymorphEffectPlan{form->id,form->mobTemplate,skill.powerBp,r.nowUtcMs+duration,
        bonusDuration ? r.nowUtcMs+bonusDuration : 0,bonus,unit,magnitude};
    if(p->kind==PolymorphKind::Orb)plan.consumeUnits=1;
    else if(plan.after.practiceRemaining)--plan.after.practiceRemaining;
    return plan;
}
}
