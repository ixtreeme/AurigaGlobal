#include "BloodCouncilWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseBloodCouncilRuntime.hpp"
}
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){using namespace EclipseRaid;for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
 actors.clear();messages.clear();nativeAffects.clear();spawnedCreatureSources.clear();serial=102;clockNow=0;Run run;run.mode=mode;
 for(uint32_t i=0;i<3;i++){actors.emplace(100+i,TestActor{{737+float(i)*6,745}});run.bosses.push_back(100+i);}
 for(uint32_t pid=1;pid<=4;pid++){actors.emplace(pid,TestActor{{737+float(pid),748},true,false,false,pid});run.members.push_back(Member{pid});}
 BloodCouncilRuntime runtime(run);Require(run.empowered==0,"native initial invocation");Require(runtime.TargetDamage(1,101,1000)==0,"inactive prince cannot lose health");auto amount=runtime.TargetDamage(1,100,12345);actors.at(100).hp-=amount;Require(actors.at(101).hp==actors.at(100).hp&&actors.at(102).hp==actors.at(100).hp,"active damage synchronizes all three health pools");
 for(uint32_t t=250;t<=26000;t+=250){clockNow=t;Require(runtime.Update(t),"native council update");}
 entt::entity bomb=0,nucleus=0;for(auto row:spawnedCreatureSources){if(row.second==38454)bomb=row.first;if(row.second==38369)nucleus=row.first;}
 Require(bomb&&nucleus,"original kinetic bomb and dark nucleus spawned");Require(runtime.TargetDamage(2,bomb,1000000)==0,"attacking bomb lifts without destroying it");Require(runtime.TargetDamage(3,nucleus,1000000)==0,"attacking nucleus transfers ownership without instant kill");runtime.Status(3);Require(std::any_of(messages.begin(),messages.end(),[](auto& s){return s.rfind("er_lift ",0)==0;}),"server altitude protocol");
 for(uint32_t t=26250;t<=46000;t+=250){clockNow=t;Require(runtime.Update(t),"council invocation timer");}
 Require(run.empowered!=0,"native invocation moves after45 seconds");auto active=run.bosses[run.empowered];runtime.TargetDamage(1,active,2000000000);for(auto e:run.bosses)if(e!=active)Require(actors.at(e).dead,"shared lethal damage defeats other princes");
 }
 std::cout<<"ICC_BLOOD_COUNCIL_RUNTIME_PASS shared_health inactive_immunity bomb_lift nucleus_ownership invocation shared_death four_modes\n";
}
