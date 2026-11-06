#include <item/PersistentItems.hpp>
#include <item/SocketMaintenance.hpp>
#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <openssl/evp.h>

namespace auriga::item
{
bool RecallRequested(const PersistentItem& item)
{
    return (item.companion && item.companion->recallRequested) ||
        (item.growthPet && item.growthPet->recallRequested);
}
namespace
{
using E=ItemPersistenceError;
constexpr std::uint16_t CompanionSealOutcome=0x2601;
constexpr std::uint16_t PetHatchOutcome=0x2602;
constexpr std::uint16_t GrowthPetCompanionOutcome=0x2603;
constexpr std::uint16_t PetFeedOutcome=0x2604;
constexpr std::uint16_t PetProgressOutcome=0x2605;
bool PermanentSeal(const ItemCompanionRow& row)
{
    return (row.kind==CompanionKind::Pet || row.kind==CompanionKind::Mount) && row.clock==CompanionClock::Permanent;
}
bool StorageOwner(ItemStorage s)
{
    if(!ValidItemOwner(s.owner) || !s.id)return false;
    switch(s.kind)
    {
    case ItemStorageKind::Inventory: case ItemStorageKind::Equipment: case ItemStorageKind::MountInventory:
        return s.owner.kind==ItemOwnerKind::Character;
    case ItemStorageKind::Safebox: case ItemStorageKind::Mall:return s.owner.kind==ItemOwnerKind::Account;
    case ItemStorageKind::GuildStorage:return s.owner.kind==ItemOwnerKind::Guild;
    case ItemStorageKind::TradeEscrow: case ItemStorageKind::Mail: case ItemStorageKind::PendingAward:return true;
    default:return false;
    }
}
bool Wallet(const ItemWallet& w)
{
    return ValidItemOwner(w.owner) && w.revision && w.balance<=INT64_MAX &&
        ((w.currency==WalletCurrency::Gold && w.owner.kind==ItemOwnerKind::Character) ||
         (w.currency==WalletCurrency::DragonCoin && w.owner.kind==ItemOwnerKind::Account));
}
struct Writer
{
    std::vector<std::uint8_t> bytes;
    void Put(std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)bytes.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
    void Owner(ItemOwner o){Put(static_cast<std::uint8_t>(o.kind),1);Put(o.id,8);}
    void Key(ItemKey k){Put(static_cast<std::uint16_t>(k.Category()),2);Put(k.LocalId(),4);}
    bool Item(const ItemCatalog& c,const PersistentItem& p)
    {
        Put(p.id.Value(),8);Key(p.key);Owner(p.owner);Put(p.revision,8);Put(p.refinementLevel,2);Put(p.count,4);
        Put(p.boundTo.has_value(),1);if(p.boundTo)Owner(*p.boundTo);Put(p.destroyed,1);
        Put(p.placement.has_value(),1);if(p.placement){Put(static_cast<std::uint8_t>(p.placement->kind),1);Put(p.placement->storageId,8);Put(p.placement->anchor,4);}
        Put(p.attributes.lockedSlot.has_value(),1);if(p.attributes.lockedSlot)Put(*p.attributes.lockedSlot,1);
        for(const auto& e:p.attributes.slots){Put(e.has_value(),1);if(e){Put(static_cast<std::uint32_t>(e->bonus),4);Put(static_cast<std::uint64_t>(e->valueScaled),8);}}
        for(const auto& s:p.stones){Put(static_cast<std::uint8_t>(s.state),1);Put(s.grade,1);Put(s.stone.has_value(),1);if(s.stone)Key(*s.stone);Put(s.refinementLevel,2);}
        Put(p.accessory.has_value(),1);if(p.accessory){Put(p.accessory->unlockedCapacity,1);Put(p.accessory->activeGrade,1);Put(p.accessory->remainingMs,8);Put(p.accessory->permanent,1);}
        // Append only for charged items: existing instance/receipt digests stay unchanged.
        if(p.automatic){Put(0x31545541,4);Put(p.automatic->remainingRaw,8);Put(p.automatic->active,1);Put(p.automatic->nextTickInMs,8);}
        if(p.toggle){Put(0x314c4754,4);Put(p.toggle->remainingMs,8);Put(p.toggle->active,1);}
        if(p.bookSkill){Put(0x31534b42,4);Put(*p.bookSkill,2);}
        if(p.polymorph){Put(0x314d4c50,4);Put(p.polymorph->boundForm.has_value(),1);if(p.polymorph->boundForm)Put(*p.polymorph->boundForm,4);Put(p.polymorph->bookGrade,2);Put(p.polymorph->practiceRemaining,4);}
        if(p.tool){Put(0x314c4f54,4);Put(p.tool->trainingPoints,4);Put(p.tool->baitPower,4);}
        if(p.companion){Put(0x31504d43,4);Put(p.companion->recallRequested,1);}
        if(p.growthPet)
        {
            const auto state=EncodeGrowthPetState(c,p.key,*p.growthPet);
            if(!state)return false;
            Put(0x31544550,4); // PET1 extension. Legacy item digests stay unchanged.
            Put(state->size(),2);
            bytes.insert(bytes.end(),state->begin(),state->end());
        }
        return true;
    }
    void Money(const ItemWallet& w){Owner(w.owner);Put(static_cast<std::uint8_t>(w.currency),1);Put(w.revision,8);Put(w.balance,8);}
};

std::vector<std::uint8_t> HatchReceipt(ItemInstanceId id)
{
    Writer w;
    w.bytes={'P','H','T',1};
    w.Put(id.Value(),8);
    return std::move(w.bytes);
}

// The owner thread can append elapsed accessory time to any write. Allow
// only those ancillary equipment changes in a hatch transaction.
bool AccessoryTimeOnly(const ItemChange& change)
{
    if(!change.before || !change.before->accessory || !change.after.accessory ||
        !change.before->placement || change.before->placement->kind!=ItemStorageKind::Equipment)
        return false;
    const auto& before=*change.before->accessory;
    const auto& after=*change.after.accessory;
    if(before.permanent || after.permanent || !before.activeGrade || after.unlockedCapacity!=before.unlockedCapacity ||
        after.activeGrade>before.activeGrade ||
        (after.activeGrade==before.activeGrade && after.remainingMs>=before.remainingMs))return false;
    auto expected=*change.before;
    expected.revision=change.after.revision;
    expected.accessory=after;
    return expected==change.after;
}

struct PetFeedReceipt
{
    std::uint64_t food=0,seal=0,elapsedMs=0;
    PetFeedingService service{};
    std::uint16_t level=0;
    std::uint8_t characterClass=0,sex=0;
    bool inCombat=false;
};

std::optional<PetFeedReceipt> ReadPetFeedReceipt(const ItemCommit& commit)
{
    const auto& bytes=commit.result;
    if(commit.outcomeCode!=PetFeedOutcome || bytes.size()!=34 ||
        bytes[0]!='P' || bytes[1]!='F' || bytes[2]!='D' || bytes[3]!=1 || bytes[33]>1)
        return {};
    const auto read=[&](std::size_t at) {
        std::uint64_t value=0;
        for(unsigned i=0;i<8;++i)value|=std::uint64_t(bytes[at+i])<<(8*i);
        return value;
    };
    return PetFeedReceipt{read(4),read(12),read(20),static_cast<PetFeedingService>(bytes[28]),
        static_cast<std::uint16_t>(bytes[29] | (bytes[30]<<8)),bytes[31],bytes[32],
        bytes[33]!=0};
}

std::expected<PetFeedPlan,PetCareError> PersistentFeedPlan(const ItemCatalog& c,const PetCareContext& context,
    const PersistentItem& food,const PersistentItem& seal,PetFeedingService service,std::uint64_t elapsed)
{
    const auto inBag=[&](const PersistentItem& item) {
        return !item.destroyed && item.placement && item.placement->kind==ItemStorageKind::Inventory &&
            item.owner==ItemOwner{ItemOwnerKind::Character,context.actorId} && (!item.boundTo || item.boundTo==item.owner);
    };
    if(!inBag(food) || !inBag(seal) || !seal.growthPet || food.growthPet || food.companion ||
        (food.automatic && food.automatic->active) || (food.toggle && food.toggle->active))
        return std::unexpected(PetCareError::InvalidSnapshot);
    const auto& pet=*seal.growthPet;
    const bool following=service==PetFeedingService::Cube;
    if(pet.carrierInstanceId || following!=pet.recallRequested ||
        (elapsed && (!pet.everSummoned || pet.life.clock!=CompanionClock::ActiveTime)))
        return std::unexpected(PetCareError::WrongTargetState);
    const auto snapshot=[](const PersistentItem& item,bool active) {
        return PetCareItemSnapshot{item.key,item.id.Value(),item.revision,item.owner.id,item.count,item.refinementLevel,
            false,false,false,item.growthPet.has_value(),active,active ? item.id.Value() : 0};
    };
    const auto life=PlanPetLife(c,{context,snapshot(seal,elapsed!=0),pet.life,seal.revision,elapsed});
    if(!life)return std::unexpected(life.error());
    return PlanPetFeed(c,{context,snapshot(food,false),snapshot(seal,following),life->after,seal.revision,service,pet.progress});
}

PersistentItem FedPet(const PersistentItem& seal,const PetFeedPlan& plan)
{
    auto after=seal;
    ++after.revision;
    after.growthPet->life=plan.after;
    if(plan.progress)after.growthPet->progress=plan.progress->after;
    return after;
}

bool ValidPetFeedWrite(const ItemCatalog& c,const ItemCommit& commit)
{
    const auto receipt=ReadPetFeedReceipt(commit);
    if(!receipt || receipt->food==receipt->seal || commit.kind!=ItemOperationKind::Service ||
        commit.owners.size()!=1 || !commit.wallets.empty() || commit.ground || commit.recovery || commit.learning)
        return false;
    const ItemChange* food=nullptr;
    const ItemChange* seal=nullptr;
    for(const auto& change:commit.items)
    {
        if(change.after.id.Value()==receipt->food)food=&change;
        else if(change.after.id.Value()==receipt->seal)seal=&change;
        else if(!AccessoryTimeOnly(change))return false;
    }
    if(!food || !food->before || !seal || !seal->before)return false;
    const auto& owner=commit.owners.front();
    const PetCareContext context{commit.operationId,owner.owner.id,owner.sessionGeneration,1,
        receipt->level,receipt->characterClass,receipt->sex,true,true,true,false,false,receipt->inCombat};
    const auto plan=PersistentFeedPlan(c,context,*food->before,*seal->before,receipt->service,receipt->elapsedMs);
    if(!plan || FedPet(*seal->before,*plan)!=seal->after)return false;
    auto remaining=*food->before;
    ++remaining.revision;
    remaining.count-=plan->consumeFoodUnits;
    if(!remaining.count)
    {
        remaining.destroyed=true;
        remaining.placement.reset();
    }
    return remaining==food->after;
}

std::expected<GrowthPetState,E> ApplyPetProgress(const ItemCatalog& c,const PetCareContext& context,
    const PersistentItem& seal,std::span<const std::uint64_t> awards,const std::optional<GrowthPetEvolution>& evolution)
{
    if(!seal.growthPet || !seal.growthPet->everSummoned || seal.growthPet->carrierInstanceId ||
        seal.destroyed || !seal.placement || seal.placement->kind!=ItemStorageKind::Inventory ||
        seal.owner!=ItemOwner{ItemOwnerKind::Character,context.actorId} ||
        (seal.boundTo && seal.boundTo!=seal.owner) || !context.admittedByServer)
        return std::unexpected(E::InvalidChange);
    auto pet=*seal.growthPet;
    if(evolution)
    {
        if(!awards.empty() || !pet.recallRequested)return std::unexpected(E::InvalidChange);
        const PetCareItemSnapshot snapshot{seal.key,seal.id.Value(),seal.revision,seal.owner.id,seal.count,
            seal.refinementLevel,false,false,false,true,true,seal.id.Value()};
        PetLifecycleRequest request{context,snapshot,seal.revision,evolution->nowUtcMs,pet};
        const auto life=PlanPetMaintenance(c,request,evolution->elapsedMs);
        if(!life || life->expired)return std::unexpected(E::RequirementNotMet);
        request.state=life->pet.after;
        const auto next=PlanPetEvolution(c,request,evolution->growth);
        if(!next)return std::unexpected(E::RequirementNotMet);
        return next->after;
    }
    if(awards.empty() || awards.size()>MaxGrowthPetMobAwards)return std::unexpected(E::InvalidOperation);
    // Each monster reward has its own level boundary. A batch only shares a SQL receipt.
    for(const auto experience:awards)
    {
        const GrowthPetRequest request{seal.key,pet.progress,seal.id.Value(),seal.revision,seal.revision,
            seal.owner.id,context.actorId,context.rulesetRevision,context.operationId,seal.count,seal.refinementLevel,
            GrowthPetAction::AwardMobExperience,experience,true,true};
        const auto plan=PlanGrowthPetProgress(c,request);
        if(!plan)return std::unexpected(E::InvalidChange);
        pet.progress=plan->after;
    }
    if(pet.progress==seal.growthPet->progress)return std::unexpected(E::RequirementNotMet);
    return pet;
}

bool ValidPetProgressWrite(const ItemCatalog& c,const ItemCommit& commit)
{
    const auto& bytes=commit.result;
    if(commit.kind!=ItemOperationKind::Service || commit.owners.size()!=1 || !commit.wallets.empty() ||
        commit.ground || commit.recovery || commit.learning || bytes.size()<6 ||
        bytes[0]!='P' || bytes[1]!='R' || bytes[2]!='G' || bytes[3]!=1 || bytes[4]>1)
        return false;
    const ItemChange* seal=nullptr;
    for(const auto& change:commit.items)
    {
        if(change.before && change.before->growthPet)
        {
            if(seal)return false;
            seal=&change;
        }
        else if(!AccessoryTimeOnly(change))return false;
    }
    if(!seal || !seal->after.growthPet)return false;
    const auto read=[&](std::size_t at,unsigned size) {
        std::uint64_t value=0;
        for(unsigned i=0;i<size;++i)value|=std::uint64_t(bytes[at+i])<<(8*i);
        return value;
    };
    const auto& owner=commit.owners.front();
    PetCareContext context{commit.operationId,owner.owner.id,owner.sessionGeneration,1,0,0,0,true};
    std::optional<GrowthPetEvolution> evolution;
    std::vector<std::uint64_t> awards;
    if(bytes[4])
    {
        if(bytes.size()!=32 || bytes[31]>1)return false;
        evolution=GrowthPetEvolution{read(5,8),read(13,8),{static_cast<std::uint16_t>(read(21,2)),
            static_cast<std::uint16_t>(read(23,2)),static_cast<std::uint16_t>(read(25,2))}};
        context.characterLevel=static_cast<std::uint16_t>(read(27,2));
        context.characterClass=bytes[29];context.sex=bytes[30];context.inCombat=bytes[31]!=0;
        context.canUseItems=context.ownerAlive=true;
    }
    else
    {
        const auto count=bytes[5];
        if(count>MaxGrowthPetMobAwards || bytes.size()!=6+std::size_t(count)*8)return false;
        for(unsigned i=0;i<count;++i)awards.push_back(read(6+std::size_t(i)*8,8));
    }
    const auto pet=ApplyPetProgress(c,context,*seal->before,awards,evolution);
    if(!pet)return false;
    auto after=*seal->before;
    ++after.revision;
    after.growthPet=*pet;
    return after==seal->after;
}

bool ValidGrowthPetCompanionWrite(const ItemCatalog& c,const ItemCommit& r)
{
    if(r.kind!=ItemOperationKind::Service || r.owners.size()!=1 || !r.wallets.empty() ||
        r.ground || r.recovery || r.learning || r.result.size()!=21 ||
        r.result[0]!='G' || r.result[1]!='P' || r.result[2]!='C' || r.result[3]!=1)
        return false;
    const auto read=[&](std::size_t offset) {
        std::uint64_t value=0;
        for(unsigned i=0;i<8;++i)value|=std::uint64_t(r.result[offset+i])<<(8*i);
        return value;
    };
    const auto action=r.result[4];
    const auto now=read(5);
    const auto elapsed=read(13);
    if(action>static_cast<unsigned>(CompanionAction::Dismiss) || (action ? elapsed!=0 : elapsed==0))
        return false;

    const ItemChange* pet=nullptr;
    for(const auto& change:r.items)
    {
        if(change.before && change.before->growthPet)
        {
            if(pet)return false;
            pet=&change;
        }
        else if(!AccessoryTimeOnly(change))return false;
    }
    if(!pet || !pet->after.growthPet)return false;
    const auto& before=*pet->before;
    const auto& owner=r.owners.front();
    const PetCareContext context{r.operationId,owner.owner.id,owner.sessionGeneration,1,0,0,0,true};
    const PetCareItemSnapshot seal{before.key,before.id.Value(),before.revision,before.owner.id,before.count,
        before.refinementLevel,false,false,false,true,elapsed!=0,elapsed ? before.id.Value() : 0};
    const PetLifecycleRequest request{context,seal,before.revision,now,*before.growthPet};
    const auto plan=PlanPetMaintenance(c,request,elapsed);
    if(!plan)return false;
    auto after=plan->pet.after;
    if(action)
    {
        const bool summon=action==static_cast<unsigned>(CompanionAction::Summon);
        if(summon==after.recallRequested || (summon && c.Find(before.key)->base.retired))return false;
        after.recallRequested=summon;
        after.everSummoned|=summon;
    }
    else if(!before.growthPet->everSummoned || after.life==before.growthPet->life)return false;

    auto expected=before;
    expected.revision=pet->after.revision;
    expected.growthPet=std::move(after);
    return expected==pet->after;
}

bool ValidHatchWrite(const ItemCatalog& c,const ItemCommit& r)
{
    if(r.kind!=ItemOperationKind::Service || r.owners.size()!=1 || r.owners.front().owner.kind!=ItemOwnerKind::Character ||
        r.ground || r.recovery || r.learning)return false;
    const ItemChange* source=nullptr;
    const PersistentItem* born=nullptr;
    const PetHatchProfile* profile=nullptr;
    for(const auto& change:r.items)
    {
        if(!change.before)
        {
            if(born || !change.after.growthPet)return false;
            born=&change.after;
            continue;
        }
        const auto* entry=c.Find(change.before->key);
        const auto* egg=entry ? std::get_if<ItemContainerRow>(&entry->category) : nullptr;
        if(!egg || egg->kind!=ContainerKind::PetEgg)
        {
            if(!AccessoryTimeOnly(change))return false;
            continue;
        }
        if(source || entry->base.retired || !PlainInventoryItem(*change.before,r.owners.front().owner))return false;
        source=&change;
        profile=c.FindPetHatchProfile(*egg->hatchProfile);
        auto expected=*change.before;
        expected.revision=change.after.revision;
        if(!--expected.count){expected.destroyed=true;expected.placement.reset();}
        if(expected!=change.after)return false;
    }
    if(!source || !born || !profile || born->key!=profile->target || r.result!=HatchReceipt(born->id))return false;
    const auto* target=c.Find(born->key);
    const auto& state=*born->growthPet;
    const auto progress=MakeGrowthPetProgress(c,born->key);
    const auto life=MakeGrowthPetLife(c,born->key);
    const auto binding=target->base.binding==BindingPolicy::OnAcquire ? std::optional{born->owner} : std::nullopt;
    if(target->base.retired || born->boundTo!=binding ||
        born->attributes!=InstanceAttributes{} || !progress || state.progress!=*progress || !life || state.life!=*life ||
        state.ageMs || state.skills!=decltype(state.skills){} || state.carrierInstanceId || state.everSummoned ||
        state.recallRequested || !ValidPetBirthTraits(*profile,state.traits))return false;
    if(!profile->goldCost)return r.wallets.empty();
    if(r.wallets.size()!=1)return false;
    const auto& debit=r.wallets.front();
    return debit.before.owner==born->owner && debit.before.currency==WalletCurrency::Gold &&
        debit.before.balance>=profile->goldCost && debit.after.balance==debit.before.balance-profile->goldCost;
}
}
bool ValidItemOwner(ItemOwner o) noexcept
{return o.id && o.kind>=ItemOwnerKind::Character && o.kind<=ItemOwnerKind::Guild;}
bool ValidItemStorage(const ItemStorageLayout& l) noexcept
{
    return StorageOwner(l.storage) && l.columns && l.rows && l.pages &&
        std::uint64_t{l.columns}*l.rows*l.pages<=65536 &&
        (l.storage.kind!=ItemStorageKind::Equipment || (l.rows==1 && l.pages==1));
}
bool SupportsPersistentItem(const ItemCatalog& c,const ItemCatalogEntry& e) noexcept
{
    if(const auto* companion=std::get_if<ItemCompanionRow>(&e.category))return PermanentSeal(*companion) || companion->kind==CompanionKind::GrowthPet;
    if(std::holds_alternative<ItemToolRow>(e.category))return true;
    if(std::holds_alternative<ItemFishingRow>(e.category) || std::holds_alternative<ItemPolymorphRow>(e.category) || std::holds_alternative<ItemSkillBookRow>(e.category))return true;
    if(std::holds_alternative<ItemContainerRow>(e.category))return true;
    if(const auto* consumable=std::get_if<ItemConsumableRow>(&e.category))
    {
        if(consumable->bookAidProfile || consumable->socketServiceProfile.has_value() || consumable->attributeServiceProfile.has_value())return true;
        // Instant, reserve and periodic recovery act on the character, not the item.
        const auto* recovery=consumable->recoveryProfile ? c.FindRecovery(consumable->recoveryProfile) : nullptr;
        return recovery || consumable->timedEffectProfile || consumable->toggleEffectProfile;
    }
    return std::holds_alternative<ItemWeaponRow>(e.category) || std::holds_alternative<ItemArmorRow>(e.category) ||
        std::holds_alternative<ItemAccessoryRow>(e.category) || std::holds_alternative<ItemMaterialRow>(e.category) ||
        std::holds_alternative<ItemSocketStoneRow>(e.category) || std::holds_alternative<ItemCurrencyTokenRow>(e.category) ||
        std::holds_alternative<ItemSkillBookRow>(e.category) || std::holds_alternative<ItemQuestRow>(e.category) ||
        std::holds_alternative<ItemContainerKeyRow>(e.category);
}
bool SupportsPersistentGrant(const ItemCatalog& c,const ItemCatalogEntry& e) noexcept
{
    const auto* seal=std::get_if<ItemCompanionRow>(&e.category);
    return (!seal || seal->kind!=CompanionKind::GrowthPet) && SupportsPersistentItem(c,e);
}
bool PlainInventoryItem(const PersistentItem& item,ItemOwner owner) noexcept
{return item.owner==owner && !item.destroyed && item.placement && item.placement->kind==ItemStorageKind::Inventory && (!item.boundTo || item.boundTo==owner) && item.attributes==InstanceAttributes{} && item.stones==decltype(item.stones){} && (!item.accessory || *item.accessory==AccessorySocketState{}) && (!item.automatic || !item.automatic->active) && (!item.toggle || !item.toggle->active) && !RecallRequested(item) && !item.growthPet;}
std::expected<void,E> ValidatePersistentItem(const ItemCatalog& c,const PersistentItem& p)
{
    const auto* e=c.Find(p.key);if(!e)return std::unexpected(E::MissingItem);
    if(!SupportsPersistentItem(c,*e))return std::unexpected(E::UnsupportedState);
    if(!ValidItemOwner(p.owner) || !p.revision || (p.boundTo && !ValidItemOwner(*p.boundTo)) ||
       (e->base.binding==BindingPolicy::OnAcquire && !p.boundTo) ||
       (p.destroyed ? (p.count || p.placement.has_value()) : (!p.count || p.count>e->base.maxStack || !p.placement)))return std::unexpected(E::InvalidItem);
    if(!c.FindRefinement(e->base.refinementProfile)->FindLevel(p.refinementLevel))return std::unexpected(E::InvalidLevel);
    if(std::holds_alternative<ItemFishingRow>(e->category) && p.refinementLevel)return std::unexpected(E::InvalidLevel);
    if(!ValidateInstanceAttributes(c,p.key,p.attributes))return std::unexpected(E::InvalidAttributes);
    const auto* armor=std::get_if<ItemArmorRow>(&e->category);
    if(std::holds_alternative<ItemWeaponRow>(e->category) || (armor && armor->slot==EquipmentSlot::Body))
    {if(!ValidateEquipmentStoneSockets(c,p.key,p.stones))return std::unexpected(E::InvalidSockets);}
    else if(std::ranges::any_of(p.stones,[](const auto& s){return s!=StoneSocketSlot{};}))return std::unexpected(E::InvalidSockets);
    const auto* a=std::get_if<ItemAccessoryRow>(&e->category);
    if(a && a->socketProfile)
    {if(!p.accessory || !ValidateAccessorySocketState(*c.FindAccessorySocket(*a->socketProfile),*p.accessory))return std::unexpected(E::InvalidSockets);}
    else if(p.accessory)return std::unexpected(E::InvalidSockets);
    const auto* consumable=std::get_if<ItemConsumableRow>(&e->category);
    const auto* recovery=consumable && consumable->recoveryProfile ? c.FindRecovery(consumable->recoveryProfile) : nullptr;
    const auto* automatic=recovery ? std::get_if<AutomaticRecovery>(&recovery->Data().effect) : nullptr;
    if(automatic)
    {
        if(!p.automatic || (!p.destroyed && p.count!=1) || p.refinementLevel) return std::unexpected(E::InvalidItem);
        const auto& a=*p.automatic;
        if(a.remainingRaw<0 || (automatic->supply==RecoverySupply::Finite ? a.remainingRaw>automatic->capacityRaw : a.remainingRaw!=0) ||
           (a.active ? !a.nextTickInMs || a.nextTickInMs>automatic->tickIntervalMs || p.destroyed || p.owner.kind!=ItemOwnerKind::Character || !p.placement || p.placement->kind!=ItemStorageKind::Inventory || (automatic->supply==RecoverySupply::Finite && !a.remainingRaw) : a.nextTickInMs!=0))return std::unexpected(E::InvalidItem);
    }
    else if(p.automatic)return std::unexpected(E::InvalidItem);
    const auto* toggle=consumable && consumable->toggleEffectProfile ? c.FindToggleEffect(*consumable->toggleEffectProfile) : nullptr;
    if(toggle)
    {
        if(!p.toggle || p.refinementLevel || (!p.destroyed && p.count!=1) || p.toggle->remainingMs>toggle->durationMs ||
           (p.toggle->active && (!p.toggle->remainingMs || p.destroyed || p.owner.kind!=ItemOwnerKind::Character || !p.placement || p.placement->kind!=ItemStorageKind::Inventory)))return std::unexpected(E::InvalidItem);
    }
    else if(p.toggle)return std::unexpected(E::InvalidItem);
    const auto* book=std::get_if<ItemSkillBookRow>(&e->category);
    if(book){const auto* profile=c.FindSkillBook(book->profile);if(p.refinementLevel || (profile->targetMode==SkillBookTargetMode::InstanceBound ? !p.bookSkill || std::ranges::none_of(profile->targets,[&](const auto& t){return t.skillId==*p.bookSkill;}) : p.bookSkill.has_value()))return std::unexpected(E::InvalidItem);}
    else if(p.bookSkill)return std::unexpected(E::InvalidItem);
    const auto* poly=std::get_if<ItemPolymorphRow>(&e->category);
    if(poly)
    {
        const auto* profile=c.FindPolymorphProfile(poly->profile);
        if(!p.polymorph || p.refinementLevel || (poly->fixedForm ? p.polymorph->boundForm.has_value() : !p.polymorph->boundForm || !c.FindPolymorphForm(*p.polymorph->boundForm)) ||
           (profile->kind==PolymorphKind::Orb ? p.polymorph->bookGrade || p.polymorph->practiceRemaining :
            p.polymorph->bookGrade>profile->maxBookGrade || p.polymorph->practiceRemaining>std::uint64_t{profile->maxBookGrade}*profile->practicePerGrade))return std::unexpected(E::InvalidItem);
    }
    else if(p.polymorph)return std::unexpected(E::InvalidItem);
    if(const auto* tool=std::get_if<ItemToolRow>(&e->category))
    {
        const auto* profile=c.FindToolProfile(tool->profile);
        if(!p.tool || (!p.destroyed && p.count!=1) || p.refinementLevel>=profile->levels.size() ||
           p.tool->trainingPoints>profile->levels[p.refinementLevel].requiredPoints || p.tool->baitPower>INT32_MAX ||
           (tool->kind!=ToolKind::Rod && p.tool->baitPower))return std::unexpected(E::InvalidItem);
    }
    else if(p.tool)return std::unexpected(E::InvalidItem);
    const auto* seal=std::get_if<ItemCompanionRow>(&e->category);
    if(seal && seal->kind==CompanionKind::GrowthPet)
    {
        if(!p.growthPet || p.companion || p.destroyed || p.count!=1 || p.refinementLevel ||
            p.owner.kind!=ItemOwnerKind::Character || p.placement->kind!=ItemStorageKind::Inventory ||
            !ValidateGrowthPetState(c,p.key,*p.growthPet))return std::unexpected(E::InvalidItem);
    }
    else if(p.growthPet)return std::unexpected(E::InvalidItem);
    if(seal && PermanentSeal(*seal))
    {
        if(!p.companion || (!p.destroyed && p.count!=1) ||
           p.companion->phase!=CompanionPhase::Dormant || p.companion->activeFormId ||
           p.companion->lifetime!=CompanionLifetime{} ||
           (RecallRequested(p) && (p.destroyed || p.owner.kind!=ItemOwnerKind::Character ||
               !p.placement || p.placement->kind!=ItemStorageKind::Inventory)))
            return std::unexpected(E::InvalidItem);
    }
    else if(p.companion)return std::unexpected(E::InvalidItem);
    if(p.placement && !StorageOwner({p.owner,p.placement->kind,p.placement->storageId}))return std::unexpected(E::InvalidPlacement);
    return {};
}
std::expected<std::vector<std::uint32_t>,E> OccupiedItemCells(const ItemCatalog& c,const PersistentItem& p,const ItemStorageLayout& l)
{
    if(!ValidatePersistentItem(c,p) || !ValidItemStorage(l) || p.destroyed || !p.placement ||
       l.storage!=ItemStorage{p.owner,p.placement->kind,p.placement->storageId})return std::unexpected(E::InvalidPlacement);
    const auto& b=c.Find(p.key)->base;
    const auto anchor=p.placement->anchor,pageSize=std::uint32_t{l.columns}*l.rows;
    if(anchor>=std::uint64_t{pageSize}*l.pages)return std::unexpected(E::InvalidPlacement);
    if(l.storage.kind==ItemStorageKind::Equipment)
    {
        std::optional<EquipmentSlot> slot;
        const auto& category=c.Find(p.key)->category;
        if(const auto* w=std::get_if<ItemWeaponRow>(&category))slot=w->slot;
        if(std::holds_alternative<ItemToolRow>(category))slot=EquipmentSlot::Weapon;
        if(const auto* a=std::get_if<ItemArmorRow>(&category))slot=a->slot;
        if(const auto* a=std::get_if<ItemAccessoryRow>(&category))slot=a->slot;
        if(!slot || anchor+1!=static_cast<unsigned>(*slot))return std::unexpected(E::InvalidPlacement);
        return std::vector<std::uint32_t>{anchor};
    }
    const auto offset=anchor%pageSize;
    if(offset%l.columns+b.width>l.columns || offset/l.columns+b.height>l.rows)return std::unexpected(E::InvalidPlacement);
    std::vector<std::uint32_t> cells;
    for(unsigned y=0;y<b.height;++y)for(unsigned x=0;x<b.width;++x)cells.push_back(anchor+y*l.columns+x);
    return cells;
}
std::expected<void,E> ValidateItemCommit(const ItemCatalog& c,const ItemCommit& r)
{
    if(r.catalogRevision!=c.Revision() || r.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    if(r.ground)
    {
        const auto& g=*r.ground;
        if(g.core.empty() || g.core.size()>128 || !g.run || !g.storedRun || g.storedRun>g.run || !g.instance ||
           (g.create && g.storedRun!=g.run)) return std::unexpected(E::InvalidChange);
        const auto changed=std::ranges::find_if(r.items,[&](const ItemChange& i){return i.after.id.Value()==g.instance;});
        if(changed==r.items.end()) return std::unexpected(E::InvalidChange);
        const auto escrow=[](const PersistentItem& i){return !i.destroyed && i.placement && i.placement->kind==ItemStorageKind::TradeEscrow && i.placement->storageId==OwnedGroundStorageId;};
        if(g.create ? (!escrow(changed->after) || (changed->before && escrow(*changed->before))) :
           (!changed->before || !escrow(*changed->before) || escrow(changed->after))) return std::unexpected(E::InvalidChange);
    }
    if(r.owners.empty() || r.owners.size()>MaxItemCommitOwners || r.items.size()>MaxItemCommitItems ||
       r.storages.size()>MaxItemCommitStorages || r.wallets.size()>MaxItemCommitOwners || r.result.size()>MaxItemCommitResult)return std::unexpected(E::LimitExceeded);
    if((r.items.empty() && r.wallets.empty() && !r.recovery && !r.learning) || !r.outcomeCode || r.kind<ItemOperationKind::Grant || r.kind>ItemOperationKind::Trade ||
       std::ranges::all_of(r.operationId,[](auto n){return !n;}))return std::unexpected(E::InvalidOperation);
    std::set<ItemOwner> owners;
    for(const auto& o:r.owners)if(!ValidItemOwner(o.owner) || !o.sessionGeneration || !o.revision || o.revision==UINT64_MAX || !owners.insert(o.owner).second)return std::unexpected(E::InvalidFence);
    std::map<ItemStorage,ItemStorageLayout> layouts;
    for(const auto& l:r.storages)if(!ValidItemStorage(l) || !owners.contains(l.storage.owner) || !layouts.emplace(l.storage,l).second)return std::unexpected(E::InvalidStorage);
    std::set<ItemOwner> usedOwners;std::set<ItemStorage> usedLayouts;
    std::set<std::uint64_t> ids;std::set<std::pair<ItemStorage,std::uint32_t>> occupied;
    for(const auto& change:r.items)
    {
        const auto& p=change.after;
        if(!ids.insert(p.id.Value()).second)return std::unexpected(E::InvalidChange);
        for(const auto* item:{change.before ? &*change.before : nullptr,&p})if(item)
        {
            if(auto valid=ValidatePersistentItem(c,*item);!valid)return valid;
            if(!owners.contains(item->owner))return std::unexpected(E::InvalidFence);
            usedOwners.insert(item->owner);
            if(item->placement)
            {
                const ItemStorage storage{item->owner,item->placement->kind,item->placement->storageId};
                auto layout=layouts.find(storage);if(layout==layouts.end())return std::unexpected(E::InvalidStorage);
                auto cells=OccupiedItemCells(c,*item,layout->second);if(!cells)return std::unexpected(cells.error());usedLayouts.insert(storage);
                if(item==&p)for(auto cell:*cells)if(!occupied.emplace(storage,cell).second)return std::unexpected(E::InvalidPlacement);
            }
        }
        if(change.before)
        {
            const auto& b=*change.before;
            if((b.growthPet || p.growthPet) && r.outcomeCode!=GrowthPetCompanionOutcome &&
                r.outcomeCode!=PetFeedOutcome && r.outcomeCode!=PetProgressOutcome)
            {
                auto expected=b;
                expected.revision=p.revision;
                expected.placement=p.placement;
                if(r.kind!=ItemOperationKind::Move || expected!=p)return std::unexpected(E::TransferDenied);
            }
            if(b.automatic && b.automatic->active && (p.owner!=b.owner || p.key!=b.key || (p.placement!=b.placement && !p.destroyed) || (p.count!=b.count && !p.destroyed) || (p.destroyed && (r.outcomeCode!=0x2003 || !p.automatic || p.automatic->remainingRaw || p.automatic->active))))return std::unexpected(E::TransferDenied);
            if(b.toggle && b.toggle->active && (p.owner!=b.owner || p.key!=b.key || (p.placement!=b.placement && !p.destroyed) || (p.count!=b.count && !p.destroyed) || (p.destroyed && (!r.recovery || r.recovery->owner!=b.owner || r.recovery->elapsedMs<b.toggle->remainingMs || !p.toggle || p.toggle->remainingMs || p.toggle->active))))return std::unexpected(E::TransferDenied);
            if(b.companion!=p.companion || r.outcomeCode==CompanionSealOutcome)
            {
                if(r.kind!=ItemOperationKind::Service || r.outcomeCode!=CompanionSealOutcome ||
                   r.items.size()!=1 || !r.wallets.empty() || r.ground || r.recovery || r.learning ||
                   !b.companion || !p.companion || RecallRequested(b)==RecallRequested(p))
                    return std::unexpected(E::TransferDenied);
                auto expected=b;
                expected.revision=p.revision;
                expected.companion=p.companion;
                if(expected!=p)return std::unexpected(E::TransferDenied);
            }
            else if(b.companion && RecallRequested(b))
            {
                auto expected=b;
                expected.revision=p.revision;
                expected.placement=p.placement;
                if(r.kind!=ItemOperationKind::Move || !p.placement ||
                   p.placement->kind!=ItemStorageKind::Inventory || expected!=p)
                    return std::unexpected(E::TransferDenied);
            }
            const bool bindingUnchanged=p.boundTo==b.boundTo;
            const bool bindOnEquip=!b.boundTo && p.boundTo==p.owner && b.owner==p.owner && b.key==p.key &&
                c.Find(p.key)->base.binding==BindingPolicy::OnEquip && p.placement && p.placement->kind==ItemStorageKind::Equipment;
            if(b.destroyed || b.id!=p.id || b.revision==UINT64_MAX || p.revision!=b.revision+1 || (!bindingUnchanged && !bindOnEquip) || b==p)return std::unexpected(E::InvalidChange);
        }
        else if(p.destroyed || p.revision!=1 || RecallRequested(p) || r.outcomeCode==CompanionSealOutcome ||
            (p.growthPet && r.outcomeCode!=PetHatchOutcome))return std::unexpected(E::InvalidChange);
        if((!change.before || change.before->key!=p.key) && c.Find(p.key)->base.retired && !IsSocketMaintenanceRecovery(c,r))return std::unexpected(E::InvalidChange);
    }
    std::set<std::pair<ItemOwner,WalletCurrency>> wallets;
    for(const auto& w:r.wallets)
    {
        if(!Wallet(w.before) || !Wallet(w.after) || w.before.owner!=w.after.owner || w.before.currency!=w.after.currency ||
           w.before.revision==UINT64_MAX || w.after.revision!=w.before.revision+1 || w.before.balance==w.after.balance ||
           !owners.contains(w.before.owner) || !wallets.emplace(w.before.owner,w.before.currency).second)return std::unexpected(E::InvalidWallet);
        usedOwners.insert(w.before.owner);
    }
    if(r.learning){const auto& b=*r.learning;if(!ValidItemOwner(b.owner) || b.owner.kind!=ItemOwnerKind::Character || std::ranges::none_of(r.owners,[&](const auto& f){return f.owner==b.owner;}) || !ValidBookLearning(b.before) || !ValidBookLearning(b.after) || b.before.revision==UINT64_MAX || b.after.revision!=b.before.revision+1)return std::unexpected(E::InvalidChange);}
    if(r.recovery)
    {
        const auto& a=*r.recovery;
        if(a.owner.kind!=ItemOwnerKind::Character || !owners.contains(a.owner) || a.before.revision==UINT64_MAX || a.after.revision!=a.before.revision+1 ||
           a.elapsedMs>INT64_MAX || !ValidateRecoverySchedule(c,a.before) || !ValidateRecoverySchedule(c,a.after))return std::unexpected(E::InvalidChange);
        usedOwners.insert(a.owner);
    }
    if(usedOwners!=owners || usedLayouts.size()!=layouts.size())return std::unexpected(E::InvalidFence);
    if(r.outcomeCode==PetHatchOutcome && !ValidHatchWrite(c,r))return std::unexpected(E::InvalidChange);
    if(r.outcomeCode==GrowthPetCompanionOutcome && !ValidGrowthPetCompanionWrite(c,r))return std::unexpected(E::InvalidChange);
    if(r.outcomeCode==PetFeedOutcome && !ValidPetFeedWrite(c,r))return std::unexpected(E::InvalidChange);
    if(r.outcomeCode==PetProgressOutcome && !ValidPetProgressWrite(c,r))return std::unexpected(E::InvalidChange);
    return {};
}
std::expected<ItemCatalogDigest,E> ItemCommitDigest(const ItemCatalog& c,const ItemCommit& r)
{
    if(auto v=ValidateItemCommit(c,r);!v)return std::unexpected(v.error());
    Writer w;for(auto b:std::string_view("auriga.item_commit.v1"))w.Put(static_cast<unsigned char>(b),1);
    for(auto b:r.operationId)w.Put(b,1);w.Put(r.catalogRevision,8);for(auto b:r.catalogHash)w.Put(b,1);w.Put(static_cast<std::uint8_t>(r.kind),1);
    auto owners=r.owners;std::ranges::sort(owners,{},&ItemOwnerFence::owner);w.Put(owners.size(),1);
    for(const auto& o:owners){w.Owner(o.owner);w.Put(o.sessionGeneration,8);w.Put(o.revision,8);}
    auto storages=r.storages;std::ranges::sort(storages,{},&ItemStorageLayout::storage);w.Put(storages.size(),1);
    for(const auto& s:storages){w.Owner(s.storage.owner);w.Put(static_cast<std::uint8_t>(s.storage.kind),1);w.Put(s.storage.id,8);w.Put(s.columns,2);w.Put(s.rows,2);w.Put(s.pages,2);}
    auto items=r.items;std::ranges::sort(items,{},[](const auto& i){return i.after.id;});w.Put(items.size(),1);
    for(const auto& i:items)
    {
        w.Put(i.before.has_value(),1);
        if((i.before && !w.Item(c,*i.before)) || !w.Item(c,i.after))return std::unexpected(E::InvalidItem);
    }
    auto wallets=r.wallets;std::ranges::sort(wallets,{},[](const auto& v){return std::pair{v.before.owner,v.before.currency};});w.Put(wallets.size(),1);
    for(const auto& v:wallets){w.Money(v.before);w.Money(v.after);}w.Put(r.outcomeCode,2);w.Put(r.result.size(),4);for(auto b:r.result)w.Put(b,1);
    if(r.ground)
    {
        const auto& g=*r.ground;
        w.Put(0x31444e554f5247ULL,8); // GROUND1 extension; legacy commits keep their digest
        w.Put(g.core.size(),2); for(unsigned char b:g.core) w.Put(b,1);
        w.Put(g.run,8);w.Put(g.storedRun,8);w.Put(g.instance,8);w.Put(g.create,1);
    }
    if(r.recovery)
    {
        const auto& a=*r.recovery;w.Put(0x3156434552ULL,8);w.Owner(a.owner);w.Put(a.elapsedMs,8);
        for(const auto* state:{&a.before,&a.after}){const auto encoded=EncodeRecoverySchedule(c,*state);if(!encoded)return std::unexpected(E::InvalidChange);w.Put(encoded->size(),4);for(auto b:*encoded)w.Put(b,1);}
    }
    if(r.learning){const auto& b=*r.learning;w.Put(0x314b4f42,4);w.Owner(b.owner);for(const auto* state:{&b.before,&b.after}){const auto encoded=EncodeBookLearning(*state);w.Put(encoded->size(),4);for(auto byte:*encoded)w.Put(byte,1);}}
    ItemCatalogDigest digest{};unsigned length=0;
    if(EVP_Digest(w.bytes.data(),w.bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1 || length!=digest.size())return std::unexpected(E::HashFailure);
    return digest;
}
std::expected<ItemCatalogDigest,E> PersistentItemDigest(const ItemCatalog& c,const PersistentItem& p)
{
    if(auto v=ValidatePersistentItem(c,p);!v)return std::unexpected(v.error());
    Writer w;for(auto b:std::string_view("auriga.item_instance.v1"))w.Put(static_cast<unsigned char>(b),1);
    if(!w.Item(c,p))return std::unexpected(E::InvalidItem);
    ItemCatalogDigest digest{};unsigned length=0;
    if(EVP_Digest(w.bytes.data(),w.bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1 || length!=digest.size())return std::unexpected(E::HashFailure);
    return digest;
}
std::expected<ItemCatalogDigest,E> RecoveryScheduleDigest(const ItemCatalog& c,ItemOwner owner,const RecoverySchedule& s)
{
    if(owner.kind!=ItemOwnerKind::Character || !ValidItemOwner(owner))return std::unexpected(E::InvalidOwner);
    const auto encoded=EncodeRecoverySchedule(c,s);if(!encoded)return std::unexpected(E::InvalidChange);Writer w;for(char b:std::string_view("auriga.recovery.v1"))w.Put(b,1);w.Owner(owner);for(auto b:*encoded)w.Put(b,1);
    ItemCatalogDigest digest{};unsigned length=0;if(EVP_Digest(w.bytes.data(),w.bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1 || length!=digest.size())return std::unexpected(E::HashFailure);return digest;
}
std::expected<ItemCatalogDigest,E> BookLearningDigest(ItemOwner owner,const BookLearning& s)
{const auto bytes=EncodeBookLearning(s);if(!bytes || !ValidItemOwner(owner) || owner.kind!=ItemOwnerKind::Character)return std::unexpected(E::InvalidChange);Writer w;for(char c:std::string_view("auriga.book_learning.v1"))w.Put(c,1);w.Owner(owner);for(auto b:*bytes)w.Put(b,1);ItemCatalogDigest digest{};unsigned n=0;if(EVP_Digest(w.bytes.data(),w.bytes.size(),digest.data(),&n,EVP_sha256(),nullptr)!=1 || n!=digest.size())return std::unexpected(E::HashFailure);return digest;}
std::expected<ItemCommit,E> MakeRefinementCommit(const ItemCatalog& c,const RefinementPlan& p,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,const ItemWallet& wallet)
{
    if(p.catalogRevision!=c.Revision() || p.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,p.actorId} || wallet.owner!=owner.owner || wallet.currency!=WalletCurrency::Gold ||
       wallet.revision!=p.expectedWalletRevision || wallet.balance!=p.expectedGold || p.goldCost>wallet.balance)return std::unexpected(E::InvalidWallet);
    const auto* recipe=c.FindRefinementRecipe(p.recipeId);
    if(!recipe || items.size()>MaxRefinementInventory)return std::unexpected(E::InvalidChange);
    std::set<std::uint64_t> ids;std::vector<RefinementItemSnapshot> inventory;
    const PersistentItem* source=nullptr;
    for(const auto& i:items)
    {
        if(!ids.insert(i.id.Value()).second || i.owner!=owner.owner || i.destroyed || !i.placement || i.placement->kind!=ItemStorageKind::Inventory || !ValidatePersistentItem(c,i))return std::unexpected(E::InvalidChange);
        if(i.id.Value()==p.sourceInstanceId)source=&i;
        inventory.push_back({i.id.Value(),i.revision,i.owner.id,i.key,i.refinementLevel,i.count,false});
    }
    if(!source)return std::unexpected(E::InvalidChange);
    // Recompute the pure plan from exact snapshots to reject a forged debit,
    // result, cost or outcome before it reaches SQL.
    auto expected=PrepareRefinement(c,p.recipeId,{p.operationId,p.actorId,wallet.revision,wallet.balance,true},
        {source->id.Value(),source->revision,source->owner.id,source->key,source->refinementLevel,source->count,false},inventory,p.recordedRoll);
    if(!expected || expected->source!=p.source || expected->sourceLevel!=p.sourceLevel || expected->expectedSourceRevision!=p.expectedSourceRevision ||
       expected->goldCost!=p.goldCost || expected->result!=p.result || expected->resultLevel!=p.resultLevel || expected->success!=p.success || expected->debits.size()!=p.debits.size())return std::unexpected(E::InvalidChange);
    for(std::size_t i=0;i<p.debits.size();++i)
    {const auto& a=expected->debits[i];const auto& b=p.debits[i];if(a.instanceId!=b.instanceId || a.expectedRevision!=b.expectedRevision || a.expectedCount!=b.expectedCount || a.consumeCount!=b.consumeCount)return std::unexpected(E::InvalidChange);}
    ItemCommit out;out.operationId=p.operationId;out.catalogRevision=p.catalogRevision;out.catalogHash=p.catalogHash;out.kind=ItemOperationKind::Refine;out.owners={owner};
    auto after=*source;after.revision++;
    if(p.result){after.key=*p.result;after.refinementLevel=*p.resultLevel;}
    else{after.destroyed=true;after.count=0;after.placement.reset();}
    out.items.push_back({*source,after});
    for(const auto& debit:p.debits)
    {
        const auto found=std::ranges::find(items,debit.instanceId,[](const auto& i){return i.id.Value();});
        if(found==items.end() || found->count<debit.consumeCount || found->id==source->id)return std::unexpected(E::InvalidChange);
        auto next=*found;next.revision++;next.count-=debit.consumeCount;if(!next.count){next.destroyed=true;next.placement.reset();}out.items.push_back({*found,next});
    }
    if(p.goldCost){auto next=wallet;next.revision++;next.balance-=p.goldCost;out.wallets.push_back({wallet,next});}
    for(const auto& layout:storages)
    {
        bool used=false;for(const auto& change:out.items)for(const auto* i:{&*change.before,&change.after})
            if(i->placement && layout.storage==ItemStorage{i->owner,i->placement->kind,i->placement->storageId})used=true;
        if(used)out.storages.push_back(layout);
    }
    out.outcomeCode=p.success ? 1 : 2;out.result={static_cast<std::uint8_t>(p.success)};
    if(auto v=ValidateItemCommit(c,out);!v)return std::unexpected(v.error());return out;
}
namespace
{
std::expected<ItemCommit,E> PairCommit(const ItemCatalog& c,const SocketItemPair& pair,std::uint16_t units,
    ItemOwnerFence owner,std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory)
{
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,pair.context.actorId} || owner.sessionGeneration!=pair.context.sessionGeneration ||
       owner.revision!=pair.context.inventoryRevision || inventory.size()>MaxRefinementInventory)return std::unexpected(E::InvalidFence);
    std::set<std::uint64_t> ids;for(const auto& i:inventory)if(!ids.insert(i.id.Value()).second)return std::unexpected(E::InvalidChange);
    const auto find=[&](const StoneItemSnapshot& snapshot)->const PersistentItem*
    {
        const auto it=std::ranges::find(inventory,snapshot.instanceId,[](const auto& i){return i.id.Value();});
        if(it==inventory.end() || !ValidatePersistentItem(c,*it) || it->destroyed || it->owner!=owner.owner ||
           snapshot.ownerId!=it->owner.id || snapshot.item!=it->key || snapshot.revision!=it->revision || snapshot.refinementLevel!=it->refinementLevel ||
           snapshot.count!=it->count || !it->placement || it->placement->kind!=ItemStorageKind::Inventory || snapshot.locked || snapshot.equipped || snapshot.exchanging)return nullptr;
        return &*it;
    };
    const auto* source=find(pair.source);const auto* target=find(pair.target);
    if(!source || !target || source->id==target->id || units>source->count)return std::unexpected(E::InvalidChange);
    ItemCommit r;r.operationId=pair.context.operationId;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Service;r.owners={owner};
    auto next=*target;next.revision++;r.items.push_back({*target,next});
    if(units){next=*source;next.revision++;next.count-=units;if(!next.count){next.destroyed=true;next.placement.reset();}r.items.push_back({*source,next});}
    for(const auto& layout:storages)
    {
        bool used=false;for(const auto& change:r.items)for(const auto* i:{&*change.before,&change.after})
            if(i->placement && layout.storage==ItemStorage{i->owner,i->placement->kind,i->placement->storageId})used=true;
        if(used)r.storages.push_back(layout);
    }
    return r;
}
}
std::expected<ItemCommit,E> MakeAttributeCommit(const ItemCatalog& c,const AttributePlan& plan,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory)
{
    if(plan.catalogRevision!=c.Revision() || plan.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    const auto recalculated=PlanAttributeService(c,plan.expected);
    if(!recalculated || recalculated->after!=plan.after || recalculated->consumeUnits!=plan.consumeUnits || recalculated->succeeded!=plan.succeeded)return std::unexpected(E::InvalidChange);
    auto commit=PairCommit(c,plan.expected.items,plan.consumeUnits,owner,storages,inventory);if(!commit)return commit;
    if(commit->items[0].before->attributes!=plan.expected.state)return std::unexpected(E::InvalidChange);
    commit->items[0].after.attributes=plan.after;commit->outcomeCode=plan.succeeded ? 1 : 2;
    const auto payload=EncodeInstanceAttributes(c,commit->items[0].after.key,plan.after);if(!payload)return std::unexpected(E::InvalidAttributes);
    commit->result={'A','T','R',1,static_cast<std::uint8_t>(plan.expected.successRoll&255),static_cast<std::uint8_t>(plan.expected.successRoll>>8),plan.expected.lockRoll,static_cast<std::uint8_t>(plan.expected.draws.size())};
    for(const auto& draw:plan.expected.draws)
    {
        commit->result.push_back(static_cast<std::uint8_t>(draw.tierRoll&255));commit->result.push_back(static_cast<std::uint8_t>(draw.tierRoll>>8));
        for(unsigned i=0;i<4;++i)commit->result.push_back(static_cast<std::uint8_t>(draw.bonusRoll>>(8*i)));
    }
    commit->result.insert(commit->result.end(),payload->begin(),payload->end());
    if(auto valid=ValidateItemCommit(c,*commit);!valid)return std::unexpected(valid.error());return commit;
}
std::expected<ItemCommit,E> MakeSocketServiceCommit(const ItemCatalog& c,const SocketServicePlan& plan,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory)
{
    if(plan.catalogRevision!=c.Revision() || plan.catalogHash!=c.ContentHash())return std::unexpected(E::CatalogMismatch);
    // Returned stones require output-ID/placement admission, implemented in a
    // later writer slice. Do not consume their source and silently lose returns.
    if(!plan.returns.empty())return std::unexpected(E::UnsupportedState);
    const auto recalculated=PlanSocketService(c,plan.expected);
    if(!recalculated || !recalculated->returns.empty() || recalculated->after!=plan.after || recalculated->consumeUnits!=plan.consumeUnits)return std::unexpected(E::InvalidChange);
    auto commit=PairCommit(c,plan.expected.items,plan.consumeUnits,owner,storages,inventory);if(!commit)return commit;
    if(commit->items[0].before->stones!=plan.expected.sockets)return std::unexpected(E::InvalidChange);
    commit->items[0].after.stones=plan.after;commit->outcomeCode=1;
    if(auto valid=ValidateItemCommit(c,*commit);!valid)return std::unexpected(valid.error());return commit;
}
namespace
{
// Relocations of placed items (move, equip, swap, unequip): every touched
// storage is re-checked as a whole after all changes, so a swap frees its
// cells before they are taken and no two items ever overlap.
std::expected<ItemCommit,E> Relocate(const ItemCatalog& c,const std::array<std::uint8_t,16>& operationId,const ItemOwnerFence& owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,std::vector<ItemChange> changes)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto layoutOf=[&](const ItemPlacement& p)->const ItemStorageLayout*{
        const auto it=std::ranges::find_if(storages,[&](const ItemStorageLayout& l){return l.storage==ItemStorage{owner.owner,p.kind,p.storageId};});
        return it==storages.end()?nullptr:&*it;};
    std::vector<ItemStorageLayout> touched;
    for(const auto& change:changes)
        for(const auto* p:{&*change.before->placement,&*change.after.placement})
        {
            const auto* layout=layoutOf(*p);if(!layout)return std::unexpected(E::InvalidStorage);
            if(std::ranges::find(touched,*layout)==touched.end())touched.push_back(*layout);
        }
    std::set<std::pair<ItemStorage,std::uint32_t>> taken;
    for(const auto& item:inventory)
    {
        if(item.destroyed || !item.placement)continue;
        const auto changed=std::ranges::find_if(changes,[&](const ItemChange& ch){return ch.after.id==item.id;});
        const auto& placed=changed==changes.end()?item:changed->after;
        const auto* layout=layoutOf(*placed.placement);
        if(!layout || std::ranges::find(touched,*layout)==touched.end())continue;
        const auto cells=OccupiedItemCells(c,placed,*layout);if(!cells)return std::unexpected(cells.error());
        for(const auto cell:*cells)if(!taken.emplace(layout->storage,cell).second)return std::unexpected(E::InvalidPlacement);
    }
    ItemCommit r;r.operationId=operationId;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Move;
    r.owners={owner};r.storages=std::move(touched);r.items=std::move(changes);r.outcomeCode=1;r.result={static_cast<std::uint8_t>(ItemOperationKind::Move)};
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());
    return r;
}
const PersistentItem* Placed(std::span<const PersistentItem> inventory,const ItemOwnerFence& owner,ItemInstanceId id)
{
    const auto it=std::ranges::find_if(inventory,[&](const PersistentItem& i){return i.id==id && !i.destroyed;});
    return it==inventory.end() || it->owner!=owner.owner || !it->placement?nullptr:&*it;
}
std::vector<ItemStorageLayout> InventoryBags(std::span<const ItemStorageLayout> storages,ItemOwner owner)
{
    std::vector<ItemStorageLayout> bags;
    for(const auto& l:storages)
        if(l.storage.owner==owner && l.storage.kind==ItemStorageKind::Inventory)bags.push_back(l);
    std::ranges::sort(bags,{},[](const ItemStorageLayout& l){return l.storage.id;});
    return bags;
}

using InventoryCells=std::set<std::pair<std::uint64_t,std::uint32_t>>;

const ItemStorageLayout* PlaceFirst(const ItemCatalog& c,PersistentItem& item,
    std::span<const ItemStorageLayout> bags,InventoryCells& taken)
{
    for(const auto& bag:bags)
    {
        if(!ValidItemStorage(bag))continue;
        const auto size=std::uint32_t{bag.columns}*bag.rows*bag.pages;
        for(std::uint32_t anchor=0;anchor<size;++anchor)
        {
            item.placement=ItemPlacement{ItemStorageKind::Inventory,bag.storage.id,anchor};
            const auto cells=OccupiedItemCells(c,item,bag);
            if(!cells || std::ranges::any_of(*cells,[&](auto cell){return taken.contains({bag.storage.id,cell});}))continue;
            for(const auto cell:*cells)taken.emplace(bag.storage.id,cell);
            return &bag;
        }
    }
    return nullptr;
}
std::optional<EquipmentSlot> SlotOf(const ItemCatalogEntry& entry)
{
    if(std::holds_alternative<ItemToolRow>(entry.category))return EquipmentSlot::Weapon;
    if(const auto* w=std::get_if<ItemWeaponRow>(&entry.category))return w->slot;
    if(const auto* a=std::get_if<ItemArmorRow>(&entry.category))return a->slot;
    if(const auto* a=std::get_if<ItemAccessoryRow>(&entry.category))return a->slot;
    return std::nullopt;
}
}
std::expected<ItemCommit,E> MakeMoveCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,ItemPlacement target)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    if(target.kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    if(*source->placement==target)return std::unexpected(E::InvalidChange);
    auto moved=*source;moved.revision++;moved.placement=target;
    return Relocate(c,operationId,owner,storages,inventory,{{*source,moved}});
}
std::expected<ItemCommit,E> MakeEquipCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,EquipContext who)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    const auto* entry=c.Find(source->key);if(!entry)return std::unexpected(E::MissingItem);
    const auto slot=SlotOf(*entry);if(!slot)return std::unexpected(E::UnsupportedState);
    if(entry->base.minimumLevel>who.level || who.characterClass>3 || who.sex>1)return std::unexpected(E::RequirementNotMet);
    if(entry->base.usePolicy)
    {
        const auto* policy=c.FindUsePolicy(entry->base.usePolicy);if(!policy)return std::unexpected(E::InvalidItem);
        if(!(policy->allowedClasses>>who.characterClass & 1) || !(policy->allowedSexes>>who.sex & 1))return std::unexpected(E::RequirementNotMet);
    }
    const ItemPlacement target{ItemStorageKind::Equipment,1,static_cast<std::uint32_t>(*slot)-1};
    auto equipped=*source;equipped.revision++;equipped.placement=target;
    if(entry->base.binding==BindingPolicy::OnEquip && !equipped.boundTo)equipped.boundTo=owner.owner;
    std::vector<ItemChange> changes{{*source,equipped}};
    const auto occupant=std::ranges::find_if(inventory,[&](const PersistentItem& i){return !i.destroyed && i.placement && *i.placement==target;});
    if(occupant!=inventory.end())
    {
        // The worn item takes the freed place; Relocate refuses it if it does not fit there.
        auto back=*occupant;back.revision++;back.placement=source->placement;
        changes.push_back({*occupant,back});
    }
    return Relocate(c,operationId,owner,storages,inventory,std::move(changes));
}
std::expected<ItemCommit,E> MakeConsumeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,std::uint32_t units)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    if(!units || units>source->count)return std::unexpected(E::InvalidChange);
    const auto layout=std::ranges::find_if(storages,[&](const ItemStorageLayout& l){
        return l.storage==ItemStorage{owner.owner,source->placement->kind,source->placement->storageId};});
    if(layout==storages.end())return std::unexpected(E::InvalidStorage);
    auto after=*source;after.revision++;after.count-=units;
    if(!after.count){after.destroyed=true;after.placement.reset();}
    ItemCommit r;r.operationId=operationId;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Consume;
    r.owners={owner};r.storages={*layout};r.items={{*source,after}};r.outcomeCode=1;r.result={static_cast<std::uint8_t>(ItemOperationKind::Consume)};
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());
    return r;
}
std::expected<ItemCommit,E> MakeCompanionCommit(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,const CompanionRequest& request)
{
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,request.ownerId})return std::unexpected(E::InvalidFence);
    const auto source=std::ranges::find(inventory,request.instanceId,[](const auto& item){return item.id.Value();});
    if(source==inventory.end() || !source->companion || source->destroyed ||
       !source->placement || source->placement->kind!=ItemStorageKind::Inventory ||
       (source->boundTo && source->boundTo!=owner.owner) ||
       source->owner!=owner.owner || source->key!=request.key ||
       source->revision!=request.revision || source->count!=request.count ||
       source->refinementLevel!=request.refinementLevel ||
       source->companion->recallRequested!=request.state.recallRequested ||
       source->companion->lifetime!=request.state.lifetime)
        return std::unexpected(E::InvalidChange);

    const auto* entry=c.Find(source->key);
    const auto* row=entry ? std::get_if<ItemCompanionRow>(&entry->category) : nullptr;
    if(!row || !PermanentSeal(*row))return std::unexpected(E::UnsupportedState);
    if(request.action!=CompanionAction::Summon && request.action!=CompanionAction::Dismiss)
        return std::unexpected(E::InvalidOperation);
    auto admitted=request;
    admitted.channelOccupiedByOther|=std::ranges::any_of(inventory,[&](const auto& other){
        return other.id!=source->id && other.owner==owner.owner && RecallRequested(other);
    });
    const auto plan=PlanCompanion(c,admitted);
    if(!plan)return std::unexpected(plan.error()==CompanionError::ChannelOccupied ? E::Conflict : E::RequirementNotMet);
    if(plan->after.recallRequested==source->companion->recallRequested)return std::unexpected(E::InvalidChange);

    const auto layout=std::ranges::find(storages,ItemStorage{owner.owner,source->placement->kind,source->placement->storageId},&ItemStorageLayout::storage);
    if(layout==storages.end())return std::unexpected(E::InvalidStorage);
    auto after=*source;
    ++after.revision;
    after.companion->recallRequested=plan->after.recallRequested;
    ItemCommit commit;
    commit.operationId=request.operationId;
    commit.catalogRevision=c.Revision();
    commit.catalogHash=c.ContentHash();
    commit.kind=ItemOperationKind::Service;
    commit.owners={owner};
    commit.storages={*layout};
    commit.items={{*source,after}};
    commit.outcomeCode=CompanionSealOutcome;
    commit.result={'C','M','P',1,static_cast<std::uint8_t>(request.action)};
    if(auto valid=ValidateItemCommit(c,commit);!valid)return std::unexpected(valid.error());
    return commit;
}

