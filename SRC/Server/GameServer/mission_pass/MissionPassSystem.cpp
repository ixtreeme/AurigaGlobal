#include "../core/stdafx.h"
#include "MissionPassSystem.hpp"
#include "MissionPassState.hpp"
#include "MissionPassEvents.hpp"
#include "../core/JsonFile.h"
#include "../core/locale_service.h"
#include "../core/utils.h"
#include "../command/cmd.h"
#include "../network/db.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../guild/guild.h"
#include <chrono>
#include <deque>
#include <sstream>
#include <unordered_map>
#include <thread>
#include <random>

extern bool g_bShutdown;

namespace AurigaMissionPass {
namespace {
Season season;
bool loaded=false,draining=false;
struct Account {
    State state;
    uint64_t revision=0;
    uint32_t pid=0;
    bool ready=false,busy=false,open=false;
    int64_t retryAt=0,lastActive=0,lastMinute=0,lastRequest=0,lastSeen=0,flushAt=0,lastSent=0;
    std::deque<Event> events;
    std::vector<Reward> rewards;
    size_t inFlight=0;
    std::string token;
    std::vector<ProgressKey> completions;
    std::deque<std::pair<uint32_t,uint32_t>> claims;
};
std::map<uint32_t,Account> accounts;
struct Run {
    std::set<uint32_t> participants;
    std::map<uint32_t,uint32_t> guilds;
    bool completed=false;
};
std::map<int32_t,Run> runs;
uint64_t serial=0;
const std::string boot=std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+"_"+std::to_string(std::random_device{}());
int64_t Now(){return get_global_time();}
entt::entity Player(uint32_t account) {
    auto found=accounts.find(account);if(found==accounts.end())return entt::null;
    auto player=ecs::PlayerRuntime::FindByPlayerID(found->second.pid);
    return g_registry.valid(player) && ecs::PlayerRuntime::GetAccountID(player)==account ? player : entt::null;
}
void Notice(uint32_t account,const char* key) {
    auto player=Player(account);if(player!=entt::null)ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"amp_notice %s",key);
}
bool Load() {
    loaded=true;
    nlohmann::json doc;
    if(!JsonFile::Load(LocaleService_GetBasePath()+"/mission_pass.json",doc))return false;
    try {season=ReadConfig(doc);LOG_INFO("MISSION_PASS_CONFIG season={} enabled={}",season.id,season.enabled);return true;}
    catch(const std::exception& e){LOG_ERROR("MISSION_PASS_CONFIG {}",e.what());season.enabled=false;return false;}
}
void Snapshot(uint32_t account) {
    auto& a=accounts.at(account);auto player=Player(account);
    if(!a.open || player==entt::null || !a.ready)return;
    a.lastSent=Now();
    nlohmann::json doc={{"available",season.Active(Now())},{"preview",false},{"title_key",season.titleKey},
        {"levels",season.levels},{"xp_per_level",season.xpPerLevel},{"xp",a.state.xp},
        {"premium",a.state.premiumActivatedAt>=season.startsAt && a.state.premiumActivatedAt<=Now() && a.state.premiumActivatedAt>0},{"ends_at",season.endsAt},{"server_utc",Now()},
        {"missions",nlohmann::json::array()},{"rewards",nlohmann::json::array()}};
    for(const auto& m:season.missions) {
        auto p=a.state.progress.find({m.id,Period(season,m.cadence,Now())});
        doc["missions"].push_back({{"id",m.id},{"name_key",m.nameKey},{"description_key",m.descriptionKey},
            {"cadence",m.cadence==Cadence::Daily?"daily":m.cadence==Cadence::Weekly?"weekly":"season"},
            {"goal",m.goal},{"xp",m.xp},{"progress",p==a.state.progress.end()?0:p->second.value}});
    }
    for(const auto& r:a.rewards)doc["rewards"].push_back({{"level",r.level},{"track",r.track==Track::Free?0:1},
        {"item",r.item},{"count",r.count},{"claimed",a.state.reservedRewards.count({r.level,r.track})!=0}});
    const auto bytes=doc.dump();if(bytes.size()>60000){LOG_ERROR("MISSION_PASS snapshot exceeds bound");return;}
    const auto id=++serial;
    ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"amp_begin 1 %llu %zu",static_cast<unsigned long long>(id),bytes.size());
    static constexpr char digits[]="0123456789abcdef";
    for(size_t start=0,index=0;start<bytes.size();start+=120,++index) {
        std::string hex;for(size_t i=start;i<std::min(start+120,bytes.size());++i){auto c=static_cast<unsigned char>(bytes[i]);hex+=digits[c>>4];hex+=digits[c&15];}
        ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"amp_chunk %llu %zu %s",static_cast<unsigned long long>(id),index,hex.c_str());
    }
    ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"amp_end %llu",static_cast<unsigned long long>(id));
}
bool ReadResult(uint32_t account,SQLMsg* result,std::string& status) {
    if(!result || result->uiSQLErrno || !result->Get() || !result->Get()->uiNumRows ||
        mysql_num_fields(result->Get()->pSQLResult)<7)return false;
    auto row=result->Get()->FetchRow();if(!row || !row[0])return false;
    status=row[0];
    try {
        auto& a=accounts.at(account);
        a.state=LoadState(season,account,row[1]?row[1]:"");
        a.revision=std::stoull(row[2]);a.state.premiumActivatedAt=std::stoll(row[3]);
        if(a.state.xp!=std::stoull(row[4]))throw std::runtime_error("XP mismatch");
        for(const auto& r:nlohmann::json::parse(row[5]?row[5]:"[]"))
            a.state.reservedRewards.emplace(r[0].get<uint32_t>(),r[1].get<int>()==0?Track::Free:Track::Premium);
        a.rewards.clear();
        for(const auto& r:nlohmann::json::parse(row[6]?row[6]:"[]"))
            a.rewards.push_back({r[0].get<uint32_t>(),r[1].get<int>()==0?Track::Free:Track::Premium,r[2].get<uint32_t>(),r[3].get<uint32_t>()});
        a.ready=true;return true;
    }catch(const std::exception& e){LOG_ERROR("MISSION_PASS_LOAD account={} {}",account,e.what());return false;}
}
void Refresh(uint32_t account) {
    auto& a=accounts.at(account);a.busy=true;
    DBManager::instance().FuncQueryText([account](SQLMsg* result){
        auto& current=accounts.at(account);current.busy=false;std::string status;
        if(!ReadResult(account,result,status)){current.retryAt=Now()+5;Notice(account,"MP_DATABASE_ERROR");return;}
        Snapshot(account);
    },"CALL mission_pass_snapshot("+std::to_string(account)+","+std::to_string(season.id)+")");
}
void Commit(uint32_t account) {
    auto& a=accounts.at(account);
    const bool newBatch=a.token.empty();
    if(newBatch){a.token=boot+"_"+std::to_string(++serial);a.inFlight=std::min<size_t>(128,a.events.size());}
    State next=a.state;
    for(size_t i=0;i<a.inFlight;++i){auto e=a.events[i];e.sequence=next.lastSequence+1;Apply(season,next,e,std::min(Now(),season.endsAt-1));}
    if(newBatch)a.completions=NewCompletions(a.state,next);
    a.busy=true;
    auto query="CALL mission_pass_commit("+std::to_string(account)+","+std::to_string(season.id)+","+
        std::to_string(a.revision)+","+SqlText(a.token)+","+SqlText(SaveState(next).dump())+")";
    DBManager::instance().FuncQueryText([account](SQLMsg* result){
        auto& current=accounts.at(account);current.busy=false;std::string status;
        if(!ReadResult(account,result,status)){current.retryAt=Now()+5;return;}
        if(status=="ok"){
            const auto player=Player(account);
            if(player!=entt::null)for(const auto& key:current.completions){
                const auto progress=current.state.progress.find(key);
                const auto mission=std::find_if(season.missions.begin(),season.missions.end(),[&](const Mission& m){return m.id==key.first;});
                if(progress!=current.state.progress.end() && progress->second.xpAwarded && mission!=season.missions.end())
                    ecs::ChatSystem::Send(player,CHAT_TYPE_COMMAND,"amp_complete %u %u %lld %u",season.id,key.first,static_cast<long long>(key.second),mission->xp);
            }
            for(size_t i=0;i<current.inFlight;++i)current.events.pop_front();
            current.inFlight=0;current.token.clear();current.completions.clear();Snapshot(account);
            current.flushAt=Now()+3;
        }else if(status=="conflict"){
            // Rebase on the authoritative state with a fresh receipt identity;
            // completions another core already persisted must not be replayed.
            current.token.clear();current.inFlight=0;current.completions.clear();
        }else current.retryAt=Now()+5;
    },query);
}
}
void Record(entt::entity player,Kind kind,uint32_t subject,uint32_t amount,uint32_t targetLevel,uint32_t participants,bool guildParty,bool partyEligible) {
    if(!loaded)Load();
    if(g_bShutdown || !season.Active(Now()) || !g_registry.valid(player) || !ecs::PlayerRuntime::IsPC(player) || !amount)return;
    auto account=ecs::PlayerRuntime::GetAccountID(player);if(!account)return;
    auto& a=accounts[account];a.pid=ecs::PlayerRuntime::GetPlayerID(player);a.lastSeen=Now();
    if(kind!=Kind::ActiveMinute)a.lastActive=Now();
    if(a.events.size()>=8192){LOG_ERROR("MISSION_PASS queue saturated account={}",account);return;}
    Event event;event.account=account;event.occurredAt=Now();event.kind=kind;event.subject=subject;event.amount=amount;
    event.characterLevel=ecs::PointSystem::GetLevel(player);event.targetLevel=targetLevel;event.participants=participants;event.guildParty=guildParty;
    event.partyEligible=partyEligible;
    if(a.ready) {
        bool needed=false;
        for(const auto& m:season.missions)if(Matches(m,event)){
            auto p=a.state.progress.find({m.id,Period(season,m.cadence,Now())});
            if(p==a.state.progress.end() || !p->second.xpAwarded){needed=true;break;}
        }
        if(!needed)return;
    }
    a.events.push_back(event);
}
void OnKill(entt::entity player,entt::entity victim) {
    if(!g_registry.valid(victim) || ecs::PlayerRuntime::IsPC(victim))return;
    if(!g_registry.any_of<ecs::TagMonster,ecs::TagStone>(victim))return;
    Kind kind=ecs::PlayerRuntime::IsStone(victim)?Kind::MetinKill:ecs::PlayerRuntime::GetMobRank(victim)>=4?Kind::BossKill:Kind::MobKill;
    Record(player,kind,ecs::PlayerRuntime::GetRaceNum(victim),1,ecs::PointSystem::GetLevel(victim));
}
void DungeonEnter(entt::entity player,int32_t map) {
    if(map<10000 || !ecs::PlayerRuntime::IsPC(player))return;
    auto& run=runs[map];auto id=ecs::PlayerRuntime::GetPlayerID(player);run.participants.insert(id);
    auto* guild=ecs::SocialSystem::GetGuild(player);run.guilds[id]=guild?guild->GetID():0;
}
void DungeonComplete(int32_t map,uint32_t type) {
    auto found=runs.find(map);if(found==runs.end() || found->second.completed)return;
    auto& run=found->second;run.completed=true;
    std::set<uint32_t> credited;
    for(auto pid:run.participants){auto player=ecs::PlayerRuntime::FindByPlayerID(pid);
        if(!g_registry.valid(player) || !ecs::PlayerRuntime::GetDesc(player) || ecs::PlayerRuntime::GetMapIndex(player)!=map)continue;
        if(!credited.insert(ecs::PlayerRuntime::GetAccountID(player)).second)continue;
        auto* guild=ecs::SocialSystem::GetGuild(player);auto party=ecs::SocialSystem::GetParty(player);
        bool guildParty=false,partyEligible=false;
        for(auto other:run.participants)if(other!=pid){auto teammate=ecs::PlayerRuntime::FindByPlayerID(other);
            if(!g_registry.valid(teammate) || !ecs::PlayerRuntime::GetDesc(teammate) || ecs::PlayerRuntime::GetMapIndex(teammate)!=map ||
                party==entt::null || ecs::SocialSystem::GetParty(teammate)!=party)continue;
            partyEligible=true;auto* otherGuild=ecs::SocialSystem::GetGuild(teammate);
            if(guild && otherGuild && guild->GetID()==otherGuild->GetID())guildParty=true;
        }
        Record(player,Kind::DungeonComplete,type,1,0,static_cast<uint32_t>(run.participants.size()),guildParty,partyEligible);
    }
}
void Tick() {
    Events::record=&Record;Events::kill=&OnKill;Events::enter=&DungeonEnter;Events::complete=&DungeonComplete;
    Events::reset=+[](int32_t map){runs.erase(map);};
    if(!loaded)Load();if(!season.enabled)return;
    const auto now=Now();
    // At most one active-time minute per account; no catch-up burst after lag.
    if(!g_bShutdown)for(auto player:g_registry.view<ecs::TagPC,ecs::PlayerID>()) {
        if(!ecs::PlayerRuntime::GetDesc(player))continue;
        auto id=ecs::PlayerRuntime::GetAccountID(player);auto found=accounts.find(id);
        if(found==accounts.end())continue;
        auto& a=found->second;a.lastSeen=now;a.pid=ecs::PlayerRuntime::GetPlayerID(player);
        if(!a.lastMinute)a.lastMinute=now;
        if(now-a.lastMinute>=60){a.lastMinute=now;if(now-a.lastActive<=90)Record(player,Kind::ActiveMinute);}
    }
    for(auto it=accounts.begin();it!=accounts.end();) {
        auto id=it->first;auto& a=it->second;
        if(a.busy || now<a.retryAt){++it;continue;}
        if(a.events.empty() && a.claims.empty() && now-a.lastSeen>300){it=accounts.erase(it);continue;}
        if(!a.ready){Refresh(id);++it;continue;}
        if(!a.events.empty()){if(draining || now>=a.flushAt)Commit(id);++it;continue;}
        if(a.open && now-a.lastSent>=30){Refresh(id);++it;continue;}
        if(!a.claims.empty()) {
            const size_t count=a.claims.size();a.busy=true;std::string query;
            for(const auto& claim:a.claims)query+="CALL mission_pass_claim("+std::to_string(id)+","+std::to_string(season.id)+","+std::to_string(claim.first)+","+std::to_string(claim.second)+");";
            DBManager::instance().FuncQueryText([id,count](SQLMsg* result){auto& c=accounts.at(id);c.busy=false;
                if(!result || result->uiSQLErrno){c.retryAt=Now()+5;return;}
                bool sent=false;std::string message="MP_CLAIMED";
                for(auto* set:result->vec_pkResult)if(set && set->uiNumRows){auto row=set->FetchRow();
                    if(row && row[0]){message=row[0];if(message=="MP_REWARD_SENT")sent=true;}}
                Notice(id,sent?"MP_REWARD_SENT":message.c_str());
                for(size_t i=0;i<count;++i)c.claims.pop_front();c.ready=false;
            },query);
        }
        ++it;
    }
}
void Flush() {
    if(!loaded || !season.enabled)return;
    draining=true;
    for(auto& [id,a]:accounts)a.flushAt=a.retryAt=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    auto pending=[](){for(const auto& [id,a]:accounts)if(a.busy || !a.events.empty() || !a.claims.empty())return true;return false;};
    while(pending() && std::chrono::steady_clock::now()<deadline){
        Tick();DBManager::instance().Process();std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(pending())LOG_ERROR("MISSION_PASS shutdown drain timed out; SQL unavailable");
    else LOG_INFO("MISSION_PASS shutdown drain complete");
    draining=false;
}
void HandleCommand(entt::entity player,const char* arguments) {
    if(!loaded)Load();if(!g_registry.valid(player) || !ecs::PlayerRuntime::GetDesc(player))return;
    const auto id=ecs::PlayerRuntime::GetAccountID(player);if(!id)return;
    auto& a=accounts[id];a.pid=ecs::PlayerRuntime::GetPlayerID(player);a.lastSeen=Now();
    std::istringstream input(arguments?arguments:"");std::string action;input>>action;
    if(action=="close"){a.open=false;return;}
    if(Now()-a.lastRequest<2)return;a.lastRequest=Now();
    if(!season.Active(Now())){Notice(id,"MP_OFFLINE");return;}
    if(action=="open"){a.open=true;if(!a.busy)Refresh(id);return;}
    if(action=="claim" || action=="claimall"){
        if(!a.ready || a.claims.size()>0)return;
        uint32_t level=0,track=0;
        if(action=="claim"){
            if(!(input>>level>>track) || track>1 || level<1 || level>season.levels)return;
            a.claims.emplace_back(level,track);
        } else for(const auto& r:a.rewards)
            if(Level(season,a.state)>=r.level && (r.track==Track::Free || a.state.premiumActivatedAt) &&
                !a.state.reservedRewards.count({r.level,r.track}))a.claims.emplace_back(r.level,r.track==Track::Free?0:1);
    }
}
}
ACMD(do_missionpass){AurigaMissionPass::HandleCommand(character,argument);}
