#include "ICCWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseMarrowgarRuntime.hpp"
}
void Require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
int main(){using namespace EclipseRaid;
 for(uint32_t tick:{50u,250u})for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
  actors.clear();messages.clear();clockNow=0;serial=100;Run run;run.mode=mode;
  actors.emplace(100,TestActor{Arenas[0].center});run.bosses.push_back(100);
  for(uint32_t i=1;i<=10;i++){float a=float(i)*.63f;Vec point=i==1?Vec{515,315}:Vec{512+15*std::cos(a),315+15*std::sin(a)};actors.emplace(i,TestActor{point,true,false,false,i});run.members.push_back(Member{i});}
  auto runtime=std::make_shared<MarrowgarRuntime>(run);runtime->Attacked(1,100000);Require(bool(runtime),"source controller factory");
  size_t peakPins=0;bool freed=false,storm=false,flames=false,protectedTank=true;
  for(uint32_t time=tick;time<=125000;time+=tick){clockNow=time;auto before=actors.at(100).point;Require(runtime->Update(time),"fight remains active");auto after=actors.at(100).point;
   if(Distance(before,after)>.01f){storm=true;Require(Distance(before,after)<=25*tick*.001f+.03f,"storm charge respects 25 metres per second at both server tick rates");}
   peakPins=std::max(peakPins,run.rooted.size());if(run.rooted.count(1))protectedTank=false;
   if(time%1000==0&&!run.rooted.empty()){
    auto pid=*run.rooted.begin();auto member=Participant(run,pid);Require(member&&member->object,"root has destructible spike owner");auto object=member->object;Require(spawnedCreatureSources.at(object)==36619,"spike uses original creature health in every raid mode");runtime->Status(pid);Require(std::find(messages.begin(),messages.end(),"er_effect 69065 1 0")!=messages.end(),"pinned player receives rescue status");actors.at(object).dead=true;runtime->Died(object);
    clockNow=time+1;Require(runtime->Update(time+1),"spike death leaves encounter active");Require(!run.rooted.count(pid),"destroying spike frees its passenger");freed=true;
   }
  }
  bool stormMarker=false;for(auto& message:messages){if(message.rfind("er_hazard ",0)==0)flames=true;if(message.rfind("er_marker ",0)==0&&message.find(" 100 3 900 ")!=std::string::npos)stormMarker=true;}
  Require(peakPins==(ICC::Raid25(mode)?3u:1u),"source raid-size spike count reaches actual world");Require(protectedTank,"tank excluded by actual spell dispatcher");Require(freed&&storm&&flames,"world adapter executes rescue, movement and ground dispatch");
  Require(actors.at(1).hp<1000000000,"melee damages actual target");
  Require(stormMarker,"source storm publishes a moving boss marker");
  // The fixture does not pretend to prove rendering, real sockets or mob stats.
 }
 // Explicit solo-GM spike diagnostic uses the same spawn/root/release path.
 actors.clear();messages.clear();clockNow=0;serial=100;Run debugRun;
 actors.emplace(100,TestActor{Arenas[0].center});debugRun.bosses.push_back(100);
 actors.emplace(1,TestActor{Vec{515,315},true,false,true,1});debugRun.members.push_back(Member{1});
 auto debug=std::make_shared<MarrowgarRuntime>(debugRun);
 Require(debug->TestSpike(1),"explicit spike diagnostic can pin a solo GM");
 Require(!debug->TestSpike(1),"diagnostic cannot duplicate an existing spike");
 auto spike=Participant(debugRun,1)->object;
 Require(std::find(messages.begin(),messages.end(),"er_pin 1 "+std::to_string(spike))!=messages.end(),"pin visual is broadcast to the party");
 actors.at(spike).dead=true;debug->Died(spike);clockNow=50;debug->Update(clockNow);
 Require(!debugRun.rooted.count(1),"diagnostic spike releases GM on the next server update");
 Require(std::find(messages.begin(),messages.end(),"er_unpin 1")!=messages.end(),"release visual is broadcast to the party");
 std::cout<<"ICC_MARROWGAR_RUNTIME_PASS four_modes spell_dispatch spike_rescue tank_exclusion charge_speed ground_protocol solo_gm_diagnostic pin_visual_protocol\n";
}
