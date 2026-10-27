#include "../../SRC/Server/GameServer/story/CaravanRules.hpp"
#include "../../SRC/Server/GameServer/story/CaravanEncounterRules.hpp"
#include "../../SRC/Server/GameServer/story/CaravanVoiceTiming.hpp"
#include "../../SRC/Server/GameServer/story/CaravanInstanceRules.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main(){using namespace CaravanQuest;
 for(unsigned stage=0;stage<Encounter::Milestones.size();stage++){assert(!Encounter::Due(stage,Encounter::Milestones[stage]-.001f));assert(Encounter::Due(stage,Encounter::Milestones[stage]));}assert(!Encounter::Due(5,1));
 assert((Encounter::Pursuers==std::array<uint32_t,6>({654,655,656,657,689,690})));
 Encounter::Hazard hazard{1,1000,10,10,0,0,false};assert(!hazard.TryHit(4199,10,10));assert(!hazard.TryHit(4200,13,10));assert(hazard.TryHit(4200,10,10));assert(!hazard.TryHit(4201,10,10));
 Encounter::Hazard old{2,1000,10,10,0,0,false};assert(!old.TryHit(11000,10,10));assert(old.Expired(11000));
 assert(!Encounter::Hit(0,0,2.1f,0));assert(Encounter::Hit(0,0,1,0));
 assert(Encounter::ImpactLane(1)==0&&Encounter::ImpactLane(2)>2&&Encounter::ImpactLane(3)<-2);assert(Encounter::Hit(0,0,Encounter::ImpactLane(1),0));assert(!Encounter::Hit(2.1f,0,Encounter::ImpactLane(1),0));
 assert(Encounter::Boardable(true,true,3,0));assert(!Encounter::Boardable(true,true,3,1));assert(!Encounter::Boardable(false,true,1,0));assert(!Encounter::Boardable(true,false,1,0));
 for(int a=0;a<3;a++)for(int b=a+1;b<3;b++)assert(std::hypot(Encounter::DeckX(a)-Encounter::DeckX(b),Encounter::DeckZ(a)-Encounter::DeckZ(b))>.8f);
 assert(Encounter::MaxBoarders==1&&Encounter::BoardingReady(1000,0)&&!Encounter::BoardingReady(18999,19000)&&Encounter::BoardingReady(19000,19000));assert(Encounter::CanyonStage(1)&&Encounter::CanyonStage(3)&&!Encounter::CanyonStage(2));assert(Encounter::ImpactLeadMetres(Vehicle::MaxSpeed)>28&&Encounter::ImpactLeadMetres(2)==13);assert(std::abs(Vehicle::MaxSpeed*3.6f-25)<.001f);
 assert(TemplateOf(4200000)==420&&TemplateOf(4209999)==420&&TemplateOf(41)==41);
 assert(!Admitted(420,420,true,true,false)); // The shared template is never a session.
 assert(Admitted(4200000,4200000,true,true,false));
 assert(Admitted(4200001,4200001,true,true,false));
 assert(!Admitted(4200000,4200001,true,true,false)); // Simultaneous parties stay isolated.
 assert(!Admitted(4200000,4200000,true,false,false));
 assert(!Admitted(4200000,4200000,true,true,true));
 assert(!Admitted(4200000,4200000,false,true,false));
 assert(!Admitted(410000,410000,true,true,false));
 assert(DialogueTiming::LoadingEnd>47.7f&&DialogueTiming::LoadingEnd<60);
 assert(DialogueTiming::ArrivalEnd>25.3f&&DialogueTiming::ArrivalEnd<40);
 assert(DialogueTiming::DeclineEnd>11.7f&&DialogueTiming::LaterEnd>12.6f);
 assert(ReadyToStart(1,1,true));assert(ReadyToStart(3,3,true));assert(!ReadyToStart(3,2,true));assert(!ReadyToStart(4,4,true));assert(!ReadyToStart(2,2,false));
 Vehicle safe;for(int i=0;i<10000;i++)safe.Step(.2f,.6f,0,true);assert(!safe.cargoLost&&safe.risk==0&&safe.speed<=Vehicle::MaxSpeed*.6f+.001f);
 Vehicle fast;for(int i=0;i<300;i++)fast.Step(.2f,1,0,true);assert(fast.cargoLost);
 Vehicle grace;grace.speed=Vehicle::MaxSpeed;for(int i=0;i<50;i++)grace.Step(.2f,1,0,true);assert(grace.risk<.00001f&&!grace.cargoLost);grace.Step(.2f,1,0,true);assert(grace.risk>.019f&&grace.risk<.021f);
 grace.Step(.2f,.8f,0,true);assert(grace.maxSpeedSeconds==0&&grace.risk==0);
 Vehicle repeated;repeated.speed=Vehicle::MaxSpeed;for(int burst=0;burst<20;burst++){for(int i=0;i<40;i++)repeated.Step(.2f,1,0,true);repeated.Step(.2f,0,0,true);}assert(repeated.risk==0&&!repeated.cargoLost);
 Vehicle nearTop;for(int i=0;i<10000;i++)nearTop.Step(.2f,.98f,0,true);assert(nearTop.risk==0&&!nearTop.cargoLost);
 Vehicle acceleration;for(int i=0;i<85;i++)acceleration.Step(.2f,1,0,true);assert(acceleration.risk==0&&acceleration.maxSpeedSeconds<10);
 Vehicle unattended;for(int i=0;i<10000;i++)unattended.Step(.2f,1,0,false);assert(unattended.speed<=2.01f&&!unattended.cargoLost);
 Vehicle turn;turn.speed=Vehicle::MaxSpeed;turn.Step(.2f,1,90,true);assert(turn.yaw>0&&turn.yaw<=Vehicle::MaxTurnRate*.2f+.001f&&std::abs(turn.speed-Vehicle::MaxSpeed)<.001f);for(int i=0;i<20;i++){turn.Step(.2f,1,turn.yaw+(i%2?90:-90),true);assert(std::abs(turn.speed-Vehicle::MaxSpeed)<.001f);}
 Vehicle noise;float maxYaw=0;for(int i=0;i<200;i++){noise.Step(.2f,.6f,i%2?2.f:-2.f,true);maxYaw=std::max(maxYaw,std::abs(noise.yaw));}assert(maxYaw<.3f);
 struct RoadPoint{int x,y;};std::vector<RoadPoint> road;for(int i=0;i<100;i++)road.push_back({i,i%2});
 float centred=RoadHeading(road,30,30.5f,.5f,3),offset=RoadHeading(road,30,30.5f,2.5f,3);assert(std::abs(Wrap(centred-offset))<.1f);
 assert(!CanUseSeat(false,true,true,0));assert(!CanUseSeat(true,false,true,0));assert(!CanUseSeat(true,true,false,1));assert(!CanUseSeat(true,true,true,3));assert(CanUseSeat(true,true,true,2));
 assert(BanditLimit(1)<BanditLimit(3));assert(std::abs(Wrap(350)+10)<.001f);
}
