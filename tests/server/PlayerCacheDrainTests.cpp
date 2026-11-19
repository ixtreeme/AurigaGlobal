#include "../../SRC/Server/Database/stdafx.h"
#include "../../SRC/Server/Database/Cache.h"
#include "../../SRC/Server/Database/DBManager.h"
#include "../../SRC/Server/Database/player_cache_drain.h"
#include <Core/Logging.hpp>
#include <Core/player_cache_snapshot.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <type_traits>

namespace pc = auriga::player_cache;
namespace ps = auriga::player_session;
namespace wire = auriga::player_cache_wire;
namespace
{
int checks = 0;
std::vector<std::string> writes;
CAsyncSQL *mainWorker = nullptr, *escapeConnection = nullptr;
void Check(bool value, const char *why)
{
    ++checks;
    if (!value)
        throw std::runtime_error(why);
}
ps::Token Token(uint8_t n)
{
    ps::Token t{};
    t.fill(n);
    return t;
}
void Loaded(ps::Authority &authority, const ps::Lease &lease)
{
    for (uint8_t part : {uint8_t(HEADER_DG_PLAYER_LOAD_SUCCESS),
                         uint8_t(HEADER_DG_QUEST_LOAD), uint8_t(HEADER_DG_AFFECT_LOAD)
#ifdef __SKILL_COLOR_SYSTEM__
                                                            ,
                         uint8_t(HEADER_DG_SKILL_COLOR_LOAD)
#endif
         })
        Check(authority.TakePart(lease, part), "native load part not accepted");
    Check(authority.Loaded(lease), "completed native load not recognized");
}
struct Fixture
{
    ps::Authority authority;
    ps::Lease lease;
    CPlayerTableCache player;
    Fixture()
    {
        lease = *authority.Open(7, 50, 900, 100, Token(1), Token(2));
        Check(!authority.Loaded(lease), "unfinished load appeared complete");
        Loaded(authority, lease);
        TPlayerTable p{};
        p.id = 100;
        p.gold = 500;
        p.level = 50;
        p.hp = 1000;
        p.sp = 500;
        player.Put(&p);
    }
    pc::View View()
    {
        return {&player};
    }
};
void Unit()
{
    static_assert(!std::is_copy_constructible_v<CPlayerTableCache>);
    Fixture f;
    const auto version=f.player.Version();
    f.player.Get();Check(f.player.Version()==version,"cache read changed revision");
    writes.clear();f.player.Flush();f.player.Flush();
    Check(writes.size()==1 && writes.front().starts_with("UPDATE player_cache_drain SET "),"player flush must write once and never write retired items");
    Check(!f.player.HasPendingQuery(),"player flush left dirty state");
    CAsyncSQL a,b;const std::array origins{a.QueueOrigin(),b.QueueOrigin()};bool queued=false;
    auto enqueue=[&]{queued=true;return std::array{a.DrainQueue(),b.DrainQueue()};};
    for(unsigned bad=0;bad<8;++bad) {
        auto lease=f.lease;auto challenge=Token(3);auto view=f.View();auto lanes=origins;
        switch(bad) {
            case 0:++lease.peer;break;case 1:challenge={};break;case 2:view.player=nullptr;break;
            case 3:lanes[1]=lanes[0];break;case 4:lanes[0].sequence=1;break;case 5:lanes[0].unconfirmed=1;break;
            case 6:++lease.key.character;break;case 7:lease.key.link={};break;
        }
        Check(!pc::Drain::Begin(f.authority,lease,challenge,view,lanes,enqueue) && !queued,"invalid drain submitted SQL");
    }
    auto drain=pc::Drain::Begin(f.authority,f.lease,Token(3),f.View(),origins,enqueue);
    Check(drain && queued && drain->Poll(f.authority,f.View()).status==pc::Status::Blocked,"unavailable workers confirmed persistence");
    f.authority.InvalidatePeer(7);Check(drain->Poll(f.authority,f.View()).status==pc::Status::Stale,"disconnected writer retained authority");
}
void Text(CAsyncSQL &sql, const std::string &query)
{
    std::unique_ptr<SQLMsg> result(sql.DirectQuery(query.c_str()));
    Check(result && !result->uiSQLErrno, query.c_str());
}
uint64_t Number(CAsyncSQL &sql, const std::string &query)
{
    std::unique_ptr<SQLMsg> result(sql.DirectQuery(query.c_str()));
    Check(result && !result->uiSQLErrno && result->Get(), "test SQL read failed");
    auto row = result->Get()->FetchRow();
    Check(row && row[0], "test SQL read row missing");
    return std::stoull(row[0]);
}
void Connect(CAsyncSQL &sql, unsigned port, const std::string &schema, bool direct)
{
    sql.SetTimeouts(3);
    sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1", "root", "", schema.c_str(), "utf8mb4", direct, port) &&
              (direct || sql.WaitForConnection(std::chrono::seconds(5))),
          "isolated cache SQL unavailable");
}
void Schema(CAsyncSQL &sql, const char *path)
{
    std::ifstream file(path, std::ios::binary);
    Check(bool(file), "canonical schema missing");
    const std::string source((std::istreambuf_iterator<char>(file)), {});
    for (const auto &name : {"player"})
    {
        const std::string prefix = std::string("CREATE TABLE IF NOT EXISTS `player`.`") + name + "` (";
        const auto begin = source.find(prefix), end = source.find(';', begin);
        Check(begin != source.npos && end != source.npos, "canonical cache table missing");
        auto ddl = source.substr(begin, end - begin);
        ddl.replace(ddl.find("`player`."), 9, "");
        const std::string table = std::string("`") + name + "`";
        ddl.replace(ddl.find(table), table.size(), std::string("`") + name + "_cache_drain`");
        Text(sql, ddl);
    }
    Text(sql, "INSERT INTO player_cache_drain(id,account_id,name) VALUES(100,900,'cache_drain')");
}
pc::Receipt Await(pc::Drain &drain, Fixture &f)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    auto result = drain.Poll(f.authority, f.View());
    while (result.status == pc::Status::Pending && std::chrono::steady_clock::now() < end)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        result = drain.Poll(f.authority, f.View());
    }
    Check(result.status != pc::Status::Pending, "cache SQL drain timed out");
    return result;
}


