#include "../../SRC/Server/GameServer/raid/icc/Gunship.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseRaidRules.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct World:GunshipPort {
 Difficulty mode=Difficulty::Normal10;unsigned waves=0,mages=0,rockets=0,won=0,lost=0,shots=0;bool frozen=false;
 Difficulty Mode()const override{return mode;}void BoardingWave(unsigned n)override{waves+=n;}
 void SummonMage()override{mages++;}void RocketBarrage()override{rockets++;}void Frozen(bool v)override{frozen=v;}
 void CannonHit(uint32_t,int64_t,bool)override{shots++;}void Victory()override{won++;}void Defeat()override{lost++;}
};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){
 World w;w.mode=mode;Gunship c;c.Reset(w,1000000);
 Require(c.Fire(w,1,5,false),"first cannon shot");Require(!c.Fire(w,1,6,false),"shared cannon cooldown prevents two-player spam");
 Require(!c.Fire(w,2,6,true),"charged blast requires heat");
 for(int i=0;i<121;i++)c.Tick(w,250);
 Require(w.waves==(Raid25(mode)?6u:3u)&&w.mages==1&&w.frozen,"boarding and frost mage source timing");
 Require(!c.Fire(w,1,5,false),"frozen cannons cannot damage hull");c.MageKilled(w);Require(!w.frozen,"mage kill thaws cannons");
 Require(c.Fire(w,1,5,false),"cannon resumes after thaw");
 c.DamageHull(w,2000000);Require(w.lost==1&&c.Friendly()==0,"hull destruction causes defeat without underflow");
 c.DamageHull(w,1);c.Tick(w,600000);Require(w.lost==1,"defeat occurs once");
 c.Reset(w,1);Require(c.Fire(w,1,5,false)&&w.won==1,"enemy hull victory");Require(!c.Fire(w,1,5,false),"no post-victory damage");
 c.Reset(w,10000000);for(int i=0;i<2405;i++)c.Tick(w,250);Require(w.lost==2,"ten minute timeout");
 }
 Require(!EclipseRaid::Unlocked(1,3)&&EclipseRaid::Unlocked(3,3),"second-room kill unlocks siege");
 std::cout<<"ICC_GUNSHIP_PASS four_modes shared_heat frozen_mage waves hulls victory timeout progression\n";
}
