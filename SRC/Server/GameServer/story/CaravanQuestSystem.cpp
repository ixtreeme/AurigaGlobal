#include "../core/stdafx.h"
#include "CaravanQuestSystem.hpp"
#include "CaravanRules.hpp"
#include "CaravanInstanceRules.hpp"
#include "CaravanVoiceTiming.hpp"
#include "CaravanEncounterRules.hpp"
#include "SpyQuestNavigation.hpp"
#include "../core/JsonFile.h"
#include "../core/locale_service.h"
#include "../core/utils.h"
#include "../command/cmd.h"
#include "../command/gm.h"
#include "../entity/char_manager.h"
#include "../entity/mob_manager.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/components/combat_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../social/party.h"
#include "../world/sectree_manager.h"
#include <sstream>
#include <random>
#include <set>

namespace CaravanQuest {
namespace {
namespace P=ecs::PlayerRuntime;namespace M=ecs::MovementSystem;
struct Member {uint32_t pid=0;bool ready=false;int mode=0;float x=0,z=-.5f;int64_t missing=0;uint32_t lastInput=0,seatChanged=0;bool released=false,entering=false;};
struct Run {uint32_t id=0,leader=0;int map=0,arrivalStage=0;bool privateMap=false;Phase phase=Phase::Offer;int64_t started=0,changed=0,lastWave=0;uint32_t lastSend=0,changedTick=0;std::vector<Member> members;std::vector<entt::entity> enemies;entt::entity cart=entt::null,giver=entt::null,receiver=entt::null;Vehicle vehicle;float x=0,y=0,throttle=.55f,steer=0;size_t waypoint=1;std::string failure;std::set<uint32_t> viewers;unsigned encounter=0;uint32_t nextHazard=1,nextBoardAt=0;std::vector<Encounter::Hazard> hazards;};
std::map<uint32_t,Run> runs;std::vector<SpyQuest::Cell> route;SpyQuest::Navigation nav;bool loaded=false,enabled=false;uint32_t previous=0;std::map<int,std::pair<entt::entity,entt::entity>> fixtures;
int templateIndex=ForestTemplate,maxRuns=32;std::string entryMap="metin2_map_main_razor93";SpyQuest::Cell entryPosition{23,28};
// Private sectors retain their template's world coordinates, but GetMapRegion only
// indexes public templates. Always normalize before resolving the coordinate base.
const TMapRegion* Region(int map){return SECTREE_MANAGER::instance().GetMapRegion(TemplateOf(map));}
bool InForest(int map){return ForestMap(map);}
std::map<int,int64_t> draining;std::map<uint32_t,int64_t> returnRetry;
int64_t Now(){return get_global_time();}
bool Valid(entt::entity e){return e!=entt::null&&g_registry.valid(e);}
entt::entity Player(const Member& m){return P::FindByPlayerID(m.pid);}
float Distance(entt::entity a,entt::entity b){if(!Valid(a)||!Valid(b))return 1e9f;return std::hypot(float(P::GetX(a)-P::GetX(b)),float(P::GetY(a)-P::GetY(b)))*.01f;}
void Notice(entt::entity p,const char* key){if(Valid(p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_notice %s",key);}
void Destroy(entt::entity e){if(Valid(e))CHARACTER_MANAGER::instance().DestroyCharacter(e);}
Run* For(uint32_t pid){for(auto& [id,r]:runs){if(r.phase==Phase::Failed&&Now()-r.changed>=6)continue;for(auto& m:r.members)if(m.pid==pid&&!m.released)return &r;}return nullptr;}
Member* Participant(Run& r,uint32_t pid){for(auto& m:r.members)if(m.pid==pid)return &m;return nullptr;}
void Load(){loaded=true;nlohmann::json j;
 if(JsonFile::Load(LocaleService_GetBasePath()+"/caravan_quest.json",j)&&j.value("schema_version",0)==1&&j.value("enabled",false)&&j.value("map","")=="auriga_caravan_forest"&&j.value("template_index",0)==420){
  maxRuns=std::clamp(j.value("max_runs",32),1,128);
  if(nav.Load(LocaleService_GetBasePath()+"/caravan_forest.nav")){
   for(auto& p:j["route"])route.push_back({p.at(0).get<int>(),p.at(1).get<int>()});enabled=route.size()>100;
   for(size_t i=0;i<route.size()&&enabled;i++){auto c=route[i];for(int x=-2;x<=2;x++)for(int y=-2;y<=2;y++)if(!nav.Walk({c.x+x,c.y+y}))enabled=false;if(i&&!nav.Step(route[i-1],c))enabled=false;}
  }
 }LOG_INFO("CARAVAN_QUEST_CONFIG enabled={} points={} private_template={}",enabled,route.size(),templateIndex);
}
std::pair<int,int> Global(int map,SpyQuest::Cell c){auto* region=Region(map);return region?std::pair{region->sx+c.x*100+50,region->sy+c.y*100+50}:std::pair{0,0};}
bool Walk(int map,int x,int y){auto* region=Region(map);if(!region)return false;SpyQuest::Cell c{(x-region->sx)/100,(y-region->sy)/100};
 for(int dx=-2;dx<=2;dx++)for(int dy=-2;dy<=2;dy++)if(!nav.Walk({c.x+dx,c.y+dy}))return false;
 auto* sector=SECTREE_MANAGER::instance().Get(map,x,y);return sector&&!(sector->GetAttribute(x,y)&(ATTR_BLOCK|ATTR_OBJECT|ATTR_BANPK));}
bool Landing(int map,int x,int y){auto* region=Region(map);if(!region||!nav.Walk({(x-region->sx)/100,(y-region->sy)/100}))return false;auto* sector=SECTREE_MANAGER::instance().Get(map,x,y);return sector&&!(sector->GetAttribute(x,y)&(ATTR_BLOCK|ATTR_OBJECT));}
entt::entity Spawn(uint32_t race,int map,int x,int y,uint32_t id,uint8_t role){auto e=CHARACTER_MANAGER::instance().SpawnMobEntity(race,map,x,y,0,false,0,false);if(!Valid(e))return entt::null;
 auto original=CMobManager::instance().Get(race);if(!original){Destroy(e);return entt::null;}auto proto=std::make_shared<CMob>(*original);auto& t=proto->m_table;
 t.dwExp=t.dwGoldMin=t.dwGoldMax=t.dwDropItemVnum=t.dwResurrectionVnum=t.dwSummonVnum=0;t.dwAIFlag=0;t.bRegenPercent=0;
 g_registry.emplace<Actor>(e,Actor{id,role,proto,get_dword_time()});P::SetProto(e,proto.get());M::SetNowWalking(e,false);
 if(!M::Show(e,map,x,y,0,false)){Destroy(e);return entt::null;}return e;}
void EnsureFixtures(int map){if(map>=10000||fixtures.count(map))return;auto start=Global(map,entryPosition);auto a=Spawn(20042,map,start.first+300,start.second,0,3);
 if(Valid(a))fixtures[map]={a,entt::null};}
void ValidateInstances(){static bool done=false;if(done||!SECTREE_MANAGER::instance().GetMap(templateIndex))return;done=true;
 auto& manager=SECTREE_MANAGER::instance();int a=manager.CreatePrivateMap(templateIndex),b=manager.CreatePrivateMap(templateIndex);bool pass=a>=10000&&b>=10000&&a!=b;
 entt::entity first=entt::null,second=entt::null;auto start=Global(templateIndex,route.front());
 if(pass){first=Spawn(20042,a,start.first,start.second,0,3);second=Spawn(20042,b,start.first,start.second,0,3);
  pass=Valid(first)&&Valid(second)&&P::GetMapIndex(first)==a&&P::GetMapIndex(second)==b&&manager.Get(a,start.first,start.second)!=manager.Get(b,start.first,start.second);
  for(auto c:route){auto point=Global(templateIndex,c);if(!Walk(a,point.first,point.second)||!Walk(b,point.first,point.second)){pass=false;break;}}
 }
 Destroy(first);Destroy(second);if(a)manager.DestroyPrivateMap(a);if(b)manager.DestroyPrivateMap(b);
 if(pass)LOG_INFO("CARAVAN_INSTANCE_PROOF PASS isolated_maps={} {} route_samples={}",a,b,route.size());
 else{enabled=false;LOG_ERROR("CARAVAN_INSTANCE_PROOF FAILED; caravan admission disabled");}
}
void ReturnHome(entt::entity p){if(!Valid(p)||!P::GetDesc(p))return;auto pid=P::GetPlayerID(p);if(returnRetry[pid]>Now())return;returnRetry[pid]=Now()+5;
 int map=P::GetQuestFlag(p,"caravan_food.return_map"),x=P::GetQuestFlag(p,"caravan_food.return_x"),y=P::GetQuestFlag(p,"caravan_food.return_y");
 // Return coordinates are server-written; stale private destinations are never reused.
 auto* region=Region(map);if(map>0&&map<10000&&!InForest(map)&&region&&x>=region->sx&&x<region->ex&&y>=region->sy&&y<region->ey){if(M::WarpSet(p,x,y,map))return;}
 M::GoHome(p);
}
bool Present(Run& r,Member& m){auto p=Player(m);if(m.released||m.mode<0||!Valid(p)||!P::GetDesc(p)||P::GetMapIndex(p)!=r.map||CombatSystem::IsDead(p)||MountSystem::GetMountVnum(p)||P::GetRaceNum(p)>=8||Distance(p,r.giver)>20)return false;auto victim=CombatSystem::GetVictim(p);return !Valid(victim)||CombatSystem::IsDead(victim);}
void SetPhase(Run& r,Phase phase){r.phase=phase;r.changed=Now();r.changedTick=get_dword_time();}
void CleanupMember(Run& r,Member& m){if(m.released)return;m.released=true;auto p=Player(m);if(!Valid(p))return;
 if(g_registry.all_of<Rider>(p)&&g_registry.get<Rider>(p).run==r.id){g_registry.remove<Rider>(p);M::Stop(p);
  if(P::GetMapIndex(p)==r.map&&!CombatSystem::IsDead(p)){
   size_t index=size_t(&m-r.members.data());float side=index%2?-2.4f:2.4f,forward=1.f-float(index),angle=r.vehicle.yaw*float(M_PI)/180;int x=int(r.x+(std::cos(angle)*side+std::sin(angle)*forward)*100),y=int(r.y+(std::sin(angle)*side-std::cos(angle)*forward)*100);bool moved=false;
   if(Landing(r.map,x,y)){M::Sync(p,x,y);moved=true;}else for(auto offset:{std::pair{400,0},std::pair{-400,0},std::pair{0,400},std::pair{0,-400}})if(Walk(r.map,int(r.x)+offset.first,int(r.y)+offset.second)){M::Sync(p,int(r.x)+offset.first,int(r.y)+offset.second);moved=true;break;}
   if(moved)NetworkSyncSystem::BroadcastSyncPacket(g_registry,p);
  }
 }ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_release %u",r.id);
 if(r.privateMap&&InForest(P::GetMapIndex(p)))ReturnHome(p);
}
void DespawnEnemies(Run& r){for(auto e:r.enemies)Destroy(e);r.enemies.clear();r.hazards.clear();}
void Fail(Run& r,const char* key){if(r.phase==Phase::Failed||r.phase==Phase::Complete)return;r.failure=key;r.vehicle.speed=0;SetPhase(r,Phase::Failed);DespawnEnemies(r);for(auto& m:r.members)Notice(Player(m),key);}
void Send(Run& r,bool force=false){uint32_t now=get_dword_time();if(!force&&now-r.lastSend<200)return;r.lastSend=now;
 auto giver=Valid(r.giver)?P::GetPacketVID(r.giver):0,receiver=Valid(r.receiver)?P::GetPacketVID(r.receiver):0,cart=Valid(r.cart)?P::GetPacketVID(r.cart):0;
 std::set<uint32_t> seen;for(auto p:g_registry.view<ecs::PlayerID>()){
  if(!P::GetDesc(p)||P::GetMapIndex(p)!=r.map)continue;auto pid=P::GetPlayerID(p);auto* own=Participant(r,pid);
  bool nearby=Valid(r.cart)?Distance(p,r.cart)<90:Distance(p,r.giver)<90;if(!own&&!nearby)continue;seen.insert(pid);
  ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_frame %u %d %u %d %d %d %d %d %d %u %u %u",r.id,int(r.phase),cart,int(r.x),int(r.y),int(r.vehicle.yaw*100),int(r.vehicle.speed*100),r.vehicle.hp,int(r.vehicle.risk*100),giver,receiver,now-r.changedTick);
  if(!r.failure.empty())ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_reason %u %s",r.id,r.failure.c_str());
  if(r.phase==Phase::Arrival)ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_stage %u %d",r.id,r.arrivalStage);
  if(r.phase==Phase::Travelling){
   for(auto e:r.enemies)if(Valid(e)&&!CombatSystem::IsDead(e)){auto& a=g_registry.get<Actor>(e);if(a.boardMode)ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_enemy %u %u %d %d %d %d %u",r.id,P::GetPacketVID(e),a.boardMode,a.fromX,a.fromY,a.deckSlot,now-a.moveAt);}
   for(auto& h:r.hazards)ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_hazard %u %u %d %d %d %d %u",r.id,h.id,int(h.x*100),int(h.y*100),int(h.fromX*100),int(h.fromY*100),now-h.created);
  }
  int ready=0;for(size_t i=0;i<r.members.size();i++){auto& m=r.members[i];auto member=Player(m);bool present=Present(r,m);if(m.ready&&present)ready++;
   ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_member %u %zu %u %u %d %d %d %d %d %u",r.id,i,Valid(member)?P::GetPacketVID(member):0,Valid(member)?P::GetRaceNum(member):0,m.mode,int(m.x*100),int(m.z*100),m.ready?1:0,present?1:0,std::min<uint32_t>(60000,now-m.seatChanged));
  }
  if(own&&!own->released&&own->mode>=0&&!(r.phase==Phase::Failed&&Now()-r.changed>=6)){ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_you %u %u %d %zu",r.id,own->pid==r.leader?1:0,ready,r.members.size());
   if(r.phase==Phase::Travelling){std::ostringstream points;for(size_t k=r.waypoint;k<std::min(route.size(),r.waypoint+60);k+=5){auto v=Global(r.map,route[k]);points<<' '<<v.first<<' '<<v.second;}ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_route %u%s",r.id,points.str().c_str());}
  }
 }
 for(auto pid:r.viewers)if(!seen.count(pid)){auto p=P::FindByPlayerID(pid);if(Valid(p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_end %u",r.id);}r.viewers=std::move(seen);
}
void Retire(Run& r){for(auto& m:r.members)CleanupMember(r,m);for(auto pid:r.viewers){auto p=P::FindByPlayerID(pid);if(Valid(p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_end %u",r.id);}DespawnEnemies(r);Destroy(r.cart);
 if(r.privateMap){Destroy(r.giver);Destroy(r.receiver);draining[r.map]=Now();LOG_INFO("CARAVAN_INSTANCE_RETIRE run={} map={}",r.id,r.map);}}
bool EnterForest(Run& r){
 // Allocation and all state remain on the host which owns the entry NPC. The
 // template must be assigned to that same host; cross-core in-memory runs are unsafe.
 if(!SECTREE_MANAGER::instance().GetMap(templateIndex))return false;
 for(auto& m:r.members){auto p=Player(m);if(!Present(r,m))return false;}
 int instance=SECTREE_MANAGER::instance().CreatePrivateMap(templateIndex);if(!instance)return false;
 auto start=Global(instance,route.front()),end=Global(instance,route.back());
 auto cart=Spawn(20042,instance,start.first,start.second,r.id,1),giver=Spawn(20042,instance,start.first+300,start.second,r.id,3),receiver=Spawn(20015,instance,end.first+300,end.second,r.id,3);
 if(!Valid(cart)||!Valid(giver)||!Valid(receiver)){Destroy(cart);Destroy(giver);Destroy(receiver);SECTREE_MANAGER::instance().DestroyPrivateMap(instance);return false;}
 for(auto pid:r.viewers){auto p=P::FindByPlayerID(pid);if(Valid(p))ecs::ChatSystem::Send(p,CHAT_TYPE_COMMAND,"cv_end %u",r.id);}r.viewers.clear();
 r.map=instance;r.privateMap=true;r.cart=cart;r.giver=giver;r.receiver=receiver;r.x=start.first;r.y=start.second;
 auto next=Global(instance,route[6]);r.vehicle.yaw=std::atan2(float(next.first-start.first),float(start.second-next.second))*180/float(M_PI);
 SetPhase(r,Phase::Waiting);
 for(size_t i=0;i<r.members.size();i++){auto& m=r.members[i];auto p=Player(m);m.entering=true;m.missing=Now();
  P::SetQuestFlag(p,"caravan_food.return_map",P::GetMapIndex(p));P::SetQuestFlag(p,"caravan_food.return_x",P::GetX(p));P::SetQuestFlag(p,"caravan_food.return_y",P::GetY(p));M::SaveExitLocation(p);
  if(!M::WarpSet(p,start.first+500+int(i)*150,start.second,instance)){Fail(r,"CV_UNAVAILABLE");return false;}
 }
 LOG_INFO("CARAVAN_INSTANCE_ENTER run={} map={} members={}",r.id,instance,r.members.size());return true;
}
void Board(Run& r){for(size_t i=0;i<r.members.size();i++){auto& m=r.members[i];auto p=Player(m);if(!Valid(p)||m.mode<0||m.missing)continue;(void)g_registry.get_or_emplace<Rider>(p,Rider{r.id});M::Stop(p);m.mode=int(i);m.seatChanged=get_dword_time();m.x=i==1?-.55f:i==2?.55f:0;m.z=i==0?1.05f:-1.7f;}}
void SyncRiders(Run& r){float yaw=r.vehicle.yaw*float(M_PI)/180;for(auto& m:r.members){auto p=Player(m);if(!Valid(p)||!g_registry.all_of<Rider>(p))continue;
  float ox=m.mode==0?0:m.x,oz=m.mode==0?1.05f:m.z;int x=int(r.x+(std::cos(yaw)*ox+std::sin(yaw)*oz)*100),y=int(r.y+(std::sin(yaw)*ox-std::cos(yaw)*oz)*100);
  M::Sync(p,x,y);NetworkSyncSystem::BroadcastSyncPacket(g_registry,p);
 }}
void Wave(Run& r){if(r.enemies.size()>=size_t(BanditLimit(int(r.members.size()))))return;auto target=route[std::min(route.size()-1,r.waypoint+15)];auto pos=Global(r.map,target);
 for(int i=0;i<int(r.members.size())+1;i++){if(r.enemies.size()>=size_t(BanditLimit(int(r.members.size()))))break;int x=pos.first+(i%2?500:-500),y=pos.second+(i/2)*300;if(!Walk(r.map,x,y))continue;
  auto e=Spawn(Encounter::Pursuers[(Now()/15+i)%Encounter::Pursuers.size()],r.map,x,y,r.id,2);if(!Valid(e))continue;auto& t=g_registry.get<Actor>(e).prototype->m_table;auto p=Player(r.members.front());t.bLevel=std::clamp(ecs::PointSystem::GetLevel(p),5,150);t.dwMaxHP=std::clamp<uint64_t>(ecs::PointSystem::GetMaxHP(p)/2,300,50000);t.wDef=t.bLevel/2;t.dwDamageRange[0]=20;t.dwDamageRange[1]=40;t.sMovingSpeed=180;P::SetProto(e,&*g_registry.get<Actor>(e).prototype);ecs::PointSystem::Compute(e);P::SetHP(e,t.dwMaxHP);r.enemies.push_back(e);
 }}
void RunNotice(Run& r,const char* key){for(auto& m:r.members)Notice(Player(m),key);}
entt::entity Special(Run& r,uint32_t race,int x,int y){auto e=Spawn(race,r.map,x,y,r.id,2);if(!Valid(e))return e;auto& t=g_registry.get<Actor>(e).prototype->m_table;auto player=Player(r.members.front());t.bLevel=std::clamp(ecs::PointSystem::GetLevel(player),5,150);t.dwMaxHP=std::clamp<uint64_t>(ecs::PointSystem::GetMaxHP(player),800,race==691?100000:50000);t.dwDamageRange[0]=20;t.dwDamageRange[1]=40;t.sMovingSpeed=180;P::SetProto(e,g_registry.get<Actor>(e).prototype.get());ecs::PointSystem::Compute(e);P::SetHP(e,t.dwMaxHP);r.enemies.push_back(e);return e;}
void Thrower(Run& r,int x,int y,uint32_t now){auto e=Special(r,657,x,y);if(!Valid(e))return;auto& a=g_registry.get<Actor>(e);a.boardMode=5;a.moveAt=now;a.expires=now+28000;a.fromX=x;a.fromY=y;}
void Encounters(Run& r,uint32_t now){
 r.hazards.erase(std::remove_if(r.hazards.begin(),r.hazards.end(),[now](auto& h){return h.Expired(now);}),r.hazards.end());
 for(auto& h:r.hazards)if(h.TryHit(now,r.x*.01f,r.y*.01f)){r.vehicle.hp=std::max(0,r.vehicle.hp-850);r.vehicle.speed*=.55f;RunNotice(r,"CV_OBSTACLE_HIT");}
 if(Encounter::Due(r.encounter,float(r.waypoint)/float(route.size()-1))){
  unsigned stage=r.encounter++;size_t centerIndex=std::min(route.size()-1,r.waypoint+28);auto center=Global(r.map,route[centerIndex]),forward=Global(r.map,route[std::min(route.size()-1,centerIndex+6)]);float yaw=std::atan2(float(forward.first-center.first),float(center.second-forward.second));
  if(Encounter::CanyonStage(stage)){RunNotice(r,"CV_CANYON_AMBUSH");int side=stage==1?-1:1;int x=center.first+int(std::cos(yaw)*side*750),y=center.second+int(std::sin(yaw)*side*750);auto e=Special(r,side<0?655:656,x,y);if(Valid(e)){auto& a=g_registry.get<Actor>(e);a.boardMode=3;a.deckSlot=0;a.moveAt=now;a.expires=now+20000;a.fromX=x;a.fromY=y;}
   // The thrower stands on the opposite road shoulder, in front of the cliff
   // face rather than inside its rock geometry. One jumper plus one thrower.
   Thrower(r,center.first-int(std::cos(yaw)*side*450),center.second-int(std::sin(yaw)*side*450),now);RunNotice(r,"CV_ROCK_WARNING");}
  else if(stage==2){RunNotice(r,"CV_WARLORD");Special(r,691,center.first,center.second);}
  else{RunNotice(r,"CV_ROCK_WARNING");if(stage==4){RunNotice(r,"CV_WARLORD");Special(r,691,center.first,center.second);}int side=stage==0?1:-1;Thrower(r,center.first+int(std::cos(yaw)*side*700),center.second+int(std::sin(yaw)*side*700),now);}
 }
 int aboard=0;bool used[Encounter::MaxBoarders]={};for(auto e:r.enemies)if(Valid(e)&&!CombatSystem::IsDead(e)){auto& a=g_registry.get<Actor>(e);if(a.boardMode==1||a.boardMode==2||a.boardMode==4){aboard++;if(a.deckSlot>=0&&a.deckSlot<Encounter::MaxBoarders)used[a.deckSlot]=true;}}
 for(auto e:r.enemies){if(!Valid(e)||CombatSystem::IsDead(e))continue;auto& a=g_registry.get<Actor>(e);float distance=Distance(e,r.cart);
  if(a.boardMode==3||a.boardMode==5){
   if(P::GetRaceNum(e)==657){if(int32_t(now-a.expires)>=0){a.boardMode=0;continue;}if(int32_t(now-a.attackAt)>=0&&distance<48&&r.hazards.size()<5){a.attackAt=now+6500;M::SetRotationToXY(e,int(r.x),int(r.y));M::SendMovePacket(e,FUNC_ATTACK,0,P::GetX(e),P::GetY(e),0);
     // One fixed impact lane at a time, including the centre. Both other lanes stay open.
     size_t impactIndex=r.waypoint;float ahead=0;while(impactIndex+1<route.size()&&ahead<Encounter::ImpactLeadMetres(r.vehicle.speed)){ahead+=std::hypot(float(route[impactIndex+1].x-route[impactIndex].x),float(route[impactIndex+1].y-route[impactIndex].y));impactIndex++;}
     auto point=Global(r.map,route[impactIndex]);float angle=r.vehicle.yaw*float(M_PI)/180;float side=Encounter::ImpactLane(r.nextHazard);int x=point.first+int(std::cos(angle)*side*100),y=point.second+int(std::sin(angle)*side*100);
     if(Landing(r.map,x,y))r.hazards.push_back({r.nextHazard++,now,x*.01f,y*.01f,P::GetX(e)*.01f,P::GetY(e)*.01f,false});}
    continue;
   }
   if(distance>11){if(int32_t(now-a.expires)>=0){a.boardMode=0;}else continue;}
   else if(aboard<Encounter::MaxBoarders&&Encounter::BoardingReady(now,r.nextBoardAt)){int slot=0;while(slot<Encounter::MaxBoarders&&used[slot])slot++;a.deckSlot=slot;used[slot]=true;aboard++;a.boardMode=4;a.moveAt=now;r.nextBoardAt=now+Encounter::BoardingCooldownMs;M::Stop(e);RunNotice(r,"CV_BOARDERS");}
   else continue;
  }
  if(a.boardMode==0&&Encounter::BoardingReady(now,r.nextBoardAt)&&Encounter::Boardable(true,true,distance,aboard)&&P::GetRaceNum(e)!=654&&P::GetRaceNum(e)!=691){int slot=0;while(slot<Encounter::MaxBoarders&&used[slot])slot++;a.deckSlot=slot;used[slot]=true;aboard++;a.boardMode=1;a.moveAt=now;r.nextBoardAt=now+Encounter::BoardingCooldownMs;a.fromX=P::GetX(e);a.fromY=P::GetY(e);M::Stop(e);RunNotice(r,"CV_BOARDERS");}
  if(a.boardMode==1||a.boardMode==2||a.boardMode==4){float duration=a.boardMode==4?Encounter::LeapSeconds:Encounter::ClimbSeconds;if(a.boardMode!=2&&now-a.moveAt>=uint32_t(duration*1000)){a.boardMode=2;a.moveAt=now;}
   float angle=r.vehicle.yaw*float(M_PI)/180,x=Encounter::DeckX(a.deckSlot),z=Encounter::DeckZ(a.deckSlot);M::Sync(e,int(r.x+(std::cos(angle)*x+std::sin(angle)*z)*100),int(r.y+(std::sin(angle)*x-std::cos(angle)*z)*100));NetworkSyncSystem::BroadcastSyncPacket(g_registry,e);
   if(a.boardMode==2&&int32_t(now-a.attackAt)>=0){a.attackAt=now+1700;M::SetRotation(e,r.vehicle.yaw+180);M::SendMovePacket(e,FUNC_ATTACK,0,P::GetX(e),P::GetY(e),0);r.vehicle.hp=std::max(0,r.vehicle.hp-65);}
  }
 }
}
}
bool Locked(entt::entity p){return Valid(p)&&g_registry.all_of<Rider>(p);}
bool Defending(entt::entity p){if(!Locked(p))return false;auto* r=For(P::GetPlayerID(p));auto* m=r?Participant(*r,P::GetPlayerID(p)):nullptr;return r&&m&&r->phase==Phase::Travelling&&m->mode==3&&get_dword_time()-m->seatChanged>=1200;}
const CMob* Prototype(entt::entity e){auto* a=Valid(e)?g_registry.try_get<Actor>(e):nullptr;return a&&a->prototype?a->prototype.get():nullptr;}
bool AllowCombat(entt::entity attacker,entt::entity victim){
 if(Locked(victim))return false;
 for(auto pair:{std::pair{attacker,victim},std::pair{victim,attacker}})if(Valid(pair.first))if(auto* a=g_registry.try_get<Actor>(pair.first)){
  if(a->role!=2)return false;auto it=runs.find(a->run);if(it==runs.end()||it->second.phase!=Phase::Travelling)return false;
  if(!Valid(pair.second))return false;auto* id=g_registry.try_get<ecs::PlayerID>(pair.second);if(!id)return false;auto* m=Participant(it->second,id->pid);if(!m||m->mode<3)return false;
 }
 if(Locked(attacker)){auto r=For(P::GetPlayerID(attacker));auto* m=r?Participant(*r,P::GetPlayerID(attacker)):nullptr;if(!m||r->phase!=Phase::Travelling||m->mode<3)return false;}
 return true;
}
bool Click(entt::entity p,entt::entity npc){if(!loaded)Load();if(!enabled||!Valid(npc)||P::GetRaceNum(npc)!=20042)return false;
 auto* region=Region(P::GetMapIndex(p));if(!region||region->strMapName!=entryMap||P::GetMapIndex(p)>=10000)return false;
 if(!P::IsPC(p)||CombatSystem::IsDead(p)||Distance(p,npc)>5)return true;
 auto pid=P::GetPlayerID(p);if(auto* existing=For(pid)){Send(*existing,true);return true;}
 if(P::GetQuestFlag(p,"caravan_food.completed")){Notice(p,"CV_ALREADY_DONE");return true;}
 if(runs.size()>=size_t(maxRuns)||!SECTREE_MANAGER::instance().GetMap(templateIndex)){Notice(p,"CV_UNAVAILABLE");return true;}
 Run r;r.leader=pid;r.map=P::GetMapIndex(p);r.giver=npc;r.started=r.changed=Now();r.changedTick=get_dword_time();r.id=std::random_device{}();while(!r.id||runs.count(r.id))r.id=std::random_device{}();
 auto party=PartySystem::GetCharacterParty(p);auto* state=PartySystem::Find(party);
 if(state){if(state->leaderPID!=pid){Notice(p,"CV_LEADER_ONLY");return true;}if(state->members.size()>3){Notice(p,"CV_GROUP_LIMIT");return true;}
  for(auto& [memberId,row]:state->members){if(For(memberId)){Notice(p,"CV_MEMBER_BUSY");return true;}auto member=P::FindByPlayerID(memberId);if(!Valid(member)||!P::GetDesc(member)||P::GetMapIndex(member)!=r.map||CombatSystem::IsDead(member)){Notice(p,"CV_GROUP_PRESENT");return true;}r.members.push_back(Member{memberId});}
 }else r.members.push_back(Member{pid});
 std::stable_sort(r.members.begin(),r.members.end(),[pid](auto& a,auto& b){return a.pid==pid&&b.pid!=pid;});r.x=P::GetX(npc);r.y=P::GetY(npc);
 runs.emplace(r.id,std::move(r));Send(runs.at(r.id),true);return true;
}
void Command(entt::entity p,const char* args){if(!loaded)Load();if(!enabled||!Valid(p)||!P::IsPC(p))return;std::istringstream in(args?args:"");std::string action;uint32_t id=0;in>>action;
 if(action=="reset"&&P::GetGMLevel(p)>=GM_LOW_WIZARD){auto* r=For(P::GetPlayerID(p));if(r){auto key=r->id;Retire(*r);runs.erase(key);}P::SetQuestFlag(p,"caravan_food.completed",0);Notice(p,"CV_RESET");return;}
 if(action=="setup"&&P::GetGMLevel(p)>=GM_LOW_WIZARD){auto* region=SECTREE_MANAGER::instance().GetMapRegion(P::GetMapIndex(p));if(region&&region->strMapName=="metin2_map_main_razor93")EnsureFixtures(P::GetMapIndex(p));return;}
 if(action=="start"&&P::GetGMLevel(p)>=GM_LOW_WIZARD){auto* region=Region(P::GetMapIndex(p));if(region&&region->strMapName==entryMap&&P::GetMapIndex(p)<10000&&!For(P::GetPlayerID(p))){EnsureFixtures(P::GetMapIndex(p));auto pos=Global(P::GetMapIndex(p),entryPosition);M::WarpSet(p,pos.first+500,pos.second,0);}return;}
 in>>id;auto it=runs.find(id);if(it==runs.end())return;auto& r=it->second;auto* m=Participant(r,P::GetPlayerID(p));if(!m||m->released||m->mode<0)return;
 if(action=="cancel"){Fail(r,"CV_CANCELLED");return;}
 if(r.phase==Phase::Offer&&m->pid==r.leader){if(action=="later"){SetPhase(r,Phase::Later);Send(r,true);return;}if(action=="decline"){SetPhase(r,Phase::Declined);Send(r,true);return;}
  if(action=="accept"){if(!EnterForest(r)){Notice(p,"CV_GROUP_PRESENT");if(r.privateMap)Fail(r,"CV_UNAVAILABLE");}Send(r,true);}return;}
 if(action=="ready"&&(r.phase==Phase::Waiting||r.phase==Phase::Countdown)){m->ready=Present(r,*m);Send(r,true);return;}
 if(r.phase!=Phase::Travelling||!Locked(p))return;
 if(action=="seat"){
  if(get_dword_time()-m->seatChanged<1200)return;m->seatChanged=get_dword_time();
  if(m->mode<3){m->mode=3;m->x=m->x<0?-.7f:.7f;m->z=-.5f;r.throttle=.4f;}
  else{for(int seat=0;seat<3;seat++){bool occupied=false;for(auto& other:r.members)if(&other!=m&&other.mode==seat)occupied=true;if(!occupied){m->mode=seat;m->x=seat==1?-.55f:seat==2?.55f:0;m->z=seat==0?1.05f:-1.7f;break;}}}
  Send(r,true);return;
 }
 if(action=="input"){uint32_t now=get_dword_time();if(now-m->lastInput<80||now-m->seatChanged<1200)return;m->lastInput=now;float a=0,b=0;in>>a>>b;if(!std::isfinite(a)||!std::isfinite(b))return;
  if(m->mode==0){r.throttle=std::clamp(a,0.f,1.f);r.steer=std::clamp(b,-1.f,1.f);}else if(m->mode==3){m->x=std::clamp(m->x+std::clamp(a,-1.f,1.f)*.14f,-.7f,.7f);m->z=std::clamp(m->z+std::clamp(b,-1.f,1.f)*.14f,-1.6f,.3f);}
 }
}
void Tick(){if(!loaded)Load();if(!enabled)return;auto now=get_dword_time();if(now-previous<200)return;float dt=std::min(.25f,(now-previous)*.001f);previous=now;
 ValidateInstances();if(!enabled)return;
 for(auto p:g_registry.view<ecs::PlayerID>())if(P::GetDesc(p)&&P::GetSectree(p)){int map=P::GetMapIndex(p);auto* region=Region(map);if(region&&region->strMapName==entryMap&&map<10000)EnsureFixtures(map);
  if(InForest(map)){auto* run=For(P::GetPlayerID(p));if(!run||!Admitted(map,run->map,run->privateMap,Participant(*run,P::GetPlayerID(p))!=nullptr,false))ReturnHome(p);}else returnRetry.erase(P::GetPlayerID(p));}
 for(auto it=runs.begin();it!=runs.end();){auto& r=it->second;bool remove=false;
  if(Now()-r.started>1800&&r.phase!=Phase::Failed)Fail(r,"CV_TIMEOUT");
  int alive=0;for(auto& m:r.members){auto p=Player(m);bool valid=Valid(p)&&P::GetDesc(p)&&P::GetMapIndex(p)==r.map&&!CombatSystem::IsDead(p);
   if(m.entering){if(valid)m.entering=false;else if(Now()-r.changed<60)continue;else{m.entering=false;Fail(r,"CV_GROUP_PRESENT");}}
   if(!valid&&Valid(p)&&(P::GetMapIndex(p)!=r.map||CombatSystem::IsDead(p))){CleanupMember(r,m);m.mode=-1;}
   if(valid&&m.mode>=0&&!m.released){alive++;if(m.missing&&r.phase==Phase::Travelling)(void)g_registry.get_or_emplace<Rider>(p,Rider{r.id});m.missing=0;}else{if(!m.missing)m.missing=Now();if(Valid(p))if(auto* rider=g_registry.try_get<Rider>(p);rider&&rider->run==r.id)g_registry.remove<Rider>(p);if(r.phase==Phase::Travelling&&Now()-m.missing>30){m.ready=false;m.mode=-1;CleanupMember(r,m);}}
  }
  bool reconnecting=false;for(auto& member:r.members)if(member.entering||(member.missing&&Now()-member.missing<=30))reconnecting=true;
  if(!alive&&!reconnecting&&r.phase!=Phase::Failed)Fail(r,"CV_NO_PLAYERS");
  if(r.phase==Phase::Offer){if(Now()-r.changed>120)remove=true;}
  else if(r.phase==Phase::Waiting||r.phase==Phase::Countdown){int ready=0;bool present=true;for(auto& m:r.members){bool here=Present(r,m);if(!here)m.ready=false;present&=here;if(m.ready)ready++;}
   bool all=ReadyToStart(int(r.members.size()),ready,present);if(r.phase==Phase::Waiting&&all)SetPhase(r,Phase::Countdown);else if(r.phase==Phase::Countdown&&!all)SetPhase(r,Phase::Waiting);
   else if(r.phase==Phase::Countdown&&Now()-r.changed>=3){if(!Valid(r.cart)){Fail(r,"CV_UNAVAILABLE");}else{Board(r);SetPhase(r,Phase::Loading);}}
  }else if(r.phase==Phase::Loading){SyncRiders(r);if(Now()-r.changed>=DialogueTiming::LoadingEnd){SetPhase(r,Phase::Travelling);r.lastWave=Now();}}
  else if(r.phase==Phase::Travelling){
   if(!Valid(r.cart))Fail(r,"CV_DESTROYED");else{
    bool assembled=true;for(auto& member:r.members)if(member.mode>=0&&member.missing)assembled=false;
    auto goal=Global(r.map,route.back());if(std::hypot(r.x-goal.first,r.y-goal.second)<=2000&&assembled){SetPhase(r,Phase::Arrival);Board(r);DespawnEnemies(r);}
    else{
     size_t closest=r.waypoint;float best=1e20f;for(size_t index=r.waypoint>2?r.waypoint-2:0;index<std::min(route.size(),r.waypoint+15);index++){auto pt=Global(r.map,route[index]);float distance=std::hypot(r.x-pt.first,r.y-pt.second);if(distance<best){best=distance;closest=index;}}r.waypoint=std::max(r.waypoint,closest);
     auto* region=Region(r.map);float desired=RoadHeading(route,r.waypoint,(r.x-region->sx)*.01f,(r.y-region->sy)*.01f,r.vehicle.speed);bool driver=false;for(auto& m:r.members)if(m.mode==0&&Valid(Player(m))&&!CombatSystem::IsDead(Player(m)))driver=true;
     r.vehicle.Step(dt,r.throttle,desired+(driver?r.steer*24:0),driver);float angle=r.vehicle.yaw*float(M_PI)/180;float nx=r.x+std::sin(angle)*r.vehicle.speed*dt*100,ny=r.y-std::cos(angle)*r.vehicle.speed*dt*100;
     if(Walk(r.map,int(nx),int(ny))){r.x=nx;r.y=ny;M::Sync(r.cart,int(nx),int(ny));M::SetRotation(r.cart,r.vehicle.yaw);NetworkSyncSystem::BroadcastSyncPacket(g_registry,r.cart);}else{r.vehicle.speed=0;r.steer=0;}
     SyncRiders(r);if(r.vehicle.cargoLost)Fail(r,"CV_CARGO_LOST");
     if(r.phase!=Phase::Travelling){Send(r);++it;continue;}
     if(Now()-r.lastWave>=15){r.lastWave=Now();Wave(r);}
     r.enemies.erase(std::remove_if(r.enemies.begin(),r.enemies.end(),[](auto e){return !Valid(e)||CombatSystem::IsDead(e);}),r.enemies.end());
     Encounters(r,now);
     for(auto e:r.enemies){if(g_registry.get<Actor>(e).boardMode)continue;bool archer=P::GetRaceNum(e)==654;float side=(P::GetPacketVID(e)%2?1.f:-1.f)*(archer?3.5f:2.1f),offset=float(P::GetPacketVID(e)%3)-1;float angle=r.vehicle.yaw*float(M_PI)/180;
      int tx=int(r.x+(std::cos(angle)*side+std::sin(angle)*offset)*100),ty=int(r.y+(std::sin(angle)*side-std::cos(angle)*offset)*100);auto& actor=g_registry.get<Actor>(e);
      if(std::hypot(float(P::GetX(e)-tx),float(P::GetY(e)-ty))>120){if(!Walk(r.map,tx,ty))continue;if(!M::Goto(e,tx,ty))continue;M::SendMovePacket(e,FUNC_WAIT,0,0,0,0);}
      else if(int32_t(now-actor.attackAt)>=0){actor.attackAt=now+(archer?1600:1000);M::Stop(e);M::SetRotationToXY(e,int(r.x),int(r.y));M::SendMovePacket(e,FUNC_ATTACK,0,P::GetX(e),P::GetY(e),0);
       if(archer)for(auto pid:r.viewers){auto viewer=P::FindByPlayerID(pid);if(Valid(viewer))ecs::ChatSystem::Send(viewer,CHAT_TYPE_COMMAND,"cv_shot %u %u",r.id,P::GetPacketVID(e));}
       r.vehicle.hp=std::max(0,r.vehicle.hp-(25+int(r.members.size())*8));}
     }
     if(r.vehicle.hp<=0)Fail(r,"CV_DESTROYED");
    }
   }
  }else if(r.phase==Phase::Arrival){
   if(r.arrivalStage==0){auto goal=Global(r.map,route.back());
    if(std::hypot(r.x-goal.first,r.y-goal.second)<=300){r.arrivalStage=1;r.vehicle.speed=0;r.changed=Now();r.changedTick=now;}
    else if(Now()-r.changed>30)Fail(r,"CV_UNAVAILABLE");
    else{size_t closest=r.waypoint;float best=1e20f;for(size_t index=r.waypoint>2?r.waypoint-2:0;index<std::min(route.size(),r.waypoint+15);index++){auto point=Global(r.map,route[index]);float distance=std::hypot(r.x-point.first,r.y-point.second);if(distance<best){best=distance;closest=index;}}r.waypoint=std::max(r.waypoint,closest);
     auto* region=Region(r.map);float desired=RoadHeading(route,r.waypoint,(r.x-region->sx)*.01f,(r.y-region->sy)*.01f,r.vehicle.speed);r.vehicle.Step(dt,0,desired,false);float angle=r.vehicle.yaw*float(M_PI)/180,nx=r.x+std::sin(angle)*r.vehicle.speed*dt*100,ny=r.y-std::cos(angle)*r.vehicle.speed*dt*100;
     if(Walk(r.map,int(nx),int(ny))){r.x=nx;r.y=ny;M::Sync(r.cart,int(nx),int(ny));M::SetRotation(r.cart,r.vehicle.yaw);NetworkSyncSystem::BroadcastSyncPacket(g_registry,r.cart);}else r.vehicle.speed=0;SyncRiders(r);
    }
   }else if(Now()-r.changed>=DialogueTiming::ArrivalEnd){for(auto& m:r.members){auto p=Player(m);if(Valid(p)&&m.mode>=0&&!m.missing&&P::GetMapIndex(p)==r.map){P::SetQuestFlag(p,"caravan_food.completed",1);Notice(p,"CV_COMPLETE");}CleanupMember(r,m);}SetPhase(r,Phase::Complete);}}
  else if(r.phase==Phase::Failed){if(Now()-r.changed>=6)for(auto& m:r.members)CleanupMember(r,m);if(Now()-r.changed>=150)remove=true;}
  else if(r.phase==Phase::Complete||r.phase==Phase::Later||r.phase==Phase::Declined){float duration=r.phase==Phase::Declined?DialogueTiming::DeclineEnd:r.phase==Phase::Later?DialogueTiming::LaterEnd:8.f;if(Now()-r.changed>=duration)remove=true;}
  if(remove){Retire(r);it=runs.erase(it);}else{Send(r);++it;}
 }
 // WarpSet removes sector membership immediately, but wait for every player to
 // leave logically too. DestroyPrivateMap must never disconnect a returning PC.
 for(auto it=draining.begin();it!=draining.end();){bool occupied=false;for(auto p:g_registry.view<ecs::PlayerID>())if(P::GetMapIndex(p)==it->first){occupied=true;ReturnHome(p);}
  if(!occupied){SECTREE_MANAGER::instance().DestroyPrivateMap(it->first);it=draining.erase(it);}else ++it;}
}
void OnDeath(entt::entity victim,entt::entity){if(!Valid(victim))return;if(auto* actor=g_registry.try_get<Actor>(victim))if(actor->role==1){auto it=runs.find(actor->run);if(it!=runs.end())Fail(it->second,"CV_DESTROYED");}}
}
ACMD(do_caravan){CaravanQuest::Command(character,argument);}
