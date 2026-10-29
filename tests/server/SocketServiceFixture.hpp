#pragma once
#include "ItemCatalogFixture.hpp"
#include <algorithm>
namespace socket_service_fixture
{
using namespace auriga::item;
inline ItemKey Tool(unsigned kind){return ItemKey::Create(5,10+kind).value();}
inline ItemKey Stone(unsigned local=1){return ItemKey::Create(7,local).value();}
inline ItemCatalogSource Catalog()
{
    auto s=item_catalog_fixture::Catalog();
    for(unsigned kind=1;kind<=3;++kind)
    {
        auto b=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
        b.key=Tool(kind);b.contentCode="consumable.socket_service_"+std::to_string(kind);
        s.items.push_back(b);s.consumables.push_back({b.key,0,{},{},kind});
        s.socketServiceProfiles.push_back({kind,"socket.service_"+std::to_string(kind),static_cast<SocketServiceKind>(kind),1});
    }
    RefinementProfile p{5,"refine.socket_fixture",RefinementKind::SocketStone,30,{}};
    for(std::uint16_t i=0;i<=30;++i)p.levels.push_back({i,{},{}});
    s.refinements.push_back(p);
    for(unsigned i=1;i<=2;++i)
    {
        auto b=*std::ranges::find(s.items,item_catalog_fixture::Leather(),&ItemBaseRow::key);
        b.key=Stone(i);b.contentCode="socket_stone.service_fixture_"+std::to_string(i);b.refinementProfile=5;
        s.items.push_back(b);s.socketStones.push_back({b.key,SocketStoneKind::Insertable,3,1,"stone.group_"+std::to_string(i),""});
    }
    return s;
}
}
