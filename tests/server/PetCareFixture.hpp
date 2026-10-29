#pragma once
#include "ItemCatalogFixture.hpp"
namespace pet_care_fixture {
using namespace auriga::item;
inline ItemKey Pet(){return ItemKey::Create(15,1).value();}
inline ItemKey Egg(){return ItemKey::Create(8,4).value();}
inline ItemCatalogSource Catalog() {
    auto s=item_catalog_fixture::Catalog();
    s.companionForms={{1,"care.pet",34041,"models/care_pet.model"}};
    s.growthPetProfiles={{1,"growth.care",3,GrowthExpPolicy::MobAndItem,{{1,100,10},{2,200,20},{3,0,0}},{{0,3,1}}}};
    auto pet=s.items[3];pet.key=Pet();pet.maxStack=1;pet.usePolicy=1;pet.contentCode="companion.item_1";
    pet.nameKey="item.companion_1.name";pet.descriptionKey="item.companion_1.description";s.items.push_back(pet);
    s.companions={{Pet(),CompanionKind::GrowthPet,1,{},CompanionClock::ActiveTime,300000,false,1}};
    auto egg=s.items[3];egg.key=Egg();egg.maxStack=20;egg.usePolicy=1;egg.contentCode="container.pet_egg";
    egg.nameKey="item.pet_egg.name";egg.descriptionKey="item.pet_egg.description";s.items.push_back(egg);
    s.containers.push_back({Egg(),ContainerKind::PetEgg,{},0,1});
    s.petHatchProfiles={{1,"hatch.care",Pet(),100000,{30,40,5},{10,20,5},{10,20,5}}};
    namespace f=item_catalog_fixture;
    s.petFeedRules={{f::Leather(),PetFeedingService::Cube,PetFoodKind::LifetimeFraction,PetFoodConsumption::WholeStack,0,300,0,0},
                   {f::Sword(),PetFeedingService::Cube,PetFoodKind::EquipmentExperience,PetFoodConsumption::ExactUnits,1,0,0,2},
                   {Egg(),PetFeedingService::Cube,PetFoodKind::LifetimeFraction,PetFoodConsumption::ExactUnits,1,500,0,0},
                   {f::SocketJewel(),PetFeedingService::Direct,PetFoodKind::RefillOrRevive,PetFoodConsumption::ExactUnits,1,0,5000,0}};
    return s;
}
}
