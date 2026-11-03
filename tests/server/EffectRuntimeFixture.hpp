#pragma once
#include "PersistentItemFixture.hpp"
#include <item/EffectRuntime.hpp>
namespace effect_runtime_fixture
{
using namespace auriga::item;
inline ItemKey Key(unsigned local){return ItemKey::Create(ItemCategory::Consumable,5000+local).value();}
inline void Extend(ItemCatalogSource& s)
{
    const auto base=*std::ranges::find(s.items,item_catalog_fixture::TimedPotion(),&ItemBaseRow::key);
    s.bonuses.push_back({21101,"bonus.effect_runtime_replace",BonusSource::TimedEffect,{{BonusId::MovementSpeedPercent,-500},{BonusId::Strength,9},{BonusId::PhysicalAttackFlat,12000}}});
    s.timedEffectGroups.insert(s.timedEffectGroups.end(),{{21100,"effect.runtime_wall",TimedEffectReapply::Replace,TimedEffectClock::WallTime,false},{21101,"effect.runtime_online_reject",TimedEffectReapply::Reject,TimedEffectClock::OnlineTime,true},{21102,"effect.runtime_online_replace",TimedEffectReapply::Replace,TimedEffectClock::OnlineTime,false}});
    s.timedEffects.insert(s.timedEffects.end(),{{21100,"effect.runtime_wall",21100,7,60000,1,15,true,{}},{21101,"effect.runtime_reject",21101,7,60000,1,15,true,{}},{21102,"effect.runtime_replace",21102,7,60000,1,15,true,{}},{21103,"effect.runtime_opposite",21102,21101,30000,1,15,true,{}}});
    s.toggleEffectGroups={{21104,"toggle.runtime_primary",false},{21105,"toggle.runtime_married",false},{21106,"toggle.runtime_leader",false},{21107,"toggle.runtime_tiny",true}};
    s.toggleEffects={{21104,"toggle.runtime_primary",21104,7,1,60000,0,15,true,{}},{21105,"toggle.runtime_opposite",21104,21101,2,90000,0,15,true,{}},{21106,"toggle.runtime_same",21104,7,1,60000,0,15,true,{}},{21107,"toggle.runtime_married",21105,7,1,60000,1,15,true,{}},{21108,"toggle.runtime_leader",21106,7,1,60000,2,15,true,{}},{21109,"toggle.runtime_tiny",21107,7,1,1000,0,15,true,{}}};
    for(unsigned i=0;i<10;++i){auto row=base;row.key=Key(i);row.contentCode="consumable.effect_runtime_"+std::to_string(i);row.maxStack=i<4 ? 20 : 1;s.items.push_back(row);ItemConsumableRow child{row.key};if(i<4)child.timedEffectProfile=21100+i;else child.toggleEffectProfile=21100+i;s.consumables.push_back(child);}
}
inline ItemCatalogSource Catalog(){auto s=persistent_item_fixture::Catalog();Extend(s);return s;}
inline ItemEffectFacts Facts(){ItemEffectFacts f;f.actor.pools={1000000,2000000,100000,1000000};f.actor.context=RecoveryContext::World;f.actor.level=100;f.alive=f.allowed=true;return f;}
inline PersistentItem Item(const ItemCatalog& c,unsigned index,std::uint64_t id)
{PersistentItem item{ItemInstanceId::Create(id).value(),Key(index),persistent_item_fixture::Character()};item.count=index<4 ? 5 : 1;item.placement=ItemPlacement{ItemStorageKind::Inventory,1,static_cast<std::uint32_t>(id-70)};if(const auto initial=InitialToggleEffectState(c,item.key);initial)item.toggle=*initial;return item;}
}
