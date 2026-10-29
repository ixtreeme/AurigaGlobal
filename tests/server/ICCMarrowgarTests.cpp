#include "../../SRC/Server/GameServer/raid/icc/Marrowgar.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct World:MarrowgarPort {
 Difficulty mode=Difficulty::Normal10;uint32_t now=0,stormEnd=0;bool casting=false,victim=true,queued=false;std::vector<std::pair<uint32_t,uint32_t>> casts;std::vector<uint32_t> durations;std::vector<Candidate> targets={{1,2},{2,7},{3,10},{4,20},{5,35},{6,40}};bool eligible=false;
 Difficulty Mode()const override{return mode;}bool HasVictim()const override{return victim;}bool Casting()const override{return casting;}bool HasStorm()const override{return stormEnd>now;}bool Charging()const override{return false;}bool MeleeReady()const override{return true;}bool MeleeSpellQueued()const override{return queued;}uint32_t Victim()const override{return 1;}bool VictimInsideArena()const override{return true;}std::vector<Candidate> Targets()const override{return targets;}
 void Cast(uint32_t spell,uint32_t,bool)override{casts.emplace_back(spell,now);if(spell==MarrowgarSpell::BoneStorm)stormEnd=now+40000;}
 void Talk(uint32_t)override{}void SetPassive(bool)override{}void StopMovement()override{}void InterruptMelee()override{}void ChaseVictim()override{}void Charge(uint32_t target,float speed)override{Require(target!=1&&speed==25,"storm charge target/speed");}void StormDuration(uint32_t d)override{durations.push_back(d);stormEnd=now+d;}void Melee()override{}void Evade()override{victim=false;}void BonedEligible(bool e)override{eligible=e;}
};
struct Spike:SpikePort {bool living=true,active=true;int released=0,failed=0;bool HasLivingPassenger()const override{return living;}bool EncounterActive()const override{return active;}void ReleasePassenger()override{released++;}void Despawn()override{}void BonedFailed()override{failed++;}};
struct Flame:ColdflamePort {int casts=0,steps=0;bool canStep=true;void CastSegment()override{casts++;}bool StepFiveMetres()override{steps++;return canStep;}};
int main(){
 for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){
  World w;w.mode=mode;Marrowgar boss(42);boss.Reset(w);Require(w.eligible,"achievement reset");boss.SliceHit(2);boss.SliceHit(3);boss.SliceHit(4);auto selected=boss.SelectSpikes(w);Require(selected.size()==(Raid25(mode)?2:1),"spike raid size and slice exclusions");for(auto id:selected)Require(id!=1&&id!=2&&id!=3&&id!=4,"spike must exclude all tanks");
  boss.Reset(w);for(uint32_t t=50;t<=80000;t+=50){w.now=t;boss.Tick(w,50);}Require(w.durations.size()==1&&w.durations[0]==(Raid25(mode)?30000u:20000u),"source storm durations");auto warn=std::find_if(w.casts.begin(),w.casts.end(),[](auto c){return c.first==MarrowgarSpell::BoneStorm;});Require(warn!=w.casts.end()&&warn->second>=45000&&warn->second<=50100,"initial storm timing");
  boss.Reset(w);w.now=w.stormEnd=0;w.casts.clear();w.casting=true;boss.Tick(w,6000);Require(w.casts.empty(),"casting guard");w.casting=false;w.now=6000;boss.Tick(w,0);Require(!w.casts.empty()&&w.casts[0].first==MarrowgarSpell::Coldflame,"event clock advances while casting");
 }
 Require(Marrowgar::StormDamage(1000,9)==1000,"near storm damage");Require(Marrowgar::StormDamage(1000,25)==250,"distance falloff");Require(Marrowgar::SliceDamage(900,10,3)==300,"slice target cap/split");Require(Marrowgar::SliceDamage(900,0,3)==900,"zero-target division safety");
 Spike world;BoneSpike spike;spike.Tick(world,7999);Require(world.failed==0,"Boned grace period");spike.Tick(world,1);Require(world.failed==1,"Boned failure at eight seconds");spike.Tick(world,8000);Require(world.failed==1,"achievement failure once");world.living=false;spike.Tick(world,1);Require(world.released==1,"orphan/dead passenger cleanup");spike.Release(world);Require(world.released==1,"idempotent spike release");
 Flame flame;ColdflameTrail trail;trail.Tick(flame,449);Require(flame.casts==0,"flame first delay");trail.Tick(flame,1);Require(flame.casts==1&&flame.steps==1,"flame five-metre advance");flame.canStep=false;trail.Tick(flame,450);trail.Tick(flame,450);Require(flame.casts==2&&!trail.Moving(),"LOS blocks further flame propagation");
 std::cout<<"ICC_MARROWGAR_DOMAIN_PASS four_modes timers selection damage spike_cleanup flame_los\n";
}
