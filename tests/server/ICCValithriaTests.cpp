#include "../../SRC/Server/GameServer/raid/icc/Valithria.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct World:ValithriaPort{Difficulty mode=Difficulty::Normal10;unsigned ports=0,won=0,lost=0;std::array<unsigned,5> counts{};std::vector<unsigned> notices;Difficulty Mode()const override{return mode;}uint32_t SpawnPeriod(unsigned k)const override{return k==2||k==3?30000:60000;}void Wave(unsigned k,unsigned n)override{counts[k]+=n;}void Portals(unsigned n)override{ports+=n;}void HealthNotice(unsigned p)override{notices.push_back(p);}void Victory()override{won++;}void Defeat()override{lost++;}};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World w;w.mode=mode;Valithria c(42);c.Reset(w);Require(w.counts[3]==4,"four initial channeling archmages");for(unsigned i=0;i<200;i++)c.Tick(w,250);Require(w.ports==(Raid25(mode)?8u:3u),"source three/eight dream portals");Require(w.counts[0]==0&&w.counts[2]==1,"summoning starts after initial aura plus source period");c.Health(w,80,100);c.Health(w,90,100);c.Health(w,20,100);Require(w.notices.size()==2,"75/25 percent notices once");c.Health(w,100,100);c.Health(w,0,100);Require(w.won==1&&!w.lost,"successful healing never becomes a death reward");c.Reset(w);for(unsigned i=0;i<1685;i++)c.Tick(w,250);Require(w.lost==(Heroic(mode)?1u:0u),"heroic seven minute berserk defeat");Require(w.counts[1]>6,"suppresser waves accelerate");}
 std::cout<<"ICC_VALITHRIA_PASS healing_victory no_death_win initial_mages portal_counts decaying_waves heroic_timeout four_modes\n";
}
