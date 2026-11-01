#include "PersistentItemFixture.hpp"
#include <item/Consumables.hpp>
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
    // F4 C4: equip into the item's own slot (swap back when it fits), unequip into free cells.
    {
        const std::vector<ItemStorageLayout> bags{f::Inventory(),{{f::Character(),ItemStorageKind::Equipment,1},8,1,1}};
        std::array<std::uint8_t,16> op{};op[0]=0xC4;
        const EquipContext hero{10,0,0},novice{3,0,0};
        PersistentItem armor{ItemInstanceId::Create(200).value(),item_catalog_fixture::Armor(),f::Character()};armor.placement=ItemPlacement{ItemStorageKind::Inventory,1,20};
        PersistentItem spare{ItemInstanceId::Create(201).value(),f::LongSword(),f::Character()};spare.placement=ItemPlacement{ItemStorageKind::Inventory,1,3};
        const std::vector<PersistentItem> items{f::Sword(),f::Leather(),armor,spare};
        auto equip=MakeEquipCommit(c,op,fence,bags,items,f::Sword().id,hero);
        Check(equip && equip->items.size()==1 && equip->items[0].after.placement==ItemPlacement{ItemStorageKind::Equipment,1,0} &&
              !equip->items[0].after.boundTo && equip->storages.size()==2,"sword equipped into the weapon slot");
        auto low=MakeEquipCommit(c,op,fence,bags,items,armor.id,novice);
        Check(!low && low.error()==ItemPersistenceError::RequirementNotMet,"armor level requirement");
        auto body=MakeEquipCommit(c,op,fence,bags,items,armor.id,hero);
        Check(body && body->items[0].after.placement->anchor==1,"armor goes to the body slot");
        Check(MakeEquipCommit(c,op,fence,bags,items,f::Leather().id,hero).error()==ItemPersistenceError::UnsupportedState,"material is not equipment");
        // Swap: the worn sword takes the spare's freed place.
        auto worn=items;worn[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};
        auto swap=MakeEquipCommit(c,op,fence,bags,worn,spare.id,hero);
        Check(swap && swap->items.size()==2 && swap->items[0].after.placement==ItemPlacement{ItemStorageKind::Equipment,1,0} &&
              swap->items[1].after.id==f::Sword().id && swap->items[1].after.placement==spare.placement,"equip swaps the worn item back");
        auto off=MakeUnequipCommit(c,op,fence,bags,worn,f::Sword().id,ItemPlacement{ItemStorageKind::Inventory,1,12});
        Check(off && off->items[0].after.placement->kind==ItemStorageKind::Inventory,"unequip into free cells");
        auto blocked=MakeUnequipCommit(c,op,fence,bags,worn,f::Sword().id,ItemPlacement{ItemStorageKind::Inventory,1,1});
        Check(!blocked && blocked.error()==ItemPersistenceError::InvalidPlacement,"unequip onto an occupied cell refused");
        Check(MakeUnequipCommit(c,op,fence,bags,items,f::Sword().id,ItemPlacement{ItemStorageKind::Inventory,1,10}).error()==ItemPersistenceError::UnsupportedState,"unequip of a carried item refused");
        Check(MakeUnequipCommit(c,op,fence,bags,worn,f::Sword().id,ItemPlacement{ItemStorageKind::Equipment,1,0}).error()==ItemPersistenceError::UnsupportedState,"unequip into equipment refused");
        const std::vector<ItemStorageLayout> carryOnly{f::Inventory()};
        Check(MakeEquipCommit(c,op,fence,carryOnly,items,f::Sword().id,hero).error()==ItemPersistenceError::InvalidStorage,"no equipment storage");
        // Class/sex rules, OnEquip binding and a swap that does not fit back.
        auto source=f::Catalog();
        source.usePolicies[0].allowedClasses=0b0001;source.usePolicies[0].allowedSexes=0b01;
        for(auto& base:source.items)
        {
            if(base.key==item_catalog_fixture::Sword())base.binding=BindingPolicy::OnEquip;
            if(base.key==f::LongSword())base.height=1;
        }
        auto strictCatalog=BuildItemCatalog(source);Check(strictCatalog.has_value(),"strict fixture catalog");
        const auto& strict=**strictCatalog;
        Check(MakeEquipCommit(strict,op,fence,bags,items,f::Sword().id,EquipContext{10,2,0}).error()==ItemPersistenceError::RequirementNotMet,"class rule");
        Check(MakeEquipCommit(strict,op,fence,bags,items,f::Sword().id,EquipContext{10,0,1}).error()==ItemPersistenceError::RequirementNotMet,"sex rule");
        auto bound=MakeEquipCommit(strict,op,fence,bags,items,f::Sword().id,hero);
        Check(bound && bound->items[0].after.boundTo==f::Character(),"OnEquip binds when equipped");
        auto small=items;small[3].placement=ItemPlacement{ItemStorageKind::Inventory,1,44};small[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};small[0].boundTo=f::Character();
        auto tight=MakeEquipCommit(strict,op,fence,bags,small,spare.id,hero);
        Check(!tight && tight.error()==ItemPersistenceError::InvalidPlacement,"a worn item that does not fit back blocks the swap");
    }
    // F4 C5: using up units of an inventory item; the last unit destroys it.
    {
        std::array<std::uint8_t,16> op{};op[0]=0xC5;
        PersistentItem potion{ItemInstanceId::Create(300).value(),item_catalog_fixture::InstantPotion(),f::Character()};
        potion.count=5;potion.placement=ItemPlacement{ItemStorageKind::Inventory,1,30};
        const std::vector<PersistentItem> items{f::Sword(),f::Leather(),potion};
        const std::vector<ItemStorageLayout> bags{f::Inventory()};
        Check(ValidatePersistentItem(c,potion).has_value(),"an instant potion is a persistent item");
        auto automatic=potion;automatic.key=item_catalog_fixture::AutomaticPotion();automatic.count=1;
        Check(ValidatePersistentItem(c,automatic).error()==ItemPersistenceError::UnsupportedState,"the automatic potion's charge is not stored yet");
        auto sip=MakeConsumeCommit(c,op,fence,bags,items,potion.id,1);
        Check(sip && sip->kind==ItemOperationKind::Consume && sip->items.size()==1 && sip->items[0].after.count==4 &&
              sip->items[0].after.placement==potion.placement && sip->items[0].after.revision==potion.revision+1 &&
              sip->storages.size()==1,"one unit used");
        auto last=MakeConsumeCommit(c,op,fence,bags,items,potion.id,5);
        Check(last && last->items[0].after.destroyed && !last->items[0].after.count && !last->items[0].after.placement,"the last unit destroys the item");
        Check(MakeConsumeCommit(c,op,fence,bags,items,potion.id,6).error()==ItemPersistenceError::InvalidChange,"more units than carried");
        Check(MakeConsumeCommit(c,op,fence,bags,items,potion.id,0).error()==ItemPersistenceError::InvalidChange,"zero units");
        Check(MakeConsumeCommit(c,{},fence,bags,items,potion.id,1).error()==ItemPersistenceError::InvalidOperation,"null operation id");
        Check(MakeConsumeCommit(c,op,fence,bags,items,ItemInstanceId::Create(77).value(),1).error()==ItemPersistenceError::MissingItem,"missing item");
        auto worn=items;worn[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};
        const std::vector<ItemStorageLayout> withGear{f::Inventory(),{{f::Character(),ItemStorageKind::Equipment,1},8,1,1}};
        Check(MakeConsumeCommit(c,op,fence,withGear,worn,f::Sword().id,1).error()==ItemPersistenceError::UnsupportedState,"an equipped item is not used up");
        Check(MakeConsumeCommit(c,op,fence,{},items,potion.id,1).error()==ItemPersistenceError::InvalidStorage,"no storage layout");
        auto foreign=fence;foreign.owner.id++;
        Check(!MakeConsumeCommit(c,op,foreign,bags,items,potion.id,1),"another owner's item refused");
        // The planner and the builder together: the plan's units are what is spent.
        InstantRecoveryAdmission who;who.pools={1000000,3000000,0,1000000};who.context=RecoveryContext::World;
        who.level=10;who.availableUnits=potion.count;
        auto plan=PlanInstantRecovery(c,potion.key,who);
        Check(plan && plan->hpRestoredRaw>0 && plan->unitsToConsume==1,"the fixture potion heals");
        auto spent=MakeConsumeCommit(c,op,fence,bags,items,potion.id,plan->unitsToConsume);
        Check(spent && spent->items[0].after.count==4,"the plan's units are spent");
    }
    // F4 C7a: a grant tops up matching stacks, then takes the first free cells; all or nothing.
    {
        namespace k=item_catalog_fixture;
        std::array<std::uint8_t,16> op{};op[0]=0xC7;
        const std::vector<ItemStorageLayout> bags{f::Inventory(),{{f::Character(),ItemStorageKind::Equipment,1},8,1,1}};
        const std::vector<PersistentItem> items{f::Sword(),f::Leather()}; // sword: cells 0,5,10; three leather: cell 1
        const std::array ids{ItemInstanceId::Create(500).value(),ItemInstanceId::Create(501).value(),ItemInstanceId::Create(502).value()};
        auto topUp=MakeGrantCommit(c,op,fence,bags,items,{k::Leather(),0,7},ids);
        Check(topUp && topUp->kind==ItemOperationKind::Grant && topUp->items.size()==1 && topUp->items[0].before &&
              topUp->items[0].after.count==10 && topUp->items[0].after.revision==f::Leather().revision+1 && topUp->storages.size()==1,"a matching stack takes the units");
        auto overflow=MakeGrantCommit(c,op,fence,bags,items,{k::Leather(),0,250},ids);
        Check(overflow && overflow->items.size()==2 && overflow->items[0].after.count==200 && !overflow->items[1].before &&
              overflow->items[1].after.count==53 && overflow->items[1].after.id==ids[0] && overflow->items[1].after.placement->anchor==2,"a full stack spills into a new one");
        auto drills=MakeGrantCommit(c,op,fence,bags,items,{k::SocketDrill(),0,45},ids);
        Check(drills && drills->items.size()==3 && drills->items[0].after.count==20 && drills->items[2].after.count==5 &&
              drills->items[0].after.placement->anchor==2 && drills->items[1].after.placement->anchor==3 && drills->items[2].after.placement->anchor==4 &&
              drills->items[2].after.id==ids[2],"new stacks of 20 in the first free cells, IDs from the front");
        auto blade=MakeGrantCommit(c,op,fence,bags,items,{k::Sword(),9,1},ids);
        Check(blade && blade->items.size()==1 && blade->items[0].after.refinementLevel==9 && blade->items[0].after.placement->anchor==2 &&
              blade->items[0].after.revision==1 && !blade->items[0].after.boundTo,"a new sword at its level, where its three rows fit");
        Check(MakeGrantCommit(c,op,fence,bags,items,{k::SocketDrill(),0,45},std::span(ids).first(2)).error()==ItemPersistenceError::LimitExceeded,"too few IDs");
        const std::vector<ItemStorageLayout> tiny{{{f::Character(),ItemStorageKind::Inventory,1},2,2,1}};
        Check(MakeGrantCommit(c,op,fence,tiny,{},{k::Sword(),0,1},ids).error()==ItemPersistenceError::InvalidPlacement,"no room is refused, nothing half given");
        const std::vector<ItemStorageLayout> gearOnly{{{f::Character(),ItemStorageKind::Equipment,1},8,1,1}};
        Check(MakeGrantCommit(c,op,fence,gearOnly,{},{k::Leather(),0,1},ids).error()==ItemPersistenceError::InvalidStorage,"no inventory storage");
        Check(MakeGrantCommit(c,op,fence,bags,items,{k::Leather(),0,0},ids).error()==ItemPersistenceError::InvalidChange,"zero units");
        Check(MakeGrantCommit(c,{},fence,bags,items,{k::Leather(),0,1},ids).error()==ItemPersistenceError::InvalidOperation,"null operation id");
        Check(MakeGrantCommit(c,op,fence,bags,items,{ItemKey::Create(1,999).value(),0,1},ids).error()==ItemPersistenceError::InvalidItem,"unknown item");
        const std::array taken{f::Leather().id};
        Check(MakeGrantCommit(c,op,fence,bags,items,{k::SocketDrill(),0,1},taken).error()==ItemPersistenceError::InvalidChange,"an ID already in use");
        // OnAcquire: a new unit is bound to its owner, and joins the owner's bound stack.
        auto source=f::Catalog();
        for(auto& base:source.items)if(base.key==k::Leather())base.binding=BindingPolicy::OnAcquire;
        auto boundCatalog=BuildItemCatalog(source);Check(boundCatalog.has_value(),"OnAcquire fixture catalog");
        const std::vector<PersistentItem> swordOnly{f::Sword()};
        auto first=MakeGrantCommit(**boundCatalog,op,fence,bags,swordOnly,{k::Leather(),0,5},ids);
        Check(first && first->items.size()==1 && !first->items[0].before && first->items[0].after.boundTo==f::Character(),"a new OnAcquire unit is bound to its owner");
        auto mine=f::Leather();mine.boundTo=f::Character();const std::vector<PersistentItem> boundItems{f::Sword(),mine};
        auto joined=MakeGrantCommit(**boundCatalog,op,fence,bags,boundItems,{k::Leather(),0,5},ids);
        Check(joined && joined->items.size()==1 && joined->items[0].before && joined->items[0].after.count==8,"it joins the owner's bound stack");
        // F4 C7b-3: drops wait per owner; one item state merges up to a stack.
        std::deque<ItemGrant> pending;
        Check(QueueGrant(pending,{k::Leather(),0,150},200) && QueueGrant(pending,{k::Leather(),0,50},200) &&
              pending.size()==1 && pending[0].count==200,"one item state merges up to one stack");
        Check(QueueGrant(pending,{k::Leather(),0,1},200) && pending.size()==2 && pending[1].count==1,"a full stack starts another grant");
        Check(QueueGrant(pending,{k::Leather(),1,1},200) && pending.size()==3,"another level is another item state");
        Check(QueueGrant(pending,{k::Sword(),0,1},1) && QueueGrant(pending,{k::Sword(),0,1},1) && pending.size()==5,"one-unit stacks never merge");
        Check(QueueGrant(pending,{k::Leather(),0,2},200) && pending[1].count==3 && pending.size()==5,"a later drop joins the first waiting grant with room");
        Check(!QueueGrant(pending,{k::Leather(),0,0},200) && pending.size()==5,"zero units are refused");
        while(pending.size()<MaxPendingGrants)pending.push_back({k::Sword(),0,1});
        Check(!QueueGrant(pending,{k::Sword(),0,1},1) && pending.size()==MaxPendingGrants,"a full queue refuses and keeps its grants");
        Check(QueueGrant(pending,{k::Leather(),0,1},200) && pending[1].count==4 && pending.size()==MaxPendingGrants,"a full queue still merges");
    }
    // F4 C6: refinement from the owner's state; equipment never takes part.
    {
        std::array<std::uint8_t,16> op{};op[0]=0xC6;
        const std::vector<ItemStorageLayout> bags{f::Inventory(),{{f::Character(),ItemStorageKind::Equipment,1},8,1,1}};
        PersistentItem worn{ItemInstanceId::Create(400).value(),f::LongSword(),f::Character()};worn.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};
        const std::vector<PersistentItem> items{f::Sword(),f::Leather(),worn};
        const std::vector<ItemWallet> purse{f::Gold()};
        const auto use=[](const auto& r,RefinementUseError e){return !r && std::holds_alternative<RefinementUseError>(r.error()) && std::get<RefinementUseError>(r.error())==e;};
        const auto store=[](const auto& r,ItemPersistenceError e){return !r && std::holds_alternative<ItemPersistenceError>(r.error()) && std::get<ItemPersistenceError>(r.error())==e;};
        auto done=MakeOwnerRefinement(c,fence,bags,items,purse,{op,f::Sword().id,1,0,true});
        Check(done && done->plan.success && done->commit.kind==ItemOperationKind::Refine && done->commit.items.size()==2 &&
              done->commit.items[0].after.refinementLevel==10 && done->commit.items[1].after.destroyed &&
              done->commit.wallets.size()==1 && done->commit.wallets[0].after.balance==900,"refine with an equipped item beside it");
        Check(done->commit.storages.size()==1 && done->commit.storages[0].storage.kind==ItemStorageKind::Inventory,"the equipment is not part of the write set");
        auto lost=MakeOwnerRefinement(c,fence,bags,items,purse,{op,f::Sword().id,4,0,true});
        Check(lost && !lost->plan.success && lost->commit.items[0].after.destroyed && lost->commit.outcomeCode==2,"a failed attempt under the destroy rule");
        auto kept=MakeOwnerRefinement(c,fence,bags,items,purse,{op,f::Sword().id,3,0,true});
        Check(kept && !kept->plan.success && kept->commit.items[0].after.refinementLevel==9 && kept->commit.wallets[0].after.balance==900,"a failed attempt under the keep rule still pays");
        Check(use(MakeOwnerRefinement(c,fence,bags,items,purse,{op,f::Sword().id,1,0,false}),RefinementUseError::Forbidden),"the server's admission is required");
        Check(use(MakeOwnerRefinement(c,fence,bags,items,purse,{op,f::Sword().id,1,5,true}),RefinementUseError::InvalidDecision),"a certain recipe takes no roll");
        Check(use(MakeOwnerRefinement(c,fence,bags,items,purse,{op,f::Sword().id,900,0,true}),RefinementUseError::MissingRecipe),"unknown recipe");
        auto poor=f::Gold();poor.balance=50;const std::vector<ItemWallet> thin{poor};
        Check(use(MakeOwnerRefinement(c,fence,bags,items,thin,{op,f::Sword().id,1,0,true}),RefinementUseError::InsufficientGold),"not enough gold");
        Check(store(MakeOwnerRefinement(c,fence,bags,items,{},{op,f::Sword().id,1,0,true}),ItemPersistenceError::InvalidWallet),"no gold wallet");
        Check(store(MakeOwnerRefinement(c,fence,bags,items,purse,{op,worn.id,1,0,true}),ItemPersistenceError::UnsupportedState),"an equipped item is not refined");
        Check(store(MakeOwnerRefinement(c,fence,bags,items,purse,{op,ItemInstanceId::Create(77).value(),1,0,true}),ItemPersistenceError::MissingItem),"missing item");
        Check(store(MakeOwnerRefinement(c,fence,bags,items,purse,{{},f::Sword().id,1,0,true}),ItemPersistenceError::InvalidOperation),"null operation id");
        auto foreign=fence;foreign.owner.id++;
        Check(store(MakeOwnerRefinement(c,foreign,bags,items,purse,{op,f::Sword().id,1,0,true}),ItemPersistenceError::MissingItem),"another owner's item refused");
    }
}
}
int main(){try{Run();std::cout<<checks<<" Persistent item checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