std::expected<ItemCommit,E> MakeGrowthPetCompanionCommit(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,
    const PetLifecycleRequest& request,std::optional<CompanionAction> action,std::uint64_t elapsedMs)
{
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,request.context.actorId} ||
       owner.sessionGeneration!=request.context.sessionGeneration)
        return std::unexpected(E::InvalidFence);
    const auto id=ItemInstanceId::Create(request.seal.instanceId);
    const auto* source=id ? Placed(inventory,owner,*id) : nullptr;
    if(!source || !source->growthPet || source->key!=request.seal.key ||
       source->revision!=request.seal.revision || *source->growthPet!=request.state ||
       source->count!=request.seal.count || source->refinementLevel!=request.seal.refinementLevel ||
       (source->boundTo && source->boundTo!=owner.owner))
        return std::unexpected(E::InvalidChange);
    const auto layout=std::ranges::find(storages,
        ItemStorage{owner.owner,source->placement->kind,source->placement->storageId},&ItemStorageLayout::storage);
    if(layout==storages.end())return std::unexpected(E::InvalidStorage);
    auto after=*source;
    ++after.revision;
    if(action)
    {
        const bool summon=*action==CompanionAction::Summon;
        const bool occupied=std::ranges::any_of(inventory,[&](const auto& other){
            return other.id!=source->id && other.owner==owner.owner && RecallRequested(other);
        });
        const auto plan=PlanGrowthPetSummon(c,request,summon,true,occupied);
        if(!plan)return std::unexpected(E::RequirementNotMet);
        after.growthPet=plan->pet.after;
    }
    else
    {
        const auto plan=PlanPetMaintenance(c,request,elapsedMs);
        if(!plan)return std::unexpected(E::InvalidChange);
        after.growthPet=plan->pet.after;
    }
    ItemCommit commit;
    commit.operationId=request.context.operationId;
    commit.catalogRevision=c.Revision();
    commit.catalogHash=c.ContentHash();
    commit.kind=ItemOperationKind::Service;
    commit.owners={owner};
    commit.storages={*layout};
    commit.items={{*source,std::move(after)}};
    commit.outcomeCode=GrowthPetCompanionOutcome;
    Writer result;
    result.bytes={'G','P','C',1,action ? static_cast<std::uint8_t>(*action) : std::uint8_t{0}};
    result.Put(request.nowUtcMs,8);
    result.Put(elapsedMs,8);
    commit.result=std::move(result.bytes);
    if(auto valid=ValidateItemCommit(c,commit);!valid)return std::unexpected(valid.error());
    return commit;
}

