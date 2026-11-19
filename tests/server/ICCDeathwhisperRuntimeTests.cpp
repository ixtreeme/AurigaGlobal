#include "DeathwhisperWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseDeathwhisperRuntime.hpp"
}
void Require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){using namespace EclipseRaid;
 for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
  actors.clear();messages.clear();nativeAffects.clear();spawnedCreatureSources.clear();serial=100;clockNow=0;Run run;run.mode=mode;actors.emplace(100,TestActor{Arenas[1].center});run.bosses.push_back(100);
  for(uint32_t i=1;i<=10;i++){actors.emplace(i,TestActor{{512+float(i),430},true,false,false,i});run.members.push_back(Member{i});}
  auto runtime=std::make_shared<DeathwhisperRuntime>(run);Require(run.phase==1&&run.barrier==1000000000,"real world adapter initializes barrier");runtime->Attacked(1,10000);auto hp=actors.at(1).hp;
  for(uint32_t t=250;t<=16000;t+=250){clockNow=t;Require(runtime->Update(t),"phase one adapter stays active");}
  Require(spawnedCreatureSources.size()==(ICC::Raid25(mode)?7u:3u),"correct cultist wave in native adapter");Require(actors.at(1).hp<hp,"spell projectiles damage living players");
  auto overflow=runtime->TargetDamage(1,100,1000000050);Require(overflow==50&&run.barrier==0&&run.phase==2,"real adapter consumes shield and retains overflow");
  for(uint32_t t=16250;t<=72000;t+=250){clockNow=t;Require(runtime->Update(t),"phase two adapter stays active");}
  Require(std::any_of(messages.begin(),messages.end(),[](auto& m){return m.find(" 71420 1 ")!=std::string::npos;}),"native frost projectile protocol");Require(std::any_of(messages.begin(),messages.end(),[](auto& m){return m=="ER_DW_SHADE";}),"native shades actually spawned");Require(!run.rooted.count(1),"tank never mind controlled");runtime->Status(1);Require(std::any_of(messages.begin(),messages.end(),[](auto& m){return m.rfind("er_dwstate ",0)==0;}),"barrier phase UI state published");
  runtime.reset();Require(run.rooted.empty()&&nativeAffects.empty(),"wipe removes movement and native debuffs");
 }
 std::cout<<"ICC_DEATHWHISPER_RUNTIME_PASS native_adapter four_modes barrier casts adds shades root_cleanup state_protocol\n";
}
