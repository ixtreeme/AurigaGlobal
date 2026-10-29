#include "../../SRC/Server/GameServer/raid/icc/MarrowgarSpells.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct World:MarrowgarFlamePort {
 int block=0;std::vector<FlameSpawn> flames;
 bool LineOfSight(Point p)const override{return block==0||(block==1&&PointDistance({},p)<1)||(block==2&&p.x>0);}
 void SpawnFlame(const FlameSpawn& f)override{flames.push_back(f);}
};
int main(){
 EventMap random(37);std::vector<FlameTarget> players={{1,{1,0},.5f},{2,{12,0},.5f},{3,{14,0},.5f,true,true},{4,{30,0},.5f,false,false}};
 for(int n=0;n<100;n++)Require(SelectColdflameTarget(random,{},5,1,players)->id==2,"choose unimpaled non-tank outside combat reach");
 std::vector<FlameTarget> edge={{1,{40,0},.5f},{2,{6.5f,0},.5f},{3,{6.49f,0},.5f}};
 for(int n=0;n<100;n++)Require(SelectColdflameTarget(random,{},5,1,edge)->id==2,"negative range excludes tank and strict inside-range, includes exact boundary");
 players.erase(players.begin()+1);Require(SelectColdflameTarget(random,{},50,1,players).has_value(),"all inside model uses upstream fallback");
 Require(!SelectColdflameTarget(random,{},5,1,{}),"empty list does not invent target");
 World world;Require(CastColdflame(world,{},10,{20,0},69138),"normal summon");Require(std::abs(world.flames[0].position.x-5)<.001f,"spawn at half boss object size");
 world.flames.clear();world.block=1;Require(CastColdflame(world,{},10,{20,0},69138),"fallback around blocked boss model");Require(std::abs(world.flames[0].position.x-.5f)<.001f,"half metre LOS retry");
 world.flames.clear();world.block=3;Require(!CastColdflame(world,{},10,{20,0},69138)&&world.flames.empty(),"both LOS probes blocked means no summon");
 world.block=0;Require(CastStormColdflame(world,{},72701)==4,"four diagonal storm trails");for(size_t i=0;i<4;i++){Require(world.flames[i].summonSpell==72701+i,"source consecutive summon IDs");Require(std::abs(PointDistance({},world.flames[i].position)-2.5f)<.001f,"storm spawn radius");}
 world.flames.clear();world.block=2;Require(CastStormColdflame(world,{},72701)==2,"storm trails independently respect LOS");
 std::cout<<"ICC_MARROWGAR_SPELLS_PASS target_fallback model_offset four_diagonals line_of_sight\n";
}