std::optional<std::uint64_t> PetFeedClockCharge(const ItemCommit& commit,std::uint64_t sealId)
{
    const auto receipt=ReadPetFeedReceipt(commit);
    if(!receipt || receipt->seal!=sealId)return {};
    return receipt->elapsedMs;
}

std::expected<PetFeedWrite,PetFeedCommitError> MakePetFeedCommit(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,const PetCareContext& context,
    ItemInstanceId foodId,ItemInstanceId sealId,PetFeedingService service,std::uint64_t elapsedMs)
{
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,context.actorId} || owner.sessionGeneration!=context.sessionGeneration)
        return std::unexpected(PetFeedCommitError{E::InvalidFence});
    const auto* food=Placed(inventory,owner,foodId);
    const auto* seal=Placed(inventory,owner,sealId);
    if(!food || !seal)return std::unexpected(PetFeedCommitError{E::MissingItem});
    const auto plan=PersistentFeedPlan(c,context,*food,*seal,service,elapsedMs);
    if(!plan)return std::unexpected(PetFeedCommitError{plan.error()});
    const auto layout=std::ranges::find(storages,ItemStorage{owner.owner,seal->placement->kind,seal->placement->storageId},
        &ItemStorageLayout::storage);
    if(layout==storages.end())return std::unexpected(PetFeedCommitError{E::InvalidStorage});
    auto commit=MakeConsumeCommit(c,context.operationId,owner,storages,inventory,foodId,plan->consumeFoodUnits);
    if(!commit)return std::unexpected(PetFeedCommitError{commit.error()});
    if(std::ranges::find(commit->storages,*layout)==commit->storages.end())commit->storages.push_back(*layout);
    commit->items.push_back({*seal,FedPet(*seal,*plan)});
    commit->kind=ItemOperationKind::Service;
    commit->outcomeCode=PetFeedOutcome;
    Writer writer;
    writer.bytes={'P','F','D',1};
    writer.Put(foodId.Value(),8);
    writer.Put(sealId.Value(),8);
    writer.Put(elapsedMs,8);
    writer.Put(static_cast<std::uint8_t>(service),1);
    writer.Put(context.characterLevel,2);
    writer.Put(context.characterClass,1);
    writer.Put(context.sex,1);
    writer.Put(context.inCombat,1);
    commit->result=std::move(writer.bytes);
    if(auto valid=ValidateItemCommit(c,*commit);!valid)return std::unexpected(PetFeedCommitError{valid.error()});
    return PetFeedWrite{*plan,std::move(*commit)};
}

