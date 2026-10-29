#include <item/PetLifecycle.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>
#include <limits>
#include <set>
#include <type_traits>

namespace auriga::item {
namespace {
using E=PetLifecycleError;
constexpr auto MaxTime=static_cast<std::uint64_t>(INT64_MAX);
bool ValidEffect(PetEffect e,std::int64_t value) {
    return e>=PetEffect::MaximumHpFlat && e<=PetEffect::DodgeArrowChance && value>=0 &&
        value<=(e==PetEffect::MaximumHpFlat ? 1'000'000'000'000'000LL : 1'000'000LL);
}
const ItemCompanionRow* Companion(const ItemCatalog& c,ItemKey key) {
    const auto* e=c.Find(key);const auto* p=e ? std::get_if<ItemCompanionRow>(&e->category) : nullptr;
    return p && p->kind==CompanionKind::GrowthPet ? p : nullptr;
}
const PetTrainingProfile* Training(const ItemCatalog& c,ItemKey key) {
    const auto* p=Companion(c,key);return p && p->trainingProfile ? c.FindPetTrainingProfile(*p->trainingProfile) : nullptr;
}
const PetSkillDefinition* Skill(const PetTrainingProfile* p,std::uint32_t id) {
    if(!p)return nullptr;
    const auto found=std::ranges::find(p->skills,id,&PetSkillDefinition::id);
    return found==p->skills.end() ? nullptr : &*found;
}
bool CanUse(const ItemCatalog& c,ItemKey key,const PetCareContext& r) {
    const auto* e=c.Find(key);
    if(!e || e->base.retired || !r.canUseItems || !r.ownerAlive || r.observer || r.polymorphed ||
       r.characterClass<1 || r.characterClass>4 || r.sex<1 || r.sex>2 || r.characterLevel<e->base.minimumLevel)return false;
    const auto* p=c.FindUsePolicy(e->base.usePolicy);
    return (p->allowedClasses&(1u<<(r.characterClass-1))) && (p->allowedSexes&(1u<<(r.sex-1))) &&
        (!r.inCombat || p->useDuringCombat);
}
bool Source(const ItemCatalog& c,const PetCareItemSnapshot& s,std::uint64_t owner,bool allowEquipped=false,bool allowContained=false) {
    const auto* e=c.Find(s.key);
    return e && s.instanceId && s.revision && s.revision!=UINT64_MAX && s.ownerId==owner && s.count &&
        s.count<=e->base.maxStack && !s.refinementLevel && !s.lockedByOther && !s.exchanging &&
        (allowEquipped || !s.equipped) && (allowContained || !s.containsPet) && !s.hasPetState && !s.active && !s.actorGeneration;
}
const PetSupplyProfile* Supply(const ItemCatalog& c,ItemKey key) {
    const auto* e=c.Find(key);const auto* r=e ? std::get_if<ItemPetSupplyRow>(&e->category) : nullptr;
    return r ? c.FindPetSupplyProfile(r->profile) : nullptr;
}
bool Alive(const GrowthPetLife& life) {return life.clock==CompanionClock::Permanent || life.remainingMs>0;}
std::expected<void,E> Request(const ItemCatalog& c,const PetLifecycleRequest& r,bool writable=true) {
    if(!r.context.admittedByServer)return std::unexpected(E::Forbidden);
    auto life=PlanPetLife(c,{r.context,r.seal,r.state.life,r.petRevision,0});
    if(!life)return std::unexpected(E::InvalidSnapshot);
    if(!r.nowUtcMs || r.nowUtcMs>MaxTime)return std::unexpected(E::InvalidTime);
    if(auto valid=ValidateGrowthPetState(c,r.seal.key,r.state);!valid)return valid;
    if(r.seal.containsPet || (r.state.carrierInstanceId && (r.seal.active || r.state.carrierInstanceId==r.seal.instanceId)))return std::unexpected(E::WrongTargetState);
    if(writable && !CanUse(c,r.seal.key,r.context))return std::unexpected(E::Forbidden);
    return {};
}
void AdvanceAge(const PetTrainingProfile* profile,GrowthPetState& s,std::uint64_t now) {
    if(now>=s.bornAtUtcMs)s.ageMs=(std::max)(s.ageMs,now-s.bornAtUtcMs);
    if(profile)for(const auto& rule:profile->slots)
        if(s.progress.stage>=rule.minimumStage && s.ageMs>=rule.minimumAgeMs)s.skills[rule.slot].unlocked=true;
}
PetLifecyclePlan Plan(const ItemCatalog& c,const PetLifecycleRequest& r) {
    return {r,r.state,c.Revision(),c.ContentHash()};
}
std::uint16_t Trait(const PetBirthTraits& traits,unsigned index) {
    return index==0 ? traits.maximumHpPercentTenths : index==1 ? traits.monsterDamagePercentTenths : traits.pveNormalDamagePercentTenths;
}
void SetTrait(PetBirthTraits& traits,unsigned index,std::uint16_t value) {
    if(index==0)traits.maximumHpPercentTenths=value;
    else if(index==1)traits.monsterDamagePercentTenths=value;
    else traits.pveNormalDamagePercentTenths=value;
}
}
bool ValidPetTrainingProfile(const PetTrainingProfile& p) noexcept {
    if(p.ages.empty() || p.ages.size()>32 || p.slots.size()!=4 || p.skills.size()>128 ||
       !p.evolutionMinimum || p.evolutionMinimum>p.evolutionMaximum || p.evolutionMaximum>32767 ||
       p.hpEvolutionPeriod>255 || p.monsterEvolutionPeriod>255 || p.pveEvolutionPeriod>255)return false;
    std::uint64_t previous=0;
    for(std::size_t i=0;i<p.ages.size();++i) {
        const auto& a=p.ages[i];
        if((i ? a.minimumAgeMs<=previous : a.minimumAgeMs!=0) || a.minimumAgeMs>MaxTime ||
           a.monsterResistanceBp>10000 || a.humanResistanceBp>10000)return false;
        previous=a.minimumAgeMs;
    }
    for(std::size_t i=0;i<4;++i)
        if(p.slots[i].slot!=i || p.slots[i].minimumStage>15 || p.slots[i].minimumAgeMs>MaxTime)return false;
    std::uint32_t previousSkill=0;
    for(const auto& skill:p.skills) {
        if(!skill.id || skill.id<=previousSkill || skill.code.empty() || skill.ranks.empty() || skill.ranks.size()>255)return false;
        for(std::size_t i=0;i<skill.ranks.size();++i)
            if(skill.ranks[i].rank!=i+1 || !ValidEffect(skill.effect,skill.ranks[i].value))return false;
        previousSkill=skill.id;
    }
    return true;
}
bool ValidPetSupplyProfile(const PetSupplyProfile& p) noexcept {
    using K=PetSupplyKind;
    if(p.kind<K::Study || p.kind>K::Skin || (p.kind==K::Study)!=p.study.has_value() ||
       (p.kind==K::ExtendLife)!=p.extension.has_value() || (p.kind==K::Enchant)!=p.enchant.has_value() ||
       (p.kind==K::Skin)!=p.skin.has_value())return false;
    if(p.study && (!p.study->skill || (!p.study->selectedAllowed && !p.study->knownAllowed) ||
       p.study->knownSuccessBp>10000 || p.study->knownCooldownMs>MaxTime ||
       (!p.study->knownAllowed && (p.study->knownSuccessBp || p.study->knownCooldownMs))))return false;
    if(p.extension && (!p.extension->addedMs || p.extension->addedMs>p.extension->maximumCapacityMs ||
       p.extension->maximumCapacityMs>MaxTime))return false;
    if(p.enchant && (!p.enchant->step || p.enchant->step>32767 || p.enchant->successBp>10000 ||
       !p.enchant->limits.maximumHpPercentTenths || p.enchant->limits.maximumHpPercentTenths>32767 ||
       !p.enchant->limits.monsterDamagePercentTenths || p.enchant->limits.monsterDamagePercentTenths>32767 ||
       !p.enchant->limits.pveNormalDamagePercentTenths || p.enchant->limits.pveNormalDamagePercentTenths>32767))return false;
    if(p.skin && (!p.skin->form || p.skin->clock<CostumeClock::Permanent || p.skin->clock>CostumeClock::EquippedTime ||
       p.skin->durationMs>MaxTime || (p.skin->clock==CostumeClock::Permanent ? p.skin->durationMs!=0 : !p.skin->durationMs)))return false;
    return true;
}
std::expected<void,E> ValidateGrowthPetState(const ItemCatalog& c,ItemKey key,const GrowthPetState& s) {
    const auto* row=Companion(c,key);if(!row)return std::unexpected(E::NotGrowthPet);
    const auto* progression=c.FindGrowthPetProfile(*row->growthProfile);const auto* training=Training(c,key);
    if(!ValidPetName(s.name) || !s.bornAtUtcMs || s.bornAtUtcMs>MaxTime || s.ageMs>MaxTime ||
       !ValidGrowthPetProgress(*progression,s.progress) || !ValidGrowthPetLife(*row,s.life) ||
       Trait(s.traits,0)>32767 || Trait(s.traits,1)>32767 || Trait(s.traits,2)>32767)return std::unexpected(E::InvalidState);
    if(s.recallRequested && (!s.everSummoned || s.carrierInstanceId || !Alive(s.life)))return std::unexpected(E::InvalidState);
    std::set<std::uint32_t> skills;
    for(const auto& slot:s.skills) {
        if(!slot.unlocked && (slot.skill || slot.rank))return std::unexpected(E::InvalidState);
        if(!slot.skill) {if(slot.rank)return std::unexpected(E::InvalidState);continue;}
        const auto* skill=Skill(training,slot.skill);
        if(!skill || !slot.rank || slot.rank>skill->ranks.size() || !skills.insert(slot.skill).second)return std::unexpected(E::InvalidSkill);
    }
    return {};
}
std::expected<GrowthPetState,E> MakeGrowthPetState(const ItemCatalog& c,const PetHatchPlan& plan) {
    if(plan.catalogRevision!=c.Revision() || plan.catalogHash!=c.ContentHash())return std::unexpected(E::InvalidOperation);
    auto hatch=PlanPetHatch(c,plan.expected);if(!hatch || hatch->target!=plan.target)return std::unexpected(E::InvalidOperation);
    GrowthPetState s{hatch->expected.name,hatch->expected.bornAtUtcMs,0,hatch->progress,hatch->life,hatch->expected.recordedTraits};
    if(auto valid=ValidateGrowthPetState(c,hatch->target,s);!valid)return std::unexpected(valid.error());
    return s;
}
std::expected<PetLifecyclePlan,E> PlanPetAge(const ItemCatalog& c,const PetLifecycleRequest& r) {
    if(auto valid=Request(c,r,false);!valid)return std::unexpected(valid.error());
    auto plan=Plan(c,r);AdvanceAge(Training(c,r.seal.key),plan.after,r.nowUtcMs);return plan;
}
std::expected<PetMaintenancePlan,E> PlanPetMaintenance(const ItemCatalog& c,const PetLifecycleRequest& r,std::uint64_t elapsed) {
    auto age=PlanPetAge(c,r);if(!age)return std::unexpected(age.error());
    auto life=PlanPetLife(c,{r.context,r.seal,r.state.life,r.petRevision,elapsed});
    if(!life)return std::unexpected(E::InvalidTime);
    age->after.life=life->after;
    if(life->expired)age->after.recallRequested=false;
    return PetMaintenancePlan{*age,elapsed,life->expired,life->despawnFollower};
}
std::expected<PetSummonPlan,E> PlanGrowthPetSummon(const ItemCatalog& c,const PetLifecycleRequest& r,bool summon,bool worldAllowsPet,bool occupied) {
    auto age=PlanPetAge(c,r);if(!age)return std::unexpected(age.error());
    PetSummonPlan plan{*age};
    if(!summon) {plan.pet.after.recallRequested=false;plan.despawnFollower=r.seal.active;return plan;}
    if(!CanUse(c,r.seal.key,r.context) || !worldAllowsPet || occupied)return std::unexpected(E::Forbidden);
    if(r.state.carrierInstanceId || !Alive(r.state.life))return std::unexpected(E::WrongTargetState);
    const auto* row=Companion(c,r.seal.key);const auto* profile=c.FindGrowthPetProfile(*row->growthProfile);
    plan.form=profile->stages[r.state.progress.stage].form;plan.spawnFollower=!r.seal.active;
    plan.pet.after.everSummoned=plan.pet.after.recallRequested=true;return plan;
}
std::expected<PetLifecyclePlan,E> PlanPetEvolution(const ItemCatalog& c,const PetLifecycleRequest& r,PetBirthTraits decisions) {
    if(auto valid=Request(c,r);!valid)return std::unexpected(valid.error());
    if(!r.seal.active || r.state.carrierInstanceId || !Alive(r.state.life))return std::unexpected(E::WrongTargetState);
    const auto* profile=Training(c,r.seal.key);if(!profile)return std::unexpected(E::MissingTraining);
    GrowthPetRequest growth{r.seal.key,r.state.progress,r.seal.instanceId,r.seal.revision,r.petRevision,
        r.seal.ownerId,r.context.actorId,r.context.rulesetRevision,r.context.operationId,r.seal.count,r.seal.refinementLevel,
        GrowthPetAction::Evolve,0,true,true};
    auto result=PlanGrowthPetProgress(c,growth);if(!result)return std::unexpected(E::InvalidState);
    auto plan=Plan(c,r);plan.after.progress=result->after;
    const std::array periods={profile->hpEvolutionPeriod,profile->monsterEvolutionPeriod,profile->pveEvolutionPeriod};
    for(unsigned i=0;i<3;++i) {
        const auto delta=Trait(decisions,i);const bool due=periods[i] && result->after.level%periods[i]==0;
        if(due ? delta<profile->evolutionMinimum || delta>profile->evolutionMaximum : delta!=0)return std::unexpected(E::InvalidDecision);
        SetTrait(plan.after.traits,i,static_cast<std::uint16_t>((std::min)(32767u,unsigned(Trait(r.state.traits,i))+delta)));
    }
    AdvanceAge(profile,plan.after,r.nowUtcMs);return plan;
}
std::expected<PetSupplyPlan,E> PlanPetSupply(const ItemCatalog& c,const PetSupplyRequest& r) {
    if(auto valid=Request(c,r.pet);!valid)return std::unexpected(valid.error());
    if(!Source(c,r.supply,r.pet.context.actorId) || r.supply.instanceId==r.pet.seal.instanceId)return std::unexpected(E::InvalidSnapshot);
    if(!CanUse(c,r.supply.key,r.pet.context))return std::unexpected(E::Forbidden);
    const auto* supply=Supply(c,r.supply.key);if(!supply)return std::unexpected(E::WrongSupply);
    if(r.pet.state.carrierInstanceId)return std::unexpected(E::WrongTargetState);
    auto after=r.pet.state;const auto* training=Training(c,r.pet.seal.key);AdvanceAge(training,after,r.pet.nowUtcMs);
    PetSupplyPlan plan{r,after};plan.catalogRevision=c.Revision();plan.catalogHash=c.ContentHash();
    using K=PetSupplyKind;
    const bool trainingAction=supply->kind>=K::Study && supply->kind<=K::ResetAll;
    if(trainingAction && (!r.pet.seal.active || !Alive(after.life)))return std::unexpected(E::WrongTargetState);
    if(!trainingAction && r.pet.seal.active)return std::unexpected(E::WrongTargetState);
    if(trainingAction && !training)return std::unexpected(E::MissingTraining);
    if(supply->kind==K::Study) {
        const auto& rule=*supply->study;const auto* skill=Skill(training,rule.skill);
        if(!skill)return std::unexpected(E::InvalidSkill);
        unsigned index=4;
        if(r.selectedSlot) {
            if(!rule.selectedAllowed || *r.selectedSlot>=4)return std::unexpected(E::InvalidSkill);
            index=*r.selectedSlot;
        } else {
            if(!rule.knownAllowed)return std::unexpected(E::InvalidSkill);
            for(unsigned i=0;i<4;++i)if(plan.after.skills[i].skill==rule.skill)index=i;
        }
        if(index>=4)return std::unexpected(E::InvalidSkill);
        auto& slot=plan.after.skills[index];
        if(!slot.unlocked)return std::unexpected(E::LockedSlot);
        if(slot.skill && slot.skill!=rule.skill)return std::unexpected(E::InvalidSkill);
        for(unsigned i=0;i<4;++i)if(i!=index && plan.after.skills[i].skill==rule.skill)return std::unexpected(E::DuplicateSkill);
        if(slot.rank>=skill->ranks.size())return std::unexpected(E::MaximumRank);
        if(r.selectedSlot) {
            if(r.recordedRollBp)return std::unexpected(E::InvalidDecision); // no decision is used by certain learning
            plan.succeeded=true;
        } else {
            if(!r.ownerCooldownRevision || r.ownerCooldownRevision==UINT64_MAX || r.ownerSkillNotBeforeUtcMs>MaxTime)return std::unexpected(E::InvalidOperation);
            if(r.recordedRollBp>=10000)return std::unexpected(E::InvalidDecision);
            if(r.ownerSkillNotBeforeUtcMs>r.pet.nowUtcMs) {
                if(!r.bypassAvailable || !r.bypassEffectRevision || r.bypassEffectRevision==UINT64_MAX)return std::unexpected(E::Cooldown);
                plan.consumeBypass=true;
            }
            plan.succeeded=r.recordedRollBp<rule.knownSuccessBp;
            if(plan.succeeded) {
                if(rule.knownCooldownMs>MaxTime-r.pet.nowUtcMs)return std::unexpected(E::InvalidTime);
                plan.writeOwnerCooldown=true;plan.cooldownSkill=rule.skill;
                plan.ownerSkillNotBeforeUtcMs=r.pet.nowUtcMs+rule.knownCooldownMs;
            }
        }
        if(plan.succeeded){slot.skill=rule.skill;++slot.rank;}
    } else if(supply->kind==K::ResetOne || supply->kind==K::ResetAll) {
        bool changed=false;
        if(supply->kind==K::ResetOne && (!r.selectedSlot || *r.selectedSlot>=4))return std::unexpected(E::InvalidSkill);
        for(unsigned i=0;i<4;++i)if(supply->kind==K::ResetAll || i==*r.selectedSlot) {
            auto& slot=plan.after.skills[i];changed=changed || slot.skill!=0;slot.skill=0;slot.rank=0;
        }
        if(!changed)return std::unexpected(E::NoEffect);plan.succeeded=true;
    } else if(supply->kind==K::ExtendLife || supply->kind==K::PermanentLife) {
        if(after.life.clock==CompanionClock::Permanent)return std::unexpected(E::NoEffect);
        if(supply->kind==K::PermanentLife)plan.after.life={CompanionClock::Permanent,0,0,true};
        else {
            const auto& rule=*supply->extension;
            if(after.life.remainingMs>rule.maximumCapacityMs || rule.addedMs>rule.maximumCapacityMs-after.life.remainingMs)return std::unexpected(E::NoEffect);
            const auto amount=after.life.remainingMs+rule.addedMs;
            plan.after.life={CompanionClock::ActiveTime,amount,amount};
        }
        plan.succeeded=true;
    } else if(supply->kind==K::Enchant) {
        if(r.traitIndex>=3)return std::unexpected(E::InvalidDecision);
        const auto& rule=*supply->enchant;const auto current=Trait(after.traits,r.traitIndex),limit=Trait(rule.limits,r.traitIndex);
        if(current>=limit)return std::unexpected(E::NoEffect);
        if(r.recordedRollBp>=10000)return std::unexpected(E::InvalidDecision);
        plan.succeeded=r.recordedRollBp<rule.successBp;
        if(plan.succeeded)SetTrait(plan.after.traits,r.traitIndex,static_cast<std::uint16_t>((std::min)(unsigned(limit),unsigned(current)+rule.step)));
    } else return std::unexpected(E::WrongSupply);
    return plan;
}
std::expected<PetBonusPlan,E> PlanPetBonuses(const ItemCatalog& c,const PetLifecycleRequest& r,bool worldAllowsPet,bool inDungeon) {
    auto age=PlanPetAge(c,r);if(!age)return std::unexpected(age.error());
    PetBonusPlan plan{*age};const auto* row=Companion(c,r.seal.key);const auto& s=plan.pet.after;
    if(!r.seal.active || s.carrierInstanceId || !Alive(s.life) || !worldAllowsPet || !r.context.ownerAlive ||
       r.context.observer || r.context.polymorphed || c.Find(r.seal.key)->base.retired || (row->dungeonOnlyBonus && !inDungeon))return plan;
    const auto* training=Training(c,r.seal.key);
    if(training) {
        const PetAgeTier* tier=&training->ages.front();for(const auto& a:training->ages)if(s.ageMs>=a.minimumAgeMs)tier=&a;
        for(const auto& [effect,value]:std::array<std::pair<PetEffect,std::int64_t>,3>{{{PetEffect::MaximumHpFlat,tier->maximumHpFlat},
            {PetEffect::MonsterResistancePercent,tier->monsterResistanceBp},{PetEffect::HumanResistancePercent,tier->humanResistanceBp}}})
            if(value)plan.contributions.push_back({effect,value,PetBonusSource::Age,0});
    }
    for(unsigned i=0;i<3;++i) {
        // Preserve the source's truncation to an integer percentage first.
        const auto value=static_cast<std::int64_t>(Trait(s.traits,i)/10)*100;
        if(value)plan.contributions.push_back({std::array{PetEffect::MaximumHpPercent,PetEffect::MonsterDamagePercent,PetEffect::NormalPveDamagePercent}[i],value,PetBonusSource::Trait,static_cast<std::uint8_t>(i)});
    }
    for(unsigned i=0;i<4;++i)if(const auto& slot=s.skills[i];slot.skill) {
        const auto* skill=Skill(training,slot.skill);const auto value=skill->ranks[slot.rank-1].value;
        if(value)plan.contributions.push_back({skill->effect,value,PetBonusSource::Skill,static_cast<std::uint8_t>(i)});
    }
    return plan;
}
std::expected<PetTransportPlan,E> PlanPetTransport(const ItemCatalog& c,const PetTransportRequest& r) {
    if(auto valid=Request(c,r.pet);!valid)return std::unexpected(valid.error());
    if(!Source(c,r.carrier,r.pet.context.actorId,false,r.unpack) || r.carrier.containsPet!=(r.containedSealInstanceId!=0) ||
       r.carrier.count!=1 || r.carrier.instanceId==r.pet.seal.instanceId ||
       !r.carrierRevision || r.carrierRevision==UINT64_MAX)return std::unexpected(E::InvalidSnapshot);
    if(!CanUse(c,r.carrier.key,r.pet.context))return std::unexpected(E::Forbidden);
    const auto* supply=Supply(c,r.carrier.key);
    if(!supply || supply->kind!=PetSupplyKind::Carrier)return std::unexpected(E::WrongSupply);
    if(r.pet.seal.active)return std::unexpected(E::WrongTargetState);
    PetTransportPlan plan{r,r.pet.state,false,c.Revision(),c.ContentHash()};
    if(r.unpack) {
        if(r.pet.state.carrierInstanceId!=r.carrier.instanceId || r.containedSealInstanceId!=r.pet.seal.instanceId)return std::unexpected(E::WrongTargetState);
        if(!r.placementReservedByServer)return std::unexpected(E::InvalidPlacement);
        plan.after.carrierInstanceId=0;plan.consumeCarrier=true;
    } else {
        if(r.pet.state.carrierInstanceId || r.pet.state.recallRequested || r.containedSealInstanceId || !Alive(r.pet.state.life))return std::unexpected(E::WrongTargetState);
        plan.after.carrierInstanceId=r.carrier.instanceId;
    }
    return plan;
}
std::expected<PetAppearancePlan,E> PlanPetAppearance(const ItemCatalog& c,const PetAppearanceRequest& r) {
    if(auto valid=Request(c,r.pet,false);!valid)return std::unexpected(valid.error());
    if(!Source(c,r.skin,r.pet.context.actorId,true) || r.skin.count!=1 || r.skin.instanceId==r.pet.seal.instanceId ||
       r.time.wasEquipped!=r.skin.equipped)return std::unexpected(E::InvalidSnapshot);
    const auto* supply=Supply(c,r.skin.key);if(!supply || !supply->skin)return std::unexpected(E::WrongSupply);
    const auto& definition=*supply->skin;
    // Reuse the same typed lifetime math as ordinary costumes; no second
    // clock writer and no legacy costume/proto lookup is introduced.
    const CostumeClockDefinition clock{definition.clock,definition.durationMs};
    auto step=r.time;step.useRequested=r.equipRequested;step.nowUtcMs=r.pet.nowUtcMs;
    auto time=AdvanceItemClock(clock,r.lifetime,step);if(!time)return std::unexpected(E::InvalidTime);
    const auto* companion=Companion(c,r.pet.seal.key);const auto* growth=c.FindGrowthPetProfile(*companion->growthProfile);
    const auto ordinary=growth->stages[r.pet.state.progress.stage].form;
    const bool eligible=r.worldAllowsPet && r.pet.context.ownerAlive && !r.pet.context.observer && !r.pet.context.polymorphed &&
        r.pet.seal.active && !r.pet.state.carrierInstanceId && Alive(r.pet.state.life) &&
        CanUse(c,r.skin.key,r.pet.context) && !c.Find(r.pet.seal.key)->base.retired;
    const bool equip=r.equipRequested && eligible && !time->removeItem;
    return PetAppearancePlan{r,*time,equip ? definition.form : ordinary,equip,c.Revision(),c.ContentHash()};
}
}
