"""FreeBSD-only isolated regression of production stone cleanup and drop block.
No game processes, sockets or database access. Run: python3 StoneRewardTests.py.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def body(source, marker):
    start = source.index(marker)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

combat = (ROOT/'SRC/Server/GameServer/ecs/systems/CombatSystem.cpp').read_text()
clear = body(combat, 'void ClearStone(entt::entity e,')
drop = body((ROOT/'SRC/Server/GameServer/item/item_manager.cpp').read_text(), '// Drop Item Group')
fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <type_traits>
#include <vector>
#include "common/character_creation.h"
namespace entt { using entity=int; constexpr int null=-1; }
namespace ecs {
struct BattleContribution {uint64_t totalDamage=0;int aggro=0;};
struct DamageLedger {std::map<int,BattleContribution> entries;};
struct StoneSpawns {std::vector<int> members;};
struct StoneOwner {int stone=entt::null;};
}
struct Registry {
 std::map<int,ecs::StoneSpawns> spawns;
 std::map<int,ecs::StoneOwner> owners;
 std::array<bool,32> alive{};
 bool valid(int e){return e>=0&&e<32&&alive[e];}
 template<class T>T* try_get(int e){
  if constexpr(std::is_same_v<T,ecs::StoneSpawns>){auto it=spawns.find(e);return it==spawns.end()?nullptr:&it->second;}
  else {auto it=owners.find(e);return it==owners.end()?nullptr:&it->second;}
 }
 template<class T>void remove(int e){owners.erase(e);}
}g_registry;
struct Flags {int instantFlag=0;};
std::array<Flags,32> flags;
std::array<ecs::DamageLedger,32> ledgers;
std::array<bool,32> dead{};
std::array<int,32> awards{},deathCalls{};
std::array<std::map<int,ecs::BattleContribution>,32> awardedContributions;
int destroyDuringAward=-1;
constexpr int INSTANT_FLAG_NO_REWARD=1;
#define ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
#define SET_BIT(value,bit) ((value)|=(bit))
namespace CombatSystem {
bool IsDead(int e){return dead[e];}
Flags* RuntimeFlags(int e){return &flags[e];}
void SetStone(int e,int stone){if(stone==entt::null)g_registry.owners.erase(e);else g_registry.owners[e]={stone};}
ecs::DamageLedger& DamageLedgerOf(int e){return ledgers[e];}
void DistributeExp(int e){assert(!g_registry.owners.contains(e));++awards[e];awardedContributions[e]=ledgers[e].entries;if(e==destroyDuringAward)g_registry.alive[e]=false;}
void Dead(int e,int){assert(flags[e].instantFlag&INSTANT_FLAG_NO_REWARD);++deathCalls[e];dead[e]=true;}
// PRODUCTION_CLEAR
}
bool stone=true;int race=8001,roll=1;
namespace ecs::PlayerRuntime {int GetRaceNum(int){return race;}bool IsStone(int){return stone;}int GetPolymorphItemVnum(int){return 0;}}
namespace ecs {bool IsCharacter(int){return true;}}
namespace ecs::PointSystem {int GetLevel(int){return 5;}}
namespace ItemSystem {bool IsValidItem(int e){return e>0;}int GetItemType(int){return 0;}int GetItemVnum(int e){return e;}void SetItemSocket(int,int,int){}}
constexpr int ITEM_POLYMORPH=99;
#define LOG_ERROR(...) ((void)0)
int number(int,int){return roll;}
struct Drop {uint32_t dwVnum;int iCount;int dwPct;};
struct Group {std::vector<Drop> entries;auto GetVector(){return entries;}};
struct Manager {
 std::map<int,Group*> m_map_pkDropItemGroup;
 int lastCount=0;
 int CreateItem(int id,int count,int,bool){lastCount=count;return id;}
 std::vector<int> Run(){int chr=0,killer=1,iDeltaPercent=100,iRandRange=1000000,item=0;std::vector<int> vec_item;
 // PRODUCTION_DROP
 return vec_item;}
};
int main(){
 constexpr uint32_t bases[]{73001,73251,73501,73751,75001,75201,75401,75601};
 for(int r=0;r<8;++r)for(int s=0;s<12;++s){auto a=21+s%4+s/4*10;assert(character_creation::HairItem(r,a)==bases[r]+s);}
 assert(character_creation::StartLevel==5&&!character_creation::ValidHair(25));
 assert(character_creation::Weapon(0,2)==3009&&character_creation::Weapon(1,2)==2009&&character_creation::Weapon(3,1)==7009);
 g_registry.alive.fill(true);g_registry.spawns[0].members={1,2,3,4};
 for(int e:{1,2,3,4})g_registry.owners[e]={0};
 ecs::DamageLedger credit;credit.entries[10]={75,0};credit.entries[11]={25,0};
 ledgers[2].entries[11]={5,0};dead[3]=true;g_registry.alive[4]=false;
 CombatSystem::ClearStone(0,&credit);
 assert(awards[1]==1&&awardedContributions[1][10].totalDamage==75&&awardedContributions[1][11].totalDamage==25);
 assert(awards[2]==1&&awardedContributions[2].size()==1&&awardedContributions[2][11].totalDamage==5);
 assert(awards[3]==0&&awards[4]==0&&deathCalls[4]==0);
 CombatSystem::ClearStone(0,&credit);assert(awards[1]==1&&awards[2]==1);
 g_registry.spawns[5].members={6};g_registry.owners[6]={5};CombatSystem::ClearStone(5,nullptr);assert(awards[6]==0&&dead[6]);
 g_registry.spawns[7].members={8};g_registry.owners[8]={7};destroyDuringAward=8;CombatSystem::ClearStone(7,&credit);assert(awards[8]==1&&deathCalls[8]==0);
 Manager manager;Group group{{{19,2,1000000}}};manager.m_map_pkDropItemGroup[8001]=&group;
 stone=true;auto result=manager.Run();assert(result.size()==1&&result[0]==19&&manager.lastCount==2);
 stone=false;assert(manager.Run().size()==1);
 group.entries[0].dwPct=1;roll=2;stone=true;assert(manager.Run().empty());
 manager.m_map_pkDropItemGroup[8001]=nullptr;assert(manager.Run().empty());
 std::cout<<"STONE_REWARDS=PASS stone-drop,drop-count,chance,one-shot-exp,shares,existing-damage,no-duplicate,cleanup,stale-entity\n";
}
'''.replace('// PRODUCTION_CLEAR', clear).replace('// PRODUCTION_DROP', drop)
with tempfile.TemporaryDirectory(prefix='auriga-stone-test-') as tmp:
    source=Path(tmp)/'test.cpp';source.write_text(fixture)
    executable=Path(tmp)/'test'
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++23','-I'+str(ROOT/'SRC/Server'),str(source),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
