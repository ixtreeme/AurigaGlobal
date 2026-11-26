#include "stdafx.h"
#include "../item/TypedUniqueAccessories.hpp"
#include "SocialHistory.hpp"
#include "SocialHistoryCrypto.hpp"
#include "SocialActions.hpp"
#include "../core/SocialRules.hpp"
#include "../core/config.h"
#include "../network/db.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../guild/guild.h"
#include "../entity/char_manager.h"
#include "../core/empire_text_convert.h"
#include "../item/unique_item.h"
#include "../combat/skill.h"
#include "../command/gm.h"
#include <Core/Logging.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>
#include <openssl/crypto.h>

namespace {
namespace H=auriga::social::history;namespace P=ecs::PlayerRuntime;
std::optional<H::Key> key;
using Message=std::unique_ptr<SQLMsg>;
Message Query(const std::string& sql,const SqlParams& params={}){return Message(DBManager::instance().DirectQueryPrepared(sql.c_str(),params));}
bool Good(const Message& m){return m&&!m->uiSQLErrno&&m->Get();}
std::string Table(const char* name){return std::string(name)+get_table_postfix();}
std::uint64_t Number(const char* s){return s?std::strtoull(s,nullptr,10):0;}
bool Integer(std::string_view s,std::uint64_t& n){if(s.empty())return false;auto result=std::from_chars(s.data(),s.data()+s.size(),n);return result.ec==std::errc()&&result.ptr==s.data()+s.size()&&n<9223372036854775807ULL;}
void Error(entt::entity e,const char* reason,std::uint64_t token=0){ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_error 1 %llu %s",static_cast<unsigned long long>(token),reason);}
struct Scope{unsigned kind=0;std::uint32_t a=0,b=0;std::string peer;};
std::optional<Scope> Resolve(entt::entity e,const std::string& type,const std::string& target)
{
    auto self=P::GetPlayerID(e);if(type=="p"){
        if(!auriga::social::ValidName(target))return {};auto player=Query("SELECT id,name FROM "+Table("player")+" WHERE name=?",{SqlParam::Text(target)});if(!Good(player)||player->Get()->uiNumRows!=1)return {};auto row=player->Get()->FetchRow();auto other=static_cast<std::uint32_t>(Number(row[0]));if(!other||other==self)return {};return Scope{1,std::min(self,other),std::max(self,other),row[1]};
    }
    if(type=="g"&&target=="-"){
        auto* guild=ecs::SocialSystem::GetGuild(e);if(!guild)return {};auto member=Query("SELECT 1 FROM "+Table("guild_member")+" WHERE pid=? AND guild_id=?",{SqlParam::UInt(self),SqlParam::UInt(guild->GetID())});if(!Good(member)||member->Get()->uiNumRows!=1)return {};return Scope{2,guild->GetID(),0,"@guild:"+std::to_string(guild->GetID())};
    }return {};
}
SqlParams Params(const Scope& scope){return {SqlParam::UInt(scope.kind),SqlParam::UInt(scope.a),SqlParam::UInt(scope.b)};}
// Datetimes are stored as UTC. Use integer arithmetic, independent of the SQL
// session timezone, to reproduce the exact authenticated millisecond timestamp.
const std::string columns="id,kind,peer_a,peer_b,sender_id,sender_name,(TIMESTAMPDIFF(MICROSECOND,CAST('1970-01-01 00:00:00' AS DATETIME),created_at) DIV 1000),empire,game_master,HEX(nonce),HEX(ciphertext),HEX(auth_tag)";
struct Record{H::Metadata metadata;std::string text;};
std::optional<Record> Decode(char** row)
{
    H::Metadata m;m.id=Number(row[0]);m.kind=static_cast<std::uint8_t>(Number(row[1]));m.a=static_cast<std::uint32_t>(Number(row[2]));m.b=static_cast<std::uint32_t>(Number(row[3]));m.sender=static_cast<std::uint32_t>(Number(row[4]));m.name=row[5]?row[5]:"";m.created=Number(row[6]);m.empire=static_cast<std::uint8_t>(Number(row[7]));m.gm=static_cast<std::uint8_t>(Number(row[8]));
    auto nonce=H::Unhex(row[9]?row[9]:""),cipher=H::Unhex(row[10]?row[10]:""),tag=H::Unhex(row[11]?row[11]:"");if(!nonce||!cipher||!tag)return {};auto plain=H::Open(*key,m,{*nonce,*cipher,*tag});if(!plain)return {};return Record{m,*plain};
}
bool Append(entt::entity sender,Scope scope,std::string body)
{
    if(!key)return true;if(body.empty()||body.size()>1024)return false;
    H::Metadata m;m.kind=static_cast<std::uint8_t>(scope.kind);m.a=scope.a;m.b=scope.b;m.sender=P::GetPlayerID(sender);m.name=P::GetName(sender);m.empire=P::GetEmpire(sender);m.gm=P::GetGMLevel(sender)!=GM_PLAYER;
    m.created=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for(int attempt=0;attempt<4;attempt++){
        auto sequence=Query("SELECT value FROM social_chat_sequence WHERE singleton=1");if(!Good(sequence)||sequence->Get()->uiNumRows!=1)return false;auto previous=Number(sequence->Get()->FetchRow()[0]);m.id=previous+1;
        auto sealed=H::Seal(*key,m,body);if(!sealed)return false;
        SqlTransaction tx;
        tx.push_back({"UPDATE social_chat_sequence SET value=? WHERE singleton=1 AND value=?",{SqlParam::UInt(m.id),SqlParam::UInt(previous)},1,{}});
        tx.push_back({"INSERT INTO social_chat_message(id,kind,peer_a,peer_b,sender_id,sender_name,created_at,empire,game_master,nonce,ciphertext,auth_tag) VALUES(?,?,?,?,?,?,TIMESTAMPADD(MICROSECOND,?*1000,CAST('1970-01-01 00:00:00' AS DATETIME)),?,?,?,?,?)",{SqlParam::UInt(m.id),SqlParam::UInt(m.kind),SqlParam::UInt(m.a),SqlParam::UInt(m.b),SqlParam::UInt(m.sender),SqlParam::Blob(m.name),SqlParam::UInt(m.created),SqlParam::UInt(m.empire),SqlParam::UInt(m.gm),SqlParam::Blob(sealed->nonce),SqlParam::Blob(sealed->cipher),SqlParam::Blob(sealed->tag)},1,{}});
        Message result(DBManager::instance().DirectTransactionPrepared(std::move(tx)));if(result&&result->transactionOutcome==SqlTransactionOutcome::Committed)return true;
    }return false;
}
std::string Display(entt::entity viewer,const Record& record)
{
    auto text=record.text;const auto& m=record.metadata;
    // History must not bypass the game's language restrictions for the recipient.
    if(m.kind==1&&m.sender!=P::GetPlayerID(viewer)&&g_bEmpireWhisper&&!m.gm&&P::GetGMLevel(viewer)==GM_PLAYER&&P::GetEmpire(viewer)&&m.empire&&P::GetEmpire(viewer)!=m.empire&&!auriga::item::runtime::HasTypedLanguageRing(viewer)){
        auto start=text.find(" : ");if(start!=std::string::npos)ConvertEmpireText(m.empire,text.data()+start+3,text.size()-start-3,10+2*SkillSystem::GetSkillPower(viewer,SKILL_LANGUAGE1+m.empire-1));
    }return text;
}
void SendRecord(entt::entity e,const Scope& scope,const Record& record,std::uint64_t token)
{
    auto text=Display(e,record);const auto& m=record.metadata;const auto count=(text.size()+99)/100;
    ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_row 1 %llu %llu %llu %u %s %u",static_cast<unsigned long long>(token),static_cast<unsigned long long>(m.id),static_cast<unsigned long long>(m.created),m.sender,H::Hex(m.name).c_str(),static_cast<unsigned>(count));
    for(size_t part=0;part<count;part++)ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_part 1 %llu %llu %u %s",static_cast<unsigned long long>(token),static_cast<unsigned long long>(m.id),static_cast<unsigned>(part),H::Hex(std::string_view(text).substr(part*100,100)).c_str());
}
struct Throttle{std::chrono::steady_clock::time_point history,read;};
}
bool InitializeSocialHistory(const std::string& file,std::string& error)
{
    key.reset();if(file.empty())return true;std::ifstream input(file,std::ios::binary);H::Key value{};input.read(reinterpret_cast<char*>(value.data()),value.size());char extra=0;
    if(input.gcount()!=32||input.get(extra)||std::all_of(value.begin(),value.end(),[](auto c){return c==0;})){error="key file must contain exactly 32 secret random bytes";OPENSSL_cleanse(value.data(),value.size());return false;}
    key=value;OPENSSL_cleanse(value.data(),value.size());return true;
}
bool SocialHistoryEnabled(){return key.has_value();}
bool SavePrivateHistory(entt::entity sender,const std::string& recipient,const std::string& body)
{
    if(!key)return true;auto scope=Resolve(sender,"p",recipient);if(!scope||!Append(sender,*scope,std::string(P::GetName(sender))+" : "+body)){Error(sender,"save_failed");return false;}
    SendSocialCommand(std::string(P::GetName(sender)),"social_log_changed 1 "+H::Hex(scope->peer));SendSocialCommand(scope->peer,"social_log_changed 1 "+H::Hex(P::GetName(sender)));return true;
}
bool SaveGuildHistory(entt::entity sender,const std::string& formatted)
{
    if(!key)return true;auto scope=Resolve(sender,"g","-");if(!scope||!Append(sender,*scope,formatted)){Error(sender,"save_failed");return false;}
    auto members=Query("SELECT p.name FROM "+Table("guild_member")+" g JOIN "+Table("player")+" p ON p.id=g.pid WHERE g.guild_id=?",{SqlParam::UInt(scope->a)});
    if(Good(members))for(std::uint64_t i=0;i<members->Get()->uiNumRows;i++)SendSocialCommand(members->Get()->FetchRow()[0],"social_log_changed 1 "+H::Hex(scope->peer));return true;
}
void SendSocialHistoryIndex(entt::entity e)
{
    if(!key)return;auto pid=P::GetPlayerID(e);auto* guild=ecs::SocialSystem::GetGuild(e);auto gid=guild?guild->GetID():0;
    auto rows=Query("SELECT m.kind,m.peer_a,m.peer_b,MAX(m.id),TIMESTAMPDIFF(MICROSECOND,CAST('1970-01-01 00:00:00' AS DATETIME),MAX(m.created_at)) DIV 1000,COALESCE(r.last_id,0),SUM(m.id>COALESCE(r.last_id,0) AND m.sender_id<>?),p.name FROM social_chat_message m LEFT JOIN social_chat_read r ON r.player_id=? AND r.kind=m.kind AND r.peer_a=m.peer_a AND r.peer_b=m.peer_b LEFT JOIN "+Table("player")+" p ON m.kind=1 AND p.id=IF(m.peer_a=?,m.peer_b,m.peer_a) WHERE (m.kind=1 AND (m.peer_a=? OR m.peer_b=?)) OR (m.kind=2 AND m.peer_a=? AND EXISTS(SELECT 1 FROM "+Table("guild_member")+" gm WHERE gm.pid=? AND gm.guild_id=m.peer_a)) GROUP BY m.kind,m.peer_a,m.peer_b,r.last_id,p.name ORDER BY MAX(m.id) DESC LIMIT 256",{SqlParam::UInt(pid),SqlParam::UInt(pid),SqlParam::UInt(pid),SqlParam::UInt(pid),SqlParam::UInt(pid),SqlParam::UInt(gid),SqlParam::UInt(pid)});
    if(!Good(rows)){Error(e,"unavailable");return;}ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_index_begin 1");
    for(std::uint64_t i=0;i<rows->Get()->uiNumRows;i++){
        auto row=rows->Get()->FetchRow();std::string peer=Number(row[0])==2?"@guild:"+std::string(row[1]):row[7]?row[7]:"";if(peer.empty())continue;
        ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_peer 1 %s %s %s %s %s",H::Hex(peer).c_str(),row[3],row[4],row[5],row[6]);
    }ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_index_end 1");
}
bool HandleSocialHistory(entt::entity e,const char* arguments)
{
    std::istringstream input(arguments?arguments:"");std::string action,type,target,cursorText,tokenText,extra;input>>action;if(action!="history"&&action!="read")return false;
    if(!key){Error(e,"disabled");return true;}
    input>>type>>target>>cursorText;if(action=="history")input>>tokenText;input>>extra;std::uint64_t cursor=0,token=0;
    if(!extra.empty()||!Integer(cursorText,cursor)||(action=="history"&&(!Integer(tokenText,token)||!token||token>2147483647))){Error(e,"invalid");return true;}
    auto scope=Resolve(e,type,target);if(!scope){Error(e,"denied",token);return true;}
    auto& rate=g_registry.get_or_emplace<Throttle>(e);auto now=std::chrono::steady_clock::now();auto& deadline=action=="history"?rate.history:rate.read;
    if(now<deadline){Error(e,"wait",token);return true;}deadline=now+std::chrono::milliseconds(action=="history"?200:75);
    auto params=Params(*scope);
    if(action=="read"){
        auto valid=params;valid.push_back(SqlParam::UInt(cursor));auto row=Query("SELECT 1 FROM social_chat_message WHERE kind=? AND peer_a=? AND peer_b=? AND id=?",valid);
        if(!Good(row)||row->Get()->uiNumRows!=1){Error(e,"denied");return true;}
        auto result=Query("INSERT INTO social_chat_read(player_id,kind,peer_a,peer_b,last_id) VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE last_id=GREATEST(last_id,VALUES(last_id))",{SqlParam::UInt(P::GetPlayerID(e)),SqlParam::UInt(scope->kind),SqlParam::UInt(scope->a),SqlParam::UInt(scope->b),SqlParam::UInt(cursor)});
        if(!Good(result))Error(e,"unavailable");else ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_read 1 %s %llu",H::Hex(scope->peer).c_str(),static_cast<unsigned long long>(cursor));return true;
    }
    params.push_back(SqlParam::UInt(cursor));params.push_back(SqlParam::UInt(cursor));auto rows=Query("SELECT "+columns+" FROM social_chat_message WHERE kind=? AND peer_a=? AND peer_b=? AND (?=0 OR id<?) ORDER BY id DESC LIMIT 41",params);
    if(!Good(rows)){Error(e,"unavailable",token);return true;}std::vector<Record> records;bool more=rows->Get()->uiNumRows>40;
    for(std::uint64_t i=0;i<std::min<std::uint64_t>(40,rows->Get()->uiNumRows);i++){auto record=Decode(rows->Get()->FetchRow());if(!record){LOG_ERROR("SOCIAL_HISTORY_AUTH_FAILED viewer={} scope={}:{}:{}",P::GetPlayerID(e),scope->kind,scope->a,scope->b);Error(e,"integrity",token);return true;}records.push_back(std::move(*record));}
    ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_begin 1 %llu %s %llu",static_cast<unsigned long long>(token),H::Hex(scope->peer).c_str(),static_cast<unsigned long long>(cursor));
    std::reverse(records.begin(),records.end());for(const auto& record:records)SendRecord(e,*scope,record,token);
    ecs::ChatSystem::Send(e,CHAT_TYPE_COMMAND,"social_log_end 1 %llu %u",static_cast<unsigned long long>(token),more?1:0);return true;
}
