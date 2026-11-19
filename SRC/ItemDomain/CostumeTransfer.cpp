#include <item/CostumeTransfer.hpp>
#include <algorithm>
namespace auriga::item {
namespace {
using E=ItemPersistenceError;
void Put(std::vector<std::uint8_t>& b,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(i*8)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned at,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(b[at+i])<<(8*i);return v;}
const PersistentItem* Find(std::span<const PersistentItem> items,ItemInstanceId id){const auto i=std::ranges::find(items,id,&PersistentItem::id);return i==items.end() ? nullptr : &*i;}
bool Bag(const ItemCatalog& c,const PersistentItem* p,ItemOwner owner){return p && p->owner==owner && !p->destroyed && p->placement && p->placement->kind==ItemStorageKind::Inventory && (!p->boundTo || p->boundTo==owner) && ValidatePersistentItem(c,*p).has_value();}
std::uint8_t Kind(const ItemCatalog& c,ItemKey key){
    if(!c.CostumeTransferAllowed(key))return 0;const auto* e=c.Find(key);if(!e)return 0;
    if(const auto* row=std::get_if<ItemCostumeRow>(&e->category))return row->kind<=CostumeKind::Weapon ? static_cast<std::uint8_t>(row->kind) : 0;
    if(const auto* row=std::get_if<ItemRegaliaRow>(&e->category);row && row->kind==RegaliaKind::Stole)return 4;return 0;
}
bool Live(const ItemCatalog& c,const PersistentItem& p,std::uint64_t now){if(!p.costume)return true;const auto clock=FindWearableClock(c,p.key);if(!clock)return false;const auto state=AdvanceItemClock(*clock,*p.costume,{now,0,false,false});return state && !state->removeItem;}
std::vector<std::uint8_t> Receipt(const CostumeTransferQuote& q,const CostumeTransferActor& actor){std::vector<std::uint8_t> b{'C','T','F',1};Put(b,q.scroll,8);Put(b,q.receiver,8);Put(b,q.donor,8);Put(b,actor.equipment.level,2);Put(b,actor.equipment.characterClass,1);Put(b,actor.equipment.sex,1);Put(b,actor.equipment.nowUtcMs,8);Put(b,actor.equipment.inCombat,1);return b;}
std::expected<std::vector<ItemChange>,E> Changes(const ItemCatalog& c,ItemOwner owner,std::span<const PersistentItem> items,const CostumeTransferQuote& q){
    const auto a=ItemInstanceId::Create(q.scroll),b=ItemInstanceId::Create(q.receiver),d=ItemInstanceId::Create(q.donor);if(!a || !b || !d)return std::unexpected(E::InvalidItem);
    const auto* scroll=Find(items,*a);const auto* target=Find(items,*b);const auto* donor=Find(items,*d);if(!scroll || !target || !donor)return std::unexpected(E::InvalidItem);
    for(const auto* p:{scroll,target,donor})if(p->revision==UINT64_MAX || p->owner!=owner)return std::unexpected(E::InvalidChange);
    auto sourceAfter=*scroll,receiverAfter=*target,donorAfter=*donor;++sourceAfter.revision;++receiverAfter.revision;++donorAfter.revision;
    if(!--sourceAfter.count){sourceAfter.destroyed=true;sourceAfter.placement.reset();}
    donorAfter.count=0;donorAfter.destroyed=true;donorAfter.placement.reset();receiverAfter.attributes=q.attributes;if(q.stole)receiverAfter.regalia=q.stole;
    return std::vector<ItemChange>{{*scroll,sourceAfter},{*target,receiverAfter},{*donor,donorAfter}};
}
bool AccessoryDebit(const ItemChange& ch){if(!ch.before || !ch.before->accessory || !ch.after.accessory || !ch.before->placement || ch.before->placement->kind!=ItemStorageKind::Equipment)return false;const auto& b=*ch.before->accessory;const auto& a=*ch.after.accessory;if(b.permanent || a.permanent || !b.activeGrade || a.unlockedCapacity!=b.unlockedCapacity || a.activeGrade>b.activeGrade || (a.activeGrade==b.activeGrade && a.remainingMs>=b.remainingMs))return false;auto p=*ch.before;p.revision=ch.after.revision;p.accessory=a;return p==ch.after;}
}
std::expected<CostumeTransferQuote,E> PrepareCostumeTransfer(const ItemCatalog& c,ItemOwner owner,std::span<const PersistentItem> items,ItemInstanceId scrollId,ItemInstanceId receiverId,ItemInstanceId donorId,const CostumeTransferActor& actor){
    if(owner.kind!=ItemOwnerKind::Character || !ValidItemOwner(owner) || !actor.admitted || !actor.alive || !actor.nearNpc || !actor.equipment.nowUtcMs || actor.equipment.level>255 || actor.equipment.characterClass>3 || actor.equipment.sex>1 || scrollId==receiverId || scrollId==donorId || receiverId==donorId)return std::unexpected(E::RequirementNotMet);
    const auto* scroll=Find(items,scrollId);const auto* receiver=Find(items,receiverId);const auto* donor=Find(items,donorId);
    if(!Bag(c,scroll,owner) || !Bag(c,receiver,owner) || !Bag(c,donor,owner) || receiver->count!=1 || donor->count!=1 || !PlainInventoryItem(*scroll,owner))return std::unexpected(E::InvalidItem);
    const auto* entry=c.Find(scroll->key);const auto* tool=std::get_if<ItemBonusToolRow>(&entry->category);const auto* policy=c.FindUsePolicy(entry->base.usePolicy);
    if(!tool || tool->operation!=SpecialBonusOperation::CostumeTransfer || tool->units!=1 || !policy || !(policy->allowedClasses&(1u<<actor.equipment.characterClass)) || !(policy->allowedSexes&(1u<<actor.equipment.sex)) || actor.equipment.level<c.LevelPolicy(scroll->key,scroll->refinementLevel).minimumLevel || (actor.equipment.inCombat && !policy->useDuringCombat))return std::unexpected(E::RequirementNotMet);
    const auto kind=Kind(c,receiver->key);if(!kind || kind!=Kind(c,donor->key) || !Live(c,*receiver,actor.equipment.nowUtcMs) || !Live(c,*donor,actor.equipment.nowUtcMs))return std::unexpected(E::RequirementNotMet);
    CostumeTransferQuote q{scrollId.Value(),receiverId.Value(),donorId.Value(),kind};q.attributes.slots=donor->attributes.slots;q.attributes.lockedSlot=receiver->attributes.lockedSlot;q.attributes.slots[5].reset();q.attributes.slots[6].reset();
    if(kind==4){
        if(!receiver->regalia || !donor->regalia)return std::unexpected(E::InvalidItem);
        q.stole=receiver->regalia;for(unsigned i=0;i<5;++i)q.stole->bonuses[i]=donor->regalia->bonuses[i];q.stole->bonuses[5].reset();
        if(!ValidRegaliaState(std::get<ItemRegaliaRow>(c.Find(receiver->key)->category),*q.stole))return std::unexpected(E::InvalidItem);
    }
    const auto positive=[](const auto& entry){return entry && entry->valueScaled>0;};
    if(!std::ranges::any_of(q.attributes.slots,positive) && !(q.stole && std::ranges::any_of(q.stole->bonuses,positive)))return std::unexpected(E::RequirementNotMet);
    if(!ValidateInstanceAttributes(c,receiver->key,q.attributes))return std::unexpected(E::InvalidAttributes);return q;
}
std::expected<ItemCommit,E> MakeCostumeTransferCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ItemInstanceId scroll,ItemInstanceId receiver,ItemInstanceId donor,const CostumeTransferActor& actor){
    const auto q=PrepareCostumeTransfer(c,fence.owner,items,scroll,receiver,donor,actor);if(!q)return std::unexpected(q.error());const auto changes=Changes(c,fence.owner,items,*q);if(!changes)return std::unexpected(changes.error());
    ItemCommit out;out.operationId=operation;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Service;out.owners={fence};out.outcomeCode=CostumeTransferOutcome;out.items=*changes;out.result=Receipt(*q,actor);
    for(const auto& change:out.items)for(const auto* p:{&*change.before,&change.after})if(p->placement){const ItemStorage storage{p->owner,p->placement->kind,p->placement->storageId};if(std::ranges::find(out.storages,storage,&ItemStorageLayout::storage)!=out.storages.end())continue;const auto layout=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);if(layout==layouts.end())return std::unexpected(E::InvalidStorage);out.storages.push_back(*layout);}
    if(const auto valid=ValidateItemCommit(c,out);!valid)return std::unexpected(valid.error());return out;
}
bool ValidCostumeTransferWrite(const ItemCatalog& c,const ItemCommit& out){
    const auto& b=out.result;if(out.outcomeCode!=CostumeTransferOutcome || out.kind!=ItemOperationKind::Service || out.owners.size()!=1 || !out.wallets.empty() || out.ground || out.recovery || out.learning || out.petStudyCooldown || b.size()!=41 || b[0]!='C' || b[1]!='T' || b[2]!='F' || b[3]!=1 || b[40]>1)return false;
    const auto scroll=ItemInstanceId::Create(Get(b,4,8)),receiver=ItemInstanceId::Create(Get(b,12,8)),donor=ItemInstanceId::Create(Get(b,20,8));if(!scroll || !receiver || !donor)return false;
    CostumeTransferActor actor;actor.admitted=actor.alive=actor.nearNpc=true;actor.equipment.level=static_cast<std::uint16_t>(Get(b,28,2));actor.equipment.characterClass=b[30];actor.equipment.sex=b[31];actor.equipment.nowUtcMs=Get(b,32,8);actor.equipment.inCombat=b[40];
    std::vector<PersistentItem> before;for(const auto& change:out.items){if(!change.before)return false;before.push_back(*change.before);}
    const auto q=PrepareCostumeTransfer(c,out.owners[0].owner,before,*scroll,*receiver,*donor,actor);if(!q || Receipt(*q,actor)!=b)return false;const auto expected=Changes(c,out.owners[0].owner,before,*q);if(!expected)return false;
    unsigned matched=0;for(const auto& change:out.items){const auto wanted=std::ranges::find(*expected,change.after.id,[](const auto& v){return v.after.id;});if(wanted==expected->end()){if(!AccessoryDebit(change))return false;}else{if(change.before!=wanted->before || change.after!=wanted->after)return false;++matched;}}
    return matched==3;
}
}
