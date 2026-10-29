#pragma once
// Executes the actual instance adapter against a deterministic world fixture.
// Native ECS compilation is checked separately; this fixture tests the adapter
// lifecycle, spell dispatch and protocol events without a database/network.
#include "../../SRC/Server/GameServer/raid/EclipseRaidRules.hpp"
#include "../../SRC/Server/GameServer/raid/icc/Marrowgar.hpp"
#include "../../SRC/Server/GameServer/raid/icc/MarrowgarSpells.hpp"
#include "../../SRC/Server/GameServer/raid/icc/AuraLedger.hpp"
#include "../../SRC/Server/GameServer/raid/icc/SpellRecords.generated.hpp"
#include "../../SRC/Server/GameServer/raid/icc/CreatureRecords.generated.hpp"
#include <map>
#include <set>
#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdio>
#include "../../SRC/Server/GameServer/raid/ICCTransientAffects.hpp"
#define LOG_ERROR(...) do { throw std::runtime_error("missing runtime spell"); } while(false)
namespace entt {using entity=uint32_t;constexpr entity null=0;}
namespace EclipseRaid {
struct TestActor {Vec point;bool present=true,dead=false,gm=false;uint32_t pid=0;int64_t hp=1000000000;};
std::map<entt::entity,TestActor> actors;std::vector<std::string> messages;uint32_t clockNow=0,serial=100;
std::map<entt::entity,uint32_t> spawnedCreatureSources;
uint32_t get_dword_time(){return clockNow;}
bool Valid(entt::entity e){return actors.count(e)!=0;}
namespace P {
int GetX(entt::entity e){return BaseX+int(actors.at(e).point.x*100);}int GetY(entt::entity e){return BaseY+int(actors.at(e).point.y*100);}
uint32_t GetPlayerID(entt::entity e){return Valid(e)?actors.at(e).pid:0;}
uint32_t GetPacketVID(entt::entity e){return e;}
bool IsPC(entt::entity e){return GetPlayerID(e)!=0;}bool IsGM(entt::entity e){return actors.at(e).gm;}
}
enum {POINT_ATT_SPEED=1,POINT_CASTING_SPEED=2,POINT_MOV_SPEED=3};
struct TestAffect {uint8_t point;int32_t delta;};std::map<std::pair<entt::entity,uint32_t>,TestAffect> nativeAffects;
namespace Points {int64_t GetMaxHP(entt::entity){return 1000000000;}int64_t Get(entt::entity e,uint8_t point){int64_t value=100;for(auto& row:nativeAffects)if(row.first.first==e&&row.second.point==point)value+=row.second.delta;return value;}}
namespace AffectSystem {bool RemoveAffect(entt::entity e,uint32_t type){return nativeAffects.erase({e,type})!=0;}bool AddAffect(entt::entity e,uint32_t type,uint8_t point,int32_t delta,uint32_t,int32_t,int32_t,bool){nativeAffects[{e,type}]={point,delta};return true;}}
namespace CombatSystem {bool IsDead(entt::entity e){return !Valid(e)||actors.at(e).dead;}void Damage(entt::entity e,entt::entity,int64_t amount,int){actors.at(e).hp-=amount;if(actors.at(e).hp<=0)actors.at(e).dead=true;}}
namespace M {void Stop(entt::entity){}void Goto(entt::entity,int,int){}void SetRotationToXY(entt::entity,int,int){}void SendMovePacket(entt::entity,int,int,int,int,uint32_t){}void Sync(entt::entity e,int x,int y){actors.at(e).point={(x-BaseX)*.01f,(y-BaseY)*.01f};}}
namespace ecs {struct ChatSystem {template<class...T>static void Send(entt::entity,int,const char* format,T... args){char buffer[1024];std::snprintf(buffer,sizeof(buffer),format,args...);messages.push_back(buffer);}};}
enum {DAMAGE_TYPE_SPECIAL,CHAT_TYPE_COMMAND,FUNC_MOVE,FUNC_ATTACK,FUNC_MOB_SKILL};
struct Member {uint32_t pid=0;Mechanic aura=Mechanic::Strike;uint32_t auraEnd=0,object=0;int stacks=0;};
struct SourceRuntime {virtual ~SourceRuntime()=default;virtual bool Update(uint32_t)=0;virtual void Attacked(entt::entity,int64_t)=0;virtual void Died(entt::entity)=0;virtual void Status(entt::entity){};virtual int64_t Outgoing(entt::entity,int64_t amount){return amount;};virtual int64_t Healing(entt::entity,int64_t amount){return amount;}virtual bool Dispel(entt::entity){return false;}virtual int64_t TargetDamage(entt::entity,entt::entity,int64_t amount){return amount;}};
struct Run {std::vector<Member> members;std::vector<entt::entity> bosses;std::set<uint32_t> rooted;ICC::Difficulty mode=ICC::Difficulty::Normal10;State state=State::Combat;uint32_t id=17,nextHazard=1;int encounter=1,resource=0;};
Vec Position(entt::entity e){return actors.at(e).point;}
entt::entity Player(const Member& m){return m.pid;}
bool Present(const Run&,entt::entity e){return Valid(e)&&actors.at(e).present;}
bool Alive(const Run& r,entt::entity e){return Present(r,e)&&!actors.at(e).dead;}
Member* Participant(Run& r,uint32_t pid){for(auto& m:r.members)if(m.pid==pid)return &m;return nullptr;}
void Notice(Run&,const char* key){messages.push_back(key);}
void Broadcast(Run&,const std::string& line){messages.push_back(line);}
bool Walk(const Run&,Vec p){return p.x>=460&&p.x<=564&&p.y>=265&&p.y<=365;}
std::vector<entt::entity> Living(Run& r){std::vector<entt::entity> out;for(auto m:r.members)if(Alive(r,Player(m)))out.push_back(Player(m));return out;}
entt::entity Spawn(Run&,uint32_t,Vec p,int,uint32_t,uint32_t source=0){auto id=++serial;actors.emplace(id,TestActor{p});spawnedCreatureSources[id]=source;return id;}
void Destroy(entt::entity e){actors.erase(e);}
int CalcMeleeDamage(entt::entity,entt::entity){return 1000;}
}
