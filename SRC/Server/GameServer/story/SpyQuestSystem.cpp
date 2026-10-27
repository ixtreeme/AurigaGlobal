#include "../core/stdafx.h"
#include "SpyQuestSystem.hpp"
#include "SpyQuestRules.hpp"
#include "SpyQuestNavigation.hpp"
#include "../ecs/services/EntityNetworkDispatch.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../core/config.h"
#include "../combat/battle.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/VisibilitySystem.hpp"
#include "../entity/char_manager.h"
#include "../entity/mob_manager.h"
#include "../world/sectree_manager.h"
#include "../core/locale_service.h"
#include "../core/JsonFile.h"
#include "../core/utils.h"
#include "../core/packet.h"
#include "../core/vector.h"
#include "../command/gm.h"
#include "../command/cmd.h"
#include <sstream>
#include <random>
namespace SpyQuest {
namespace {
namespace P=ecs::PlayerRuntime;namespace M=ecs::MovementSystem;
enum class Phase {Offer,Planning,Following,Meeting,Cinematic,Combat,Return};
struct Run {
 uint32_t pid=0,nonce=0;int map=0;entt::entity giver=entt::null,guard=entt::null,balso=entt::null;
 Phase phase=Phase::Offer;int64_t started=0,changed=0,farSince=0,lastSent=0,followGraceUntil=0;int suspicion=0;size_t waypoint=0,escapeWaypoint=0;
 std::unique_ptr<RouteSearch> search;std::vector<Cell> route,goals;Cell meeting,start;size_t goalIndex=0;
 std::unique_ptr<RouteSearch> approach;std::vector<Cell> approachRoute;size_t approachPoint=0;int sceneStage=0,ability=0,pendingAbility=-1;bool aura=false,knocked=false;int64_t abilityAt=0;uint32_t hitAt=0,busyUntil=0;
};
std::map<uint32_t,Run> runs;bool loaded=false,enabled=false;uint32_t clock=0;Navigation navigation;
int64_t Now(){return get_global_time();}
entt::entity Player(Run& r){return P::FindByPlayerID(r.pid);}
void Notice(entt::entity p,const char* key){ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"spy_notice %s",key);}
void Load(){loaded=true;nlohmann::json d;
 if(JsonFile::Load(LocaleService_GetBasePath()+"/spy_quest.json",d))enabled=d.value("schema_version",0)==1&&d.value("enabled",false);
 if(enabled&&!navigation.Load(LocaleService_GetBasePath()+"/spy_quest.nav")){enabled=false;LOG_ERROR("SPY_QUEST_NAV missing or invalid; quest disabled");}
 LOG_INFO("SPY_QUEST_CONFIG enabled={}",enabled);
}
Cell Position(entt::entity e){return {P::GetX(e)/100,P::GetY(e)/100};}
Cell Local(int map,Cell c){const auto* r=SECTREE_MANAGER::instance().GetMapRegion(map);return r?Cell{c.x-r->sx/100,c.y-r->sy/100}:Cell{-1,-1};}
bool Step(int map,Cell a,Cell b){return navigation.Step(Local(map,a),Local(map,b));}
bool Walk(int map,Cell c,bool fight=false){
 if(!navigation.Walk(Local(map,c)))return false;
 int x=c.x*100+50,y=c.y*100+50;auto* s=SECTREE_MANAGER::instance().Get(map,x,y);
 if(!s||(s->GetAttribute(x,y)&(ATTR_BLOCK|ATTR_OBJECT|(fight?ATTR_BANPK:0))))return false;
 // Sample the player's footprint too, not merely the NPC's centre point.
 for(auto d:{Cell{-25,0},Cell{25,0},Cell{0,-25},Cell{0,25}})if(s->GetAttribute(x+d.x,y+d.y)&(ATTR_BLOCK|ATTR_OBJECT))return false;
 return true;
}
bool Line(int map,Cell a,Cell b){int steps=std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));
 Cell previous=a;for(int i=0;i<=steps;i++){Cell c{a.x+(b.x-a.x)*i/std::max(1,steps),a.y+(b.y-a.y)*i/std::max(1,steps)};if(!Walk(map,c)||!Step(map,previous,c))return false;previous=c;}return true;}
