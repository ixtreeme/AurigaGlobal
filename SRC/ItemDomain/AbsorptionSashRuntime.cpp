#include <item/AbsorptionSashRuntime.hpp>
#include <algorithm>
#include <climits>

namespace auriga::item {
namespace {
using E=ItemPersistenceError;
const PersistentItem* Find(std::span<const PersistentItem> items,ItemInstanceId id){const auto i=std::ranges::find(items,id,&PersistentItem::id);return i==items.end() ? nullptr : &*i;}
const ItemWallet* Gold(std::span<const ItemWallet> wallets,ItemOwner owner){const auto i=std::ranges::find_if(wallets,[&](const auto& w){return w.owner==owner && w.currency==WalletCurrency::Gold;});return i==wallets.end() ? nullptr : &*i;}
bool Admitted(const ItemCatalog& c,const PersistentItem& i,const AbsorptionSashActor& actor){
    const auto* e=c.Find(i.key);const auto* p=e ? c.FindUsePolicy(e->base.usePolicy) : nullptr;const auto& a=actor.equipment;
    return actor.admitted && actor.alive && !actor.observer && !actor.polymorphed && e && !e->base.retired &&
        p && a.characterClass<4 && a.sex<2 && a.level>=c.LevelPolicy(i.key,i.refinementLevel).minimumLevel &&
        (p->allowedClasses&(1u<<a.characterClass)) && (p->allowedSexes&(1u<<a.sex)) && (!a.inCombat || p->useDuringCombat);
}
bool Bag(const PersistentItem& i,ItemOwner owner){return i.owner==owner && !i.destroyed && i.placement && i.placement->kind==ItemStorageKind::Inventory && (!i.boundTo || i.boundTo==owner);}
std::optional<ItemKey> NextStole(ItemKey key, std::uint8_t grade)
{
    // The catalog's migrated stoles have independent keys at each grade.
    static constexpr std::uint32_t families[][4] = {
        {163,164,165,147}, {166,167,168,148}, {169,170,171,149}, {172,173,174,150},
        {175,176,177,151}, {178,179,180,152}, {181,182,183,153}, {184,185,186,154},
        {187,188,189,155}, {190,191,192,156}, {193,194,195,157}, {196,197,198,158},
        {199,200,201,159}, {202,203,204,205},
    };
    if (key.Category() != ItemCategory::Regalia || grade < 1 || grade > 3)
        return {};
    for (const auto& family : families)
        if (family[grade - 1] == key.LocalId())
            return ItemKey::Create(static_cast<std::uint16_t>(ItemCategory::Regalia), family[grade]).value();
    return {};
}
std::expected<AbsorptionSashQuote,E> PrepareStoleCombination(const ItemCatalog& catalog,
    const PersistentItem& first, const PersistentItem& second, std::span<const ItemWallet> wallets,
    const AbsorptionSashActor& actor)
{
    const auto* entry = catalog.Find(first.key);
    const auto* stole = std::get_if<ItemRegaliaRow>(&entry->category);
    if (first.key != second.key || !stole || stole->kind != RegaliaKind::Stole ||
        !first.regalia || !second.regalia || first.count != 1 || second.count != 1 ||
        first.costume || second.costume || !Admitted(catalog, second, actor))
        return std::unexpected(E::RequirementNotMet);

    const auto next = NextStole(first.key, stole->grade);
    const auto* result = next ? catalog.Find(*next) : nullptr;
    const auto* nextStole = result ? std::get_if<ItemRegaliaRow>(&result->category) : nullptr;
    if (!nextStole || result->base.retired || nextStole->kind != RegaliaKind::Stole ||
        nextStole->grade != stole->grade + 1 || result->base.width != entry->base.width ||
        result->base.height != entry->base.height)
        return std::unexpected(E::RequirementNotMet);

    constexpr std::uint64_t prices[] = {1000000, 4000000, 8000000};
    constexpr std::uint16_t chances[] = {8000, 6000, 4000};
    const auto cost = prices[stole->grade - 1];
    const auto* wallet = Gold(wallets, first.owner);
    if (!wallet || !wallet->revision || wallet->revision == UINT64_MAX || wallet->balance < cost)
        return std::unexpected(E::InvalidWallet);

    AbsorptionSashQuote quote{AbsorptionSashOperation::CombineStoles, first.id, second.id};
    quote.resultKey = next;
    quote.grade = nextStole->grade;
    quote.successBp = chances[stole->grade - 1];
    quote.goldCost = cost;
    return quote;
}
void Put(std::vector<std::uint8_t>& b,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned at,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(b[at+i])<<(8*i);return v;}
std::vector<std::uint8_t> Receipt(AbsorptionSashOperation mode,ItemInstanceId source,ItemInstanceId target,const AbsorptionSashActor& a,const AbsorptionSashDecision& d){
    std::vector<std::uint8_t> b{'S','A','S',1,static_cast<std::uint8_t>(mode)};Put(b,source.Value(),8);Put(b,target.Value(),8);Put(b,d.newInstance ? d.newInstance->Value() : 0,8);Put(b,d.successRoll,2);Put(b,d.resultPercent,1);
    Put(b,a.equipment.level,2);Put(b,a.equipment.characterClass,1);Put(b,a.equipment.sex,1);Put(b,a.equipment.inCombat,1);Put(b,(a.admitted ? 1 : 0)|(a.alive ? 2 : 0)|(a.observer ? 4 : 0)|(a.polymorphed ? 8 : 0),1);Put(b,a.equipment.nowUtcMs,8);return b;
}
bool AddLayout(ItemCommit& r,std::span<const ItemStorageLayout> layouts,const PersistentItem& i){
    if(!i.placement)return true;const ItemStorage wanted{i.owner,i.placement->kind,i.placement->storageId};const auto l=std::ranges::find(layouts,wanted,&ItemStorageLayout::storage);if(l==layouts.end())return false;
    if(std::ranges::find(r.storages,*l)==r.storages.end())r.storages.push_back(*l);return true;
}
// Constructs the strict expected mutation without validating itself recursively.
// The ordinary birth factory is validated independently before its source-owned
// donor/percentage state is replaced by the recorded service outcome.
std::expected<ItemCommit,E> Build(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,
    const AbsorptionSashQuote& q,const AbsorptionSashActor& actor,const AbsorptionSashDecision& d){
    const auto* a=Find(items,q.source);const auto* b=Find(items,q.target);if(!a || !b)return std::unexpected(E::MissingItem);
    const bool combine=IsRegaliaCombination(q.operation),success=combine && d.successRoll<=q.successBp;
    if(combine){
        if(!d.successRoll || d.successRoll>10000 || (success ? !d.newInstance || d.resultPercent<q.minimumPercent || d.resultPercent>q.maximumPercent || *d.newInstance==a->id || *d.newInstance==b->id : d.newInstance.has_value() || d.resultPercent))return std::unexpected(E::InvalidChange);
    }else if(d.successRoll || d.resultPercent || d.newInstance)return std::unexpected(E::InvalidChange);
    const auto debitId=q.operation==AbsorptionSashOperation::Clear ? a->id : b->id;
    const auto* tool=q.operation==AbsorptionSashOperation::Clear ? std::get_if<ItemBonusToolRow>(&c.Find(a->key)->category) : nullptr;
    auto r=MakeConsumeCommit(c,op,fence,layouts,items,debitId,tool ? tool->units : 1);if(!r)return std::unexpected(r.error());
    r->kind=ItemOperationKind::Service;r->outcomeCode=AbsorptionSashOutcome;r->result=Receipt(q.operation,q.source,q.target,actor,d);
    if(!AddLayout(*r,layouts,*a) || !AddLayout(*r,layouts,*b))return std::unexpected(E::InvalidStorage);
    if(combine){
        auto first=*a;++first.revision;if(success){first.count=0;first.destroyed=true;first.placement.reset();}r->items.push_back({*a,first});
        const auto* wallet=Gold(wallets,fence.owner);if(!wallet || wallet->revision==UINT64_MAX || wallet->balance<q.goldCost)return std::unexpected(E::InvalidWallet);auto next=*wallet;++next.revision;next.balance-=q.goldCost;r->wallets.push_back({*wallet,next});
        if(success){
            std::vector<PersistentItem> remaining;for(const auto& i:items)if(i.id!=a->id && i.id!=b->id)remaining.push_back(i);
            ItemGrant grant{*q.resultKey,0,1};
            grant.createdAtUtcMs=actor.equipment.nowUtcMs;
            const auto granted=MakeGrantCommit(c,op,fence,layouts,remaining,grant,std::array{*d.newInstance});if(!granted || granted->items.size()!=1)return std::unexpected(granted ? E::InvalidChange : granted.error());
            auto result=granted->items.front().after;result.placement=a->placement;
            if(q.operation==AbsorptionSashOperation::Combine){result.sash=a->sash;result.sash->percent=d.resultPercent;}
            if(a->boundTo || b->boundTo)result.boundTo=fence.owner;
            r->items.push_back({{},std::move(result)});
        }
    }else{
        const auto* before=q.operation==AbsorptionSashOperation::Clear ? b : a;auto after=*before;++after.revision;after.sash=q.previewMinimum;
        if(q.operation==AbsorptionSashOperation::Absorb && b->boundTo)after.boundTo=fence.owner;
        r->items.push_back({*before,std::move(after)});
    }
    return r;
}
}
std::expected<AbsorptionSashQuote,E> PrepareAbsorptionSash(const ItemCatalog& c,ItemOwner owner,
    std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,ItemInstanceId first,ItemInstanceId second,
    AbsorptionSashOperation mode,const AbsorptionSashActor& actor){
    const auto* a=Find(items,first);const auto* b=Find(items,second);
    if(owner.kind!=ItemOwnerKind::Character || first==second || !a || !b || !Bag(*a,owner) || !Bag(*b,owner) || !ValidatePersistentItem(c,*a) || !ValidatePersistentItem(c,*b) || a->revision==UINT64_MAX || b->revision==UINT64_MAX || !Admitted(c,*a,actor))return std::unexpected(E::RequirementNotMet);
    if(mode==AbsorptionSashOperation::CombineStoles)return PrepareStoleCombination(c,*a,*b,wallets,actor);
    if(mode<AbsorptionSashOperation::Combine || mode>AbsorptionSashOperation::Clear)return std::unexpected(E::InvalidOperation);
    const auto* sash=mode==AbsorptionSashOperation::Clear ? b : a;const auto* p=c.FindAbsorptionSash(sash->key);
    if(!p || !sash->sash)return std::unexpected(E::RequirementNotMet);
    AbsorptionSashQuote q{mode,first,second};q.resultKey=sash->key;q.grade=p->grade;q.currentPercent=q.minimumPercent=q.maximumPercent=sash->sash->percent;q.successBp=10000;q.previewMinimum=q.previewMaximum=*sash->sash;
    if(mode==AbsorptionSashOperation::Combine){
        const auto* other=c.FindAbsorptionSash(b->key);const auto* next=c.Find(p->next);const auto* current=c.Find(a->key);
        if(!other || !b->sash || other->grade!=p->grade || a->sash->percent>=p->maximumPercent || !next || next->base.width!=current->base.width || next->base.height!=current->base.height)return std::unexpected(E::RequirementNotMet);
        const auto* wallet=Gold(wallets,owner);if(!wallet || !wallet->revision || wallet->revision==UINT64_MAX || wallet->balance<p->goldCost)return std::unexpected(E::InvalidWallet);
        q.resultKey=p->next;q.goldCost=p->goldCost;q.successBp=p->successBp;
        q.minimumPercent=p->grade==4 ? a->sash->percent+p->combineMinimum : p->combineMinimum;
        q.maximumPercent=p->grade==4 ? std::min<unsigned>(p->maximumPercent,a->sash->percent+p->combineMaximum) : p->combineMaximum;
        q.previewMinimum.percent=q.minimumPercent;q.previewMaximum.percent=q.maximumPercent;
    }else if(mode==AbsorptionSashOperation::Absorb){
        if(a->sash->donor || !c.FindSashDonor(b->key,b->refinementLevel))return std::unexpected(E::RequirementNotMet);
        q.previewMinimum.donor=AbsorbedSashDonor{b->key,b->refinementLevel};
        for(unsigned i=0;i<8;++i)if(b->attributes.slots[i] && b->attributes.slots[i]->valueScaled>0)q.previewMinimum.donor->attributes[i]=b->attributes.slots[i];
        q.previewMaximum=q.previewMinimum;
    }else{
        const auto* tool=std::get_if<ItemBonusToolRow>(&c.Find(a->key)->category);
        if(!tool || tool->operation!=SpecialBonusOperation::SashClear || a->count<tool->units || !b->sash->donor)return std::unexpected(E::RequirementNotMet);
        q.previewMinimum.donor.reset();q.previewMaximum=q.previewMinimum;
    }
    return q;
}
std::expected<AbsorptionSashWrite,E> MakeAbsorptionSashCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,
    ItemInstanceId source,ItemInstanceId target,AbsorptionSashOperation mode,const AbsorptionSashActor& actor,const AbsorptionSashDecision& d){
    const auto q=PrepareAbsorptionSash(c,fence.owner,items,wallets,source,target,mode,actor);if(!q)return std::unexpected(q.error());
    auto r=Build(c,op,fence,layouts,items,wallets,*q,actor,d);if(!r)return std::unexpected(r.error());
    if(const auto valid=ValidateItemCommit(c,*r);!valid)return std::unexpected(valid.error());
    return AbsorptionSashWrite{std::move(*r),*q,!IsRegaliaCombination(mode) || d.successRoll<=q->successBp,d.newInstance};
}
bool ValidAbsorptionSashWrite(const ItemCatalog& c,const ItemCommit& r){
    const auto& b=r.result;if(r.kind!=ItemOperationKind::Service || r.outcomeCode!=AbsorptionSashOutcome || r.owners.size()!=1 || r.ground || r.recovery || r.learning || r.petStudyCooldown || b.size()!=46 || b[0]!='S' || b[1]!='A' || b[2]!='S' || b[3]!=1 || b[4]<1 || b[4]>4 || b[36]>1 || b[37]!=3)return false;
    const auto first=ItemInstanceId::Create(Get(b,5,8)),second=ItemInstanceId::Create(Get(b,13,8));if(!first || !second)return false;
    AbsorptionSashActor actor;actor.admitted=actor.alive=true;actor.equipment.level=static_cast<std::uint16_t>(Get(b,32,2));actor.equipment.characterClass=b[34];actor.equipment.sex=b[35];actor.equipment.inCombat=b[36];actor.equipment.nowUtcMs=Get(b,38,8);
    AbsorptionSashDecision d{static_cast<std::uint16_t>(Get(b,29,2)),b[31]};if(const auto id=Get(b,21,8)){const auto value=ItemInstanceId::Create(id);if(!value)return false;d.newInstance=*value;}
    std::vector<PersistentItem> before;for(const auto& change:r.items)if(change.before)before.push_back(*change.before);
    if(before.size()!=2)return false;std::vector<ItemWallet> wallets;for(const auto& w:r.wallets)wallets.push_back(w.before);
    const auto mode=static_cast<AbsorptionSashOperation>(b[4]);const auto q=PrepareAbsorptionSash(c,r.owners[0].owner,before,wallets,*first,*second,mode,actor);if(!q)return false;
    const auto expected=Build(c,r.operationId,r.owners[0],r.storages,before,wallets,*q,actor,d);if(!expected || expected->items.size()!=r.items.size() || expected->wallets.size()!=r.wallets.size() || expected->result!=r.result)return false;
    for(const auto& change:r.items){const auto match=std::ranges::find_if(expected->items,[&](const auto& v){return v.after.id==change.after.id;});if(match==expected->items.end() || match->before!=change.before || match->after!=change.after)return false;}
    for(unsigned i=0;i<r.wallets.size();++i)if(r.wallets[i].before!=expected->wallets[i].before || r.wallets[i].after!=expected->wallets[i].after)return false;
    return true;
}
}
