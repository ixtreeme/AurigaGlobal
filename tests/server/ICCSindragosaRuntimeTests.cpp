#include "SindragosaWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseSindragosaRuntime.hpp"
}
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){using namespace EclipseRaid;for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
 actors.clear();messages.clear();spawnedCreatureSources.clear();nativeAffects.clear();clockNow=0;serial=100;Run run;run.mode=mode;actors.emplace(100,TestActor{Arenas[10].center});run.bosses.push_back(100);
 for(unsigned pid=1;pid<=8;pid++){actors.emplace(pid,TestActor{{600+float(pid)*2,985},true,false,false,pid});run.members.push_back(Member{pid});}
 {SindragosaRuntime runtime(run);runtime.Flight(true);Require(runtime.TargetDamage(1,100,500)==0,"airborne dragon immune to melee");runtime.Ability(70126);clockNow=7100;runtime.Update(clockNow);Require(spawnedCreatureSources.size()==ICC::Sindragosa::TombCount(mode,true),"source tomb count across modes");Require(run.rooted.size()==spawnedCreatureSources.size(),"ice tomb actually roots victims");
  auto victim=*run.rooted.begin();entt::entity tomb=0;for(auto& actor:actors)if(actor.first>100&&Distance(actor.second.point,actors.at(victim).point)<.1f)tomb=actor.first;Require(tomb&&spawnedCreatureSources[tomb]==36980,"real source ice tomb creature");runtime.Died(tomb);Require(Valid(tomb),"native death callback preserves victim entity");Require(!run.rooted.count(victim),"destroying tomb releases its victim");
  runtime.Flight(false);Require(runtime.TargetDamage(1,100,500)==500,"landing restores damage");runtime.Ability(69762);for(auto p:Living(run))runtime.Skill(p,1);runtime.Status(1);Require(std::any_of(messages.begin(),messages.end(),[](auto& s){return s.rfind("er_sindra ",0)==0;}),"source debuff HUD");
  runtime.Ability(70117);Require(Distance(actors.at(1).point,actors.at(100).point)<4,"icy grip authoritative pull");runtime.Ability(70123);actors.at(1).point={650,1020};auto hp=actors.at(1).hp;clockNow=12300;runtime.Update(clockNow);Require(actors.at(1).hp>hp-100000,"escaping25m cold avoids lethal blast");
 }
 Require(run.rooted.empty(),"teardown releases every ice tomb root");
 }
 std::cout<<"ICC_SINDRAGOSA_RUNTIME_PASS source_tombs root_release flight_immunity grip cold_escape cleanup four_modes\n";
}
