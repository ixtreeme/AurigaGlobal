#include <item/PersistentItems.hpp>
#include <algorithm>
#include <map>
#include <set>

namespace auriga::item {
namespace {
using E=ItemPersistenceError;
std::optional<CostumeClockDefinition> Clock(const ItemCatalog& catalog,ItemKey key,unsigned mode)
{return mode==3 ? FindPersistentItemClock(catalog,key) : FindWearableClock(catalog,key);}
struct Proof {unsigned mode=0;std::uint64_t main=0,now=0;EquipContext who;std::optional<WeaponSubtype> weapon;std::map<std::uint64_t,std::uint64_t> elapsed;};
void Put(std::vector<std::uint8_t>& b,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
std::optional<Proof> Read(const ItemCommit& c) {
    const auto& b=c.result;if(b.size()<28 || b[0]!='C' || b[1]!='T' || b[2]!='M' || b[3]!=1 || b[4]<1 || b[4]>3 || b[25]>1 || b[26]>7 || !b[27] || b[27]>2 || b.size()!=28+16u*b[27])return {};
    const auto get=[&](unsigned at,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t{b[at+i]}<<(8*i);return v;};
    Proof p;p.mode=b[4];p.main=get(5,8);p.now=get(13,8);p.who.level=static_cast<std::uint16_t>(get(21,2));p.who.characterClass=b[23];p.who.sex=b[24];p.who.inCombat=b[25];if(b[26])p.weapon=static_cast<WeaponSubtype>(b[26]);
    for(unsigned i=0;i<b[27];++i)if(!p.elapsed.emplace(get(28+i*16,8),get(36+i*16,8)).second)return {};
    if(!p.elapsed.contains(p.main))return {};return p;
}
bool AccessoryDebit(const ItemChange& ch) {
    if(!ch.before || !ch.before->accessory || !ch.after.accessory || !ch.before->placement || ch.before->placement->kind!=ItemStorageKind::Equipment)return false;
    const auto& b=*ch.before->accessory;const auto& a=*ch.after.accessory;
    if(b.permanent || a.permanent || !b.activeGrade || a.unlockedCapacity!=b.unlockedCapacity || a.activeGrade>b.activeGrade || (a.activeGrade==b.activeGrade && a.remainingMs>=b.remainingMs))return false;
    auto expected=*ch.before;expected.revision=ch.after.revision;expected.accessory=ch.after.accessory;return expected==ch.after;
}
std::expected<PersistentItem,E> Time(const ItemCatalog& c,const PersistentItem& before,const Proof& p) {
    const auto found=p.elapsed.find(before.id.Value());const auto row=Clock(c,before.key,p.mode);
    if(!row || found==p.elapsed.end() || (found->second && row->clock!=CostumeClock::EquippedTime))return std::unexpected(E::InvalidChange);
    if(!before.costume){if(row->clock!=CostumeClock::Permanent || (before.key.Category()!=ItemCategory::Regalia && before.key.Category()!=ItemCategory::Accessory))return std::unexpected(E::InvalidItem);auto after=before;++after.revision;return after;}
    const bool worn=before.placement && before.placement->kind==ItemStorageKind::Equipment;
    const auto plan=AdvanceItemClock(*row,*before.costume,{p.now,found->second,worn || found->second!=0,p.mode==1 && p.main==before.id.Value()});
    if(!plan)return std::unexpected(E::InvalidItem);
    auto after=before;++after.revision;after.costume=plan->after;
    if(plan->removeItem){after.destroyed=true;after.count=0;after.placement.reset();after.privateShop.reset();}return after;
}
std::uint32_t WearSlot(const ItemCatalog& c,ItemKey key) {
    const auto& category=c.Find(key)->category;
    if(const auto* row=std::get_if<ItemRegaliaRow>(&category))return RegaliaSlot(*row);
    if(const auto* row=std::get_if<ItemAccessoryRow>(&category))return static_cast<unsigned>(row->slot);
    return CostumeSlot(std::get<ItemCostumeRow>(category).kind);
}
bool CanEquip(const ItemCatalog& c,const PersistentItem& item,const Proof& p) {
    if(item.key.Category()==ItemCategory::Regalia || item.key.Category()==ItemCategory::Accessory)return p.who.level>=c.LevelPolicy(item.key,item.refinementLevel).minimumLevel;
    return PlanCostumeEquip(c,{item.key,item.count,p.who.level,item.refinementLevel,static_cast<std::uint8_t>(p.who.characterClass+1),static_cast<std::uint8_t>(p.who.sex+1),p.weapon,*item.costume,{p.now,0,false,true},true,p.who.inCombat}).has_value();
}
bool AdmittedUse(const ItemCatalog& c,const PersistentItem& item,const Proof& p) {
    const auto* entry=c.Find(item.key);if(!entry || entry->base.retired || p.who.characterClass>3 || p.who.sex>1 || p.who.level<c.LevelPolicy(item.key,item.refinementLevel).minimumLevel)return false;
    if(!entry->base.usePolicy)return true;
    const auto* policy=c.FindUsePolicy(entry->base.usePolicy);
    return policy && (policy->allowedClasses&(1u<<p.who.characterClass)) && (policy->allowedSexes&(1u<<p.who.sex)) && (item.key.Category()==ItemCategory::Accessory || !p.who.inCombat || policy->useDuringCombat);
}
std::expected<ItemCommit,E> Finish(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,Proof p,std::vector<ItemChange> changes) {
    ItemCommit r;r.operationId=op;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Service;r.owners={owner};r.items=std::move(changes);r.outcomeCode=CostumeOutcome;
    r.result={'C','T','M',1,static_cast<std::uint8_t>(p.mode)};Put(r.result,p.main,8);Put(r.result,p.now,8);Put(r.result,p.who.level,2);Put(r.result,p.who.characterClass,1);Put(r.result,p.who.sex,1);Put(r.result,p.who.inCombat,1);Put(r.result,p.weapon ? static_cast<unsigned>(*p.weapon) : 0,1);Put(r.result,p.elapsed.size(),1);
    for(const auto [id,elapsed]:p.elapsed){Put(r.result,id,8);Put(r.result,elapsed,8);}
    for(const auto& l:layouts)for(const auto& ch:r.items)for(const auto* item:{&*ch.before,&ch.after})
        if(item->placement && l.storage==ItemStorage{owner.owner,item->placement->kind,item->placement->storageId} && std::ranges::find(r.storages,l)==r.storages.end())r.storages.push_back(l);
    // Swapping must validate occupied cells against the complete owner state.
    std::set<std::pair<ItemStorage,std::uint32_t>> occupied;
    for(const auto& item:inventory) {
        const auto ch=std::ranges::find(r.items,item.id,[](const auto& x){return x.after.id;});const auto& after=ch==r.items.end() ? item : ch->after;
        if(after.destroyed || !after.placement)continue;
        const auto l=std::ranges::find(r.storages,ItemStorage{owner.owner,after.placement->kind,after.placement->storageId},&ItemStorageLayout::storage);if(l==r.storages.end())continue;
        const auto cells=OccupiedItemCells(c,after,*l);if(!cells)return std::unexpected(cells.error());for(auto cell:*cells)if(!occupied.emplace(l->storage,cell).second)return std::unexpected(E::InvalidPlacement);
    }
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());return r;
}
}
std::uint32_t CostumeSlot(CostumeKind kind){return kind>=CostumeKind::PetSkin ? static_cast<unsigned>(kind)+21 : static_cast<unsigned>(kind)+8;}
std::uint16_t WornWeaponCostumeEffect(const ItemCatalog& c,std::span<const PersistentItem> items)
{
    const PersistentItem* weapon=nullptr;const ItemCostumeRow* effect=nullptr;
    for(const auto& p:items)if(!p.destroyed && p.placement && p.placement->kind==ItemStorageKind::Equipment) {
        const auto* e=c.Find(p.key);if(!e || e->base.retired)continue;
        if(p.placement->anchor==0 && std::holds_alternative<ItemWeaponRow>(e->category))weapon=&p;
        if(const auto* row=std::get_if<ItemCostumeRow>(&e->category);row && row->kind==CostumeKind::WeaponEffect && p.costume && !p.costume->expired &&
            p.placement->anchor==CostumeSlot(row->kind)-1 && ValidCostumeLifetime(*row,*p.costume,true))effect=row;
    }
    if(!weapon || !effect)return 0;
    const auto subtype=std::get<ItemWeaponRow>(c.Find(weapon->key)->category).subtype;
    if(subtype<WeaponSubtype::Sword || subtype>WeaponSubtype::Fan)return 0;
    if(const auto* rule=c.FindDefinitionRules(weapon->key,weapon->refinementLevel);rule && rule->weaponEffectAlternate) {
        if(subtype==WeaponSubtype::Dagger)return effect->weaponEffects[6];
        if(subtype==WeaponSubtype::Fan)return effect->weaponEffects[7];
    }
    return effect->weaponEffects[static_cast<unsigned>(subtype)-1];
}
std::uint16_t WornBodyCostumeEffect(const ItemCatalog& c,std::span<const PersistentItem> items)
{
    bool body=false;std::uint16_t effect=0;
    for(const auto& p:items)if(!p.destroyed && p.placement && p.placement->kind==ItemStorageKind::Equipment) {
        const auto* e=c.Find(p.key);if(!e || e->base.retired)continue;
        if(const auto* armor=std::get_if<ItemArmorRow>(&e->category);armor && armor->slot==EquipmentSlot::Body && p.placement->anchor==1)body=true;
        if(const auto* row=std::get_if<ItemCostumeRow>(&e->category);row && p.costume && !p.costume->expired &&
            ValidCostumeLifetime(*row,*p.costume,true) && p.placement->anchor==CostumeSlot(row->kind)-1) {
            if(row->kind==CostumeKind::Body)body=true;
            if(row->kind==CostumeKind::BodyEffect)effect=row->bodyEffect;
        }
    }
    return body ? effect : 0;
}
std::optional<std::uint32_t> WornCompanionCostumeForm(const ItemCatalog& c,std::span<const PersistentItem> items,CostumeKind kind)
{
    if(kind!=CostumeKind::PetSkin && kind!=CostumeKind::MountSkin)return {};
    for(const auto& p:items)if(!p.destroyed && p.costume && !p.costume->expired && p.placement &&
        p.placement->kind==ItemStorageKind::Equipment && p.placement->anchor==CostumeSlot(kind)-1) {
        const auto* e=c.Find(p.key);const auto* row=e && !e->base.retired ? std::get_if<ItemCostumeRow>(&e->category) : nullptr;
        if(row && row->kind==kind && ValidCostumeLifetime(*row,*p.costume,true))return row->companionForm;
    }
    return {};
}
bool ValidCostumeWrite(const ItemCatalog& c,const ItemCommit& commit) {
    if(commit.outcomeCode!=CostumeOutcome || commit.kind!=ItemOperationKind::Service || commit.owners.size()!=1 || !commit.wallets.empty() || commit.ground || commit.recovery || commit.learning || commit.petStudyCooldown)return false;
    const auto proof=Read(commit);if(!proof)return false;const auto& p=*proof;
    const auto main=std::ranges::find(commit.items,p.main,[](const auto& ch){return ch.after.id.Value();});if(main==commit.items.end() || !main->before || !main->before->placement)return false;
    const auto& before=*main->before;const auto* entry=c.Find(before.key);const auto definition=Clock(c,before.key,p.mode);if(!entry || !definition)return false;
    auto slot=p.mode==1 ? WearSlot(c,before.key) : 0;
    if((p.mode==1 && before.placement->kind!=ItemStorageKind::Inventory) || (p.mode==2 && before.placement->kind!=ItemStorageKind::Equipment) || (p.mode==3 && p.elapsed.size()!=1))return false;
    if(p.mode==1 && !AdmittedUse(c,before,p))return false;
    const auto mainTimed=Time(c,before,p);if(!mainTimed)return false;
    const bool equipped=p.mode==1 && !mainTimed->destroyed && CanEquip(c,*mainTimed,p);
    if(equipped && slot==static_cast<unsigned>(EquipmentSlot::Unique)) {
        if(!main->after.placement || main->after.placement->kind!=ItemStorageKind::Equipment || !FitsEquipmentSlot(EquipmentSlot::Unique,main->after.placement->anchor))return false;
        slot=main->after.placement->anchor+1;
    }
    std::size_t seen=0;
    for(const auto& change:commit.items) {
        if(!change.before || !Clock(c,change.before->key,p.mode)) {if(!AccessoryDebit(change))return false;continue;}
        ++seen;auto expected=Time(c,*change.before,p);if(!expected)return false;
        if(!expected->destroyed) {
            if(change.after.id.Value()==p.main) {
                if(equipped) {expected->placement=ItemPlacement{ItemStorageKind::Equipment,1,slot-1};if(c.Find(expected->key)->base.binding==BindingPolicy::OnEquip && !expected->boundTo)expected->boundTo=expected->owner;}
                else if(p.mode==2) {if(!change.after.placement || change.after.placement->kind!=ItemStorageKind::Inventory)return false;expected->placement=change.after.placement;}
            } else {
                if(!equipped || !change.before->placement || change.before->placement->kind!=ItemStorageKind::Equipment || change.before->placement->anchor!=slot-1)return false;
                expected->placement=before.placement;
            }
        }
        if(*expected!=change.after)return false;
    }
    return seen==p.elapsed.size();
}
std::expected<ItemCommit,E> MakeCostumeWearCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,ItemInstanceId id,ItemPlacement target,EquipContext who,bool equip) {
    const auto source=std::ranges::find(inventory,id,&PersistentItem::id);
    if(source==inventory.end() || source->destroyed || !FindWearableClock(c,source->key) || !source->placement || source->owner!=owner.owner || (source->boundTo && source->boundTo!=owner.owner))return std::unexpected(E::InvalidItem);
    if(equip ? source->placement->kind!=ItemStorageKind::Inventory : source->placement->kind!=ItemStorageKind::Equipment || target.kind!=ItemStorageKind::Inventory)return std::unexpected(E::InvalidPlacement);
    Proof p;p.mode=equip ? 1 : 2;p.main=id.Value();p.now=who.nowUtcMs;p.who=who;
    if(equip && !AdmittedUse(c,*source,p))return std::unexpected(E::RequirementNotMet);
    for(const auto& item:inventory)if(!item.destroyed && item.placement && item.placement->kind==ItemStorageKind::Equipment && item.placement->anchor==0)
        if(const auto* weapon=std::get_if<ItemWeaponRow>(&c.Find(item.key)->category))p.weapon=weapon->subtype;
    const auto debt=[&](std::uint64_t i){const auto d=std::ranges::find(who.costumeElapsed,i,&CostumeElapsed::instance);return d==who.costumeElapsed.end() ? 0 : d->elapsedMs;};
    p.elapsed.emplace(id.Value(),debt(id.Value()));auto after=Time(c,*source,p);if(!after)return std::unexpected(after.error());
    const bool allowed=equip && !after->destroyed && CanEquip(c,*after,p);
    auto slot=WearSlot(c,source->key);
    if(allowed && slot==static_cast<unsigned>(EquipmentSlot::Unique)) {
        const auto worn=[&](unsigned s){return std::ranges::any_of(inventory,[&](const auto& i){return !i.destroyed && i.placement==ItemPlacement{ItemStorageKind::Equipment,1,s-1};});};
        if(worn(slot) && !worn(static_cast<unsigned>(EquipmentSlot::UniqueAlternate)))slot=static_cast<unsigned>(EquipmentSlot::UniqueAlternate);
    }
    if(!after->destroyed) {
        if(allowed) {after->placement=ItemPlacement{ItemStorageKind::Equipment,1,slot-1};if(c.Find(after->key)->base.binding==BindingPolicy::OnEquip && !after->boundTo)after->boundTo=owner.owner;}
        else if(!equip)after->placement=target;
    }
    std::vector<ItemChange> changes{{*source,*after}};
    if(allowed)for(const auto& item:inventory)if(!item.destroyed && item.id!=id && item.placement==after->placement) {
        if(!FindWearableClock(c,item.key))return std::unexpected(E::InvalidItem);p.elapsed.emplace(item.id.Value(),debt(item.id.Value()));auto displaced=Time(c,item,p);if(!displaced)return std::unexpected(displaced.error());if(!displaced->destroyed)displaced->placement=source->placement;changes.push_back({item,*displaced});
    }
    if(!allowed && equip && after->costume==source->costume && !after->destroyed)return std::unexpected(E::RequirementNotMet);
    return Finish(c,op,owner,layouts,inventory,std::move(p),std::move(changes));
}
std::expected<ItemCommit,E> MakeCostumeClockCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> inventory,ItemInstanceId id,std::uint64_t now,std::uint64_t elapsed) {
    const auto source=std::ranges::find(inventory,id,&PersistentItem::id);
    if(source==inventory.end() || !source->costume || source->destroyed || !source->placement || source->owner!=owner.owner)return std::unexpected(E::InvalidItem);
    Proof p;p.mode=3;p.main=id.Value();p.now=now;p.elapsed.emplace(id.Value(),elapsed);const auto after=Time(c,*source,p);if(!after)return std::unexpected(after.error());
    auto unchanged=*after;unchanged.revision=source->revision;if(unchanged==*source)return std::unexpected(E::InvalidChange);
    return Finish(c,op,owner,layouts,inventory,std::move(p),{{*source,*after}});
}
}
