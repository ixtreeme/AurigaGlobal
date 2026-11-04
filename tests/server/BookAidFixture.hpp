#pragma once
#include "BookRuntimeFixture.hpp"
namespace book_aid_fixture
{
using namespace auriga::item;
inline ItemKey Key(unsigned n){return ItemKey::Create(ItemCategory::Consumable,6300+n).value();}
inline void Extend(ItemCatalogSource& s)
{
    const auto base=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
    s.bookAidProfiles={{23001,"book_aid.chance",BookAidKind::Chance,1,1,1},
        {23002,"book_aid.bypass",BookAidKind::DelayBypass,2,2,3}};
    for(unsigned n=0;n<2;++n){auto b=base;b.key=Key(n);b.contentCode="consumable.book_aid_"+std::to_string(n);b.maxStack=20;s.items.push_back(b);ItemConsumableRow child{b.key};child.bookAidProfile=23001+n;s.consumables.push_back(child);}
}
inline ItemCatalogSource Catalog(){auto s=book_runtime_fixture::Catalog();Extend(s);return s;}
inline PersistentItem Item(unsigned n){auto i=book_runtime_fixture::Item(n,23000+n);i.key=Key(n);i.bookSkill.reset();i.placement->anchor=10+n;return i;}
}
