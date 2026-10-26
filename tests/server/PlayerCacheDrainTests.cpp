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
    for (uint8_t part : {uint8_t(HEADER_DG_PLAYER_LOAD_SUCCESS), uint8_t(HEADER_DG_ITEM_LOAD),
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
    CItemCache item;
    std::array<CItemCache *, 1> items{&item};
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
        TPlayerItem i{};
        i.id = 200;
        i.owner = 100;
        i.window = INVENTORY;
        i.count = 3;
        i.vnum = 19;
        item.Put(&i);
    }
    pc::View View()
    {
        return {&player, items};
    }
};
void Unit()
{
    static_assert(!std::is_copy_constructible_v<CItemCache>);
    Fixture f;
    Check(f.player.HasPendingQuery() && f.item.HasPendingQuery(), "dirty cache not recorded");
    const auto playerVersion = f.player.Version();
    const auto itemVersion = f.item.Version();
    f.player.Get();
    f.item.Get();
    Check(playerVersion == f.player.Version() && itemVersion == f.item.Version(),
          "cache read changed version");
    writes.clear();
    f.player.Flush();
    f.item.Flush();
    Check(writes.size() == 2 && writes[0].starts_with("UPDATE player_cache_drain SET ") &&
              writes[1].starts_with("REPLACE INTO item_cache_drain "),
          "production cache DML did not run");
    Check(!f.player.HasPendingQuery() && !f.item.HasPendingQuery() && playerVersion == f.player.Version(),
          "flush evicted or changed a clean cache");
    f.player.Flush();
    f.item.Flush();
    Check(writes.size() == 2, "clean cache flushed twice");
    auto row = *f.item.Get(false);
    row.count = 4;
    f.item.Put(&row);
    row.count = 3;
    f.item.Put(&row);
    Check(f.item.Version().mutation == itemVersion.mutation + 2, "A-B-A cache mutation disappeared");
    f.item.Delete();
    Check(f.item.Version().mutation == itemVersion.mutation + 3 && !f.item.Get(false)->vnum &&
              writes.back().starts_with("DELETE FROM item_cache_drain"),
          "tombstone mutation or delete missing");
    f.item.Delete();
    Check(f.item.Version().mutation == itemVersion.mutation + 3, "duplicate delete changed version");
    CPlayerTableCache replacement;
    auto player = *f.player.Get(false);
    replacement.Put(&player, true);
    Check(replacement.Version().instance != playerVersion.instance && !replacement.HasPendingQuery(),
          "new cache instance reused old identity");
    CAsyncSQL a, b;
    const std::array origins{a.QueueOrigin(), b.QueueOrigin()};
    bool enqueued = false;
    const auto enqueue = [&]
    {
        enqueued = true;
        return std::array{a.DrainQueue(), b.DrainQueue()};
    };
    for (int bad = 0; bad < 9; ++bad)
    {
        auto lease = f.lease;
        auto challenge = Token(3);
        auto view = f.View();
        auto baseline = origins;
        std::array<CItemCache *, 2> duplicate{&f.item, &f.item};
        std::vector<CItemCache *> excessive(pc::MaximumItems + 1, &f.item);
        if (bad == 0)
            lease.peer++;
        if (bad == 1)
            challenge = {};
        if (bad == 2)
            view.player = nullptr;
        if (bad == 3)
            baseline[1] = baseline[0];
        if (bad == 4)
            baseline[0].sequence = 1;
        if (bad == 5)
            baseline[0].unconfirmed = 1;
        if (bad == 6)
            view.items = duplicate;
        if (bad == 7)
            view.items = excessive;
        if (bad == 8)
            lease.key.character++;
        const auto count = writes.size();
        Check(!pc::Drain::Begin(f.authority, lease, challenge, view, baseline, enqueue) &&
                  writes.size() == count && !enqueued,
              "invalid drain flushed cache or queued work");
    }
    auto foreign = *f.item.Get(false);
    foreign.owner++;
    f.item.Put(&foreign);
    Check(!pc::Drain::Begin(f.authority, f.lease, Token(3), f.View(), origins, enqueue) && !enqueued,
          "foreign cache owner was flushed");
    foreign.owner--;
    f.item.Put(&foreign);
    ps::Authority incomplete;
    auto unready = *incomplete.Open(7, 50, 900, 100, Token(1), Token(2));
    Check(!pc::Drain::Begin(incomplete, unready, Token(3), f.View(), origins, enqueue) && !enqueued,
          "unfinished load admitted cache drain");
    ps::Authority legacy;
    auto oldProtocol = *legacy.Open(7, 50, 900, 100);
    Loaded(legacy, oldProtocol);
    Check(!pc::Drain::Begin(legacy, oldProtocol, Token(3), f.View(), origins, enqueue) && !enqueued,
          "legacy login admitted versioned cache drain");
    std::vector<std::unique_ptr<CItemCache>> owned;
    std::vector<CItemCache *> full;
    for (size_t i = 0; i < pc::MaximumItems; ++i)
    {
        auto cache = std::make_unique<CItemCache>();
        TPlayerItem row{};
        row.id = 1000 + static_cast<uint32_t>(i);
        row.owner = 100;
        row.vnum = 19;
        row.window = INVENTORY;
        row.count = 1;
        cache->Put(&row, true);
        full.push_back(cache.get());
        owned.push_back(std::move(cache));
    }
    auto boundary = pc::Drain::Begin(f.authority, f.lease, Token(4), {&f.player, full}, origins, enqueue);
    Check(boundary && enqueued, "exact 1024-item native cache view rejected");
    enqueued = false;
    auto drain = pc::Drain::Begin(f.authority, f.lease, Token(3), f.View(), origins, enqueue);
    Check(drain && enqueued && drain->Poll(f.authority, f.View()).status == pc::Status::Blocked,
          "unavailable SQL worker supplied successful cache proof");
    f.authority.InvalidatePeer(7);
    Check(drain->Poll(f.authority, f.View()).status == pc::Status::Stale,
          "disconnected native peer retained cache authority");
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
    for (const auto &name : {"player", "item"})
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
void SnapshotBatch(Fixture &f)
{
    namespace snapshot = auriga::player_cache_snapshot;
    snapshot::Snapshot state;
    state.account = f.lease.key.account;
    state.player = *f.player.Get(false);
    state.items = {*f.item.Get(false)};
    state.player.gold = 777;
    state.player.exp = 4321;
    state.items[0].count = 9;
    state.items[0].alSockets[0] = 42;
    state.items[0].aAttr[0] = {1, 25};
    const auto batch = snapshot::Batch(f.lease.key, Token(20), f.lease.descriptor, state);
    Check(batch.has_value(), "native snapshot batch unavailable");
    size_t at = 0, count = 0;
    const auto integer = [&]
    {
        Check(at + 4 <= batch->size(), "truncated snapshot header");
        uint32_t n = 0;
        for (int shift = 0; shift < 32; shift += 8)
            n |= uint32_t((*batch)[at++]) << shift;
        return n;
    };
    while (at < batch->size())
    {
        const auto header = (*batch)[at++];
        const auto descriptor = integer(), size = integer();
        Check(size <= batch->size() - at, "snapshot body exceeds batch");
        if (count == 0)
        {
            Check(header == HEADER_GD_PLAYER_SAVE && descriptor == f.lease.descriptor &&
                      size == sizeof(TPlayerTable),
                  "player snapshot not first");
            TPlayerTable row{};
            std::memcpy(&row, batch->data() + at, sizeof(row));
            f.player.Put(&row);
        }
        else if (count == 1)
        {
            Check(header == HEADER_GD_ITEM_SAVE && descriptor == 0 && size == sizeof(TPlayerItem),
                  "item snapshot missing before drain");
            TPlayerItem row{};
            std::memcpy(&row, batch->data() + at, sizeof(row));
            f.item.Put(&row);
        }
        else
        {
            const auto request = wire::DecodeRequest({batch->data() + at, size});
            Check(header == HEADER_GD_PLAYER_CACHE_REQUEST && descriptor == f.lease.descriptor && request &&
                      request->key == f.lease.key && request->challenge == Token(20) &&
                      request->action == wire::Action::Begin && at + size == batch->size(),
                  "drain preceded snapshot or lost native identity");
        }
        at += size;
        ++count;
    }
    Check(count == 3, "snapshot/drain batch had unexpected frames");
}

void Integration(unsigned port, const std::string &schema, const char *path)
{
    Check(schema.starts_with("auriga_mission_sql_test_") &&
              schema.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == schema.npos,
          "private schema required");
    CAsyncSQL admin;
    Connect(admin, port, "", true);
    Text(admin, "CREATE DATABASE " + schema + " CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
    Text(admin, "USE " + schema);
    Schema(admin, path);
    escapeConnection = &admin;
    for (int scenario = 0; scenario < 14; ++scenario)
    {
        CAsyncSQL main, async;
        Connect(main, port, schema, false);
        Connect(async, port, schema, false);
        mainWorker = &main;
        Fixture f;
        if (scenario == 13)
            SnapshotBatch(f);
        const std::array origins{main.QueueOrigin(), async.QueueOrigin()};
        const auto enqueue = [&] { return std::array{main.DrainQueue(), async.DrainQueue()}; };
        // Hold BOTH real workers before their FIFO boundaries. No timing-only sleep assertion.
        const auto lock = schema + "_hold";
        Check(Number(admin, "SELECT GET_LOCK('" + lock + "',0)") == 1, "test lock unavailable");
        if (scenario != 12)
            main.AsyncQuery(("SELECT GET_LOCK('" + lock + "',5), RELEASE_LOCK('" + lock + "')").c_str());
        async.AsyncQuery(("SELECT GET_LOCK('" + lock + "',5), RELEASE_LOCK('" + lock + "')").c_str());
        if (scenario == 2)
            async.AsyncQuery("UPDATE absent_cache_drain_table SET missing=1");
        if (scenario == 3)
            Text(admin, "CREATE TRIGGER fail_cache_drain BEFORE UPDATE ON player_cache_drain FOR EACH ROW "
                        "SIGNAL SQLSTATE '45000'");
        if (scenario == 10)
            f.item.Delete();
        const auto enqueueActual = [&]
        {
            auto tickets = enqueue();
            if (scenario == 11)
                tickets[1] = tickets[0];
            return tickets;
        };
        auto drain = pc::Drain::Begin(f.authority, f.lease, Token(20), f.View(), origins, enqueueActual);
        Check(drain.has_value(), "valid native cache drain refused");
        wire::Client game;
        Check(game.Begin(f.lease.key, 12345, Token(20)), "native cache request could not start");
        const auto request = wire::DecodeRequest(wire::Encode(*game.Next()));
        Check(request && request->key == f.lease.key && request->challenge == drain->Challenge(),
              "native wire request lost session/challenge context");
        auto pending = drain->Poll(f.authority, f.View());
        const auto pendingWire = wire::DecodeReceipt(wire::Encode(pc::ToWire(pending)));
        Check(pendingWire && game.Accept(*pendingWire, f.lease.key, 12345),
              "pending Database receipt did not reach native client");
        Check(pending.status == (scenario == 11 ? pc::Status::Blocked : pc::Status::Pending) &&
                  pending.challenge == Token(20) && pending.lease == f.lease,
              "flush claimed success before both workers finished");
        Check(!f.player.HasPendingQuery() && !f.item.HasPendingQuery() &&
                  f.item.Get(false)->count == (scenario == 13 ? 9 : 3),
              "drain evicted cache or failed to submit dirty rows");
        if (scenario == 1)
        {
            auto row = *f.item.Get(false);
            row.count = 4;
            f.item.Put(&row);
            row.count = 3;
            f.item.Put(&row);
            f.item.Flush(); // Even an equal, clean A-B-A postimage must invalidate the old proof.
        }
        if (scenario == 4)
            f.authority.InvalidateAccount(900);
        if (scenario == 5)
            f.item.Delete();
        if (scenario == 6)
        {
            auto row = *f.player.Get(false);
            row.gold++;
            f.player.Put(&row);
            f.player.Flush();
        }
        if (scenario == 7)
            f.items[0] = nullptr;
        if (scenario == 8)
        {
            f.authority.InvalidatePeer(7);
            const auto replacement = f.authority.Open(7, 50, 900, 100, Token(1), Token(2));
            Check(replacement && replacement->key != f.lease.key, "relogin reused session identity");
            Loaded(f.authority, *replacement);
        }
        if (scenario == 12)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            do
            {
                pending = drain->Poll(f.authority, f.View());
                if (pending.lanes[0].status == SqlQueueDrainStatus::Complete)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (std::chrono::steady_clock::now() < deadline);
            Check(pending.status == pc::Status::Pending &&
                      pending.lanes[0].status == SqlQueueDrainStatus::Complete &&
                      pending.lanes[1].status == SqlQueueDrainStatus::Pending,
                  "one completed worker was accepted as a two-lane drain");
        }
        Check(Number(admin, "SELECT RELEASE_LOCK('" + lock + "')") == 1, "test unlock failed");
        const auto result = Await(*drain, f);
        const auto wireResult = wire::DecodeReceipt(wire::Encode(pc::ToWire(result)));
        Check(wireResult && wireResult->key == f.lease.key && wireResult->challenge == Token(20),
              "real SQL completion lost native wire context");
        if (scenario == 4 || scenario == 8)
        {
            game.Invalidate();
            Check(!game.Accept(*wireResult, f.lease.key, 12345),
                  "old wire completion survived native disconnect");
        }
        else if (scenario == 11)
            Check(!game.Accept(*wireResult, f.lease.key, 12345), "terminal blocked receipt applied twice");
        else
            Check(game.Accept(*wireResult, f.lease.key, 12345) && !game.Waiting(),
                  "real SQL completion was not accepted once by native client");
        if (scenario == 0 || scenario == 9 || scenario == 10 || scenario == 12 || scenario == 13)
        {
            Check(result.status == pc::Status::Flushed && result.failure == pc::Failure::None &&
                      result.lanes[0].position.sequence && result.lanes[1].position.sequence &&
                      result.lanes[0].position.lane != result.lanes[1].position.lane,
                  "successful cache flush not confirmed by both workers");
            Check(Number(admin, "SELECT gold FROM player_cache_drain WHERE id=100") ==
                          (scenario == 13 ? 777 : 500) &&
                      (scenario == 10
                           ? Number(admin, "SELECT COUNT(*) FROM item_cache_drain WHERE id=200") == 0
                           : Number(admin, "SELECT count FROM item_cache_drain WHERE id=200") ==
                                 (scenario == 13 ? 9 : 3)),
                  "confirmed production cache DML did not persist canonical rows");
            if (scenario == 13)
                Check(Number(admin, "SELECT exp FROM player_cache_drain WHERE id=100") == 4321 &&
                          Number(admin, "SELECT socket0 FROM item_cache_drain WHERE id=200") == 42 &&
                          Number(admin, "SELECT attrvalue0 FROM item_cache_drain WHERE id=200") == 25,
                      "snapshot resource/item fields were not persisted by production DML");
            if (scenario == 9)
            {
                CPlayerTableCache replacement;
                auto row = *f.player.Get(false);
                replacement.Put(&row, true);
                Check(drain->Poll(f.authority, {&replacement, f.items}).failure == pc::Failure::CacheChanged,
                      "recreated cache inherited completed proof");
                Check(drain->Poll(f.authority, f.View()).status == pc::Status::Blocked,
                      "blocked proof became successful again");
            }
        }
        else if (scenario == 4 || scenario == 8)
            Check(result.status == pc::Status::Stale, "old cache SQL callback regained relogin authority");
        else
            Check(result.status == pc::Status::Blocked &&
                      result.failure == (scenario == 11                   ? pc::Failure::InvalidLanes
                                         : scenario == 2 || scenario == 3 ? pc::Failure::SqlUnconfirmed
                                                                          : pc::Failure::CacheChanged),
                  "cache mutation/SQL failure was acknowledged as successful");
        if (scenario == 3)
            Text(admin, "DROP TRIGGER fail_cache_drain");
        mainWorker = nullptr;
        main.Quit();
        async.Quit();
        SQLMsg *message = nullptr;
        while (main.PopResult(&message))
            delete message;
    }
    escapeConnection = nullptr;
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
