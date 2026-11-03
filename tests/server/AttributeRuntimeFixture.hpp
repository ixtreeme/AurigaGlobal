#pragma once
#include "PersistentItemFixture.hpp"
namespace attribute_runtime_fixture
{
using namespace auriga::item;
inline ItemCatalogSource Catalog()
{
    auto s=persistent_item_fixture::Catalog();
    for(unsigned kind=3;kind<=9;++kind)
    {
        auto base=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);base.key=attribute_fixture::Tool(kind);base.contentCode="consumable.attribute_"+std::to_string(kind);
        s.items.push_back(base);s.consumables.push_back({base.key,0,{},{},{},kind});s.attributeServices.push_back(attribute_fixture::Service(kind));
    }
    return s;
}
inline InstanceAttributes Full()
{InstanceAttributes a;for(unsigned slot=0;slot<5;++slot)a.slots[slot]=BonusEntry{static_cast<BonusId>(slot+2),100};return a;}
}