void Integration(unsigned port,const std::string& schema,const char* path)
{
    Check(schema.starts_with("auriga_mission_sql_test_") && schema.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==schema.npos,"private schema required");
    CAsyncSQL admin;
    Connect(admin,port,"",true);
    Text(admin,"CREATE DATABASE "+schema+" CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
    Text(admin,"USE "+schema);Schema(admin,path);escapeConnection=&admin;
    for(unsigned scenario=0;scenario<9;++scenario) {
        CAsyncSQL main,async;
        Connect(main,port,schema,false);Connect(async,port,schema,false);mainWorker=&main;Fixture f;
        const std::array origins{main.QueueOrigin(),async.QueueOrigin()};
        const auto lock=schema+"_hold";
        Check(Number(admin,"SELECT GET_LOCK('"+lock+"',0)")==1,"lock unavailable");
        if(scenario!=8)main.AsyncQuery(("SELECT GET_LOCK('"+lock+"',5),RELEASE_LOCK('"+lock+"')").c_str());
        async.AsyncQuery(("SELECT GET_LOCK('"+lock+"',5),RELEASE_LOCK('"+lock+"')").c_str());
        if(scenario==2)async.AsyncQuery("UPDATE absent_cache_drain_table SET missing=1");
        if(scenario==3)Text(admin,"CREATE TRIGGER fail_cache_drain BEFORE UPDATE ON player_cache_drain FOR EACH ROW SIGNAL SQLSTATE '45000'");
        auto drain=pc::Drain::Begin(f.authority,f.lease,Token(20),f.View(),origins,[&]{auto tickets=std::array{main.DrainQueue(),async.DrainQueue()};if(scenario==7)tickets[1]=tickets[0];return tickets;});
        Check(bool(drain),"native drain refused");
        auto pending=drain->Poll(f.authority,f.View());
        Check(pending.status==(scenario==7?pc::Status::Blocked:pc::Status::Pending),"SQL confirmed before FIFO boundary");
        Check(!f.player.HasPendingQuery(),"dirty player not submitted");
        CPlayerTableCache replacement;pc::View current=f.View();
        if(scenario==1){auto row=*f.player.Get(false);++row.gold;f.player.Put(&row);--row.gold;f.player.Put(&row);f.player.Flush();}
        if(scenario==4)f.authority.InvalidateAccount(900);
        if(scenario==5){replacement.Put(f.player.Get(false),true);current.player=&replacement;}
        if(scenario==6){f.authority.InvalidatePeer(7);auto lease=f.authority.Open(7,50,900,100,Token(1),Token(2));Check(lease && lease->key!=f.lease.key,"relogin reused generation");Loaded(f.authority,*lease);}
        if(scenario==8){
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            do{pending=drain->Poll(f.authority,current);if(pending.lanes[0].status==SqlQueueDrainStatus::Complete)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}while(std::chrono::steady_clock::now()<end);
            Check(pending.status==pc::Status::Pending && pending.lanes[0].status==SqlQueueDrainStatus::Complete && pending.lanes[1].status==SqlQueueDrainStatus::Pending,"one SQL lane confirmed both");
        }
        Check(Number(admin,"SELECT RELEASE_LOCK('"+lock+"')")==1,"unlock failed");
        auto result=drain->Poll(f.authority,current);
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(6);
        while(result.status==pc::Status::Pending && std::chrono::steady_clock::now()<end){std::this_thread::sleep_for(std::chrono::milliseconds(1));result=drain->Poll(f.authority,current);}
        const auto decoded=wire::DecodeReceipt(wire::Encode(pc::ToWire(result)));
        Check(decoded && decoded->key==f.lease.key && decoded->challenge==Token(20),"SQL receipt lost scope");
        if(scenario==0 || scenario==8){Check(result.status==pc::Status::Flushed && result.failure==pc::Failure::None,"clean SQL not confirmed");Check(Number(admin,"SELECT gold FROM player_cache_drain WHERE id=100")==500,"player save missing");}
        else if(scenario==4 || scenario==6)Check(result.status==pc::Status::Stale,"old login accepted");
        else Check(result.status==pc::Status::Blocked,"changed cache or SQL failure accepted");
        if(scenario==3)Text(admin,"DROP TRIGGER fail_cache_drain");mainWorker=nullptr;
    }
    Check(Number(admin,"SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='"+schema+"' AND table_name='item_cache_drain'")==0,"retired item cache table recreated");
    escapeConnection=nullptr;Text(admin,"DROP DATABASE "+schema);
}
} // namespace

