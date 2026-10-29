#include "ICCWorldFixture.hpp"
#include "../../SRC/Server/GameServer/raid/icc/Festergut.hpp"
#include "../../SRC/Server/GameServer/raid/icc/Rotface.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseRotfaceRuntime.hpp"
}
void Require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
int main(){using namespace EclipseRaid;
 for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
  actors.clear();messages.clear();nativeAffects.clear();clockNow=0;serial=100;Run run;run.mode=mode;run.encounter=6;
  actors.emplace(100,TestActor{Arenas[5].center});run.bosses.push_back(100);
  for(uint32_t i=1;i<=10;i++){auto at=Arenas[5].center;at.x+=3;at.y+=float(i)*.1f;actors.emplace(i,TestActor{at,true,false,false,i});run.members.push_back(Member{i});}
  bool infection=false,cleansed=false,spawned=false,big=false,explosion=false,slow=false,heroicGas=false;
  {
   auto runtime=std::make_shared<RotfaceRuntime>(run);runtime->Attacked(1,100000);
   for(uint32_t time=50;time<=451000;time+=50){clockNow=time;Require(runtime->Update(time),"source adapter remains active");
    for(auto& m:run.members)if(m.aura==Mechanic::Infection){Require(m.pid!=1,"actual infection dispatcher excludes tank");infection=true;auto spell=ICC::Spells().Resolve(ICC::RotfaceSpell::Infection,mode);auto reduction=spell->effects[1].basePoints+1;Require(runtime->Healing(m.pid,1000)==1000*(100+reduction)/100,"source healing reduction reaches the live adapter");if(!cleansed){Require(runtime->Dispel(m.pid),"real dispel removes infection");Require(runtime->Healing(m.pid,1000)==1000,"dispel immediately restores healing");cleansed=true;}}
    if(serial>100)spawned=true;if(!nativeAffects.empty())slow=true;if(!run.rooted.empty())heroicGas=true;
   }
   runtime->Status(2);
   for(auto& text:messages){if(text.find("er_actor_scale ")==0&&text.find(" 200")!=std::string::npos)big=true;if(text=="ER_BOSS_6_LINE_5")explosion=true;}
   Require(infection&&cleansed&&spawned&&big&&explosion&&slow,"infection, dispel summon, merges, explosions and flood slows all execute");Require(heroicGas==ICC::Heroic(mode),"vile gas control is heroic only");
  }
  Require(nativeAffects.empty(),"no temporary movement stat leaks after encounter teardown");
 }
 std::cout<<"ICC_ROTFACE_RUNTIME_PASS four_modes infection healing_reduction dispel_ooze merges explosion flood_slow heroic_gas cleanup\n";
}
