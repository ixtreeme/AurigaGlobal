#pragma once
#include "SocketServiceFixture.hpp"
#include "InstanceAttributeFixture.hpp"
#include <item/PersistentItems.hpp>
namespace persistent_item_fixture
{
using namespace auriga::item;
inline ItemOwner Character(){return {ItemOwnerKind::Character,42};}
inline ItemStorageLayout Inventory(ItemOwner owner=Character()) {return {{owner,ItemStorageKind::Inventory,1},5,9,2};}
inline ItemWallet Gold(){return {Character(),WalletCurrency::Gold,1,1000};}
inline ItemKey LongSword(){return ItemKey::Create(1,2).value();}
inline ItemCatalogSource Catalog()
{
    auto s=socket_service_fixture::Catalog();
    auto& weapon=s.refinements[1];weapon.maxLevel=30;
    for(std::uint16_t i=10;i<=30;++i)weapon.levels.push_back({i,{10000LL*i,4000LL*i,0,0},{}});
    s.attributeProfiles={attribute_fixture::Profile()};s.attributeBindings={{item_catalog_fixture::Sword(),1},{LongSword(),1}};
    for(unsigned i:{1u,2u})
    {
        auto tool=s.items[6];tool.key=attribute_fixture::Tool(i);tool.contentCode="consumable.attribute_"+std::to_string(i);
        s.items.push_back(tool);s.consumables.push_back({tool.key,0,{},{},{},i});s.attributeServices.push_back(attribute_fixture::Service(i));
    }
    auto b=s.items[0];b.key=LongSword();b.contentCode="weapon.long_sword";s.items.push_back(b);
    auto w=s.weapons[0];w.key=LongSword();s.weapons.push_back(w);
    s.refinementRecipes={
        {1,"refine.nine_to_ten",item_catalog_fixture::Sword(),9,item_catalog_fixture::Sword(),10,100,10000,RefinementFailure::Keep,{},{{item_catalog_fixture::Leather(),0,3}}},
        {2,"refine.transform",item_catalog_fixture::Sword(),20,LongSword(),0,100,10000,RefinementFailure::Keep,{},{}},
        {3,"refine.failure_keep",item_catalog_fixture::Sword(),9,item_catalog_fixture::Sword(),10,100,0,RefinementFailure::Keep,{},{{item_catalog_fixture::Leather(),0,3}}},
        {4,"refine.failure_destroy",item_catalog_fixture::Sword(),9,item_catalog_fixture::Sword(),10,100,0,RefinementFailure::Destroy,{},{{item_catalog_fixture::Leather(),0,3}}},
        {5,"refine.failure_lower",item_catalog_fixture::Sword(),9,item_catalog_fixture::Sword(),10,100,0,RefinementFailure::SetLevel,8,{{item_catalog_fixture::Leather(),0,3}}},
    };return s;
}
inline PersistentItem Sword(std::uint64_t id=UINT64_MAX-8)
{
    PersistentItem p{ItemInstanceId::Create(id).value(),item_catalog_fixture::Sword(),Character()};
    p.refinementLevel=9;p.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};
    p.attributes=attribute_fixture::PayloadFixture();
    p.stones={StoneSocketSlot{StoneSocketState::Occupied,1,socket_service_fixture::Stone(),20},StoneSocketSlot{StoneSocketState::Broken,1,{},0},StoneSocketSlot{StoneSocketState::Empty,2,{},0}};return p;
}
inline PersistentItem Leather(std::uint64_t id=2)
{
    PersistentItem p{ItemInstanceId::Create(id).value(),item_catalog_fixture::Leather(),Character()};
    p.count=3;p.placement=ItemPlacement{ItemStorageKind::Inventory,1,1};return p;
}
inline ItemCommit Grant(const ItemCatalog& c,std::vector<PersistentItem> items={Sword(),Leather()})
{
    ItemCommit r;r.operationId[0]=1;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Grant;
    r.owners={{Character(),7,1}};r.storages={Inventory()};for(auto& i:items)r.items.push_back({{},std::move(i)});
    r.outcomeCode=1;r.result={0,255,0,1};return r;
}
inline SocketItemPair Pair(const PersistentItem& source,const PersistentItem& target,ItemOwnerFence fence,unsigned operation)
{
    const auto snapshot=[](const PersistentItem& p){return StoneItemSnapshot{p.key,p.id.Value(),p.revision,p.owner.id,p.refinementLevel,p.count};};
    SocketItemPair pair{{},snapshot(source),snapshot(target)};auto& ctx=pair.context;ctx.operationId[0]=static_cast<std::uint8_t>(operation);ctx.actorId=fence.owner.id;
    ctx.sessionGeneration=fence.sessionGeneration;ctx.rulesetRevision=1;ctx.inventoryRevision=fence.revision;
    ctx.characterLevel=100;ctx.characterClass=1;ctx.sex=1;ctx.admittedByServer=true;
    pair.sourceInInventory=pair.targetInInventory=true;return pair;
}
inline RefinementPlan Refine(const ItemCatalog& c,unsigned recipe,std::array<std::uint8_t,16> op,std::span<const PersistentItem> items,ItemWallet wallet)
{
    std::vector<RefinementItemSnapshot> inv;for(const auto& i:items)inv.push_back({i.id.Value(),i.revision,i.owner.id,i.key,i.refinementLevel,i.count,false});
    const auto& src=*std::ranges::find(items,Sword().id,&PersistentItem::id);
    return PrepareRefinement(c,recipe,{op,Character().id,wallet.revision,wallet.balance,true},
        {src.id.Value(),src.revision,src.owner.id,src.key,src.refinementLevel,src.count,false},inv,0).value();
}
}
