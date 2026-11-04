#pragma once
#include "PersistentItemFixture.hpp"
#include <item/ToolRuntime.hpp>
namespace tool_runtime_fixture
{
using namespace auriga::item;
inline ItemKey Tool(unsigned n=1){return ItemKey::Create(11,6600+n).value();}
inline ItemKey Bait(unsigned n=1){return ItemKey::Create(12,6600+n).value();}
inline void Extend(ItemCatalogSource& s)
{
    s.refinements.push_back({26001,"refine.runtime_tool",RefinementKind::Tool,2,{{0,{},{}},{1,{},{}},{2,{},{}}}});
    ToolProfile profile{26001,"tool.runtime_training",{{0,100000,250,3,1,3,10000,ToolFailure::LosePoints,{},0},
        {1,200000,500,5,2,7,5000,ToolFailure::ResetAtLevel,0,0},{2,300000,1000,0,0,1,0,ToolFailure::LosePoints,{},0}}};
    auto failure=profile;failure.id=26002;failure.code="tool.runtime_failure";failure.levels[0].refineChanceBp=0;failure.levels[0].failureLossBp=5000;
    s.toolProfiles.push_back(profile);s.toolProfiles.push_back(failure);
    const auto prototype=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
    for(unsigned n=1;n<=5;++n)
    {
        auto b=prototype;b.key=Tool(n);b.contentCode="tool.runtime_"+std::to_string(n);b.maxStack=1;b.minimumLevel=n==5 ? 255 : 0;b.refinementProfile=26001;s.items.push_back(b);
        const auto kind=n==1 || n==5 ? ToolKind::Rod : n==2 ? ToolKind::Pick : n==3 ? ToolKind::Axe : ToolKind::Shears;
        s.tools.push_back({b.key,kind,n==2 ? 26002u : 26001u,kind==ToolKind::Rod ? FishingPool::First : FishingPool::None});
    }
    for(unsigned n=1;n<=2;++n){auto b=prototype;b.key=Bait(n);b.contentCode="fishing.runtime_tool_bait_"+std::to_string(n);b.minimumLevel=0;b.maxStack=200;s.items.push_back(b);s.fishingItems.push_back({b.key,FishingKind::Bait,{},{},n==1 ? 20u : 50u});}
}
inline ItemCatalogSource Catalog(){auto s=persistent_item_fixture::Catalog();Extend(s);return s;}
inline PersistentItem Item(unsigned n=1)
{PersistentItem p{ItemInstanceId::Create(26000+n).value(),Tool(n),persistent_item_fixture::Character()};p.placement=ItemPlacement{ItemStorageKind::Inventory,1,10+n};p.tool=ToolInstanceState{3,0};return p;}
inline PersistentItem BaitItem(unsigned n=1)
{PersistentItem p{ItemInstanceId::Create(26100+n).value(),Bait(n),persistent_item_fixture::Character()};p.count=2;p.placement=ItemPlacement{ItemStorageKind::Inventory,1,20+n};return p;}
inline ToolRuntimeActor Actor(){return {100,0,0,true,false,false,true,false};}
}
