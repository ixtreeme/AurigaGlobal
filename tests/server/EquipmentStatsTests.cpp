#include "PersistentItemFixture.hpp"
#include <item/EquipmentStats.hpp>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=persistent_item_fixture;
namespace
{
unsigned checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    auto source=f::Catalog();source.weapons[0].physicalAttackBaseRaw=100000;source.weapons[0].magicalAttackBaseRaw=20000;
    source.armors[0].physicalDefenseBaseRaw=40000;source.armors[0].magicalDefenseBaseRaw=10000;
    source.attributeProfiles[0].candidates.push_back({AttributePool::Normal,BonusId::Strength,10,{{1,3},{2,6}}});
    source.resistances[0].physicalBp=500;source.resistances[0].magicalBp=1000;source.resistances[0].swordBp=700;
    const auto stone=socket_service_fixture::Stone();source.bonuses.push_back({9000,"bonus.stone_equipment",BonusSource::ItemBase,{{BonusId::PhysicalDefenseFlat,12000}}});
    std::ranges::find(source.items,stone,&ItemBaseRow::key)->baseBonusProfile=9000;
    const auto built=BuildItemCatalog(source);Check(built.has_value(),"catalog");const auto& c=**built;
    auto weapon=f::Sword(700);weapon.refinementLevel=3;weapon.attributes={};weapon.attributes.slots[0]=BonusEntry{BonusId::Strength,3};weapon.attributes.slots[1]=BonusEntry{BonusId::NormalOutgoing,100};
    weapon.stones={};weapon.stones[0]={StoneSocketState::Occupied,1,stone,20};weapon.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};
    PersistentItem armor{ItemInstanceId::Create(701).value(),item_catalog_fixture::Armor(),f::Character()};armor.refinementLevel=1;armor.placement=ItemPlacement{ItemStorageKind::Equipment,1,1};
    std::vector<PersistentItem> inventory{armor,weapon,f::Sword(702)};
    const auto stats=BuildEquipmentStats(c,f::Character(),inventory);
    Check(stats.has_value(),"equipment accepted");
    Check(stats->flat.physicalAttackRaw==137500 && stats->flat.magicalAttackRaw==32000,"base and cumulative refinement plus flat profiles once");
    Check(stats->flat.physicalDefenseRaw==57000 && stats->flat.magicalDefenseRaw==12000,"armor plus occupied socket contribution");
    Check(stats->Bonus(BonusId::NormalOutgoing)==600 && stats->Bonus(BonusId::Strength)==3 && stats->Bonus(BonusId::CritChance)==100,"instance, base and current refinement bonuses");
    Check(stats->resistances[0]==500 && stats->resistances[1]==1000 && stats->resistances[2]==700,"typed resistance profile");
    Check(stats->weapon==WeaponSubtype::Sword && stats->equipped.size()==2 && stats->equipped[0].id==weapon.id,"stable slot order and weapon source");
    std::ranges::reverse(inventory);Check(BuildEquipmentStats(c,f::Character(),inventory)==stats,"input order does not affect projection");
    auto carried=weapon;carried.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};
    Check(BuildEquipmentStats(c,f::Character(),std::span(&carried,1))->flat==ItemBaseStats{},"inventory copies never grant equipped stats");
    auto empty=BuildEquipmentStats(c,f::Character(),{});Check(empty && !empty->weapon && empty->flat==ItemBaseStats{},"empty typed equipment is a complete zero contribution");
    auto wrong=weapon;wrong.owner.id++;Check(!BuildEquipmentStats(c,f::Character(),std::span(&wrong,1)),"foreign owner refused");
    wrong=weapon;wrong.placement->anchor=1;Check(!BuildEquipmentStats(c,f::Character(),std::span(&wrong,1)),"slot mismatch refused");
    auto duplicate=weapon;duplicate.id=ItemInstanceId::Create(710).value();std::vector<PersistentItem> collision{weapon,duplicate};Check(!BuildEquipmentStats(c,f::Character(),collision),"duplicate equipment slot refused");
    auto broken=weapon;broken.stones[0]={StoneSocketState::Broken,1,{},0};
    Check(BuildEquipmentStats(c,f::Character(),std::span(&broken,1))->flat.physicalDefenseRaw==0,"broken stone contributes nothing");
    auto level=weapon;level.refinementLevel=4;
    Check(BuildEquipmentStats(c,f::Character(),std::span(&level,1))->Bonus(BonusId::CritChance)==0,"refinement profiles are current-level cumulative data, not previous-level sums");
    source.weapons[0].subtype=WeaponSubtype::Bow;source.weapons[0].ammoPolicy=2;const auto bow=BuildItemCatalog(source);
    Check(bow.has_value() && BuildEquipmentStats(**bow,f::Character(),std::span(&weapon,1)).error()==EquipmentStatsError::UnsupportedState,"unported consumed ammunition never silently falls back to legacy");
    Check(ScaleEquipmentValue(1,5000)==1 && ScaleEquipmentValue(-1,5000)==-1,"signed half-away rounding");
    Check(ScaleEquipmentValue(1000000000000000LL,10000000)==1000000000000000000LL,"wide intermediate avoided by quotient/remainder scaling");
    Check(!ScaleEquipmentValue(INT64_MAX,20000) && !AddEquipmentValue(INT64_MAX,1),"overflow rejected");
    Check(ScaleEquipmentValue(0,0)==0 && ScaleEquipmentValue(1234,10000)==1234,"identity and zero factors");
    const auto wire=EncodeEquipmentStats(&*stats,12345,7,9,{10,11,12,13,200,80,120,70,125,220,110,2000,500});
    Check(wire.size()==285+12*std::ranges::count_if(stats->bonuses,[](auto v){return v!=0;}),"wire length includes each bonus once");
    Check(EncodeEquipmentStats(nullptr,12345,8,10,{}).size()==285,"rejected projection has no item contribution");
    return wire;
}
}
int main(int argc,char** argv)
{try{const auto wire=Run();if(argc==3 && std::string_view(argv[1])=="--write-golden"){std::ofstream out(argv[2]);for(auto b:wire)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("write golden");}std::cout<<checks<<" equipment stat checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
