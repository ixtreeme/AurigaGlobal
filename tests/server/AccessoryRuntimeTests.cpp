#include "PersistentItemFixture.hpp"
#include <item/AccessoryRuntime.hpp>
#include <item/EquipmentStats.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;namespace f=persistent_item_fixture;namespace cf=item_catalog_fixture;
namespace
{
unsigned checks=0;
void Check(bool good,const char* why,std::source_location at=std::source_location::current()){++checks;if(!good)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    auto definition=f::Catalog();auto& p=definition.accessorySockets[0];p.unlockUnitsPerAttempt=2;p.chargeUnitsPerAttempt=3;
    for(unsigned i=1;i<=3;++i){p.grades[i].durationMs=i*1000;p.grades[i-1].chargeChanceBp=5000;}
    const auto c=BuildItemCatalog(definition).value();const ItemOwnerFence fence{f::Character(),7,2};const SocketMaintenanceActor actor{100,1,1,false};
    auto target=f::Leather(70);target.key=cf::Bracelet();target.count=1;target.refinementLevel=15;target.placement->anchor=0;target.accessory=AccessorySocketState{};
    auto source=f::Leather(71);source.key=cf::SocketDrill();source.count=9;source.placement->anchor=1;
    std::vector<PersistentItem> items{target,source};std::vector layouts{f::Inventory()};
    const auto make=[&](AccessoryServiceKind kind,std::uint16_t roll){return MakeAccessorySocketCommit(*c,{17},fence,layouts,items,source.id,target.id,actor,kind,roll);};
    Check(AccessoryMaterial(*c,cf::SocketDrill()) && AccessoryMaterial(*c,cf::SocketJewel()) && !AccessoryMaterial(*c,cf::Leather()),"only explicitly listed materials expose accessory action");
    auto success=make(AccessoryServiceKind::Unlock,5000),failure=make(AccessoryServiceKind::Unlock,5001);
    Check(success && failure && success->succeeded && !failure->succeeded && success->quote==failure->quote,"unlock boundary has outcome-independent exact quote");
    Check(success->commit.items[0].after.count==7 && failure->commit.items[0].after.count==7 && success->commit.items[1].after.accessory->unlockedCapacity==1 && failure->commit.items[1].after.accessory->unlockedCapacity==0,"both attempts consume profile units, only success opens capacity");
    Check(success->commit.items[1].after.refinementLevel==15 && success->commit.wallets.empty() && success->commit.items[1].after.id==target.id,"refinement, identity and wallet preserved");
    Check(ItemCommitDigest(*c,success->commit)!=ItemCommitDigest(*c,failure->commit),"receipt binds recorded draw and outcome");
    Check(!make(AccessoryServiceKind::Unlock,0) && !make(AccessoryServiceKind::Unlock,10001),"missing or invalid draw refused");
    items[0].accessory->unlockedCapacity=3;Check(make(AccessoryServiceKind::Unlock,0).error()==SocketMaintenanceStatus::NoEffect,"full capacity consumes nothing");
    items[1].key=cf::SocketJewel();auto charge=make(AccessoryServiceKind::Charge,5000);Check(charge && charge->quote.kind==6 && charge->quote.units==3 && charge->commit.items[0].after.count==6 && charge->commit.items[1].after.accessory==AccessorySocketState{3,1,1000,false},"timed charge uses exact new-grade duration");
    const auto golden=EncodeSocketMaintenance(*c,0x123456789abcdefULL,&items[1],&items[0],2,SocketMaintenanceStatus::Ready,charge->quote,std::array{target.id.Value()});Check(golden && std::to_integer<unsigned>((*golden)[5])==3,"accessory quote uses GC235 v3");
    items[0].accessory={3,1,250,false};auto next=make(AccessoryServiceKind::Charge,1);Check(next && next->commit.items[1].after.accessory==AccessorySocketState{3,2,2000,false},"higher charge replaces rather than adds duration");
    items[1].count=2;Check(!make(AccessoryServiceKind::Charge,1),"insufficient material never debits");items[1].count=9;
    items[0].accessory={0,0,0,false};Check(make(AccessoryServiceKind::Charge,1).error()==SocketMaintenanceStatus::NoEffect,"charge requires previously unlocked capacity");
    items[0].accessory={3,2,250,false};items[0].placement={ItemStorageKind::Equipment,1,5};
    const auto time=AdvanceAccessorySocketTime(*c->FindAccessorySocket(1),*items[0].accessory,300,true);Check(time && time->state==AccessorySocketState{3,1,950,false},"expiry carries elapsed remainder into lower-grade duration");
    auto projected=items;Check(ProjectAccessoryTime(*c,projected,UINT64_MAX).has_value() && projected[0].accessory==AccessorySocketState{3,0,0,false},"long elapsed safely expires all grades");
    const auto before=BuildEquipmentStats(*c,fence.owner,items);Check(before && before->flat.physicalDefenseRaw==11000,"base and refinement 9000 boosted once by grade-two minimum 2000");
    Check(BuildEquipmentStats(*c,fence.owner,projected)->flat.physicalDefenseRaw==9000,"expired grade removes entire boost");
    auto bonusSource=definition;auto& bonus=*std::ranges::find(bonusSource.bonuses,6,&BonusProfile::id);
    bonus.entries.push_back({BonusId::Strength,2});bonus.entries.push_back({BonusId::MovementSpeedPercent,50});bonus.entries.push_back({BonusId::MagicalDefenseFlat,-50});bonus.entries.push_back({BonusId::PhysicalAttackFlat,0});
    bonusSource.attributeProfiles[0].groups[0].excludeStatic=false;bonusSource.attributeBindings.push_back({cf::Bracelet(),1});const auto bonusCatalog=BuildItemCatalog(bonusSource).value();auto withInstance=items[0];withInstance.attributes.slots[0]=BonusEntry{BonusId::PhysicalDefenseFlat,100};
    const auto boosted=BuildEquipmentStats(*bonusCatalog,fence.owner,std::span(&withInstance,1));
    Check(boosted && boosted->flat.physicalDefenseRaw==11100,"instance bonuses are added after boosting and never multiplied");
    Check(boosted->Bonus(BonusId::Strength)==4 && boosted->Bonus(BonusId::MovementSpeedPercent)==250,"integer and basis-point minima apply in their own units");
    Check(boosted->flat.magicalDefenseRaw==-2050 && boosted->flat.physicalAttackRaw==0,"boost preserves negative sign and never creates a bonus from zero");
    const ItemStorageLayout equipment{{fence.owner,ItemStorageKind::Equipment,1},8,1,1};layouts.push_back(equipment);
    auto commit=MakeConsumeCommit(*c,{17,1},fence,layouts,items,source.id,1).value();
    Check(SettleAccessoryTime(*c,commit,fence,layouts,items,300).has_value() && commit.items.size()==2 && commit.items[1].before==items[0] && commit.items[1].after.accessory==time->state,"elapsed time joins another economic operation atomically");
    Check(commit.items[1].after.revision==items[0].revision+1 && ValidateItemCommit(*c,commit).has_value(),"timer adds a single CAS revision and equipment layout");
    auto moved=commit;moved.items[1].after.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};Check(ValidateItemCommit(*c,moved).has_value(),"unequip retains fully settled state");
    auto direct=moved;direct.items.pop_back();auto after=items[0];++after.revision;after.placement={ItemStorageKind::Inventory,1,0};direct.items.push_back({items[0],after});
    Check(SettleAccessoryTime(*c,direct,fence,layouts,items,300).has_value() && direct.items[1].after.revision==2 && direct.items[1].after.accessory==time->state,"settling a moved item never increments its revision twice");
    auto bag=items;bag[0].placement={ItemStorageKind::Inventory,1,0};Check(ProjectAccessoryTime(*c,bag,UINT64_MAX).has_value() && bag[0].accessory==items[0].accessory,"inventory time is paused");
    auto stale=commit;stale.items[1].before->revision++;Check(!SettleAccessoryTime(*c,stale,fence,layouts,items,100),"mismatched before image refused");
    auto full=items;full[0].accessory={3,3,3000,false};Check(!MakeAccessorySocketCommit(*c,{17,2},fence,layouts,full,source.id,target.id,actor,AccessoryServiceKind::Charge,1),"equipped target is never serviced");
    auto bound=definition;const auto permanent=ItemKey::Create(6,4).value();auto base=*std::ranges::find(bound.items,cf::SocketJewel(),&ItemBaseRow::key);base.key=permanent;base.contentCode="material.permanent_fixture";bound.items.push_back(base);bound.materials.push_back({permanent,MaterialKind::Jewel});bound.accessorySockets[0].permanentChargeMaterials={permanent};
    const auto pc=BuildItemCatalog(bound).value();items[0]=target;items[0].accessory={3,0,0,false};items[1].key=permanent;
    auto permanentPlan=MakeAccessorySocketCommit(*pc,{17,3},fence,layouts,items,source.id,target.id,actor,AccessoryServiceKind::Charge,1);Check(permanentPlan && permanentPlan->commit.items[1].after.accessory==AccessorySocketState{3,1,0,true},"explicit permanent material creates permanent grade");
    auto active=permanentPlan->commit.items[1].after;active.placement={ItemStorageKind::Equipment,1,5};Check(ProjectAccessoryTime(*pc,std::span(&active,1),UINT64_MAX).has_value() && active.accessory->activeGrade==1,"permanent grades never consume time");
    items[0].accessory={3,1,0,true};items[1].key=cf::SocketJewel();Check(MakeAccessorySocketCommit(*pc,{17,4},fence,layouts,items,source.id,target.id,actor,AccessoryServiceKind::Charge,1).error()==SocketMaintenanceStatus::Incompatible,"timed and permanent charges cannot be mixed");
    auto corrupt=items;corrupt[0].accessory={3,1,0,false};Check(!MakeAccessorySocketCommit(*pc,{17,5},fence,layouts,corrupt,source.id,target.id,actor,AccessoryServiceKind::Charge,1),"corrupt duration rejected before consume");
    return *golden;
}
}
int main(int argc,char** argv){try{auto bytes=Run();if(argc==3 && std::string_view(argv[1])=="--write-golden"){std::ofstream out(argv[2]);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write");}std::cout<<checks<<" accessory runtime checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
