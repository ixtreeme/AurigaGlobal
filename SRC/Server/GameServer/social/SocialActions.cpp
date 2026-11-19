#include "stdafx.h"
#include "SocialActions.hpp"
#include "SocialHistory.hpp"
#include "../core/SocialRules.hpp"
#include "../core/config.h"
#include "../core/utils.h"
#include "../core/packet.h"
#include "../network/db.h"
#include "../network/desc.h"
#include "../network/p2p.h"
#include "../entity/char_manager.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "messenger_manager.h"
#include "gm.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <fmt/format.h>

namespace {
namespace P=ecs::PlayerRuntime;
using Message=std::unique_ptr<SQLMsg>;
Message Query(const std::string& sql,const SqlParams& params={}){return Message(DBManager::instance().DirectQueryPrepared(sql.c_str(),params));}
bool Good(const Message& m){return m && !m->uiSQLErrno && m->Get();}
std::uint64_t Number(const char* value){return value?std::strtoull(value,nullptr,10):0;}
std::string Table(const char* name){return std::string(name)+get_table_postfix();}
entt::entity Online(const std::string& name){const auto e=CHARACTER_MANAGER::instance().FindPCEntity(name.c_str());return e!=entt::null && g_registry.valid(e) && P::GetDesc(e)?e:entt::null;}
void Command(const std::string& name,const std::string& line)
{
    if(const auto e=Online(name);e!=entt::null){ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"%s",line.c_str());return;}
    if(auto* peer=P2P_MANAGER::instance().Find(name.c_str());peer && peer->pkDesc){
        TPacketGCChat packet{};packet.header=HEADER_GC_CHAT;packet.type=CHAT_TYPE_COMMAND;packet.size=static_cast<std::uint16_t>(sizeof(packet)+line.size()+1);
        std::vector<char> data(sizeof(packet)+line.size()+1);std::memcpy(data.data(),&packet,sizeof(packet));std::memcpy(data.data()+sizeof(packet),line.c_str(),line.size()+1);
        peer->pkDesc->SetRelay(name.c_str());peer->pkDesc->Packet(data.data(),static_cast<int>(data.size()));
    }
}
void Result(entt::entity e,const char* status,const std::string& target="-"){ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_result %s %s",status,target.c_str());}
std::uint32_t Pid(const std::string& name){auto m=Query("SELECT id FROM "+Table("player")+" WHERE name=?",{SqlParam::Text(name)});return Good(m)&&m->Get()->uiNumRows?static_cast<std::uint32_t>(Number(m->Get()->FetchRow()[0])):0;}
void Profile(entt::entity viewer,const std::string& name,bool location)
{
    if(!auriga::social::ValidName(name))return;
    auto m=Query("SELECT id,name,job,level,COALESCE(UNIX_TIMESTAMP(last_play),0) FROM "+Table("player")+" WHERE name=?",{SqlParam::Text(name)});
    if(!Good(m)||!m->Get()->uiNumRows)return;auto row=m->Get()->FetchRow();std::string canonical=row[1];
    unsigned race=static_cast<unsigned>(Number(row[2])),level=static_cast<unsigned>(Number(row[3]));int map=0,channel=0;bool online=false;
    if(const auto e=Online(canonical);e!=entt::null){online=true;race=P::GetRaceNum(e);level=ecs::PointSystem::GetLevel(e);if(location){map=P::GetMapIndex(e);channel=g_bChannel;}}
    else if(const auto* peer=P2P_MANAGER::instance().Find(canonical.c_str())){online=true;if(location){map=peer->lMapIndex;channel=peer->bChannel;}}
    ecs::ChatSystem::Send(viewer,CHAT_TYPE_COMMAND,"social_peer %s %u %u %u %d %d %llu %d",canonical.c_str(),static_cast<unsigned>(Number(row[0])),race,level,map,channel,static_cast<unsigned long long>(Number(row[4])),online?1:0);
}
struct SocialThrottle {std::chrono::steady_clock::time_point next,mutation,profile;};
}

