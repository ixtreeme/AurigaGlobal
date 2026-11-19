#include "../../SRC/Server/GameServer/raid/icc/Deathwhisper.hpp"
#include "../../SRC/Server/GameServer/raid/EclipseRaidRules.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct World:DeathwhisperPort{Difficulty mode=Difficulty::Normal10;bool casting=false;int phase=0,waves=0,waves2=0,empowers=0;std::vector<uint32_t> spells;Difficulty Mode()const override{return mode;}bool Casting()const override{return casting;}void Ability(uint32_t s)override{spells.push_back(s);}void Wave(bool p)override{if(p)waves2++;else waves++;}void Empower()override{empowers++;}void PhaseTwo()override{phase++;}};
int main(){
 for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World w;w.mode=mode;Deathwhisper c(42);c.Reset(w);uint64_t mana=1000;Require(c.Absorb(w,mana,300)==0&&mana==700&&w.phase==0,"barrier absorbs without HP loss");Require(c.Absorb(w,mana,750)==50&&mana==0&&w.phase==1,"phase switch conserves overflow damage");Require(c.Absorb(w,mana,90)==90&&w.phase==1,"phase transition occurs once");for(int i=0;i<2408;i++)c.Tick(w,250);Require(w.waves==0,"phase-one waves canceled after mana depletion");Require(w.waves2==(Heroic(mode)?13:0),"heroic phase-two wave timing");Require(std::find(w.spells.begin(),w.spells.end(),71420)!=w.spells.end(),"phase-two frostbolt");Require(std::find(w.spells.begin(),w.spells.end(),71363)!=w.spells.end(),"phase-two spirits");Require(std::find(w.spells.begin(),w.spells.end(),26662)!=w.spells.end(),"ten-minute berserk");Require(Deathwhisper::WaveSize(mode,false)==(Raid25(mode)?7:3),"normal wave counts");Require(Deathwhisper::ControlTargets(mode)==RaidMode<unsigned>(mode,0,1,1,3),"mind control difficulty");
  c.Reset(w);w.spells.clear();w.waves=w.waves2=0;for(int i=0;i<24;i++)c.Tick(w,250);Require(w.waves==1,"first wave at five seconds");Require(std::find(w.spells.begin(),w.spells.end(),71254)!=w.spells.end(),"phase-one shadow bolt");Require(std::find(w.spells.begin(),w.spells.end(),71420)==w.spells.end(),"no frostbolt in phase one");
 }
 Require(!EclipseRaid::Unlocked(0,2),"second room locked before first kill");Require(EclipseRaid::Unlocked(EclipseRaid::Clear(0,1),2),"first kill unlocks second room");
 World w;Deathwhisper c;c.Reset(w);w.casting=true;c.Tick(w,5000);Require(w.spells.empty()&&w.waves==0,"casting prevents event execution");w.casting=false;c.Tick(w,0);c.Tick(w,0);Require(w.waves==1,"due events retained after casting");std::cout<<"ICC_DEATHWHISPER_PASS barrier overflow transition four_modes waves frost shades berserk progression casting\n";
}
