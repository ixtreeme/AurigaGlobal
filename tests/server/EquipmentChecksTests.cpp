#include "ItemCatalogFixture.hpp"
#include <item/EquipmentChecks.hpp>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace {
int checks=0;
void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
}
int main(){
try {
    EquipmentStats stats;
    Check(!EquippedWeapon(nullptr)&&!EquippedWeapon(&stats),"empty equipment accepted as weapon");
    const auto wide=ItemInstanceId::Create((1ULL<<40)+17).value();
    stats.weapon=WeaponSubtype::Sword;
    Check(!EquippedWeapon(&stats),"subtype without an equipped instance accepted");
    stats.equipped.push_back({static_cast<uint8_t>(EquipmentSlot::CostumeWeapon),wide,f::Sword()});
    Check(!EquippedWeapon(&stats),"costume accepted as combat weapon");
    stats.equipped[0].slot=static_cast<uint8_t>(EquipmentSlot::Weapon);
    auto weapon=EquippedWeapon(&stats);
    Check(weapon&&weapon->instance==wide.Value(),"persistent weapon identity was narrowed");
    auto replacement=stats;replacement.equipped[0].id=ItemInstanceId::Create(wide.Value()+(1ULL<<32)).value();
    Check(EquippedWeapon(&replacement)->instance!=weapon->instance,"wide weapon replacement was invisible");
    const WeaponSubtype types[]={WeaponSubtype::Sword,WeaponSubtype::Dagger,WeaponSubtype::Bow,
        WeaponSubtype::TwoHanded,WeaponSubtype::Bell,WeaponSubtype::Fan,WeaponSubtype::MountSpear};
    const bool allowed[4][7]={{1,0,0,1,0,0,0},{1,1,1,0,0,0,0},{1,0,0,0,0,0,0},{0,0,0,0,1,1,0}};
    for(uint8_t job=0;job<4;++job)for(unsigned t=0;t<7;++t){
        stats.weapon=types[t];weapon=EquippedWeapon(&stats);
        Check(weapon&&WeaponAllowedForJob(*weapon,job)==allowed[job][t],"class weapon policy changed");
    }
    Check(!WeaponAllowedForJob(*weapon,4),"unknown class accepted");
    auto source=f::Catalog();
    const char* codes[]={"armor.szmoking","armor.szmoking_40abc1b8530c",
        "armor.menyasszonyi_ruha","armor.menyasszonyi_ruha_a5fdd09ff2b2"};
    std::vector<ItemKey> keys;
    for(unsigned n=0;n<4;++n){
        auto key=ItemKey::Create(ItemCategory::Armor,900+n).value();keys.push_back(key);
        auto base=source.items[1];base.key=key;base.contentCode=codes[n];source.items.push_back(base);
        source.armors.push_back({key,EquipmentSlot::Body,0,0,1});
    }
    auto catalog=BuildItemCatalog(std::move(source)).value();
    stats={};Check(!BlocksHorseRiding(*catalog,stats),"empty body blocked riding");
    stats.equipped.push_back({static_cast<uint8_t>(EquipmentSlot::Body),wide,f::Armor()});
    Check(!BlocksHorseRiding(*catalog,stats),"ordinary armor blocked riding");
    for(auto key:keys){
        stats.equipped[0].key=key;
        Check(BlocksHorseRiding(*catalog,stats),"native ceremonial body armor missed");
        stats.equipped[0].slot=static_cast<uint8_t>(EquipmentSlot::CostumeBody);
        Check(!BlocksHorseRiding(*catalog,stats),"costume slot inherited body restriction");
        stats.equipped[0].slot=static_cast<uint8_t>(EquipmentSlot::Body);
    }
    std::cout<<"Equipment checks passed: "<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
