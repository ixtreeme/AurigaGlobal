#pragma once
#include "PetCareFixture.hpp"
namespace pet_lifecycle_fixture {
using namespace auriga::item;
inline ItemKey Supply(unsigned kind){return ItemKey::Create(17,kind).value();}
inline ItemCatalogSource Catalog() {
    auto s=pet_care_fixture::Catalog();
    s.companionForms.push_back({2,"lifecycle.skin",34042,"models/pet_skin.model"});
    s.companions[0].trainingProfile=1;s.growthPetProfiles[0].stages={{0,1,1},{1,2,1},{2,3,1}};
    s.petTrainingProfiles={{1,"training.fixture",2,3,0,1,6,
        {{0,500,200,200},{1000,1200,400,400},{2000,2100,700,700},{3000,3000,1000,1000}},
        {{0,1,0},{1,2,0},{2,3,0},{3,0,1500}},
        {{1,"pet.warrior_resist",PetEffect::WarriorResistancePercent,{{1,100},{2,200}}},
         {2,"pet.boss_damage",PetEffect::BossDamagePercent,{{1,200},{2,400}}}}}};
    for(unsigned kind=1;kind<=8;++kind) {
        PetSupplyProfile p{kind,"supply.fixture_"+std::to_string(kind),static_cast<PetSupplyKind>(kind)};
        if(kind==1)p.study=PetStudyRule{1,true,true,7100,10800000};
        if(kind==4)p.extension=PetExtensionRule{86400000,31535940000};
        if(kind==6)p.enchant=PetEnchantRule{3000,5,{150,100,100}};
        if(kind==8)p.skin=PetSkinRule{2,CostumeClock::EquippedTime,10000};
        s.petSupplyProfiles.push_back(p);
        auto item=s.items[3];item.key=Supply(kind);item.maxStack=kind==7 || kind==8 ? 1 : 20;item.usePolicy=1;
        item.contentCode="special.pet_supply_"+std::to_string(kind);item.nameKey="item.pet_supply_"+std::to_string(kind)+".name";
        item.descriptionKey="item.pet_supply_"+std::to_string(kind)+".description";s.items.push_back(item);
        s.petSupplies.push_back({item.key,kind});
    }
    return s;
}
}
