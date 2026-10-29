#pragma once
#include "ItemCatalogFixture.hpp"
namespace attribute_fixture
{
using namespace auriga::item;
inline ItemKey Tool(unsigned id){return ItemKey::Create(5,100+id).value();}
inline AttributeProfile Profile()
{
    AttributeProfile p{1,"attribute.fixture",true,{{AttributePool::Normal,5,true},{AttributePool::Rare,2,false},{AttributePool::Extra,1,false}},{}};
    for(unsigned id=1;id<=8;++id)p.candidates.push_back({AttributePool::Normal,static_cast<BonusId>(id),id*10,{{1,100},{2,200}}});
    for(const auto pool:{AttributePool::Rare,AttributePool::Extra})for(unsigned id=21;id<=23;++id)
        p.candidates.push_back({pool,static_cast<BonusId>(id),1,{{1,100},{2,200}}});
    return p;
}
inline AttributeServiceProfile Service(unsigned id)
{
    const auto op=id<=5 ? static_cast<AttributeOperation>(id) : (id%2 ? AttributeOperation::Reroll : AttributeOperation::Add);
    const auto pool=id<=5 ? AttributePool::Normal : id<=7 ? AttributePool::Rare : AttributePool::Extra;
    AttributeServiceProfile p{id,"attribute.service_"+std::to_string(id),op,pool,1,10000,7,0,static_cast<std::uint8_t>(id<=5 ? 5 : id<=7 ? 2 : 1),static_cast<std::uint8_t>(op==AttributeOperation::Add),0,false,{}};
    if(op==AttributeOperation::Add || op==AttributeOperation::Reroll)p.tiers={{1,4000},{2,6000}};
    return p;
}
inline ItemCatalogSource Catalog()
{
    auto s=item_catalog_fixture::Catalog();s.attributeProfiles={Profile()};s.attributeBindings={{item_catalog_fixture::Sword(),1},{item_catalog_fixture::Armor(),1}};
    for(unsigned i=1;i<=9;++i){auto b=s.items[6];b.key=Tool(i);b.contentCode="consumable.attribute_"+std::to_string(i);s.items.push_back(b);s.consumables.push_back({b.key,0,{},{},{},i});s.attributeServices.push_back(Service(i));}
    return s;
}
inline ItemCatalogSource HashFixture()
{
    auto s=item_catalog_fixture::Catalog();s.attributeProfiles={Profile()};s.attributeBindings={{item_catalog_fixture::Sword(),1}};
    s.attributeServices={Service(1)};s.consumables[0].recoveryProfile=0;s.consumables[0].attributeServiceProfile=1;return s;
}
inline InstanceAttributes PayloadFixture()
{
    InstanceAttributes s;s.slots[0]=BonusEntry{BonusId::MagicalAttackFlat,-100};s.slots[2]=BonusEntry{BonusId::PhysicalDefenseFlat,200};s.slots[5]=BonusEntry{BonusId::CritChance,100};s.lockedSlot=2;return s;
}
}
