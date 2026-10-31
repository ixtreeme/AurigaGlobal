/* Siege adaptation of AzerothCore boss_icecrown_gunship_battle.cpp.
 * Copyright AzerothCore contributors; GPL-2.0-or-later.
 * Auriga uses the existing skybridge instead of moving WoW transports.
 * Mage freezes, boarding waves, escalating veterans and cannon heat retain
 * their encounter roles. Hull damage is authoritative and never client supplied. */
#pragma once
#include "ICCEventMap.hpp"
#include <map>
namespace EclipseRaid::ICC {
struct GunshipPort {
 virtual ~GunshipPort()=default;
 virtual Difficulty Mode()const=0;
 virtual void BoardingWave(unsigned count)=0;
 virtual void SummonMage()=0;
 virtual void RocketBarrage()=0;
 virtual void Frozen(bool)=0;
 virtual void CannonHit(uint32_t operatorID,int64_t damage,bool charged)=0;
 virtual void Victory()=0;
 virtual void Defeat()=0;
};
class Gunship {
 EventMap events;uint64_t friendly=0,enemy=0,maximum=0;bool frozen=false,finished=false;
 struct Cannon {unsigned heat=0;uint32_t next=0,overheat=0;};
 std::map<uint32_t,Cannon> cannons;
 enum:uint32_t {Board=1,Mage,Rocket,Timeout,Cool};
public:
 explicit Gunship(uint32_t seed=1):events(seed){}
 void Reset(GunshipPort& w,uint64_t hull){events.Reset();cannons.clear();friendly=enemy=maximum=std::max<uint64_t>(1,hull);frozen=finished=false;events.Schedule(Board,12000);events.Schedule(Mage,30000);events.Schedule(Rocket,8000);events.Schedule(Timeout,600000);events.Schedule(Cool,1000);w.Frozen(false);}
 bool Finished()const{return finished;}bool IsFrozen()const{return frozen;}
 uint64_t Friendly()const{return friendly;}uint64_t Enemy()const{return enemy;}uint64_t Maximum()const{return maximum;}
 unsigned Heat(uint32_t cannon)const{auto it=cannons.find(cannon);return it==cannons.end()?0:it->second.heat;}
 bool Fire(GunshipPort& w,uint32_t cannon,uint32_t player,bool charged){
  if(finished||frozen||!cannon||!player)return false;auto& c=cannons[cannon];
  if(int32_t(events.Time()-c.next)<0||int32_t(events.Time()-c.overheat)<0||(charged&&c.heat<10))return false;
  // Incinerating blast consumes energy and uses the original quadratic bonus.
  int64_t damage=charged?10000+int64_t(c.heat-10)*(c.heat-10)*8:10000;
  c.next=events.Time()+2000;
  if(charged)c.heat=0;else{c.heat=std::min(100u,c.heat+20);if(c.heat>=100)c.overheat=events.Time()+5000;}
  w.CannonHit(player,damage,charged);enemy-=std::min<uint64_t>(enemy,uint64_t(damage));
  if(!enemy){finished=true;w.Victory();}return true;
 }
 void DamageHull(GunshipPort& w,uint64_t damage){if(finished)return;friendly-=std::min(friendly,damage);if(!friendly){finished=true;w.Defeat();}}
 void Repair(uint64_t amount){if(!finished)friendly+=std::min(amount,maximum-friendly);}
 void MageKilled(GunshipPort& w){if(finished||!frozen)return;frozen=false;w.Frozen(false);events.Schedule(Mage,30000,33500);}
 void Tick(GunshipPort& w,uint32_t diff){if(finished)return;events.Update(diff);switch(events.Execute()){
  case Board:w.BoardingWave(Raid25(w.Mode())?6:3);events.Schedule(Board,60000);break;
  case Mage:if(!frozen){frozen=true;w.SummonMage();w.Frozen(true);}break;
  case Rocket:w.RocketBarrage();events.Schedule(Rocket,Heroic(w.Mode())?5000:8000);break;
  case Cool:for(auto& row:cannons)row.second.heat=row.second.heat>5?row.second.heat-5:0;events.Schedule(Cool,1000);break;
  case Timeout:finished=true;w.Defeat();break;
  default:break;
 }}
};
}
