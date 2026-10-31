#include "GunshipWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseGunshipRuntime.hpp"
}
void Require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){using namespace EclipseRaid;
 for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
  actors.clear();messages.clear();spawnedCreatureSources.clear();serial=100;clockNow=0;Run run;run.mode=mode;
  actors.emplace(100,TestActor{Arenas[2].center});run.bosses.push_back(100);actors.emplace(1,TestActor{{499,538},true,false,false,1});run.members.push_back(Member{1});
  GunshipRuntime runtime(run);Require(spawnedCreatureSources.size()==3,"three interactable cannons spawned");
  Require(runtime.TargetDamage(1,100,100000)==0,"hand attacks cannot bypass siege hull mechanic");
  auto hp=actors.at(100).hp;Require(runtime.Action(1,"interact",101),"cannon interaction handled");Require(actors.at(100).hp<hp,"server cannon damages hostile hull");Require(std::any_of(messages.begin(),messages.end(),[](auto& line){std::istringstream in(line);std::string command;uint32_t run,event,spell,stage,source;in>>command>>run>>event>>spell>>stage>>source;return command=="er_dwfx"&&stage==1&&source==101;}),"projectile originates from cannon not player");
  hp=actors.at(100).hp;runtime.Action(1,"interact",101);Require(actors.at(100).hp==hp,"same tick repeated interaction does not shoot");
  actors.at(1).point={530,550};clockNow=2500;runtime.Update(clockNow);runtime.Action(1,"interact",101);Require(actors.at(100).hp==hp,"remote cannon interaction rejected");
  for(uint32_t t=2750;t<=31500;t+=250){clockNow=t;Require(runtime.Update(t),"siege native adapter active");}
  entt::entity mage=0;for(auto row:spawnedCreatureSources)if(row.second==37117)mage=row.first;
  Require(mage!=0,"freeze mage source creature spawned");runtime.Status(1);Require(std::any_of(messages.begin(),messages.end(),[](auto& s){return s.rfind("er_siege ",0)==0;}),"per-player hull heat status");Require(std::any_of(messages.begin(),messages.end(),[](auto& line){return line=="er_siege_target 17 100";}),"server publishes statue aim target for every player");
  actors.at(1).point={499,538};hp=actors.at(100).hp;runtime.Action(1,"cannon",101);Require(actors.at(100).hp==hp,"freeze actually blocks native cannon");
  actors.at(mage).dead=true;runtime.Died(mage);runtime.Action(1,"cannon",101);Require(actors.at(100).hp<hp,"killing mage restores cannon damage");
  Require(!runtime.Action(1,"unrelated",0),"unrelated raid commands remain available");
  Require(!run.completionPending,"repair and boarders cannot fake victory");
 }
 std::cout<<"ICC_GUNSHIP_RUNTIME_PASS native_spawn hull_filter shot_range shared_cooldown freeze_thaw protocol four_modes\n";
}