bool SocialBlocked(const std::string& first,const std::string& second)
{
    auto m=Query("SELECT 1 FROM social_block b JOIN "+Table("player")+" a ON a.id=b.owner_id JOIN "+Table("player")+" t ON t.id=b.blocked_id WHERE (a.name=? AND t.name=?) OR (a.name=? AND t.name=?) LIMIT 1",{SqlParam::Text(first),SqlParam::Text(second),SqlParam::Text(second),SqlParam::Text(first)});
    return !Good(m) || m->Get()->uiNumRows>0;
}
bool SaveSocialFriendRequest(const std::string& requester,const std::string& recipient)
{
    if(!auriga::social::ValidName(requester)||!auriga::social::ValidName(recipient)||requester==recipient||SocialBlocked(requester,recipient))return false;
    auto from=Pid(requester),to=Pid(recipient);if(!from||!to||from==to)return false;
    auto existing=Query("SELECT 1 FROM "+Table("messenger_list")+" WHERE account=? AND companion=?",{SqlParam::Text(requester),SqlParam::Text(recipient)});if(!Good(existing)||existing->Get()->uiNumRows)return false;
    auto count=Query("SELECT (SELECT COUNT(*) FROM social_friend_request WHERE recipient_id=?),(SELECT COUNT(*) FROM social_friend_request WHERE requester_id=?)",{SqlParam::UInt(to),SqlParam::UInt(from)});
    if(!Good(count))return false;auto limits=count->Get()->FetchRow();if(Number(limits[0])>=auriga::social::PendingLimit||Number(limits[1])>=auriga::social::PendingLimit)return false;
    auto result=Query("INSERT IGNORE INTO social_friend_request(requester_id,recipient_id) VALUES(?,?)",{SqlParam::UInt(from),SqlParam::UInt(to)});
    return Good(result);
}
bool ResolveSocialFriendRequest(const std::string& recipient,const std::string& requester,bool deny)
{
    const auto from=Pid(requester),to=Pid(recipient);if(!from||!to||(!deny && SocialBlocked(requester,recipient)))return false;
    SqlTransaction tx;
    tx.push_back({"SELECT requester_id FROM social_friend_request WHERE requester_id=? AND recipient_id=? FOR UPDATE",{SqlParam::UInt(from),SqlParam::UInt(to)},{},1});
    if(!deny){
        tx.push_back({"INSERT IGNORE INTO "+Table("messenger_list")+"(account,companion) VALUES(?,?),(?,?)",{SqlParam::Text(requester),SqlParam::Text(recipient),SqlParam::Text(recipient),SqlParam::Text(requester)},{},{}});
    }
    tx.push_back({"DELETE FROM social_friend_request WHERE requester_id=? AND recipient_id=?",{SqlParam::UInt(from),SqlParam::UInt(to)},1,{}});
    Message result(DBManager::instance().DirectTransactionPrepared(std::move(tx)));
    return result && result->transactionOutcome==SqlTransactionOutcome::Committed;
}
void NotifySocialChanged(const std::string& name){Command(name,"social_changed");}
void SendSocialCommand(const std::string& name,const std::string& command){Command(name,command);}
void SendSocialSnapshot(entt::entity e)
{
    if(e==entt::null||!g_registry.valid(e)||!P::GetDesc(e))return;
    const std::string self(P::GetName(e));const auto pid=P::GetPlayerID(e);
    auto friends=Query("SELECT companion FROM "+Table("messenger_list")+" WHERE account=? ORDER BY companion LIMIT 256",{SqlParam::Text(self)});
    auto requests=Query("SELECT p.name FROM social_friend_request r JOIN "+Table("player")+" p ON p.id=r.requester_id WHERE r.recipient_id=? ORDER BY r.created_at LIMIT 64",{SqlParam::UInt(pid)});
    auto blocks=Query("SELECT p.name FROM social_block b JOIN "+Table("player")+" p ON p.id=b.blocked_id WHERE b.owner_id=? ORDER BY p.name LIMIT 256",{SqlParam::UInt(pid)});
    if(!Good(friends)||!Good(requests)||!Good(blocks)){Result(e,"unavailable");return;}
    ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_begin 1");
    Profile(e,self,true);
    for(std::uint64_t i=0;i<friends->Get()->uiNumRows;i++){const std::string name=friends->Get()->FetchRow()[0];ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_friend %s",name.c_str());Profile(e,name,true);}
    for(std::uint64_t i=0;i<requests->Get()->uiNumRows;i++){const std::string name=requests->Get()->FetchRow()[0];ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_request %s",name.c_str());Profile(e,name,false);}
    for(std::uint64_t i=0;i<blocks->Get()->uiNumRows;i++){const std::string name=blocks->Get()->FetchRow()[0];ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_block %s",name.c_str());Profile(e,name,false);}
    ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_end 1");
    SendSocialHistoryIndex(e);
}
void HandleSocialCommand(entt::entity e,const char* arguments)
{
    if(e==entt::null||!g_registry.valid(e)||!P::GetDesc(e))return;
    if(HandleSocialHistory(e,arguments))return;
    char action[32]{},target[64]{},extra[32]{};const auto* tail=two_arguments(arguments,action,sizeof(action),target,sizeof(target));one_argument(tail,extra,sizeof(extra));
    const std::string self(P::GetName(e)),name(target);const auto pid=P::GetPlayerID(e);
    if(std::string_view(action)=="sync" && name.empty()){
        auto& rate=g_registry.get_or_emplace<SocialThrottle>(e);const auto now=std::chrono::steady_clock::now();if(now<rate.next)return;rate.next=now+std::chrono::seconds(1);SendSocialSnapshot(e);return;
    }
    if(*extra||!auriga::social::ValidName(name)){Result(e,"invalid");return;}
    auto& rate=g_registry.get_or_emplace<SocialThrottle>(e);const auto now=std::chrono::steady_clock::now();
    if(std::string_view(action)=="profile"){
        if(now<rate.profile)return;rate.profile=now+std::chrono::milliseconds(150);
        auto friendRow=Query("SELECT 1 FROM "+Table("messenger_list")+" WHERE account=? AND companion=?",{SqlParam::Text(self),SqlParam::Text(name)});
        Profile(e,name,Good(friendRow)&&friendRow->Get()->uiNumRows>0);return;
    }
    if(now<rate.mutation){Result(e,"wait",name);return;}rate.mutation=now+std::chrono::seconds(1);
    if(Pid(name)==0||self==name){Result(e,"invalid",name);return;}
    if(std::string_view(action)=="add"){
        auto present=Query("SELECT 1 FROM "+Table("messenger_list")+" WHERE account=? AND companion=?",{SqlParam::Text(self),SqlParam::Text(name)});if(Good(present)&&present->Get()->uiNumRows){Result(e,"already",name);return;}
        if(P::GetGMLevel(e)==GM_PLAYER && gm_get_level(name.c_str())!=GM_PLAYER){Result(e,"denied",name);return;}
        const auto other=Online(name);if(other!=entt::null && P::IsBlockMode(other,BLOCK_MESSENGER_INVITE)){Result(e,"blocked",name);return;}
        if(!SaveSocialFriendRequest(self,name)){Result(e,"blocked",name);return;}
        Command(name,"messenger_auth "+self);NotifySocialChanged(name);Result(e,"requested",name);SendSocialSnapshot(e);return;
    }
    if(std::string_view(action)=="block" || std::string_view(action)=="unblock"){
        const auto other=Pid(name);const bool block=std::string_view(action)=="block";
        if(block){auto count=Query("SELECT COUNT(*) FROM social_block WHERE owner_id=?",{SqlParam::UInt(pid)});if(!Good(count)||Number(count->Get()->FetchRow()[0])>=auriga::social::ContactLimit){Result(e,"limit",name);return;}}
        auto result=Query(block?"INSERT IGNORE INTO social_block(owner_id,blocked_id) VALUES(?,?)":"DELETE FROM social_block WHERE owner_id=? AND blocked_id=?",{SqlParam::UInt(pid),SqlParam::UInt(other)});
        if(!Good(result)){Result(e,"unavailable",name);return;}
        if(block)Query("DELETE FROM social_friend_request WHERE (requester_id=? AND recipient_id=?) OR (requester_id=? AND recipient_id=?)",{SqlParam::UInt(pid),SqlParam::UInt(other),SqlParam::UInt(other),SqlParam::UInt(pid)});
        Result(e,block?"blocked_ok":"unblocked",name);SendSocialSnapshot(e);NotifySocialChanged(name);return;
    }
    Result(e,"invalid",name);
}
