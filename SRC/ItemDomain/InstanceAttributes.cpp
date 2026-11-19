#include <item/InstanceAttributes.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <set>
#include <tuple>
#include <cmath>

namespace auriga::item
{
namespace
{
using E=AttributeError;
unsigned Begin(AttributePool p){return p==AttributePool::Normal ? 0 : p==AttributePool::Rare ? 5 : 7;}
unsigned Limit(AttributePool p){return p==AttributePool::Normal ? 5 : p==AttributePool::Rare ? 2 : 1;}
bool Pool(AttributePool p){return p>=AttributePool::Normal && p<=AttributePool::Extra;}
bool Bonus(BonusId id,std::int64_t value)
{
    const auto* d=FindBonusDefinition(id);
    return d && AllowsBonusSource(*d,BonusSource::Instance) && value && value>=d->minimum && value<=d->maximum;
}
const AttributeGroup* Group(const AttributeProfile& p,AttributePool pool)
{const auto i=std::ranges::find(p.groups,pool,&AttributeGroup::pool);return i==p.groups.end() ? nullptr : &*i;}
const AttributeCandidate* Candidate(const AttributeProfile& p,AttributePool pool,BonusId bonus)
{for(const auto& c:p.candidates)if(c.pool==pool && c.bonus==bonus)return &c;return nullptr;}
unsigned Count(const InstanceAttributes& s,AttributePool p)
{unsigned n=0;for(unsigned i=Begin(p);i<Begin(p)+Limit(p);++i)n+=s.slots[i].has_value();return n;}
std::set<BonusId> Excluded(const ItemCatalog& catalog,ItemKey key,std::uint16_t level,
    const AttributeProfile& p,AttributePool pool,const InstanceAttributes& state)
{
    std::set<BonusId> result;
    const auto begin=pool==AttributePool::Normal ? 0 : 5, end=pool==AttributePool::Normal ? 5 : 8;
    for(int i=begin;i<end;++i)if(state.slots[i])result.insert(state.slots[i]->bonus);
    if(Group(p,pool)->excludeStatic)
    {
        const auto profiles=catalog.BonusProfilesAt(key,level).value();
        for(const auto* profile:{profiles.base,profiles.refinement})if(profile)
            for(const auto& e:profile->Data().entries)result.insert(e.bonus);
    }
    return result;
}
unsigned TargetMask(const ItemCatalogEntry& e)
{const auto* armor=std::get_if<ItemArmorRow>(&e.category);return e.base.key.Category()==ItemCategory::Weapon ? 1 : armor ? (armor->slot==EquipmentSlot::Pendant ? 16 : 2) : e.base.key.Category()==ItemCategory::Accessory ? 4 : e.base.key.Category()==ItemCategory::Costume ? 8 : 0;}
}
bool ValidAttributeProfile(const AttributeProfile& p)
{
    if(!p.id || !detail::ContentCode(p.code) || p.groups.empty() || p.groups.size()>3 || p.candidates.size()>MaxAttributeCandidates)return false;
    std::set<AttributePool> pools;std::set<std::pair<AttributePool,BonusId>> keys;
    for(const auto& g:p.groups)if(!Pool(g.pool) || !g.capacity || g.capacity>Limit(g.pool) || !pools.insert(g.pool).second)return false;
    if(p.allowLock && !Group(p,AttributePool::Normal))return false;
    for(const auto& c:p.candidates)
    {
        if(!Group(p,c.pool) || c.weight>1'000'000 || c.values.empty() || c.values.size()>MaxAttributeTiers || !keys.insert({c.pool,c.bonus}).second)return false;
        for(std::size_t i=0;i<c.values.size();++i)if(c.values[i].tier!=i+1 || !Bonus(c.bonus,c.values[i].value))return false;
    }
    for(const auto& g:p.groups)if(!std::ranges::any_of(p.candidates,[&](const auto& c){return c.pool==g.pool;}))return false;
    return true;
}
bool ValidAttributeService(const AttributeServiceProfile& p) noexcept
{
    if(p.targetRerollGroup>2 || (p.targetRerollGroup && (p.operation!=AttributeOperation::Reroll || p.pool!=AttributePool::Normal)))return false;
    if(!p.id || !detail::ContentCode(p.code) || !Pool(p.pool) || !p.successBp || p.successBp>10000 ||
       !p.targetMask || p.targetMask>31 || p.minimumCount>p.maximumCount || p.maximumCount>Limit(p.pool) ||
       p.operation<AttributeOperation::Add || p.operation>AttributeOperation::Unlock || p.tiers.size()>MaxAttributeTiers)return false;
    if(!p.units && (p.operation!=AttributeOperation::Reroll || p.pool!=AttributePool::Rare || p.targetMask!=7 || p.minimumCount<1 || p.successBp!=10000 || p.consumeOnFailure))return false;
    const bool rolling=p.operation==AttributeOperation::Add || p.operation==AttributeOperation::Reroll;
    if((p.operation==AttributeOperation::Add)!=bool(p.addCount) || p.addCount>Limit(p.pool))return false;
    if(p.operation!=AttributeOperation::Add && (p.successBp!=10000 || p.consumeOnFailure))return false;
    if(!rolling && (p.pool!=AttributePool::Normal || !p.tiers.empty() || (p.targetMask&24)))return false;
    if(rolling && p.tiers.empty())return false;
    unsigned total=0;std::uint8_t previous=0;
    for(const auto& t:p.tiers){if(t.tier<=previous || t.tier>MaxAttributeTiers || !t.chanceBp || t.chanceBp>10000-total)return false;total+=t.chanceBp;previous=t.tier;}
    return true;
}
bool ValidAttributeBirthProfile(const AttributeBirthProfile& p) noexcept
{
    if(!p.id || !detail::ContentCode(p.code) || p.secondChanceBp>10000 || p.thirdChanceBp>10000 || p.tiers.empty() || p.tiers.size()>3*MaxAttributeTiers)return false;
    std::array<unsigned,3> sums{};std::array<std::array<bool,16>,3> seen{};
    for(const auto& t:p.tiers) {
        if(t.draw>2 || !t.tier || t.tier>MaxAttributeTiers || !t.chanceBp || t.chanceBp>10000 || seen[t.draw][t.tier-1])return false;
        seen[t.draw][t.tier-1]=true;sums[t.draw]+=t.chanceBp;
    }
    return sums==std::array<unsigned,3>{10000,10000,10000};
}
std::expected<void,AttributeError> ValidateInstanceAttributes(const ItemCatalog& c,ItemKey key,const InstanceAttributes& s)
{
    const auto* p=c.FindItemAttributes(key);
    if(!p)
    {
        if(!c.Find(key) || s.lockedSlot || std::ranges::any_of(s.slots,[](const auto& e){return e.has_value();}))return std::unexpected(E::InvalidState);
        return {};
    }
    std::set<BonusId> normal,rare;
    for(const auto pool:{AttributePool::Normal,AttributePool::Rare,AttributePool::Extra})
    {
        const auto* g=Group(*p,pool);
        for(unsigned i=0;i<Limit(pool);++i)
        {
            const auto& e=s.slots[Begin(pool)+i];if(!e)continue;
            // Explicit donor tools have their own published payload policy. They
            // must not change the ordinary random attribute candidate weights.
            const bool donor=pool==AttributePool::Rare && key.Category()==ItemCategory::Costume && std::ranges::any_of(c.Entries(),[&](const auto& entry){
                const auto* tool=std::get_if<ItemBonusToolRow>(&entry.second.category);return tool && tool->operation==SpecialBonusOperation::CostumeGrant &&
                    ((tool->payload && tool->payload->bonus==e->bonus) || std::ranges::any_of(tool->choices,[&](const auto& choice){return choice.payload.bonus==e->bonus;}));
            });
            const auto* rules=c.FindDefinitionRules(key);
            const bool addon=pool==AttributePool::Normal && (key.Category()==ItemCategory::Weapon || (rules && rules->birthAddon)) && (e->bonus==BonusId::NormalDamagePercent || e->bonus==BonusId::SkillDamagePercent);
            // Donor-only rare slots are independent of the random rare pool.
            // Ordinary costumes often publish only their normal reroll group.
            if((donor ? i>=2 : !g || i>=g->capacity) || !Bonus(e->bonus,e->valueScaled) || (!Candidate(*p,pool,e->bonus) && !donor && !addon) ||
               !(pool==AttributePool::Normal ? normal : rare).insert(e->bonus).second)return std::unexpected(E::InvalidState);
        }
    }
    if(s.lockedSlot && (!p->allowLock || *s.lockedSlot>=5 || !s.slots[*s.lockedSlot]))return std::unexpected(E::InvalidState);
    return {};
}
bool UsesPairedAttributeAddon(const ItemCatalog& c,ItemKey key,std::uint16_t level,const AttributeServiceProfile& service)
{
    const auto* rules=c.FindDefinitionRules(key,level);
    return service.operation==AttributeOperation::Reroll && service.pool==AttributePool::Normal && rules && rules->birthAddon;
}
std::expected<InstanceAttributes,AttributeError> AttributeRerollBase(const ItemCatalog& c,ItemKey key,const InstanceAttributes& before,std::span<const std::uint32_t> rolls,std::uint16_t level)
{
    auto after=before;
    for(unsigned i=0;i<5;++i)if(before.lockedSlot!=i)after.slots[i].reset();
    if(rolls.empty())return after;
    if(rolls.size()!=5 || !rolls[0] || !rolls[1] || rolls[2]>16 || rolls[3]>16 || rolls[4]>=20)return std::unexpected(E::InvalidDecision);
    const auto* profile=c.FindItemAttributes(key);if(!profile || !Group(*profile,AttributePool::Normal))return std::unexpected(E::InvalidState);
    const double u=(double(rolls[0])-.5)/double(UINT32_MAX),v=(double(rolls[1])-.5)/double(UINT32_MAX);
    const auto skill=std::clamp(static_cast<int>(std::sqrt(-2*std::log(u))*std::cos(6.2831853071795864769*v)*5+.5),-30,30);
    const int hit=-2*skill+(std::abs(skill)<=20 ? std::abs(int(rolls[2])+int(rolls[3])-16)+int(rolls[4]%4)+1 : int(rolls[4]%5)+1);
    for(const auto entry:{BonusEntry{BonusId::NormalDamagePercent,hit*100LL},BonusEntry{BonusId::SkillDamagePercent,skill*100LL}})
    {
        if(!entry.valueScaled || Excluded(c,key,level,*profile,AttributePool::Normal,after).contains(entry.bonus))continue;
        const auto free=std::ranges::find_if(after.slots.begin(),after.slots.begin()+5,[](const auto& b){return !b;});
        if(free==after.slots.begin()+5)return std::unexpected(E::NoEffect);*free=entry;
    }
    if(!ValidateInstanceAttributes(c,key,after))return std::unexpected(E::InvalidState);
    return after;
}
std::expected<InstanceAttributes,AttributeError> RollBirthMagicAttributes(const ItemCatalog& c,ItemKey key,std::uint16_t level,
    const InstanceAttributes& before,const AttributeBirthProfile& profile,std::uint16_t chance,const AttributeBirthRandom& random)
{
    if(chance>10000 || !ValidAttributeBirthProfile(profile) || !ValidateInstanceAttributes(c,key,before))return std::unexpected(E::InvalidState);
    if(!chance)return before;
    const auto* attributes=c.FindItemAttributes(key);const auto* group=attributes ? Group(*attributes,AttributePool::Normal) : nullptr;
    if(!group)return std::unexpected(E::IncompatibleTarget);
    const auto draw=[&](std::uint32_t maximum)->std::optional<std::uint32_t>{if(!maximum || !random)return {};const auto value=random(maximum);return value && *value>=1 && *value<=maximum ? value : std::nullopt;};
    const auto gate=draw(10000);if(!gate)return std::unexpected(E::InvalidDecision);if(*gate>chance)return before;
    auto after=before;
    for(unsigned ordinal=0;ordinal<3;++ordinal) {
        if(ordinal){const auto roll=draw(10000);if(!roll)return std::unexpected(E::InvalidDecision);if(*roll>(ordinal==1 ? profile.secondChanceBp : profile.thirdChanceBp))continue;}
        const auto free=std::ranges::find_if(after.slots.begin(),after.slots.begin()+group->capacity,[](const auto& b){return !b;});
        if(free==after.slots.begin()+group->capacity)continue;
        const auto roll=draw(10000);if(!roll)return std::unexpected(E::InvalidDecision);unsigned remaining=*roll,tier=0;
        for(const auto& t:profile.tiers)if(t.draw==ordinal){if(remaining<=t.chanceBp){tier=t.tier;break;}remaining-=t.chanceBp;}
        if(!tier)return std::unexpected(E::UnassignedTier);
        const auto excluded=Excluded(c,key,level,*attributes,AttributePool::Normal,after);std::uint32_t total=0;
        for(const auto& candidate:attributes->candidates)if(candidate.pool==AttributePool::Normal && !excluded.contains(candidate.bonus))total+=candidate.weight;
        if(!total)continue;
        const auto type=draw(total);if(!type)return std::unexpected(E::InvalidDecision);remaining=*type;
        for(const auto& candidate:attributes->candidates)if(candidate.pool==AttributePool::Normal && candidate.weight && !excluded.contains(candidate.bonus)) {
            if(remaining>candidate.weight){remaining-=candidate.weight;continue;}
            *free=BonusEntry{candidate.bonus,candidate.values[std::min<std::size_t>(tier,candidate.values.size())-1].value};break;
        }
    }
    if(!ValidateInstanceAttributes(c,key,after))return std::unexpected(E::InvalidState);return after;
}
std::expected<AttributePlan,AttributeError> PlanAttributeService(const ItemCatalog& c,const AttributeRequest& r)
{
    if(r.draws.size()>AttributeSlots)return std::unexpected(E::InvalidDecision);
    if(!ValidateItemOperationPair(c,r.items))return std::unexpected(E::AdmissionDenied);
    const auto* child=std::get_if<ItemConsumableRow>(&c.Find(r.items.source.item)->category);
    const auto* service=child && child->attributeServiceProfile ? c.FindAttributeService(*child->attributeServiceProfile) : nullptr;
    if(!service)return std::unexpected(E::WrongSource);
    const auto* p=c.FindItemAttributes(r.items.target.item);const auto* g=p ? Group(*p,service->pool) : nullptr;
    if(!p || !g)return std::unexpected(E::IncompatibleTarget);
    const auto& target=*c.Find(r.items.target.item);
    const auto* targetRules=c.FindDefinitionRules(r.items.target.item,r.items.target.refinementLevel);
    if(service->targetRerollGroup && service->targetRerollGroup!=(targetRules ? targetRules->attributeRerollGroup : 1))return std::unexpected(E::TargetDenied);
    const bool paired=UsesPairedAttributeAddon(c,r.items.target.item,r.items.target.refinementLevel,*service);
    if(paired ? r.addonRolls.size()!=5 : !r.addonRolls.empty())return std::unexpected(E::InvalidDecision);
    if(!(service->targetMask&TargetMask(target)) || (service->maximumTargetLevel && c.LevelPolicy(r.items.target.item,r.items.target.refinementLevel).minimumLevel>service->maximumTargetLevel))return std::unexpected(E::TargetDenied);
    if(r.items.source.count<service->units)return std::unexpected(E::InsufficientUnits);
    if(auto valid=ValidateInstanceAttributes(c,r.items.target.item,r.state);!valid)return std::unexpected(valid.error());
    const auto count=Count(r.state,service->pool);
    if(count<service->minimumCount || count>service->maximumCount)return std::unexpected(E::NotEnoughAttributes);
    AttributePlan plan{r,c.Revision(),c.ContentHash(),r.state,service->units,true};
    const bool rolling=service->operation==AttributeOperation::Add || service->operation==AttributeOperation::Reroll;
    if((service->successBp==10000 ? r.successRoll!=0 : (r.successRoll<1 || r.successRoll>10000)) || (rolling && r.lockRoll))return std::unexpected(E::InvalidDecision);
    if(!rolling)
    {
        if(!p->allowLock || !r.draws.empty())return std::unexpected(E::InvalidDecision);
        if(service->operation==AttributeOperation::Unlock)
        {
            if(!r.state.lockedSlot)return std::unexpected(E::NotLocked);
            if(r.lockRoll)return std::unexpected(E::InvalidDecision);
            plan.after.lockedSlot.reset();return plan;
        }
        if(service->operation==AttributeOperation::Lock)
        {
            if(r.state.lockedSlot)return std::unexpected(E::AlreadyLocked);
            if(count!=g->capacity)return std::unexpected(E::NotEnoughAttributes);
        }
        else if(!r.state.lockedSlot)return std::unexpected(E::NotLocked);
        std::vector<std::uint8_t> candidates;
        for(std::uint8_t i=0;i<g->capacity;++i)if(r.state.slots[i] && r.state.lockedSlot!=i)candidates.push_back(i);
        if(candidates.empty())return std::unexpected(E::NoAlternative);
        if(!r.lockRoll || r.lockRoll>candidates.size())return std::unexpected(E::InvalidDecision);
        plan.after.lockedSlot=candidates[r.lockRoll-1];return plan;
    }
    unsigned needed=service->addCount;
    if(service->operation==AttributeOperation::Add)
    {if(count+needed>g->capacity)return std::unexpected(E::NoEffect);}
    else
    {
        needed=count;
        for(unsigned i=Begin(service->pool);i<Begin(service->pool)+g->capacity;++i)
            if(plan.after.slots[i]){if(r.state.lockedSlot==i)--needed;else plan.after.slots[i].reset();}
        if(!needed)return std::unexpected(E::NoEffect);
        if(paired)
        {
            const auto base=AttributeRerollBase(c,r.items.target.item,r.state,r.addonRolls,r.items.target.refinementLevel);if(!base)return std::unexpected(base.error());
            plan.after=*base;needed=count>Count(plan.after,service->pool) ? count-Count(plan.after,service->pool) : 0;
        }
    }
    if(service->successBp<10000 && r.successRoll>service->successBp)
    {
        if(!r.draws.empty())return std::unexpected(E::InvalidDecision);
        plan.succeeded=false;plan.consumeUnits=service->consumeOnFailure ? service->units : 0;return plan;
    }
    if(r.draws.size()!=needed)return std::unexpected(E::InvalidDecision);
    for(const auto& draw:r.draws)
    {
        if(!draw.tierRoll || draw.tierRoll>10000)return std::unexpected(E::InvalidDecision);
        unsigned tier=0,remainder=draw.tierRoll;
        for(const auto& t:service->tiers){if(remainder<=t.chanceBp){tier=t.tier;break;}remainder-=t.chanceBp;}
        if(!tier)return std::unexpected(E::UnassignedTier);
        const auto excluded=Excluded(c,r.items.target.item,r.items.target.refinementLevel,*p,service->pool,plan.after);
        std::vector<const AttributeCandidate*> candidates;std::uint32_t total=0;
        for(const auto& a:p->candidates)if(a.pool==service->pool && !excluded.contains(a.bonus)){candidates.push_back(&a);total+=a.weight;}
        if(!total)return std::unexpected(E::NoCandidate);
        if(!draw.bonusRoll || draw.bonusRoll>total)return std::unexpected(E::InvalidDecision);
        auto roll=draw.bonusRoll;const AttributeCandidate* selected=nullptr;
        for(const auto* a:candidates){if(roll<=a->weight){selected=a;break;}roll-=a->weight;}
        const auto& value=selected->values[std::min<std::size_t>(tier,selected->values.size())-1];
        for(unsigned i=Begin(service->pool);i<Begin(service->pool)+g->capacity;++i)if(!plan.after.slots[i]){plan.after.slots[i]=BonusEntry{selected->bonus,value.value};break;}
    }
    if(auto valid=ValidateInstanceAttributes(c,r.items.target.item,plan.after);!valid)return std::unexpected(valid.error());
    return plan;
}
std::expected<std::vector<BonusEntry>,AttributeError> InstanceAttributeContributions(const ItemCatalog& c,ItemKey key,const InstanceAttributes& s)
{
    if(auto v=ValidateInstanceAttributes(c,key,s);!v)return std::unexpected(v.error());
    std::vector<BonusEntry> out;for(const auto& e:s.slots)if(e)out.push_back(*e);return out;
}
std::expected<std::vector<std::uint8_t>,AttributeError> EncodeInstanceAttributes(const ItemCatalog& c,ItemKey key,const InstanceAttributes& s)
{
    if(auto v=ValidateInstanceAttributes(c,key,s);!v)return std::unexpected(v.error());
    std::vector<std::uint8_t> out;auto put=[&](std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)out.push_back(static_cast<std::uint8_t>(n>>(i*8)));};
    put(0x54414741,4);put(1,2);put(static_cast<std::uint16_t>(key.Category()),2);put(key.LocalId(),4);put(s.lockedSlot ? *s.lockedSlot+1 : 0,1);
    for(const auto& e:s.slots){put(e.has_value(),1);if(e){put(static_cast<std::uint32_t>(e->bonus),4);put(static_cast<std::uint64_t>(e->valueScaled),8);}}
    return out;
}
std::expected<InstanceAttributes,AttributeError> DecodeInstanceAttributes(const ItemCatalog& c,ItemKey key,std::span<const std::uint8_t> bytes)
{
    if(bytes.size()<21 || bytes.size()>117)return std::unexpected(E::InvalidPayload);
    std::size_t pos=0;auto get=[&](unsigned size){std::uint64_t n=0;for(unsigned i=0;i<size;++i)n|=std::uint64_t{bytes[pos++]}<<(i*8);return n;};
    if(get(4)!=0x54414741 || get(2)!=1 || get(2)!=static_cast<std::uint16_t>(key.Category()) || get(4)!=key.LocalId())return std::unexpected(E::InvalidPayload);
    InstanceAttributes out;const auto locked=get(1);if(locked>5)return std::unexpected(E::InvalidPayload);if(locked)out.lockedSlot=static_cast<std::uint8_t>(locked-1);
    for(auto& slot:out.slots)
    {
        if(pos>=bytes.size())return std::unexpected(E::InvalidPayload);
        const auto present=get(1);if(present>1 || (present && bytes.size()-pos<12))return std::unexpected(E::InvalidPayload);
        if(present){const auto id=get(4),raw=get(8);const auto signedValue=raw<=INT64_MAX ? static_cast<std::int64_t>(raw) : -1-static_cast<std::int64_t>(UINT64_MAX-raw);slot=BonusEntry{static_cast<BonusId>(id),signedValue};}
    }
    if(pos!=bytes.size())return std::unexpected(E::InvalidPayload);
    if(auto v=ValidateInstanceAttributes(c,key,out);!v)return std::unexpected(v.error());return out;
}
} // namespace auriga::item
