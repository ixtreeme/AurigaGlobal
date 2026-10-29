#include "../core/stdafx.h"
#include "EclipseRaidSystem.hpp"
#include "EclipseRaidRules.hpp"
#include "EclipseVoiceTiming.hpp"
#include "icc/Difficulty.hpp"
#include "icc/Marrowgar.hpp"
#include "icc/Festergut.hpp"
#include "icc/Rotface.hpp"
#include "icc/MarrowgarSpells.hpp"
#include "icc/AuraLedger.hpp"
#include "ICCTransientAffects.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "icc/SpellRecords.generated.hpp"
#include "icc/CreatureRecords.generated.hpp"
#include "../core/config.h"
#include "../combat/battle.h"
#include "../core/JsonFile.h"
#include "../core/locale_service.h"
#include "../core/utils.h"
#include "../command/cmd.h"
#include "../core/packet.h"
#include "../command/gm.h"
#include "../entity/char_manager.h"
#include "../entity/mob_manager.h"
#include "../entity/char.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../social/party.h"
#include "../world/sectree_manager.h"
#include <map>
#include <set>
#include <sstream>
#include <random>

namespace EclipseRaid {
namespace {
namespace P=ecs::PlayerRuntime;namespace M=ecs::MovementSystem;namespace Points=ecs::PointSystem;
struct Member {uint32_t pid=0;bool ready=false,rewarded=false;uint32_t lastAction=0,missing=0;Mechanic aura=Mechanic::Strike;uint32_t auraEnd=0,object=0;int stacks=0;};
struct SourceRuntime {virtual ~SourceRuntime()=default;virtual bool Update(uint32_t now)=0;virtual void Attacked(entt::entity attacker,int64_t damage)=0;virtual void Died(entt::entity actor)=0;virtual void Status(entt::entity){};virtual int64_t Outgoing(entt::entity,int64_t amount){return amount;};virtual int64_t Healing(entt::entity,int64_t amount){return amount;}virtual bool Dispel(entt::entity){return false;}virtual int64_t TargetDamage(entt::entity,entt::entity,int64_t amount){return amount;}};
struct Run {std::shared_ptr<SourceRuntime> source;std::set<uint32_t> rooted;ICC::Difficulty mode=ICC::Difficulty::Normal10;uint32_t id=0,leader=0,started=0,changed=0,lastSend=0,emptyAt=0,combatAt=0,immuneUntil=0,countdownAt=0;int countdownEncounter=0;int map=0,encounter=0,empowered=0,resource=0,phase=0;uint16_t cleared=0;State state=State::Waiting;std::vector<Member> members;std::vector<entt::entity> actors,bosses;std::set<uint32_t> dead;std::vector<Hazard> hazards;Scheduler scheduler;uint32_t nextHazard=1;uint64_t barrier=0;bool completionPending=false;};
std::map<uint32_t,Run> runs;bool loaded=false,enabled=false,gmOnly=true;uint16_t availableEncounters=1;uint32_t previous=0;int maximumRuns=16,minimumMembers=1,durationSeconds=5400;int64_t rewardYang=500000;uint32_t sequence=1;
bool Valid(entt::entity e){return e!=entt::null&&g_registry.valid(e);}
Vec Position(entt::entity e){return {(P::GetX(e)-BaseX)*.01f,(P::GetY(e)-BaseY)*.01f};}
entt::entity Player(const Member& m){return P::FindByPlayerID(m.pid);}
Member* Participant(Run& r,uint32_t pid){for(auto& m:r.members)if(m.pid==pid)return &m;return nullptr;}
Run* For(entt::entity p){if(!Valid(p))return nullptr;uint32_t pid=P::GetPlayerID(p);for(auto& [id,r]:runs)if(Participant(r,pid))return &r;return nullptr;}
Run* Of(entt::entity e){if(!Valid(e))return nullptr;if(auto* a=g_registry.try_get<Actor>(e)){auto it=runs.find(a->run);return it==runs.end()?nullptr:&it->second;}return For(e);}
bool Present(const Run& r,entt::entity p){return Valid(p)&&P::GetDesc(p)&&P::GetMapIndex(p)==r.map;}
bool Alive(const Run& r,entt::entity p){return Present(r,p)&&!CombatSystem::IsDead(p);}
void Notice(entt::entity p,const char* key){if(Valid(p)&&P::GetDesc(p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_notice %s",key);}
void Notice(Run& r,const char* key){for(auto& m:r.members)Notice(Player(m),key);}
void Broadcast(Run& r,const std::string& line){for(auto& m:r.members){auto p=Player(m);if(Present(r,p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"%s",line.c_str());}}
void Destroy(entt::entity e){if(!Valid(e))return;auto actor=g_registry.try_get<Actor>(e);auto retainedPrototype=actor?actor->prototype:std::shared_ptr<CMob>{};CHARACTER_MANAGER::instance().DestroyCharacter(e);}
void Load(){loaded=true;nlohmann::json config;if(!JsonFile::Load(LocaleService_GetBasePath()+"/eclipse_raid.json",config))return;
 if(config.value("schema_version",0)!=1||config.value("template_index",0)!=MapIndex)return;
 enabled=config.value("enabled",false);minimumMembers=std::clamp(config.value("minimum_members",1),1,Capacity);maximumRuns=std::clamp(config.value("maximum_instances",16),1,64);durationSeconds=std::clamp(config.value("duration_seconds",5400),600,14400);rewardYang=std::clamp<int64_t>(config.value("reward_yang",int64_t(500000)),0,10000000);
 gmOnly=config.value("gm_only",true);availableEncounters=0;if(config.contains("available_encounters")&&config["available_encounters"].is_array())for(auto& entry:config["available_encounters"])if(entry.is_number_integer()){int n=entry.get<int>();if(n>=1&&n<=12)availableEncounters|=uint16_t(1u<<(n-1));}if(!availableEncounters)enabled=false;
 LOG_INFO("ECLIPSE_RAID_CONFIG enabled={} template={} capacity={} gm_only={} encounters={}",enabled,MapIndex,Capacity,gmOnly,availableEncounters);
}
void StatePacket(Run& r,bool force=false){auto now=get_dword_time();if(!force&&now-r.lastSend<1000)return;r.lastSend=now;int ready=0;uint64_t hp=0,maxHP=0;
 for(auto& m:r.members)if(m.ready)ready++;
 for(auto e:r.bosses)if(Valid(e)){maxHP+=Points::GetMaxHP(e);if(!CombatSystem::IsDead(e))hp+=P::GetHP(e);}
 for(auto& m:r.members){auto p=Player(m);if(!Present(r,p))continue;ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_state %u %d %d %u %d %d %llu %llu %llu %d %d %d",r.id,r.encounter,int(r.state),unsigned(r.cleared),ready,int(r.members.size()),(unsigned long long)hp,(unsigned long long)maxHP,(unsigned long long)r.barrier,r.resource,r.empowered,m.pid==r.leader?1:0);
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_mode %u %u",r.id,unsigned(r.mode));ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_available %u %u",r.id,unsigned(availableEncounters));
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_countdown %u %d %u",r.id,r.countdownEncounter,r.countdownAt&&!Due(now,r.countdownAt)?r.countdownAt-now:0);
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_root %d",r.rooted.count(m.pid)?1:0);
  if(force)for(auto& row:r.members){auto member=Player(row);if(Valid(member))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_member %u %u %s",row.pid,P::GetPacketVID(member),std::string(P::GetName(member)).c_str());}
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_objects_clear 0");
  for(auto e:r.actors)if(Valid(e)&&!CombatSystem::IsDead(e)&&g_registry.get<Actor>(e).role==2)ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_object %u %d",P::GetPacketVID(e),r.encounter);
  if(r.source)r.source->Status(p);
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_aura %d %u %d",int(m.aura),m.auraEnd&& !Due(now,m.auraEnd)?(m.auraEnd-now)/1000:0,m.stacks);
 }
}
void Return(entt::entity p){if(!Valid(p)||!P::GetDesc(p))return;ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"er_release 0");int map=P::GetQuestFlag(p,"eclipse.return_map"),x=P::GetQuestFlag(p,"eclipse.return_x"),y=P::GetQuestFlag(p,"eclipse.return_y");auto* region=SECTREE_MANAGER::instance().GetMapRegion(map);
 if(map>0&&map<10000&&!IsMap(map)&&region&&x>=region->sx&&x<region->ex&&y>=region->sy&&y<region->ey){if(M::WarpSet(p,x,y,map))return;}M::GoHome(p);
}
void DestroyActors(Run& r){r.countdownAt=0;r.countdownEncounter=0;r.source.reset();r.rooted.clear();Broadcast(r,"er_root 0");for(auto e:r.actors)Destroy(e);r.actors.clear();r.bosses.clear();r.dead.clear();r.scheduler.Reset();r.hazards.clear();r.completionPending=false;Broadcast(r,"er_clear "+std::to_string(r.id));for(auto& m:r.members){m.aura=Mechanic::Strike;m.auraEnd=m.object=0;m.stacks=0;}}
void Retire(Run& r){DestroyActors(r);r.state=State::Retiring;for(auto& m:r.members){auto p=Player(m);if(Present(r,p))Return(p);}r.changed=get_dword_time();}
bool Walk(const Run& r,Vec p){auto* tree=SECTREE_MANAGER::instance().Get(r.map,BaseX+int(p.x*100),BaseY+int(p.y*100));return tree&&!(tree->GetAttribute(BaseX+int(p.x*100),BaseY+int(p.y*100))&(ATTR_BLOCK|ATTR_OBJECT));}
void WarpCheckpoint(Run& r,entt::entity p){Vec checkpoint=r.encounter>0?Arenas[r.encounter-1].center:Arenas[0].center;checkpoint.y-=25;
 if(Present(r,p)&&Walk(r,checkpoint)){M::Sync(p,BaseX+int(checkpoint.x*100),BaseY+int(checkpoint.y*100));NetworkSyncSystem::BroadcastSyncPacket(g_registry,p);}
}
entt::entity Spawn(Run& r,uint32_t race,Vec point,int role=0,uint32_t owner=0,uint32_t sourceCreature=0){r.actors.erase(std::remove_if(r.actors.begin(),r.actors.end(),[](auto e){return !Valid(e);}),r.actors.end());if(r.actors.size()>=128||!Walk(r,point))return entt::null;
 auto* source=CMobManager::instance().Get(101);if(!source)return entt::null;
 auto e=CHARACTER_MANAGER::instance().SpawnMobEntity(101,r.map,BaseX+int(point.x*100),BaseY+int(point.y*100),0,false,0,false);if(!Valid(e))return e;
 auto proto=std::make_shared<CMob>(*source);auto& t=proto->m_table;t.dwVnum=race;t.bSize=role==0||sourceCreature==36899?MOBSIZE_BIG:MOBSIZE_MEDIUM;auto nameKey=role==0?"ER_BOSS_"+std::to_string(r.encounter):std::string(race==62019?"ER_BONE_SPIKE_NAME":race==62015?"ER_PRISON_NAME":race==62016?"ER_FOCUS_NAME":race==62018?"ER_OOZE_NAME":"ER_MINION_NAME");snprintf(t.szName,sizeof(t.szName),"%s",nameKey.c_str());
#ifdef ENABLE_MULTI_NAMES
 for(auto& localeName:t.szLocaleName)snprintf(localeName,sizeof(localeName),"%s",nameKey.c_str());
#else
 snprintf(t.szLocaleName,sizeof(t.szLocaleName),"%s",nameKey.c_str());
#endif
 t.bType=role==2?CHAR_TYPE_NPC:CHAR_TYPE_MONSTER;t.bRank=role==0?MOB_RANK_BOSS:MOB_RANK_KNIGHT;t.bBattleType=BATTLE_TYPE_MELEE;t.bEmpire=0;t.bLevel=100;t.bStr=t.bDex=t.bCon=t.bInt=100;t.wDef=80;t.dwMaxHP=uint32_t(role==0?BossHealth(int(r.members.size()),r.encounter):80000*int(r.members.size()));t.dwDamageRange[0]=500;t.dwDamageRange[1]=1100;t.dwExp=t.dwGoldMin=t.dwGoldMax=t.dwDropItemVnum=t.dwResurrectionVnum=t.dwSummonVnum=0;t.dwAIFlag=0;t.bRegenPercent=0;t.sMovingSpeed=120;t.sAttackSpeed=100;t.wAttackRange=role==0?550:200;t.fDamMultiply=1;
 if(!sourceCreature&&role==0){sourceCreature=race==62013?37972:race==62014?37973:ICC::BossCreatureIDs[r.encounter-1];}
 if(auto data=ICC::Creatures().Resolve(sourceCreature,r.mode)){t.dwMaxHP=data->health;t.dwDamageRange[0]=uint32_t(std::max(0.f,data->minimumDamage));t.dwDamageRange[1]=uint32_t(std::max(data->minimumDamage,data->maximumDamage));t.bLevel=data->sourceLevel;t.sMovingSpeed=100;if(sourceCreature==36899)t.wAttackRange=500;}
 g_registry.emplace<Actor>(e,Actor{r.id,r.encounter,role,proto,get_dword_time()+2000,0,owner,sourceCreature});P::SetProto(e,proto.get());g_registry.get_or_emplace<ecs::RaceState>(e).baseRace=race;if(role==2){g_registry.remove<ecs::TagMonster>(e);g_registry.get_or_emplace<ecs::TagNPC>(e);}else{g_registry.remove<ecs::TagNPC>(e);g_registry.get_or_emplace<ecs::TagMonster>(e);}P::SetMaxHP(e,t.dwMaxHP);P::SetHP(e,t.dwMaxHP);M::SetNowWalking(e,false);
 if(!M::Show(e,r.map,BaseX+int(point.x*100),BaseY+int(point.y*100),0,false)){Destroy(e);return entt::null;}r.actors.push_back(e);if(role==0)r.bosses.push_back(e);return e;
}
std::vector<entt::entity> Living(Run& r){std::vector<entt::entity> list;for(auto& m:r.members){auto p=Player(m);if(Alive(r,p))list.push_back(p);}return list;}
entt::entity Pick(Run& r){auto list=Living(r);if(list.empty())return entt::null;return list[(r.nextHazard+r.resource+get_dword_time()/1000)%list.size()];}
void Hit(Run& r,entt::entity p,int percent,entt::entity source=entt::null){if(!Alive(r,p))return;if(!Valid(source)&&!r.bosses.empty())source=r.bosses.front();if(!Valid(source))return;CombatSystem::Damage(p,source,int64_t(Points::GetMaxHP(p))*std::clamp(percent,1,95)/100,DAMAGE_TYPE_SPECIAL);}
void Aura(Run& r,entt::entity p,Mechanic kind,uint32_t duration,int stacks=1){if(auto* m=Participant(r,P::GetPlayerID(p))){m->aura=kind;m->auraEnd=get_dword_time()+duration;m->stacks=stacks;}}
void Telegraph(Run& r,Mechanic kind,Vec center,float radius,int percent,uint32_t delay=3000,uint32_t life=1,Shape shape=Shape::Circle,Vec direction={1,0},float length=15,bool repeat=false){if(r.hazards.size()>=64)return;uint32_t now=get_dword_time();Hazard h;h.id=r.nextHazard++;h.kind=kind;h.center=center;h.radius=radius;h.damagePercent=percent;h.at=now+delay;h.end=h.at+life;h.next=h.at;h.shape=shape;h.direction=direction;h.length=length;h.repeat=repeat;r.hazards.push_back(h);
 std::ostringstream line;line<<"er_hazard "<<r.id<<' '<<h.id<<' '<<int(kind)<<' '<<int(shape)<<' '<<BaseX+int(center.x*100)<<' '<<BaseY+int(center.y*100)<<' '<<int(radius*100)<<' '<<int(direction.x*100)<<' '<<int(direction.y*100)<<' '<<int(length*100)<<' '<<delay<<' '<<life;Broadcast(r,line.str());}
// Fixed-capacity room spawns never use a global NPC scan or a timer per actor.
void Summon(Run& r,int count,int role=1){auto c=Arenas[r.encounter-1].center;for(int n=0;n<count;n++){float a=(n+r.resource)*2.399f;Spawn(r,role==3?62015:role==4?62018:62017,{c.x+std::sin(a)*22,c.y+std::cos(a)*22},role);}}
std::shared_ptr<SourceRuntime> CreateSource(Run& r);
void Start(Run& r,int encounter){DestroyActors(r);r.encounter=encounter;r.state=State::Combat;r.changed=r.combatAt=get_dword_time();r.phase=r.resource=r.empowered=0;r.immuneUntil=0;r.barrier=encounter==2?BossHealth(int(r.members.size()),encounter)/2:0;
 auto c=Arenas[encounter-1].center;auto e=Spawn(r,FirstRace+encounter-1,c);if(!Valid(e)){r.state=State::Waiting;Notice(r,"ER_UNAVAILABLE");return;}
 if(encounter==8){Spawn(r,62013,{c.x-13,c.y});Spawn(r,62014,{c.x+13,c.y});}
 if(encounter==10){P::SetHP(e,Points::GetMaxHP(e)*4/10);CombatSystem::SetInvincible(e,true);}
 r.source=CreateSource(r);if(!r.source)Schedule(r.scheduler,encounter,r.changed);for(auto& m:r.members)m.ready=false;Notice(r,"ER_ENGAGED");StatePacket(r,true);
}
void Complete(Run& r){int encounter=r.encounter;r.cleared=Clear(r.cleared,encounter);DestroyActors(r);r.state=RewardEligible(r.cleared)?State::Complete:State::Waiting;r.changed=get_dword_time();Notice(r,"ER_BOSS_DEFEATED");
 if(r.state==State::Complete){for(auto& m:r.members){auto p=Player(m);if(m.rewarded||!Present(r,p))continue;m.rewarded=true; // Mark before currency publication/reentrant callbacks.
   if(P::GetQuestFlag(p,"eclipse.reward_run")!=int(r.id)){P::SetQuestFlag(p,"eclipse.reward_run",int(r.id));Points::Change(p,POINT_GOLD,rewardYang,true,true);P::SetQuestFlag(p,"eclipse.completed",int(get_global_time()));}Notice(p,"ER_COMPLETE");}}
 StatePacket(r,true);
}
void Wipe(Run& r){DestroyActors(r);r.state=State::Waiting;r.changed=get_dword_time();r.barrier=0;for(auto& m:r.members)m.ready=false;Notice(r,"ER_WIPE");StatePacket(r,true);}
void Ability(Run& r,Mechanic kind){auto c=Arenas[r.encounter-1].center;auto target=Pick(r);auto now=get_dword_time();if(!Valid(target))return;Vec pos=Position(target);auto boss=r.bosses.empty()?entt::null:r.bosses.front();
 if(kind==Mechanic::Strike){
  if(r.encounter==10){for(auto e:r.actors)if(Valid(e)&&g_registry.get<Actor>(e).role==1&&!CombatSystem::IsDead(e)){P::SetHP(boss,P::GetHP(boss)>Points::GetMaxHP(boss)/200?P::GetHP(boss)-Points::GetMaxHP(boss)/200:0);if(!P::GetHP(boss)){Wipe(r);return;}}return;}
  if(Due(now,r.immuneUntil)&&Valid(boss)){entt::entity nearest=entt::null;float distance=1e6f;for(auto p:Living(r)){float d=Distance(Position(p),Position(boss));if(d<distance){distance=d;nearest=p;}}if(Valid(nearest)){if(distance>5)M::Goto(boss,P::GetX(nearest),P::GetY(nearest));else{M::SetRotationToXY(boss,P::GetX(nearest),P::GetY(nearest));M::SendMovePacket(boss,FUNC_ATTACK,0,P::GetX(boss),P::GetY(boss),0);Hit(r,nearest,r.encounter==5?8+3*r.resource:10,boss);}}}return;
 }
 if(Valid(boss))M::SendMovePacket(boss,FUNC_MOB_SKILL,0,P::GetX(boss),P::GetY(boss),0);
 switch(kind){
 case Mechanic::Flame:{Vec from=Valid(boss)?Position(boss):c;Telegraph(r,kind,from,r.encounter==11?20:1.8f,r.encounter==11?30:15,2800,8000,r.encounter==11?Shape::Cone:Shape::Line,{pos.x-from.x,pos.y-from.y},45,true);break;}
 case Mechanic::Spike:case Mechanic::IceTomb:case Mechanic::Valkyrie:{int count=std::min(kind==Mechanic::Spike?2:3,int(Living(r).size()));auto living=Living(r);for(int n=0;n<count;n++){auto p=living[(n+r.nextHazard)%living.size()];auto object=Spawn(r,62015,Position(p),3,P::GetPlayerID(p));if(Valid(object)){g_registry.get<Actor>(object).expires=now+(kind==Mechanic::Valkyrie?14000:18000);Aura(r,p,kind,kind==Mechanic::Valkyrie?14000:18000);Participant(r,P::GetPlayerID(p))->object=P::GetPacketVID(object);}}if(kind==Mechanic::IceTomb)r.immuneUntil=now+13000;Notice(r,"ER_RESCUE");break;}
 case Mechanic::Storm:r.immuneUntil=now+14000;for(auto p:Living(r))Telegraph(r,kind,Position(p),7,22,4000,9000,Shape::Circle,{1,0},0,true);Notice(r,"ER_SPREAD");break;
 case Mechanic::Cultist:Summon(r,std::min(6,2+int(r.members.size())/6));break;
 case Mechanic::Control:Aura(r,target,kind,10000);Notice(r,"ER_CONTROL");break;
 case Mechanic::Siege:Telegraph(r,kind,pos,6,30,4000);Summon(r,2);Notice(r,"ER_SIEGE");break;
 case Mechanic::Mark:Aura(r,target,kind,22000);r.resource=std::min(100,r.resource+25);if(r.resource>=100){Telegraph(r,kind,c,38,45,4000);r.resource=0;}break;
 case Mechanic::Gas:{r.resource++;if(r.resource>=4){for(auto p:Living(r)){auto* m=Participant(r,P::GetPlayerID(p));Hit(r,p,std::max(10,70-m->stacks*20));m->stacks=0;}r.resource=0;Notice(r,"ER_GAS_RELEASE");}else for(auto p:Living(r))Hit(r,p,6+r.resource*2);break;}
 case Mechanic::Spore:Aura(r,target,kind,7000);Notice(r,"ER_SPORE");break;
 case Mechanic::Infection:Aura(r,target,kind,12000);Notice(r,"ER_CLEANSE");break;
 case Mechanic::Slime:Summon(r,2,4);break;
 case Mechanic::Experiment:Summon(r,2,4);Telegraph(r,kind,pos,6,18,3500,14000,Shape::Circle,{1,0},0,true);break;
 case Mechanic::Mutation:Spawn(r,62016,{c.x-20,c.y-14},2);Notice(r,"ER_MUTATION");break;
 case Mechanic::Invocation:r.empowered=(r.empowered+1)%3;Notice(r,"ER_INVOCATION");break;
 case Mechanic::Kinetic:Spawn(r,62016,pos,2);Telegraph(r,kind,pos,8,32,10000);Notice(r,"ER_KINETIC");break;
 case Mechanic::Bite:Aura(r,target,kind,30000);Notice(r,"ER_BITE");break;
 case Mechanic::Pact:{auto living=Living(r);for(size_t n=0;n<std::min<size_t>(3,living.size());n++)Aura(r,living[n],kind,14000);Notice(r,"ER_PACT");break;}
 case Mechanic::Portal:Spawn(r,62016,{c.x-18,c.y-13},2);Spawn(r,62016,{c.x+18,c.y-13},2);Notice(r,"ER_HEAL_PORTAL");break;
 case Mechanic::FrostBomb:{int n=0;for(auto p:Living(r)){Telegraph(r,kind,Position(p),35,55,5000+n*800);if(++n==3)break;}Notice(r,"ER_SHELTER");break;}
 case Mechanic::Defile:Telegraph(r,kind,pos,4,14,3000,20000,Shape::Circle,{1,0},0,true);Notice(r,"ER_DEFILE");break;
 case Mechanic::Soul:Aura(r,target,kind,15000);Summon(r,2);Notice(r,"ER_SOUL");break;
 default:break;
 }
}
void Mechanics(Run& r,uint32_t now){
 for(auto it=r.hazards.begin();it!=r.hazards.end();){auto& h=*it;if(Due(now,h.at)&&Due(now,h.next)){
   if(h.kind==Mechanic::Defile)h.radius=std::min(22.f,h.radius+.65f);
   for(auto p:Living(r))if(Contains(h,Position(p))){bool sheltered=false;if(h.kind==Mechanic::FrostBomb){auto from=h.center,to=Position(p);for(auto e:r.actors)if(Valid(e)&&!CombatSystem::IsDead(e)&&g_registry.get<Actor>(e).role==3){auto at=Position(e);Hazard ray;ray.shape=Shape::Line;ray.center=from;ray.direction={to.x-from.x,to.y-from.y};ray.length=Distance(from,to);ray.radius=2;if(Contains(ray,at)&&Distance(from,at)<Distance(from,to)){sheltered=true;break;}}}if(!sheltered)Hit(r,p,h.damagePercent);}
   h.next=now+(h.repeat?1000:600000);
  }if(Due(now,h.end))it=r.hazards.erase(it);else ++it;
 }
 auto living=Living(r);bool pact=true;Vec pactAt;int pactCount=0;for(auto& m:r.members)if(m.aura==Mechanic::Pact&&m.auraEnd){auto p=Player(m);if(!Alive(r,p)){pact=false;continue;}if(!pactCount)pactAt=Position(p);else if(Distance(pactAt,Position(p))>4)pact=false;pactCount++;}if(pact&&pactCount){for(auto& m:r.members)if(m.aura==Mechanic::Pact){m.aura=Mechanic::Strike;m.auraEnd=0;}Notice(r,"ER_PACT_CLEARED");}
 for(auto& m:r.members){auto p=Player(m);if(!Alive(r,p)||!m.auraEnd)continue;
  if(m.aura==Mechanic::Spike||m.aura==Mechanic::IceTomb||m.aura==Mechanic::Valkyrie){for(auto e:r.actors)if(Valid(e)&&P::GetPacketVID(e)==m.object&&!CombatSystem::IsDead(e)){if(Distance(Position(p),Position(e))>1){M::Sync(p,P::GetX(e),P::GetY(e));NetworkSyncSystem::BroadcastSyncPacket(g_registry,p);}break;}}
  if(Due(now,m.auraEnd)){auto aura=m.aura;m.aura=Mechanic::Strike;m.auraEnd=0;if(aura==Mechanic::Spore){for(auto other:living)if(Distance(Position(p),Position(other))<=8)Participant(r,P::GetPlayerID(other))->stacks=std::min(3,Participant(r,P::GetPlayerID(other))->stacks+1);}else if(aura==Mechanic::Infection)Telegraph(r,aura,Position(p),5,10,1000,15000,Shape::Circle,{1,0},0,true);else if(aura==Mechanic::Bite){Aura(r,p,Mechanic::Control,15000);Notice(p,"ER_BITE_FAILED");}else if(aura==Mechanic::Pact||aura==Mechanic::Valkyrie||aura==Mechanic::Soul)Hit(r,p,70);}
 }
 for(auto e:r.actors){if(!Valid(e)||CombatSystem::IsDead(e))continue;auto& a=g_registry.get<Actor>(e);if(a.role==0||a.role==2||a.role==3)continue;if(!Due(now,a.next))continue;a.next=now+2500;auto target=Pick(r);if(!Valid(target))continue;
  float d=Distance(Position(e),Position(target));if(d>2)M::Goto(e,P::GetX(target),P::GetY(target));else{M::SendMovePacket(e,FUNC_ATTACK,0,P::GetX(e),P::GetY(e),0);Hit(r,target,5,e);}
 }
 // Merge two living ooze adds once; their owner components carry no global timers.
 for(size_t i=0;i<r.actors.size();i++){auto a=r.actors[i];if(!Valid(a)||CombatSystem::IsDead(a)||g_registry.get<Actor>(a).role!=4)continue;for(size_t j=i+1;j<r.actors.size();j++){auto b=r.actors[j];if(Valid(b)&&!CombatSystem::IsDead(b)&&g_registry.get<Actor>(b).role==4&&Distance(Position(a),Position(b))<4){Telegraph(r,Mechanic::Slime,Position(a),13,35,4500);g_registry.get<Actor>(a).role=1;Destroy(b);break;}}}
 if(r.encounter==12&&!r.bosses.empty()&&Valid(r.bosses.front())){int hp=P::GetHPPct(r.bosses.front());if((r.phase==0&&hp<=70)||(r.phase==1&&hp<=40)){r.phase++;r.immuneUntil=now+12000;Telegraph(r,Mechanic::Quake,Arenas[11].center,46,35,6500,1,Shape::Ring);r.hazards.back().inner=24;Summon(r,3);Notice(r,"ER_INTERMISSION");}}
}
#include "EclipseMarrowgarRuntime.hpp"
#include "EclipseFestergutRuntime.hpp"
#include "EclipseRotfaceRuntime.hpp"
std::shared_ptr<SourceRuntime> CreateSource(Run& r){if(r.encounter==1)return std::make_shared<MarrowgarRuntime>(r);if(r.encounter==5)return std::make_shared<FestergutRuntime>(r);if(r.encounter==6)return std::make_shared<RotfaceRuntime>(r);return {};}
}
bool MovementLocked(entt::entity player){if(!Valid(player)||!P::IsPC(player)||!IsMap(P::GetMapIndex(player)))return false;auto* r=For(player);return r&&Present(*r,player)&&r->state==State::Combat&&r->rooted.count(P::GetPlayerID(player));}
int64_t FilterHealing(entt::entity player,int64_t amount){if(amount<=0||!Valid(player)||!P::IsPC(player)||!IsMap(P::GetMapIndex(player)))return amount;auto r=For(player);return r&&Present(*r,player)&&r->source?r->source->Healing(player,amount):amount;}
void Dispel(entt::entity player){if(!Valid(player)||!P::IsPC(player)||!IsMap(P::GetMapIndex(player)))return;if(auto r=For(player))if(Present(*r,player)&&r->source)r->source->Dispel(player);}
float MovementSpeed(entt::entity actor){if(!Valid(actor))return 0;auto a=g_registry.try_get<Actor>(actor);if(!a||!a->sourceCreature)return 0;auto r=runs.find(a->run);if(r==runs.end())return 0;auto data=ICC::Creatures().Resolve(a->sourceCreature,r->second.mode);return data?data->runSpeed*100.f:0;}
const CMob* Prototype(entt::entity e){if(Valid(e))if(auto* a=g_registry.try_get<Actor>(e))return a->prototype.get();return nullptr;}
bool AllowCombat(entt::entity attacker,entt::entity victim){if(MovementLocked(attacker))return false;auto* a=Valid(attacker)?g_registry.try_get<Actor>(attacker):nullptr;auto* v=Valid(victim)?g_registry.try_get<Actor>(victim):nullptr;if(!a&&!v)return true;auto* r=Of(a?attacker:victim);if(!r||r->state!=State::Combat)return false;
 if(a&&v)return a->run==v->run&&r->encounter==10&&v->role==0&&a->role!=0;
 auto pc=a?victim:attacker;if(!Alive(*r,pc)||!Participant(*r,P::GetPlayerID(pc)))return false;if(v&&(v->role==2||(r->encounter==10&&v->role==0)))return false;auto* m=Participant(*r,P::GetPlayerID(pc));if(!a&&(m->aura==Mechanic::Control||m->aura==Mechanic::IceTomb||m->aura==Mechanic::Spike))return false;return true;
}
int64_t FilterDamage(entt::entity attacker,entt::entity victim,int64_t amount){if(amount<=0)return amount;auto* a=Valid(victim)?g_registry.try_get<Actor>(victim):nullptr;if(!a)return amount;auto* r=Of(victim);if(!r)return 0;if(r->source)amount=r->source->TargetDamage(attacker,victim,amount);if(a->role!=0)return amount;if(!Due(get_dword_time(),r->immuneUntil))return 0;if(r->encounter==8){auto it=std::find(r->bosses.begin(),r->bosses.end(),victim);if(it-r->bosses.begin()!=r->empowered)return 0;}
 if(r->source&&Valid(attacker)){amount=r->source->Outgoing(attacker,amount);r->source->Attacked(attacker,amount);}
 if(r->barrier){auto absorbed=std::min<uint64_t>(r->barrier,uint64_t(amount));r->barrier-=absorbed;amount-=int64_t(absorbed);}
 if(Valid(attacker))if(auto* m=Participant(*r,P::GetPlayerID(attacker)))if(m->aura==Mechanic::Bite)amount+=amount/2;
 return amount;
}
void OnDeath(entt::entity victim,entt::entity){if(!Valid(victim))return;if(auto* run=Of(victim))if(run->source)run->source->Died(victim);auto* a=g_registry.try_get<Actor>(victim);if(!a)return;auto* r=Of(victim);if(!r)return;if(a->role==0){if(r->encounter==8)r->completionPending=true;r->dead.insert(P::GetPacketVID(victim));if(r->dead.size()>=r->bosses.size())r->completionPending=true;}if(a->role==3&&a->owner)if(auto* m=Participant(*r,a->owner)){m->aura=Mechanic::Strike;m->auraEnd=m->object=0;}}
void Command(entt::entity p,const char* arguments){if(!loaded)Load();if(!Valid(p)||!P::IsPC(p)||!P::GetDesc(p))return;std::istringstream input(arguments?arguments:"");std::string action;input>>action;auto* r=For(p);auto now=get_dword_time();
 if(action=="enter"){
  if(!r&&gmOnly&&P::GetGMLevel(p)<GM_LOW_WIZARD){Notice(p,"ER_GM_TEST_ONLY");return;}
  std::string modeName;input>>modeName;auto mode=ICC::ParseMode(modeName.empty()?"10_normal":modeName);if(!mode){Notice(p,"ER_MODE_INVALID");return;}
  if(r){if(r->state!=State::Retiring&&P::GetMapIndex(p)!=r->map)M::WarpSet(p,BaseX+51200,BaseY+21000,r->map);else StatePacket(*r,true);return;}if(!enabled||runs.size()>=size_t(maximumRuns)||P::GetMapIndex(p)>=10000){Notice(p,"ER_UNAVAILABLE");return;}Run run;run.mode=*mode;run.id=sequence++;while(runs.count(run.id)||run.id==0)run.id=sequence++;run.leader=P::GetPlayerID(p);run.started=run.changed=now;
  auto party=PartySystem::GetCharacterParty(p);auto* state=PartySystem::Find(party);if(state){if(state->leaderPID!=run.leader){Notice(p,"ER_LEADER_ONLY");return;}if(state->members.size()>ICC::PlayerLimit(run.mode)){Notice(p,"ER_GROUP_SIZE");return;}for(auto& [pid,row]:state->members){auto member=P::FindByPlayerID(pid);if(!Valid(member)||!P::GetDesc(member)||P::GetMapIndex(member)!=P::GetMapIndex(p)||CombatSystem::IsDead(member)||Distance({P::GetX(member)*.01f,P::GetY(member)*.01f},{P::GetX(p)*.01f,P::GetY(p)*.01f})>30||For(member)){Notice(p,"ER_GROUP_PRESENT");return;}run.members.push_back(Member{pid});}}
  else run.members.push_back(Member{run.leader});if(int(run.members.size())<minimumMembers&&P::GetGMLevel(p)<GM_LOW_WIZARD){Notice(p,"ER_GROUP_SIZE");return;}
  if(!SECTREE_MANAGER::instance().GetMap(MapIndex)){for(auto& member:run.members){auto player=Player(member);if(!IsMap(P::GetMapIndex(player))){P::SetQuestFlag(player,"eclipse.return_map",P::GetMapIndex(player));P::SetQuestFlag(player,"eclipse.return_x",P::GetX(player));P::SetQuestFlag(player,"eclipse.return_y",P::GetY(player));}Notice(player,"ER_TRANSFER_ENTRY");if(!M::WarpSet(player,BaseX+51200,BaseY+21000,MapIndex))Notice(player,"ER_UNAVAILABLE");}return;}
  run.map=SECTREE_MANAGER::instance().CreatePrivateMap(MapIndex);if(!Private(run.map)){Notice(p,"ER_UNAVAILABLE");return;}auto id=run.id;runs.emplace(id,std::move(run));r=&runs.at(id);
  for(size_t i=0;i<r->members.size();i++){auto member=Player(r->members[i]);if(!IsMap(P::GetMapIndex(member))){P::SetQuestFlag(member,"eclipse.return_map",P::GetMapIndex(member));P::SetQuestFlag(member,"eclipse.return_x",P::GetX(member));P::SetQuestFlag(member,"eclipse.return_y",P::GetY(member));}M::SaveExitLocation(member);if(!M::WarpSet(member,BaseX+51200+int(i%5)*150,BaseY+21000+int(i/5)*150,r->map)){Retire(*r);Notice(p,"ER_UNAVAILABLE");break;}}return;
 }
 if(!r){Notice(p,"ER_NO_INSTANCE");return;}auto* m=Participant(*r,P::GetPlayerID(p));if(action=="rejoin"){if(r->state!=State::Retiring&&P::GetMapIndex(p)!=r->map)M::WarpSet(p,BaseX+51200,BaseY+22000,r->map);return;}
 if(action=="leave"){Return(p);return;}if(!Present(*r,p))return;if(action=="status"){StatePacket(*r,true);return;}if(action=="ready"){m->ready=!CombatSystem::IsDead(p);StatePacket(*r,true);return;}
 if(action=="engage"){int encounter=0;input>>encounter;if(m->pid!=r->leader||r->state!=State::Waiting||!Unlocked(r->cleared,encounter)||!Cleared(availableEncounters,encounter)||Cleared(r->cleared,encounter)){Notice(p,"ER_LOCKED");return;}for(auto& other:r->members){auto member=Player(other);if(!other.ready||!Alive(*r,member)||Distance(Position(member),Arenas[encounter-1].center)>55){Notice(p,"ER_NOT_READY");return;}}if(!r->countdownAt){r->countdownEncounter=encounter;r->countdownAt=now+BeginVoiceMilliseconds;}StatePacket(*r,true);return;}
 if(action=="checkpoint"&&r->state==State::Waiting){if(!CombatSystem::IsDead(p))WarpCheckpoint(*r,p);return;}
 if(action=="reset"&&P::GetGMLevel(p)>=GM_LOW_WIZARD){if(r->encounter>=1&&r->encounter<=12)r->cleared&=uint16_t(~(1u<<(r->encounter-1)));Wipe(*r);return;}
 // GM-only diagnostic. Raid targeting treats GM characters like ordinary players.
 // Tank exclusions remain unchanged, including for solo raid tests.
 if(action=="test_spike"){if(P::GetGMLevel(p)>=GM_LOW_WIZARD&&r->state==State::Combat&&r->encounter==1&&Alive(*r,p))if(auto source=std::dynamic_pointer_cast<MarrowgarRuntime>(r->source))source->TestSpike(m->pid);return;}
 if(r->state!=State::Combat||!Alive(*r,p)||now-m->lastAction<1500)return;m->lastAction=now;
 if(action=="heal"&&r->encounter==10&&!r->bosses.empty()){auto boss=r->bosses.front();if(Valid(boss)&&Distance(Position(p),Position(boss))<35&&P::GetSP(p)>=100){Points::Change(p,POINT_SP,-100);auto hp=std::min<uint64_t>(Points::GetMaxHP(boss),P::GetHP(boss)+Points::GetMaxHP(boss)*(m->aura==Mechanic::Heal?5:1)/100);P::SetHP(boss,hp);if(hp==Points::GetMaxHP(boss))r->completionPending=true;}return;}
 if(action=="cleanse"&&r->source){uint32_t pid=0;input>>pid;auto other=Participant(*r,pid);auto target=other?Player(*other):p;if(!Alive(*r,target)||Distance(Position(p),Position(target))>20||!SkillSystem::UseSkill(p,109,target))Notice(p,"ER_DISPEL_UNAVAILABLE");return;}
 if(action=="cleanse"){uint32_t pid=0;input>>pid;auto* other=Participant(*r,pid);if(!other)other=m;auto target=Player(*other);if(Alive(*r,target)&&Distance(Position(p),Position(target))<=20&&other->aura==Mechanic::Infection){Telegraph(*r,Mechanic::Infection,Position(target),5,10,1000,15000,Shape::Circle,{1,0},0,true);other->aura=Mechanic::Strike;other->auraEnd=0;}return;}
 if(action=="bite"&&m->aura==Mechanic::Bite){uint32_t pid=0;input>>pid;auto* other=Participant(*r,pid);if(other&&other!=m&&other->aura!=Mechanic::Bite){auto target=Player(*other);if(Alive(*r,target)&&Distance(Position(p),Position(target))<=5){Aura(*r,target,Mechanic::Bite,30000);m->auraEnd=now+30000;Notice(p,"ER_BITE_TRANSFERRED");}}return;}
 if(action=="interact"){uint32_t vid=0;input>>vid;for(auto e:r->actors)if(Valid(e)&&P::GetPacketVID(e)==vid&&g_registry.get<Actor>(e).role==2&&Distance(Position(p),Position(e))<6){if(r->encounter==10)Aura(*r,p,Mechanic::Heal,25000,3);else if(r->encounter==7)Aura(*r,p,Mechanic::Mutation,30000);else if(r->encounter==8){for(auto& h:r->hazards)if(h.kind==Mechanic::Kinetic&&Distance(h.center,Position(e))<2)h.end=now;}Destroy(e);return;}return;}
 if(action=="mutate"&&m->aura==Mechanic::Mutation){for(auto& h:r->hazards)if(Distance(Position(p),h.center)<10)h.end=now;return;}
 if(action=="repair"&&r->encounter==3&&P::GetSP(p)>=100){Points::Change(p,POINT_SP,-100);r->resource=std::min(100,r->resource+5);if(r->resource>=100)r->completionPending=true;return;}
}
void Tick(){if(!loaded)Load();if(!enabled)return;auto now=get_dword_time();if(now-previous<250)return;previous=now;
 for(auto p:g_registry.view<ecs::PlayerID>())if(P::GetDesc(p)&&Private(P::GetMapIndex(p))){auto* r=For(p);if(!r||r->map!=P::GetMapIndex(p)||r->state==State::Retiring)Return(p);}
 for(auto it=runs.begin();it!=runs.end();){auto& r=it->second;bool erase=false;
  if(now-r.started>uint32_t(durationSeconds)*1000&&r.state!=State::Retiring){Notice(r,"ER_TIMEOUT");Retire(r);}
  if(r.state==State::Retiring){bool occupied=false;for(auto p:g_registry.view<ecs::PlayerID>())if(P::GetMapIndex(p)==r.map){occupied=true;if(P::GetDesc(p)&&now-r.changed>3000)Return(p);}if(!occupied){SECTREE_MANAGER::instance().DestroyPrivateMap(r.map);erase=true;}}
  else{int online=0,alive=0;for(auto& m:r.members){auto p=Player(m);if(Present(r,p)){online++;if(!CombatSystem::IsDead(p))alive++;m.missing=0;}else if(!m.missing)m.missing=now;}
   if(!online){if(!r.emptyAt)r.emptyAt=now;else if(now-r.emptyAt>120000)Retire(r);}else r.emptyAt=0;
   if(r.state==State::Waiting&&r.countdownAt){
    bool ready=true;for(auto& member:r.members){auto p=Player(member);if(!member.ready||!Alive(r,p)||Distance(Position(p),Arenas[r.countdownEncounter-1].center)>55){ready=false;break;}}
    if(!ready){r.countdownAt=0;r.countdownEncounter=0;Notice(r,"ER_NOT_READY");StatePacket(r,true);}
    else if(Due(now,r.countdownAt)){int encounter=r.countdownEncounter;Start(r,encounter);}
   }
   if(r.state==State::Combat){if(r.completionPending)Complete(r);else if(!alive)Wipe(r);else if(!r.source&&now-r.combatAt>720000){Notice(r,"ER_ENRAGE");for(auto p:Living(r))Hit(r,p,95);}else{
     bool inArena=false;for(auto p:Living(r))if(Distance(Position(p),Arenas[r.encounter-1].center)<Arenas[r.encounter-1].radius+8)inArena=true;if(!inArena)Wipe(r);else if(r.source){auto source=r.source;if(!source->Update(now))Wipe(r);}else{r.scheduler.Poll(now,[&](Mechanic kind){if(r.state==State::Combat)Ability(r,kind);});if(r.state==State::Combat)Mechanics(r,now);}
    }}
   // Locked encounter rooms cannot be skipped by passing a client-side door.
   for(auto& m:r.members){auto p=Player(m);if(!Alive(r,p))continue;for(int n=1;n<=12;n++)if(!Unlocked(r.cleared,n)&&Distance(Position(p),Arenas[n-1].center)<Arenas[n-1].radius){WarpCheckpoint(r,p);Notice(p,"ER_LOCKED");break;}}
   StatePacket(r);
  }
  if(erase)it=runs.erase(it);else ++it;
 }
}
}
ACMD(do_eclipseraid){EclipseRaid::Command(character,argument);}
