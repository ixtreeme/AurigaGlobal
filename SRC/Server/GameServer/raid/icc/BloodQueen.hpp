/* Adapted from AzerothCore boss_blood_queen_lana_thel.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct BloodQueenPort {
 virtual ~BloodQueenPort()=default;
 virtual Difficulty Mode()const=0;
 virtual void Ability(uint32_t)=0;
 virtual void Flight(unsigned stage)=0;
};
class BloodQueen {
 EventMap events;bool flying=false;enum:uint32_t{Bite=1,Mirror,Slash,Pact,Shadows,Bolt,Air,Raise,VolleyOne,VolleyTwo,Land,Enrage};
 void Group(uint32_t id,uint32_t low,uint32_t high){events.Schedule(id,low,high,1);}
public:
 explicit BloodQueen(uint32_t seed=1):events(seed){}
 bool Flying()const{return flying;}
 void Reset(BloodQueenPort& w){events.Reset();flying=false;Group(Bite,15000,15000);Group(Mirror,2500,2500);Group(Slash,10000,12000);Group(Pact,20000,20000);Group(Shadows,30000,30000);Group(Bolt,15000,25000);events.Schedule(Air,Raid25(w.Mode())?127000:124000);events.Schedule(Enrage,330000);}
 void Tick(BloodQueenPort& w,uint32_t diff){events.Update(diff);switch(events.Execute()){
  case Bite:w.Ability(71726);break;
  case Mirror:w.Ability(70838);Group(Mirror,2500,2500);break;
  case Slash:w.Ability(71623);Group(Slash,20000,24000);break;
  case Pact:w.Ability(71340);Group(Pact,30000,30000);break;
  case Shadows:w.Ability(71264);Group(Shadows,30000,30000);break;
  case Bolt:w.Ability(71818);Group(Bolt,10000,15000);break;
  case Air:flying=true;events.DelayGroup(1,12000);w.Flight(0);events.Schedule(Raise,2500);events.Schedule(VolleyOne,3000);events.Schedule(VolleyTwo,5500);events.Schedule(Land,9500);events.Schedule(Air,Raid25(w.Mode())?100000:120000);break;
  case Raise:w.Flight(1);break;
  case VolleyOne:case VolleyTwo:w.Ability(71446);break;
  case Land:flying=false;w.Flight(2);events.Reschedule(Pact,5000,5000);events.Reschedule(Shadows,20000,20000);break;
  case Enrage:w.Ability(26662);break;
  default:break;
 }}
};
}
