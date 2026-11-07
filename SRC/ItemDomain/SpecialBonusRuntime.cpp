#include <item/SpecialBonusRuntime.hpp>
#include <algorithm>
#include <cmath>
#include <set>

namespace auriga::item {
namespace {
using E=ItemPersistenceError;
struct Decision {SpecialBonusAction action{};std::uint64_t source=0,target=0,elapsed=0;std::uint8_t selection=0;EquipContext actor;std::vector<std::uint32_t> rolls;};
void Put(std::vector<std::uint8_t>& b,std::uint64_t value,unsigned n){for(unsigned j=0;j<n;++j)b.push_back(static_cast<std::uint8_t>(value>>(8*j)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned at,unsigned n){std::uint64_t v=0;for(unsigned j=0;j<n;++j)v|=std::uint64_t{b[at+j]}<<(8*j);return v;}
std::vector<std::uint8_t> Encode(const Decision& d){std::vector<std::uint8_t> b{'S','B','R',1,static_cast<std::uint8_t>(d.action)};Put(b,d.source,8);Put(b,d.target,8);Put(b,d.elapsed,8);Put(b,d.selection,1);Put(b,d.actor.level,2);Put(b,d.actor.characterClass,1);Put(b,d.actor.sex,1);Put(b,d.actor.inCombat,1);Put(b,d.actor.nowUtcMs,8);Put(b,d.rolls.size(),1);for(auto r:d.rolls)Put(b,r,4);return b;}
// Explicit offsets keep persisted receipt replay independent of struct padding.
std::optional<Decision> Read(std::span<const std::uint8_t> b){
    if(b.size()<44 || b[0]!='S' || b[1]!='B' || b[2]!='R' || b[3]!=1 || b[4]<1 || b[4]>4 || b[34]>1 || b[43]>12 || b.size()!=44+4u*b[43])return {};
    Decision d{static_cast<SpecialBonusAction>(b[4]),Get(b,5,8),Get(b,13,8),Get(b,21,8),b[29]};d.actor.level=static_cast<std::uint16_t>(Get(b,30,2));d.actor.characterClass=b[32];d.actor.sex=b[33];d.actor.inCombat=b[34];d.actor.nowUtcMs=Get(b,35,8);for(unsigned j=0;j<b[43];++j)d.rolls.push_back(static_cast<std::uint32_t>(Get(b,44+j*4,4)));return d;
}
bool Bag(const PersistentItem& i){return !i.destroyed && i.placement && i.placement->kind==ItemStorageKind::Inventory;}
bool Worn(const PersistentItem& i){return !i.destroyed && i.placement && i.placement->kind==ItemStorageKind::Equipment;}
bool Admitted(const ItemCatalog& c,const PersistentItem& i,EquipContext a){const auto* e=c.Find(i.key);const auto* p=e ? c.FindUsePolicy(e->base.usePolicy) : nullptr;return e && !e->base.retired && p && a.characterClass<4 && a.sex<2 && a.level>=e->base.minimumLevel && (p->allowedClasses&(1u<<a.characterClass)) && (p->allowedSexes&(1u<<a.sex)) && (!a.inCombat || p->useDuringCombat);}
std::uint64_t Fraction(std::uint64_t n,unsigned bp){return (n/10000)*bp+(n%10000)*bp/10000;}
std::expected<std::vector<ItemChange>,E> Plan(const ItemCatalog& c,std::span<const PersistentItem> items,const Decision& d,const SpecialBonusRandom& random,Decision* recorded) {
    const auto source=std::ranges::find(items,d.source,[](const auto& i){return i.id.Value();});if(source==items.end() || source->destroyed || !source->placement || !ValidatePersistentItem(c,*source))return std::unexpected(E::InvalidItem);
    auto src=*source;++src.revision;std::size_t read=0;
    const auto draw=[&](std::uint32_t maximum)->std::optional<std::uint32_t>{std::optional<std::uint32_t> v;if(recorded){if(!random)return {};v=random(maximum);if(v)recorded->rolls.push_back(*v);}else if(read<d.rolls.size())v=d.rolls[read];++read;return v && *v<=maximum ? v : std::nullopt;};
    if(d.action!=SpecialBonusAction::Service) {
        const auto* r=std::get_if<ItemRegaliaRow>(&c.Find(src.key)->category);if(!r || r->kind!=RegaliaKind::Rune || r->slot==7 || (!Worn(src) && !(d.action==SpecialBonusAction::Maintain && Bag(src) && d.elapsed && src.regalia && !src.regalia->active)) || !src.regalia || d.target || d.selection || (d.action!=SpecialBonusAction::Maintain && !Admitted(c,src,d.actor)))return std::unexpected(E::RequirementNotMet);
        src.regalia=AdvanceRegalia(*r,*src.regalia,d.elapsed);
        if(d.action==SpecialBonusAction::Activate){if(!src.regalia->remainingMs)return std::unexpected(E::RequirementNotMet);src.regalia->active=true;}
        else if(d.action==SpecialBonusAction::Deactivate)src.regalia->active=false;
        if(src.regalia==source->regalia || !d.rolls.empty())return std::unexpected(E::InvalidChange);
        return std::vector<ItemChange>{{*source,src}};
    }
    const auto* tool=std::get_if<ItemBonusToolRow>(&c.Find(src.key)->category);
    const auto target=std::ranges::find(items,d.target,[](const auto& i){return i.id.Value();});
    if(!tool || !Bag(src) || !Admitted(c,src,d.actor) || src.count<tool->units || target==items.end() || target==source || target->owner!=src.owner || !ValidatePersistentItem(c,*target) || target->destroyed || (target->boundTo && target->boundTo!=src.owner))return std::unexpected(E::RequirementNotMet);
    auto dst=*target;++dst.revision;const auto* regalia=std::get_if<ItemRegaliaRow>(&c.Find(dst.key)->category);const bool costume=std::holds_alternative<ItemCostumeRow>(c.Find(dst.key)->category);
    if(costume){const auto lifetime=AdvanceCostumeLifetime(std::get<ItemCostumeRow>(c.Find(dst.key)->category),*dst.costume,{d.actor.nowUtcMs,0,false,false});if(!lifetime || lifetime->removeItem)return std::unexpected(E::RequirementNotMet);}
    if(tool->operation!=SpecialBonusOperation::RuneCharge && (!Bag(dst) || d.elapsed))return std::unexpected(E::InvalidPlacement);
    if(tool->operation!=SpecialBonusOperation::CostumeRemove && d.selection)return std::unexpected(E::InvalidChange);
    std::uint16_t debit=tool->units;
    switch(tool->operation) {
    case SpecialBonusOperation::StoleReroll:
        if(!regalia || regalia->kind!=RegaliaKind::Stole || !dst.regalia)return std::unexpected(E::RequirementNotMet);
        for(unsigned j=0;j<6;++j){const auto v=draw(3);if(!v)return std::unexpected(E::InvalidChange);dst.regalia->bonuses[j]=std::ranges::find_if(regalia->rules,[&](const auto& r){return r.index==j && r.variant==*v;})->bonus;}break;
    case SpecialBonusOperation::RuneCharge: {
        if(!regalia || regalia->kind!=RegaliaKind::Rune || regalia->slot==7 || !dst.regalia || !src.bonusTool || !Worn(dst))return std::unexpected(E::RequirementNotMet);
        dst.regalia=AdvanceRegalia(*regalia,*dst.regalia,d.elapsed);const auto before=dst.regalia->remainingMs,capacity=dst.regalia->capacityMs;
        if(before==capacity)return std::unexpected(E::InvalidChange);
        const auto added=std::min(capacity-before,Fraction(capacity,src.bonusTool->remainingBp));if(!added)return std::unexpected(E::InvalidChange);
        unsigned used=1;while(used<src.bonusTool->remainingBp && Fraction(capacity,used)<added)++used;
        src.bonusTool->remainingBp-=static_cast<std::uint16_t>(used);dst.regalia->remainingMs+=added;if(src.bonusTool->remainingBp)debit=0;break;}
    case SpecialBonusOperation::CostumeGrant: {
        if(!costume || !src.bonusTool || !src.bonusTool->payload)return std::unexpected(E::RequirementNotMet);
        for(unsigned j=5;j<8;++j)if(dst.attributes.slots[j] && dst.attributes.slots[j]->bonus==src.bonusTool->payload->bonus)return std::unexpected(E::InvalidChange);
        unsigned slot=5;while(slot<7 && dst.attributes.slots[slot])++slot;if(slot==7)return std::unexpected(E::InvalidChange);dst.attributes.slots[slot]=src.bonusTool->payload;break;}
    case SpecialBonusOperation::CostumeRemove:
        if(!costume || d.selection>1 || !dst.attributes.slots[5+d.selection])return std::unexpected(E::InvalidChange);
        if(!d.selection)dst.attributes.slots[5]=dst.attributes.slots[6];dst.attributes.slots[6].reset();break;
    case SpecialBonusOperation::CostumeReset: {
        const auto* p=c.FindItemAttributes(dst.key);if(!costume || !p || dst.attributes.lockedSlot || std::ranges::none_of(dst.attributes.slots.begin(),dst.attributes.slots.begin()+5,[](const auto& b){return b.has_value();}))return std::unexpected(E::RequirementNotMet);
        const auto group=std::ranges::find(p->groups,AttributePool::Normal,&AttributeGroup::pool);if(group==p->groups.end() || group->capacity<3)return std::unexpected(E::RequirementNotMet);
        for(unsigned j=0;j<5;++j)dst.attributes.slots[j].reset();std::set<BonusId> excluded;
        if(group->excludeStatic){const auto profiles=c.BonusProfilesAt(dst.key,dst.refinementLevel);if(!profiles)return std::unexpected(E::InvalidItem);for(const auto* v:{profiles->base,profiles->refinement})if(v)for(const auto& b:v->Data().entries)excluded.insert(b.bonus);}
        for(unsigned j=0;j<3;++j) {
            std::vector<const AttributeCandidate*> choices;std::uint32_t total=0;for(const auto& candidate:p->candidates)if(candidate.pool==AttributePool::Normal && !excluded.contains(candidate.bonus)){choices.push_back(&candidate);total+=candidate.weight;}
            if(!total)return std::unexpected(E::RequirementNotMet);const auto tier=draw(9999),type=draw(total-1);if(!tier || !type)return std::unexpected(E::InvalidChange);
            unsigned remaining=*tier,selected=0;for(const auto& t:tool->tiers)if(t.draw==j){if(remaining<t.chanceBp){selected=t.tier;break;}remaining-=t.chanceBp;}if(!selected)return std::unexpected(E::InvalidChange);
            auto weight=*type;const AttributeCandidate* chosen=nullptr;for(auto* choice:choices)if(weight<choice->weight){chosen=choice;break;}else weight-=choice->weight;
            if(!chosen)return std::unexpected(E::InvalidChange);dst.attributes.slots[j]=BonusEntry{chosen->bonus,chosen->values[std::min<std::size_t>(selected,chosen->values.size())-1].value};excluded.insert(chosen->bonus);
        }break;}
    case SpecialBonusOperation::PairedAddon: {
        if(dst.key.Category()!=ItemCategory::Weapon)return std::unexpected(E::RequirementNotMet);
        for(unsigned j=0;j<5;++j)if(dst.attributes.slots[j] && (dst.attributes.slots[j]->bonus==BonusId::NormalOutgoing || dst.attributes.slots[j]->bonus==BonusId::SkillOutgoing)) {if(dst.attributes.lockedSlot==j)return std::unexpected(E::RequirementNotMet);dst.attributes.slots[j].reset();}
        const auto first=draw(UINT32_MAX),second=draw(UINT32_MAX);if(!first || !second)return std::unexpected(E::InvalidChange);
        const double u=(double(*first)+.5)/4294967296.,v=(double(*second)+.5)/4294967296.;const int skill=std::clamp(static_cast<int>(std::sqrt(-2*std::log(u))*std::cos(6.2831853071795864769*v)*5+.5),-30,30);
        int hit=-2*skill;if(std::abs(skill)<=20){const auto a=draw(16),b=draw(16),tail=draw(3);if(!a || !b || !tail)return std::unexpected(E::InvalidChange);hit+=std::abs(int(*a)+int(*b)-16)+int(*tail)+1;}else{const auto tail=draw(4);if(!tail)return std::unexpected(E::InvalidChange);hit+=int(*tail)+1;}
        for(const auto b:{BonusEntry{BonusId::NormalOutgoing,hit*100LL},BonusEntry{BonusId::SkillOutgoing,skill*100LL}})if(b.valueScaled){auto slot=std::ranges::find_if(dst.attributes.slots.begin(),dst.attributes.slots.begin()+5,[](const auto& e){return !e;});if(slot==dst.attributes.slots.begin()+5)return std::unexpected(E::RequirementNotMet);*slot=b;}break;}
    }
    if(!recorded && read!=d.rolls.size())return std::unexpected(E::InvalidChange);
    if(!ValidateInstanceAttributes(c,dst.key,dst.attributes))return std::unexpected(E::InvalidItem);
    src.count-=debit;if(!src.count){src.destroyed=true;src.placement.reset();}
    // The consumed bottle's zero charge remains an auditable tombstone.
    return std::vector<ItemChange>{{*source,src},{*target,dst}};
}
bool AccessoryDebit(const ItemChange& ch){if(!ch.before || !ch.before->accessory || !ch.after.accessory || !Worn(*ch.before))return false;const auto& b=*ch.before->accessory;const auto& a=*ch.after.accessory;if(b.permanent || a.permanent || !b.activeGrade || a.unlockedCapacity!=b.unlockedCapacity || a.activeGrade>b.activeGrade || (a.activeGrade==b.activeGrade && a.remainingMs>=b.remainingMs))return false;auto p=*ch.before;p.revision=ch.after.revision;p.accessory=a;return p==ch.after;}
}
std::uint64_t SpecialBonusElapsed(const ItemCommit& c,std::uint64_t id){if(c.outcomeCode!=SpecialBonusOutcome)return 0;const auto d=Read(c.result);return d && (d->action==SpecialBonusAction::Service ? d->target : d->source)==id ? d->elapsed : 0;}
bool ValidSpecialBonusWrite(const ItemCatalog& c,const ItemCommit& commit){
    if(commit.kind!=ItemOperationKind::Service || commit.outcomeCode!=SpecialBonusOutcome || commit.owners.size()!=1 || !commit.wallets.empty() || commit.ground || commit.recovery || commit.learning || commit.petStudyCooldown)return false;
    const auto d=Read(commit.result);if(!d)return false;std::vector<PersistentItem> before;for(const auto& change:commit.items){if(!change.before)return false;before.push_back(*change.before);}
    const auto planned=Plan(c,before,*d,{},nullptr);if(!planned)return false;std::size_t count=0;for(const auto& change:commit.items){const auto expected=std::ranges::find(*planned,change.after.id,[](const auto& ch){return ch.after.id;});if(expected==planned->end()){if(!AccessoryDebit(change))return false;}else{if(change.before!=expected->before || change.after!=expected->after)return false;++count;}}return count==planned->size();
}
std::expected<ItemCommit,E> MakeSpecialBonusCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ItemInstanceId source,std::optional<ItemInstanceId> target,SpecialBonusAction action,std::uint8_t selection,std::uint64_t elapsed,EquipContext actor,const SpecialBonusRandom& random){
    Decision d{action,source.Value(),target ? target->Value() : 0,elapsed,selection,actor};
    // Check every precondition with a legal deterministic decision before consuming RNG.
    Decision probe=d;const auto preflight=Plan(c,items,d,[](std::uint32_t){return std::optional<std::uint32_t>{0};},&probe);if(!preflight)return std::unexpected(preflight.error());
    ItemCommit r;r.operationId=op;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Service;r.owners={fence};r.outcomeCode=SpecialBonusOutcome;
    r.items=*preflight;r.result=Encode(probe);
    for(const auto& ch:r.items){if(ch.after.owner!=fence.owner)return std::unexpected(E::InvalidFence);for(const auto* p:{&*ch.before,&ch.after})if(p->placement){const auto l=std::ranges::find(layouts,ItemStorage{fence.owner,p->placement->kind,p->placement->storageId},&ItemStorageLayout::storage);if(l==layouts.end())return std::unexpected(E::InvalidStorage);if(std::ranges::find(r.storages,*l)==r.storages.end())r.storages.push_back(*l);}}
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());
    auto changes=Plan(c,items,d,random,&d);if(!changes)return std::unexpected(changes.error());r.items=std::move(*changes);r.result=Encode(d);
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());return r;
}
}
