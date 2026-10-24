#include "../../SRC/Server/Database/stdafx.h"
#include "../../SRC/Server/Database/ClientManager.h"
#include "../../SRC/Server/Database/Marriage.h"
#include "../../SRC/Server/Database/PrivManager.h"
#include <common/event_flag.h>
#include <common/time_64_packets.h>
#include <Core/Logging.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>

static_assert(sizeof(THorseInfo) == 14 && offsetof(THorseInfo, dwHorseHealthDropTime) == 6);
static_assert(sizeof(TPacketSetEventFlag) == EVENT_FLAG_NAME_MAX_LEN + 1 + 8);
static_assert(sizeof(TPacketMarriageAdd) == 16 + 2 * (CHARACTER_NAME_MAX_LEN + 1));
static_assert(sizeof(TPacketGiveGuildPriv) == 17 && sizeof(TPacketGiveEmpirePriv) == 14);
static_assert(sizeof(TPacketDGChangeGuildPriv) == 18 && sizeof(TPacketDGChangeEmpirePriv) == 15);
static_assert(std::is_same_v<decltype(TPacketMarriageAdd::tMarryTime), int64_t>);

class TestClientManager : public CClientManager {
public:
    using CClientManager::LoadEventFlag;
    using CClientManager::SetEventFlag;
    using CClientManager::SendEventFlagsOnSetup;
};

