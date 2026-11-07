#pragma once
#include "ItemCatalogFixture.hpp"
#include <item/SpecialBonusRuntime.hpp>
namespace special_bonus_fixture {
using namespace auriga::item;
inline ItemKey Regalia(unsigned local){return ItemKey::Create(19,local).value();}
inline ItemKey Tool(unsigned local){return ItemKey::Create(20,local).value();}
inline ItemKey Costume(){return ItemKey::Create(4,880).value();}
inline ItemCatalogSource Catalog(){
    auto s=item_catalog_fixture::Catalog();auto b=s.items[3];b.maxStack=1;b.binding=BindingPolicy::None;b.appearanceProfile.reset();b.baseBonusProfile.reset();b.refinementProfile=1;b.usePolicy=1;
    for(unsigned local=1;local<=8;++local){auto base=b;base.key=Regalia(local);base.contentCode="regalia.probe_"+std::to_string(local);s.items.push_back(base);ItemRegaliaRow r{base.key,local==1 ? RegaliaKind::Stole : RegaliaKind::Rune,static_cast<std::uint8_t>(local==1 ? 0 : local-1),static_cast<std::uint8_t>(local==1 ? 4 : 0),local==1 || local==8 ? 0ull : 60000ull,local==1 || local==8 ? 0ull : 60000ull,{}};
        if(local==1){const BonusId ids[]={BonusId::Strength,BonusId::Dexterity,BonusId::Constitution,BonusId::Intelligence,BonusId::PhysicalAttackFlat,BonusId::PhysicalDefenseFlat};for(unsigned j=0;j<6;++j)for(unsigned v=0;v<4;++v)r.rules.push_back({static_cast<std::uint8_t>(j),static_cast<std::uint8_t>(v),0,{ids[j],std::int64_t(1+v)*(j>=4 ? 1000 : 1)}});}
        else for(unsigned j=0;j<2;++j)for(unsigned tier=0;tier<(local==8 ? 1 : 7);++tier){const unsigned bp[]={0,600,1100,2100,4100,6100,8100};r.rules.push_back({static_cast<std::uint8_t>(j),0,static_cast<std::uint16_t>(bp[tier]),{j ? BonusId::PhysicalAttackFlat : BonusId::Strength,std::int64_t(tier+1)*(j ? 1000 : 1)}});}
        s.regalia.push_back(r);
    }
    for(unsigned op=1;op<=6;++op){auto base=b;base.key=Tool(op);base.maxStack=op==2 || op==4 ? 1 : 50;base.contentCode="bonus_tool.probe_"+std::to_string(op);s.items.push_back(base);ItemBonusToolRow row{base.key,static_cast<SpecialBonusOperation>(op),1,static_cast<std::uint16_t>(op==2 ? 6000 : 0),op==4 ? std::optional{BonusEntry{BonusId::Dexterity,7}} : std::nullopt,{}};if(op==3)for(unsigned j=0;j<3;++j)row.tiers.push_back({static_cast<std::uint8_t>(j),static_cast<std::uint8_t>(j==0 ? 2 : 1),10000});s.bonusTools.push_back(row);}
    auto base=b;base.key=Costume();base.contentCode="costume.special_probe";s.items.push_back(base);s.costumes.push_back({base.key,CostumeKind::Body,{},CostumeClock::Permanent,0});for(unsigned c=1;c<=4;++c)for(unsigned sex=1;sex<=2;++sex)s.costumeVisuals.push_back({base.key,static_cast<std::uint8_t>(c),static_cast<std::uint8_t>(sex),"models/costume.model"});
    AttributeProfile p{880,"attributes.special",false,{{AttributePool::Normal,5,false},{AttributePool::Rare,2,false},{AttributePool::Extra,1,false}}, {}};
    for(auto pool:{AttributePool::Normal,AttributePool::Rare,AttributePool::Extra})for(auto id:{BonusId::Strength,BonusId::Dexterity,BonusId::Constitution,BonusId::Intelligence,BonusId::NormalOutgoing,BonusId::SkillOutgoing})p.candidates.push_back({pool,id,1,{{1,100},{2,200}}});s.attributeProfiles.push_back(p);s.attributeBindings.push_back({base.key,880});s.attributeBindings.push_back({item_catalog_fixture::Sword(),880});return s;
}
}
