#include "PersistentItemFixture.hpp"
#include "../../SRC/Server/GameServer/mission/ItemDelivery.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=persistent_item_fixture;
namespace m=auriga::mission;
namespace
{
unsigned checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    const auto catalog=BuildItemCatalog(f::Catalog()).value();const auto& c=*catalog;
    const auto material=f::Leather().key;const auto sword=f::Sword().key;
    ItemMission mission{7,1,"mission.item_probe","Item delivery","Bring ore to the merchant",5,{{material,0,10}},{{material,0,4}},{{sword,9,1},{material,0,2}},25,100};
    const auto proof=m::ItemDeliveryProof(mission,42);auto definitions=std::make_shared<m::DefinitionRegistry>();
    Check(definitions->Publish(m::ItemDeliveryDefinition(mission,9003)).errors.empty(),"existing Mission kernel accepts hand-in graph");
    auto delivery=m::BeginItemDelivery(definitions,mission,{42,7},9003,1000);
    Check(delivery && delivery->runtime->Inspect(1)->status==m::MissionStatus::Active,"reward waits for durable receipt");
    Check(!m::BeginItemDelivery(definitions,mission,{42,7},9004,1000),"wrong NPC never reaches reward action");
    auto items=std::vector{f::Leather()};items[0].count=10;
    const std::vector<ItemStorageLayout> bags{f::Inventory()};const std::vector<ItemWallet> wallets{f::Gold()};const auto fence=ItemOwnerFence{f::Character(),7,1};
    const std::vector<ItemInstanceId> ids{ItemInstanceId::Create(700).value()};
    const auto plan=MakeItemMissionCommit(c,mission,proof,5,fence,bags,items,wallets,ids);
    Check(plan.has_value(),"item mission planned");
    Check(plan->items.size()==2 && plan->items[0].after.count==8 && plan->items[0].after.revision==2,"consume and reward coalesce on original stack once");
    Check(plan->items[1].after.id==ids[0] && plan->items[1].after.refinementLevel==9 && plan->wallets[0].after.balance==1075,"exact result level and net gold in same commit");
    Check(ValidateItemCommit(c,*plan).has_value() && plan->result==*ItemMissionReceipt(mission,proof),"complete mutation and semantic receipt validate together");
    Check(m::FinishItemDelivery(*delivery,1001) && delivery->runtime->Inspect(1)->status==m::MissionStatus::Completed,"confirmed item effect acknowledges real Mission runtime");
    auto changed=mission;changed.name="Renamed quest";Check(ItemMissionDigest(changed)==ItemMissionDigest(mission),"presentation rename does not permit farming");
    changed=mission;std::ranges::reverse(changed.rewards);Check(ItemMissionDigest(changed)==ItemMissionDigest(mission),"set ordering does not change contract");
    changed=mission;changed.goldReward++;Check(ItemMissionDigest(changed)!=ItemMissionDigest(mission) && ItemMissionOperation(m::ItemDeliveryProof(changed,42))==ItemMissionOperation(proof),"same version economic change is a receipt conflict, not a second reward");
    changed.version++;Check(ItemMissionOperation(m::ItemDeliveryProof(changed,42))!=ItemMissionOperation(proof),"explicit new version has a new action identity");
    Check(ItemMissionOperation(m::ItemDeliveryProof(mission,43))!=ItemMissionOperation(proof),"receipt bound to character");
    Check(!MakeItemMissionCommit(c,mission,proof,4,fence,bags,items,wallets,ids),"minimum level enforced");
    items[0].count=9;Check(!MakeItemMissionCommit(c,mission,proof,5,fence,bags,items,wallets,ids),"requirement checks preexisting count, not reward-financed count");
    items[0].count=10;items[0].boundTo=f::Character();auto boundSource=f::Catalog();
    std::ranges::find(boundSource.items,material,&ItemBaseRow::key)->binding=BindingPolicy::OnAcquire;
    const auto boundCatalog=BuildItemCatalog(boundSource).value();
    Check(MakeItemMissionCommit(*boundCatalog,mission,proof,5,fence,bags,items,wallets,ids).has_value(),"own bound quest tokens can be consumed");items[0].boundTo.reset();
    items[0].placement=ItemPlacement{ItemStorageKind::TradeEscrow,1,0};Check(CountMissionItems(items,f::Character(),material,0)==0,"escrow never satisfies carried requirement");
    auto custom=f::Sword(50);Check(!MissionItemUsable(custom,f::Character()),"modified weapon is not an automatic quest cost");
    auto poor=wallets;poor[0].balance=24;items[0].placement=ItemPlacement{ItemStorageKind::Inventory,1,1};
    Check(MakeItemMissionCommit(c,mission,proof,5,fence,bags,items,poor,ids).error()==ItemMissionError::NoMoney,"payment shortage rejects whole bundle");
    changed=mission;changed.requirements.clear();changed.costs={{material,0,10}};changed.rewards={{material,0,10}};
    Check(MakeItemMissionCommit(c,changed,m::ItemDeliveryProof(changed,42),5,fence,bags,items,wallets,ids).has_value(),"whole consumed source may become reward footprint");
    changed=mission;changed.rewards.push_back(changed.rewards.front());Check(!ValidateItemMission(c,changed),"duplicate reward key and level rejected");
    changed=mission;changed.goldReward=INT64_MAX;Check(MakeItemMissionCommit(c,changed,m::ItemDeliveryProof(changed,42),5,fence,bags,items,wallets,ids).error()==ItemMissionError::Limit,"gold overflow rejected");
    ItemMissionBoard board{9003,"Mission board",{mission}};const std::vector<std::uint32_t> completed{7};
    const auto packet=EncodeItemMissionBoard(c,&board,12345,0x123456789abcdefULL,1,ItemMissionStatus::Completed,7,completed);
    Check(packet.has_value() && EncodeItemMissionBoard(c,nullptr,0,0,0,ItemMissionStatus::Closed,0,{})->size()==38,"golden and closed mission packets");return *packet;
}
}
int main(int argc,char** argv)
{try{const auto bytes=Run();if(argc==3 && std::string_view(argv[1])=="--write-golden"){std::ofstream out(argv[2]);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write");}std::cout<<checks<<" item mission checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
