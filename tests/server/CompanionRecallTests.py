"""Compile the production AutoRecallProcess body with isolated service doubles.

Run on FreeBSD: python3 tests/server/CompanionRecallTests.py
No server processes, account data or sockets are touched by this test.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'SRC/Server/GameServer/ecs/systems/ItemSystem_LegacyBridge.cpp').read_text()
begin = source.index('void AffectSystem::AutoRecallProcess(')
brace = source.index('{', begin)
depth, end = 1, brace + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
production = source[begin:end]

fixture = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
namespace entt { using entity=int; constexpr entity null=-1; }
constexpr int PHASE_GAME=5, AFFECT_RECALL1=1, AFFECT_RECALL2=2;
struct CAffect { uint32_t dwFlag=0; };
struct Descriptor { bool game=true; bool IsPhase(int) const {return game;} } descriptor;
bool connected=true, placed=true, loaded=true, validItem=true, failSpawn=false;
int mountChecks=0,mapIndex=1;bool arena=false;
struct CArenaManager {static CArenaManager& instance(){static CArenaManager self;return self;}bool IsArenaMap(int){return arena;}};
namespace MountSystem {void CheckMount(int){++mountChecks;}}
bool effects[3]={false,true,true}; int live[3]={0,0,0}, calls[3]={0,0,0}, removed=0;
CAffect records[3]={{0},{1},{2}};
namespace ecs::PlayerRuntime {
int GetMapIndex(entt::entity){return mapIndex;}
Descriptor* GetDesc(entt::entity){return connected?&descriptor:nullptr;}
void* GetSectree(entt::entity){return placed?&descriptor:nullptr;}
}
namespace InventorySystem {bool IsItemLoaded(entt::entity){return loaded;}}
namespace ItemSystem {
entt::entity FindItemByID(entt::entity,uint32_t id){return validItem?int(id):entt::null;}
bool IsValidItem(entt::entity e){return e!=entt::null&&validItem;}
int GetItemValue(entt::entity,int){return 34041;}
// A persisted active socket must never be consulted as a live actor test.
int GetItemSocket(entt::entity,int){assert(false&&"stale socket gate used");return 1;}
}
namespace AffectSystem {
void AutoRecallProcess(entt::entity);
const CAffect* FindAffect(entt::entity,int kind){return effects[kind]?&records[kind]:nullptr;}
void RemoveAffect(entt::entity,CAffect* p){effects[p->dwFlag]=false;++removed;}
}
namespace PetSystem {
int CountSummoned(entt::entity){return live[1];}
void* Summon(entt::entity,int,entt::entity,const char*,bool){++calls[1];if(!failSpawn)++live[1];return nullptr;}
}
namespace NewPetSystem {
int CountSummoned(entt::entity){return live[2];}
void* Summon(entt::entity,int,entt::entity,const char*,bool){++calls[2];if(!failSpawn)++live[2];return nullptr;}
}
'''
checks = r'''
void reset(){connected=placed=loaded=validItem=descriptor.game=true;failSpawn=false;mountChecks=0;mapIndex=1;arena=false;
 effects[1]=effects[2]=true;live[1]=live[2]=calls[1]=calls[2]=removed=0;}
int main(){
 reset();AffectSystem::AutoRecallProcess(1);
 assert(calls[1]==1&&calls[2]==1&&live[1]==1&&live[2]==1&&mountChecks==1);
 AffectSystem::AutoRecallProcess(1);assert(calls[1]==1&&calls[2]==1);
 for(int gate=0;gate<4;++gate){reset();
  if(gate==0)connected=false;if(gate==1)descriptor.game=false;
  if(gate==2)placed=false;if(gate==3)loaded=false;
  AffectSystem::AutoRecallProcess(1);assert(calls[1]==0&&calls[2]==0&&removed==0&&mountChecks==0);
 }
 reset();failSpawn=true;AffectSystem::AutoRecallProcess(1);
 assert(calls[1]==1&&calls[2]==1&&effects[1]&&effects[2]&&removed==0);
 failSpawn=false;AffectSystem::AutoRecallProcess(1);assert(live[1]==1&&live[2]==1);
 reset();validItem=false;AffectSystem::AutoRecallProcess(1);
 assert(removed==2&&calls[1]==0&&calls[2]==0);
 reset();effects[1]=effects[2]=false;AffectSystem::AutoRecallProcess(1);
 assert(calls[1]==0&&calls[2]==0);
 reset();mapIndex=113;AffectSystem::AutoRecallProcess(1);assert(mountChecks==0);
 reset();arena=true;AffectSystem::AutoRecallProcess(1);assert(mountChecks==0);
 std::cout<<"COMPANION_RECALL=PASS mount-recovery,restricted-map,stale-marker,repeat,loading,placement,inventory,retry,missing-item\n";
}
'''

with tempfile.TemporaryDirectory(prefix='auriga-companion-recall-') as directory:
    folder = Path(directory)
    unit = folder / 'recall.cpp'
    unit.write_text('#define ENABLE_MOUNT_COSTUME_SYSTEM\n#define __PET_SYSTEM__\n#define __NEWPET_SYSTEM__\n' + fixture + production + checks)
    binary = folder / 'recall-test'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++23', str(unit), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
