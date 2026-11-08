#pragma once
#include "SoulRuntimeFixture.hpp"
#include <item/AmmoRuntime.hpp>
namespace ammo_runtime_fixture {
using namespace auriga::item;
inline ItemKey Ammo(unsigned id=1){return ItemKey::Create(21,id).value();}
inline ItemCatalogSource Catalog(){auto s=soul_runtime_fixture::Catalog();s.weapons[0].subtype=WeaponSubtype::Bow;s.weapons[0].ammoPolicy=2;s.attacks[0].reachMillimeters=20000;
    s.ammoPolicies.push_back({3,"ammo.incompatible",AmmoKind::ConsumeStack,2});
    for(unsigned j=1;j<=2;++j){auto b=s.items[3];b.key=Ammo(j);b.contentCode="ammo.fixture_"+std::to_string(j);b.nameKey=b.contentCode+".name";b.descriptionKey=b.contentCode+".description";b.maxStack=j==1?3:200;b.usePolicy=1;b.refinementProfile=1;s.items.push_back(b);s.ammunition.push_back({b.key,j==1?2u:3u});}return s;}
}
