/* Adapted from AzerothCore boss_professor_putricide.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct PutricidePort {
 virtual ~PutricidePort()=default;
 virtual Difficulty Mode()const=0;
 virtual void Ability(uint32_t)=0;
 virtual void Experiment(bool gas)=0;
 virtual void Transition(unsigned phase,bool heroic)=0;
 virtual void Resume(unsigned phase)=0;
};
class Putricide {
 EventMap events;unsigned phase=1;bool transition=false,nextGas=false;
 enum:uint32_t{Puddle=1,ExperimentEvent,Goo,Bombs,Unbound,Plague,ResumeEvent,Enrage};
 void Group(uint32_t id,uint32_t low,uint32_t high){events.Schedule(id,low,high,1);}
public:
 explicit Putricide(uint32_t seed=1):events(seed){}
 unsigned Phase()const{return phase;}bool Transitioning()const{return transition;}
 void Reset(PutricidePort& w){events.Reset();phase=1;transition=nextGas=false;Group(Puddle,10000,10000);Group(ExperimentEvent,30000,35000);events.Schedule(Enrage,600000);if(Heroic(w.Mode()))Group(Unbound,20000,20000);}
 void Health(PutricidePort& w,unsigned percent){if(transition||phase==3||percent>(phase==1?80u:35u))return;phase++;transition=true;uint32_t wait=Heroic(w.Mode())?49000:24000;events.DelayGroup(1,wait);events.Schedule(ResumeEvent,wait);if(phase==3){events.Cancel(ExperimentEvent);Group(Plague,wait+10000,wait+10000);}else{Group(Goo,wait+25000,wait+28000);Group(Bombs,wait+35000,wait+40000);}w.Transition(phase,Heroic(w.Mode()));if(Heroic(w.Mode())){w.Experiment(false);w.Experiment(true);}}
 void Tick(PutricidePort& w,uint32_t diff){events.Update(diff);switch(events.Execute()){
  case Puddle:w.Ability(70341);Group(Puddle,35000,35000);break;
  case ExperimentEvent:if(phase<3){w.Experiment(nextGas);nextGas=!nextGas;Group(ExperimentEvent,35000,40000);}break;
  case Goo:w.Ability(70852);Group(Goo,25000,30000);break;
  case Bombs:w.Ability(71255);Group(Bombs,35000,40000);break;
  case Unbound:w.Ability(70911);Group(Unbound,90000,90000);break;
  case Plague:w.Ability(72451);Group(Plague,10000,10000);break;
  case ResumeEvent:transition=false;w.Resume(phase);break;
  case Enrage:w.Ability(26662);break;
  default:break;
 }}
};
}
