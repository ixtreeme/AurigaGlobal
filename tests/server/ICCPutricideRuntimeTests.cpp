#include "PutricideWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
#include "../../SRC/Server/GameServer/raid/EclipsePutricideRuntime.hpp"
}
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){using namespace EclipseRaid;for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
 actors.clear();messages.clear();nativeAffects.clear();spawnedCreatureSources.clear();serial=100;clockNow=0;Run run;run.mode=mode;actors.emplace(100,TestActor{Arenas[6].center});run.bosses.push_back(100);
 for(uint32_t pid=1;pid<=4;pid++){actors.emplace(pid,TestActor{{122+float(pid),780},true,false,false,pid});run.members.push_back(Member{pid});}
 {PutricideRuntime runtime(run);runtime.Attacked(1,10000);actors.at(2).point={102,765};Require(runtime.Action(2,"interact",101),"mutation table command handled");Require(run.members[1].aura==Mechanic::Mutation,"table grants real mutation role");
  for(uint32_t t=250;t<=36000;t+=250){clockNow=t;Require(runtime.Update(t),"native alchemist update");}
  Require(std::any_of(spawnedCreatureSources.begin(),spawnedCreatureSources.end(),[](auto p){return p.second==37697;}),"volatile experiment spawned with original creature stats");Require(run.rooted.count(3)||run.rooted.count(4),"volatile ooze immobilizes non-tank target");
  auto damage=runtime.TargetDamage(1,100,2000000000);Require(damage==200000000&&run.phase==2,"overkill cannot skip 80 percent transition");actors.at(100).hp-=damage;
  Require(runtime.TargetDamage(1,100,1)==0,"boss protected during transformation");
  for(uint32_t t=36250;t<=87000;t+=250){clockNow=t;Require(runtime.Update(t),"phase-two transition resumes");}
  damage=runtime.TargetDamage(1,100,2000000000);Require(damage==450000000&&run.phase==3,"35 percent transition conserves damage boundary");Require(run.members[1].aura==Mechanic::Strike,"final phase removes mutation role");
  runtime.Status(1);Require(std::any_of(messages.begin(),messages.end(),[](auto& s){return s.rfind("er_putri ",0)==0;}),"alchemist phase energy UI packet");
 }
 Require(run.rooted.empty()&&nativeAffects.empty(),"wipe restores roots and real speed penalties");
 }
 std::cout<<"ICC_PUTRICIDE_RUNTIME_PASS native_table mutation experiment roots phase_damage_caps energy_protocol cleanup four_modes\n";
}
