#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>
namespace CaravanQuest {
enum class Phase:int {Offer,Waiting,Countdown,Loading,Travelling,Arrival,Failed,Complete,Declined,Later};
inline float Approach(float value,float target,float amount){return value<target?std::min(value+amount,target):std::max(value-amount,target);}
inline float Wrap(float degrees){while(degrees>180)degrees-=360;while(degrees<-180)degrees+=360;return degrees;}
struct Vehicle {
 static constexpr float MaxSpeed=25.f/3.6f;
 static constexpr float MaxTurnRate=42.f;
 float speed=0,yaw=0,risk=0,turnRate=0,maxSpeedSeconds=0;int hp=10000;bool cargoLost=false;
 void Step(float dt,float throttle,float desiredYaw,bool occupied){
  dt=std::clamp(dt,0.f,.25f);float target=occupied?std::clamp(throttle,0.f,1.f)*MaxSpeed:2.f;
  speed=Approach(speed,target,dt*(target<speed?1.8f:.65f));
  float error=Wrap(desiredYaw-yaw);
  float wanted=std::abs(error)<=1.25f?0.f:std::copysign(std::min(MaxTurnRate,(std::abs(error)-1.25f)*2.2f),error);
  turnRate+=(wanted-turnRate)*(1-std::exp(-dt*4.5f));yaw=Wrap(yaw+turnRate*dt);
  // Steering changes heading only; speed responds to throttle/braking, never
  // to the corner angle. The damped yaw rate keeps the wagon stable at 25 km/h.
  // Count actual continuous top speed, not time holding the accelerator while
  // accelerating. Braking or leaving the driver's seat resets the grace.
  bool fullSpeed=occupied&&throttle>=.999f&&speed>=MaxSpeed-.001f;
  float previousDanger=std::max(0.f,maxSpeedSeconds-10.f);
  maxSpeedSeconds=fullSpeed?maxSpeedSeconds+dt:0.f;
  float dangerTime=fullSpeed?std::max(0.f,maxSpeedSeconds-10.f)-previousDanger:0.f;
  risk=std::clamp(risk+dangerTime*.1f-(fullSpeed?0.f:dt*.22f),0.f,1.f);
  if(risk>=1)cargoLost=true;
 }
};
// Average the rasterized road and look metres ahead instead of aiming at the
// next three grid cells. Inside a three-metre corridor, follow its direction
// without pulling the driver back onto the centre line at every packet.
template<class Point> float RoadHeading(const std::vector<Point>& road,size_t closest,float x,float y,float speed){
 auto sample=[&](size_t index){std::array<float,2> p{};size_t first=index>4?index-4:0,last=std::min(road.size()-1,index+4);for(size_t k=first;k<=last;k++){p[0]+=road[k].x+.5f;p[1]+=road[k].y+.5f;}float n=float(last-first+1);p[0]/=n;p[1]/=n;return p;};
 float ahead=std::clamp(10.f+speed*2.f,12.f,20.f),travel=0;size_t far=closest;
 while(far+1<road.size()&&travel<ahead){travel+=std::hypot(float(road[far+1].x-road[far].x),float(road[far+1].y-road[far].y));far++;}
 auto near=sample(closest),goal=sample(far);float dx=goal[0]-near[0],dy=goal[1]-near[1],length=std::hypot(dx,dy);
 if(length<.01f)return std::atan2(goal[0]-x,y-goal[1])*180.f/3.14159265358979323846f;
 float offset=std::hypot(x-near[0],y-near[1]),correction=std::clamp((offset-3.f)/5.f,0.f,1.f);
 dx-=(x-near[0])*correction;dy-=(y-near[1])*correction;
 return std::atan2(dx,-dy)*180.f/3.14159265358979323846f;
}
inline bool ReadyToStart(int participants,int ready,bool present){return participants>=1&&participants<=3&&ready==participants&&present;}
inline bool CanUseSeat(bool participant,bool travelling,bool alive,int seat){return participant&&travelling&&alive&&seat>=0&&seat<=2;}
inline int BanditLimit(int participants){return 3+std::clamp(participants,1,3)*2;}
}
