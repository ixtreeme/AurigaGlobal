#include "../../SRC/Server/GameServer/raid/icc/Festergut.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
struct World:FestergutPort {
 Difficulty mode;uint32_t now=0;bool casting=false;struct CastRow{uint32_t id,target,time;int max;};std::vector<CastRow> casts;int removed=0,goo=0;
 explicit World(Difficulty d):mode(d){}
 Difficulty Mode()const override{return mode;}bool HasVictim()const override{return true;}bool Casting()const override{return casting;}uint32_t Victim()const override{return 1;}
 std::vector<Candidate> Targets()const override{return {{1,2},{2,3},{3,10},{4,12},{5,16},{6,20},{7,25},{8,30},{9,35},{10,40}};}
 void Cast(uint32_t spell,uint32_t target,bool,int maximum)override{casts.push_back({spell,target,now,maximum});}
 void RemoveBlight()override{removed++;}void ProfessorCast(uint32_t spell,uint32_t target)override{Require(spell==72296&&target!=1,"heroic goo excludes tank");goo++;}void ProfessorAction(int)override{}void Talk(int)override{}void Melee()override{}
};
int main(){for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25}){World w(mode);Festergut boss(17);boss.Reset(w);boss.Engaged(w);for(uint32_t time=50;time<=305000;time+=50){w.now=time;boss.Tick(w,50);}bool inhale=false,pungent=false,spore=false,enrage=false;for(auto c:w.casts){if(c.id==FestergutSpell::Inhale&&!inhale){Require(c.time>=25000&&c.time<=30100,"first inhale source timing");inhale=true;}if(c.id==FestergutSpell::Pungent)pungent=true;if(c.id==FestergutSpell::Spore){spore=true;Require(c.max==(Raid25(mode)?3:2),"spore target cap by raid size");}if(c.id==FestergutSpell::Berserk){enrage=true;Require(c.time>=300000&&c.time<300200,"five minute enrage");}if(c.id==FestergutSpell::VileGas)Require(c.target>=3,"enough ranged means no melee vile-gas target");}Require(inhale&&pungent&&spore&&enrage,"full gas cycle");Require((w.goo>0)==Heroic(mode),"heroic-only professor goo");Require(boss.FluShotShortage(),"achievement starts eligible");boss.Inoculated(3);Require(!boss.FluShotShortage(),"achievement tracks maximum not current stacks");boss.Reset(w);Require(boss.FluShotShortage(),"wipe resets achievement");}
 Require(!Festergut::GrantsInoculation(11999,12000,false,0,0),"fresh DoT replacement must not inoculate");Require(Festergut::GrantsInoculation(0,12000,false,0,0),"expired spores inoculate");Require(!Festergut::GrantsInoculation(0,12000,true,35000,40000),"overlapping spores cannot grant two fresh stacks");Require(Festergut::GrantsInoculation(0,12000,true,30000,40000),"ten second guard boundary");Require(Festergut::GastricExplodes(10)&&!Festergut::GastricExplodes(9),"gastric explosion exact stack threshold");
 std::cout<<"ICC_FESTERGUT_DOMAIN_PASS four_modes source_timings ranged_selection inoculation gas_cycle heroic_goo\n";
}
