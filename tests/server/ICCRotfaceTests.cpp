#include "../../SRC/Server/GameServer/raid/icc/Rotface.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool good,const char* why){if(!good)throw std::runtime_error(why);}
struct World:RotfacePort {
 Difficulty mode;uint32_t time=0,goo=0,sprays=0,infections=0;std::vector<uint32_t> floodTimes,sectors;bool casting=false;
 explicit World(Difficulty m):mode(m){}
 Difficulty Mode()const override{return mode;}bool HasVictim()const override{return true;}bool Casting()const override{return casting;}uint32_t Victim()const override{return 1;}
 std::vector<Candidate> Targets()const override{return {{1,2},{2,3},{3,15},{4,20},{5,25},{6,30},{7,35},{8,40},{9,45},{10,50}};}
 void Cast(uint32_t id,uint32_t,bool,int maximum)override{if(id==RotfaceSpell::Infection){Require(maximum==1,"one infection target");infections++;}}
 bool SprayAt(uint32_t target)override{Require(target!=1,"spray excludes tank");sprays++;return true;}void Unroot()override{}bool ProfessorAvailable()const override{return true;}
 void Flood(uint32_t sector,uint32_t spell)override{Require(spell==RotfaceSpell::Flood[sector],"correct paired flood sector spell");floodTimes.push_back(time);sectors.push_back(sector);}
 void ProfessorCast(uint32_t id,uint32_t target)override{Require(id==RotfaceSpell::VileGas&&target!=0,"heroic professor gas");goo++;}
 void Talk(int)override{}void ProfessorAction(int)override{}void Melee()override{}
};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World world(mode);Rotface boss(17);boss.Reset();boss.Engaged(world);
 for(uint32_t time=50;time<=451000;time+=50){world.time=time;boss.Tick(world,50);}
 Require(boss.InfectionInterval()==(Raid25(mode)?6000u:14000u),"infection accelerates only in 25-player modes, down to six seconds");
 Require((world.goo>0)==Heroic(mode),"only heroic has professor gas");Require(world.sprays>20&&world.infections>20,"repeating source events");
 Require(!world.floodTimes.empty()&&world.floodTimes.front()>=8000&&world.floodTimes.front()<8200,"first flood at eight seconds");for(size_t i=1;i<world.sectors.size();i++)Require(world.sectors[i]==(world.sectors[i-1]+1)%4,"flood visits each sector in source order");
 }
 Require(MergeOozeStacks(false,0,false,0)==1,"two small oozes create one unstable stack");Require(MergeOozeStacks(true,3,false,0)==4,"small ooze increments large stack");Require(MergeOozeStacks(true,4,true,4)==5,"large merge caps at explosion threshold");
 std::cout<<"ICC_ROTFACE_DOMAIN_PASS four_modes infection_acceleration tank_exclusion flood_cycle heroic_gas merge_stacks\n";
}