std::expected<ItemCommit,E> MakeGrowthPetProgressCommit(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,const PetCareContext& context,
    ItemInstanceId sealId,std::span<const std::uint64_t> awards,std::optional<GrowthPetEvolution> evolution)
{
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,context.actorId} || owner.sessionGeneration!=context.sessionGeneration)
        return std::unexpected(E::InvalidFence);
    const auto* seal=Placed(inventory,owner,sealId);
    if(!seal)return std::unexpected(E::MissingItem);
    const auto pet=ApplyPetProgress(c,context,*seal,awards,evolution);
    if(!pet)return std::unexpected(pet.error());
    const auto layout=std::ranges::find(storages,ItemStorage{owner.owner,seal->placement->kind,seal->placement->storageId},
        &ItemStorageLayout::storage);
    if(layout==storages.end())return std::unexpected(E::InvalidStorage);
    auto after=*seal;
    ++after.revision;
    after.growthPet=*pet;
    ItemCommit commit;
    commit.operationId=context.operationId;
    commit.catalogRevision=c.Revision();
    commit.catalogHash=c.ContentHash();
    commit.kind=ItemOperationKind::Service;
    commit.owners={owner};
    commit.storages={*layout};
    commit.items={{*seal,std::move(after)}};
    commit.outcomeCode=PetProgressOutcome;
    Writer result;
    result.bytes={'P','R','G',1,static_cast<std::uint8_t>(evolution.has_value())};
    if(evolution)
    {
        result.Put(evolution->nowUtcMs,8);
        result.Put(evolution->elapsedMs,8);
        result.Put(evolution->growth.maximumHpPercentTenths,2);
        result.Put(evolution->growth.monsterDamagePercentTenths,2);
        result.Put(evolution->growth.pveNormalDamagePercentTenths,2);
        result.Put(context.characterLevel,2);
        result.Put(context.characterClass,1);
        result.Put(context.sex,1);
        result.Put(context.inCombat,1);
    }
    else
    {
        result.Put(awards.size(),1);
        for(const auto experience:awards)result.Put(experience,8);
    }
    commit.result=std::move(result.bytes);
    if(auto valid=ValidateItemCommit(c,commit);!valid)return std::unexpected(valid.error());
    return commit;
}

