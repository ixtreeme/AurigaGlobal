#include "CatalogSnapshotFixture.hpp"
#include <item/Costumes.hpp>
#include <item/EquipmentStats.hpp>
#include <item/TypedInventoryWire.hpp>
#include <item/AccessoryRuntime.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace db=auriga::item::sql;
namespace
{
unsigned checks=0;
void Check(bool value,const char* message,std::source_location loc=std::source_location::current())
{++checks;if(!value)throw std::runtime_error(std::string(message)+" line "+std::to_string(loc.line()));}
ItemKey Key(){return ItemKey::Create(ItemCategory::Costume,1).value();}
ItemCatalogSource Fixture()
{
    auto s=f::Catalog();auto base=s.items[3]; // Ordinary material base, distinct semantic prototype.
    base.key=Key();base.contentCode="costume.sword_mask";base.nameKey="item.sword_mask.name";
    base.descriptionKey="item.sword_mask.description";base.maxStack=1;base.usePolicy=1;base.refinementProfile=5;
    s.items.push_back(base);
    RefinementProfile profile{5,"refine.costume",RefinementKind::Costume,30,{}};
    for(std::uint16_t i=0;i<=30;++i)profile.levels.push_back({i,{},i==10 ? std::optional{2u} : std::nullopt});
    s.refinements.push_back(profile);
    s.costumes={{Key(),CostumeKind::Weapon,WeaponSubtype::Sword,CostumeClock::SinceFirstUse,60000}};
    for(std::uint8_t c=1;c<=4;++c)for(std::uint8_t sex=1;sex<=2;++sex)
        s.costumeVisuals.push_back({Key(),c,sex,"models/costume_"+std::to_string(c)+"_"+std::to_string(sex)+".model"});
    return s;
}
std::string Hex(const ItemCatalogDigest& digest)
{std::ostringstream out;for(auto v:digest)out<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(v);return out.str();}
CostumeEquipRequest Request()
{
    CostumeEquipRequest r{Key()};r.stackUnits=1;r.characterLevel=50;r.refinementLevel=10;
    r.characterClass=2;r.sex=2;r.equippedWeapon=WeaponSubtype::Sword;r.admittedByServer=true;r.time.nowUtcMs=1000;return r;
}
void Lifetime()
{
    const auto max=std::uint64_t{INT64_MAX};
    auto definition=Fixture().costumes[0];definition.kind=CostumeKind::Body;definition.weapon.reset();
    for(auto clock:{CostumeClock::Permanent,CostumeClock::SinceCreation,CostumeClock::SinceFirstUse,CostumeClock::EquippedTime})
    {
        definition.clock=clock;definition.durationMs=clock==CostumeClock::Permanent?0:100;
        auto state=MakeCostumeLifetime(definition,1000).value();
        const auto start=AdvanceCostumeLifetime(definition,state,{1050,50,false,true}).value();
        Check(!start.removeItem,"new item is alive");
        if(clock==CostumeClock::Permanent)
        {
            Check(start.after==CostumeLifetimeState{},"permanent has no hidden deadline");
            Check(!AdvanceCostumeLifetime(definition,start.after,{max,max,true,true})->removeItem,"permanent never expires");
        }
        else if(clock==CostumeClock::EquippedTime)
        {
            Check(start.after.remainingMs==100,"inventory time does not consume wear budget");
            const auto wear=AdvanceCostumeLifetime(definition,start.after,{1100,40,true,false}).value();
            Check(wear.after.remainingMs==60,"unequip edge accounts preceding equipped delta");
            const auto pause=AdvanceCostumeLifetime(definition,wear.after,{2000,900,false,true}).value();
            Check(pause.after.remainingMs==60,"re-equip never refills");
            const auto end=AdvanceCostumeLifetime(definition,pause.after,{2001,max,true,false}).value();
            Check(end.after.expired && end.after.remainingMs==0 && end.unequip && end.removeItem,"wear exhaustion saturates at zero");
            definition.durationMs=1;
            Check(AdvanceCostumeLifetime(definition,wear.after,{3000,0,false,false})->after.remainingMs==60,"catalog duration edit preserves issued budget");
        }
        else
        {
            const auto deadline=clock==CostumeClock::SinceCreation?1100u:1150u;
            Check(start.after.started && start.after.deadlineUtcMs==deadline,"correct creation or first-use deadline");
            Check(AdvanceCostumeLifetime(definition,start.after,{1060,10,true,true})->after.deadlineUtcMs==deadline,"equip cannot extend deadline");
            const auto end=AdvanceCostumeLifetime(definition,start.after,{deadline,0,true,false}).value();
            Check(end.removeItem && end.unequip && end.after.expired,"exact boundary expires and unequips");
            Check(AdvanceCostumeLifetime(definition,end.after,{1,0,false,true})->removeItem,"UTC rollback cannot revive committed expiry");
        }
        Check(MakeCostumeLifetime(definition,max+1).error()==CostumeLifetimeError::TimeOverflow,"invalid creation clock rejected");
        Check(AdvanceCostumeLifetime(definition,state,{max+1,0,false,false}).error()==CostumeLifetimeError::TimeOverflow,"invalid step UTC rejected");
        Check(AdvanceCostumeLifetime(definition,state,{0,max+1,false,false}).error()==CostumeLifetimeError::TimeOverflow,"invalid monotonic delta rejected");
    }
    definition.clock=CostumeClock::SinceCreation;definition.durationMs=100;
    Check(MakeCostumeLifetime(definition,max-99).error()==CostumeLifetimeError::TimeOverflow,"creation addition overflow");
    Check(!AdvanceCostumeLifetime(definition,{},{}),"creation timer cannot be loaded as unstarted");
    definition.clock=CostumeClock::SinceFirstUse;
    Check(!AdvanceCostumeLifetime(definition,{}, {max-99,0,false,true}),"first-use addition overflow");
    Check(!AdvanceCostumeLifetime(definition,{}, {1000,0,true,true}),"already equipped item cannot have an unstarted clock");
    Check(AdvanceCostumeLifetime(definition,{}, {max,0,false,false})->after==CostumeLifetimeState{},"unused first-use item has no deadline");
    for(auto bad: {CostumeLifetimeState{false,false,1000,0},CostumeLifetimeState{true,false,0,0},
                  CostumeLifetimeState{true,false,1000,1},CostumeLifetimeState{false,true,0,0}})
        Check(!AdvanceCostumeLifetime(definition,bad,{}),"malformed wall clock state rejected");
    definition.clock=CostumeClock::EquippedTime;
    Check(!AdvanceCostumeLifetime(definition,{false,false,0,100},{}),"wear timer starts at factory");
    Check(!AdvanceCostumeLifetime(definition,{true,true,0,1},{}),"expired wear budget cannot remain positive");
    definition.clock=CostumeClock::Permanent;definition.durationMs=0;
    Check(!AdvanceCostumeLifetime(definition,{true,false,1000,0},{}),"clock-kind conversion needs explicit instance migration");
}
void Equip()
{
    auto source=Fixture();auto catalog=BuildItemCatalog(source).value();auto r=Request();
    auto plan=PlanCostumeEquip(*catalog,r).value();
    Check(plan.equip && plan.slot==CostumeKind::Weapon && plan.refinementLevel==10,"weapon costume equips above +9");
    Check(plan.modelAsset=="models/costume_2_2.model" && plan.lifetime.after.deadlineUtcMs==61000,"class/sex visual and initial deadline");
    Check(plan.catalogHash==catalog->ContentHash() && plan.catalogRevision==1,"plan pins release");
    // The source starts first-use timers before checking the equipped weapon.
    // Its owner commits this clock outcome even if the equipment plan rejects.
    const auto used=AdvanceCostumeLifetime(source.costumes[0],{}, {1000,0,false,true}).value();
    auto denied=Request();denied.lifetime=used.after;denied.equippedWeapon.reset();
    Check(PlanCostumeEquip(*catalog,denied).error()==CostumeEquipError::WeaponRequired,"failed equip does not cancel separately committed first use");
    denied.equippedWeapon=WeaponSubtype::Sword;denied.time.nowUtcMs=2000;
    Check(PlanCostumeEquip(*catalog,denied)->lifetime.after.deadlineUtcMs==61000,"later successful equip preserves first attempt deadline");
    Check(catalog->RefinedBaseStats(Key(),30).value()==ItemBaseStats{},"costume never contributes raw weapon base");
    Check(catalog->BonusProfilesAt(Key(),10)->refinement->Data().id==2,"cumulative refinement bonus available");
    for(unsigned test=0;test<12;++test)
    {
        r=Request();auto expected=CostumeEquipError::Forbidden;
        switch(test)
        {
        case 0:r.admittedByServer=false;break;
        case 1:r.item=f::Sword();expected=CostumeEquipError::NotCostume;break;
        case 2:r.item=ItemKey::Create(4,999).value();expected=CostumeEquipError::MissingItem;break;
        case 3:r.stackUnits=0;expected=CostumeEquipError::InvalidStack;break;
        case 4:r.stackUnits=2;expected=CostumeEquipError::InvalidStack;break;
        case 5:r.characterClass=0;expected=CostumeEquipError::InvalidCharacter;break;
        case 6:r.sex=3;expected=CostumeEquipError::InvalidCharacter;break;
        case 7:r.refinementLevel=31;expected=CostumeEquipError::InvalidRefinement;break;
        case 8:r.equippedWeapon.reset();expected=CostumeEquipError::WeaponRequired;break;
        case 9:r.equippedWeapon=WeaponSubtype::Bow;expected=CostumeEquipError::WeaponMismatch;break;
        case 10:r.lifetime={true,false,0,0};expected=CostumeEquipError::InvalidLifetime;break;
        case 11:r.characterClass=5;expected=CostumeEquipError::InvalidCharacter;break;
        }
        Check(PlanCostumeEquip(*catalog,r).error()==expected,"invalid equip rejected");
    }
    for(unsigned test=0;test<5;++test)
    {
        auto s=source;r=Request();auto expected=CostumeEquipError::Forbidden;
        if(test==0)s.usePolicies[0].allowedClasses=1;
        if(test==1)s.usePolicies[0].allowedSexes=1;
        if(test==2){s.usePolicies[0].useDuringCombat=false;r.inCombat=true;}
        if(test==3){s.items.back().minimumLevel=51;expected=CostumeEquipError::LevelTooLow;}
        if(test==4){s.items.back().retired=true;expected=CostumeEquipError::RetiredItem;}
        Check(PlanCostumeEquip(*BuildItemCatalog(s).value(),r).error()==expected,"item policy checked server-side");
    }
    r=Request();r.lifetime=plan.lifetime.after;r.time={61000,0,true,true};
    auto expired=PlanCostumeEquip(*catalog,r).value();
    Check(!expired.equip && expired.modelAsset.empty() && expired.lifetime.removeItem && expired.lifetime.unequip,"expired equip returns commit-able removal");
    r.lifetime=expired.lifetime.after;r.time={1000,0,false,true};
    Check(!PlanCostumeEquip(*catalog,r)->equip,"expired latch survives clock rollback on equip");
    for(auto kind:{CostumeKind::Body,CostumeKind::Hair})
    {
        auto s=source;s.costumes[0].kind=kind;s.costumes[0].weapon.reset();r=Request();r.equippedWeapon.reset();
        Check(PlanCostumeEquip(*BuildItemCatalog(s).value(),r)->slot==kind,"body and hair need no real weapon");
    }
    catalog.reset();source={};Check(plan.modelAsset=="models/costume_2_2.model","plan owns model after catalog destruction");
}
void Publication()
{
    const auto s=Fixture();
    const auto bad=[&](auto change){auto copy=s;change(copy);Check(!BuildItemCatalog(std::move(copy)),"invalid costume cannot publish");};
    bad([](auto& s){s.costumes[0].kind=static_cast<CostumeKind>(4);});
    bad([](auto& s){s.costumes[0].weapon.reset();});
    bad([](auto& s){s.costumes[0].weapon=static_cast<WeaponSubtype>(8);});
    bad([](auto& s){s.costumes[0].kind=CostumeKind::Hair;});
    bad([](auto& s){s.costumes[0].clock=static_cast<CostumeClock>(0);});
    bad([](auto& s){s.costumes[0].clock=CostumeClock::Permanent;});
    bad([](auto& s){s.costumes[0].durationMs=0;});
    bad([](auto& s){s.costumes[0].durationMs=std::uint64_t{INT64_MAX}+1;});
    bad([](auto& s){s.costumes.push_back(s.costumes[0]);});bad([](auto& s){s.costumes.clear();});
    bad([](auto& s){s.items.pop_back();});bad([](auto& s){s.items.back().maxStack=2;});
    bad([](auto& s){s.items.back().refinementProfile=2;});bad([](auto& s){s.refinements.back().levels[1].gain.physicalAttackRaw=1;});
    bad([](auto& s){s.costumeVisuals.pop_back();});bad([](auto& s){s.costumeVisuals.push_back(s.costumeVisuals[0]);});
    bad([](auto& s){s.costumeVisuals[0].key=f::Sword();});bad([](auto& s){s.costumeVisuals[0].characterClass=0;});
    bad([](auto& s){s.costumeVisuals[0].sex=3;});bad([](auto& s){s.costumeVisuals[0].modelAsset="../escape.model";});
    auto narrow=s;narrow.usePolicies[0].allowedClasses=1;narrow.usePolicies[0].allowedSexes=1;
    narrow.costumeVisuals.erase(narrow.costumeVisuals.begin()+1,narrow.costumeVisuals.end());Check(BuildItemCatalog(narrow).has_value(),"only permitted visual combinations required");
    narrow.costumeVisuals=s.costumeVisuals;Check(BuildItemCatalog(narrow).has_value(),"extra valid variants survive policy tightening");
    const auto original=BuildItemCatalog(s).value();auto shuffled=s;std::ranges::reverse(shuffled.costumeVisuals);
    Check(BuildItemCatalog(shuffled).value()->ContentHash()==original->ContentHash(),"canonical visual ordering");
    shuffled=s;shuffled.costumeVisuals[0].modelAsset="models/changed.model";
    Check(BuildItemCatalog(shuffled).value()->ContentHash()!=original->ContentHash(),"visual contributes to digest");
    shuffled=s;shuffled.costumes[0].durationMs+=1;
    Check(BuildItemCatalog(shuffled).value()->ContentHash()!=original->ContentHash(),"lifetime contributes to digest");
    auto snapshot=catalog_snapshot_fixture::Snapshot(s);
    auto loaded=db::DecodeSnapshot(snapshot,{1,original->ContentHash()}).value();
    Check(loaded->FindCostumeVisual(Key(),2,2)->modelAsset=="models/costume_2_2.model","SQL snapshot roundtrip");
    snapshot[db::Table::Costumes][0][3]="257";
    Check(!db::DecodeSnapshot(snapshot,{1,{}}),"SQL weapon enum cannot truncate to valid u8");
    auto body=s;body.costumes[0].kind=CostumeKind::Body;body.costumes[0].weapon.reset();
    Check(db::DecodeSnapshot(catalog_snapshot_fixture::Snapshot(body),{1,{}}).has_value(),"SQL NULL weapon is explicit");
}
}
void Persistence()
{
    for(unsigned kind=1;kind<=3;++kind)for(unsigned clock=1;clock<=4;++clock) {
        auto source=Fixture();auto& row=source.costumes[0];row.kind=static_cast<CostumeKind>(kind);row.weapon=kind==3 ? std::optional{WeaponSubtype::Sword} : std::nullopt;
        row.clock=static_cast<CostumeClock>(clock);row.durationMs=clock==1 ? 0 : 10000;
        const auto c=BuildItemCatalog(source).value();const ItemOwner owner{ItemOwnerKind::Character,7};const ItemOwnerFence fence{owner,1,1};
        const std::vector<ItemStorageLayout> layouts{{{owner,ItemStorageKind::Inventory,1},5,9,2},{{owner,ItemStorageKind::Equipment,1},11,1,1}};
        const auto id=ItemInstanceId::Create(100).value();const std::array ids{id};
        const auto grant=MakeGrantCommit(*c,{1,1},fence,layouts,{},ItemGrant{Key(),10,1,{},{},{},1000},ids);
        Check(grant.has_value(),"all costume kinds/clocks grant durable state");auto costume=grant->items[0].after;
        if(clock==3) {
            auto birth=*grant;birth.kind=ItemOperationKind::Service;birth.outcomeCode=0x1201;birth.result={'B',1};
            birth.items[0].after.placement=ItemPlacement{ItemStorageKind::Equipment,1,kind+7};birth.storages={layouts[1]};
            Check(!ValidateItemCommit(*c,birth),"born-worn first-use costume cannot retain an unlimited unstarted clock");
            birth.items[0].after.costume=AdvanceItemClock(*FindWearableClock(*c,Key()),*costume.costume,{1000,0,false,true})->after;
            Check(ValidateItemCommit(*c,birth).has_value(),"born-worn starter costume has a checked first-use deadline");
            birth.items[0].after.costume->remainingMs=1;Check(!ValidateItemCommit(*c,birth),"born-worn first-use clock rejects malformed state");
        }
        Check(DecodeCostumeLifetime(EncodeCostumeLifetime(*costume.costume))==costume.costume,"versioned costume clock roundtrip");
        EquipContext who{50,0,0,2000};std::vector<PersistentItem> items{costume};
        auto denied=who;denied.characterClass=4;Check(!MakeEquipCommit(*c,{2,9},fence,layouts,items,id,denied),"unadmitted character cannot start or change a costume clock");
        if(kind==3) {
            auto missing=MakeEquipCommit(*c,{2,1},fence,layouts,items,id,who);
            if(clock==3){Check(missing && missing->items[0].after.placement==costume.placement && missing->items[0].after.costume->deadlineUtcMs==12000,"admitted missing-weapon attempt durably starts first-use clock");costume=missing->items[0].after;items[0]=costume;}
            else Check(!missing,"missing real weapon cannot equip a costume");
        }
        const auto wid=ItemInstanceId::Create(101).value();const std::array weaponIds{wid};
        const auto weaponGrant=MakeGrantCommit(*c,{1,2},fence,layouts,items,ItemGrant{f::Sword(),0,1},weaponIds).value();items.push_back(weaponGrant.items[0].after);
        auto real=MakeEquipCommit(*c,{2,2},fence,layouts,items,wid,who).value();items[1]=real.items[0].after;
        who.nowUtcMs=3000;auto equipped=MakeEquipCommit(*c,{2,3},fence,layouts,items,id,who);
        Check(equipped && equipped->items[0].after.placement->anchor==kind+7,"costume uses its own equipment slot");items[0]=equipped->items[0].after;
        const auto stats=BuildEquipmentStats(*c,owner,items);Check(stats && stats->Bonus(BonusId::CritChance)==100,"costume refinement bonuses enter equipped stats once");
        const auto bytes=EncodeTypedInventory(*c,layouts,{},items);Check(bytes && std::to_integer<unsigned>((*bytes)[5])==13,"costume selects inventory version 13");
        const auto decoded=DecodeTypedInventory(*bytes,owner);Check(decoded && decoded->items[0].costume==items[0].costume && decoded->costumeRules.at(id).kind==row.kind,"inventory preserves owned costume clock and rules");
        auto malformed=*bytes;malformed.pop_back();Check(!DecodeTypedInventory(malformed,owner),"truncated costume sidecar is rejected");
        auto tampered=*equipped;tampered.items[0].after.costume->remainingMs+=1;Check(!ValidateItemCommit(*c,tampered),"clock cannot be altered behind a costume receipt");
        const std::array elapsed{CostumeElapsed{id.Value(),clock==4 ? 400u : 0u}};who.costumeElapsed=elapsed;who.nowUtcMs=4000;
        const auto unequipped=MakeUnequipCommit(*c,{2,4},fence,layouts,items,id,{ItemStorageKind::Inventory,1,0},who);
        Check(unequipped && unequipped->items[0].after.placement->kind==ItemStorageKind::Inventory,"costume can be taken off through the same move handler");items[0]=unequipped->items[0].after;
        if(clock==4)Check(items[0].costume->remainingMs==9600,"unequip settles the preceding worn interval");
        who.costumeElapsed={};who.nowUtcMs=5000;
        const auto again=MakeEquipCommit(*c,{2,5},fence,layouts,items,id,who);Check(again.has_value(),"costume re-equip preserves its issued clock");items[0]=again->items[0].after;
        if(clock==4)Check(items[0].costume->remainingMs==9600,"paused wall time never refills or spends wear time");
        const auto expired=MakeCostumeClockCommit(*c,{2,6},fence,layouts,items,id,500000,clock==4 ? 9600 : 0);
        if(clock==1)Check(!expired,"permanent costume has no maintenance write");
        else Check(expired && expired->items[0].after.destroyed && expired->items[0].after.costume->expired && !expired->items[0].after.placement && expired->items[0].after.count==0,"expiry keeps an audited tombstone and frees equipment atomically");
        // An ordinary equipment timer may settle with the costume operation.
        PersistentItem accessory{ItemInstanceId::Create(102).value(),f::Bracelet(),owner};accessory.accessory=AccessorySocketState{1,1,1000,false};accessory.placement=ItemPlacement{ItemStorageKind::Equipment,1,5};
        auto compound=*again;auto before=items;before[0]=*again->items[0].before;before.push_back(accessory);
        Check(SettleAccessoryTime(*c,compound,fence,layouts,before,100).has_value(),"costume receipt permits ancillary accessory clock debit");
        compound.items.back().after.refinementLevel=1;Check(!ValidateItemCommit(*c,compound),"ancillary debit cannot conceal accessory refinement");
    }
}
void CompanionSkins()
{
    for(auto kind:{CostumeKind::PetSkin,CostumeKind::MountSkin})for(unsigned clock=1;clock<=4;++clock) {
        auto source=Fixture();auto& row=source.costumes[0];row.kind=kind;row.weapon.reset();row.companionForm=1;
        row.clock=static_cast<CostumeClock>(clock);row.durationMs=clock==1 ? 0 : 10000;
        source.costumeVisuals.clear();source.companionForms.push_back({1,"companion.skin",34106,"Metin2RacePrefabs/34106"});
        const auto c=BuildItemCatalog(source).value();const ItemOwner owner{ItemOwnerKind::Character,7};const ItemOwnerFence fence{owner,1,1};
        const std::vector<ItemStorageLayout> layouts{{{owner,ItemStorageKind::Inventory,1},5,9,2},{{owner,ItemStorageKind::Equipment,1},EquipmentSlotCount,1,1}};
        const auto id=ItemInstanceId::Create(100).value();const std::array ids{id};
        auto grant=MakeGrantCommit(*c,{5,1},fence,layouts,{},ItemGrant{Key(),10,1,{},{},{},1000},ids).value();
        std::vector<PersistentItem> items{grant.items[0].after};
        Check(!WornCompanionCostumeForm(*c,items,kind),"bag skin never overrides a companion");
        auto equip=MakeEquipCommit(*c,{5,2},fence,layouts,items,id,EquipContext{50,0,0,2000});
        Check(equip && ValidateItemCommit(*c,*equip),"skin equips without a weapon or active pet");items[0]=equip->items[0].after;
        Check(items[0].placement->anchor==CostumeSlot(kind)-1 && CostumeSlot(kind)>=25,"skin has a distinct appended slot");
        Check(WornCompanionCostumeForm(*c,items,kind)==1,"only the matching worn skin supplies a form");
        Check(!WornCompanionCostumeForm(*c,items,kind==CostumeKind::PetSkin ? CostumeKind::MountSkin : CostumeKind::PetSkin),"pet skin cannot override a mount and vice versa");
        Check(BuildEquipmentStats(*c,owner,items)->Bonus(BonusId::CritChance)==100,"skin grants its own bonuses without a summon");
        const auto bytes=EncodeTypedInventory(*c,layouts,{},items).value();const auto decoded=DecodeTypedInventory(bytes,owner);
        Check(std::to_integer<unsigned>(bytes[5])==19 && decoded && decoded->costumeRules.at(id).companionForm==1,"v19 roundtrip preserves form and slot");
        for(std::size_t size=0;size<bytes.size();++size)Check(!DecodeTypedInventory(std::span(bytes).first(size),owner),"truncated v19 rejected");
        auto bad=source;bad.costumes[0].companionForm=999;Check(!BuildItemCatalog(bad),"missing skin form rejected");
        bad=source;bad.costumeVisuals.push_back({Key(),1,1,"models/wrong.model"});Check(!BuildItemCatalog(bad),"skin cannot replace character geometry");
        const auto expired=MakeCostumeClockCommit(*c,{5,3},fence,layouts,items,id,500000,clock==4 ? 10000 : 0);
        if(clock!=1){Check(expired && ValidateItemCommit(*c,*expired),"skin expiry is a validated durable operation");items[0]=expired->items[0].after;Check(!WornCompanionCostumeForm(*c,items,kind) && BuildEquipmentStats(*c,owner,items)->Bonus(BonusId::CritChance)==0,"expiry removes form and bonuses together");}
    }
}
void BodyEffects()
{
    for(unsigned clock=1;clock<=4;++clock) {
        auto source=Fixture();auto& row=source.costumes[0];row.kind=CostumeKind::BodyEffect;row.weapon.reset();row.bodyEffect=794;
        row.clock=static_cast<CostumeClock>(clock);row.durationMs=clock==1 ? 0 : 10000;source.costumeVisuals.clear();
        const auto bodyKey=ItemKey::Create(ItemCategory::Costume,2).value();auto bodyBase=source.items.back();bodyBase.key=bodyKey;bodyBase.contentCode="costume.body";bodyBase.nameKey="item.body.name";bodyBase.descriptionKey="item.body.description";source.items.push_back(bodyBase);
        source.costumes.push_back({bodyKey,CostumeKind::Body,{},CostumeClock::Permanent,0});
        for(std::uint8_t cc=1;cc<=4;++cc)for(std::uint8_t sex=1;sex<=2;++sex)source.costumeVisuals.push_back({bodyKey,cc,sex,"models/body.model"});
        const auto c=BuildItemCatalog(source).value();const ItemOwner owner{ItemOwnerKind::Character,7};const ItemOwnerFence fence{owner,1,1};
        const std::vector<ItemStorageLayout> layouts{{{owner,ItemStorageKind::Inventory,1},5,9,2},{{owner,ItemStorageKind::Equipment,1},EquipmentSlotCount,1,1}};
        const auto id=ItemInstanceId::Create(100).value();const std::array ids{id};
        const auto grant=MakeGrantCommit(*c,{6,1},fence,layouts,{},ItemGrant{Key(),10,1,{},{},{},1000},ids).value();
        std::vector<PersistentItem> items{grant.items[0].after};Check(!WornBodyCostumeEffect(*c,items),"bag effect is invisible");
        const auto equipped=MakeEquipCommit(*c,{6,2},fence,layouts,items,id,{50,0,0,2000}).value();items[0]=equipped.items[0].after;
        Check(ValidateItemCommit(*c,equipped) && items[0].placement->anchor==26,"effect uses appended slot 27");
        Check(!WornBodyCostumeEffect(*c,items) && BuildEquipmentStats(*c,owner,items)->Bonus(BonusId::CritChance)==100,"visual needs armor; intrinsic bonuses do not");
        PersistentItem armor{ItemInstanceId::Create(101).value(),f::Armor(),owner};armor.placement=ItemPlacement{ItemStorageKind::Equipment,1,1};items.push_back(armor);
        Check(WornBodyCostumeEffect(*c,items)==794,"body armor enables the exact authored effect");
        items.pop_back();PersistentItem body{ItemInstanceId::Create(102).value(),bodyKey,owner};body.costume=CostumeLifetimeState{};body.placement=ItemPlacement{ItemStorageKind::Equipment,1,8};items.push_back(body);
        Check(WornBodyCostumeEffect(*c,items)==794,"body costume alone also enables the effect");items.back().costume->expired=true;Check(!WornBodyCostumeEffect(*c,items),"expired body costume cannot anchor an effect");items.back()=armor;
        const auto bytes=EncodeTypedInventory(*c,layouts,{},items).value();const auto decoded=DecodeTypedInventory(bytes,owner);
        Check(std::to_integer<unsigned>(bytes[5])==20 && decoded && decoded->costumeRules.at(id).bodyEffect==794,"v20 preserves effect and lifetime");
        for(std::size_t cut=0;cut<bytes.size();++cut)Check(!DecodeTypedInventory(std::span(bytes).first(cut),owner),"truncated v20 rejected");
        auto bad=source;bad.costumes[0].bodyEffect=0;Check(!BuildItemCatalog(bad),"effect ID is mandatory");
        bad=source;bad.costumes[0].kind=CostumeKind::Body;Check(!BuildItemCatalog(bad),"ordinary costume cannot smuggle an effect ID");
        const auto expired=MakeCostumeClockCommit(*c,{6,3},fence,layouts,items,id,500000,clock==4 ? 10000 : 0);
        if(clock!=1){Check(expired && ValidateItemCommit(*c,*expired),"effect expiry is durable");items[0]=expired->items[0].after;Check(!WornBodyCostumeEffect(*c,items),"expiry clears effect even while armor remains worn");}
    }
    CostumeAppearance p{77,1};p.bodyEffect=794;
    const auto v3=EncodeCostumeAppearance(p,3).value();Check(v3[5]==3 && v3.size()==35 && v3[33]==26 && v3[34]==3,"shared GC241 v3 encoder appends little endian effect ID");
    Check(EncodeCostumeAppearance(p,1)->size()==30 && EncodeCostumeAppearance(p,2)->size()==33,"old observer formats remain available");
    p.models[0]="../escape";Check(!EncodeCostumeAppearance(p),"appearance rejects unsafe asset paths");
    p.models[0]="";p.models[4]="Metin2Equipment/w10";Check(!EncodeCostumeAppearance(p),"weapon costume requires a real weapon");
}
void WeaponEffects()
{
    for(unsigned subtype=1;subtype<=7;++subtype)for(unsigned clock=1;clock<=4;++clock) {
        auto source=Fixture();auto& row=source.costumes[0];row.kind=CostumeKind::WeaponEffect;row.weapon.reset();row.weaponEffects={11,12,13,14,15,16,112,116};
        row.clock=static_cast<CostumeClock>(clock);row.durationMs=clock==1 ? 0 : 10000;source.costumeVisuals.clear();
        source.weapons[0].subtype=static_cast<WeaponSubtype>(subtype);if(subtype==3)source.weapons[0].ammoPolicy=2;
        ItemDefinitionRules variant{f::Sword()};variant.level=1;variant.weaponEffectAlternate=subtype==2 || subtype==6;source.definitionRules.push_back(variant);
        const auto c=BuildItemCatalog(source).value();const ItemOwner owner{ItemOwnerKind::Character,7};const ItemOwnerFence fence{owner,1,1};
        const std::vector<ItemStorageLayout> layouts{{{owner,ItemStorageKind::Inventory,1},5,9,2},{{owner,ItemStorageKind::Equipment,1},EquipmentSlotCount,1,1}};
        const auto id=ItemInstanceId::Create(100).value();const std::array ids{id};
        auto grant=MakeGrantCommit(*c,{7,1},fence,layouts,{},ItemGrant{Key(),10,1,{},{},{},1000},ids).value();std::vector<PersistentItem> items{grant.items[0].after};
        const auto equipped=MakeEquipCommit(*c,{7,2},fence,layouts,items,id,{50,0,0,2000}).value();items[0]=equipped.items[0].after;
        Check(ValidateItemCommit(*c,equipped) && items[0].placement->anchor==27,"weapon effect equips in slot 28 without requiring a real weapon");
        Check(!WornWeaponCostumeEffect(*c,items),"weaponless actor has no weapon effect");
        PersistentItem weapon{ItemInstanceId::Create(101).value(),f::Sword(),owner};weapon.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};items.push_back(weapon);
        Check(WornWeaponCostumeEffect(*c,items)==(subtype<=6 ? subtype+10 : 0),"explicit subtype selects exact normal effect; mount spear has none");
        items.back().refinementLevel=1;
        Check(WornWeaponCostumeEffect(*c,items)==(subtype==2 ? 112 : subtype==6 ? 116 : subtype<=6 ? subtype+10 : 0),"alternate shape is an exact per-level native policy");
        const auto bytes=EncodeTypedInventory(*c,layouts,{},items).value();const auto decoded=DecodeTypedInventory(bytes,owner);
        Check(std::to_integer<unsigned>(bytes[5])==21 && decoded && decoded->costumeRules.at(id).weaponEffects==row.weaponEffects,"v21 preserves all normal and alternate effect choices");
        for(std::size_t cut=0;cut<bytes.size();++cut)Check(!DecodeTypedInventory(std::span(bytes).first(cut),owner),"truncated v21 rejected");
        auto bad=source;bad.costumes[0].weaponEffects[6]=0;Check(!BuildItemCatalog(bad),"incomplete alternate effect set cannot publish");
        bad=source;bad.costumes[0].kind=CostumeKind::Body;Check(!BuildItemCatalog(bad),"other costumes cannot carry hidden weapon effects");
        const auto expired=MakeCostumeClockCommit(*c,{7,3},fence,layouts,items,id,500000,clock==4 ? 10000 : 0);
        if(clock!=1){Check(expired && ValidateItemCommit(*c,*expired),"weapon effect expiry is durable");items[0]=expired->items[0].after;Check(!WornWeaponCostumeEffect(*c,items),"expiry clears the effect while real weapon remains");}
    }
    auto bad=Fixture();ItemDefinitionRules rule{f::Sword()};rule.weaponEffectAlternate=true;bad.definitionRules.push_back(rule);Check(!BuildItemCatalog(bad),"alternate policy is forbidden on non-dagger/fan weapons");
    CostumeAppearance p{77,1};p.weapon=2;p.weaponEffect=579;const auto wire=EncodeCostumeAppearance(p,4).value();
    Check(wire[5]==4 && wire.size()==37 && wire[35]==67 && wire[36]==2,"GC241 v4 carries the exact weapon effect");p.weapon=0;Check(!EncodeCostumeAppearance(p),"weaponless appearance cannot carry a weapon effect");
}
#include "UniqueLifetimeCases.hpp"
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture"){std::cout<<Hex(BuildItemCatalog(Fixture()).value()->ContentHash())<<'\n';return 0;}
        Lifetime();Equip();Publication();Persistence();CompanionSkins();BodyEffects();WeaponEffects();UniqueLifetimes();std::cout<<checks<<" costume checks passed\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
