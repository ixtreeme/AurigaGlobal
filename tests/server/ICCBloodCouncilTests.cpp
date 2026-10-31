#include "../../SRC/Server/GameServer/raid/icc/BloodCouncil.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct World:BloodCouncilPort{Difficulty mode=Difficulty::Normal10;std::vector<unsigned> invocations;std::vector<std::pair<unsigned,uint32_t>> spells;Difficulty Mode()const override{return mode;}void Empower(unsigned n)override{invocations.push_back(n);}void Ability(unsigned n,uint32_t s)override{spells.push_back({n,s});}};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World w;w.mode=mode;BloodCouncil c(42);c.Reset(w);Require(c.Active()==0&&w.invocations.size()==1,"Valanar starts empowered");for(int i=0;i<2405;i++)c.Tick(w,250);for(size_t i=1;i<w.invocations.size();i++)Require(w.invocations[i]!=w.invocations[i-1]&&w.invocations[i]<3,"invocation changes to another prince");Require(w.invocations.size()==14,"first invocation45 seconds then46");for(unsigned prince=0;prince<3;prince++)Require(std::count(w.spells.begin(),w.spells.end(),std::make_pair(prince,26662u))==1,"each prince berserks once");Require(std::any_of(w.spells.begin(),w.spells.end(),[](auto p){return p.second==71405||p.second==71815;}),"shadow prince casts source lances");Require(std::any_of(w.spells.begin(),w.spells.end(),[](auto p){return p.second==72039;}),"empowered shock vortex used");}
 std::cout<<"ICC_BLOOD_COUNCIL_PASS active_rotation prince_spells bombs berserk four_modes\n";
}