namespace {
int checks = 0;
uint64_t now = 0;
CAsyncSQL* liveSql = nullptr;
bool failPrepared = false;
std::vector<std::vector<std::string>> flagRows, marriageRows;
struct Frame { uint8_t header; std::vector<char> bytes; };
std::vector<Frame> frames;
SqlParams lastParams;
std::string lastQuery;
uint8_t setupHeader = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
SQLMsg* Result(const std::vector<std::vector<std::string>>& rows = {}) {
    auto msg = std::make_unique<SQLMsg>();
    auto* result = new SQLResult;
    msg->vec_pkResult.push_back(result);
    result->bPrepared = true; result->uiNumRows = rows.size(); result->uiAffectedRows = 1;
    for (const auto& row : rows) {
        std::vector<char*> pointers;
        for (const auto& cell : row) {
            result->preparedCells.push_back(cell);
            pointers.push_back(result->preparedCells.back().data());
        }
        result->preparedRows.push_back(std::move(pointers));
    }
    return msg.release();
}
void Capture(uint8_t header, const void* data, size_t size) {
    const auto* bytes = static_cast<const char*>(data);
    frames.push_back({header, {bytes, bytes + size}});
}
template<class T> T Read(const Frame& frame) {
    Check(frame.bytes.size() == sizeof(T), "wrong interserver frame length");
    T value {}; std::memcpy(&value, frame.bytes.data(), sizeof(value)); return value;
}
CPeer* PeerToken() { static int token; return reinterpret_cast<CPeer*>(&token); }
void EventFlags(TestClientManager& client) {
    for (const auto [header, size] : {std::pair<uint8_t, size_t>{HEADER_DG_SET_EVENT_FLAG, sizeof(TPacketSetEventFlag)},
        {HEADER_DG_PLAYER_LOAD_SUCCESS, sizeof(TPlayerTable)}, {HEADER_DG_MARRIAGE_ADD, sizeof(TPacketMarriageAdd)},
        {HEADER_DG_CHANGE_GUILD_PRIV, sizeof(TPacketDGChangeGuildPriv)}, {HEADER_DG_CHANGE_EMPIRE_PRIV, sizeof(TPacketDGChangeEmpirePriv)}}) {
        Check(time64::ValidDatabasePacketSize(header, size) && !time64::ValidDatabasePacketSize(header, size - 4) &&
            !time64::ValidDatabasePacketSize(header, size + 4), "old/malformed time packet layout accepted");
    }
    flagRows = {{"epoch2038", "2147483648"}, {"epoch2106", "4294967296"},
        {"maximum", "9223372036854775807"}, {"minimum", "-9223372036854775808"},
        {"overflow", "9223372036854775808"}};
    frames.clear(); client.LoadEventFlag();
    Check(frames.size() == 4, "DB flag load dropped wide values or accepted overflow");
    for (size_t i = 0; i < frames.size(); ++i) {
        const auto p = Read<TPacketSetEventFlag>(frames[i]);
        Check(frames[i].header == HEADER_DG_SET_EVENT_FLAG && p.lValue == std::stoll(flagRows[i][1]),
            "loaded DB event flag narrowed on broadcast");
    }
    frames.clear(); client.SendEventFlagsOnSetup(PeerToken());
    Check(frames.size() == 4, "event flag setup lost map entries");
    bool maximum = false, minimum = false;
    for (const auto& frame : frames) {
        const auto p = Read<TPacketSetEventFlag>(frame);
        maximum |= p.lValue == INT64_MAX; minimum |= p.lValue == INT64_MIN;
    }
    Check(maximum && minimum, "DB event map truncated signed limits");
    TPacketSetEventFlag p {};
    Check(EventFlag::BuildPacket("epoch2106", 4294967297LL, p), "event packet fixture");
    client.SetEventFlag(&p);
    Check(lastParams.size() == 2 && lastParams[1].type == SqlParamType::Signed &&
        lastParams[1].i64 == 4294967297LL, "event SQL parameter narrowed");
    const auto count = frames.size();
    std::memset(p.szFlagName, 'x', sizeof(p.szFlagName)); client.SetEventFlag(&p);
    Check(frames.size() == count, "unterminated event flag reached broadcast");
    int64_t value = 9;
    Check(!EventFlag::ParseValue("12junk", value) && value == 9 &&
        !EventFlag::ParseValue("-9223372036854775809", value), "invalid flag integer accepted");
}
void MarriageTime() {
    {
        marriage::CManager manager;
        frames.clear(); failPrepared = true;
        manager.Add(50, 60, "first", "second");
        failPrepared = false;
        Check(!manager.Get(50) && frames.empty(), "failed marriage INSERT published or retained a marriage");
        manager.Add(50, 60, "first", "second");
        Check(manager.Get(50) != nullptr, "failed marriage INSERT prevented retry");
        manager.Remove(50, 60);
    }
    for (const int64_t timestamp : {int64_t{2147483648}, int64_t{4294967296}, int64_t{1} << 40}) {
        marriage::CManager manager;
        now = uint64_t(timestamp);
        marriageRows = {{"10", "20", "0", std::to_string(timestamp), "1", "first", "second"}};
        Check(manager.Initialize() && manager.Get(10)->time == timestamp, "DB marriage load narrowed time");
        frames.clear(); manager.OnSetup(PeerToken());
        Check(!frames.empty() && frames.front().header == HEADER_DG_MARRIAGE_ADD &&
            Read<TPacketMarriageAdd>(frames.front()).tMarryTime == timestamp, "marriage setup timestamp changed");
        manager.Remove(10, 20);
        manager.Add(30, 40, "first", "second");
        Check(lastParams.size() == 3 && lastParams[2].type == SqlParamType::Signed &&
            lastParams[2].i64 == timestamp && manager.Get(30)->time == timestamp, "marriage INSERT narrowed time");
        manager.ReadyWedding(1, 30, 40);
        frames.clear(); now += 4; manager.Update();
        Check(frames.empty(), "wide wedding started early");
        ++now; manager.Update();
        Check(frames.size() == 1 && frames.back().header == HEADER_DG_WEDDING_START, "wide wedding missed start deadline");
        frames.clear(); now += 3600; manager.Update();
        Check(frames.size() == 2 && frames[0].header == HEADER_DG_WEDDING_END &&
            frames[1].header == HEADER_DG_MARRIAGE_REMOVE && !manager.Get(30),
            "wide wedding end failed to retire engagement");
    }
}
void PrivilegeTime() {
    for (const int64_t timestamp : {int64_t{2147483648}, int64_t{4294967296}, int64_t{1} << 40, INT64_MAX - 3600}) {
        CPrivManager privileges; now = uint64_t(timestamp);
        privileges.AddGuildPriv(42, 1, 25, 3600);
        privileges.AddEmpirePriv(1, 1, 50, 3600);
        frames.clear(); privileges.SendPrivOnSetup(PeerToken());
        Check(frames.size() == 2 && Read<TPacketDGChangeEmpirePriv>(frames[0]).end_time_sec == timestamp + 3600 &&
            Read<TPacketDGChangeGuildPriv>(frames[1]).end_time_sec == timestamp + 3600, "DB privilege deadline narrowed on setup");
        now += 3599; privileges.Update(); frames.clear(); privileges.SendPrivOnSetup(PeerToken());
        Check(frames.size() == 2, "wide privilege expired early");
        ++now; privileges.Update(); frames.clear(); privileges.SendPrivOnSetup(PeerToken());
        Check(frames.empty(), "wide privilege failed to expire at deadline");
    }
    CPrivManager privileges; now = uint64_t(INT64_MAX - 10);
    privileges.AddCharPriv(42, 1, 50);
    privileges.Update(); frames.clear(); privileges.SendPrivOnSetup(PeerToken());
    Check(frames.size() == 1 && frames[0].header == HEADER_DG_CHANGE_CHARACTER_PRIV,
        "character privilege overflowed into an already expired deadline");
    now = uint64_t(INT64_MAX); privileges.Update(); frames.clear(); privileges.SendPrivOnSetup(PeerToken());
    Check(frames.empty(), "saturated character privilege failed to expire");
}
void PlayerSave() {
    TPlayerTable player {}; player.id = 1; player.horse.dwHorseHealthDropTime = UINT64_MAX;
    char query[QUERY_MAX_LEN] {};
    const auto length = CreatePlayerSaveQuery(query, sizeof(query), &player);
    Check(length < sizeof(query) && std::string(query).find("horse_hp_droptime = 18446744073709551615,") != std::string::npos,
        "production player UPDATE truncated horse BIGINT");
}
std::string TableDDL(const std::string& schema, const std::string& name, const std::string& temporary) {
    const auto begin = schema.find("CREATE TABLE IF NOT EXISTS `player`.`" + name + "`");
    const auto columns = schema.find('(', begin);
    const auto engine = schema.find(") ENGINE=InnoDB", columns);
    const auto end = schema.find(';', engine);
    Check(begin != std::string::npos && columns != std::string::npos && end != std::string::npos, "missing schema table");
    return "CREATE TEMPORARY TABLE " + temporary + " " + schema.substr(columns, end + 1 - columns);
}
void SqlChecks(TestClientManager& client, unsigned int port, const char* schemaPath) {
    std::ifstream file(schemaPath); Check(bool(file), "schema file unavailable");
    const std::string schema((std::istreambuf_iterator<char>(file)), {});
    CAsyncSQL sql; sql.SetTimeouts(5);
    Check(sql.Setup("127.0.0.1", "root", "", "auriga_time64_test", "utf8mb4", true, port), "isolated time SQL connection failed");
    const auto run = [&](const std::string& query) {
        std::unique_ptr<SQLMsg> result(sql.DirectQuery(query.c_str()));
        if (!result || result->uiSQLErrno)
            std::cerr << "SQL error " << (result ? result->uiSQLErrno : 0) << ": " << query.substr(0, 160) << '\n';
        Check(result && !result->uiSQLErrno, "time SQL statement failed"); return result;
    };
    run(TableDDL(schema, "quest", "quest_test"));
    run(TableDDL(schema, "player", "player_test"));
    run(TableDDL(schema, "marriage", "marriage"));
    liveSql = &sql;
    for (const int64_t value : {INT64_MIN, int64_t{2147483648}, int64_t{4294967296}, INT64_MAX}) {
        TPacketSetEventFlag packet {}; EventFlag::BuildPacket("sql_time64", value, packet);
        client.SetEventFlag(&packet);
        auto read = run("SELECT lValue FROM quest_test WHERE dwPID=0 AND szName='sql_time64'");
        Check(read->Get()->uiNumRows == 1 && std::stoll(read->Get()->FetchRow()[0]) == value, "real event SQL round-trip truncated");
    }
    frames.clear(); client.LoadEventFlag();
    Check(frames.size() == 1 && Read<TPacketSetEventFlag>(frames.back()).lValue == INT64_MAX, "real event reload lost signed maximum");
    run("INSERT INTO player_test(id,account_id,name) VALUES(1,7,'time64')");
    for (const uint64_t deadline : {uint64_t{2147483648}, uint64_t{4294967296}, UINT64_MAX}) {
        TPlayerTable player {}; player.id = 1; player.horse.dwHorseHealthDropTime = deadline;
        char query[QUERY_MAX_LEN] {}; CreatePlayerSaveQuery(query, sizeof(query), &player); run(query);
        auto read = run("SELECT horse_hp_droptime FROM player_test WHERE id=1");
        const auto row = read->Get()->FetchRow();
        Check(row && std::stoull(row[0]) == deadline, "real horse UPDATE/read narrowed deadline");
    }
    marriage::CManager manager; now = uint64_t{1} << 40;
    manager.Add(30, 40, "first", "second");
    auto read = run("SELECT time FROM marriage WHERE pid1=30 AND pid2=40");
    Check(read->Get()->uiNumRows == 1 && std::stoull(read->Get()->FetchRow()[0]) == now, "real marriage INSERT/read narrowed time");
    manager.Remove(30, 40); liveSql = nullptr;
}
} // namespace

