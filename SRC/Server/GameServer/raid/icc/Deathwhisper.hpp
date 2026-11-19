/* Adapted encounter timing/phases from AzerothCore boss_lady_deathwhisper.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later. Auriga world adapter
 * deliberately uses project-native actors and combat. See docs/eclipse_raid. */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
struct DeathwhisperPort {
 virtual ~DeathwhisperPort()=default;
 virtual Difficulty Mode()const=0;
 virtual bool Casting()const=0;
 virtual void Ability(uint32_t spell)=0;
 virtual void Wave(bool phaseTwo)=0;
 virtual void Empower()=0;
 virtual void PhaseTwo()=0;
};
class Deathwhisper {
 enum EventID:uint32_t {Decay=1,Control,Shadow,WaveOne,EmpowerAdd,Frost,Volley,Touch,Shade,WaveTwo,Enrage};
 EventMap events;bool second=false;
public:
 explicit Deathwhisper(uint32_t seed=1):events(seed){}
 void Reset(DeathwhisperPort& w){events.Reset();second=false;events.Schedule(Decay,10000);events.Schedule(Shadow,2000);events.Schedule(WaveOne,5000);events.Schedule(EmpowerAdd,20000,30000);events.Schedule(Enrage,600000);if(w.Mode()!=Difficulty::Normal10)events.Schedule(Control,30000);}
 bool SecondPhase()const{return second;}
 static unsigned ControlTargets(Difficulty d){return RaidMode<unsigned>(d,0,1,1,3);}
 static unsigned WaveSize(Difficulty d,bool phaseTwo){return phaseTwo?(Raid25(d)?3:1):(Raid25(d)?7:3);}
 static unsigned ShadeCount(Difficulty d){return RaidMode<unsigned>(d,1,2,1,3);}
 int64_t Absorb(DeathwhisperPort& w,uint64_t& mana,int64_t damage){if(damage<=0||second)return damage;auto blocked=std::min<uint64_t>(mana,uint64_t(damage));mana-=blocked;damage-=int64_t(blocked);if(!mana){second=true;events.Cancel(Shadow);events.Cancel(WaveOne);events.Cancel(EmpowerAdd);events.Schedule(Frost,10000,12000);events.Schedule(Volley,19000,21000);events.Schedule(Touch,6000,9000);events.Schedule(Shade,12000,15000);if(Heroic(w.Mode()))events.Schedule(WaveTwo,45000);w.PhaseTwo();}return damage;}
 void Tick(DeathwhisperPort& w,uint32_t diff){events.Update(diff);if(w.Casting())return;switch(events.Execute()){
  case Decay:w.Ability(71001);events.Schedule(Decay,22000,30000);break;
  case Control:w.Ability(71289);events.Schedule(Control,40000,45000);break;
  case Shadow:if(!second){w.Ability(71254);events.Schedule(Shadow,2100);}break;
  case WaveOne:if(!second){w.Wave(false);events.Schedule(WaveOne,Heroic(w.Mode())?45000:60000);}break;
  case EmpowerAdd:if(!second){w.Empower();events.Schedule(EmpowerAdd,18000,25000);}break;
  case Frost:w.Ability(71420);events.Schedule(Frost,12000);break;
  case Volley:w.Ability(72905);events.Schedule(Volley,13000,15000);break;
  case Touch:w.Ability(71204);events.Schedule(Touch,6000,9000);break;
  case Shade:w.Ability(71363);events.Schedule(Shade,12000);break;
  case WaveTwo:w.Wave(true);events.Schedule(WaveTwo,45000);break;
  case Enrage:w.Ability(26662);break;
  default:break;
 }}
};
}
