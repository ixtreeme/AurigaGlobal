/* Adapted from AzerothCore boss_the_lich_king.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct LichKingPort {virtual ~LichKingPort()=default;virtual Difficulty Mode()const=0;virtual void Ability(uint32_t)=0;virtual void Phase(unsigned)=0;virtual void Victory()=0;};
class LichKing {
 EventMap events;unsigned phase=1;
 enum:uint32_t {Horror=1,Ghouls,Infest,Plague,Trap,Pain,Sphere,Raging,Quake,Defile,Reaper,Valkyr,Spirits,Harvest,Return,Enrage,Finish};
 void At(uint32_t id,uint32_t delay){events.Schedule(id,delay,delay,1);}
 void Transition(LichKingPort& w,unsigned next){events.CancelGroup(1);phase=next;w.Phase(phase);w.Ability(phase==2?68981:72259);At(Pain,3500);At(Sphere,8000);At(Raging,4000);At(Quake,62500);}
public:
 explicit LichKing(uint32_t seed=1):events(seed){}
 unsigned Phase()const{return phase;}
 bool Transitioning()const{return phase==2||phase==4;}bool Ending()const{return phase==7;}
 void Reset(LichKingPort& w){events.Reset();phase=1;w.Phase(phase);At(Horror,15000);At(Ghouls,10000);At(Infest,5000);At(Plague,30000);if(Heroic(w.Mode()))At(Trap,15500);events.Schedule(Enrage,900000);}
 void Health(LichKingPort& w,unsigned percent){if(phase==1&&percent<=70)Transition(w,2);else if(phase==3&&percent<=40)Transition(w,4);else if(phase==5&&percent<=10){events.CancelGroup(1);events.Cancel(Enrage);phase=7;w.Phase(phase);w.Ability(72350);events.Schedule(Finish,35000);}}
 void Tick(LichKingPort& w,uint32_t diff){events.Update(diff);switch(events.Execute()){
 case Horror:w.Ability(70372);At(Horror,60000);break;
 case Ghouls:w.Ability(70358);At(Ghouls,30000);break;
 case Infest:w.Ability(70541);At(Infest,22500);break;
 case Plague:w.Ability(70337);At(Plague,events.Uniform(30000,31000));break;
 case Trap:w.Ability(73539);At(Trap,15500);break;
 case Pain:w.Ability(72133);At(Pain,3500);break;
 case Sphere:w.Ability(69104);At(Sphere,7500);break;
 case Raging:w.Ability(69200);At(Raging,phase==4?15000:20000);break;
 case Quake:phase=phase==2?3:5;events.CancelGroup(1);w.Phase(phase);w.Ability(72262);At(Reaper,40000);At(Defile,38000);if(phase==3){At(Infest,14000);At(Valkyr,20000);}else{At(Spirits,20000);At(Harvest,14000);}break;
 case Defile:w.Ability(72762);At(Defile,32500);break;
 case Reaper:w.Ability(69409);At(Reaper,30500);break;
 case Valkyr:w.Ability(69037);At(Valkyr,45000);break;
 case Spirits:w.Ability(70498);At(Spirits,30000);break;
 case Harvest:w.Ability(Heroic(w.Mode())?73654:68980);if(Heroic(w.Mode())){events.CancelGroup(1);phase=6;w.Phase(phase);At(Return,55000);}else At(Harvest,75000);break;
 case Return:phase=5;w.Phase(phase);At(Defile,1000);At(Reaper,10000);At(Spirits,20000);At(Harvest,events.Uniform(45000,55000));break;
 case Enrage:w.Ability(26662);break;
 case Finish:w.Victory();break;
 default:break;
 }}
};
}
