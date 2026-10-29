/* Adapted from AzerothCore boss_festergut.cpp, GPL-2.0-or-later.
 * Source snapshot and attribution: docs/eclipse_raid/upstream_inventory.json.
 */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
namespace FestergutSpell {
constexpr uint32_t Inhale=69165,Pungent=69195,GastricBloat=72219,GastricExplosion=72227,Spore=69278,VileGas=69240,Inoculated=69291,HeroicGoo=72296,Berserk=47008;
constexpr std::array<uint32_t,3> Blight={69157,69162,69164},Visual={69126,69152,69154};
}
struct FestergutPort {
 virtual ~FestergutPort()=default;
 virtual Difficulty Mode()const=0;virtual bool HasVictim()const=0;virtual bool Casting()const=0;virtual uint32_t Victim()const=0;virtual std::vector<Candidate> Targets()const=0;
 virtual void Cast(uint32_t spell,uint32_t target=0,bool triggered=false,int maximumTargets=0)=0;
 virtual void RemoveBlight()=0;virtual void ProfessorCast(uint32_t spell,uint32_t target)=0;virtual void ProfessorAction(int action)=0;virtual void Talk(int text)=0;virtual void Melee()=0;
};
class Festergut {
 enum EventID:uint32_t {Berserk=1,Inhale,VileGas,Spore,GastricBloat,HeroicGoo};
 EventMap events;uint32_t inhaled=0,maxInoculated=0;
 uint32_t Target(FestergutPort& world,bool nonTank,bool ranged){auto all=world.Targets();all.erase(std::remove_if(all.begin(),all.end(),[&](auto& t){return !t.alive||(nonTank&&t.id==world.Victim());}),all.end());auto pool=all;if(ranged){pool.erase(std::remove_if(pool.begin(),pool.end(),[](auto& t){return (t.combatDistance>=0?t.combatDistance:t.distance)<5;}),pool.end());if(pool.size()<size_t(Raid25(world.Mode())?8:3))pool=all;}return pool.empty()?0:pool[events.Uniform(0,uint32_t(pool.size()-1))].id;}
public:
 explicit Festergut(uint32_t seed=1):events(seed){}
 void Reset(FestergutPort& world){events.Reset();inhaled=maxInoculated=0;world.RemoveBlight();}
 void Engaged(FestergutPort& world){events.Schedule(Berserk,300000);events.Schedule(Inhale,25000,30000);events.Schedule(Spore,20000,25000);events.Schedule(VileGas,30000,40000,1);events.Schedule(GastricBloat,12500,15000);if(Heroic(world.Mode()))events.Schedule(HeroicGoo,15000,20000);world.Talk(1);world.ProfessorAction(0);}
 void Died(FestergutPort& world){events.Reset();world.Talk(9);world.ProfessorAction(1);world.RemoveBlight();}
 void Evaded(FestergutPort& world){events.Reset();world.ProfessorAction(2);}
 void KilledPlayer(FestergutPort& world){world.Talk(7);}
 void Inoculated(uint32_t stacks){maxInoculated=std::max(maxInoculated,stacks);}
 bool FluShotShortage()const{return maxInoculated<3;}
 uint32_t Inhaled()const{return inhaled;}
 void Tick(FestergutPort& world,uint32_t diff){if(!world.HasVictim())return;events.Update(diff);if(world.Casting())return;switch(events.Execute()){
  case Berserk:world.Cast(FestergutSpell::Berserk,0,true);world.Talk(8);break;
  case Inhale:world.RemoveBlight();if(inhaled==3){world.Talk(5);world.Talk(4);world.Cast(FestergutSpell::Pungent);inhaled=0;events.Reschedule(Spore,20000,25000);}else{world.Cast(FestergutSpell::Inhale);inhaled++;if(inhaled<3)world.Cast(FestergutSpell::Blight[inhaled],0,true);}events.Schedule(Inhale,34000);break;
  case Spore:world.Talk(3);world.Talk(2);world.Cast(FestergutSpell::Spore,0,false,Raid25(world.Mode())?3:2);events.Schedule(Spore,40000,45000);events.DelayGroupToMax(1,20000);break;
  case VileGas:{auto target=Target(world,false,true);if(target)world.Cast(FestergutSpell::VileGas,target);events.Schedule(VileGas,28000,35000,1);break;}
  case GastricBloat:world.Cast(FestergutSpell::GastricBloat,world.Victim());events.Schedule(GastricBloat,15000,17500);break;
  case HeroicGoo:{auto target=Target(world,true,false);if(target)world.ProfessorCast(FestergutSpell::HeroicGoo,target);events.Schedule(HeroicGoo,15000,20000);break;}
  default:break;
 }world.Melee();}
 // Aura-removal guard from spell_festergut_blighted_spores_aura. Replaced DoTs
 // and overlapping fresh inoculations must not give multiple protection stacks.
 static bool GrantsInoculation(int auraDuration,int auraMaximum,bool hasInoculated,int inoculatedDuration,int inoculatedMaximum){if(auraDuration>auraMaximum-1000)return false;if(hasInoculated&&inoculatedDuration>inoculatedMaximum-10000)return false;return true;}
 static bool GastricExplodes(uint32_t stacks){return stacks==10;}
};
}
