#include "../../SRC/Server/Database/ItemAwardDelivery.hpp"
#include "../../SRC/Server/Database/ItemAwardScan.hpp"
#include <set>
#include <Core/Logging.hpp>
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <latch>
#include <memory>
#include <source_location>
#include <sstream>
#include <thread>

namespace award = auriga::item_award;
namespace
{
using Message = std::unique_ptr<SQLMsg>;
size_t checks = 0;
unsigned int port = 0;
std::string database;
void Check(bool value, const char *why, const std::source_location line = std::source_location::current())
{
    ++checks;
    if (!value)
        throw std::runtime_error(std::string(why) + " at " + std::to_string(line.line()));
}
award::Request Request(uint32_t id, uint32_t account = 7)
{
    award::Request request;
    request.source.id = id;
    request.source.login = "test'bound";
    request.source.vnum = 50011;
    request.source.count = 2;
    request.source.sockets = {UINT32_MAX, 2, 123};
    request.source.attributes[0] = {7, -32768};
    request.source.attributes[7] = {255, 32767};
    request.item.id = id + 10000001;
    request.item.account = account;
    request.item.count = request.source.count;
    request.item.vnum = request.source.vnum;
    request.item.sockets = {-1, 2, 123};
    request.item.attributes = request.source.attributes;
    request.rows = 9;
    request.itemHeight = 1;
    return request;
}
void Unit()
{
    auto request = Request(1);
    Check(bool(award::Deliver(request)), "valid award plan rejected");
    Check(award::Deliver(request).statements.size() == 7, "unexpected transaction shape");
    Check(bool(award::Deliver(request, "_test")), "valid postfix rejected");
    for (const auto &postfix : std::array<std::string, 4>{";DROP", "x`", " x", std::string(33, 'x')})
        Check(!award::Deliver(request, postfix), "unsafe table identifier accepted");
    Check(award::Deliver(request).statements[3].query.find("test'bound") == std::string::npos,
          "login interpolated into SQL");
    const auto reject = [](award::Request invalid)
    {
        auto plan = award::Deliver(invalid);
        Check(!plan && plan.statements.empty() && !plan.errors.empty(), "invalid plan retained writes");
    };
    auto changed = request;
    changed.item.id = 0;
    reject(changed);
    changed = request;
    changed.item.account = 0;
    reject(changed);
    changed = request;
    changed.source.id = 0;
    reject(changed);
    changed = request;
    changed.source.login = std::string("bad\0login", 9);
    reject(changed);
    changed = request;
    changed.item.vnum++;
    reject(changed);
    for (auto count : {0u, 50001u, UINT32_MAX})
    {
        changed = request;
        changed.item.count = count;
        reject(changed);
    }
    changed = request;
    changed.item.count = 50000;
    Check(bool(award::Deliver(changed)), "maximum stack rejected");
    for (uint16_t rows : {0, 1, 8, 87, 65535})
    {
        changed = request;
        changed.rows = rows;
        reject(changed);
    }
    for (uint16_t position : {45, 65535})
    {
        changed = request;
        changed.item.position = position;
        reject(changed);
    }
    changed = request;
    changed.itemHeight = 0;
    reject(changed);
    changed.itemHeight = 255;
    reject(changed);
    changed = request;
    changed.storage = {{100, 50010, 0, 2}};
    changed.item.position = 5;
    reject(changed); // The second occupied cell, not only the top-left position.
    changed.item.position = 1;
    Check(bool(award::Deliver(changed)), "adjacent vertical placement rejected");
    changed.storage.push_back(changed.storage.front());
    reject(changed);
    changed = request;
    for (uint16_t i = 0; i < 45; ++i)
        changed.storage.push_back({uint32_t(100 + i), 50010, i, 1});
    reject(changed); // Full box: no SQL writes, award stays pending.
    changed.rows = 81;
    changed.item.position = 404;
    changed.storage.clear();
    for (uint16_t i = 0; i < 404; ++i)
        changed.storage.push_back({uint32_t(100 + i), 50010, i, 1});
    auto maximum = award::Deliver(changed);
    Check(bool(maximum), "bounded maximum container rejected");
    size_t parameters = 0;
    for (const auto &step : maximum.statements)
    {
        parameters += step.params.size();
        Check(step.query.size() <= SqlTransactionLimits::QueryBytes, "query exceeds SQL executor bound");
    }
    Check(parameters <= SqlTransactionLimits::Parameters, "batch exceeds parameter budget");
    SQLMsg missing;
    Check(!award::Confirmed(missing) && award::DecodeReceipt(missing).state == award::ReceiptState::SqlError,
          "unconfirmed receipt accepted");
}
void Connect(CAsyncSQL &sql, unsigned int target = 0, bool direct = true)
{
    sql.SetTimeouts(3);
    sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1", "root", "", database.c_str(), "utf8mb4", direct, target ? target : port),
          "private connection failed");
    if (!direct)
        Check(sql.WaitForConnection(std::chrono::seconds(5)), "private worker failed");
}
void Text(CAsyncSQL &sql, const std::string &query)
{
    Message result(sql.DirectQuery(query.c_str()));
    if (result && result->uiSQLErrno)
        std::cerr << "Setup SQL errno=" << result->uiSQLErrno << ": " << query << '\n';
    Check(result && !result->uiSQLErrno, "setup SQL failed");
}
uint64_t Number(CAsyncSQL &sql, const char *query, SqlParams params = {})
{
    Message result(sql.DirectQueryPrepared(query, params));
    Check(result && !result->uiSQLErrno && result->Get(), "scalar failed");
    auto row = result->Get()->FetchRow();
    Check(row && row[0], "scalar missing");
    return std::stoull(row[0]);
}
void Source(CAsyncSQL &sql, const award::Source &source)
{
    std::string query = "INSERT INTO item_award(id,login,vnum,count,mall";
    SqlParams params = {SqlParam::UInt(source.id), SqlParam::Text(source.login), SqlParam::UInt(source.vnum),
                        SqlParam::UInt(source.count), SqlParam::UInt(source.mall)};
    for (size_t i = 0; i < source.sockets.size(); ++i)
    {
        query += ",socket" + std::to_string(i);
        params.push_back(SqlParam::UInt(source.sockets[i]));
    }
    for (size_t i = 0; i < source.attributes.size(); ++i)
    {
        query += ",attrtype" + std::to_string(i) + ",attrvalue" + std::to_string(i);
        params.push_back(SqlParam::UInt(source.attributes[i].type));
        params.push_back(SqlParam::Int(source.attributes[i].value));
    }
    query += ") VALUES(";
    for (size_t i = 0; i < params.size(); ++i)
        query += i ? ",?" : "?";
    query += ')';
    Message result(sql.DirectQueryPrepared(query.c_str(), params));
    Check(!result->uiSQLErrno && result->Get()->uiAffectedRows == 1, "source insert failed");
}
Message Execute(CAsyncSQL &sql, const award::Plan &plan, bool success = true)
{
    Check(bool(plan), "invalid delivery plan");
    Message result(sql.DirectTransactionPrepared(plan.statements));
    if (award::Confirmed(*result) != success)
        std::cerr << "SQL errno=" << result->uiSQLErrno << " step=" << result->transactionFailedStep << '\n';
    Check(award::Confirmed(*result) == success, "unexpected delivery outcome");
    return result;
}
award::Receipt Receipt(CAsyncSQL &sql, const award::Source &source, const std::string &postfix = {})
{
    Message result(sql.DirectTransactionPrepared(award::Resolve(source, postfix)));
    return award::DecodeReceipt(*result);
}
void Pending(CAsyncSQL &sql, const award::Request &request)
{
    Check(Receipt(sql, request.source).state == award::ReceiptState::Pending, "failed award was consumed");
    Check(Number(sql, "SELECT COUNT(*) FROM item WHERE id=?", {SqlParam::UInt(request.item.id)}) == 0,
          "failed delivery left an item");
}
void Tables(CAsyncSQL &sql, const char *path)
{
    std::ifstream file(path, std::ios::binary);
    Check(bool(file), "canonical schema missing");
    const std::string source((std::istreambuf_iterator<char>(file)), {});
    for (const auto &name : {"item", "item_award", "safebox"})
    {
        const auto prefix = std::string("CREATE TABLE IF NOT EXISTS `player`.`") + name + "` (";
        const size_t begin = source.find(prefix);
        Check(begin != std::string::npos, "canonical table missing");
        const size_t end = source.find(';', begin);
        Check(end != std::string::npos, "canonical table incomplete");
        auto ddl = source.substr(begin, end - begin);
        ddl.replace(ddl.find("`player`."), 9, "");
        Text(sql, ddl);
    }
    Check(Number(sql, "SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() "
                      "AND TABLE_NAME='item' AND COLUMN_NAME='id' AND EXTRA LIKE '%auto_increment%'") == 0,
          "fixture accidentally made item ID auto-increment");
    Text(sql, "CREATE TABLE account_mission_pass(account_id INT PRIMARY KEY,marker INT) ENGINE=InnoDB");
    Text(sql, "INSERT INTO account_mission_pass VALUES(1,123)");
}
void Basic(CAsyncSQL &sql)
{
    auto request = Request(1);
    Source(sql, request.source);
    auto plan = award::Deliver(request);
    auto result = Execute(sql, plan);
    const auto insert = result->transactionResultOffsets[result->transactionResultOffsets.size() - 2];
    Check(result->vec_pkResult[insert]->uiInsertID == 0, "explicit ID unexpectedly generated an insert_id");
    auto receipt = Receipt(sql, request.source);
    Check(receipt.state == award::ReceiptState::Taken && receipt.item == request.item.id,
          "committed receipt missing explicit item ID");
    Check(Number(sql,
                 "SELECT COUNT(*) FROM item WHERE id=? AND owner_id=7 AND window='SAFEBOX' "
                 "AND pos=0 AND vnum=50011 AND count=2 AND lockedattr=-1 AND socket0=-1 "
                 "AND socket1=2 AND socket2=123 AND attrtype0=7 AND attrvalue0=-32768 "
                 "AND attrtype7=255 AND attrvalue7=32767",
                 {SqlParam::UInt(request.item.id)}) == 1,
          "canonical item payload changed");
    Execute(sql, plan, false);
    request.item.id++;
    Execute(sql, award::Deliver(request), false);
    Check(Number(sql, "SELECT COUNT(*) FROM item WHERE owner_id=7") == 1, "retry duplicated award");
    // Historical receipt must survive moving/deleting the item; it is never a
    // request to recreate the original item in its original cell.
    Text(sql, "DELETE FROM item WHERE owner_id=7");
    Check(Receipt(sql, request.source).state == award::ReceiptState::Taken, "deleted item lost receipt");
    Execute(sql, award::Deliver(request), false);
    Check(Number(sql, "SELECT COUNT(*) FROM item WHERE owner_id=7") == 0, "consumed item was recreated");
}
void RollbackAndStale(CAsyncSQL &sql)
{
    auto request = Request(2, 8);
    Source(sql, request.source);
    auto plan = award::Deliver(request);
    // Fail after the real item insert but BEFORE its taken update.
    plan.statements.insert(plan.statements.end() - 1, {"SELECT 1 WHERE 0", {}, {}, 1});
    Execute(sql, plan, false);
    Pending(sql, request);
    // Fail after BOTH writes: neither one is durable without COMMIT.
    plan = award::Deliver(request);
    plan.statements.push_back({"SELECT 1 WHERE 0", {}, {}, 1});
    Execute(sql, plan, false);
    Pending(sql, request);
    for (const auto &column : {"count", "vnum", "socket1", "attrvalue0", "mall"})
    {
        auto original = request;
        Text(sql,
             "UPDATE item_award SET " + std::string(column) + '=' + std::string(column) + "+1 WHERE id=2");
        Execute(sql, award::Deliver(request), false);
        Text(sql,
             "UPDATE item_award SET " + std::string(column) + '=' + std::string(column) + "-1 WHERE id=2");
        Pending(sql, original);
    }
    Text(sql, "INSERT INTO item(id,owner_id,window,pos,vnum,count) VALUES(200,8,'SAFEBOX',0,50010,1)");
    Execute(sql, award::Deliver(request), false); // Same count snapshot is not enough.
    request.storage = {{200, 50010, 1, 1}};
    request.item.position = 2;
    Execute(sql, award::Deliver(request), false); // Correct ID, wrong position.
    request.storage.front().position = 0;
    request.storage.front().vnum++;
    Execute(sql, award::Deliver(request), false);
    request.storage.front().vnum = 50010;
    auto collision = request;
    collision.item.id = 200;
    Check(!award::Deliver(collision), "snapshot allocator collision accepted");
    Text(sql, "INSERT INTO item(id,owner_id,window,pos,vnum,count) VALUES(201,999,'INVENTORY',0,50010,1)");
    collision = request;
    collision.item.id = 201;
    Execute(sql, award::Deliver(collision), false);
    Check(Receipt(sql, request.source).state == award::ReceiptState::Pending, "ID collision consumed award");
    Execute(sql, award::Deliver(request));
}
void FullBox(CAsyncSQL &sql)
{
    auto request = Request(3, 9);
    Source(sql, request.source);
    for (uint16_t i = 0; i < 45; ++i)
    {
        request.storage.push_back({uint32_t(300 + i), 50010, i, 1});
        Text(sql, "INSERT INTO item(id,owner_id,window,pos,vnum,count) VALUES(" + std::to_string(300 + i) +
                      ",9,'SAFEBOX'," + std::to_string(i) + ",50010,1)");
    }
    Check(!award::Deliver(request), "full box produced writes");
    Pending(sql, request);
    Text(sql, "DELETE FROM item WHERE id=344");
    request.storage.pop_back();
    request.item.position = 44;
    Execute(sql, award::Deliver(request));
    Check(Number(sql, "SELECT COUNT(*) FROM item WHERE owner_id=9 AND window='SAFEBOX'") == 45,
          "full-box retry lost or duplicated items");
}
void EnginesAndPostfix(CAsyncSQL &sql)
{
    auto request = Request(4, 10);
    request.source.mall = true;
    Source(sql, request.source);
    Text(sql, "ALTER TABLE item_award ENGINE=MyISAM");
    Execute(sql, award::Deliver(request), false);
    Text(sql, "ALTER TABLE item_award ENGINE=InnoDB");
    Pending(sql, request);
    Text(sql, "SET foreign_key_checks=0");
    Execute(sql, award::Deliver(request), false);
    Text(sql, "SET foreign_key_checks=1");
    Text(sql, "CREATE TRIGGER test_award_trigger AFTER INSERT ON item FOR EACH ROW SET @unused=1");
    Execute(sql, award::Deliver(request), false);
    Text(sql, "DROP TRIGGER test_award_trigger");
    Text(sql, "CREATE TABLE item_test LIKE item");
    Text(sql, "CREATE TABLE safebox_test LIKE safebox");
    Execute(sql, award::Deliver(request, "_test"));
    Check(Receipt(sql, request.source, "_test").item == request.item.id &&
              Number(sql, "SELECT COUNT(*) FROM item_test WHERE owner_id=10 AND window='MALL'") == 1 &&
              Number(sql, "SELECT COUNT(*) FROM item WHERE owner_id=10") == 0,
          "postfix or mall delivery targeted the wrong table/window");
}
void Concurrent(CAsyncSQL &sql)
{
    auto request = Request(5, 11);
    Source(sql, request.source);
    CAsyncSQL first, second;
    Connect(first);
    Connect(second);
    std::latch ready(2), start(1);
    Message a, b;
    auto other = request;
    other.item.id += 1000;
    auto run = [&](CAsyncSQL &connection, award::Request value, Message &result)
    {
        ready.count_down();
        start.wait();
        result.reset(connection.DirectTransactionPrepared(award::Deliver(value).statements));
    };
    std::thread left(run, std::ref(first), request, std::ref(a));
    std::thread right(run, std::ref(second), other, std::ref(b));
    ready.wait();
    start.count_down();
    left.join();
    right.join();
    Check(award::Confirmed(*a) != award::Confirmed(*b), "concurrent claims did not have exactly one winner");
    Check(Number(sql, "SELECT COUNT(*) FROM item WHERE owner_id=11") == 1,
          "concurrent claim duplicated item");
    auto receipt = Receipt(sql, request.source);
    Check(receipt.state == award::ReceiptState::Taken &&
              receipt.item == (award::Confirmed(*a) ? request.item.id : other.item.id),
          "concurrent receipt disagrees with winner");
    // Different awards compete for the same stale empty-container snapshot.
    request = Request(6, 12);
    other = Request(7, 12);
    Source(sql, request.source);
    Source(sql, other.source);
    std::latch bothReady(2), bothStart(1);
    const auto competing = [&](CAsyncSQL &connection, award::Request value, Message &result)
    {
        bothReady.count_down();
        bothStart.wait();
        result.reset(connection.DirectTransactionPrepared(award::Deliver(value).statements));
    };
    std::thread one(competing, std::ref(first), request, std::ref(a));
    std::thread two(competing, std::ref(second), other, std::ref(b));
    bothReady.wait();
    bothStart.count_down();
    one.join();
    two.join();
    Check(award::Confirmed(*a) != award::Confirmed(*b), "stale snapshot put two items in one cell");
    if (award::Confirmed(*b))
        std::swap(request, other);
    Pending(sql, other);
    other.storage = {{request.item.id, request.item.vnum, 0, 1}};
    other.item.position = 1;
    Execute(sql, award::Deliver(other));
}
void Interrupted(CAsyncSQL &admin)
{
    auto request = Request(8, 13);
    Source(admin, request.source);
    CAsyncSQL worker;
    Connect(worker, 0, false);
    auto plan = award::Deliver(request);
    const size_t pause = plan.statements.size() - 1;
    plan.statements.insert(plan.statements.begin() + pause, {"SELECT SLEEP(15)", {}, {}, 1});
    worker.ReturnTransactionPrepared(plan.statements, nullptr);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    uint64_t id = 0;
    while (std::chrono::steady_clock::now() < deadline && !id)
    {
        id = Number(admin,
                    "SELECT COALESCE(MAX(ID),0) FROM information_schema.PROCESSLIST WHERE "
                    "DB=? AND ID<>CONNECTION_ID() AND INFO LIKE 'SELECT SLEEP(15)%'",
                    {SqlParam::Text(database)});
        if (!id)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Check(id != 0, "interrupted delivery never reached post-insert pause");
    Text(admin, "KILL CONNECTION " + std::to_string(id));
    Message result;
    while (std::chrono::steady_clock::now() < deadline && !result)
    {
        SQLMsg *raw = nullptr;
        if (worker.PopResult(&raw))
            result.reset(raw);
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Check(result && !award::Confirmed(*result), "killed delivery was reported committed");
    Pending(admin, request);
    Execute(admin, award::Deliver(request));
}
void ResolutionBarrier(CAsyncSQL &admin)
{
    auto request = Request(10, 15);
    Source(admin, request.source);
    CAsyncSQL writer, reader;
    Connect(writer, 0, false);
    Connect(reader, 0, false);
    auto plan = award::Deliver(request);
    plan.statements.insert(plan.statements.end() - 1, {"SELECT SLEEP(15)", {}, {}, 1});
    writer.ReturnTransactionPrepared(plan.statements, nullptr);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    uint64_t id = 0;
    while (std::chrono::steady_clock::now() < deadline && !id)
    {
        id = Number(admin,
                    "SELECT COALESCE(MAX(ID),0) FROM information_schema.PROCESSLIST WHERE "
                    "DB=? AND ID<>CONNECTION_ID() AND INFO LIKE 'SELECT SLEEP(15)%'",
                    {SqlParam::Text(database)});
        if (!id)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Check(id != 0, "resolution test writer never locked award");
    reader.ReturnTransactionPrepared(award::Resolve(request.source), nullptr);
    bool waiting = false;
    while (std::chrono::steady_clock::now() < deadline && !waiting)
    {
        waiting = Number(admin,
                         "SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE DB=? "
                         "AND INFO LIKE 'SELECT taken_time IS NOT NULL%'",
                         {SqlParam::Text(database)}) > 0;
        if (!waiting)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Check(waiting, "resolution did not reach its locking read");
    SQLMsg *raw = nullptr;
    Check(!reader.PopResult(&raw), "resolution observed pending before original writer settled");
    Text(admin, "KILL CONNECTION " + std::to_string(id));
    Message resolved, interrupted;
    while (std::chrono::steady_clock::now() < deadline && (!resolved || !interrupted))
    {
        if (!resolved && reader.PopResult(&raw))
            resolved.reset(raw);
        if (!interrupted && writer.PopResult(&raw))
            interrupted.reset(raw);
        if (!resolved || !interrupted)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Check(resolved && interrupted && !award::Confirmed(*interrupted) &&
              award::DecodeReceipt(*resolved).state == award::ReceiptState::Pending,
          "resolution did not wait for rollback");
    Pending(admin, request);
    Execute(admin, award::Deliver(request));
}
void LostCommit(CAsyncSQL &admin, unsigned int proxy)
{
    auto request = Request(9, 14);
    Source(admin, request.source);
    CAsyncSQL faulty;
    Connect(faulty, proxy);
    auto plan = award::Deliver(request);
    auto result = Execute(faulty, plan, false);
    Check(result->transactionOutcome == SqlTransactionOutcome::CommitUnknown,
          "lost real COMMIT response was not uncertain");
    const auto receipt = Receipt(admin, request.source);
    Check(receipt.state == award::ReceiptState::Taken && receipt.item == request.item.id,
          "lost COMMIT could not be resolved by stable award ID");
    Execute(admin, plan, false);
    request.item.id++;
    Execute(admin, award::Deliver(request), false);
    Check(Number(admin, "SELECT COUNT(*) FROM item WHERE owner_id=14") == 1,
          "lost COMMIT retry duplicated item");
}
void ScanRegression(CAsyncSQL &sql)
{
    CAsyncSQL delayed;
    Connect(delayed);
    Text(delayed, "START TRANSACTION");
    Text(delayed, "INSERT INTO item_award(login,vnum,count,why) VALUES('late',50011,2,'[GIFT]late')");
    const auto lower = Number(delayed, "SELECT LAST_INSERT_ID()");
    Text(sql, "INSERT INTO item_award(login,vnum,count) VALUES('early',50011,3)");
    const auto higher = Number(sql, "SELECT LAST_INSERT_ID()");
    Check(lower < higher, "fixture did not allocate IDs in opposite commit order");
    std::string batch = "INSERT INTO item_award(login,vnum,count) VALUES";
    for (size_t i = 0; i < 600; ++i)
        batch += (i ? "," : "") + std::string("('bulk',50011,1)");
    Text(sql, batch);
    award::Scan scan;
    const auto sweep = [&](bool growth, bool expectLower)
    {
        std::set<uint32_t> seen;
        size_t pages = 0;
        bool done = false;
        while (!done)
        {
            auto query = scan.Request(true);
            Check(query.has_value(), "scan did not release its outstanding request");
            Check(!scan.Request(true), "scan queued overlapping requests");
            Message response(sql.DirectQuery(query->c_str()));
            auto result = scan.Complete(*response);
            Check(result.success, "production scan rejected real raw MYSQL_RES");
            if (result.page)
            {
                ++pages;
                Check(response->Get()->uiNumRows <= award::Scan::PageSize, "scan exceeded page budget");
                while (auto row = mysql_fetch_row(response->Get()->pSQLResult))
                    Check(seen.insert(uint32_t(std::stoul(row[0]))).second, "sweep repeated an award ID");
                if (growth)
                    Text(sql, "INSERT INTO item_award(login,vnum,count) VALUES('growth',50011,1)");
            }
            done = result.sweepComplete;
            Check(pages <= 4, "new inserts prevented bounded scan from wrapping");
        }
        Check(seen.contains(uint32_t(higher)), "scan missed committed higher ID");
        Check(seen.contains(uint32_t(lower)) == expectLower, "late lower ID discovery mismatch");
        return seen;
    };
    sweep(true, false);
    Text(delayed, "COMMIT");
    const auto discovered = sweep(false, true);
    Check(discovered == sweep(false, true), "repeated sweep changed stable pending awards");
    Text(sql, "UPDATE item_award SET taken_time=NOW(),item_id=123 WHERE id=" + std::to_string(lower));
    const auto pending = sweep(false, false);
    Check(pending.size() + 1 == discovered.size(), "taken award remained in pending discovery");
    // A failed query retries the exact same cursor/phase instead of skipping it.
    auto query = scan.Request(false);
    SQLMsg failure;
    failure.uiSQLErrno = 2013;
    Check(!scan.Complete(failure).success, "failed scan was accepted");
    Check(query == scan.Request(false), "failed scan advanced its cursor");
    Message ceiling(sql.DirectQuery(query->c_str()));
    Check(scan.Complete(*ceiling).success, "ceiling retry failed");
    query = scan.Request(false);
    Message malformed(sql.DirectQuery("SELECT 1 AS id"));
    Check(!scan.Complete(*malformed).success && query == scan.Request(false),
          "malformed page skipped pending awards");
    Message retry(sql.DirectQuery(query->c_str()));
    Check(scan.Complete(*retry).success, "legacy non-extended page rejected");
}
void Cold(CAsyncSQL &sql)
{
    for (uint32_t id : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10})
    {
        auto request = Request(id);
        request.source.mall = id == 4;
        const auto receipt = Receipt(sql, request.source, id == 4 ? "_test" : "");
        Check(receipt.state == award::ReceiptState::Taken, "cold receipt lost after MariaDB crash/restart");
        if (id == 1)
            Check(Number(sql, "SELECT COUNT(*) FROM item WHERE id=?", {SqlParam::UInt(receipt.item)}) == 0,
                  "cold deleted item was recreated");
        else
        {
            const auto table = id == 4 ? "item_test" : "item";
            Check(Number(sql, (std::string("SELECT COUNT(*) FROM ") + table + " WHERE id=?").c_str(),
                         {SqlParam::UInt(receipt.item)}) == 1,
                  "cold committed receipt lost its original item");
        }
        request.item.id += 20000000;
        Execute(sql, award::Deliver(request, id == 4 ? "_test" : ""), false);
    }
    Check(Number(sql, "SELECT COUNT(*) FROM item WHERE owner_id=14") == 1,
          "cold lost-COMMIT item duplicated");
    Check(Number(sql, "SELECT marker FROM account_mission_pass WHERE account_id=1") == 123,
          "seasonal Mission Pass sentinel changed");
}
} // namespace
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("item-award-delivery-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    return logging::GetLogger();
}
int main(int argc, char **argv)
{
    std::cout << std::unitbuf;
    try
    {
        Unit();
        if (argc == 2 && std::string_view(argv[1]) == "--unit")
        {
            std::cout << "Item award delivery unit: " << checks << " checks passed\n";
            return 0;
        }
        Check(argc == 5 || argc == 6, "usage: ItemAwardDeliveryTests --unit | PORT FRESH_SCHEMA PROXY_PORT "
                                      "CANONICAL_SCHEMA [--cold-check]");
        port = static_cast<unsigned int>(std::stoul(argv[1]));
        database = argv[2];
        Check(database.starts_with("auriga_mission_sql_test_") && database.size() <= 63 &&
                  database.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == database.npos,
              "private test schema required");
        if (argc == 6)
        {
            Check(std::string_view(argv[5]) == "--cold-check", "unknown followup mode");
            CAsyncSQL sql;
            Connect(sql);
            Cold(sql);
            std::cout << "Item award delivery cold-process: " << checks << " checks passed\n";
            return 0;
        }
        CAsyncSQL bootstrap;
        Check(bootstrap.Setup("127.0.0.1", "root", "", "", "utf8mb4", true, port),
              "private bootstrap failed");
        Text(bootstrap, "CREATE DATABASE " + database + " CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
        CAsyncSQL sql;
        Connect(sql);
        Tables(sql, argv[4]);
        Basic(sql);
        RollbackAndStale(sql);
        FullBox(sql);
        EnginesAndPostfix(sql);
        Concurrent(sql);
        Interrupted(sql);
        ResolutionBarrier(sql);
        LostCommit(sql, static_cast<unsigned int>(std::stoul(argv[3])));
        ScanRegression(sql);
        Check(Number(sql, "SELECT marker FROM account_mission_pass WHERE account_id=1") == 123,
              "seasonal Mission Pass sentinel changed");
        std::cout << "Item award delivery MariaDB: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
