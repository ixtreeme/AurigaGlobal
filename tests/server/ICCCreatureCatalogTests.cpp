#include "../../SRC/Server/GameServer/raid/icc/CreatureRecords.generated.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool good,const char* why){if(!good)throw std::runtime_error(why);}
int main(){
 uint32_t previous=0;for(auto& row:CreatureRecords){Require(row.id>previous,"sorted unique templates");previous=row.id;Require(row.attackPeriod>0&&row.runSpeed>=0&&row.health>0,"source defaults normalized safely");}
 auto marrowgar=Creatures().Resolve(36612,Difficulty::Normal10);Require(marrowgar&&marrowgar->health==6972500&&marrowgar->attackPeriod==1000,"source ten-player marrowgar health and one-second swing");
 for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25})for(auto id:BossCreatureIDs)Require(Creatures().Resolve(id,mode)!=nullptr,"all twelve encounter stat sources exist in every mode");
 Require(Creatures().Resolve(36612,Difficulty::Normal25)->health>marrowgar->health,"twenty-five-player template health");
 Require(Creatures().Resolve(36899,Difficulty::Heroic25)->id==Creatures().Resolve(36899,Difficulty::Normal25)->id,"big ooze heroic falls back to matching raid-size normal template");
 Require(Creatures().Resolve(36897,Difficulty::Normal10)->runSpeed>Creatures().Resolve(36899,Difficulty::Normal10)->runSpeed,"small ooze pursues faster than large ooze");
 Require(!Creatures().Resolve(999999,Difficulty::Normal10),"missing source creature not invented");
 std::cout<<"ICC_CREATURE_CATALOG_PASS "<<CreatureRecords.size()<<"_templates four_modes health damage swing_period speed fallback\n";
}
