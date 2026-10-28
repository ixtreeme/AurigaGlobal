#include "CatalogSnapshotFixture.hpp"
#include <item/Costumes.hpp>
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
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture"){std::cout<<Hex(BuildItemCatalog(Fixture()).value()->ContentHash())<<'\n';return 0;}
        Lifetime();Equip();Publication();std::cout<<checks<<" costume checks passed\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
