#include <item/AmmoRuntime.hpp>
#include <algorithm>
#include <tuple>

namespace auriga::item {
namespace {
using E=ItemPersistenceError;
constexpr ItemPlacement BowPlace{ItemStorageKind::Equipment,1,0};
constexpr ItemPlacement AmmoPlace{ItemStorageKind::Equipment,1,19};
void Put(std::vector<std::uint8_t>& b,std::uint64_t v,unsigned n){for(unsigned j=0;j<n;++j)b.push_back(static_cast<std::uint8_t>(v>>(j*8)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned p,unsigned n=8){std::uint64_t v=0;for(unsigned j=0;j<n;++j)v|=std::uint64_t{b[p+j]}<<(j*8);return v;}
const PersistentItem* Find(std::span<const PersistentItem> all,std::uint64_t id){const auto i=std::ranges::find(all,id,[](const auto& p){return p.id.Value();});return i==all.end()?nullptr:&*i;}
struct Decision {std::uint64_t bow=0,ammo=0,reserve=0;SoulContext context;std::int64_t output=0;std::uint16_t units=0;std::vector<std::uint8_t> soul;};
std::vector<std::uint8_t> Encode(const Decision& d){std::vector<std::uint8_t> b{'A','M','R',1};for(auto v:{d.bow,d.ammo,d.reserve,d.context.event,d.context.target,d.context.targetRevision,std::uint64_t(d.context.damage),std::uint64_t(d.output)})Put(b,v,8);Put(b,d.units,2);Put(b,d.context.actor.level,2);Put(b,d.context.actor.characterClass,1);Put(b,d.context.actor.sex,1);Put(b,d.context.actor.inCombat,1);Put(b,d.context.targetIsNpc,1);Put(b,d.context.alive,1);Put(b,d.context.polymorphed,1);Put(b,unsigned(d.context.channel),1);Put(b,d.soul.size(),1);b.insert(b.end(),d.soul.begin(),d.soul.end());return b;}
std::optional<Decision> Read(std::span<const std::uint8_t> b){
    if(b.size()<80 || b[0]!='A' || b[1]!='M' || b[2]!='R' || b[3]!=1 || (b[79]!=0 && b[79]!=84) || b.size()!=80+b[79] || b[74]>1 || b[75]>1 || b[76]>1 || b[77]>1 || b[78]<1 || b[78]>2)return {};
    Decision d;d.bow=Get(b,4);d.ammo=Get(b,12);d.reserve=Get(b,20);d.context.event=Get(b,28);d.context.target=Get(b,36);d.context.targetRevision=Get(b,44);d.context.damage=static_cast<std::int64_t>(Get(b,52));d.output=static_cast<std::int64_t>(Get(b,60));d.units=Get(b,68,2);d.context.actor.level=Get(b,70,2);d.context.actor.characterClass=b[72];d.context.actor.sex=b[73];d.context.actor.inCombat=b[74];d.context.targetIsNpc=b[75];d.context.alive=b[76];d.context.polymorphed=b[77];d.context.channel=static_cast<SoulChannel>(b[78]);d.soul.assign(b.begin()+80,b.end());return d;
}
bool Allowed(const ItemCatalog& c,const PersistentItem& p,const SoulContext& ctx){const auto* e=c.Find(p.key);const auto* u=e?c.FindUsePolicy(e->base.usePolicy):nullptr;return ValidatePersistentItem(c,p).has_value() && !p.destroyed && !e->base.retired && e->base.minimumLevel<=ctx.actor.level && u && ctx.actor.characterClass<4 && ctx.actor.sex<2 && (u->allowedClasses>>ctx.actor.characterClass&1) && (u->allowedSexes>>ctx.actor.sex&1) && (!ctx.actor.inCombat || u->useDuringCombat);}
std::expected<ItemCommit,E> Plan(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence f,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> all,const Decision& d){
    if(!d.context.alive || d.context.polymorphed || d.context.actor.characterClass!=1 || !d.context.event || !d.context.target || !d.context.targetRevision || d.context.damage<0 || d.output<0)return std::unexpected(E::RequirementNotMet);
    const auto view=FindBowAmmo(c,f.owner,all);if(!view || view->bow.Value()!=d.bow || view->ammo.Value()!=d.ammo || view->units!=d.units)return std::unexpected(E::RequirementNotMet);
    const auto* bow=Find(all,d.bow);const auto* ammo=Find(all,d.ammo);if(!Allowed(c,*bow,d.context) || !Allowed(c,*ammo,d.context))return std::unexpected(E::RequirementNotMet);
    ItemCommit out;out.operationId=op;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={f};out.outcomeCode=AmmoShotOutcome;out.result=Encode(d);
    auto touched=*bow;++touched.revision;out.items.push_back({*bow,touched});
    auto spent=*ammo;++spent.revision;spent.count-=d.units;if(!spent.count){spent.destroyed=true;spent.placement.reset();}out.items.push_back({*ammo,spent});
    if(d.reserve){const auto* reserve=Find(all,d.reserve);if(spent.count || !reserve || reserve->key!=ammo->key || !PlainInventoryItem(*reserve,f.owner) || !Allowed(c,*reserve,d.context))return std::unexpected(E::InvalidChange);auto worn=*reserve;++worn.revision;worn.placement=AmmoPlace;out.items.push_back({*reserve,worn});}
    if(!d.soul.empty()){
        const auto& b=d.soul;if(b[0]!='S' || b[1]!='L' || b[2]!='R' || b[3]!=1 || b[4]!=4)return std::unexpected(E::InvalidChange);
        const auto core=ItemInstanceId::Create(Get(b,5));if(!core)return std::unexpected(E::InvalidChange);
        const auto s=MakeSoulCommit(c,op,f,layouts,all,*core,{},SoulAction::Strike,Get(b,21),0,d.context);
        if(!s || s->commit.result!=b || s->damage!=d.output)return std::unexpected(E::InvalidChange);out.items.insert(out.items.end(),s->commit.items.begin(),s->commit.items.end());
    }else if(d.output!=d.context.damage)return std::unexpected(E::InvalidChange);
    for(const auto& ch:out.items)for(const auto* p:{&*ch.before,&ch.after})if(p->placement){const auto i=std::ranges::find(layouts,ItemStorage{p->owner,p->placement->kind,p->placement->storageId},&ItemStorageLayout::storage);if(i==layouts.end())return std::unexpected(E::InvalidStorage);if(std::ranges::find(out.storages,*i)==out.storages.end())out.storages.push_back(*i);}
    return out;
}
bool AccessoryDebit(const ItemChange& ch){if(!ch.before || !ch.before->accessory || !ch.after.accessory || !ch.before->placement || ch.before->placement->kind!=ItemStorageKind::Equipment)return false;const auto& b=*ch.before->accessory;const auto& a=*ch.after.accessory;if(b.permanent || a.permanent || !b.activeGrade || a.unlockedCapacity!=b.unlockedCapacity || a.activeGrade>b.activeGrade || (a.activeGrade==b.activeGrade && a.remainingMs>=b.remainingMs))return false;auto p=*ch.before;p.revision=ch.after.revision;p.accessory=a;return p==ch.after;}
}
std::optional<BowAmmoView> FindBowAmmo(const ItemCatalog& c,ItemOwner owner,std::span<const PersistentItem> all){
    const PersistentItem* bow=nullptr;const PersistentItem* ammo=nullptr;
    for(const auto& p:all)if(!p.destroyed && p.placement && p.owner==owner){if(*p.placement==BowPlace){if(bow)return {};bow=&p;}if(*p.placement==AmmoPlace){if(ammo)return {};ammo=&p;}}
    if(!bow || !ammo || !ValidatePersistentItem(c,*bow) || !ValidatePersistentItem(c,*ammo))return {};
    const auto* w=std::get_if<ItemWeaponRow>(&c.Find(bow->key)->category);const auto* a=std::get_if<ItemAmmoRow>(&c.Find(ammo->key)->category);if(!w || w->subtype!=WeaponSubtype::Bow || !a || a->ammoPolicy!=w->ammoPolicy)return {};const auto* policy=c.FindAmmoPolicy(w->ammoPolicy);if(!policy || policy->kind!=AmmoKind::ConsumeStack || !policy->unitsPerHit || ammo->count<policy->unitsPerHit)return {};
    return BowAmmoView{bow->id,ammo->id,ammo->count,w->ammoPolicy,policy->unitsPerHit};
}
std::expected<SoulCommit,E> MakeAmmoShotCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> all,SoulContext context,std::optional<ItemInstanceId> soul,std::uint64_t elapsed){
    const auto v=FindBowAmmo(c,fence.owner,all);if(!v)return std::unexpected(E::RequirementNotMet);Decision d{v->bow.Value(),v->ammo.Value(),0,context,context.damage,v->units,{}};
    const auto* ammo=Find(all,d.ammo);if(v->count==v->units){const PersistentItem* next=nullptr;for(const auto& p:all)if(p.key==ammo->key && p.id!=ammo->id && PlainInventoryItem(p,fence.owner) && Allowed(c,p,context) && (!next || std::tuple{p.placement->storageId,p.placement->anchor,p.id}<std::tuple{next->placement->storageId,next->placement->anchor,next->id}))next=&p;if(next)d.reserve=next->id.Value();}
    if(soul){const auto s=MakeSoulCommit(c,op,fence,layouts,all,*soul,{},SoulAction::Strike,elapsed,0,context);if(!s)return std::unexpected(s.error());d.soul=s->commit.result;d.output=s->damage;}
    const auto planned=Plan(c,op,fence,layouts,all,d);if(!planned)return std::unexpected(planned.error());if(const auto valid=ValidateItemCommit(c,*planned);!valid)return std::unexpected(valid.error());return SoulCommit{*planned,d.output,true};
}
bool ValidAmmoShotWrite(const ItemCatalog& c,const ItemCommit& r){if(r.kind!=ItemOperationKind::Service || r.owners.size()!=1 || !r.wallets.empty() || r.ground || r.recovery || r.learning || r.petStudyCooldown)return false;const auto d=Read(r.result);if(!d)return false;std::vector<PersistentItem> before;for(const auto& ch:r.items){if(!ch.before)return false;before.push_back(*ch.before);}const auto expected=Plan(c,r.operationId,r.owners[0],r.storages,before,*d);if(!expected)return false;unsigned matched=0;for(const auto& ch:r.items){const auto i=std::ranges::find(expected->items,ch.after.id,[](const auto& v){return v.after.id;});if(i==expected->items.end()){if(!AccessoryDebit(ch))return false;}else{if(ch.before!=i->before || ch.after!=i->after)return false;++matched;}}return matched==expected->items.size();}
std::span<const std::uint8_t> AmmoSoulReceipt(const ItemCommit& r){if(r.outcomeCode!=AmmoShotOutcome || !Read(r.result) || r.result[79]!=84)return {};return std::span<const std::uint8_t>(r.result).subspan(80);}
}
