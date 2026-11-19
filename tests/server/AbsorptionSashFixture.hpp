#pragma once
#include "PersistentItemFixture.hpp"
#include <item/AbsorptionSashRuntime.hpp>
namespace absorption_sash_fixture {
using namespace auriga::item;
inline ItemKey Sash(unsigned grade,unsigned family=0){return ItemKey::Create(19,95000+family*10+grade).value();}
inline ItemKey Clear(){return ItemKey::Create(20,95000).value();}
inline ItemCatalogSource Catalog(){
    auto s=persistent_item_fixture::Catalog();auto base=*std::ranges::find(s.items,item_catalog_fixture::Leather(),&ItemBaseRow::key);base.width=base.height=1;base.maxStack=1;base.appearanceProfile.reset();base.baseBonusProfile.reset();base.binding=BindingPolicy::None;base.refinementProfile=1;base.usePolicy=1;
    for(unsigned family=0;family<2;++family)for(unsigned grade=1;grade<=4;++grade){
        auto b=base;b.key=Sash(grade,family);b.contentCode="regalia.absorption_probe_"+std::to_string(family)+"_"+std::to_string(grade);s.items.push_back(b);
        s.regalia.push_back({b.key,RegaliaKind::AbsorptionSash,0,static_cast<std::uint8_t>(grade),0,0,{}});
        const auto lo=std::array<std::uint8_t,4>{1,5,10,11}[grade-1],hi=std::array<std::uint8_t,4>{1,5,10,19}[grade-1],cl=std::array<std::uint8_t,4>{5,10,12,1}[grade-1],ch=std::array<std::uint8_t,4>{5,10,19,5}[grade-1];
        s.absorptionSashes.push_back({b.key,Sash(std::min(4u,grade+1),family),static_cast<std::uint8_t>(grade),lo,hi,cl,ch,25,std::array<std::uint16_t,4>{10000,8000,7000,6000}[grade-1],std::array<std::uint64_t,4>{1000000,3000000,5000000,8000000}[grade-1]});
    }
    base.key=Clear();base.contentCode="bonus_tool.absorption_clear";base.maxStack=10;s.items.push_back(base);s.bonusTools.push_back({base.key,SpecialBonusOperation::SashClear,1,0,{},{},{}});
    s.sashDonors.push_back({item_catalog_fixture::Sword(),0,60000,20000,true,true,{{BonusId::Strength,3},{BonusId::Strength,4}}});
    s.sashDonors.push_back({item_catalog_fixture::Armor(),0,0,0,false,false,{{BonusId::Constitution,5}}});
    return s;
}
inline AbsorptionSashActor Actor(){AbsorptionSashActor a;a.equipment.level=255;a.admitted=a.alive=true;return a;}
}
