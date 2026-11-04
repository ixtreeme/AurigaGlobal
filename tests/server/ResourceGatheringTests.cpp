#include "ToolRuntimeFixture.hpp"
#include <item/ResourceGathering.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;namespace f=tool_runtime_fixture;namespace pf=persistent_item_fixture;
unsigned checks=0;void Check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 auto c=BuildItemCatalog(f::Catalog()).value();auto rod=f::Item();rod.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};rod.tool=ToolInstanceState{0,50};
 const ItemOwnerFence fence{pf::Character(),7,1};std::vector<ItemStorageLayout> layouts{pf::Inventory(),{{pf::Character(),ItemStorageKind::Equipment,1},8,1,1}};std::vector<PersistentItem> items{rod};
 GatheringSite site{1,41,9009,"Fishing site",ToolKind::Rod,FishingPool::First,900,2000,1,100,10000,0,0,{{f::Bait(),2,1}}};
 Check(ValidateGatheringSites(*c,{&site,1}).has_value(),"valid world rule");auto bad=site;bad.rewards[0].weight=0;Check(!ValidateGatheringSites(*c,{&bad,1}),"zero reward weight");bad=site;bad.kind=ToolKind::Pick;Check(!ValidateGatheringSites(*c,{&bad,1}),"pool belongs to rod only");
 ResourceRequest r{rod.id,f::Actor(),true};Check(CheckResourceGathering(*c,site,fence.owner,items,r)==10000,"chance saturates exactly");auto noBait=items;noBait[0].tool->baitPower=0;Check(CheckResourceGathering(*c,site,fence.owner,noBait,r).error()==ResourceError::NoBait,"bait required");
 auto carried=items;carried[0].placement=ItemPlacement{ItemStorageKind::Inventory,1,0};Check(!CheckResourceGathering(*c,site,fence.owner,carried,r),"must wear tool");bad=site;bad.minimumPower=200000;Check(!CheckResourceGathering(*c,bad,fence.owner,items,r),"minimum power");r.actor.admitted=false;Check(!CheckResourceGathering(*c,site,fence.owner,items,r),"server admission");r.actor=f::Actor();
 std::array ids{ItemInstanceId::Create(30001).value(),ItemInstanceId::Create(30002).value()};unsigned draws=0;auto draw=[&](std::uint32_t bound){++draws;return 0u;};
 auto success=MakeResourceGatheringCommit(*c,site,{27,1},fence,layouts,items,r,ids,draw);Check(success && success->success && success->points==1 && success->bait==0 && success->yields[0].count==2 && success->commit.items.size()==2,"reward, practice and bait one commit");Check(draws==1,"only fractional practice draws");
 r.completed=false;Check(MakeResourceGatheringCommit(*c,site,{27,2},fence,layouts,items,r,ids,draw).error()==ResourceError::NotCompleted,"client cannot complete event");r.completed=true;
 draws=0;Check(!MakeResourceGatheringCommit(*c,site,{27,3},fence,layouts,items,r,{},draw) && draws==0,"ID reservation checked before random");
 auto tiny=layouts;tiny[0].columns=1;tiny[0].rows=1;tiny[0].pages=1;auto full=items;auto material=pf::Leather();material.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};material.count=c->Find(material.key)->base.maxStack;full.push_back(material);draws=0;
 Check(!MakeResourceGatheringCommit(*c,site,{27,4},fence,tiny,full,r,ids,draw) && draws==0,"full bag cannot reroll");
 auto failedSource=f::Catalog();for(auto& p:failedSource.toolProfiles)for(auto& l:p.levels)l.gatherBonusBp=0;auto zero=BuildItemCatalog(failedSource).value();bad=site;bad.baseChance=0;draws=0;auto failed=MakeResourceGatheringCommit(*zero,bad,{27,5},fence,layouts,items,r,ids,draw);Check(failed && !failed->success && failed->points==0 && failed->bait==0 && failed->yields.empty() && draws==0,"failed catch clears bait without reward or practice");
 auto maximum=items;maximum[0].refinementLevel=2;maximum[0].tool->trainingPoints=0;draws=0;auto capped=MakeResourceGatheringCommit(*c,site,{27,6},fence,layouts,maximum,r,ids,draw);Check(capped && capped->points==0 && draws==0,"maximum gets reward without training RNG");
 auto invalidDraw=[](std::uint32_t n){return n;};Check(!MakeResourceGatheringCommit(*c,site,{27,7},fence,layouts,items,r,ids,invalidDraw),"out of bound RNG denied");
 for(unsigned n:{2u,3u,4u}){auto pick=f::Item(n);pick.tool->trainingPoints=0;pick.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};auto s=site;s.kind=n==2 ? ToolKind::Pick : n==3 ? ToolKind::Axe : ToolKind::Shears;s.pool=FishingPool::None;Check(MakeResourceGatheringCommit(*c,s,{27,static_cast<std::uint8_t>(n)},fence,layouts,std::span(&pick,1),{pick.id,f::Actor(),true},ids,draw).has_value(),"all non-fishing kinds complete");}
 auto weighted=site;weighted.rewards.push_back({f::Bait(2),4,3});std::vector<std::uint32_t> bounds;
 auto picked=MakeResourceGatheringCommit(*c,weighted,{27,20},fence,layouts,items,r,ids,[&](auto n){bounds.push_back(n);return n==4 ? 1u : 2u;});Check(picked && picked->yields[0].item==f::Bait(2) && picked->points==0 && bounds==std::vector<std::uint32_t>{4,3},"weighted boundary and failed fractional practice are exact");
 auto fractional=site;fractional.baseChance=0;fractional.powerStep=100000;fractional.powerChance=100;fractional.baitChance=2;Check(CheckResourceGathering(*c,fractional,fence.owner,items,r)==450,"authored power, bait and gathering bonus formula");
 auto edge=MakeResourceGatheringCommit(*c,fractional,{27,21},fence,layouts,items,r,ids,[](auto n){return n==10000 ? 449u : 0u;});Check(edge && edge->success,"last success chance value");edge=MakeResourceGatheringCommit(*c,fractional,{27,22},fence,layouts,items,r,ids,[](auto n){return n==10000 ? 450u : 0u;});Check(edge && !edge->success && edge->points==0,"first failure chance value");
 auto trained=items;trained[0].tool->trainingPoints=3;draws=0;auto cappedTraining=MakeResourceGatheringCommit(*c,site,{27,23},fence,layouts,trained,r,ids,draw);Check(cappedTraining && cappedTraining->points==3 && draws==0,"full practice does not roll or overflow");
 auto split=site;split.rewards[0].count=401;draws=0;Check(!MakeResourceGatheringCommit(*c,split,{27,24},fence,layouts,items,r,ids,draw) && draws==0,"all split reward IDs admitted before random");
 const std::array splitIds{ids[0],ids[1],ItemInstanceId::Create(30003).value()};auto stacked=MakeResourceGatheringCommit(*c,split,{27,25},fence,layouts,items,r,splitIds,draw);Check(stacked && stacked->commit.items.size()==4 && stacked->yields[0].count==401,"full quantity split by target stack limit");
 Check(success->commit.result.size()>5 && success->commit.result[success->commit.result.size()-8]==3,"RNG bound and value recorded in receipt");
 if(argc==3 && std::string_view(argv[1])=="--golden-output")
 {const std::filesystem::path folder=argv[2];auto write=[&](const char* file,const ResourceView& v){auto bytes=EncodeResourceGathering(*c,v).value();std::ofstream out(folder/file);for(auto b:bytes)out<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden");};write("typed_resource_ready_v3.txt",{ResourceStatus::Ready,255,777,&rod,&site,444,0,10000});write("typed_resource_running_v3.txt",{ResourceStatus::Running,255,777,&rod,&site,444,1500,10000});write("typed_resource_applied_v3.txt",{ResourceStatus::Applied,255,777,&rod,&site,444,0,10000,&*success});write("typed_resource_closed_v3.txt",{});}
 std::cout<<"Resource gathering: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
