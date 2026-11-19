#include "LichKingWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseLichKingRuntime.hpp"
}
void Require(bool b,const char* s){if(!b){std::cerr<<"ASSERTION: "<<s<<std::endl;throw std::runtime_error(s);}}
int main(){using namespace EclipseRaid;for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
 actors.clear();messages.clear();spawnedCreatureSources.clear();clockNow=0;serial=100;Run run;run.mode=mode;actors.emplace(100,TestActor{Arenas[11].center});run.bosses.push_back(100);for(unsigned pid=1;pid<=6;pid++){actors.emplace(pid,TestActor{{510+float(pid),1110},true,false,false,pid});run.members.push_back(Member{pid});}
 {LichKingRuntime runtime(run);Require(runtime.TargetDamage(1,100,999999999)==300000000,"overkill capped at first phase boundary");actors.at(100).hp=700000000;clockNow=250;runtime.Update(clockNow);Require(run.phase==2&&runtime.TargetDamage(1,100,1000)==0,"winter cannot be damaged through");
  runtime.Ability(70337);Require(runtime.Dispel(2)||runtime.Dispel(3)||runtime.Dispel(4)||runtime.Dispel(5)||runtime.Dispel(6)||runtime.Dispel(1),"plague uses actual server dispel hook");runtime.Ability(69037);clockNow=4000;runtime.Update(clockNow);Require(!run.rooted.empty(),"valkyr actually captures player");entt::entity valk=0;for(auto row:spawnedCreatureSources)if(row.second==36609){valk=row.first;break;}Require(valk!=0,"valkyr source actor");for(auto row:spawnedCreatureSources)if(row.second==36609){runtime.Died(row.first);Require(Valid(row.first),"death callback must not destroy active ECS victim");}Require(run.rooted.empty(),"killing all valkyrs frees every passenger");
  for(unsigned i=0;i<260;i++){clockNow+=250;runtime.Update(clockNow);}Require(run.phase==3,"runtime first winter transitions");actors.at(100).hp=400000000;clockNow+=250;runtime.Update(clockNow);Require(run.phase==4,"runtime second transition");for(unsigned i=0;i<260;i++){clockNow+=250;runtime.Update(clockNow);}Require(run.phase==5,"runtime final combat opens");actors.at(100).hp=100000000;clockNow+=250;runtime.Update(clockNow);Require(run.phase==7&&!run.completionPending,"10 percent starts outro not immediate win");for(unsigned i=0;i<144;i++){clockNow+=250;runtime.Update(clockNow);}Require(run.completionPending,"scripted rescue completes raid");
 }Require(run.rooted.empty(),"all roots removed on teardown");
 }
 std::cout<<"ICC_LICH_KING_RUNTIME_PASS threshold_caps winters plague_dispel valkyr_rescue finale cleanup four_modes\n";
}
