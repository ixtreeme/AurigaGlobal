#include "FishProcessingFixture.hpp"
#include <item/TypedItemDetails.hpp>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;namespace f=fish_processing_fixture;namespace pf=persistent_item_fixture;
unsigned checks=0;
void Check(bool b,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!b)throw std::runtime_error(std::string(why)+" line "+std::to_string(at.line()));}
int main(int argc,char** argv){try{
 const auto c=BuildItemCatalog(f::Catalog()).value();const ItemOwnerFence fence{pf::Character(),7,1};
 std::vector<ItemStorageLayout> bags{pf::Inventory()};std::vector<PersistentItem> items{f::Item(1)};
 std::vector<ItemInstanceId> ids;for(unsigned n=0;n<32;++n)ids.push_back(ItemInstanceId::Create(26000+n).value());
 auto request=f::Request(items[0]);unsigned draws=0;const LootDraw draw=[&](auto bound){++draws;Check(bound==2,"weighted fish bound");return 0;};
 auto quote=CheckFishProcessing(*c,fence.owner,items,request);Check(quote && quote->consume==1 && quote->possibilities.size()==2,"owned live fish quote");
 auto plan=MakeFishProcessingCommit(*c,{25},fence,bags,items,request,ids,draw);
 Check(plan && draws==1 && plan->yields.size()==1 && plan->yields[0].item==f::Fish(2) && plan->yields[0].count==3,"native fish draw and complete yield");
 Check(plan->commit.items.size()==2 && plan->commit.items[0].after.count==1 && plan->commit.items[1].after.count==3 && ValidateItemCommit(*c,plan->commit).has_value(),"atomic source and reward");
 const auto original=items;const auto receipt=ItemCommitDigest(*c,plan->commit);Check(receipt.has_value() && items==original,"planner does not mutate live inputs");
 auto changed=plan->commit;changed.items[0].after.count=2;Check(ItemCommitDigest(*c,changed)!=receipt,"source mutation changes replay digest");
 const std::vector<ItemStorageLayout> tiny{{{fence.owner,ItemStorageKind::Inventory,1},1,1,1}};
 draws=0;auto blocked=MakeFishProcessingCommit(*c,{26},fence,tiny,items,request,ids,draw);
 Check(!blocked && blocked.error()==FishProcessingError::NoRoom && draws==0,"all outcomes must fit before RNG");
 items[0].count=1;plan=MakeFishProcessingCommit(*c,{27},fence,tiny,items,request,ids,draw);
 Check(plan && plan->commit.items[0].after.destroyed && plan->commit.items[1].after.placement->anchor==0,"last fish frees output footprint");
 items={f::Item(2,17)};request=f::Request(items[0],FishAction::Grill);draws=0;
 auto noDraw=[&](auto){++draws;return 0;};plan=MakeFishProcessingCommit(*c,{28},fence,bags,items,request,ids,noDraw);
 Check(plan && draws==0 && plan->yields[0].count==17 && plan->commit.items[0].after.destroyed,"whole dead-fish stack grills without RNG");
 std::uint32_t total=0;for(const auto& i:plan->commit.items)if(!i.before){Check(i.after.count<=c->Find(i.after.key)->base.maxStack,"split output respects max stack");total+=i.after.count;}
 Check(total==17,"grill output never truncates count");
 request.actor.campfireAvailable=false;Check(CheckFishProcessing(*c,fence.owner,items,request).error()==FishProcessingError::CampfireRequired,"trusted fire required");
 Check(!MakeFishProcessingCommit(*c,{29},fence,bags,items,request,ids,noDraw) && draws==0,"fire lost before confirm consumes nothing");request.actor.campfireAvailable=true;
 request.actor.admitted=false;Check(CheckFishProcessing(*c,fence.owner,items,request).error()==FishProcessingError::Forbidden,"owner admission default deny");request.actor.admitted=true;
 request.actor.characterClass=4;Check(!CheckFishProcessing(*c,fence.owner,items,request),"invalid class");request.actor.characterClass=0;
 request.actor.sex=2;Check(!CheckFishProcessing(*c,fence.owner,items,request),"invalid sex");request.actor.sex=0;
 auto foreign=items;foreign[0].owner.id++;Check(!CheckFishProcessing(*c,fence.owner,foreign,request),"foreign fish denied");
 auto worn=items;worn[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};Check(!CheckFishProcessing(*c,fence.owner,worn,request),"equipped fish denied");
 auto high=f::Catalog();std::ranges::find(high.items,f::Fish(2),&ItemBaseRow::key)->minimumLevel=101;auto highCatalog=BuildItemCatalog(high).value();
 Check(CheckFishProcessing(*highCatalog,fence.owner,items,request).error()==FishProcessingError::Level,"fish minimum level enforced");
 request.action=FishAction::Use;Check(CheckFishProcessing(*c,fence.owner,items,request).error()==FishProcessingError::WrongAction,"dead fish cannot use live action");
 for(unsigned n:{3,4}){items={f::Item(n)};request=f::Request(items[0]);Check(!CheckFishProcessing(*c,fence.owner,items,request),"bait and ungrillable fish not processable");}
 items={f::Item(5)};request=f::Request(items[0]);draws=0;plan=MakeFishProcessingCommit(*c,{30},fence,bags,items,request,ids,[&](auto b){++draws;Check(b==10000,"independent fish bound");return 9999;});
 Check(plan && draws==2 && plan->yields.empty() && plan->commit.items.size()==1,"empty independent yield still consumes exactly once");
 draws=0;Check(!MakeFishProcessingCommit(*c,{31},fence,bags,items,request,{},noDraw) && draws==0,"missing IDs cannot reroll empty loot");
 auto many=f::Catalog();many.lootProfiles.back().entries.clear();for(unsigned n=1;n<=9;++n)many.lootProfiles.back().entries.push_back({static_cast<std::uint16_t>(n),LootRewardKind::Item,f::Fish(2),1,5000,0});
 auto manyCatalog=BuildItemCatalog(many).value();Check(CheckFishProcessing(*manyCatalog,fence.owner,items,request).error()==FishProcessingError::Unsupported,"uncertain-outcome budget bounded");
 items={f::Item(1)};request=f::Request(items[0]);Check(!MakeFishProcessingCommit(*c,{},fence,bags,items,request,ids,noDraw),"zero operation denied before draw");
 auto duplicate=items;duplicate.push_back(items[0]);draws=0;Check(!MakeFishProcessingCommit(*c,{32},fence,bags,duplicate,request,ids,noDraw) && draws==0,"duplicate identity denied before RNG");
 const auto liveQuote=CheckFishProcessing(*c,fence.owner,items,request).value();
 const auto packet=EncodeFishProcessing(*c,FishProcessingStatus::Ready,255,123,&items[0],FishAction::Use,0,liveQuote,{});Check(packet.has_value(),"native live golden");
 auto grilled=f::Item(2,17);const auto grillQuote=CheckFishProcessing(*c,fence.owner,std::span(&grilled,1),f::Request(grilled,FishAction::Grill)).value();const std::vector<FishingYield> yields{{item_catalog_fixture::InstantPotion(),17}};
 const auto grillPacket=EncodeFishProcessing(*c,FishProcessingStatus::Applied,255,456,&grilled,FishAction::Grill,987,grillQuote,yields);Check(grillPacket.has_value(),"native grill golden");
 const auto empty=EncodeFishProcessing(*c,FishProcessingStatus::Closed,255,0,nullptr,FishAction{},0,{},{});Check(empty && empty->size()>=61,"closed wire envelope");
 if(argc==3 && std::string_view(argv[1])=="--golden-output")
 {const auto write=[](const std::filesystem::path& path,const auto& bytes){std::ofstream out(path);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write");};const std::filesystem::path path=argv[2];write(path,*packet);write(path.parent_path()/"typed_fish_grill_v1.txt",*grillPacket);write(path.parent_path()/"typed_fish_closed_v1.txt",*empty);}
 std::cout<<"Fish processing: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
