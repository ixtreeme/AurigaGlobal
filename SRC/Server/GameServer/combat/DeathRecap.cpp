#include "stdafx.h"
#include "DeathRecap.hpp"
#include "DeathRecapEvents.hpp"
#include "DeathRecapModel.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../network/desc.h"
#include "../world/sectree_manager.h"
#include <chrono>
#include <map>
#include <sstream>
#include <openssl/rand.h>

namespace DeathRecap {
namespace {
namespace P=ecs::PlayerRuntime;
struct Tracking {History history;bool frozen=false;std::uint64_t requested=0;};
struct Snapshot {std::uint64_t serial=0,at=0,maxHp=0,expires=0;int map=0,x=0,y=0;Source killer;std::vector<Hit> hits;unsigned omitted=0;};
std::map<std::uint32_t,Snapshot> recent;
std::uint64_t Now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::string Hex(std::string_view s){static const char* d="0123456789abcdef";std::string out;for(unsigned char c:s){out+=d[c>>4];out+=d[c&15];}return out.empty()?"-":out;}
Source Describe(entt::entity e){
    if(!g_registry.valid(e)||!ecs::IsCharacter(e))return {};
    return {std::string(P::GetName(e)),P::GetPacketVID(e),P::GetRaceNum(e),static_cast<std::uint32_t>(ecs::PointSystem::GetLevel(e)),P::IsPC(e)};
}
void Send(entt::entity e,const std::string& line){if(g_registry.valid(e)&&P::GetDesc(e))ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"%s",line.c_str());}
void Publish(entt::entity e,const Snapshot& s){
    const auto pid=P::GetPlayerID(e);Send(e,"nx_death_ready 1 "+std::to_string(pid));
    std::ostringstream head;head<<"nx_death_begin 1 "<<pid<<' '<<s.serial<<' '<<s.map<<' '<<s.x<<' '<<s.y<<' '<<s.maxHp<<' '<<s.killer.race<<' '<<s.killer.level<<' '<<s.killer.player<<' '<<Hex(s.killer.name)<<' '<<s.hits.size()<<' '<<s.omitted;Send(e,head.str());
    for(const auto& h:s.hits){std::ostringstream row;row<<"nx_death_hit 1 "<<s.serial<<' '<<std::min(WindowMs,s.at-h.at)<<' '<<h.before<<' '<<h.after<<' '<<h.maxHp<<' '<<h.Amount()<<' '<<h.source.vid<<' '<<h.source.race<<' '<<h.source.level<<' '<<h.source.player<<' '<<unsigned(h.type)<<' '<<h.skill<<' '<<h.critical<<' '<<h.dot<<' '<<Hex(h.source.name);Send(e,row.str());}
    Send(e,"nx_death_end 1 "+std::to_string(s.serial));
}
void Loss(entt::entity e,std::uint64_t before,std::uint64_t after,std::uint64_t maxHp,const Events::Context& context){
    if(!g_registry.valid(e)||!P::IsPC(e)||!P::GetDesc(e)||CombatSystem::IsDead(e))return;
    auto& tracking=g_registry.get_or_emplace<Tracking>(e);if(tracking.frozen)return;
    tracking.history.Record({Now(),before,after,maxHp,Describe(context.attacker),context.skill,context.type,context.critical&&*context.critical,context.dot});
}
void Death(entt::entity e,entt::entity killer){
    if(!g_registry.valid(e)||!P::IsPC(e))return;
    auto& tracking=g_registry.get_or_emplace<Tracking>(e);if(tracking.frozen)return;tracking.frozen=true;
    Snapshot s;if(RAND_bytes(reinterpret_cast<unsigned char*>(&s.serial),sizeof(s.serial))!=1||!s.serial)return;
    s.at=Now();s.expires=s.at+600000;s.maxHp=std::max<std::int64_t>(1,ecs::PointSystem::GetMaxHP(e));s.map=P::GetMapIndex(e);s.x=P::GetX(e)/100;s.y=P::GetY(e)/100;s.killer=Describe(killer);s.hits=tracking.history.Capture(s.at);s.omitted=tracking.history.omitted;
    if(auto* map=SECTREE_MANAGER::instance().GetMap(s.map)){s.x=(P::GetX(e)-map->m_setting.iBaseX)/100;s.y=(P::GetY(e)-map->m_setting.iBaseY)/100;}
    if(s.killer.name.empty()&&!s.hits.empty())s.killer=s.hits.back().source;
    if(recent.size()>=1024&&!recent.contains(P::GetPlayerID(e))){auto oldest=std::min_element(recent.begin(),recent.end(),[](const auto& a,const auto& b){return a.second.expires<b.second.expires;});recent.erase(oldest);}
    recent[P::GetPlayerID(e)]=s;Publish(e,s);
}
void Clear(entt::entity e){if(!g_registry.valid(e)||!P::IsPC(e))return;recent.erase(P::GetPlayerID(e));g_registry.remove<Tracking>(e);Send(e,"nx_death_clear 1");}
}
void Tick(){
    Events::loss=&Loss;Events::death=&Death;Events::clear=&Clear;
    const auto now=Now();for(auto it=recent.begin();it!=recent.end();)if(it->second.expires<=now)it=recent.erase(it);else ++it;
}
void Handle(entt::entity e,const char* argument){
    if(!g_registry.valid(e)||!P::IsPC(e)||!P::GetDesc(e))return;
    if(argument&&std::string_view(argument).find_first_not_of(" \t\r\n")!=std::string_view::npos){Send(e,"nx_death_status 1 invalid");return;}
    auto& tracking=g_registry.get_or_emplace<Tracking>(e);const auto now=Now();if(now<tracking.requested)return;tracking.requested=now+500;
    Send(e,"nx_death_ready 1 "+std::to_string(P::GetPlayerID(e)));
    if(!CombatSystem::IsDead(e)){Send(e,"nx_death_clear 1");return;}
    const auto found=recent.find(P::GetPlayerID(e));if(found==recent.end()||found->second.expires<=now){Send(e,"nx_death_status 1 no_history");return;}Publish(e,found->second);
}
}
