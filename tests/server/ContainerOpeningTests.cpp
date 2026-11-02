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
    modified=Source();modified.lootProfiles[0].entries[0].rareAttributeChanceBp=1;const auto rare=BuildItemCatalog(modified).value();
    Check(!ValidateContainerContents(*rare,box.key),"unsupported rare-attribute policy cannot be silently dropped");
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
}
int main(int argc,char** argv)
{try{const auto bytes=Run();if(argc==3 && std::string_view(argv[1])=="--write-golden"){std::ofstream out(argv[2]);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write");}std::cout<<checks<<" container opening checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
