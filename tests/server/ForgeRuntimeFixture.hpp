#pragma once
#include "PersistentItemFixture.hpp"
#include <item/ForgeRuntime.hpp>
namespace forge_runtime_fixture {
using namespace auriga::item;
inline ItemKey Aid(unsigned id=1){return ItemKey::Create(22,id).value();}
inline ItemCatalogSource Catalog(){auto s=persistent_item_fixture::Catalog();
    for(unsigned j=1;j<=10;++j){auto b=s.items[3];b.key=Aid(j);b.contentCode="forge.fixture_"+std::to_string(j);b.nameKey=b.contentCode+".name";b.descriptionKey=b.contentCode+".description";s.items.push_back(b);ItemForgeAidRow a{b.key};
        if(j==2)a.failure=ForgeFailure::Destroy;
        if(j==3 || j==4){a.chanceMode=ForgeChance::Levels;const std::array<unsigned,9> rates=j==3?std::array<unsigned,9>{10000,7500,6500,5500,4500,4000,3500,2500,2000}:std::array<unsigned,9>{10000,10000,9000,8000,7000,6000,5000,3000,2000};for(unsigned l=0;l<=255;++l)a.levels.push_back({static_cast<std::uint16_t>(l),static_cast<std::uint16_t>(rates[std::min(l,8u)])});}
        if(j==5 || j==6){a.chanceMode=ForgeChance::Replace;a.chanceBp=10000;a.failure=ForgeFailure::Keep;if(j==6)a.maximumLevel=0;}
        if(j==7){a.targetMask=8;a.minimumLevel=a.maximumLevel=4;a.chanceMode=ForgeChance::Replace;a.chanceBp=8000;}
        if(j>=8){a.kind=ForgeAidKind::Booster;a.chanceMode=ForgeChance::Add;a.chanceBp=(j-7)*500;a.boosterGroup=j-7;a.failure=ForgeFailure::Recipe;}
        s.forgeAids.push_back(a);
    }
    s.refinementRecipes.push_back({6,"refine.zero_to_one",item_catalog_fixture::Sword(),0,item_catalog_fixture::Sword(),1,100,0,RefinementFailure::Destroy,{},{{item_catalog_fixture::Leather(),0,3}}});
    s.refinementRecipes.push_back({7,"refine.socket_four_to_five",socket_service_fixture::Stone(),4,socket_service_fixture::Stone(),5,100,10000,RefinementFailure::Destroy,{},{{item_catalog_fixture::Leather(),0,3}}});return s;
}
}
