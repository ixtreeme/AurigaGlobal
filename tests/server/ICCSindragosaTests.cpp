#include "../../SRC/Server/GameServer/raid/icc/Sindragosa.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct World:SindragosaPort{Difficulty mode=Difficulty::Normal10;std::vector<uint32_t> spells;std::vector<bool> flights;unsigned finals=0;Difficulty Mode()const override{return mode;}void Ability(uint32_t s)override{spells.push_back(s);}void Flight(bool f)override{flights.push_back(f);}void FinalPhase()override{finals++;}};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World w;w.mode=mode;Sindragosa c(42);c.Reset(w);for(unsigned i=0;i<204;i++)c.Tick(w,250);Require(c.Flying()&&w.flights.size()==1,"air phase starts50 seconds");c.Health(w,35);Require(c.FinalRequested()&&!c.Final()&&!w.finals,"final transition waits for landing");for(unsigned i=204;i<340;i++)c.Tick(w,250);Require(!c.Flying()&&c.Final()&&w.finals==1,"landing enters final phase once");Require(std::count(w.spells.begin(),w.spells.end(),69845)==4,"four frost bombs each air phase");for(unsigned i=0;i<2400;i++)c.Tick(w,250);Require(w.flights.size()==2,"no further flight below35 percent");Require(std::find(w.spells.begin(),w.spells.end(),70128)!=w.spells.end(),"final phase mystic buffet");Require(Sindragosa::TombCount(mode,true)==RaidMode<unsigned>(mode,2,5,2,6),"source ice tomb target counts");Require(std::count(w.spells.begin(),w.spells.end(),26662)==1,"ten minute berserk");}
 std::cout<<"ICC_SINDRAGOSA_PASS air four_bombs deferred_final no_late_flight buffet tomb_counts enrage four_modes\n";
}
