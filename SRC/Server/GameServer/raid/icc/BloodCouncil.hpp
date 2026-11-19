/* Adapted from AzerothCore boss_blood_prince_council.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct BloodCouncilPort {
 virtual ~BloodCouncilPort()=default;
 virtual Difficulty Mode()const=0;
 virtual void Empower(unsigned)=0;
 virtual void Ability(unsigned prince,uint32_t spell)=0;
};
class BloodCouncil {
 EventMap events;unsigned active=0;enum:uint32_t{Invocation=1,Lance,Nucleus,Flame,Sparks,Bomb,Vortex,Enrage};
public:
 explicit BloodCouncil(uint32_t seed=1):events(seed){}
 unsigned Active()const{return active;}
 void Reset(BloodCouncilPort& w){events.Reset();active=0;w.Empower(active);events.Schedule(Invocation,45000);events.Schedule(Lance,3000);events.Schedule(Nucleus,10000,15000);events.Schedule(Flame,20000);events.Schedule(Sparks,12000,15000);events.Schedule(Bomb,18000,24000);events.Schedule(Vortex,15000,20000);events.Schedule(Enrage,600000);}
 void Tick(BloodCouncilPort& w,uint32_t diff){events.Update(diff);switch(events.Execute()){
  case Invocation:active=(active+events.Uniform(1,2))%3;w.Empower(active);events.Schedule(Invocation,46000);break;
  case Lance:w.Ability(1,active==1?71815:71405);events.Schedule(Lance,3000);break;
  case Nucleus:w.Ability(1,71943);events.Schedule(Nucleus,10000,15000);break;
  case Flame:w.Ability(2,active==2?72040:71718);if(active==2)events.Schedule(Flame,15000);else events.Schedule(Flame,20000,25000);break;
  case Sparks:w.Ability(2,71806);events.Schedule(Sparks,15000,25000);break;
  case Bomb:w.Ability(0,72053);events.Schedule(Bomb,Raid25(w.Mode())?20500:30500);break;
  case Vortex:w.Ability(0,active==0?72039:72037);if(active==0)events.Schedule(Vortex,30000);else events.Schedule(Vortex,18000,23000);break;
  case Enrage:for(unsigned i=0;i<3;i++)w.Ability(i,26662);break;
  default:break;
 }}
};
}
