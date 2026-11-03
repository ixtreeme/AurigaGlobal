#pragma once
#include "PersistentItemFixture.hpp"
#include <item/BookRuntime.hpp>
namespace book_runtime_fixture
{
using namespace auriga::item;
inline ItemKey Key(unsigned n){return ItemKey::Create(ItemCategory::SkillBook,6000+n).value();}
inline void Extend(ItemCatalogSource& s)
{
    const auto base=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
    s.skillBookProfiles={
        {22100,"book.runtime_learn",SkillBookOperation::Learn,SkillBookTargetMode::Fixed,1,1,true,SkillBookBonus::ApplyAndConsume,{{1}},{{0,1,0,2,10000,10000,100,1500,1500,true},{1,2,0,1,10000,10000,100,1500,1500,true}}},
        {22101,"book.runtime_fail",SkillBookOperation::Learn,SkillBookTargetMode::Fixed,1,1,false,SkillBookBonus::Ignore,{{2}},{{0,1,0,1,0,0,0,1000,1000,true}}},
        {22102,"book.runtime_bound",SkillBookOperation::Learn,SkillBookTargetMode::InstanceBound,1,1,false,SkillBookBonus::Ignore,{{1},{2}},{{0,1,0,1,10000,10000,0,0,0,false},{1,2,0,1,10000,10000,0,0,0,false}}},
        {22103,"book.runtime_forget",SkillBookOperation::Forget,SkillBookTargetMode::Fixed,1,1,false,SkillBookBonus::Ignore,{{1,SkillPointPool::Main}},{{1,0,0,1,10000,10000,0,0,0,false}}},
        {22104,"book.runtime_horse",SkillBookOperation::HorsePoint,SkillBookTargetMode::None,1,0,false,SkillBookBonus::Ignore,{},{{0,1,50,1,10000,10000,0,1000,1000,false}}},
        {22105,"book.runtime_master",SkillBookOperation::Learn,SkillBookTargetMode::Fixed,1,2,false,SkillBookBonus::Ignore,{{1}},{{20,21,0,1,10000,10000,0,0,0,false}}}};
    for(unsigned i=0;i<6;++i){auto item=base;item.key=Key(i);item.contentCode="skill_book.runtime_"+std::to_string(i);item.maxStack=20;s.items.push_back(item);s.skillBooks.push_back({item.key,22100+i});}
}
inline ItemCatalogSource Catalog(){auto s=persistent_item_fixture::Catalog();Extend(s);return s;}
inline SkillBookAdmission Facts(){SkillBookAdmission a;a.characterLevel=100;a.empire=1;a.alive=a.skillSystemEnabled=a.targetAllowedByRules=true;return a;}
inline PersistentItem Item(unsigned n,std::uint64_t id=80){PersistentItem item{ItemInstanceId::Create(id).value(),Key(n),persistent_item_fixture::Character()};item.count=5;item.placement=ItemPlacement{ItemStorageKind::Inventory,1,static_cast<std::uint32_t>(n)};if(n==2)item.bookSkill=1;return item;}
}
