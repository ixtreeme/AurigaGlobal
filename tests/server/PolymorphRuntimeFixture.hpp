#pragma once
#include "PersistentItemFixture.hpp"
#include <item/PolymorphRuntime.hpp>
namespace polymorph_runtime_fixture
{
using namespace auriga::item;
inline ItemKey Key(unsigned n){return ItemKey::Create(ItemCategory::Polymorph,6400+n).value();}
inline void Extend(ItemCatalogSource& s)
{
    s.polymorphForms={{24001,"form.runtime_wolf",101,PolymorphBonus::Movement},{24002,"form.runtime_boar",102,PolymorphBonus::Defense}};
    PolymorphProfile orb{24001,"poly.runtime_orb",PolymorphKind::Orb,20,3,10};
    PolymorphProfile book{24002,"poly.runtime_book",PolymorphKind::PracticeBook,0,0,1,3,2,100};
    for(std::uint8_t n=0;n<=40;++n){orb.skills.push_back({n,2000,2250,3333});book.skills.push_back({n,0,0,3333});}
    for(std::uint8_t n=0;n<4;++n)book.masteries.push_back({n,6000,3000});
    auto failure=book;failure.id=24003;failure.code="poly.runtime_failure";failure.bookChanceBasePercent=-10000;s.polymorphProfiles={orb,book,failure};
    const auto base=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
    for(unsigned n=0;n<4;++n){auto b=base;b.key=Key(n);b.contentCode="polymorph.runtime_"+std::to_string(n);b.maxStack=n<2 ? 20 : 1;s.items.push_back(b);s.polymorphItems.push_back({b.key,n<2 ? 24001u : n==2 ? 24002u : 24003u,n==1 ? std::nullopt : std::optional{24001u}});}
}
inline ItemCatalogSource Catalog(){auto s=persistent_item_fixture::Catalog();Extend(s);return s;}
inline PersistentItem Item(unsigned n){PersistentItem i{ItemInstanceId::Create(24000+n).value(),Key(n),persistent_item_fixture::Character()};i.count=n<2 ? 5 : 1;i.placement=ItemPlacement{ItemStorageKind::Inventory,1,10+n};i.polymorph=PolymorphInstanceState{n==1 ? std::optional{24001u} : std::nullopt,static_cast<std::uint16_t>(n>=2 ? 1 : 0),n>=2 ? 2u : 0u};return i;}
inline PolymorphRequest Facts(ItemKey key=Key(0))
{PolymorphRequest a{key};a.actorId=42;a.rulesetRevision=1;a.operationId[0]=24;a.characterLevel=50;a.characterClass=a.sex=1;a.skillLevel=20;a.mastery=1;a.mobLevel=5;a.mobTemplate=101;a.mobCatalogRevision=1;a.action=PolymorphAction::Use;a.admittedByServer=a.formAllowedByServer=true;return a;}
}
