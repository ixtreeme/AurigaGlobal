#include "PersistentItemFixture.hpp"
#include <item/ContainerOpening.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=persistent_item_fixture;namespace base=item_catalog_fixture;
namespace
{
unsigned checks=0;
void Check(bool okay,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!okay)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
ItemCatalogSource Source()
{auto source=f::Catalog();source.lootProfiles[0].entries={{1,LootRewardKind::Item,base::Leather(),3,1,0},{2,LootRewardKind::Gold,{},75,1,0}};
 source.lootProfiles[1].entries={{1,LootRewardKind::Item,base::Leather(),3,10000,0},{2,LootRewardKind::Gold,{},75,10000,0}};return source;}
PersistentItem Item(ItemKey key,std::uint64_t id,std::uint32_t cell,std::uint32_t count=1)
{PersistentItem item{ItemInstanceId::Create(id).value(),key,f::Character()};item.count=count;item.placement=ItemPlacement{ItemStorageKind::Inventory,1,cell};return item;}
std::vector<std::byte> Run()
{
    const auto catalog=BuildItemCatalog(Source()).value();const auto& c=*catalog;
    const auto box=Item(base::GiftBox(),10,0,2),chest=Item(base::IronChest(),12,0),key=Item(base::IronKey(),11,1,2);
    const auto fence=ItemOwnerFence{f::Character(),7,1};const std::vector<ItemStorageLayout> bags{f::Inventory()};const std::vector<ItemWallet> wallets{f::Gold()};
    std::vector<ItemInstanceId> ids;for(unsigned i=100;i<132;++i)ids.push_back(ItemInstanceId::Create(i).value());
    unsigned draws=0;const LootDraw itemDraw=[&](std::uint32_t bound){++draws;Check(bound==2,"weighted RNG receives exact total weight");return 0;};
    std::vector<PersistentItem> inventory{box};ContainerOpeningRequest request{box.id,{}, {100,0,0,false}};
    auto plan=MakeContainerOpeningCommit(c,{13},fence,bags,inventory,wallets,request,ids,itemDraw);
    Check(plan && draws==1 && plan->won==std::vector<std::uint16_t>{1},"weighted draw performed exactly once after admission");
    Check(plan->commit.items.size()==2 && plan->commit.items[0].after.count==1 && plan->commit.items[1].after.count==3 && plan->commit.items[1].after.refinementLevel==0,"box and base-level reward share one commit");
    Check(plan->commit.items[1].after.id==ids[0] && ValidateItemCommit(c,plan->commit).has_value(),"durable result uses reserved ID and validates");
    draws=0;plan=MakeContainerOpeningCommit(c,{14},fence,bags,inventory,wallets,request,ids,[&](auto){++draws;return 1;});
    Check(plan && draws==1 && plan->gold==75 && plan->commit.wallets[0].after.balance==1075 && plan->won==std::vector<std::uint16_t>{2},"gold award and box consumption are atomic");
    const std::vector<ItemStorageLayout> tiny{{{f::Character(),ItemStorageKind::Inventory,1},1,1,1}};
    draws=0;const auto full=MakeContainerOpeningCommit(c,{15},fence,tiny,inventory,wallets,request,ids,[&](auto){++draws;return 1;});
    Check(!full && full.error()==ContainerOpeningError::NoRoom && draws==0,"full bag cannot reroll until the gold-only result wins");
    auto one=box;one.count=1;inventory={one};plan=MakeContainerOpeningCommit(c,{16},fence,tiny,inventory,wallets,request,ids,[](auto){return 0;});
    Check(plan && plan->commit.items[0].after.destroyed && plan->commit.items[1].after.placement->anchor==0,"whole consumed box frees the reward footprint");
    auto rich=wallets;rich[0].balance=INT64_MAX;draws=0;
    const auto overflow=MakeContainerOpeningCommit(c,{17},fence,bags,inventory,rich,request,ids,[&](auto){++draws;return 0;});
    Check(!overflow && overflow.error()==ContainerOpeningError::WalletLimit && draws==0,"wallet admission checks every possible reward before RNG");
    inventory={chest,key};request={chest.id,key.id,{100,0,0,false}};draws=0;
    plan=MakeContainerOpeningCommit(c,{18},fence,bags,inventory,wallets,request,ids,[&](auto bound){++draws;Check(bound==10000,"independent draw bound");return 0;});
    Check(plan && draws==2 && plan->won==std::vector<std::uint16_t>({1,2}) && plan->gold==75,"locked chest draws the complete independent bundle");
    Check(plan->commit.items.size()==3 && plan->commit.items[0].after.destroyed && plan->commit.items[1].after.count==1 && plan->commit.wallets.size()==1,"key, chest, item and gold in one mutation");
    request.key.reset();draws=0;auto failure=MakeContainerOpeningCommit(c,{19},fence,bags,inventory,wallets,request,ids,[&](auto){++draws;return 0;});
    Check(!failure && failure.error()==ContainerOpeningError::KeyRequired && draws==0,"missing key refuses before RNG");
    request.key=chest.id;Check(!CheckContainerOpening(c,request,f::Character(),inventory),"a container cannot serve as its own key");request.key=key.id;
    auto modified=Source();modified.containerKeys[0].lockId++;const auto wrongCatalog=BuildItemCatalog(modified).value();
    Check(CheckContainerOpening(*wrongCatalog,request,f::Character(),inventory).error()==ContainerOpeningError::WrongKey,"explicit lock identity must match");
    inventory[1].owner.id++;Check(!CheckContainerOpening(c,request,f::Character(),inventory),"foreign key never admitted");inventory[1]=key;inventory[1].boundTo=f::Character();
    modified=Source();std::ranges::find(modified.items,key.key,&ItemBaseRow::key)->binding=BindingPolicy::OnAcquire;
    const auto boundCatalog=BuildItemCatalog(modified).value();Check(CheckContainerOpening(*boundCatalog,request,f::Character(),inventory).has_value(),"own bound keys are usable");inventory[1]=key;
    request.actor.characterClass=4;Check(!CheckContainerOpening(c,request,f::Character(),inventory),"invalid actor class denied");request.actor.characterClass=0;
    modified=Source();std::ranges::find(modified.items,chest.key,&ItemBaseRow::key)->minimumLevel=101;const auto levelCatalog=BuildItemCatalog(modified).value();
    Check(CheckContainerOpening(*levelCatalog,request,f::Character(),inventory).error()==ContainerOpeningError::Level,"minimum level enforced");
    modified=Source();modified.lootProfiles[1].entries[1].kind=LootRewardKind::Experience;const auto xpCatalog=BuildItemCatalog(modified).value();draws=0;
    Check(!MakeContainerOpeningCommit(*xpCatalog,{20},fence,bags,inventory,wallets,request,ids,[&](auto){++draws;return 0;}) && draws==0,"unsupported unsampled effects disable the whole profile");
    modified=Source();modified.lootProfiles[0].entries={{1,LootRewardKind::Item,base::Sword(),1,1,10000}};
    modified.attributeBirthProfiles={{1,"birth.container",10000,10000,{{0,1,10000},{1,1,10000},{2,1,10000}}}};
    ItemDefinitionRules birth{base::Sword()};birth.birthAttributeProfile=1;modified.definitionRules={birth};const auto rare=BuildItemCatalog(modified).value();
    Check(ValidateContainerContents(*rare,box.key).has_value(),"explicit rare-attribute reward is supported");
    const auto rarePlan=MakeContainerOpeningCommit(*rare,{0xB7},fence,bags,std::array{box},wallets,{box.id,{}, {255,0,0,false},0},ids,[](auto){return 0;});
    Check(rarePlan.has_value(),"actual container issues a magic award with its own chance override");
    const auto born=std::ranges::find_if(rarePlan->commit.items,[](const auto& ch){return !ch.before && ch.after.key==base::Sword();});
    Check(born!=rarePlan->commit.items.end() && std::ranges::count_if(born->after.attributes.slots,[](const auto& b){return b.has_value();})==3,"100 percent chest policy overrides zero prototype magic chance");
    Check(!ValidateContainerContents(c,base::QuestBox()),"scripted box is not a normal loot draw");
    modified=Source();modified.lootProfiles[1].entries.clear();for(unsigned i=1;i<=9;++i)modified.lootProfiles[1].entries.push_back({static_cast<std::uint16_t>(i),LootRewardKind::Item,base::Leather(),1,5000,0});
    const auto tooMany=BuildItemCatalog(modified).value();Check(!ValidateContainerContents(*tooMany,chest.key),"independent outcome enumeration has a bounded budget");
    modified.lootProfiles[1].entries.pop_back();const auto eight=BuildItemCatalog(modified).value();draws=0;
    plan=MakeContainerOpeningCommit(*eight,{21},fence,bags,inventory,wallets,request,ids,[&](auto){++draws;return 9999;});
    Check(plan && draws==8 && plan->won.empty() && plan->commit.items.size()==2 && plan->commit.wallets.empty(),"empty independent outcome consumes once with a valid empty-result receipt");
    draws=0;Check(!MakeContainerOpeningCommit(c,{22},fence,bags,inventory,wallets,request,{},[&](auto){++draws;return 0;}) && draws==0,"missing ID reservation refuses before RNG");
    const std::vector<std::uint64_t> keys{key.id.Value()};const std::vector<std::uint16_t> won{1,2};
    const auto packet=EncodeContainerOpening(c,0x123456789abcdefULL,&chest,&key,1,ContainerOpeningStatus::Opened,keys,won);
    Check(packet && EncodeContainerOpening(c,0,nullptr,nullptr,0,ContainerOpeningStatus::Closed,{},{})->size()==69,"golden and terminal container wire envelopes");
    return *packet;
}
void WorldRewardAdmission()
{
    const auto fence=ItemOwnerFence{f::Character(),7,1};const auto box=Item(base::GiftBox(),301,0,2);const std::array bags{f::Inventory()};const std::array wallets{f::Gold()};const std::array ids{ItemInstanceId::Create(302).value()};
    for(auto kind:{LootRewardKind::SpawnMonster,LootRewardKind::SpawnMonsterGroup}) {
        auto source=Source();source.lootProfiles[0].entries={{1,kind,{},101,1,0}};const auto c=BuildItemCatalog(source).value();Check(ValidateContainerContents(*c,box.key).has_value(),"world creature rewards have a real opening path");unsigned draws=0;
        ContainerOpeningRequest request{box.id,{},{100,0,0,false,false}};Check(!MakeContainerOpeningCommit(*c,{71},fence,bags,std::array{box},wallets,request,ids,[&](auto){++draws;return 0;}) && draws==0,"all world targets must be admitted by the server before RNG");
        request.actor.worldSpawnsAdmitted=true;const auto plan=MakeContainerOpeningCommit(*c,{72},fence,bags,std::array{box},wallets,request,{},[&](auto){++draws;return 0;});Check(plan && draws==1 && plan->worldRewards==std::vector<ContainerWorldReward>{{kind,101}} && plan->won==std::vector<std::uint16_t>{1},"selected world reward retains its exact creature definition and won entry");Check(plan->commit.items.size()==1 && plan->commit.items[0].after.count==1 && ValidateItemCommit(*c,plan->commit).has_value(),"world-only opening consumes once without issuing a fake inventory item");
        const auto retry=MakeContainerOpeningCommit(*c,{72},fence,bags,std::array{box},wallets,request,{},[](auto){return 0;});Check(retry && retry->worldRewards==plan->worldRewards && retry->commit.result==plan->commit.result,"same selected world result has a stable receipt");
        unsigned calls=0;const auto sink=[&](const ContainerWorldReward& value){Check(value==plan->worldRewards[0],"dispatcher preserves exact source world target");++calls;};Check(DispatchContainerWorldRewards(plan->worldRewards,false,false,sink)==0 && DispatchContainerWorldRewards(plan->worldRewards,true,true,sink)==0 && calls==0,"failed persistence and receipt replay never dispatch world rewards");Check(DispatchContainerWorldRewards(plan->worldRewards,true,false,sink)==1 && calls==1,"only successful new persistence dispatches world rewards");
    }
    {auto source=Source();ItemDefinitionRules disabled{box.key};disabled.available=false;source.definitionRules={disabled};const auto c=BuildItemCatalog(source).value();Check(!CheckContainerOpening(*c,{box.id,{},{100,0,0,false,true}},f::Character(),std::array{box}),"temporarily unavailable original assets cannot be bypassed with an existing source instance");}
    for(auto nonitem:{LootRewardKind::Nothing,LootRewardKind::SpawnMonsterGroup}) {
        auto source=Source();source.lootProfiles[0].mode=LootPickMode::IndependentChance;source.lootProfiles[0].entries={{1,nonitem,{},nonitem==LootRewardKind::Nothing ? 0u : 101u,5000,0},{2,LootRewardKind::Item,base::Leather(),1,5000,0}};const auto c=BuildItemCatalog(source).value();auto small=f::Inventory();small.columns=small.rows=small.pages=1;unsigned draws=0;
        const auto result=MakeContainerOpeningCommit(*c,{73},fence,std::array{small},std::array{box},wallets,{box.id,{},{100,0,0,false,true}},ids,[&](auto){++draws;return 0;});Check(!result && result.error()==ContainerOpeningError::NoRoom && draws==0,"empty/world outcomes never shift uncertain item mask bits or enable an inventory reroll");
    }
}
std::vector<std::byte> RefinedReward()
{
    auto source=Source();source.lootProfiles[1].entries[0]={1,LootRewardKind::Item,base::Sword(),1,10000,0,9};
    const auto c=BuildItemCatalog(source).value();
    const auto chest=Item(base::IronChest(),22,0),key=Item(base::IronKey(),23,1);
    auto plain=Item(base::Sword(),24,2);plain.refinementLevel=0;
    const std::vector<PersistentItem> items{chest,key,plain};
    const std::vector<ItemStorageLayout> bags{f::Inventory()};const std::vector<ItemWallet> wallets{f::Gold()};
    const auto resultId=ItemInstanceId::Create(90).value();unsigned draws=0;
    auto result=MakeContainerOpeningCommit(*c,{50},{f::Character(),7,1},bags,items,wallets,{chest.id,key.id,{100,0,0,false}},std::array{resultId},[&](auto){++draws;return 0;});
    Check(result && draws==2,"refined reward draws only after complete admission");
    const auto granted=std::ranges::find(result->commit.items,resultId,[](const auto& change){return change.after.id;});
    Check(granted!=result->commit.items.end() && granted->after.key==base::Sword() && granted->after.refinementLevel==9 && granted->after.count==1,"container grants the exact +9 state instead of +0");
    Check(std::ranges::none_of(result->commit.items,[&](const auto& change){return change.after.id==plain.id;}),"existing +0 weapon is unchanged");
    auto changed=source;changed.lootProfiles[1].entries[0].refinementLevel=8;
    Check(BuildItemCatalog(changed).value()->ContentHash()!=c->ContentHash(),"reward level participates in the catalog hash");
    changed.lootProfiles[1].entries[0].refinementLevel=static_cast<std::uint16_t>(source.refinements[1].maxLevel+1);
    Check(!BuildItemCatalog(changed),"unavailable reward refinement state cannot publish");
    changed=source;changed.lootProfiles[1].entries[1].refinementLevel=1;
    Check(!BuildItemCatalog(changed),"currency cannot carry a refinement level");
    changed=source;changed.refinementRecipes.clear();changed.refinements[1].kind=RefinementKind::ExactState;
    changed.refinements[1].maxLevel=200;changed.refinements[1].levels={{200,{},{}}};changed.lootProfiles[1].entries[0].refinementLevel=200;
    const auto exact=BuildItemCatalog(changed).value();
    const std::vector<PersistentItem> exactItems{chest,key};
    const auto exactPlan=MakeContainerOpeningCommit(*exact,{51},{f::Character(),7,1},bags,exactItems,wallets,{chest.id,key.id,{100,0,0,false}},std::array{resultId},[](auto){return 0;});
    Check(exactPlan && std::ranges::any_of(exactPlan->commit.items,[](const auto& change){return change.after.refinementLevel==200;}),"single exact +200 state grants without an invented +0 state");
    const std::vector<std::uint64_t> keys{key.id.Value()};const std::vector<std::uint16_t> won{1,2};
    return EncodeContainerOpening(*c,123,&chest,&key,1,ContainerOpeningStatus::Opened,keys,won).value();
}
}
void LiveContentsSnapshot()
{
    auto source=Source();source.containers[1].lootProfile=source.containers[0].lootProfile;
    const auto baseCatalog=BuildItemCatalog(source).value();const auto pinned=baseCatalog->ContentHash();
    const auto first=baseCatalog->WithRewards({},{{base::GiftBox(),LootPickMode::IndependentChance,{{1,LootRewardKind::Item,base::Leather(),2,10000,0,0}}}}).value();
    const auto second=baseCatalog->WithRewards({},{{base::GiftBox(),LootPickMode::IndependentChance,{{1,LootRewardKind::Item,base::Leather(),3,10000,0,0}}}}).value();
    Check(baseCatalog->ContentHash()==pinned && first->ContentHash()!=pinned && second->ContentHash()!=first->ContentHash(),"live loot has an independent validated hash");
    const auto unselected=std::get<ItemContainerRow>(second->Find(base::IronChest())->category);
    Check(second->FindLoot(*unselected.lootProfile)->Data().entries[0].amount==3,"unselected chest sharing a base profile is unchanged");
    const auto box=Item(base::GiftBox(),500,0,2);const std::vector<PersistentItem> inventory{box};
    const std::vector<ItemStorageLayout> bags{f::Inventory()};const std::vector<ItemWallet> wallets{f::Gold()};
    const std::array ids{ItemInstanceId::Create(600).value()};const ItemOwnerFence fence{f::Character(),7,1};
    auto oldPlan=MakeContainerOpeningCommit(*baseCatalog,{70},fence,bags,inventory,wallets,{box.id,{}, {100,0,0,false}},ids,[](auto){return 0;},first.get());
    auto newPlan=MakeContainerOpeningCommit(*baseCatalog,{71},fence,bags,inventory,wallets,{box.id,{}, {100,0,0,false}},ids,[](auto){return 0;},second.get());
    Check(oldPlan && newPlan && oldPlan->commit.items.back().after.count==2 && newPlan->commit.items.back().after.count==3,"in-flight and new openings use their captured reward versions");
    Check(oldPlan->commit.catalogHash==pinned && newPlan->commit.catalogHash==pinned && ValidateItemCommit(*baseCatalog,newPlan->commit).has_value(),"live container grant retains the persistence catalog pin");
    Check(!baseCatalog->WithRewards({},{{base::Leather(),LootPickMode::WeightedOne,{{1,LootRewardKind::Item,base::Leather(),1,1,0,0}}}}),"a material cannot become a live container");
}
int main(int argc,char** argv)
{try{WorldRewardAdmission();LiveContentsSnapshot();const auto ordinary=Run(),refined=RefinedReward();if(argc==3 && (std::string_view(argv[1])=="--write-golden" || std::string_view(argv[1])=="--write-refined-golden")){const auto& bytes=std::string_view(argv[1])=="--write-golden" ? ordinary : refined;std::ofstream out(argv[2]);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write");}std::cout<<checks<<" container opening checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