std::expected<ItemCommit,PetHatchCommitError> MakePetHatchCommit(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,
    std::span<const ItemWallet> wallets,const PetHatchRequest& request)
{
    if(owner.owner!=ItemOwner{ItemOwnerKind::Character,request.context.actorId} ||
        owner.sessionGeneration!=request.context.sessionGeneration)
        return std::unexpected(PetHatchCommitError{E::InvalidFence});
    const auto sourceId=ItemInstanceId::Create(request.egg.instanceId);
    const auto* source=sourceId ? Placed(inventory,owner,*sourceId) : nullptr;
    if(!source || !PlainInventoryItem(*source,owner.owner) || source->key!=request.egg.key ||
        source->count!=request.egg.count || source->refinementLevel!=request.egg.refinementLevel ||
        source->revision!=request.egg.revision || source->owner.id!=request.egg.ownerId)
        return std::unexpected(PetHatchCommitError{E::InvalidChange});
    const auto wallet=std::ranges::find_if(wallets,[&](const auto& w){return w.owner==owner.owner && w.currency==WalletCurrency::Gold;});
    if(wallet==wallets.end() || wallet->revision!=request.walletRevision || wallet->balance!=request.gold)
        return std::unexpected(PetHatchCommitError{E::InvalidWallet});
    const auto newId=ItemInstanceId::Create(request.newInstanceId);
    if(!newId || std::ranges::find(inventory,*newId,&PersistentItem::id)!=inventory.end())
        return std::unexpected(PetHatchCommitError{E::InvalidChange});

    const auto plan=PlanPetHatch(c,request);
    if(!plan)return std::unexpected(PetHatchCommitError{plan.error()});
    const auto state=MakeGrowthPetState(c,*plan);
    if(!state)return std::unexpected(PetHatchCommitError{E::InvalidChange});
    auto commit=MakeConsumeCommit(c,request.context.operationId,owner,storages,inventory,*sourceId,plan->consumeEggUnits);
    if(!commit)return std::unexpected(PetHatchCommitError{commit.error()});

    const auto bags=InventoryBags(storages,owner.owner);
    InventoryCells taken;
    for(const auto& item:inventory)
    {
        if(item.destroyed || item.owner!=owner.owner || !item.placement || item.placement->kind!=ItemStorageKind::Inventory ||
            (item.id==*sourceId && commit->items.front().after.destroyed))continue;
        const auto bag=std::ranges::find_if(bags,[&](const auto& l){return l.storage.id==item.placement->storageId;});
        if(bag==bags.end())return std::unexpected(PetHatchCommitError{E::InvalidStorage});
        const auto cells=OccupiedItemCells(c,item,*bag);
        if(!cells)return std::unexpected(PetHatchCommitError{cells.error()});
        for(const auto cell:*cells)
            if(!taken.emplace(bag->storage.id,cell).second)return std::unexpected(PetHatchCommitError{E::InvalidPlacement});
    }
    PersistentItem born{*newId,plan->target,owner.owner};
    born.refinementLevel=plan->refinementLevel;
    born.count=plan->newItemCount;
    born.growthPet=*state;
    if(c.Find(born.key)->base.binding==BindingPolicy::OnAcquire)born.boundTo=owner.owner;
    const auto* bag=PlaceFirst(c,born,bags,taken);
    if(!bag)return std::unexpected(PetHatchCommitError{E::InvalidPlacement});
    if(std::ranges::find(commit->storages,*bag)==commit->storages.end())commit->storages.push_back(*bag);
    commit->items.push_back({{},std::move(born)});
    if(plan->goldCost)
    {
        auto after=*wallet;
        ++after.revision;
        after.balance-=plan->goldCost;
        commit->wallets.push_back({*wallet,after});
    }
    commit->kind=ItemOperationKind::Service;
    commit->outcomeCode=PetHatchOutcome;
    commit->result=HatchReceipt(*newId);
    if(auto valid=ValidateItemCommit(c,*commit);!valid)return std::unexpected(PetHatchCommitError{valid.error()});
    return std::move(*commit);
}
bool QueueGrant(std::deque<ItemGrant>& pending,const ItemGrant& grant,std::uint32_t mergeLimit)
{
    if(!grant.count)return false;
    for(auto& waiting:pending)
        if(waiting.key==grant.key && waiting.refinementLevel==grant.refinementLevel && waiting.bookSkill==grant.bookSkill && waiting.polymorph==grant.polymorph && waiting.tool==grant.tool &&
           std::uint64_t{waiting.count}+grant.count<=mergeLimit)
        {
            waiting.count+=grant.count;
            return true;
        }
    if(pending.size()>=MaxPendingGrants)return false;
    pending.push_back(grant);
    return true;
}
std::expected<ItemCommit,E> MakeGrantCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,const ItemGrant& grant,std::span<const ItemInstanceId> ids)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto* entry=c.Find(grant.key);
    if(!entry || entry->base.retired)return std::unexpected(E::InvalidItem);
    if(!grant.count)return std::unexpected(E::InvalidChange);
    if(!SupportsPersistentGrant(c,*entry))return std::unexpected(E::UnsupportedState);
    const auto bags=InventoryBags(storages,owner.owner);
    if(bags.empty())return std::unexpected(E::InvalidStorage);
    const auto bagOf=[&](std::uint64_t id){return std::ranges::find_if(bags,[&](const ItemStorageLayout& l){return l.storage.id==id;});};
    const std::uint32_t stack=entry->base.maxStack ? entry->base.maxStack : 1;
    // The template of a new unit; each new item takes its own ID below.
    PersistentItem fresh{ItemInstanceId::Create(1).value(),grant.key,owner.owner};fresh.refinementLevel=grant.refinementLevel;
    if(entry->base.binding==BindingPolicy::OnAcquire)fresh.boundTo=owner.owner;
    if(const auto* accessory=std::get_if<ItemAccessoryRow>(&entry->category);accessory && accessory->socketProfile)
        fresh.accessory=AccessorySocketState{};
    if(const auto initial=InitialAutomaticRecoveryState(c,grant.key);initial)fresh.automatic=*initial;
    if(const auto initial=InitialToggleEffectState(c,grant.key);initial)fresh.toggle=*initial;fresh.bookSkill=grant.bookSkill;fresh.polymorph=grant.polymorph;
    if(!fresh.polymorph)if(const auto* poly=std::get_if<ItemPolymorphRow>(&entry->category);poly && poly->fixedForm)fresh.polymorph=PolymorphInstanceState{};
    fresh.tool=grant.tool;if(!fresh.tool && std::holds_alternative<ItemToolRow>(entry->category))fresh.tool=ToolInstanceState{};
    if(const auto* companion=std::get_if<ItemCompanionRow>(&entry->category);companion && PermanentSeal(*companion))
        fresh.companion=CompanionState{};
    const auto sameState=[&](const PersistentItem& i){
        return i.key==fresh.key && i.refinementLevel==fresh.refinementLevel && i.boundTo==fresh.boundTo &&
               i.attributes==fresh.attributes && i.stones==fresh.stones && i.accessory==fresh.accessory && i.automatic==fresh.automatic && i.toggle==fresh.toggle && i.bookSkill==fresh.bookSkill && i.polymorph==fresh.polymorph && i.tool==fresh.tool && i.companion==fresh.companion;};
    std::vector<const PersistentItem*> carried;
    for(const auto& i:items)
        if(!i.destroyed && i.owner==owner.owner && i.placement && i.placement->kind==ItemStorageKind::Inventory && bagOf(i.placement->storageId)!=bags.end())
            carried.push_back(&i);
    std::ranges::sort(carried,{},[](const PersistentItem* i){return std::pair{i->placement->storageId,i->placement->anchor};});
    std::vector<ItemChange> changes;std::uint32_t left=grant.count;
    // 1. Top up matching stacks.
    if(stack>1)
        for(const auto* i:carried)
        {
            if(!left)break;
            if(!sameState(*i) || i->count>=stack)continue;
            const auto add=std::min(left,stack-i->count);
            // Leave room in this atomic write for new stacks of the remainder.
            // Many almost-full stacks must not turn a small grant into an
            // oversized commit that the drop queue would discard as invalid.
            const auto newStacks=(std::uint64_t{left-add}+stack-1)/stack;
            if(changes.size()+1+newStacks>MaxItemCommitItems)continue;
            auto after=*i;after.revision++;after.count+=add;changes.push_back({*i,after});left-=add;
        }
    // 2. New items in the first free cells, bag by bag, cell by cell.
    InventoryCells taken;
    for(const auto* i:carried)
    {
        const auto cells=OccupiedItemCells(c,*i,*bagOf(i->placement->storageId));if(!cells)return std::unexpected(cells.error());
        for(const auto cell:*cells)taken.emplace(i->placement->storageId,cell);
    }
    std::size_t used=0;
    while(left)
    {
        if(changes.size()>=MaxItemCommitItems)return std::unexpected(E::InvalidPlacement);
        if(used>=ids.size())return std::unexpected(E::LimitExceeded);
        if(std::ranges::any_of(items,[&](const PersistentItem& i){return i.id==ids[used];}))return std::unexpected(E::InvalidChange);
        auto next=fresh;next.id=ids[used];next.count=std::min(left,stack);
        if(!PlaceFirst(c,next,bags,taken))return std::unexpected(E::InvalidPlacement);
        changes.push_back({std::nullopt,next});++used;left-=next.count;
    }
    ItemCommit r;r.operationId=operationId;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Grant;
    r.owners={owner};r.items=std::move(changes);r.outcomeCode=1;r.result={static_cast<std::uint8_t>(ItemOperationKind::Grant)};
    for(const auto& bag:bags)
        if(std::ranges::any_of(r.items,[&](const ItemChange& ch){return ch.after.placement->storageId==bag.storage.id;}))r.storages.push_back(bag);
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());
    return r;
}
std::expected<OwnerRefinement,OwnerRefinementError> MakeOwnerRefinement(const ItemCatalog& c,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,const OwnerRefinementRequest& request)
{
    if(std::ranges::all_of(request.operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    const auto* source=Placed(items,owner,request.source);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    const auto wallet=std::ranges::find_if(wallets,[&](const ItemWallet& w){return w.owner==owner.owner && w.currency==WalletCurrency::Gold;});
    if(wallet==wallets.end())return std::unexpected(E::InvalidWallet);
    // Equipment and tombstones are neither the source nor a material.
    std::vector<PersistentItem> carried;std::vector<RefinementItemSnapshot> inventory;
    for(const auto& i:items)
        if(!i.destroyed && i.owner==owner.owner && i.placement && i.placement->kind==ItemStorageKind::Inventory)
        {
            carried.push_back(i);
            inventory.push_back({i.id.Value(),i.revision,i.owner.id,i.key,i.refinementLevel,i.count,false});
        }
    auto plan=PrepareRefinement(c,request.recipeId,{request.operationId,owner.owner.id,wallet->revision,wallet->balance,request.admitted},
        {source->id.Value(),source->revision,source->owner.id,source->key,source->refinementLevel,source->count,false},inventory,request.recordedRoll);
    if(!plan)return std::unexpected(plan.error());
    auto commit=MakeRefinementCommit(c,*plan,owner,storages,carried,*wallet);
    if(!commit)return std::unexpected(commit.error());
    return OwnerRefinement{std::move(*plan),std::move(*commit)};
}
std::expected<ItemCommit,E> MakeUnequipCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operationId,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> inventory,ItemInstanceId id,ItemPlacement target)
{
    if(std::ranges::all_of(operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    if(target.kind!=ItemStorageKind::Inventory)return std::unexpected(E::UnsupportedState);
    const auto* source=Placed(inventory,owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::Equipment)return std::unexpected(E::UnsupportedState);
    auto taken=*source;taken.revision++;taken.placement=target;
    return Relocate(c,operationId,owner,storages,inventory,{{*source,taken}});
}
} // namespace auriga::item
