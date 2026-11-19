/* Adapted from AzerothCore boss_sindragosa.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct SindragosaPort {
 virtual ~SindragosaPort()=default;
 virtual Difficulty Mode()const=0;
 virtual void Ability(uint32_t)=0;
 virtual void Flight(bool)=0;
 virtual void FinalPhase()=0;
};
class Sindragosa {
 EventMap events;bool flying=false,final=false,requested=false;unsigned bombs=0;
 enum:uint32_t{Cleave=1,Tail,Breath,Unchained,Grip,Cold,Air,Bomb,Land,Tomb,Buffet,Enrage};
 void Group(uint32_t id,uint32_t low,uint32_t high){events.Schedule(id,low,high,1);}
 void Ground(bool resume){Group(Cleave,resume?13000:10000,resume?15000:10000);Group(Tail,resume?19000:20000,resume?23000:20000);Group(Breath,resume?7000:8000,resume?10000:12000);Group(Unchained,resume?12000:9000,resume?17000:14000);Group(Grip,resume?35000:33500,resume?40000:33500);}
 void ActivateFinal(SindragosaPort& w){if(final||flying)return;final=true;requested=false;events.Cancel(Air);events.Schedule(Tomb,7000,10000);events.Schedule(Buffet,6000);w.FinalPhase();}
public:
 explicit Sindragosa(uint32_t seed=1):events(seed){}
 bool Flying()const{return flying;}bool Final()const{return final;}bool FinalRequested()const{return requested;}
 void Reset(SindragosaPort&){events.Reset();flying=final=requested=false;bombs=0;Ground(false);events.Schedule(Air,50000);events.Schedule(Enrage,600000);}
 void Health(SindragosaPort& w,unsigned percent){if(percent>35||final)return;requested=true;events.Cancel(Air);ActivateFinal(w);}
 void Tick(SindragosaPort& w,uint32_t diff){events.Update(diff);switch(events.Execute()){
  case Cleave:w.Ability(19983);Group(Cleave,10000,15000);break;
  case Tail:w.Ability(71077);Group(Tail,22000,27000);break;
  case Breath:w.Ability(final?73061:69649);Group(Breath,20000,25000);break;
  case Unchained:w.Ability(69762);Group(Unchained,30000,35000);break;
  case Grip:w.Ability(70117);Group(Cold,1000,1000);if(!final)Group(Grip,65000,70000);break;
  case Cold:w.Ability(70123);if(final)Group(Grip,65000,70000);break;
  case Air:if(!final&&!requested){flying=true;bombs=0;events.CancelGroup(1);w.Flight(true);w.Ability(70126);events.Schedule(Bomb,7000);events.Schedule(Air,110000);}break;
  case Bomb:w.Ability(69845);if(++bombs==4)events.Schedule(Land,5500);else events.Schedule(Bomb,6000);break;
  case Land:flying=false;w.Flight(false);Ground(true);if(requested)ActivateFinal(w);break;
  case Tomb:w.Ability(70126);events.Schedule(Tomb,18000,22000);break;
  case Buffet:if(final){w.Ability(70128);events.Schedule(Buffet,6000);}break;
  case Enrage:w.Ability(26662);break;
  default:break;
 }}
 static unsigned TombCount(Difficulty d,bool air){return air?RaidMode<unsigned>(d,2,5,2,6):1;}
};
}
