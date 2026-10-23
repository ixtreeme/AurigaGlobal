"""FreeBSD isolated test of the production character-create starter item grant.
Build/run in a temporary directory; never connects to SQL or game services.
"""
from pathlib import Path
import json
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
source=(ROOT/'SRC/Server/GameServer/network/input_db.cpp').read_text()
start=source.index('\tstruct SInitialItem',source.index('void CInputDB::PlayerCreateSuccess'))
end=source.index('\n#endif\n',source.index('grant(76016, 40, -1);',start))
production=source[start:end]
fixture=r'''
#include <Core/stdafx.h>
#include <common/tables.h>
#include <common/character_creation.h>
#include <cassert>
#include <iostream>
#include <map>
#undef LOG_ERROR
#define LOG_ERROR(...) assert(false && "invalid starter item or full inventory")
constexpr int MAIN_RACE_MAX_NUM=8;
std::vector<TPlayerItem> granted;
struct DB {void DBPacket(int,int,const void* data,size_t size){assert(size==sizeof(TPlayerItem));granted.push_back(*static_cast<const TPlayerItem*>(data));}}db;
DB* db_clientdesc=&db;
struct Roll { bool skip=false; };
std::map<int,Roll> rolls;
namespace ItemSystem {
bool IsValidItem(int id){return rolls.count(id);}
void SetItemSkipSave(int id,bool skip){rolls.at(id).skip=skip;}
TPlayerItemAttribute GetItemAttribute(int id,int index){assert(rolls.count(id));return index<3?TPlayerItemAttribute{uint8_t(index+1),int16_t(10+index)}:TPlayerItemAttribute{};}
}
class ITEM_MANAGER {
 std::map<uint32_t,TItemTable> tables;
public:
 static ITEM_MANAGER& instance(){static ITEM_MANAGER self;return self;}
 const TItemTable* GetTable(uint32_t id){auto& t=tables[id];t.bSize=(id==3009?3:(id==19||id==2009||id>=11209&&id<=11809?2:1));if(id>=40129&&id<=45148||id==71231||id==88222||id==88224)t.aLimits[0]={LIMIT_REAL_TIME_START_FIRST_USE,604800};if(id>=40129&&id<=40134||id==41311||id==41312||id==45147||id==45148){t.bType=ITEM_COSTUME;t.bSubType=id<=40134?COSTUME_WEAPON:(id<=41312?COSTUME_BODY:COSTUME_HAIR);}return &t;}
 int CreateItem(uint32_t,uint32_t,uint32_t id,bool magic,int rare,bool skip){assert(id==0&&magic&&rare==-1&&skip);static int next=0;rolls.emplace(++next,Roll{});return next;}
 void DestroyItem(int id){assert(rolls.at(id).skip);rolls.erase(id);}
 uint32_t GetNewID(){static uint32_t id=0;return ++id;}
};
struct Packet {struct {uint8_t byJob=0,skill_group=1;uint32_t dwID=123;uint16_t wHairPart=21;}player;};
void Grant(Packet* pPacketDB){
// PRODUCTION_STARTERS
}
int main(){
 for(uint8_t race=0;race<8;++race)for(uint8_t group=1;group<=2;++group){
  Packet packet;packet.player.byJob=race;packet.player.skill_group=group;packet.player.wHairPart=44;granted.clear();Grant(&packet);
  std::map<int,TPlayerItem> equipment;std::array<bool,INVENTORY_MAX_NUM> occupied{};std::map<uint32_t,int> counts;
  for(auto& item:granted){
   auto proto=ITEM_MANAGER::instance().GetTable(item.vnum);
   if(proto->bType==ITEM_COSTUME){for(int i=0;i<3;++i)assert(item.aAttr[i].bType==i+1&&item.aAttr[i].sValue==10+i);for(int i=3;i<ITEM_ATTRIBUTE_MAX_NUM;++i)assert(item.aAttr[i].bType==0);}
   assert(item.owner==123&&item.id&&item.vnum!=50188);counts[item.vnum]+=item.count;
   if(item.window==EQUIPMENT){assert(equipment.emplace(item.pos,item).second);}
   else{assert(item.window==INVENTORY);int height=ITEM_MANAGER::instance().GetTable(item.vnum)->bSize;
    for(int row=0;row<height;++row){int cell=item.pos+row*INVENTORY_PAGE_COLUMN;assert(cell<INVENTORY_MAX_NUM&&!occupied[cell]);occupied[cell]=true;}
   }
  }
  assert(equipment.at(WEAR_WEAPON).vnum==character_creation::Weapon(race,group));
  assert(equipment.at(WEAR_BODY).vnum==character_creation::Armor(race));
  assert(equipment.at(WEAR_COSTUME_HAIR).vnum==(character_creation::Male(race)?45147:45148));
  assert(equipment.at(WEAR_COSTUME_BODY).vnum==(character_creation::Male(race)?41311:41312));
  assert(equipment.at(WEAR_COSTUME_WEAPON).vnum==character_creation::WeaponCostume(equipment.at(WEAR_WEAPON).vnum));
  assert(equipment.at(WEAR_COSTUME_MOUNT).vnum==71231);
  assert(equipment.at(WEAR_COSTUME_EFFECT_WEAPON).vnum==88222&&equipment.at(WEAR_COSTUME_EFFECT_BODY).vnum==88224);
  for(int slot:{WEAR_COSTUME_HAIR,WEAR_COSTUME_BODY,WEAR_COSTUME_WEAPON,WEAR_COSTUME_MOUNT,WEAR_COSTUME_EFFECT_WEAPON,WEAR_COSTUME_EFFECT_BODY})
   assert(equipment.at(slot).alSockets[0]>time(nullptr)&&equipment.at(slot).alSockets[1]==1);
  for(auto [slot,id]:{std::pair{WEAR_WRIST,14009},std::pair{WEAR_FOOTS,15009},std::pair{WEAR_NECK,16009},std::pair{WEAR_EAR,17009}}){
   auto& item=equipment.at(slot);assert(item.vnum==id&&item.aAttr[0].bType!=0&&item.aAttr[0].sValue!=0);
  }
  assert(equipment.size()==14&&counts[76016]==40&&counts[50187]==1);
  assert(rolls.empty());
  for(uint32_t id:{88943u,88946u,88947u,88948u,88951u,88952u,88953u,88954u,88955u,88957u})assert(counts[id]==0);
  const uint32_t weapons[4][3]={{19,3009,0},{1009,2009,19},{19,0,0},{5009,7009,0}};
  for(uint32_t id:weapons[race%4])if(id){assert(counts[id]==1);assert(counts[character_creation::WeaponCostume(id)]==1);}
 }
 std::cout<<"STARTER_EQUIPMENT=PASS races=8 groups=2 equipped=14 costumes=weapon-and-sex-matched expiry=initialized inventory=no-overlap removed-consumables=absent\n";
}
'''.replace('// PRODUCTION_STARTERS',production)
commands=json.loads(subprocess.check_output(['ninja','-C','/Auriga/Source/build','-t','compdb'],text=True))
entry=next(c for c in commands if c['file'].endswith('/tests/server/PlayerCreateTests.cpp') and ' -c ' in c['command'])
with tempfile.TemporaryDirectory(prefix='auriga-starter-test-') as tmp:
    tmp=Path(tmp);test=tmp/'test.cpp';test.write_text(fixture);exe=tmp/'test'
    args=shlex.split(entry['command']);clean=[];i=0
    while i<len(args):
        if args[i] in ('-o','-MF','-MT','-MQ'):i+=2;continue
        if args[i] in ('-c','-MD','-MMD','-MP','-DNDEBUG') or args[i]==entry['file']:i+=1;continue
        clean.append(args[i]);i+=1
    clean[1:1]=['-I'+str(ROOT/'SRC/Server')]
    clean+=[str(test),'-o',str(exe)]
    compiled=subprocess.run(clean,cwd=entry['directory'],capture_output=True,text=True)
    if compiled.returncode:print(compiled.stdout+compiled.stderr);raise SystemExit(compiled.returncode)
    subprocess.run([str(exe)],check=True)
