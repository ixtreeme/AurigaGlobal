#include "AmmoRuntimeFixture.hpp"
#include <item/TypedInventoryWire.hpp>
#include <item/TypedItemDetails.hpp>
#include <item/EquipmentStats.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;namespace f=ammo_runtime_fixture;
namespace {
unsigned checks=0;void Check(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
ItemOwner owner{ItemOwnerKind::Character,31};ItemOwnerFence fence{owner,1,1};
std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},10,9,1},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},20,1,1}};std::vector<PersistentItem> all;
void Apply(const ItemCommit& c){for(const auto& ch:c.items)*std::ranges::find(all,ch.after.id,&PersistentItem::id)=ch.after;++fence.revision;}
void Run(const std::filesystem::path& golden){const auto source=f::Catalog();auto catalog=BuildItemCatalog(source);Check(catalog.has_value(),"ammo catalog");const auto& c=**catalog;
    auto bad=source;bad.ammunition[0].ammoPolicy=1;Check(!BuildItemCatalog(bad),"ammunition cannot claim a non-consuming policy");bad=source;bad.ammunition.push_back(bad.ammunition[0]);Check(!BuildItemCatalog(bad),"duplicate ammunition");
    for(unsigned i=1;i<=5;++i){if(i==3)continue;ItemGrant g{i==1?source.weapons[0].key:i==5?soul_runtime_fixture::Key(1):f::Ammo(i==4?2:1)};g.count=i==2?5:i==4?4:1;std::vector ids{ItemInstanceId::Create(i).value()};if(i==2)ids.push_back(ItemInstanceId::Create(3).value());const auto grant=MakeGrantCommit(c,{static_cast<std::uint8_t>(i)},fence,layouts,all,g,ids);Check(grant.has_value(),"real ammunition factory");for(const auto& ch:grant->items)all.push_back(ch.after);++fence.revision;}
    EquipContext actor{120,1,0};const auto equip=[&](unsigned i){auto p=MakeEquipCommit(c,{static_cast<std::uint8_t>(fence.revision)},fence,layouts,all,all[i-1].id,actor);if(!p)throw std::runtime_error("equip index="+std::to_string(i)+" error="+std::to_string(unsigned(p.error())));Check(true,"ammo and bow equip");Apply(*p);};
    equip(1);Check(BuildEquipmentStats(c,owner,all)->weapon==WeaponSubtype::Bow && !FindBowAmmo(c,owner,all),"bow stats without free arrows");equip(4);Check(!FindBowAmmo(c,owner,all),"different policy is not interchangeable");equip(2);Check(FindBowAmmo(c,owner,all)->count==3,"compatible equipped stack");
    SoulContext context;context.actor=actor;context.event=31;context.target=100;context.targetRevision=1;context.damage=10001;context.targetIsNpc=true;
    auto charge=MakeSoulCommit(c,{20},fence,layouts,all,all[4].id,{},SoulAction::Recharge,30000,0,{});Check(charge.has_value(),"soul charge");Apply(charge->commit);SoulContext on;on.actor.level=120;auto active=MakeSoulCommit(c,{21},fence,layouts,all,all[4].id,{},SoulAction::Activate,0,0,on);Check(active.has_value(),"soul activate");Apply(active->commit);
    auto shot=MakeAmmoShotCommit(c,{31},fence,layouts,all,context,all[4].id);Check(shot && shot->damage==25002 && shot->commit.items.size()==3,"ammo and soul share one outcome");Check(SoulElapsed(shot->commit,5)==0 && SoulReceipt(shot->commit).size()==84,"combined soul receipt");
    auto forged=shot->commit;forged.items[1].after.count++;Check(!ValidateItemCommit(c,forged),"suppressed ammo debit refused");forged=shot->commit;forged.result[52]++;Check(!ValidateItemCommit(c,forged),"changed damage refused");forged=shot->commit;forged.items.back().after.soul->strikes++;Check(!ValidateItemCommit(c,forged),"suppressed soul debit refused");
    for(unsigned n=0;n<shot->commit.result.size();++n){forged=shot->commit;forged.result.resize(n);Check(!ValidateItemCommit(c,forged),"receipt truncation refused");}
    forged=shot->commit;forged.outcomeCode=1;forged.result={1};Check(!ValidateItemCommit(c,forged),"generic service cannot debit equipped ammunition");
    Apply(shot->commit);Check(all[1].count==2 && all[4].soul->strikes==1,"one shot one arrow one soul strike");
    auto wire=EncodeTypedInventory(c,layouts,{},all);Check(wire && (*wire)[5]==std::byte{16},"v16 inventory");auto decoded=DecodeTypedInventory(*wire,owner);Check(decoded && decoded->ammunition.size()==4 && decoded->ammunition.at(all[0].id).kind==1 && decoded->ammunition.at(all[1].id).policy==2,"native ammunition metadata");for(unsigned n=0;n<wire->size();++n)Check(!DecodeTypedInventory(std::span(*wire).first(n),owner),"inventory truncation");
    auto details=EncodeTypedItemDetails(c,all,false);Check(details.has_value(),"ammunition equip details");
    if(!golden.empty()){std::filesystem::create_directories(golden);const auto save=[&](const char* name,const auto& bytes){std::ofstream out(golden/name);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';Check(out.good(),"write golden");};save("typed_ammo_v16.hex",*wire);save("typed_ammo_details_v3.hex",*details);}
    context.event++;context.targetIsNpc=false;auto pvp=MakeAmmoShotCommit(c,{32},fence,layouts,all,context);Check(pvp && pvp->damage==context.damage,"accepted PvP shot consumes without PvE soul");Apply(pvp->commit);context.event++;context.damage=0;auto miss=MakeAmmoShotCommit(c,{33},fence,layouts,all,context);Check(miss && miss->commit.items.size()==3,"accepted zero-damage shot consumes and auto-equips reserve");Apply(miss->commit);Check(all[1].destroyed && all[2].placement->kind==ItemStorageKind::Equipment && all[2].count==2,"last unit leaves audit tombstone and same-key reserve");
    context.event++;context.damage=100;for(unsigned i=0;i<2;++i){auto next=MakeAmmoShotCommit(c,{static_cast<std::uint8_t>(34+i)},fence,layouts,all,context);Check(next.has_value(),"reserve shot");Apply(next->commit);context.event++;}
    Check(!FindBowAmmo(c,owner,all) && !MakeAmmoShotCommit(c,{40},fence,layouts,all,context),"exhaustion blocks further shots and incompatible reserves");Check(!all[0].destroyed && all[3].count==4,"bow and incompatible stack preserved");
    equip(4);auto multiSource=source;multiSource.weapons[0].ammoPolicy=3;auto multi=BuildItemCatalog(multiSource);Check(multi.has_value(),"multi-unit bow policy");context.event=88;auto two=MakeAmmoShotCommit(**multi,{44},fence,layouts,all,context);Check(two && two->commit.items[1].after.count==2,"policy consumes two units rather than a hardcoded arrow");
    context.damage=std::int64_t(INT_MAX)+1;Check(MakeAmmoShotCommit(**multi,{45},fence,layouts,all,context).has_value(),"accepted damage remains int64 without an ammunition-specific balance cap");context.damage=100;
    context.event=0;Check(!MakeAmmoShotCommit(c,{41},fence,layouts,all,context),"missing accepted event refused");context.event=99;context.alive=false;Check(!MakeAmmoShotCommit(c,{42},fence,layouts,all,context),"dead shooter refused");context.alive=true;context.polymorphed=true;Check(!MakeAmmoShotCommit(c,{43},fence,layouts,all,context),"polymorphed shooter refused");
}
}
int main(int argc,char** argv){try{Run(argc==3 && std::string_view(argv[1])=="--write-golden"?std::filesystem::path(argv[2]):std::filesystem::path{});std::cout<<"AMMO_RUNTIME_TESTS=SUCCESS checks="<<checks<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
