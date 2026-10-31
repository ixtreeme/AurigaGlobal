/* Adapted from AzerothCore boss_deathbringer_saurfang.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct SaurfangPort {
 virtual ~SaurfangPort()=default;
 virtual Difficulty Mode()const=0;
 virtual void Ability(uint32_t)=0;
 virtual void SummonBeasts(unsigned)=0;
 virtual void ScentOfBlood()=0;
 virtual bool MarkChampion()=0;
};
class Saurfang {
 EventMap events;unsigned power=0;bool frenzy=false;enum:uint32_t{Beasts=1,Boil,Nova,Rune,Berserk,Scent};
public:
 explicit Saurfang(uint32_t seed=1):events(seed){}
 void Reset(SaurfangPort& w){events.Reset();power=0;frenzy=false;events.Schedule(Beasts,30000);events.Schedule(Boil,15500);events.Schedule(Nova,17000);events.Schedule(Rune,20000);events.Schedule(Berserk,Heroic(w.Mode())?360000:480000);}
 unsigned Power()const{return power;}
 void Gain(SaurfangPort& w,unsigned amount){power=std::min(100u,power+amount);if(power==100&&w.MarkChampion())power=0;}
 void Health(SaurfangPort& w,unsigned percent){if(!frenzy&&percent<=30){frenzy=true;w.Ability(72737);}}
 void Tick(SaurfangPort& w,uint32_t diff){events.Update(diff);switch(events.Execute()){
  case Beasts:w.SummonBeasts(Raid25(w.Mode())?5:2);events.Schedule(Beasts,40000);if(Heroic(w.Mode()))events.Schedule(Scent,10000);break;
  case Scent:w.ScentOfBlood();break;
  case Boil:w.Ability(72385);events.Schedule(Boil,15000,20000);break;
  case Nova:w.Ability(72380);events.Schedule(Nova,20000,25000);break;
  case Rune:w.Ability(72410);events.Schedule(Rune,20000,25000);break;
  case Berserk:w.Ability(26662);break;
  default:break;
 }}
};
}
