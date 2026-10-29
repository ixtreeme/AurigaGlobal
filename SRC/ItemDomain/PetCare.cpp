#include <item/PetCare.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>
#include <string_view>

namespace auriga::item {
namespace {
bool ValidRange(PetBirthRange r) {
    return r.minimum<=r.maximum && r.maximum<=32767 && r.step && r.step<=32767 &&
        (r.maximum-r.minimum)%r.step==0;
}
bool InRange(std::uint16_t value,PetBirthRange r) {
    return value>=r.minimum && value<=r.maximum && (value-r.minimum)%r.step==0;
}
bool ContextIdentity(const PetCareContext& c) {
    return c.actorId && c.sessionGeneration && c.rulesetRevision &&
        !std::ranges::all_of(c.operationId,[](auto v){return !v;});
}
bool CanUse(const ItemCatalog& c,const ItemBaseRow& item,const PetCareContext& r) {
    if(!r.canUseItems || !r.ownerAlive || r.observer || r.polymorphed || item.retired ||
       r.characterClass<1 || r.characterClass>4 || r.sex<1 || r.sex>2 || r.characterLevel<item.minimumLevel)return false;
    const auto* p=c.FindUsePolicy(item.usePolicy);
    return (p->allowedClasses&(1u<<(r.characterClass-1))) && (p->allowedSexes&(1u<<(r.sex-1))) &&
        (!r.inCombat || p->useDuringCombat);
}
std::expected<const ItemCatalogEntry*,PetCareError> Snapshot(const ItemCatalog& c,const PetCareItemSnapshot& s,std::uint64_t owner) {
    const auto* entry=c.Find(s.key);
    if(!entry)return std::unexpected(PetCareError::MissingItem);
    if(!s.instanceId || !s.revision || s.revision==UINT64_MAX || s.ownerId!=owner || !s.count ||
       s.count>entry->base.maxStack || s.lockedByOther || s.exchanging || s.equipped ||
       (s.active!=(s.actorGeneration!=0)) || !c.FindRefinement(entry->base.refinementProfile)->FindLevel(s.refinementLevel))
        return std::unexpected(PetCareError::InvalidSnapshot);
    return entry;
}
const ItemCompanionRow* Growth(const ItemCatalogEntry& item) {
    const auto* row=std::get_if<ItemCompanionRow>(&item.category);
    return row && row->kind==CompanionKind::GrowthPet ? row : nullptr;
}
bool ValidLife(const ItemCompanionRow& row,const GrowthPetLife& life) {
    if(life.clock!=row.clock || life.capacityMs>INT64_MAX || life.remainingMs>life.capacityMs)return false;
    if(life.clock==CompanionClock::Permanent)return !life.capacityMs && !life.remainingMs;
    return life.clock==CompanionClock::ActiveTime && life.capacityMs>0;
}
bool ValidPetSnapshot(const PetCareItemSnapshot& s,std::uint64_t revision) {
    return s.count==1 && !s.refinementLevel && s.hasPetState && revision && revision!=UINT64_MAX;
}
// Strict UTF-8, bounded bytes/codepoints; canonical naming/uniqueness remains
// the owner's explicit reservation. This function is not a name normalizer.
bool ValidName(std::string_view name) {
    if(name.empty() || name.size()>64 || name.front()==' ' || name.back()==' ')return false;
    std::size_t at=0,points=0;
    while(at<name.size()) {
        const auto first=static_cast<unsigned char>(name[at++]);std::uint32_t cp=first;unsigned tail=0;
        if(first<0x80) {if(first<0x20 || first==0x7f)return false;}
        else if(first>=0xc2 && first<=0xdf){cp=first&0x1f;tail=1;}
        else if(first>=0xe0 && first<=0xef){cp=first&0x0f;tail=2;}
        else if(first>=0xf0 && first<=0xf4){cp=first&7;tail=3;}
        else return false;
        if(tail>name.size()-at)return false;
        for(unsigned i=0;i<tail;++i) {
            const auto b=static_cast<unsigned char>(name[at++]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&0x3f);
        }
        if((tail==1 && cp<0x80) || (tail==2 && cp<0x800) || (tail==3 && cp<0x10000) ||
           cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff) || (cp>=0x80 && cp<=0x9f) || ++points>25)return false;
    }
    return true;
}
std::uint64_t Fraction(std::uint64_t value,std::uint16_t bp) {
    // Exact floor(value*bp/10000), including INT64_MAX, without wide multiply.
    return (value/10000)*bp+((value%10000)*bp)/10000;
}
}
bool ValidPetHatchProfile(const PetHatchProfile& p) noexcept {
    return p.target.Category()==ItemCategory::Companion && p.goldCost<=INT64_MAX &&
        ValidRange(p.maximumHp) && ValidRange(p.monsterDamage) && ValidRange(p.pveNormalDamage);
}
bool ValidPetFeedRule(const PetFeedRule& r) noexcept {
    if(r.consumption==PetFoodConsumption::ExactUnits ? !r.units : r.consumption!=PetFoodConsumption::WholeStack || r.units)return false;
    switch(r.kind) {
    case PetFoodKind::LifetimeFraction:
        return r.service==PetFeedingService::Cube && r.lifetimeFractionBp>0 && r.lifetimeFractionBp<=10000 && !r.deadRecoveryBp && !r.experiencePriceDivisor;
    case PetFoodKind::RefillOrRevive:
        return r.service==PetFeedingService::Direct && !r.lifetimeFractionBp && r.deadRecoveryBp>0 && r.deadRecoveryBp<=10000 && !r.experiencePriceDivisor;
    case PetFoodKind::EquipmentExperience:
        return r.service==PetFeedingService::Cube && !r.lifetimeFractionBp && !r.deadRecoveryBp && r.experiencePriceDivisor>0;
    }
    return false;
}
std::expected<GrowthPetLife,PetCareError> MakeGrowthPetLife(const ItemCatalog& c,ItemKey key) {
    const auto* entry=c.Find(key);if(!entry)return std::unexpected(PetCareError::MissingItem);
    const auto* row=Growth(*entry);if(!row)return std::unexpected(PetCareError::NotGrowthPet);
    return GrowthPetLife{row->clock,row->durationMs,row->durationMs};
}
std::expected<PetHatchPlan,PetCareError> PlanPetHatch(const ItemCatalog& c,const PetHatchRequest& r) {
    using E=PetCareError;
    if(!r.context.admittedByServer)return std::unexpected(E::Forbidden);
    if(!ContextIdentity(r.context) || !r.walletRevision || r.walletRevision==UINT64_MAX || r.gold>INT64_MAX ||
       !r.newInstanceId || r.newInstanceId==r.egg.instanceId || !r.newInstanceReservedByServer)return std::unexpected(E::InvalidOperation);
    auto source=Snapshot(c,r.egg,r.context.actorId);if(!source)return std::unexpected(source.error());
    if(r.egg.hasPetState || r.egg.active)return std::unexpected(E::PetStateSource);
    const auto* egg=std::get_if<ItemContainerRow>(&(*source)->category);
    if(!egg || egg->kind!=ContainerKind::PetEgg)return std::unexpected(E::NotPetEgg);
    const auto* hatch=c.FindPetHatchProfile(*egg->hatchProfile);const auto* target=c.Find(hatch->target);
    if(!CanUse(c,(*source)->base,r.context) || !CanUse(c,target->base,r.context))return std::unexpected(E::Forbidden);
    if(!r.nameAcceptedByServer || !ValidName(r.name))return std::unexpected(E::InvalidName);
    if(!r.bornAtUtcMs || r.bornAtUtcMs>INT64_MAX)return std::unexpected(E::InvalidTime);
    const auto& traits=r.recordedTraits;
    if(!InRange(traits.maximumHpPercentTenths,hatch->maximumHp) ||
       !InRange(traits.monsterDamagePercentTenths,hatch->monsterDamage) ||
       !InRange(traits.pveNormalDamagePercentTenths,hatch->pveNormalDamage))return std::unexpected(E::InvalidBirthDecision);
    if(r.gold<hatch->goldCost)return std::unexpected(E::InsufficientGold);
    PetHatchPlan plan{r,hatch->target,MakeGrowthPetProgress(c,hatch->target).value(),MakeGrowthPetLife(c,hatch->target).value()};
    plan.goldCost=hatch->goldCost;plan.catalogRevision=c.Revision();plan.catalogHash=c.ContentHash();return plan;
}
std::expected<PetLifePlan,PetCareError> PlanPetLife(const ItemCatalog& c,const PetLifeRequest& r) {
    using E=PetCareError;
    if(!r.context.admittedByServer)return std::unexpected(E::Forbidden);
    if(!ContextIdentity(r.context))return std::unexpected(E::InvalidOperation);
    auto target=Snapshot(c,r.target,r.context.actorId);if(!target)return std::unexpected(target.error());
    const auto* row=Growth(**target);if(!row)return std::unexpected(E::NotGrowthPet);
    if(!ValidPetSnapshot(r.target,r.petRevision))return std::unexpected(E::InvalidSnapshot);
    if(!ValidLife(*row,r.life))return std::unexpected(E::InvalidLife);
    if(r.elapsedOwnerMs>INT64_MAX)return std::unexpected(E::InvalidTime);
    PetLifePlan plan{r,r.life};plan.catalogRevision=c.Revision();plan.catalogHash=c.ContentHash();
    if(r.life.clock==CompanionClock::ActiveTime) {
        if(r.target.active)plan.consumedMs=(std::min)(r.elapsedOwnerMs,r.life.remainingMs);
        plan.after.remainingMs-=plan.consumedMs;plan.expired=!plan.after.remainingMs;
    }
    plan.despawnFollower=r.target.active && plan.expired;return plan;
}
std::expected<PetFeedPlan,PetCareError> PlanPetFeed(const ItemCatalog& c,const PetFeedRequest& r) {
    using E=PetCareError;
    if(!r.context.admittedByServer)return std::unexpected(E::Forbidden);
    if(!ContextIdentity(r.context))return std::unexpected(E::InvalidOperation);
    if(r.service!=PetFeedingService::Cube && r.service!=PetFeedingService::Direct)return std::unexpected(E::InvalidService);
    auto food=Snapshot(c,r.food,r.context.actorId);if(!food)return std::unexpected(food.error());
    auto target=Snapshot(c,r.target,r.context.actorId);if(!target)return std::unexpected(target.error());
    const auto* row=Growth(**target);if(!row)return std::unexpected(E::NotGrowthPet);
    if(!ValidPetSnapshot(r.target,r.petRevision) || r.food.instanceId==r.target.instanceId)return std::unexpected(E::InvalidSnapshot);
    if(r.food.hasPetState || r.food.active)return std::unexpected(E::PetStateSource);
    if(!ValidLife(*row,r.life))return std::unexpected(E::InvalidLife);
    if(!CanUse(c,(*food)->base,r.context) || !CanUse(c,(*target)->base,r.context))return std::unexpected(E::Forbidden);
    if((r.service==PetFeedingService::Cube)!=r.target.active ||
       (r.target.active && r.life.clock==CompanionClock::ActiveTime && !r.life.remainingMs))return std::unexpected(E::WrongTargetState);
    const auto* rule=c.FindPetFeedRule(r.food.key,r.service);if(!rule)return std::unexpected(E::MissingFeedRule);
    const auto units=rule->consumption==PetFoodConsumption::WholeStack ? r.food.count : rule->units;
    if(r.food.count<units)return std::unexpected(E::InvalidSnapshot);
    PetFeedPlan plan{r,r.life};plan.consumeFoodUnits=units;plan.catalogRevision=c.Revision();plan.catalogHash=c.ContentHash();
    if(rule->kind==PetFoodKind::EquipmentExperience) {
        const auto experience=(*food)->base.buyAmount/rule->experiencePriceDivisor;
        if(!r.progress)return std::unexpected(E::InvalidProgress);
        if(!experience)return std::unexpected(E::NoEffect);
        GrowthPetRequest progress{r.target.key};progress.progress=*r.progress;progress.instanceId=r.target.instanceId;
        progress.revision=r.target.revision;progress.progressRevision=r.petRevision;progress.ownerId=r.target.ownerId;
        progress.actorId=r.context.actorId;progress.rulesetRevision=r.context.rulesetRevision;progress.operationId=r.context.operationId;
        progress.count=1;progress.action=GrowthPetAction::AwardItemExperience;progress.experience=experience;
        progress.admittedByServer=true;progress.activePet=true;
        auto result=PlanGrowthPetProgress(c,progress);
        if(!result)return std::unexpected(E::InvalidProgress);
        if(!result->acceptedExperience && !result->levelled)return std::unexpected(E::NoEffect);
        plan.progress=*result;return plan;
    }
    if(r.life.clock==CompanionClock::Permanent)return std::unexpected(E::NoEffect);
    if(rule->kind==PetFoodKind::RefillOrRevive) {
        plan.after.remainingMs=r.life.remainingMs ? r.life.capacityMs : Fraction(r.life.capacityMs,rule->deadRecoveryBp);
    } else {
        const auto amount=Fraction(r.life.capacityMs,rule->lifetimeFractionBp);
        plan.after.remainingMs+=(std::min)(amount,r.life.capacityMs-r.life.remainingMs);
    }
    plan.restoredMs=plan.after.remainingMs-r.life.remainingMs;
    if(!plan.restoredMs)return std::unexpected(E::NoEffect);
    plan.revived=!r.life.remainingMs;return plan;
}
}
