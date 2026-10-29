/* Adapted from AzerothCore boss_lord_marrowgar.cpp (ICC, 3.3.5a).
 * Copyright AzerothCore contributors; GPL-2.0-or-later. See docs/eclipse_raid.
 * World operations are explicit ports: no Blizzard engine class is linked here.
 */
#pragma once
#include "ICCEventMap.hpp"
namespace EclipseRaid::ICC {
namespace MarrowgarSpell {
constexpr uint32_t BoneSlice=69055,BoneStorm=69076,SpikeGraveyard=69057,Coldflame=69140,StormColdflame=72705,Impaled=69065,RideVehicle=46598,ColdflamePassive=69145,ColdflameSummon=69147,Berserk=26662;
constexpr std::array<uint32_t,3> SpikeSummons={69062,72669,72670};
}
struct MarrowgarPort {
 virtual ~MarrowgarPort()=default;
 virtual Difficulty Mode()const=0;
 virtual bool HasVictim()const=0;
 virtual bool Casting()const=0;
 virtual bool HasStorm()const=0;
 virtual bool Charging()const=0;
 virtual bool MeleeReady()const=0;
 virtual bool MeleeSpellQueued()const=0;
 virtual uint32_t Victim()const=0;
 virtual bool VictimInsideArena()const=0;
 virtual std::vector<Candidate> Targets()const=0;
 virtual void Cast(uint32_t spell,uint32_t target=0,bool triggered=false)=0;
 virtual void Talk(uint32_t group)=0;
 virtual void SetPassive(bool passive)=0;
 virtual void StopMovement()=0;
 virtual void InterruptMelee()=0;
 virtual void ChaseVictim()=0;
 virtual void Charge(uint32_t target,float speed)=0;
 virtual void StormDuration(uint32_t duration)=0;
 virtual void Melee()=0;
 virtual void Evade()=0;
 virtual void BonedEligible(bool eligible)=0;
};
class Marrowgar {
 enum EventID:uint32_t {EnableSlice=1,Spikes,Coldflame,StormFlame,WarnStorm,BeginStorm,MoveStorm,EndStorm,Enrage};
 EventMap events;bool boneSlice=false,intro=false;std::array<uint32_t,3> lastSlice{};
 uint32_t StormTarget(MarrowgarPort& world){auto targets=world.Targets();targets.erase(std::remove_if(targets.begin(),targets.end(),[&](const auto& target){return !target.alive||!target.insideBoundary||target.distance>175||target.id==world.Victim();}),targets.end());std::sort(targets.begin(),targets.end(),[](auto& a,auto& b){return a.distance>b.distance;});auto count=std::min<size_t>(targets.size(),Raid25(world.Mode())?2:1);if(count)return targets[events.Uniform(0,uint32_t(count-1))].id;if(world.VictimInsideArena())return world.Victim();world.Evade();return 0;}
public:
 explicit Marrowgar(uint32_t seed=1):events(seed){}
 void Reset(MarrowgarPort& world){events.Reset();boneSlice=false;lastSlice.fill(0);world.SetPassive(false);events.Schedule(EnableSlice,10000);events.Schedule(Spikes,10000,15000);events.Schedule(Coldflame,5000);events.Schedule(WarnStorm,45000,50000);events.Schedule(Enrage,600000);world.BonedEligible(true);}
 void Engaged(MarrowgarPort& world){world.Talk(1);}
 void EnterSight(MarrowgarPort& world,float distance,bool player,bool bossAlive){if(!intro&&bossAlive&&player&&distance<=100){intro=true;world.Talk(0);}}
 void KilledPlayer(MarrowgarPort& world){world.Talk(4);}
 void Died(MarrowgarPort& world){world.Talk(5);events.Reset();}
 void SliceHit(uint32_t target){for(auto& id:lastSlice)if(!id){id=target;break;}}
 const std::array<uint32_t,3>& SliceTargets()const{return lastSlice;}
 bool SliceEnabled()const{return boneSlice;}
 void ChargeArrived(){events.Schedule(StormFlame,0);}
 void Tick(MarrowgarPort& world,uint32_t diff){
  if(!world.HasVictim())return;events.Update(diff);if(world.Casting())return;
  switch(events.Execute()){
   case EnableSlice:boneSlice=true;break;
   case Spikes:if(Heroic(world.Mode())||!world.HasStorm())world.Cast(MarrowgarSpell::SpikeGraveyard,0,world.HasStorm());events.Schedule(Spikes,15000,20000);break;
   case Coldflame:if(!world.HasStorm())world.Cast(MarrowgarSpell::Coldflame);events.Schedule(Coldflame,5000);break;
   case StormFlame:world.Cast(MarrowgarSpell::StormColdflame);break;
   case WarnStorm:boneSlice=false;world.Talk(7);world.Talk(2);world.InterruptMelee();world.Cast(MarrowgarSpell::BoneStorm);world.SetPassive(true);world.StopMovement();events.Schedule(WarnStorm,90000,95000);events.Schedule(BeginStorm,3050);break;
   case BeginStorm:{auto duration=RaidMode<uint32_t>(world.Mode(),20000,30000,20000,30000);world.StormDuration(duration);events.Schedule(MoveStorm,0);events.Schedule(EndStorm,duration+1);break;}
   case MoveStorm:if(world.Charging())events.Schedule(MoveStorm,1);else{events.Schedule(MoveStorm,5000);auto target=StormTarget(world);if(target)world.Charge(target,25.f);}break;
   case EndStorm:world.StopMovement();world.SetPassive(false);world.ChaseVictim();events.Cancel(MoveStorm);events.Schedule(EnableSlice,10000);if(!Heroic(world.Mode()))events.Reschedule(Spikes,15000,20000);break;
   case Enrage:world.Cast(MarrowgarSpell::Berserk,0,true);world.Talk(6);break;
   default:break;
  }
  if(world.HasStorm())return;if(boneSlice&&!world.MeleeSpellQueued())world.Cast(MarrowgarSpell::BoneSlice,world.Victim());if(boneSlice&&world.MeleeReady()&&!world.Casting())lastSlice.fill(0);world.Melee();
 }
 std::vector<uint32_t> SelectSpikes(MarrowgarPort& world){auto targets=world.Targets();targets.erase(std::remove_if(targets.begin(),targets.end(),[&](auto& t){return !t.alive||t.impaled||t.distance>=150||t.id==world.Victim()||std::find(lastSlice.begin(),lastSlice.end(),t.id)!=lastSlice.end();}),targets.end());for(size_t i=targets.size();i>1;--i)std::swap(targets[i-1],targets[events.Uniform(0,uint32_t(i-1))]);std::vector<uint32_t> result;for(size_t i=0;i<std::min<size_t>(targets.size(),Raid25(world.Mode())?3:1);i++)result.push_back(targets[i].id);return result;}
 static int64_t StormDamage(int64_t damage,float distance){return int64_t(damage/std::max(std::sqrt(std::max(distance-9.f,0.f)),1.f));}
 static int64_t SliceDamage(int64_t damage,uint32_t selected,uint32_t cap){auto count=std::min(selected,cap);return count?damage/count:damage;}
};
struct ColdflamePort {virtual ~ColdflamePort()=default;virtual void CastSegment()=0;virtual bool StepFiveMetres()=0;};
class ColdflameTrail {
 uint32_t time=0,next=450;bool moving=true;
public:
 void Tick(ColdflamePort& world,uint32_t diff){time+=diff;if(time>=12000){moving=false;return;}if(!moving||time<next)return;world.CastSegment();if(!world.StepFiveMetres()){moving=false;return;}next=time+450;}
 bool Moving()const{return moving;}
};
struct SpikePort {virtual ~SpikePort()=default;virtual bool HasLivingPassenger()const=0;virtual bool EncounterActive()const=0;virtual void ReleasePassenger()=0;virtual void Despawn()=0;virtual void BonedFailed()=0;};
class BoneSpike {
 uint32_t elapsed=0;bool active=true,achievementChecked=false;
public:
 void Release(SpikePort& world){if(!active)return;active=false;world.ReleasePassenger();world.Despawn();}
 void Tick(SpikePort& world,uint32_t diff){if(!active)return;if(!world.HasLivingPassenger()||!world.EncounterActive()){Release(world);return;}elapsed+=diff;if(!achievementChecked&&elapsed>=8000){achievementChecked=true;world.BonedFailed();}}
};
}
