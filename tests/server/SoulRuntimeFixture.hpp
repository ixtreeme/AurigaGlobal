#pragma once
#include "ItemCatalogFixture.hpp"
#include <item/SoulRuntime.hpp>
namespace soul_runtime_fixture {
using namespace auriga::item;namespace f=item_catalog_fixture;
inline ItemKey Key(unsigned n){return ItemKey::Create(14,n).value();}
inline ItemCatalogSource Catalog(){
    auto s=f::Catalog();RefinementProfile refinement{5,"refine.soul",RefinementKind::Soul,2,{}};
    SoulProfile profile{1,"soul.standard",{}};
    for(std::uint16_t i=0;i<=2;++i){refinement.levels.push_back({i,{},{}});profile.levels.push_back({i,6,2,2,0,2,5000,15000,18000,25000});}
    s.refinements.push_back(refinement);s.soulProfiles.push_back(profile);
    for(unsigned i=1;i<=3;++i)
    {auto base=s.items[3];base.key=Key(i);base.contentCode="soul.item_"+std::to_string(i);base.nameKey="item.soul_"+std::to_string(i)+".name";base.descriptionKey="item.soul_"+std::to_string(i)+".description";
     base.maxStack=i==3?200:1;base.usePolicy=1;base.refinementProfile=i==3?1:5;s.items.push_back(base);}
    s.soulItems={{Key(1),SoulKind::Core,SoulChannel::NormalAttack,1,0},{Key(2),SoulKind::Core,SoulChannel::Skill,1,0},{Key(3),SoulKind::RefinementScroll,{},{},10000}};
    return s;
}
}
