#include "ICCWorldFixture.hpp"
#include "../../SRC/Server/GameServer/raid/icc/Festergut.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
}
void Require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
int main(){using namespace EclipseRaid;
 for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
  actors.clear();messages.clear();nativeAffects.clear();clockNow=0;Run run;run.mode=mode;run.encounter=5;
  actors.emplace(100,TestActor{Arenas[4].center});run.bosses.push_back(100);
  for(uint32_t i=1;i<=10;i++){auto at=Arenas[4].center;at.x+=3;at.y+=float(i)*.1f;actors.emplace(i,TestActor{at,true,false,false,i});run.members.push_back(Member{i});}
  bool spores=false,protection=false,pungent=false,goo=false,damageBoost=false;int maximumInoculated=0;
  {
   auto runtime=std::make_shared<FestergutRuntime>(run);runtime->Attacked(1,100000);
   for(uint32_t time=50;time<=305000;time+=50){clockNow=time;Require(runtime->Update(time),"source adapter has complete spell data and stays active");
    for(auto& m:run.members){if(m.aura==Mechanic::Spore)spores=true;maximumInoculated=std::max(maximumInoculated,m.stacks);if(m.stacks>0)protection=true;}
    if(runtime->Outgoing(1,1000)>1000)damageBoost=true;
    if(!nativeAffects.empty())goo=true;
   }
   for(auto& text:messages)if(text=="ER_BOSS_5_LINE_6")pungent=true;
   runtime->Status(2);
   Require(spores&&protection&&pungent&&damageBoost,"spores, expiration inoculation, pungent and tank stack bonus execute in the real adapter");
   Require(maximumInoculated==3,"multiple spore rounds reach source protection cap");
   Require(goo==ICC::Heroic(mode),"professor projectiles and temporary speed effects are heroic only");
   Require(actors.at(1).dead,"ten gastric stacks kill the original tank");
   bool enrage=false;for(auto& text:messages)if(text=="ER_BOSS_5_LINE_8")enrage=true;Require(enrage,"source five-minute enrage reaches actual world");
  }
  Require(nativeAffects.empty(),"encounter teardown removes every transient native stat effect");
 }
 std::cout<<"ICC_FESTERGUT_RUNTIME_PASS four_modes spore_inoculation pungent gastric_explosion damage_bonus heroic_goo stat_cleanup enrage\n";
}
