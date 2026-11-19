#include "SaurfangWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseSaurfangRuntime.hpp"
}
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){using namespace EclipseRaid;for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
 actors.clear();messages.clear();nativeAffects.clear();spawnedCreatureSources.clear();serial=100;clockNow=0;Run run;run.mode=mode;actors.emplace(100,TestActor{Arenas[3].center});run.bosses.push_back(100);
 for(uint32_t pid=1;pid<=4;pid++){actors.emplace(pid,TestActor{{512+float(pid),665},true,false,false,pid});run.members.push_back(Member{pid});}
 {SaurfangRuntime runtime(run);runtime.Attacked(1,1000);auto hp=actors.at(1).hp;for(uint32_t t=250;t<=41000;t+=250){clockNow=t;Require(runtime.Update(t),"native champion stays active");}
 Require(actors.at(1).hp<hp,"native melee and blood spells inflict damage");Require(spawnedCreatureSources.size()==(ICC::Raid25(mode)?5u:2u),"source-backed blood beast wave");for(auto row:spawnedCreatureSources)Require(row.second==38508,"blood beasts use correct original creature stats");
 Require(!nativeAffects.empty()==ICC::Heroic(mode),"heroic scent applies actual movement penalty");
 Require(runtime.MarkChampion(),"mark can select an eligible player");runtime.Status(2);Require(std::any_of(messages.begin(),messages.end(),[](auto& s){return s.rfind("er_blood ",0)==0;}),"blood power UI protocol");
 actors.at(100).hp=200000000;runtime.Update(clockNow+250);Require(std::count(messages.begin(),messages.end(),"ER_BLOOD_FRENZY")==1,"real boss health triggers frenzy");runtime.Update(clockNow+500);Require(std::count(messages.begin(),messages.end(),"ER_BLOOD_FRENZY")==1,"no duplicate frenzy notice");
 }
 Require(nativeAffects.empty(),"wipe cleans all transient speed penalties");
 }
 std::cout<<"ICC_SAURFANG_RUNTIME_PASS native_adapter blood_beasts power_hud mark frenzy heroic_slow cleanup four_modes\n";
}