void State(Run& r){auto p=Player(r);if(!g_registry.valid(p)||!P::GetDesc(p))return;r.lastSent=Now();
 ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"spy_state %u %d %u %u %d",r.nonce,static_cast<int>(r.phase),g_registry.valid(r.guard)?P::GetPacketVID(r.guard):0,g_registry.valid(r.balso)?P::GetPacketVID(r.balso):0,r.suspicion);
}
void Destroy(entt::entity e){if(g_registry.valid(e))CHARACTER_MANAGER::instance().DestroyCharacter(e);}
void Pose(Run& r,entt::entity p,int x,int y,int kind){M::Sync(p,x,y);ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"spy_pose %u %d %d %d",r.nonce,x,y,kind);NetworkSyncSystem::BroadcastSyncPacket(g_registry,p);}
void Ability(Run& r,const char* name){ecs::ChatSystem::Send(Player(r),CHAT_TYPE_COMMAND,"spy_ability %u %u %s",r.nonce,P::GetPacketVID(r.guard),name);}
bool BeginScene(Run& r,entt::entity p){
 Cell start=Position(p),goal{r.meeting.x-2,r.meeting.y};
 r.approach=std::make_unique<RouteSearch>(start,goal,[map=r.map](Cell c){return Walk(map,c);},[map=r.map](Cell a,Cell b){return Step(map,a,b);});
 M::Stop(p);g_registry.get_or_emplace<CinematicLock>(p);r.phase=Phase::Cinematic;r.changed=Now();r.sceneStage=0;State(r);return true;
}
void Clean(Run& r){auto p=Player(r);if(g_registry.valid(p)){g_registry.remove<CinematicLock>(p);ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"spy_end %u",r.nonce);}Destroy(r.guard);Destroy(r.balso);r.guard=r.balso=entt::null;}
entt::entity Spawn(Run& r,uint32_t race,Cell c,uint8_t role){
 auto e=CHARACTER_MANAGER::instance().SpawnMobEntity(race,r.map,c.x*100+50,c.y*100+50,0,false,0,false);
 if(!g_registry.valid(e))return entt::null;
 g_registry.emplace<Actor>(e,Actor{r.pid,role,false,{}});
 M::SetNowWalking(e,true);
 if(!M::Show(e,r.map,c.x*100+50,c.y*100+50,0,false)){Destroy(e);return entt::null;}return e;
}
bool Plan(Run& r,entt::entity p){
 const auto* region=SECTREE_MANAGER::instance().GetMapRegion(r.map);if(!region||region->strMapName!="metin2_map_main_razor93")return false;
 Cell start=Position(p);Cell low{region->sx/100+12,region->sy/100+12},high{region->ex/100-12,region->ey/100-12};
 std::vector<Cell> goals;
 // Prefer the closest map edge, with several reachable meeting spots along it.
 for(int inset:{0,15,30,50})for(int offset:{0,30,-30,60,-60,100,-100}){
  goals.push_back({low.x+inset,std::clamp(start.y+offset,low.y,high.y)});goals.push_back({high.x-inset,std::clamp(start.y+offset,low.y,high.y)});
  goals.push_back({std::clamp(start.x+offset,low.x,high.x),low.y+inset});goals.push_back({std::clamp(start.x+offset,low.x,high.x),high.y-inset});
 }
 std::sort(goals.begin(),goals.end(),[&](Cell a,Cell b){return Distance(start,a)<Distance(start,b);});
 for(Cell goal:goals){if(Distance(start,goal)<40||!Walk(r.map,goal,true))continue;
  bool clear=true;for(int x=-3;x<=3;x++)for(int y=-3;y<=3;y++)if(!Walk(r.map,{goal.x+x,goal.y+y},true))clear=false;
  if(clear)r.goals.push_back(goal);
 }if(r.goals.empty())return false;
 r.start=start;r.meeting=r.goals.front();r.search=std::make_unique<RouteSearch>(start,r.meeting,[map=r.map](Cell c){return Walk(map,c);},[map=r.map](Cell a,Cell b){return Step(map,a,b);});
 r.phase=Phase::Planning;r.changed=Now();return true;
}
void Fight(Run& r){auto p=Player(r);g_registry.remove<CinematicLock>(p);r.phase=Phase::Combat;r.changed=Now();
 auto& a=g_registry.get<Actor>(r.guard);a.prototype=std::make_shared<CMob>(*CMobManager::instance().Get(20354));
 auto& t=a.prototype->m_table;t.bType=CHAR_TYPE_MONSTER;t.bBattleType=BATTLE_TYPE_MELEE;t.bEmpire=0;t.bLevel=std::clamp<int>(ecs::PointSystem::GetLevel(p),5,150);
 t.dwMaxHP=std::clamp<uint64_t>(ecs::PointSystem::GetMaxHP(p)*3,1000,500000);t.dwDamageRange[0]=50+t.bLevel*2;t.dwDamageRange[1]=80+t.bLevel*3;
 t.dwExp=t.dwGoldMin=t.dwGoldMax=t.dwDropItemVnum=t.dwResurrectionVnum=t.dwSummonVnum=0;t.dwAIFlag=0;t.wDef=t.bLevel;
 t.bStr=t.bLevel;t.bDex=t.bLevel;t.bCon=t.bLevel;t.fDamMultiply=1;t.sAttackSpeed=100;t.sMovingSpeed=100;t.wAttackRange=180;t.bRegenPercent=0;t.bRegenCycle=0;
 P::SetProto(r.guard,a.prototype.get());g_registry.remove<ecs::TagNPC>(r.guard);g_registry.get_or_emplace<ecs::TagMonster>(r.guard);a.fighting=true;
 M::SetNowWalking(r.guard,false);r.abilityAt=Now()+4;
 // Reencode(viewer) refreshes what that entity sees, not its observers. Send
 // the hostile type to the owner explicitly; otherwise it stays an NPC client-side.
 ecs::EntityNetworkDispatch::SendRemove(g_registry,r.guard,p);ecs::EntityNetworkDispatch::SendInsert(g_registry,r.guard,p);
 CombatSystem::SetTarget(p,r.guard);State(r);
}
}
bool Click(entt::entity p,entt::entity npc){
 if(!loaded)Load();if(!enabled||!g_registry.valid(npc)||P::GetRaceNum(npc)!=9006)return false;
 if(!P::IsPC(p)||CombatSystem::IsDead(p)||P::GetMapIndex(p)!=P::GetMapIndex(npc)||Distance(Position(p),Position(npc))>5)return true;
 auto* region=SECTREE_MANAGER::instance().GetMapRegion(P::GetMapIndex(p));if(!region||region->strMapName!="metin2_map_main_razor93"){Notice(p,"SPY_WRONG_MAP");return true;}
 if(P::GetQuestFlag(p,"spy_exposure.status")==3){Notice(p,"SPY_ALREADY_DONE");return true;}
 auto pid=P::GetPlayerID(p);auto found=runs.find(pid);if(found!=runs.end()&&found->second.phase!=Phase::Offer&&found->second.phase!=Phase::Return){State(found->second);return true;}
 if(runs.size()>=16&&found==runs.end()){Notice(p,"SPY_BUSY");return true;}
 auto& r=runs[pid];if(found!=runs.end())Clean(r);r=Run{};r.pid=pid;r.map=P::GetMapIndex(p);r.giver=npc;r.started=r.changed=Now();
 r.nonce=std::random_device{}();if(!r.nonce)r.nonce=1;
 ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"spy_offer %u %d",r.nonce,P::GetQuestFlag(p,"spy_exposure.status")==2?1:0);return true;
}
void Command(entt::entity p,const char* args){
 if(!loaded)Load();if(!enabled||!g_registry.valid(p)||!P::IsPC(p))return;
 std::istringstream request(args?args:"");std::string first;request>>first;
 if(first=="reset"&&P::GetGMLevel(p)>=GM_LOW_WIZARD){auto found=runs.find(P::GetPlayerID(p));if(found!=runs.end()){Clean(found->second);runs.erase(found);}P::SetQuestFlag(p,"spy_exposure.status",0);Notice(p,"SPY_RETRY");return;}
 auto it=runs.find(P::GetPlayerID(p));if(it==runs.end())return;auto& r=it->second;
 std::istringstream in(args?args:"");std::string action;uint32_t nonce=0;in>>action>>nonce;if(nonce!=r.nonce)return;
 if(action=="cancel"){Clean(r);runs.erase(it);return;}
 if(action!="accept"||r.phase!=Phase::Offer||Now()-r.changed>60)return;
 if(!g_registry.valid(r.giver)||P::GetMapIndex(p)!=r.map||Distance(Position(p),Position(r.giver))>5||CombatSystem::IsDead(p))return;
 if(P::GetQuestFlag(p,"spy_exposure.status")==2){P::SetQuestFlag(p,"spy_exposure.status",3);Notice(p,"SPY_COMPLETE");Clean(r);runs.erase(it);return;}
 if(MountSystem::GetMountVnum(p)){Notice(p,"SPY_DISMOUNT");return;}
 if(!Plan(r,p)){Notice(p,"SPY_NO_ROUTE");Clean(r);runs.erase(it);return;}
 r.guard=Spawn(r,20354,r.start,0);if(!g_registry.valid(r.guard)){Clean(r);runs.erase(it);return;}
 P::SetQuestFlag(p,"spy_exposure.status",1);Notice(p,"SPY_PLANNING");State(r);
}
void Tick(){
 Hooks::death=&OnDeath;
 if(!loaded)Load();if(!enabled)return;uint32_t now=get_dword_time();if(now-clock<200)return;clock=now;
 const size_t routeBudget=std::clamp<size_t>(4096/std::max<size_t>(1,runs.size()),128,2048);
 for(auto it=runs.begin();it!=runs.end();){auto& r=it->second;auto p=Player(r);bool remove=false;
  if(!g_registry.valid(p)||!P::GetDesc(p)||P::GetMapIndex(p)!=r.map||CombatSystem::IsDead(p)||Now()-r.started>1200)remove=true;
  else if(r.phase==Phase::Offer){if(Now()-r.changed>60)remove=true;}
  else if(r.phase==Phase::Planning){
   auto result=r.search->Step(routeBudget);if(Now()-r.changed>120){Notice(p,"SPY_NO_ROUTE");remove=true;}
   else if(result==RouteSearch::Result::Failed){
    if(++r.goalIndex<r.goals.size()){r.meeting=r.goals[r.goalIndex];r.search=std::make_unique<RouteSearch>(r.start,r.meeting,[map=r.map](Cell c){return Walk(map,c);},[map=r.map](Cell a,Cell b){return Step(map,a,b);});}
    else{Notice(p,"SPY_NO_ROUTE");remove=true;}}
   else if(result==RouteSearch::Result::Found){r.route=r.search->route;r.search.reset();r.escapeWaypoint=r.route.size()>1?r.route.size()-2:0;
    if(Distance(Position(p),r.route.front())>25){Notice(p,"SPY_NO_ROUTE");remove=true;}
    if(!g_registry.valid(r.guard))remove=true;else if(!remove){r.phase=Phase::Following;r.waypoint=std::min<size_t>(1,r.route.size()-1);r.changed=Now();r.followGraceUntil=Now()+25;Notice(p,"SPY_HEADSTART");State(r);}}
  }else if(r.phase==Phase::Following){
   if(!g_registry.valid(r.guard))remove=true;else{
    int distance=Distance(Position(p),Position(r.guard))*100;
    if(distance>8500){if(!r.farSince)r.farSince=Now();if(Now()-r.farSince>25){Notice(p,"SPY_LOST");remove=true;}}else r.farSince=0;
    if(Now()!=r.changed){r.changed=Now();float angle=GetDegreeFromPositionXY(P::GetX(r.guard),P::GetY(r.guard),P::GetX(p),P::GetY(p));
     bool front=std::abs(std::remainder(double(angle-P::GetRotation(r.guard)),360.0))<70;
     if(distance>=5000)r.followGraceUntil=0;
     r.suspicion=Now()<r.followGraceUntil?0:Suspicion(r.suspicion,distance,front,Line(r.map,Position(p),Position(r.guard)));State(r);}
    if(r.suspicion>=100){Notice(p,"SPY_CAUGHT");remove=true;}
    else if(r.suspicion>40){M::Stop(r.guard);M::SetRotationToXY(r.guard,P::GetX(p),P::GetY(p));}
    else if(r.waypoint<r.route.size()){
     Cell next=r.route[r.waypoint];if(std::hypot(double(P::GetX(r.guard)-next.x*100-50),double(P::GetY(r.guard)-next.y*100-50))<=25){
      r.waypoint++;if(r.waypoint+1<r.route.size()){
       Cell direction{r.route[r.waypoint].x-next.x,r.route[r.waypoint].y-next.y};size_t limit=std::min(r.route.size()-1,r.waypoint+6);
       while(r.waypoint<limit&&r.route[r.waypoint+1].x-r.route[r.waypoint].x==direction.x&&r.route[r.waypoint+1].y-r.route[r.waypoint].y==direction.y)r.waypoint++;
      }}
     else if(!Walk(r.map,next)){Notice(p,"SPY_NO_ROUTE");remove=true;}
     else {M::SetRotationToXY(r.guard,next.x*100+50,next.y*100+50);if(M::Goto(r.guard,next.x*100+50,next.y*100+50))M::SendMovePacket(r.guard,FUNC_WAIT,0,0,0,0);}
    }else{
     r.balso=Spawn(r,20020,{r.meeting.x+2,r.meeting.y},1);if(!g_registry.valid(r.balso))remove=true;
     else{r.phase=Phase::Meeting;r.changed=Now();M::Stop(r.guard);M::SetRotationToXY(r.guard,P::GetX(r.balso),P::GetY(r.balso));M::SetRotationToXY(r.balso,P::GetX(r.guard),P::GetY(r.guard));State(r);}}
   }
  }else if(r.phase==Phase::Meeting){
   if(!g_registry.valid(r.guard)||!g_registry.valid(r.balso))remove=true;
   else if(std::hypot(double(P::GetX(p)-P::GetX(r.guard)),double(P::GetY(p)-P::GetY(r.guard)))<=2000){BeginScene(r,p);}
   else if(Now()-r.changed>120){Notice(p,"SPY_LOST");remove=true;}
  }else if(r.phase==Phase::Cinematic){
   if(!g_registry.valid(r.guard)||!g_registry.valid(r.balso))remove=true;
   else if(r.sceneStage==0){
    if(Now()-r.changed>45){Notice(p,"SPY_NO_ROUTE");remove=true;}
    else if(r.approach){auto result=r.approach->Step(routeBudget);if(result==RouteSearch::Result::Failed){Notice(p,"SPY_NO_ROUTE");remove=true;}else if(result==RouteSearch::Result::Found){r.approachRoute=r.approach->route;r.approach.reset();r.approachPoint=1;}}
    else if(r.approachPoint<r.approachRoute.size()){
     Cell next=r.approachRoute[r.approachPoint];double dx=next.x*100+50-P::GetX(p),dy=next.y*100+50-P::GetY(p),distance=std::hypot(dx,dy);
     if(distance<=65){Pose(r,p,next.x*100+50,next.y*100+50,1);r.approachPoint++;}
     else Pose(r,p,P::GetX(p)+int(dx/distance*65),P::GetY(p)+int(dy/distance*65),1);
    }else{M::Stop(p);M::SetRotationToXY(p,P::GetX(r.guard),P::GetY(r.guard));M::SetRotationToXY(r.guard,P::GetX(p),P::GetY(p));r.sceneStage=1;r.changed=Now();ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"spy_scene %u 1",r.nonce);}
   }else{int elapsed=Now()-r.changed;
    if(elapsed>=3&&!r.aura){r.aura=true;Ability(r,"aura");}
    if(elapsed>=6&&r.sceneStage==1){r.sceneStage=2;Ability(r,"pierce");}
    if(elapsed>=7&&!r.knocked){r.knocked=true;double dx=P::GetX(p)-P::GetX(r.guard),dy=P::GetY(p)-P::GetY(r.guard),distance=std::max(1.0,std::hypot(dx,dy));Cell from=Position(p),last=from;
     for(int cm=50;cm<=400;cm+=50){Cell next{int(P::GetX(p)+dx/distance*cm)/100,int(P::GetY(p)+dy/distance*cm)/100};if(!Line(r.map,from,next)||!Walk(r.map,next,true))break;last=next;}
     Pose(r,p,last.x*100+50,last.y*100+50,2);}
    if(elapsed>=9&&g_registry.valid(r.balso)){
     Cell next=r.route[r.escapeWaypoint];M::SetNowWalking(r.balso,false);
     if(std::hypot(double(P::GetX(r.balso)-next.x*100-50),double(P::GetY(r.balso)-next.y*100-50))<=55&&r.escapeWaypoint)r.escapeWaypoint--;
     else if(M::Goto(r.balso,next.x*100+50,next.y*100+50))M::SendMovePacket(r.balso,FUNC_WAIT,0,0,0,0);
    }if(elapsed>=13){Destroy(r.balso);r.balso=entt::null;Fight(r);}}
  }else if(r.phase==Phase::Combat){
   if(!g_registry.valid(r.guard)){Notice(p,"SPY_LOST");remove=true;}
   else if(Distance(Position(p),r.meeting)>35){Notice(p,"SPY_LOST");remove=true;}
   else if(r.pendingAbility>=0&&int32_t(now-r.hitAt)>=0){int ability=r.pendingAbility;r.pendingAbility=-1;
    if(Line(r.map,Position(r.guard),Position(p))&&Distance(Position(p),Position(r.guard))<=(ability>=2?8:4)){
     int64_t damage=std::max(1,CalcMeleeDamage(r.guard,p,ability==0,false));damage=damage*(ability==0?160:ability==1?130:145)/100;
     CombatSystem::Damage(p,r.guard,damage,ability>=2?DAMAGE_TYPE_MAGIC:DAMAGE_TYPE_MELEE);}
   }
   else if((!r.busyUntil||int32_t(now-r.busyUntil)>=0)&&Now()!=r.changed){r.changed=Now();double d=std::hypot(double(P::GetX(p)-P::GetX(r.guard)),double(P::GetY(p)-P::GetY(r.guard)));
    if(d>170){Cell next=Position(p);if(Line(r.map,Position(r.guard),next)){M::SetRotationToXY(r.guard,P::GetX(p),P::GetY(p));if(M::Goto(r.guard,P::GetX(p),P::GetY(p)))M::SendMovePacket(r.guard,FUNC_WAIT,0,0,0,0);}}
    else{M::Stop(r.guard);M::SetRotationToXY(r.guard,P::GetX(p),P::GetY(p));
     if(Now()>=r.abilityAt){static const char* abilities[]={"pierce","ninja","sura","shaman"};r.pendingAbility=r.ability++%4;Ability(r,abilities[r.pendingAbility]);r.abilityAt=Now()+6;r.hitAt=now+650;r.busyUntil=now+2000;}
     else{ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"spy_strike %u %u",r.nonce,P::GetPacketVID(r.guard));M::SendMovePacket(r.guard,FUNC_ATTACK,0,P::GetX(r.guard),P::GetY(r.guard),0);CombatSystem::Attack(r.guard,p,0);}}}
  }else if(r.phase==Phase::Return&&Now()-r.changed>5){Destroy(r.guard);r.guard=entt::null;}
  if(remove){if(g_registry.valid(p))Notice(p,"SPY_RETRY");Clean(r);it=runs.erase(it);}else{if(r.phase!=Phase::Offer&&Now()-r.lastSent>=5)State(r);++it;}
 }
}
void OnDeath(entt::entity victim,entt::entity killer){
 auto* a=g_registry.valid(victim)?g_registry.try_get<Actor>(victim):nullptr;if(!a||!a->fighting||!g_registry.valid(killer)||P::GetPlayerID(killer)!=a->owner)return;
 auto it=runs.find(a->owner);if(it==runs.end()||it->second.phase!=Phase::Combat)return;
 P::SetQuestFlag(killer,"spy_exposure.status",2);it->second.phase=Phase::Return;it->second.changed=Now();State(it->second);Notice(killer,"SPY_RETURN");
}
}
ACMD(do_spyquest){SpyQuest::Command(character,argument);}
