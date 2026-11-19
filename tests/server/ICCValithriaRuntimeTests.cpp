#include "ValithriaWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseValithriaRuntime.hpp"
}
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){using namespace EclipseRaid;for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
 actors.clear();messages.clear();nativeAffects.clear();spawnedCreatureSources.clear();serial=100;clockNow=0;Run run;run.mode=mode;actors.emplace(100,TestActor{Arenas[9].center});run.bosses.push_back(100);actors.emplace(1,TestActor{{410,930},true,false,false,1});run.members.push_back(Member{1});
 {ValithriaRuntime runtime(run);Require(actors.at(100).hp==500000000,"friendly dragon starts at source50 percent");Require(spawnedCreatureSources.size()==4,"four original initial archmages");Require(runtime.TargetDamage(1,100,10000)==0,"player cannot hurt friendly dragon");
  auto hp=actors.at(100).hp;Require(runtime.Action(1,"heal",0),"healing ritual handled");Require(actors.at(100).hp-hp==Points::GetMaxHP(100)/100,"ritual restores a visible one percent of dragon health");hp=actors.at(100).hp;runtime.Action(1,"heal",0);Require(actors.at(100).hp==hp,"same tick heal spam rejected");
  runtime.Portals(1);entt::entity portal=serial;actors.at(1).point=actors.at(portal).point;runtime.Action(1,"interact",portal);Require(run.members.front().aura!=Mechanic::Portal,"portal cannot be entered during15-second warning");clockNow=15000;runtime.Update(clockNow);runtime.Action(1,"interact",portal);Require(run.members.front().aura==Mechanic::Portal,"open portal enters dream role");runtime.Status(1);Require(std::any_of(messages.begin(),messages.end(),[](auto& s){return s.rfind("er_dream ",0)==0;}),"dream healing status packet");
  clockNow=31000;Require(runtime.Update(clockNow),"dream duration expires safely");Require(run.members.front().aura==Mechanic::Heal,"dream exit returns player and keeps temporary vigor");
  actors.at(1).point={410,930};clockNow=34000;runtime.Update(clockNow);actors.at(100).hp=999999999;runtime.Action(1,"heal",0);Require(!run.completionPending,"full health schedules3.5-second victory scene");clockNow=38000;runtime.Update(clockNow);Require(run.completionPending,"healing completion unlocks next room after scene");
 }
 actors.at(100).dead=false;run.completionPending=false;clockNow=0;{ValithriaRuntime failed(run);failed.Died(100);Require(!failed.Update(250)&&!run.completionPending,"dragon death is failure not boss-kill victory");}
 }
 std::cout<<"ICC_VALITHRIA_RUNTIME_PASS friendly_dragon flat_heal anti_spam portal_warning dream_return victory_delay death_failure four_modes\n";
}
