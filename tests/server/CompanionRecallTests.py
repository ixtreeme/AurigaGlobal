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
constexpr int PHASE_GAME=5;
struct CAffect { uint32_t dwFlag=0; };
struct Descriptor { bool game=true; bool IsPhase(int) const {return game;} } descriptor;
bool connected=true, placed=true, loaded=true, validItem=true, failSpawn=false;
int mountChecks=0,mapIndex=1;bool arena=false;
struct CArenaManager {static CArenaManager& instance(){static CArenaManager self;return self;}bool IsArenaMap(int){return arena;}};
namespace MountSystem {void CheckMount(int){++mountChecks;}}
namespace ecs::PlayerRuntime {
int GetMapIndex(entt::entity){return mapIndex;}
Descriptor* GetDesc(entt::entity){return connected?&descriptor:nullptr;}
void* GetSectree(entt::entity){return placed?&descriptor:nullptr;}
}
namespace InventorySystem {bool IsItemLoaded(entt::entity){return loaded;}}
namespace AffectSystem {void AutoRecallProcess(entt::entity);}
'''
checks = r'''
void reset(){connected=placed=loaded=descriptor.game=true;mountChecks=0;mapIndex=1;arena=false;}
int main(){
 reset();AffectSystem::AutoRecallProcess(1);assert(mountChecks==1);
 for(int gate=0;gate<4;++gate){reset();
  if(gate==0)connected=false;if(gate==1)descriptor.game=false;
  if(gate==2)placed=false;if(gate==3)loaded=false;
  AffectSystem::AutoRecallProcess(1);assert(mountChecks==0);
 }
 reset();mapIndex=113;AffectSystem::AutoRecallProcess(1);assert(mountChecks==0);
 reset();arena=true;AffectSystem::AutoRecallProcess(1);assert(mountChecks==0);
 std::cout<<"COMPANION_RECALL=PASS mount-recovery, restricted-map, phase, placement, inventory; no legacy pet service dependency\n";
}
'''

with tempfile.TemporaryDirectory(prefix='auriga-companion-recall-') as directory:
    folder = Path(directory)
    unit = folder / 'recall.cpp'
    unit.write_text('#define ENABLE_MOUNT_COSTUME_SYSTEM\n' + fixture + production + checks)
    binary = folder / ('recall-test.exe' if os.name == 'nt' else 'recall-test')
    if os.name == 'nt':
        subprocess.run([os.environ.get('CXX','cl'),'/nologo','/std:c++latest','/EHsc',
            str(unit),'/Fe:'+str(binary),'/Fo:'+str(folder/'recall.obj')],check=True,cwd=folder)
    else:
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++23',str(unit),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
