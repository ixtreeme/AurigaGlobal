#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace EclipseRaid {
constexpr int MapIndex=430, BaseX=4608000, BaseY=4608000;
constexpr int Width=1024,Height=1280,Capacity=30,EncounterCount=12;
constexpr uint32_t FirstRace=62001;
struct Vec {float x=0,y=0;};
inline float Distance(Vec a,Vec b){return std::hypot(a.x-b.x,a.y-b.y);}
inline bool Due(uint32_t now,uint32_t at){return int32_t(now-at)>=0;}
inline int Template(int map){return map>=10000?map/10000:map;}
inline bool IsMap(int map){return Template(map)==MapIndex;}
inline bool Private(int map){return map>=MapIndex*10000&&map<(MapIndex+1)*10000;}
inline uint16_t Prerequisites(int encounter){switch(encounter){case 1:return 0;case 2:return 1;case 3:return 2;case 4:return 4;case 5:case 6:case 8:case 10:return 8;case 7:return 16|32;case 9:return 128;case 11:return 512;case 12:return 64|256|1024;default:return 0xffff;}}
inline bool Unlocked(uint16_t cleared,int encounter){return encounter>=1&&encounter<=12&&(cleared&Prerequisites(encounter))==Prerequisites(encounter);}
inline bool Cleared(uint16_t mask,int encounter){return encounter>=1&&encounter<=12&&(mask&(1u<<(encounter-1)));}
inline uint16_t Clear(uint16_t mask,int encounter){return encounter>=1&&encounter<=12?uint16_t(mask|(1u<<(encounter-1))):mask;}
struct Arena {Vec center;float radius;};
inline constexpr std::array<Arena,12> Arenas={{{{512,315},32},{{512,430},32},{{512,550},39},{{512,665},31},{{287,705},33},{{287,855},33},{{122,780},40},{{737,745},38},{{902,875},43},{{407,930},36},{{612,985},41},{{512,1110},43}}};
enum class State:uint8_t {Waiting,Combat,Intermission,Complete,Retiring};
enum class Shape:uint8_t {Circle,Cone,Line,Ring};
enum class Mechanic:uint8_t {Strike,Flame,Spike,Storm,Barrier,Cultist,Control,Siege,Mark,Gas,Spore,Infection,Slime,Experiment,Mutation,Invocation,Kinetic,Bite,Pact,Portal,Heal,IceTomb,FrostBomb,Defile,Valkyrie,Quake,Soul};
struct Hazard {uint32_t id=0,at=0,end=0,next=0;Shape shape=Shape::Circle;Mechanic kind=Mechanic::Flame;Vec center,direction{1,0};float radius=5,inner=0,length=15;int damagePercent=10;bool repeat=false;};
inline bool Contains(const Hazard& h,Vec p){float x=p.x-h.center.x,y=p.y-h.center.y,d=std::hypot(x,y);switch(h.shape){case Shape::Circle:return d<=h.radius;case Shape::Ring:return d<=h.radius&&d>=h.inner;case Shape::Cone:{if(d>h.radius)return false;float n=std::hypot(h.direction.x,h.direction.y);return d<.01f||(x*h.direction.x+y*h.direction.y)/(d*std::max(.01f,n))>=.7071f;}case Shape::Line:{float n=std::hypot(h.direction.x,h.direction.y),along=(x*h.direction.x+y*h.direction.y)/std::max(.01f,n),side=std::abs(x*h.direction.y-y*h.direction.x)/std::max(.01f,n);return along>=0&&along<=h.length&&side<=h.radius;}}return false;}
struct Timer {uint32_t at=0,period=0;Mechanic kind=Mechanic::Strike;};
// A bounded, instance-owned scheduler. Missed wall time does not replay bursts.
struct Scheduler {
 std::vector<Timer> timers;
 void Add(uint32_t now,Mechanic kind,uint32_t delay,uint32_t period=0){if(timers.size()<32)timers.push_back({now+delay,period,kind});}
 void Reset(){timers.clear();}
 template<class F>void Poll(uint32_t now,F&& callback){std::vector<Mechanic> ready;for(size_t i=0;i<timers.size();){auto& t=timers[i];if(!Due(now,t.at)){++i;continue;}ready.push_back(t.kind);if(t.period){t.at=now+t.period;++i;}else timers.erase(timers.begin()+i);}for(auto k:ready)callback(k);}
};
inline void Schedule(Scheduler& s,int boss,uint32_t now){s.Reset();s.Add(now,Mechanic::Strike,2500,2500);switch(boss){
 case 1:s.Add(now,Mechanic::Flame,6000,10000);s.Add(now,Mechanic::Spike,13000,21000);s.Add(now,Mechanic::Storm,45000,55000);break;
 case 2:s.Add(now,Mechanic::Cultist,5000,22000);s.Add(now,Mechanic::Control,30000,40000);s.Add(now,Mechanic::Flame,9000,13000);break;
 case 3:s.Add(now,Mechanic::Siege,7000,13000);s.Add(now,Mechanic::Cultist,10000,23000);break;
 case 4:s.Add(now,Mechanic::Mark,15000,24000);s.Add(now,Mechanic::Cultist,9000,25000);break;
 case 5:s.Add(now,Mechanic::Gas,10000,25000);s.Add(now,Mechanic::Spore,14000,25000);break;
 case 6:s.Add(now,Mechanic::Infection,7000,18000);s.Add(now,Mechanic::Slime,16000,24000);break;
 case 7:s.Add(now,Mechanic::Experiment,7000,22000);s.Add(now,Mechanic::Flame,11000,14000);s.Add(now,Mechanic::Mutation,18000,35000);break;
 case 8:s.Add(now,Mechanic::Invocation,12000,25000);s.Add(now,Mechanic::Kinetic,6000,18000);break;
 case 9:s.Add(now,Mechanic::Bite,15000,45000);s.Add(now,Mechanic::Pact,24000,33000);s.Add(now,Mechanic::Flame,8000,18000);break;
 case 10:s.Add(now,Mechanic::Portal,12000,28000);s.Add(now,Mechanic::Cultist,4000,18000);break;
 case 11:s.Add(now,Mechanic::IceTomb,18000,35000);s.Add(now,Mechanic::FrostBomb,26000,35000);s.Add(now,Mechanic::Flame,8000,16000);break;
 case 12:s.Add(now,Mechanic::Defile,9000,23000);s.Add(now,Mechanic::Valkyrie,22000,40000);s.Add(now,Mechanic::Soul,35000,45000);break;
 }}
inline uint64_t BossHealth(int members,int encounter){return uint64_t(1500000)*std::clamp(members,1,30)*(encounter==12?3:encounter==3?2:1);}
inline int RewardEligible(uint16_t mask){return mask==0x0fff;}
}
