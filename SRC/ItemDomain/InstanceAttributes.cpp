#include <item/InstanceAttributes.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <set>
#include <tuple>

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
{return e.base.key.Category()==ItemCategory::Weapon ? 1 : e.base.key.Category()==ItemCategory::Armor ? 2 : e.base.key.Category()==ItemCategory::Accessory ? 4 : e.base.key.Category()==ItemCategory::Costume ? 8 : 0;}
}
bool ValidAttributeProfile(const AttributeProfile& p)
{
    if(!p.id || !detail::ContentCode(p.code) || p.groups.empty() || p.groups.size()>3 || p.candidates.size()>MaxAttributeCandidates)return false;
    std::set<AttributePool> pools;std::set<std::pair<AttributePool,BonusId>> keys;
    for(const auto& g:p.groups)if(!Pool(g.pool) || !g.capacity || g.capacity>Limit(g.pool) || !pools.insert(g.pool).second)return false;
    if(p.allowLock && !Group(p,AttributePool::Normal))return false;
    for(const auto& c:p.candidates)
    {
        if(!Group(p,c.pool) || !c.weight || c.weight>1'000'000 || c.values.empty() || c.values.size()>MaxAttributeTiers || !keys.insert({c.pool,c.bonus}).second)return false;
        for(std::size_t i=0;i<c.values.size();++i)if(c.values[i].tier!=i+1 || !Bonus(c.bonus,c.values[i].value))return false;
    }
    for(const auto& g:p.groups)if(!std::ranges::any_of(p.candidates,[&](const auto& c){return c.pool==g.pool;}))return false;
    return true;
}
bool ValidAttributeService(const AttributeServiceProfile& p) noexcept
{
    if(!p.id || !detail::ContentCode(p.code) || !Pool(p.pool) || !p.units || !p.successBp || p.successBp>10000 ||
       !p.targetMask || p.targetMask>15 || p.minimumCount>p.maximumCount || p.maximumCount>Limit(p.pool) ||
       p.operation<AttributeOperation::Add || p.operation>AttributeOperation::Unlock || p.tiers.size()>MaxAttributeTiers)return false;
    const bool rolling=p.operation==AttributeOperation::Add || p.operation==AttributeOperation::Reroll;
    if((p.operation==AttributeOperation::Add)!=bool(p.addCount) || p.addCount>Limit(p.pool))return false;
    if(p.operation!=AttributeOperation::Add && (p.successBp!=10000 || p.consumeOnFailure))return false;
    if(!rolling && (p.pool!=AttributePool::Normal || !p.tiers.empty() || (p.targetMask&8)))return false;
    if(rolling && p.tiers.empty())return false;
    unsigned total=0;std::uint8_t previous=0;
    for(const auto& t:p.tiers){if(t.tier<=previous || t.tier>MaxAttributeTiers || !t.chanceBp || t.chanceBp>10000-total)return false;total+=t.chanceBp;previous=t.tier;}
    return true;
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
            if(!g || i>=g->capacity || !Bonus(e->bonus,e->valueScaled) || !Candidate(*p,pool,e->bonus) ||
               !(pool==AttributePool::Normal ? normal : rare).insert(e->bonus).second)return std::unexpected(E::InvalidState);
        }
    }
    if(s.lockedSlot && (!p->allowLock || *s.lockedSlot>=5 || !s.slots[*s.lockedSlot]))return std::unexpected(E::InvalidState);
    return {};
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
    if(!(service->targetMask&TargetMask(target)) || (service->maximumTargetLevel && target.base.minimumLevel>service->maximumTargetLevel))return std::unexpected(E::TargetDenied);
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
