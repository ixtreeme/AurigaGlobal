#include "stdafx.h"
#include "DungeonUi.hpp"
#include "DungeonUiEvents.hpp"
#include "DungeonUiRules.hpp"
#include "dungeon.h"
#include "../core/config.h"
#include "../core/utils.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/db.h"
#include "../network/desc.h"
#include "../quest/questmanager.h"
#include "../quest/questlua.h"
#include "../social/party.h"
#include "../item/TypedItemOps.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedDungeonEntry.hpp"
#include <item/DungeonEntryBindings.hpp>
#include "../world/LocalWarp.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include <openssl/rand.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <sstream>

namespace DungeonUi {
namespace {
namespace P=ecs::PlayerRuntime;
using Message=std::unique_ptr<SQLMsg>;
Message Query(const std::string& q,const SqlParams& p={}){return Message(DBManager::instance().DirectQueryPrepared(q.c_str(),p));}
bool Good(const Message& m){return m&&!m->uiSQLErrno&&m->Get();}
std::uint64_t N(const char* s){return s?std::strtoull(s,nullptr,10):0;}
std::string Table(const char* s){return std::string(s)+get_table_postfix();}
std::uint64_t Now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::string Hex(std::string_view s){static const char* digits="0123456789abcdef";std::string r;for(unsigned char c:s){r+=digits[c>>4];r+=digits[c&15];}return r.empty()?"-":r;}
void Send(entt::entity e,const std::string& line){if(g_registry.valid(e)&&P::GetDesc(e))ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"%s",line.c_str());}
void Error(entt::entity e,const char* reason){Send(e,std::string("nx_dungeon_error 1 ")+reason);}
std::int64_t DisplayGold(entt::entity e){if(const auto* t=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr){for(const auto& w:t->state.wallets)if(w.currency==auriga::item::WalletCurrency::Gold)return static_cast<std::int64_t>(w.balance);return 0;}return ecs::PointSystem::GetGold(e);}
std::uint64_t EntryCount(entt::entity e,unsigned map,unsigned legacyVisual)
{
    if(HasTypedDungeonInventory(e))
    {
        const auto* binding=auriga::item::DungeonEntryForMap(map);
        return binding?TypedDungeonTicketCount(e,binding->ticketCode)+TypedDungeonTicketCount(e,binding->alternateTicketCode):0;
    }
    return legacyVisual?ItemSystem::CountItem(e,legacyVisual):0;
}
struct Entry {unsigned map=0,status=0,type=1,organization=0,min=1,max=120,partyMin=1,partyMax=8,x=0,y=0,cooldown=0,duration=0,entrance=0,strength=0,resistance=0,item=0,count=0;};
std::vector<Entry> Catalog(){
    std::vector<Entry> entries;auto* q=quest::CQuestManager::instance_ptr();auto* l=q?q->GetLuaState():nullptr;if(!l)return entries;
    const int top=lua_gettop(l);lua_getglobal(l,"dungeonInfo");
    if(lua_istable(l,-1)){lua_pushstring(l,"table");lua_gettable(l,-2);
        if(lua_istable(l,-1))for(int i=1;i<=std::min(128,luaL_getn(l,-1));++i){lua_rawgeti(l,-1,i);if(lua_istable(l,-1)){
            auto value=[&](const char* key,unsigned fallback=0){lua_pushstring(l,key);lua_gettable(l,-2);unsigned v=lua_isnumber(l,-1)?static_cast<unsigned>(std::max(0.0,lua_tonumber(l,-1))):fallback;lua_pop(l,1);return v;};
            auto pair=[&](const char* key,unsigned& a,unsigned& b){lua_pushstring(l,key);lua_gettable(l,-2);if(lua_istable(l,-1)){lua_rawgeti(l,-1,1);a=static_cast<unsigned>(std::max(0.0,lua_tonumber(l,-1)));lua_pop(l,1);lua_rawgeti(l,-1,2);b=static_cast<unsigned>(std::max(0.0,lua_tonumber(l,-1)));lua_pop(l,1);}lua_pop(l,1);};
            Entry e;e.map=value("map_index");e.status=value("status");e.type=value("type",1);e.organization=value("organization");pair("level_limit",e.min,e.max);pair("party_member_limit",e.partyMin,e.partyMax);pair("map_coords",e.x,e.y);pair("required_item",e.item,e.count);e.cooldown=value("cooldown");e.duration=value("duration");e.entrance=value("entrance_map_index");e.strength=value("strength_bonus");e.resistance=value("resistance_bonus");
            if(!e.min)e.min=1;if(!e.max)e.max=120;if(!e.partyMin)e.partyMin=1;if(!e.partyMax)e.partyMax=8;
            if(e.map&&e.x&&e.y&&e.x<20000000&&e.y<20000000)entries.push_back(e);
        }lua_pop(l,1);}}
    lua_settop(l,top);return entries;
}
const std::map<unsigned,std::string> cooldownFlags={{367,"monkey_tower_razor93"},{18,"owl_zone_razor93"},{355,"dungeonorchi"},{27,"slime_dungeon"},{217,"dungeon_spider"},{66,"deviltower_zone"},{218,"rune_zone"},{216,"catacombe"},{208,"dragonlair_zone"},{212,"meleylair_zone"},{351,"razador_zone"},{352,"nemere_zone"},{209,"temple_ochao"},{210,"enchanted_forest"},{373,"nightmare_zone_razor93_prepare"},{357,"dungeonpyramid_razor93"},{369,"tritontemple_razor93"},{161,"halloween2022"},{179,"vikingdungeon_zone"}};
unsigned Wait(entt::entity e,unsigned map){const auto f=cooldownFlags.find(map);return f==cooldownFlags.end()?0:static_cast<unsigned>(std::max<std::int64_t>(0,ecs::QuestSystem::GetFlag(e,f->second+".cooldown")));}
bool Open(entt::entity e,const Entry& d){return EntryOpen(d.status,ecs::PointSystem::GetLevel(e),d.min,d.max,Wait(e,d.map),Now()/1000);}
bool CanTravel(entt::entity e){return P::CanWarp(e)&&!P::IsObserverMode(e)&&!CombatSystem::IsDead(e)&&!CombatSystem::IsStun(e)&&!g_registry.all_of<ecs::TypedItemBusy>(e);}
struct Member {unsigned pid=0,level=0,race=0,deaths=0,oldBest=0;bool record=false;std::string name;std::uint64_t damage=0,healing=0,taken=0,highest=0,xp=0;std::int64_t gold=0,delta=0;std::map<std::pair<std::uint64_t,unsigned>,std::uint64_t> loot;};
struct Run {std::string id;unsigned map=0,base=0,elapsed=0,monsters=0,deaths=0,grade=0;std::uint64_t started=0,finished=0,due=0;bool saved=false,announced=false;std::map<unsigned,Member> members;};
std::map<int,Run> runs;
std::vector<Run> retired;
struct Recruit {unsigned map;std::uint64_t expires;};std::map<unsigned,Recruit> recruits;
struct Throttle {std::uint64_t next=0,mutation=0;};
Run* Current(entt::entity e){if(!g_registry.valid(e))return nullptr;auto found=runs.find(P::GetMapIndex(e));return found==runs.end()?nullptr:&found->second;}
Member* Participant(Run* run,entt::entity e){if(!run||!g_registry.valid(e)||!P::IsPC(e))return nullptr;auto m=run->members.find(P::GetPlayerID(e));return m==run->members.end()?nullptr:&m->second;}
void Enter(entt::entity e,int map){
    if(map<10000||!g_registry.valid(e)||!P::IsPC(e))return;auto& run=runs[map];if(run.finished)return;
    if(run.id.empty()){std::array<unsigned char,16> bytes{};if(RAND_bytes(bytes.data(),16)!=1){runs.erase(map);return;}run.id=std::string(reinterpret_cast<char*>(bytes.data()),16);run.map=map;run.base=map/10000;run.started=Now();}
    const auto pid=P::GetPlayerID(e);if(run.members.contains(pid))return;if(run.members.size()>=64)return;
    Member member;member.pid=pid;member.level=ecs::PointSystem::GetLevel(e);member.race=P::GetRaceNum(e);member.name=P::GetName(e);member.gold=DisplayGold(e);
    auto best=Query("SELECT time FROM "+Table("dungeon_ranking")+" WHERE pid=? AND dungeon_index=?",{SqlParam::UInt(pid),SqlParam::UInt(run.base)});if(Good(best)&&best->Get()->uiNumRows)member.oldBest=static_cast<unsigned>(N(best->Get()->FetchRow()[0]));
    auto ownBest=Query("SELECT MIN(r.elapsed) FROM dungeon_ui_run r JOIN dungeon_ui_member m ON m.run_id=r.id WHERE r.dungeon=? AND m.pid=?",{SqlParam::UInt(run.base),SqlParam::UInt(pid)});if(Good(ownBest)&&ownBest->Get()->uiNumRows){auto previous=static_cast<unsigned>(N(ownBest->Get()->FetchRow()[0]));if(previous&&(!member.oldBest||previous<member.oldBest))member.oldBest=previous;}
    run.members.emplace(pid,std::move(member));
}
void Complete(int map){auto found=runs.find(map);if(found==runs.end()||found->second.finished)return;auto& r=found->second;r.finished=Now();r.elapsed=std::max<std::uint64_t>(1,(r.finished-r.started)/1000);r.due=r.finished+2000;unsigned limit=0;for(const auto& d:Catalog())if(d.map==r.base)limit=d.duration;
    for(auto it=r.members.begin();it!=r.members.end();){auto e=P::FindByPlayerID(it->first);if(!g_registry.valid(e)||P::GetMapIndex(e)!=map){it=r.members.erase(it);continue;}it->second.delta=DisplayGold(e)-it->second.gold;r.deaths+=it->second.deaths;++it;}
    r.grade=Grade(r.elapsed,limit,r.deaths);for(auto& [pid,m]:r.members)m.record=!m.oldBest||r.elapsed<m.oldBest;
}
void Damage(entt::entity a,entt::entity v,std::uint64_t amount){auto* r=Current(v);if(!r||r->finished||!g_registry.valid(a)||P::GetMapIndex(a)!=P::GetMapIndex(v))return;
    if(!P::IsPC(v)){if(auto* m=Participant(r,a)){m->damage=Add(m->damage,amount);m->highest=std::max(m->highest,amount);}}
    else if(auto* m=Participant(r,v))m->taken=Add(m->taken,amount);
}
void Death(entt::entity v){auto* r=Current(v);if(!r||r->finished)return;if(auto* m=Participant(r,v))++m->deaths;else if(!P::IsPC(v))++r->monsters;}
void Healing(entt::entity a,entt::entity v,std::uint64_t amount){auto* r=Current(a);if(!r||r->finished||!Participant(r,v)||P::GetMapIndex(v)!=r->map||!AffectSystem::IsLoaded(v))return;if(auto* m=Participant(r,a))m->healing=Add(m->healing,amount);}
void Loot(std::string_view id,unsigned pid,std::uint64_t key,unsigned level,unsigned count){
    if(id.size()!=16)return;
    auto credit=[&](Run& run){if(run.id!=id)return false;auto member=run.members.find(pid);if(member==run.members.end())return true;auto& n=member->second.loot[{key,level}];n=Add(n,count);if(run.finished){run.saved=false;run.due=std::max(run.due,Now()+500);}return true;};
    for(auto& [map,run]:runs)if(credit(run))return;for(auto& run:retired)if(credit(run))return;
    // A confirmed grant can outlive the character login and retired instance.
    // Attribute it to its captured server run, never to a later dungeon/map.
    Query("INSERT INTO dungeon_ui_loot(run_id,pid,item_key,refinement,count) SELECT run_id,pid,?,?,? FROM dungeon_ui_member WHERE run_id=? AND pid=? ON DUPLICATE KEY UPDATE count=count+VALUES(count)",{SqlParam::UInt(key),SqlParam::UInt(level),SqlParam::UInt(count),SqlParam::Blob(std::string(id)),SqlParam::UInt(pid)});
}
bool Persist(Run& r){
    SqlTransaction tx;tx.push_back({"INSERT INTO dungeon_ui_run(id,dungeon,started_at,completed_at,elapsed,monsters,deaths,grade) VALUES(?,?,TIMESTAMPADD(MICROSECOND,?*1000,CAST('1970-01-01' AS DATETIME)),TIMESTAMPADD(MICROSECOND,?*1000,CAST('1970-01-01' AS DATETIME)),?,?,?,?) ON DUPLICATE KEY UPDATE id=VALUES(id)",{SqlParam::Blob(r.id),SqlParam::UInt(r.base),SqlParam::UInt(r.started),SqlParam::UInt(r.finished),SqlParam::UInt(r.elapsed),SqlParam::UInt(r.monsters),SqlParam::UInt(r.deaths),SqlParam::UInt(r.grade)},{},{}});
    for(auto& [pid,m]:r.members){auto e=P::FindByPlayerID(pid);auto active=runs.find(r.map);if(active!=runs.end()&&active->second.id==r.id&&g_registry.valid(e)&&P::GetMapIndex(e)==r.map)m.delta=DisplayGold(e)-m.gold;
        tx.push_back({"INSERT INTO dungeon_ui_member(run_id,pid,name,level,race,damage,healing,taken,highest,deaths,experience,yang_delta,is_record) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE experience=VALUES(experience),yang_delta=VALUES(yang_delta)",{SqlParam::Blob(r.id),SqlParam::UInt(pid),SqlParam::Blob(m.name),SqlParam::UInt(m.level),SqlParam::UInt(m.race),SqlParam::UInt(m.damage),SqlParam::UInt(m.healing),SqlParam::UInt(m.taken),SqlParam::UInt(m.highest),SqlParam::UInt(m.deaths),SqlParam::UInt(m.xp),SqlParam::Int(m.delta),SqlParam::UInt(m.record?1:0)},{},{}});
        for(auto [key,count]:m.loot)tx.push_back({"INSERT INTO dungeon_ui_loot(run_id,pid,item_key,refinement,count) VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE count=VALUES(count)",{SqlParam::Blob(r.id),SqlParam::UInt(pid),SqlParam::UInt(key.first),SqlParam::UInt(key.second),SqlParam::UInt(count)},{},{}});
    }
    Message result(DBManager::instance().DirectTransactionPrepared(std::move(tx)));return result&&result->transactionOutcome==SqlTransactionOutcome::Committed;
}
void Info(entt::entity e){auto catalog=Catalog();if(catalog.empty()){Error(e,"unavailable");return;}Send(e,"CleanDungeonInfo");for(const auto& d:catalog){
    std::uint64_t completed=0,fastest=0,highest=0;auto m=Query("SELECT completed,time,damage FROM "+Table("dungeon_ranking")+" WHERE pid=? AND dungeon_index=?",{SqlParam::UInt(P::GetPlayerID(e)),SqlParam::UInt(d.map)});if(Good(m)&&m->Get()->uiNumRows){auto row=m->Get()->FetchRow();completed=N(row[0]);fastest=N(row[1]);highest=N(row[2]);}
    auto recent=Query("SELECT COUNT(*),MIN(r.elapsed),MAX(m.highest) FROM dungeon_ui_member m JOIN dungeon_ui_run r ON r.id=m.run_id WHERE m.pid=? AND r.dungeon=?",{SqlParam::UInt(P::GetPlayerID(e)),SqlParam::UInt(d.map)});if(Good(recent)&&recent->Get()->uiNumRows){auto row=recent->Get()->FetchRow();completed=std::max(completed,N(row[0]));const auto best=N(row[1]);if(best&&(!fastest||best<fastest))fastest=best;highest=std::max(highest,N(row[2]));}
    std::ostringstream line;line<<"UpdateDungeonInfo "<<d.status<<' '<<Wait(e,d.map)<<' '<<d.type<<' '<<d.organization<<' '<<d.min<<' '<<d.max<<' '<<d.partyMin<<' '<<d.partyMax<<' '<<d.map<<' '<<d.x<<' '<<d.y<<' '<<d.cooldown<<' '<<d.duration<<' '<<d.entrance<<' '<<d.strength<<' '<<d.resistance<<' '<<d.item<<' '<<d.count<<' '<<completed<<' '<<fastest<<' '<<highest;Send(e,line.str());
    Send(e,auriga::item::DungeonEntryCountCommand(d.map,d.item?EntryCount(e,d.map,d.item):0));
    }Send(e,"nx_dungeon_ready 1 "+std::to_string(P::GetPlayerID(e))+" "+std::to_string(Now()/1000));
}
std::string Eligible(){return " FROM dungeon_ui_member m JOIN dungeon_ui_run r ON r.id=m.run_id JOIN "+Table("player")+" p ON p.id=m.pid JOIN account.account a ON a.id=p.account_id WHERE r.dungeon=? AND r.completed_at>=DATE_SUB(UTC_DATE(),INTERVAL WEEKDAY(UTC_DATE()) DAY) AND a.status='OK' AND p.name NOT IN(SELECT mName FROM common.gmlist) ";}
void Ranking(entt::entity e,unsigned map,unsigned metric){
    const char* order=metric==1?"completed DESC,elapsed ASC,pid ASC":metric==3?"highest DESC,elapsed ASC,pid ASC":"elapsed ASC,highest DESC,pid ASC";
    const std::string grouped="SELECT m.pid,HEX(p.name) name,p.level,p.job race,COUNT(*) completed,MIN(r.elapsed) elapsed,MAX(m.highest) highest"+Eligible()+" GROUP BY m.pid,p.name,p.level,p.job";
    auto rows=Query("SELECT pid,name,level,race,completed,elapsed,highest,ROW_NUMBER() OVER(ORDER BY "+std::string(order)+") position FROM ("+grouped+") ranked ORDER BY "+order+" LIMIT 100",{SqlParam::UInt(map)});
    auto mine=Query("SELECT * FROM (SELECT pid,name,level,race,completed,elapsed,highest,ROW_NUMBER() OVER(ORDER BY "+std::string(order)+") position FROM ("+grouped+") grouped) ranked WHERE pid=?",{SqlParam::UInt(map),SqlParam::UInt(P::GetPlayerID(e))});
    if(!Good(rows)||!Good(mine)){Error(e,"unavailable");return;}
    Send(e,"nx_dungeon_rank_begin 1 "+std::to_string(map)+" "+std::to_string(metric));
    auto emit=[&](const Message& result,bool own){for(unsigned i=0;i<result->Get()->uiNumRows;++i){auto row=result->Get()->FetchRow();std::ostringstream line;line<<"nx_dungeon_rank_row 1 "<<(own?1:0)<<' '<<N(row[7])<<' '<<N(row[0])<<' '<<row[1]<<' '<<N(row[2])<<' '<<N(row[3])<<' '<<N(row[4])<<' '<<N(row[5])<<' '<<N(row[6]);Send(e,line.str());}};
    emit(rows,false);emit(mine,true);Send(e,"nx_dungeon_rank_end 1");
}
void Result(entt::entity e){
    Send(e,"nx_dungeon_ready 1 "+std::to_string(P::GetPlayerID(e))+" "+std::to_string(Now()/1000));
    auto head=Query("SELECT HEX(r.id),r.dungeon,r.elapsed,r.monsters,r.deaths,r.grade,m.experience,m.yang_delta,(SELECT COUNT(*) FROM dungeon_ui_member t WHERE t.run_id=r.id),m.is_record FROM dungeon_ui_run r JOIN dungeon_ui_member m ON m.run_id=r.id WHERE m.pid=? ORDER BY r.completed_at DESC,r.id DESC LIMIT 1",{SqlParam::UInt(P::GetPlayerID(e))});
    if(!Good(head)){Error(e,"unavailable");return;}if(!head->Get()->uiNumRows){Error(e,"no_result");return;}auto h=head->Get()->FetchRow();std::string id=h[0];
    auto members=Query("SELECT pid,HEX(name),level,race,damage,healing,taken,highest,deaths FROM dungeon_ui_member WHERE run_id=UNHEX(?) ORDER BY damage DESC,pid ASC LIMIT 64",{SqlParam::Text(id)});
    auto loot=Query("SELECT item_key,refinement,count FROM dungeon_ui_loot WHERE run_id=UNHEX(?) AND pid=? ORDER BY item_key,refinement LIMIT 128",{SqlParam::Text(id),SqlParam::UInt(P::GetPlayerID(e))});
    if(!Good(members)||!Good(loot)){Error(e,"unavailable");return;}
    std::ostringstream line;line<<"nx_dungeon_result_begin 1 "<<id;for(unsigned n=1;n<10;++n)line<<' '<<h[n];Send(e,line.str());
    for(unsigned i=0;i<members->Get()->uiNumRows;++i){auto row=members->Get()->FetchRow();std::ostringstream body;body<<"nx_dungeon_result_member 1";for(int n=0;n<9;++n)body<<' '<<row[n];Send(e,body.str());}
    for(unsigned i=0;i<loot->Get()->uiNumRows;++i){auto row=loot->Get()->FetchRow();Send(e,"nx_dungeon_result_loot 1 "+std::string(row[0])+" "+row[1]+" "+row[2]);}
    Send(e,"nx_dungeon_result_end 1");
}
void Team(entt::entity e,const Entry& d,bool toggle){
    auto pid=P::GetPlayerID(e);if(toggle){if(recruits.contains(pid)&&recruits.at(pid).map==d.map)recruits.erase(pid);else{if(!Open(e,d)||P::GetMapIndex(e)>=10000){Error(e,"blocked");return;}recruits[pid]={d.map,Now()+300000};}}
    Send(e,"nx_dungeon_team_begin 1 "+std::to_string(d.map)+" "+std::to_string(recruits.contains(pid)&&recruits[pid].map==d.map?1:0));
    unsigned count=0;for(auto [id,r]:recruits){if(r.map!=d.map||r.expires<=Now())continue;auto peer=P::FindByPlayerID(id);if(!g_registry.valid(peer)||!P::GetDesc(peer)||P::GetMapIndex(peer)>=10000||!Open(peer,d)||P::GetEmpire(peer)!=P::GetEmpire(e))continue;
        auto leader=ecs::SocialSystem::GetPartyLeader(peer);if(leader!=entt::null&&leader!=peer)continue;
        Send(e,"nx_dungeon_team_row 1 "+std::to_string(id)+" "+Hex(P::GetName(peer))+" "+std::to_string(ecs::PointSystem::GetLevel(peer))+" "+std::to_string(P::GetRaceNum(peer)));if(++count>=32)break;
    }Send(e,"nx_dungeon_team_end 1");
}
}
void Tick(){
    Events::enter=&Enter;Events::complete=&Complete;Events::reset=+[](int map){auto it=runs.find(map);if(it!=runs.end()){if(it->second.finished&&!it->second.saved)retired.push_back(std::move(it->second));runs.erase(it);}};Events::damage=&Damage;Events::death=&Death;Events::healing=&Healing;Events::loot=&Loot;
    Events::context=+[](entt::entity e){auto* r=Current(e);return Participant(r,e)?r->id:std::string{};};
    Events::experience=+[](entt::entity e,std::uint64_t n){auto* r=Current(e);if(auto* m=Participant(r,e)){m->xp=Add(m->xp,n);if(r->finished){r->saved=false;r->due=std::max(r->due,Now()+500);}}};
    const auto now=Now();for(auto& [map,r]:runs)if(r.finished&&!r.saved&&now>=r.due){r.saved=Persist(r);r.due=now+5000;if(r.saved&&!r.announced){r.announced=true;for(auto [pid,m]:r.members){auto e=P::FindByPlayerID(pid);if(g_registry.valid(e))Send(e,"nx_dungeon_completed 1 "+Hex(r.id));}}}
    for(auto it=retired.begin();it!=retired.end();)if(now>=it->due){if(Persist(*it)){for(auto [pid,m]:it->members){auto e=P::FindByPlayerID(pid);if(g_registry.valid(e))Send(e,"nx_dungeon_completed 1 "+Hex(it->id));}it=retired.erase(it);}else{it->due=now+5000;++it;}}else ++it;
    for(auto it=recruits.begin();it!=recruits.end();){auto e=P::FindByPlayerID(it->first);if(it->second.expires<=now||!g_registry.valid(e)||!P::GetDesc(e)||P::GetMapIndex(e)>=10000)it=recruits.erase(it);else ++it;}
}
void Handle(entt::entity e,const char* argument){
    if(!g_registry.valid(e)||!P::GetDesc(e))return;std::istringstream input(argument?argument:"");std::string action,a,b,extra;input>>action>>a>>b>>extra;
    auto& throttle=g_registry.get_or_emplace<Throttle>(e);auto now=Now();if(now<throttle.next){Error(e,"throttle");return;}throttle.next=now+150;
    if(action=="info"&&a.empty()){Info(e);return;}if(action=="result"&&a.empty()){Result(e);return;}
    unsigned map=0,metric=0;if(!Parse(a,map)||!extra.empty()){Error(e,"invalid");return;}auto catalog=Catalog();auto d=std::ranges::find(catalog,map,&Entry::map);if(d==catalog.end()){Error(e,"invalid");return;}
    if(action=="rank"&&Parse(b,metric)&&metric>=1&&metric<=3){Ranking(e,map,metric);return;}
    if(action=="teams"&&b.empty()){Team(e,*d,false);return;}
    if(action=="team"&&b.empty()){if(now<throttle.mutation){Error(e,"throttle");return;}throttle.mutation=now+1000;Team(e,*d,true);return;}
    if(action=="join"&&Parse(b,metric)){
        auto r=recruits.find(metric);auto peer=P::FindByPlayerID(metric);if(r==recruits.end()||r->second.map!=map||r->second.expires<=now||peer==e||!g_registry.valid(peer)||!CanTravel(e)||!Open(e,*d)||!Open(peer,*d)){Error(e,"blocked");return;}
        auto own=ecs::SocialSystem::GetPartyLeader(e),other=ecs::SocialSystem::GetPartyLeader(peer);if(own!=entt::null&&own!=e){Error(e,"blocked");return;}
        if(own==e){if(other!=entt::null){Error(e,"blocked");return;}ecs::SocialSystem::PartyInvite(e,peer);}else if(other==peer){if(!ecs::SocialSystem::RequestToParty(e,peer)){Error(e,"blocked");return;}}else ecs::SocialSystem::PartyInvite(e,peer);
        Send(e,"nx_dungeon_action 1 invited");return;
    }
    if(action=="warp"&&b.empty()){
        if(now<throttle.mutation||!CanTravel(e)||!Open(e,*d)){Error(e,"blocked");return;}
        if(P::GetMapIndex(e)>=10000){auto* r=Current(e);if(!r||!r->finished){Error(e,"blocked");return;}}
        throttle.mutation=now+5000;if(!ecs::MovementSystem::WarpSetOnCurrentCore(e,static_cast<int>(d->x*100),static_cast<int>(d->y*100))){Error(e,"warp");return;}recruits.erase(P::GetPlayerID(e));Send(e,"nx_dungeon_action 1 warped");return;
    }
    if(action=="retry"&&b.empty()){
        auto* r=Current(e);if(now<throttle.mutation||!r||!r->finished||r->base!=map||!r->members.contains(P::GetPlayerID(e))){Error(e,"blocked");return;}
        auto party=ecs::SocialSystem::GetParty(e);if(party!=entt::null&&PartySystem::GetLeaderPID(party)!=P::GetPlayerID(e)){Error(e,"leader");return;}
        std::vector<entt::entity> members;bool ready=true;
        for(auto [pid,m]:r->members){auto member=P::FindByPlayerID(pid);if(!g_registry.valid(member)||!P::GetDesc(member)||P::GetMapIndex(member)!=P::GetMapIndex(e)||ecs::SocialSystem::GetParty(member)!=party||!CanTravel(member)||!Open(member,*d)){ready=false;break;}members.push_back(member);}
        if(!ready||members.empty()){Error(e,"team_not_ready");return;}
        if(party!=entt::null){unsigned online=0;auto count=[&](entt::entity){++online;};PartySystem::ForEachOnlineMember(party,count);if(online!=members.size()){Error(e,"team_not_ready");return;}}
        throttle.mutation=now+5000;for(auto member:members)if(!ecs::MovementSystem::WarpSetOnCurrentCore(member,static_cast<int>(d->x*100),static_cast<int>(d->y*100)))Error(member,"warp");
        Send(e,"nx_dungeon_action 1 warped");return;
    }
    if(action=="exit"&&b.empty()){auto* r=Current(e);if(!r||!r->finished||r->base!=map||!CanTravel(e)){Error(e,"blocked");return;}ecs::MovementSystem::ExitToSavedLocation(e);Send(e,"nx_dungeon_action 1 exited");return;}
    Error(e,"invalid");
}
}
