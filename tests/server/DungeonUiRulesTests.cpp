#include "../../SRC/Server/GameServer/dungeon/DungeonUiRules.hpp"
#include "../../SRC/Server/GameServer/world/WarpRoutingRules.hpp"
#include <iostream>
#include <stdexcept>
int main(){
    using namespace DungeonUi;unsigned value=0;unsigned checks=0;
    auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("Dungeon UI rule failed");};
    check(Parse("367",value)&&value==367);check(Parse("0",value)&&value==0);
    for(const auto* bad:{"","-1","+1","1x","1 2","4294967296","99999999999999999999"})check(!Parse(bad,value));
    check(Add(UINT64_MAX-4,7)==UINT64_MAX);check(Add(4,7)==11);
    check(Grade(100,0,0)==3);check(Grade(100,0,2)==2);check(Grade(100,0,3)==1);
    check(Grade(300,600,0)==3);check(Grade(301,600,0)==2);check(Grade(450,600,2)==2);check(Grade(451,600,0)==1);check(Grade(300,600,3)==1);
    check(EntryOpen(0,40,40,75,100,100));check(EntryOpen(0,75,40,75,0,100));
    check(!EntryOpen(2,50,40,75,0,100));check(!EntryOpen(0,39,40,75,0,100));check(!EntryOpen(0,76,40,75,0,100));check(!EntryOpen(0,50,40,75,101,100));
    check(auriga::world::WarpChannel(1,99,660000,true)==99);
    check(auriga::world::WarpChannel(1,99,660000,false)==1);
    check(auriga::world::WarpChannel(1,99,66,true)==1);
    check(auriga::world::WarpChannel(2,2,660000,true)==2);
    std::cout<<"DUNGEON_UI_RULES=PASS checks="<<checks<<'\n';
}
