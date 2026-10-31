#include "../../SRC/Server/GameServer/raid/icc/Saurfang.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct World:SaurfangPort {
 Difficulty mode=Difficulty::Normal10;unsigned beasts=0,scent=0,marks=0;bool canMark=true;std::vector<uint32_t> spells;
 Difficulty Mode()const override{return mode;}void Ability(uint32_t s)override{spells.push_back(s);}void SummonBeasts(unsigned n)override{beasts+=n;}void ScentOfBlood()override{scent++;}bool MarkChampion()override{if(canMark)marks++;return canMark;}
};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World w;w.mode=mode;Saurfang c(42);c.Reset(w);c.Gain(w,99);Require(c.Power()==99&&!w.marks,"power accumulates");c.Gain(w,3);Require(c.Power()==0&&w.marks==1,"100 power creates one mark and resets");w.canMark=false;c.Gain(w,1000);Require(c.Power()==100&&w.marks==1,"full power retained without valid mark target");w.canMark=true;c.Gain(w,0);Require(c.Power()==0&&w.marks==2,"mark resumes on valid target");c.Health(w,31);Require(w.spells.empty(),"no early frenzy");c.Health(w,30);c.Health(w,20);Require(w.spells.size()==1&&w.spells[0]==72737,"one low-health frenzy");for(unsigned i=0;i<164;i++)c.Tick(w,250);Require(w.beasts==(Raid25(mode)?5u:2u),"original two/five beasts");Require(w.scent==(Heroic(mode)?1u:0u),"heroic scent after ten seconds");Require(std::find(w.spells.begin(),w.spells.end(),72385)!=w.spells.end(),"boiling blood timer");for(unsigned i=164;i<1925;i++)c.Tick(w,250);Require(std::count(w.spells.begin(),w.spells.end(),26662)==1,"heroic/normal timed berserk occurs once");}
 std::cout<<"ICC_SAURFANG_PASS power mark_reset target_absence frenzy beasts heroic_scent berserk four_modes\n";
}
