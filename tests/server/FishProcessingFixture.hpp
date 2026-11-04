#pragma once
#include "PersistentItemFixture.hpp"
#include <item/FishProcessing.hpp>
namespace fish_processing_fixture
{
using namespace auriga::item;
inline ItemKey Fish(unsigned n){return ItemKey::Create(12,6500+n).value();}
inline void Extend(ItemCatalogSource& s)
{
    const auto prototype=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
    for(unsigned n=1;n<=5;++n)
    {
        auto b=prototype;b.key=Fish(n);b.contentCode="fishing.runtime_"+std::to_string(n);b.maxStack=200;b.minimumLevel=0;
        s.items.push_back(b);
    }
    s.fishingItems.insert(s.fishingItems.end(),{{Fish(1),FishingKind::LiveFish,25001,{},0},
        {Fish(2),FishingKind::DeadFish,{},item_catalog_fixture::InstantPotion(),0},
        {Fish(3),FishingKind::Bait,{},{},20},{Fish(4),FishingKind::DeadFish,{},{},0},
        {Fish(5),FishingKind::LiveFish,25002,{},0}});
    s.lootProfiles.push_back({25001,"loot.runtime_fish",LootPickMode::WeightedOne,
        {{1,LootRewardKind::Item,Fish(2),3,1,0},{2,LootRewardKind::Item,item_catalog_fixture::Leather(),1,1,0}}});
    s.lootProfiles.push_back({25002,"loot.runtime_fish_independent",LootPickMode::IndependentChance,
        {{1,LootRewardKind::Item,Fish(2),1,5000,0},{2,LootRewardKind::Item,item_catalog_fixture::Leather(),1,5000,0}}});
}
inline ItemCatalogSource Catalog(){auto s=persistent_item_fixture::Catalog();Extend(s);return s;}
inline PersistentItem Item(unsigned n,std::uint32_t count=2)
{PersistentItem p{ItemInstanceId::Create(25000+n).value(),Fish(n),persistent_item_fixture::Character()};p.count=count;p.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};return p;}
inline FishProcessingRequest Request(const PersistentItem& source,FishAction action=FishAction::Use)
{return {source.id,action,{100,0,0,true,false,true}};}
}
