#include "ToolRuntimeFixture.hpp"
#include <item/FishingChallenge.hpp>
#include <item/ResourceGathering.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;namespace f=tool_runtime_fixture;namespace pf=persistent_item_fixture;
unsigned checks=0;void Check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 std::vector<std::array<std::uint32_t,4>> positions;
 for(std::uint32_t seed=1;seed<=6;++seed)
 {
  FishingChallenge state;state.seed=seed;unsigned earned=0;
  for(std::uint32_t t=0;t<=15000;t+=25)
  {
   auto [x,y]=FishingPosition(seed,t);Check(x>=0 && x<=2600 && y>=0 && y<=2100,"fish stays in original UI water box");
   if(t%1000==0)positions.push_back({seed,t,static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y)});
   if(!FishingWon(state) && FishingInGoal(seed,t) && (state.lastAtMs==UINT32_MAX || t-state.lastAtMs>=1000)){state=ApplyFishingInput(state,t,t,state.sequence+1,true);++earned;}
  }
  Check(FishingWon(state) && earned==3,"every route allows three real time-spaced catches");
 }
 FishingChallenge s;s.seed=1;std::uint32_t goal=0;while(!FishingInGoal(1,goal))++goal;
 auto hit=ApplyFishingInput(s,goal,goal,1,true);Check(hit.hits==1 && hit.sequence==1 && hit.input==FishingInput::Hit,"server trajectory verifies hit");
 auto replay=ApplyFishingInput(hit,goal,goal,1,true);Check(replay.hits==1 && replay.input==FishingInput::BadSequence,"input replay cannot add hit");auto skipped=ApplyFishingInput(hit,goal,goal,3,true);Check(skipped.sequence==1 && skipped.hits==1,"sequence cannot skip");
 auto early=ApplyFishingInput(hit,goal+999,goal+999,2,true);Check(early.hits==1 && early.input==FishingInput::TooSoon && early.misses==0,"one-second server cooldown");
 auto future=ApplyFishingInput(s,goal,goal+251,1,true);Check(future.input==FishingInput::BadTime && future.hits==0,"future sample bound");auto old=ApplyFishingInput(s,goal+251,goal,1,true);Check(old.input==FishingInput::BadTime,"old sample bound");
 auto miss=ApplyFishingInput(s,goal,goal,1,false);Check(miss.input==FishingInput::Miss && miss.hits==0 && miss.misses==1,"water click is a miss even when fish in goal");Check(FishingExpired(miss,14000) && !FishingExpired(miss,13999),"miss costs exactly one second");
 auto expired=ApplyFishingInput(s,15000,14999,1,true);Check(expired.hits==0 && expired.input==FishingInput::Expired,"deadline cannot be rescued by delayed click");
 auto c=BuildItemCatalog(f::Catalog()).value();auto rod=f::Item();rod.tool=ToolInstanceState{0,50};rod.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};const ItemOwnerFence fence{pf::Character(),7,1};std::vector<ItemStorageLayout> layouts{pf::Inventory(),{{pf::Character(),ItemStorageKind::Equipment,1},8,1,1}};const std::array ids{ItemInstanceId::Create(31001).value()};
 GatheringSite site{1001,41,0,"Free water",ToolKind::Rod,FishingPool::First,400,15000,0,100,10000,0,0,{{f::Bait(),2,1}}};Check(ValidateGatheringSites(*c,{&site,1}).has_value(),"explicit water site supported");auto bad=site;bad.kind=ToolKind::Pick;bad.pool=FishingPool::None;Check(!ValidateGatheringSites(*c,{&bad,1}),"only rod can have a water site");
 unsigned draws=0;auto failed=MakeResourceGatheringCommit(*c,site,{28,1},fence,layouts,std::span(&rod,1),{rod.id,f::Actor(),true,false},ids,[&](auto){++draws;return 0u;});Check(failed && !failed->success && failed->points==0 && failed->bait==0 && draws==0,"failed minigame clears bait with no loot or practice RNG");
 auto success=MakeResourceGatheringCommit(*c,site,{28,2},fence,layouts,std::span(&rod,1),{rod.id,f::Actor(),true,true},ids,[](auto){return 0u;});Check(success && success->success && success->points==1,"won minigame enters same atomic reward and practice path");
 if(argc==3 && std::string_view(argv[1])=="--golden-output")
 {
  std::filesystem::path folder=argv[2];std::ofstream movement(folder/"typed_fishing_motion.txt");for(auto row:positions)movement<<row[0]<<' '<<row[1]<<' '<<row[2]<<' '<<row[3]<<'\n';if(!movement)throw std::runtime_error("movement golden");
  auto write=[&](const char* file,const ResourceView& v){auto bytes=EncodeResourceGathering(*c,v).value();std::ofstream out(folder/file);for(auto b:bytes)out<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("packet golden");};
  FishingRuntimeView view;view.state.seed=1;view.castX=986650;view.castY=269500;
  write("typed_fishing_ready_v4.txt",{ResourceStatus::Ready,255,888,&rod,&site,0,0,10000,nullptr,&view});view.state=hit;view.elapsedMs=goal;write("typed_fishing_running_v4.txt",{ResourceStatus::Running,255,888,&rod,&site,0,15000-goal,10000,nullptr,&view});view.state.hits=3;view.state.sequence=3;write("typed_fishing_applied_v4.txt",{ResourceStatus::Applied,255,888,&rod,&site,0,0,10000,&*success,&view});view.state=s;view.elapsedMs=15000;write("typed_fishing_failed_v4.txt",{ResourceStatus::Applied,255,888,&rod,&site,0,0,10000,&*failed,&view});
 }
 std::cout<<"Fishing challenge: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