// Production Database state/packet/SQL builders, with controlled clock,
// transport and SQL. --sql delegates the same writes to an isolated real DB.
CNetBase::CNetBase() = default;
CNetBase::~CNetBase() = default;
CClientManager::CClientManager() = default;
CClientManager::~CClientManager() = default;
CDBManager::CDBManager() = default;
CDBManager::~CDBManager() = default;
#ifdef __ENABLE_NEW_OFFLINESHOP__
offlineshop::CShopCache::CShopCache() = default;
offlineshop::CShopCache::~CShopCache() = default;
offlineshop::CSafeboxCache::CSafeboxCache() = default;
offlineshop::CSafeboxCache::~CSafeboxCache() = default;
offlineshop::COfferCache::COfferCache() = default;
offlineshop::COfferCache::~COfferCache() = default;
offlineshop::CAuctionCache::CAuctionCache() = default;
offlineshop::CAuctionCache::~CAuctionCache() = default;
#endif
const char* GetTablePostfix() { return "_test"; }
uint64_t CClientManager::GetCurrentTime() { return now; }
void CClientManager::ForwardPacket(uint8_t header, const void* data, int size, uint8_t, CPeer*) { Capture(header, data, size); }
void CPeer::EncodeHeader(uint8_t header, uint32_t, uint32_t) { setupHeader = header; }
void CPeerBase::Encode(const void* data, uint32_t size) { Capture(setupHeader, data, size); }
SQLMsg* CDBManager::DirectQuery(const char* query, int) {
    if (liveSql) return liveSql->DirectQuery(query);
    return Result(std::string_view(query).find("SELECT szName") == 0 ? flagRows :
        std::string_view(query).find("SELECT pid1") == 0 ? marriageRows : std::vector<std::vector<std::string>>{});
}
SQLMsg* CDBManager::DirectQueryPrepared(const char* query, const SqlParams& params, int) {
    lastQuery = query; lastParams = params;
    if (failPrepared) {
        auto* result = Result(); result->uiSQLErrno = 1064;
        result->Get()->uiAffectedRows = UINT64_MAX; return result;
    }
    return liveSql ? liveSql->DirectQueryPrepared(query, params) : Result();
}
void CDBManager::QueryPrepared(const char* query, const SqlParams& params, int slot) {
    std::unique_ptr<SQLMsg> result(DirectQueryPrepared(query, params, slot));
    Check(result && !result->uiSQLErrno, "prepared time write failed");
}
void CDBManager::AsyncQuery(const char*, int) {}
unsigned long CDBManager::EscapeString(void* dst, const void* src, unsigned long length, int) {
    if (liveSql) return liveSql->EscapeString(static_cast<char*>(dst), 8193, static_cast<const char*>(src), length);
    auto* out = static_cast<char*>(dst);
    for (unsigned long i = 0; i < length; ++i) {
        const char ch = static_cast<const char*>(src)[i];
        if (ch == '\0') { *out++ = '\\'; *out++ = '0'; }
        else if (ch == '\'' || ch == '"' || ch == '\\') { *out++ = '\\'; *out++ = ch; }
        else *out++ = ch;
    }
    *out = '\0'; return static_cast<unsigned long>(out - static_cast<char*>(dst));
}
std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("database-time-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
int main(int argc, char** argv) {
    try {
        CDBManager database;
        { TestClientManager client; EventFlags(client); MarriageTime(); PrivilegeTime(); PlayerSave(); }
        if (argc == 4 && std::string(argv[1]) == "--sql") {
            TestClientManager client; SqlChecks(client, static_cast<unsigned int>(std::stoul(argv[2])), argv[3]);
        }
        std::cout << "Database time checks passed: " << checks << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
