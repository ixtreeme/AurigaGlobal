#include "../../SRC/Server/GameServer/raid/icc/BloodQueen.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct World:BloodQueenPort{Difficulty mode=Difficulty::Normal10;std::vector<uint32_t> spells;std::vector<unsigned> stages;Difficulty Mode()const override{return mode;}void Ability(uint32_t s)override{spells.push_back(s);}void Flight(unsigned s)override{stages.push_back(s);}};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World w;w.mode=mode;BloodQueen c(42);c.Reset(w);for(unsigned i=0;i<1360;i++)c.Tick(w,250);Require(std::count(w.spells.begin(),w.spells.end(),71726)==1,"only initial queen bite is automatic");Require(std::count(w.spells.begin(),w.spells.end(),26662)==1,"five-and-half-minute berserk");Require(w.stages.size()>=6&&w.stages.size()%3==0,"air cycle has preparation raise and landing");for(size_t i=0;i<w.stages.size();i++)Require(w.stages[i]==i%3,"flight phases in order");Require(std::count(w.spells.begin(),w.spells.end(),71446)==int(w.stages.size()/3*2),"two volleys each air phase");Require(!c.Flying(),"landed after completed flight");}
 std::cout<<"ICC_BLOOD_QUEEN_PASS initial_bite mirrors ground_abilities flight_sequence volleys enrage four_modes\n";
}
