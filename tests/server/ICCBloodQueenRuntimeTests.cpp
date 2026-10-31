#include "BloodQueenWorldFixture.hpp"
namespace EclipseRaid {
#include "../../SRC/Server/GameServer/raid/EclipseFestergutRuntime.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseBloodQueenRuntime.hpp"
}
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){using namespace EclipseRaid;for(auto mode:{ICC::Difficulty::Normal10,ICC::Difficulty::Normal25,ICC::Difficulty::Heroic10,ICC::Difficulty::Heroic25}){
 actors.clear();messages.clear();nativeAffects.clear();spawnedCreatureSources.clear();serial=100;clockNow=0;Run run;run.mode=mode;actors.emplace(100,TestActor{Arenas[8].center});run.bosses.push_back(100);
 for(uint32_t pid=1;pid<=5;pid++){actors.emplace(pid,TestActor{{902+float(pid)*2,875},true,false,false,pid});run.members.push_back(Member{pid});}
 {BloodQueenRuntime runtime(run);runtime.Attacked(1,100000);runtime.Attacked(5,90000);for(uint32_t t=250;t<=15500;t+=250){clockNow=t;Require(runtime.Update(t),"native queen update");}Require(runtime.Outgoing(5,1000)==2000,"first bite gives original100 percent outgoing bonus");
 for(uint32_t t=15750;t<=92000;t+=250){clockNow=t;Require(runtime.Update(t),"bloodthirst lifecycle");}Require(run.members[4].aura==Mechanic::Bite,"essence expiry enables player bite action");actors.at(4).point=actors.at(5).point;Require(runtime.Action(5,"bite",4),"bite command handled");Require(runtime.Outgoing(4,1000)==2000&&runtime.Outgoing(5,1000)==2000,"successful bite refreshes biter and empowers new ally");Require(runtime.Action(5,"bite",3),"early repeated bite rejected without passthrough");Require(runtime.Outgoing(3,1000)==1000,"cannot bite repeatedly outside thirst window");
 for(uint32_t t=92250;t<=140000;t+=250){clockNow=t;Require(runtime.Update(t),"native flight cycle");}runtime.Status(5);Require(std::any_of(messages.begin(),messages.end(),[](auto& m){return m.rfind("er_lift ",0)==0&&m.find(" 1200")!=std::string::npos;}),"flight has actual visual altitude");
 }
 Require(run.rooted.empty()&&nativeAffects.empty(),"queen wipe clears fear and control roots");
 }
 // Exercise actual adapter damage: overlapping shadows must never multiply
 // the same periodic aura, and a new patch has a real escape warning.
 actors.clear();messages.clear();clockNow=0;Run review;review.mode=ICC::Difficulty::Normal10;actors.emplace(100,TestActor{Arenas[8].center});review.bosses.push_back(100);
 for(uint32_t pid=1;pid<=3;pid++){actors.emplace(pid,TestActor{{922,895},true,false,false,pid});review.members.push_back(Member{pid});}
 {BloodQueenRuntime runtime(review);runtime.Ability(71264);for(uint32_t t=250;t<=8000;t+=250){clockNow=t;damageCalls.clear();Require(runtime.Update(t),"shadow review remains active");for(auto& calls:damageCalls){unsigned shroud=(t-250)%2000==0?1:0;Require(calls.second<=1+shroud,"overlapping shadow pools apply only one periodic hit");if(t==250)Require(calls.second==shroud,"new shadow patch gives escape time");}}}
 std::cout<<"ICC_BLOOD_QUEEN_RUNTIME_PASS original_bite_bonus thirst player_transfer anti_spam native_flight cleanup four_modes\n";
}