int g_iPlayerCacheFlushSeconds = 300, g_iItemCacheFlushSeconds = 300, g_iSkillColorCacheFlushSeconds = 300;
int g_iItemPriceListTableCacheFlushSeconds = 1800, g_test_server = 0, g_item_count = 0;
const char *GetTablePostfix()
{
    return "_cache_drain";
}
CDBManager::CDBManager() = default;
CDBManager::~CDBManager() = default;
void CDBManager::ReturnQuery(const char *sql, int, uint32_t, void *, int)
{
    writes.emplace_back(sql);
    if (mainWorker)
        mainWorker->ReturnQuery(sql, nullptr);
}
unsigned long CDBManager::EscapeString(void *out, const void *in, unsigned long length, int)
{
    if (escapeConnection)
        return escapeConnection->EscapeString(static_cast<char *>(out), 8193, static_cast<const char *>(in),
                                              length);
    auto *target = static_cast<char *>(out);
    for (unsigned long i = 0; i < length; ++i)
    {
        const auto ch = static_cast<const char *>(in)[i];
        if (!ch)
        {
            *target++ = '\\';
            *target++ = '0';
        }
        else if (ch == '\'' || ch == '"' || ch == '\\')
        {
            *target++ = '\\';
            *target++ = ch;
        }
        else
            *target++ = ch;
    }
    *target = 0;
    return static_cast<unsigned long>(target - static_cast<char *>(out));
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto log = std::make_shared<spdlog::logger>("player-cache-drain-test");
    return log;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    return logging::GetLogger();
}
int main(int argc, char **argv)
{
    try
    {
        CDBManager manager;
        Unit();
        if (argc == 5)
            Integration(static_cast<unsigned>(std::stoul(argv[1])), argv[2], argv[4]);
        else
            Check(argc == 1, "unsupported test arguments");
        std::cout << "player cache drain: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
