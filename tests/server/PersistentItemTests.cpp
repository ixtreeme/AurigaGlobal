#include "PersistentItemFixture.hpp"
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=persistent_item_fixture;
namespace
{
unsigned checks=0;
void Check(bool good,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!good)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
void Run()
{
    auto catalog=BuildItemCatalog(f::Catalog());Check(catalog.has_value(),"fixture catalog");const auto& c=**catalog;
    auto sword=f::Sword();Check(ValidatePersistentItem(c,sword).has_value(),"complete high-ID sword");
    Check(OccupiedItemCells(c,sword,f::Inventory())==std::vector<std::uint32_t>{0,5,10},"reserve every sword cell");
    for(auto anchor:{40u,44u,89u,90u,UINT32_MAX}){auto bad=sword;bad.placement->anchor=anchor;Check(!OccupiedItemCells(c,bad,f::Inventory()),"page/edge/outside footprint rejected");}
    sword.placement->anchor=45;Check(OccupiedItemCells(c,sword,f::Inventory())==std::vector<std::uint32_t>{45,50,55},"second inventory page");
    sword.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};ItemStorageLayout equipment{{f::Character(),ItemStorageKind::Equipment,1},8,1,1};
    Check(OccupiedItemCells(c,sword,equipment)==std::vector<std::uint32_t>{0},"equipment reserves semantic slot only");
    sword.placement->anchor=1;Check(!OccupiedItemCells(c,sword,equipment),"wrong equipment slot");
    auto grant=f::Grant(c);Check(ValidateItemCommit(c,grant).has_value(),"grant valid");const auto digest=ItemCommitDigest(c,grant);Check(digest.has_value(),"grant hash");
    auto reordered=grant;std::ranges::reverse(reordered.items);Check(ItemCommitDigest(c,reordered)==digest,"request vector order canonicalized");
    const auto reject=[&](auto change){auto r=grant;change(r);Check(!ValidateItemCommit(c,r),"invalid commit accepted");};
    reject([](auto& r){r.operationId={};});reject([](auto& r){r.catalogRevision++;});reject([](auto& r){r.catalogHash[0]^=1;});
    reject([](auto& r){r.owners[0].sessionGeneration=0;});reject([](auto& r){r.owners[0].revision=UINT64_MAX;});
    reject([](auto& r){r.owners.push_back(r.owners[0]);});reject([](auto& r){r.storages[0].rows=0;});
    reject([](auto& r){r.items[1].after.placement->anchor=5;});reject([](auto& r){r.items.push_back(r.items[0]);});
    reject([](auto& r){r.items[0].after.refinementLevel=31;});reject([](auto& r){r.items[0].after.count=2;});
    reject([](auto& r){r.items[0].after.owner.kind=ItemOwnerKind::Account;});
    reject([](auto& r){r.items[0].after.attributes.lockedSlot=4;});
    reject([](auto& r){r.items[0].after.stones[0].refinementLevel=31;});
    reject([](auto& r){r.items[1].after.accessory=AccessorySocketState{};});
    reject([](auto& r){r.items[0].after.destroyed=true;});reject([](auto& r){r.items[0].after.placement.reset();});
    reject([](auto& r){r.items[1].after.key=item_catalog_fixture::AutomaticPotion();r.items[1].after.count=1;});
    auto changed=grant;changed.items[0].after.attributes.slots[0]->valueScaled--;
    Check(ValidateItemCommit(c,changed).has_value() && ItemCommitDigest(c,changed)!=digest,"signed attribute state bound to receipt");
    changed=grant;changed.result[0]=1;Check(ItemCommitDigest(c,changed)!=digest,"receipt result bound to request");
    auto a=f::Leather();a.key=item_catalog_fixture::Bracelet();a.count=1;a.accessory=AccessorySocketState{3,2,1000,false};
    Check(ValidatePersistentItem(c,a).has_value(),"timed accessory saved");a.accessory.reset();Check(!ValidatePersistentItem(c,a),"missing timer not refilled");
    const std::array items{f::Sword(),f::Leather()};const std::array storages{f::Inventory()};std::array<std::uint8_t,16> op{};op[0]=2;
    for(unsigned recipe:{1u,3u,4u,5u})
    {
        auto plan=f::Refine(c,recipe,op,items,f::Gold());auto commit=MakeRefinementCommit(c,plan,{f::Character(),7,2},storages,items,f::Gold());
        Check(commit.has_value(),"native refinement -> full writer proposal");
        Check(commit->items.size()==2 && commit->wallets[0].after.balance==900,"materials and money share write set");
        Check(commit->items[1].after.destroyed && !commit->items[1].after.count && !commit->items[1].after.placement,"depleted material tombstone");
        Check(commit->items[0].after.attributes==items[0].attributes && commit->items[0].after.stones==items[0].stones,"refinement preserves complete state");
        Check(commit->items[0].after.destroyed==(recipe==4),"explicit failure destruction");
        if(recipe!=4)Check(commit->items[0].after.refinementLevel==(recipe==1 ? 10 : recipe==3 ? 9 : 8),"success/keep/lower level");
        for(unsigned field=0;field<4;++field){auto bad=plan;if(field==0)bad.goldCost--;if(field==1)bad.debits[0].consumeCount--;if(field==2)bad.success=!bad.success;if(field==3)bad.expectedSourceRevision++;Check(!MakeRefinementCommit(c,bad,{f::Character(),7,2},storages,items,f::Gold()),"forged refinement plan");}
    }
    auto level20=f::Sword();level20.refinementLevel=20;const std::array transformItems{level20};
    const auto transform=f::Refine(c,2,op,transformItems,f::Gold());const auto commit=MakeRefinementCommit(c,transform,{f::Character(),7,2},storages,transformItems,f::Gold());
    Check(commit && commit->items[0].after.id==level20.id && commit->items[0].after.key==f::LongSword() && commit->items[0].after.refinementLevel==0,"sword+20 -> long sword+0 preserves instance");
    auto tombstone=items[0];tombstone.destroyed=true;tombstone.count=0;tombstone.placement.reset();Check(ValidatePersistentItem(c,tombstone).has_value(),"tombstone readable");
    auto resurrect=f::Grant(c,{items[0]});resurrect.items[0].before=tombstone;resurrect.items[0].after.revision=2;Check(!ValidateItemCommit(c,resurrect),"tombstone cannot resurrect");
    auto service=f::Leather(70);service.key=attribute_fixture::Tool(2);service.count=2;
    std::array serviceInventory{f::Sword(),service};const ItemOwnerFence fence{f::Character(),7,2};
    AttributeRequest attribute{f::Pair(service,serviceInventory[0],fence,30)};attribute.state=serviceInventory[0].attributes;attribute.draws={{1,1}};
    auto attributePlan=PlanAttributeService(c,attribute);Check(attributePlan.has_value(),"reroll fixture admitted");
    auto attributeCommit=MakeAttributeCommit(c,*attributePlan,fence,storages,serviceInventory);
    Check(attributeCommit && attributeCommit->items[0].after.attributes==attributePlan->after && attributeCommit->items[1].after.count==1,"reroll binds full replacement and consumption");
    Check(attributeCommit->items[0].after.attributes.slots[2]==f::Sword().attributes.slots[2],"locked slot retained by writer");
    auto forgedAttribute=*attributePlan;forgedAttribute.after.slots[0]->valueScaled++;
    Check(!MakeAttributeCommit(c,forgedAttribute,fence,storages,serviceInventory),"forged bonus result denied");
    auto staleInventory=serviceInventory;staleInventory[0].attributes.slots[0]->valueScaled--;
    Check(!MakeAttributeCommit(c,*attributePlan,fence,storages,staleInventory),"stale complete attribute snapshot denied");
    service.key=socket_service_fixture::Tool(1);service.count=1;serviceInventory[1]=service;
    SocketServiceRequest sockets{f::Pair(service,serviceInventory[0],fence,31)};sockets.sockets=serviceInventory[0].stones;
    auto socketPlan=PlanSocketService(c,sockets);Check(socketPlan.has_value(),"clean broken fixture admitted");
    auto socketCommit=MakeSocketServiceCommit(c,*socketPlan,fence,storages,serviceInventory);
    Check(socketCommit && socketCommit->items[0].after.stones[1].state==StoneSocketState::Empty && socketCommit->items[1].after.destroyed,"clean broken socket and source consumption atomic");
    auto returns=*socketPlan;returns.returns.push_back({0,socket_service_fixture::Stone(),0,71,1});
    Check(!MakeSocketServiceCommit(c,returns,fence,storages,serviceInventory),"unimplemented returned-instance writer fails closed");
    // F4 C3: moving inside the inventory, no swap/merge/equip.
    {
        const std::vector<ItemStorageLayout> bags{f::Inventory(),{{f::Character(),ItemStorageKind::Inventory,2},5,9,1},
            {{f::Character(),ItemStorageKind::Equipment,1},32,1,1}};
        const std::vector<PersistentItem> items{f::Sword(),f::Leather()};
        std::array<std::uint8_t,16> op{};op[0]=0xC3;
        const auto at=[](ItemStorageKind kind,std::uint64_t storage,std::uint32_t cell){return ItemPlacement{kind,storage,cell};};
        auto moved=MakeMoveCommit(c,op,fence,bags,items,f::Sword().id,at(ItemStorageKind::Inventory,1,12));
        Check(moved && moved->kind==ItemOperationKind::Move && moved->items.size()==1 && moved->items[0].after.placement->anchor==12 &&
              moved->items[0].after.revision==f::Sword().revision+1 && moved->items[0].after.stones==f::Sword().stones && moved->storages.size()==1,"move inside one bag");
        auto across=MakeMoveCommit(c,op,fence,bags,items,f::Leather().id,at(ItemStorageKind::Inventory,2,0));
        Check(across && across->storages.size()==2,"move into a second inventory storage carries both layouts");
        auto onto=MakeMoveCommit(c,op,fence,bags,items,f::Sword().id,at(ItemStorageKind::Inventory,1,1));
        Check(!onto && onto.error()==ItemPersistenceError::InvalidPlacement,"occupied target refused (no swap)");
        auto same=MakeMoveCommit(c,op,fence,bags,items,f::Sword().id,at(ItemStorageKind::Inventory,1,0));
        Check(!same && same.error()==ItemPersistenceError::InvalidChange,"no-op move refused");
        auto outside=MakeMoveCommit(c,op,fence,bags,items,f::Leather().id,at(ItemStorageKind::Inventory,1,5*9*2));
        Check(!outside && outside.error()==ItemPersistenceError::InvalidPlacement,"cell outside the grid refused");
        auto edge=MakeMoveCommit(c,op,fence,bags,items,f::Leather().id,at(ItemStorageKind::Inventory,1,9));
        Check(edge.has_value(),"last column of a row accepted");
        auto equip=MakeMoveCommit(c,op,fence,bags,items,f::Sword().id,at(ItemStorageKind::Equipment,1,0));
        Check(!equip && equip.error()==ItemPersistenceError::UnsupportedState,"equipping is not a move");
        auto missing=MakeMoveCommit(c,op,fence,bags,items,ItemInstanceId::Create(77).value(),at(ItemStorageKind::Inventory,1,12));
        Check(!missing && missing.error()==ItemPersistenceError::MissingItem,"unknown item refused");
        auto nowhere=MakeMoveCommit(c,op,fence,bags,items,f::Sword().id,at(ItemStorageKind::Inventory,9,0));
        Check(!nowhere && nowhere.error()==ItemPersistenceError::InvalidStorage,"unknown storage refused");
        Check(MakeMoveCommit(c,{},fence,bags,items,f::Sword().id,at(ItemStorageKind::Inventory,1,12)).error()==ItemPersistenceError::InvalidOperation,"null operation id refused");
        auto foreign=fence;foreign.owner.id++;
        Check(!MakeMoveCommit(c,op,foreign,bags,items,f::Sword().id,at(ItemStorageKind::Inventory,1,12)),"another owner's item refused");
    }
}
}
int main(){try{Run();std::cout<<checks<<" Persistent item checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
