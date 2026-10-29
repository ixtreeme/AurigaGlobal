/* Adapted from AzerothCore boss_rotface.cpp, GPL-2.0-or-later.
 * Timings, infection acceleration, flood cycling and merge stacks retain the
 * original source branches. World adapters own movement/damage/visuals.
 */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
namespace RotfaceSpell {constexpr uint32_t Spray=69508,Infection=69674,VileGas=69240,Sticky=69774,Explosion=69839;constexpr std::array<uint32_t,4> Flood={69782,69796,69798,69801};}
struct RotfacePort {
 virtual ~RotfacePort()=default;
 virtual Difficulty Mode()const=0;virtual bool HasVictim()const=0;virtual bool Casting()const=0;virtual uint32_t Victim()const=0;virtual std::vector<Candidate> Targets()const=0;
 virtual void Cast(uint32_t spell,uint32_t target=0,bool triggered=false,int maximum=0)=0;
 virtual bool SprayAt(uint32_t target)=0;virtual void Unroot()=0;virtual bool ProfessorAvailable()const=0;virtual void Flood(uint32_t sector,uint32_t spell)=0;virtual void ProfessorCast(uint32_t spell,uint32_t target)=0;
 virtual void Talk(int group)=0;virtual void ProfessorAction(int action)=0;virtual void Melee()=0;
};
class Rotface {
 enum EventID:uint32_t {Unroot=1,Spray,Hasten,Infection,Flood,VileGas};EventMap events;uint32_t interval=14000,sector=0;
 uint32_t Target(RotfacePort& w,bool nonTank,bool ranged){auto all=w.Targets();all.erase(std::remove_if(all.begin(),all.end(),[&](auto& t){return !t.alive||(nonTank&&t.id==w.Victim());}),all.end());auto pool=all;if(ranged){pool.erase(std::remove_if(pool.begin(),pool.end(),[](auto& t){return (t.combatDistance>=0?t.combatDistance:t.distance)<5;}),pool.end());if(pool.size()<size_t(Raid25(w.Mode())?8:3))pool=all;}return pool.empty()?0:pool[events.Uniform(0,uint32_t(pool.size()-1))].id;}
public:
 explicit Rotface(uint32_t seed=1):events(seed){}
 void Reset(){events.Reset();interval=14000;sector=0;}
 void Engaged(RotfacePort& world){events.Reset();events.Schedule(Spray,20000);if(Raid25(world.Mode()))events.Schedule(Hasten,90000);events.Schedule(Infection,14000);events.Schedule(Flood,8000);if(Heroic(world.Mode()))events.Schedule(VileGas,15000,20000);sector=events.Uniform(0,3);world.Talk(1);world.ProfessorAction(0);}
 uint32_t InfectionInterval()const{return interval;}
 void Died(RotfacePort& world){events.Reset();world.Talk(8);world.ProfessorAction(1);}
 void KilledPlayer(RotfacePort& world){world.Talk(6);}
 void Tick(RotfacePort& world,uint32_t diff){if(!world.HasVictim())return;events.Update(diff);if(world.Casting())return;switch(events.Execute()){
  case Unroot:world.Unroot();break;
  case Spray:{auto target=Target(world,true,false);if(target&&world.SprayAt(target)){world.Talk(2);world.Cast(RotfaceSpell::Spray);}events.DelayAll(1);events.Schedule(Spray,20000);events.Schedule(Unroot,0);break;}
  case Hasten:if(interval>=8000){interval-=2000;events.Schedule(Hasten,90000);}break;
  case Infection:world.Cast(RotfaceSpell::Infection,0,false,1);events.Schedule(Infection,interval);break;
  case Flood:if(world.ProfessorAvailable()){world.Flood(sector,RotfaceSpell::Flood[sector]);sector=(sector+1)%4;}events.Schedule(Flood,25000);break;
  case VileGas:{auto target=Target(world,false,true);if(target&&world.ProfessorAvailable())world.ProfessorCast(RotfaceSpell::VileGas,target);events.Schedule(VileGas,15000,20000);break;}
  default:break;
 }world.Melee();}
};
inline uint32_t MergeOozeStacks(bool firstBig,uint32_t first,bool secondBig,uint32_t second){if(!firstBig&&!secondBig)return 1;if(firstBig&&secondBig)return std::min(5u,first+second);return std::min(5u,(firstBig?first:second)+1);}
}
