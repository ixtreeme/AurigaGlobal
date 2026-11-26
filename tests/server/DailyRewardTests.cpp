#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/command/daily_reward.h"
#include "../../SRC/Server/GameServer/item/ItemStore.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "ItemCatalogFixture.hpp"
#include <SQL/AsyncSQL.h>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <iostream>
#include <stdexcept>

// Run the real command and native grant planner. The store/SQL boundary is
// scripted so lost acknowledgements, conflicts and logout are deterministic.
entt::registry g_registry;
namespace item=auriga::item;
namespace store=item::runtime;
namespace {
int checks=0, transactions=0, receipts=0;
bool accepting=true, due=true, offer=true, receipt=true;
std::string day="0";
SqlTransactionOutcome outcome=SqlTransactionOutcome::Committed;
SqlTransaction submitted;
std::deque<store::ItemStore::Work> jobs;
std::vector<std::string> chats;
struct Player {uint32_t pid;};
void Check(bool good,const char* why) {++checks;if(!good)throw std::runtime_error(why);}
SQLMsg* Rows(std::vector<std::vector<std::string>> rows={}) {
    auto* msg=new SQLMsg;auto* result=new SQLResult;result->bPrepared=true;
    for (const auto& row:rows) {
        std::vector<char*> cells;
        for (const auto& cell:row) {
            result->preparedCells.push_back(cell);
            cells.push_back(result->preparedCells.back().data());
        }
        result->preparedRows.push_back(std::move(cells));
    }
    result->uiNumRows=rows.size();msg->vec_pkResult.push_back(result);return msg;
}
void Reset() {
    g_registry.clear();jobs.clear();chats.clear();submitted.clear();
    transactions=receipts=0;accepting=due=offer=receipt=true;
    day="0";outcome=SqlTransactionOutcome::Committed;
}
entt::entity Make() {auto e=g_registry.create();g_registry.emplace<Player>(e,uint32_t{42});return e;}
bool Said(std::string_view text) {
    return std::ranges::any_of(chats,[&](const auto& s){return s.find(text)!=std::string::npos;});
}
store::ItemStore::Completion Execute() {
    Check(!jobs.empty(),"claim did not reach native worker");
    auto work=std::move(jobs.front());jobs.pop_front();
    CAsyncSQL sql;
    const auto catalog=item::BuildItemCatalog(item_catalog_fixture::Catalog()).value();
    return work(sql,*catalog);
}
void Run() {auto done=Execute();if(done)done();}
void Queue(entt::entity e) {DailyReward::Claim(e,"fixture-hwid",7);}
}
CSemaphore::CSemaphore()=default;CSemaphore::~CSemaphore()=default;
CAsyncSQL::CAsyncSQL():m_queueLane(0){}CAsyncSQL::~CAsyncSQL()=default;
MYSQL_ROW SQLResult::FetchRow() {
    return preparedRowPos<preparedRows.size()?preparedRows[preparedRowPos++].data():nullptr;
}
SQLMsg* CAsyncSQL::DirectQueryPrepared(const char* query,const SqlParams&) {
    const std::string_view q(query);
    if(q.starts_with("SELECT reward FROM"))return due?Rows({{day}}):Rows();
    if(q.starts_with("SELECT category_id")) {
        const auto key=item_catalog_fixture::Leather();
        return offer?Rows({{std::to_string(static_cast<unsigned>(key.Category())),std::to_string(key.LocalId()),"0","5"}}):Rows();
    }
    if(q.starts_with("SELECT id FROM player.item_native_award")) {
        ++receipts;return receipt?Rows({{"99"}}):Rows();
    }
    if(q.starts_with("INSERT INTO player.daily_reward_status"))return Rows();
    if(q.starts_with("SELECT UNIX_TIMESTAMP(time)"))return Rows({{"1800000000",day}});
    throw std::runtime_error("unexpected SQL boundary");
}
SQLMsg* CAsyncSQL::DirectTransactionPrepared(SqlTransaction transaction) {
    ++transactions;submitted=std::move(transaction);
    auto* msg=Rows();msg->transactionOutcome=outcome;return msg;
}
namespace auriga::item::runtime {
ItemStore::~ItemStore()=default;
ItemStore& ItemStore::Instance() {static ItemStore instance;return instance;}
bool ItemStore::Submit(Work work,std::chrono::milliseconds) {
    if(!accepting)return false;jobs.push_back(std::move(work));return true;
}
}
namespace ecs::PlayerRuntime {
bool IsValid(entt::entity e) {return g_registry.valid(e)&&g_registry.all_of<Player>(e);}
uint32_t GetPlayerID(entt::entity e) {return IsValid(e)?g_registry.get<Player>(e).pid:0;}
// Every fixture supplies the account explicitly; the pointer is only an
// admission marker and must never be dereferenced.
LPDESC GetDesc(entt::entity e) {Check(IsValid(e),"stale descriptor read");return reinterpret_cast<LPDESC>(1);}
}
void ecs::ChatSystem::Send(entt::entity e,uint8_t,const char* format,...) {
    Check(ecs::PlayerRuntime::IsValid(e),"publication to retired player");
    char text[2048];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    chats.emplace_back(text);
}
int main() {
    try {
        Reset();auto e=Make();Queue(e);
        Check(chats.empty()&&transactions==0,"claim published before worker completion");Run();
        Check(transactions==1&&Said("ajándéktárolóban"),"committed native award not reported");
        Check(submitted.size()>=2,"status/award were not one transaction");
        for(const auto& step:submitted)Check(step.expectedAffectedRows==1,"claim lacks conditional row count");
        const auto& award=submitted.back();
        const auto key=item_catalog_fixture::Leather();
        Check(award.query.find("item_native_award")!=std::string::npos&&award.params.size()==7,"raw item grant used");
        Check(award.params[0].type==SqlParamType::Blob&&award.params[0].bytes.size()==16,"durable request ID missing");
        Check(award.params[1].u64==42&&award.params[2].u64==7&&
              award.params[3].u64==static_cast<unsigned>(key.Category())&&award.params[4].u64==key.LocalId()&&
              award.params[5].u64==0&&award.params[6].u64==5,"typed reward or owner changed");
#ifdef ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93
        Check(submitted.size()==3&&submitted[1].query.find("daily_reward_claim_hwid")!=std::string::npos,
              "hardware claim left atomic transaction");
#endif
        for(const auto refused:{SqlTransactionOutcome::Conflict,SqlTransactionOutcome::RolledBack,SqlTransactionOutcome::Failed}) {
            Reset();e=Make();outcome=refused;Queue(e);Run();
            Check(transactions==1&&!Said("ajándéktárolóban"),"uncommitted award reported as delivered");
        }
        for(const bool found:{true,false}) {
            Reset();e=Make();outcome=SqlTransactionOutcome::CommitUnknown;receipt=found;Queue(e);Run();
            Check(transactions==1&&receipts==1,"lost commit acknowledgement replayed the write");
            Check(Said("ajándéktárolóban")==found,"receipt result not respected");
        }
        for(int invalid=0;invalid<3;++invalid) {
            Reset();e=Make();if(invalid==0)due=false;if(invalid==1)offer=false;if(invalid==2)day="999";
            Queue(e);Run();Check(transactions==0&&!Said("ajándéktárolóban"),"invalid/due-less reward consumed claim");
        }
        Reset();e=Make();Queue(e);auto done=Execute();g_registry.destroy(e);Make();done();
        Check(chats.empty()&&transactions==1,"logout rebound the publication or discarded committed award");
        Reset();e=Make();accepting=false;Queue(e);
        Check(jobs.empty()&&Said("átmenetileg"),"unavailable store silently accepted claim");
        Reset();e=Make();DailyReward::Reload(e);Run();
        Check(Said("SetNativeReward|")&&!Said("SetReward|"),"reload published raw item identity");
        std::cout<<checks<<" native daily-reward checks passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
