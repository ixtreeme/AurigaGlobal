#include "../../SRC/Server/SQL/AsyncSQL.h"
#include <Core/Logging.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

// The production CAsyncSQL::StorePreparedResult, driven through a scripted
// result set. The statement calls it makes are doubles that behave like the
// MySQL/MariaDB client library for MYSQL_TYPE_STRING result binds: at most
// buffer_length bytes are copied, the full length is reported, a longer value
// makes mysql_stmt_fetch return MYSQL_DATA_TRUNCATED, and
// mysql_stmt_fetch_column reads one column of the current row again.
// MYSQL_FIELD::max_length stays 0, as the library leaves it unless
// STMT_ATTR_UPDATE_MAX_LENGTH is set.
namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }

using Cell = std::optional<std::string>;
using Rows = std::vector<std::vector<Cell>>;

struct ScriptedResult
{
    Rows rows;
    std::vector<MYSQL_FIELD> fields;
    MYSQL_BIND* bound { nullptr };
    size_t next { 0 };
    size_t current { SIZE_MAX };
    int wholeColumnReads { 0 };
    bool failWholeColumn { false };
};

ScriptedResult script;
bool connectSucceeds = false;
std::atomic<int> closedConnections { 0 }, timeoutOptions { 0 }, prepareCalls { 0 };
bool queryConnectionFailure = false;
bool executeConnectionFailure = false;
std::atomic<int> executeCalls { 0 };

void Load(Rows rows)
{
    script = ScriptedResult {};
    script.rows = std::move(rows);
    script.fields.assign(script.rows.empty() ? 0 : script.rows.front().size(), MYSQL_FIELD {});
}

// Copies a cell into a string bind the way the client library does; true
// when the value did not fit.
bool CopyCell(MYSQL_BIND& bind, const Cell& cell, unsigned long offset)
{
    if (!cell) {
        *bind.is_null = 1;
        return false;
    }
    *bind.is_null = 0;
    *bind.length = static_cast<unsigned long>(cell->size());
    const size_t available = cell->size() > offset ? cell->size() - offset : 0;
    const size_t copied = std::min<size_t>(available, bind.buffer_length);
    if (copied)
        std::memcpy(bind.buffer, cell->data() + offset, copied);
    if (copied < bind.buffer_length)
        static_cast<char*>(bind.buffer)[copied] = '\0';
    return available > bind.buffer_length;
}

class PreparedHarness : public CAsyncSQL
{
public:
    bool Store(SQLMsg& msg)
    {
        return StorePreparedResult(&msg, reinterpret_cast<MYSQL_STMT*>(&script), reinterpret_cast<MYSQL_RES*>(&script));
    }
};

std::string Text(const char* cell) { return cell ? std::string(cell) : std::string("<null>"); }
}

// --- the client library double ---------------------------------------------------
// Every MySQL call AsyncSQL.cpp makes is defined here, so nothing is linked
// from the (static) client library; the ones this test does not reach stop it.
namespace {
[[noreturn]] void Unexpected(const char* name)
{
    std::cerr << "FAILED: unexpected call to " << name << "\n";
    std::abort();
}
}

void gettimeofday(struct timeval* t, struct timezone*) { t->tv_sec = 0; t->tv_usec = 0; }

extern "C" {
my_ulonglong STDCALL mysql_affected_rows(MYSQL*) { Unexpected("mysql_affected_rows"); }
void STDCALL mysql_close(MYSQL*) { ++closedConnections; }
unsigned int STDCALL mysql_errno(MYSQL*) { return CR_SERVER_GONE_ERROR; }
const char* STDCALL mysql_error(MYSQL*) { return "scripted connection failure"; }
MYSQL_ROW STDCALL mysql_fetch_row(MYSQL_RES*) { Unexpected("mysql_fetch_row"); }
void STDCALL mysql_free_result(MYSQL_RES*) { Unexpected("mysql_free_result"); }
MYSQL* STDCALL mysql_init(MYSQL* handle) { return handle; }
my_ulonglong STDCALL mysql_insert_id(MYSQL*) { Unexpected("mysql_insert_id"); }
int STDCALL mysql_next_result(MYSQL*) { Unexpected("mysql_next_result"); }
my_ulonglong STDCALL mysql_num_rows(MYSQL_RES*) { Unexpected("mysql_num_rows"); }
int STDCALL mysql_options(MYSQL*, enum mysql_option option, const void* value) {
    if ((option == MYSQL_OPT_CONNECT_TIMEOUT || option == MYSQL_OPT_READ_TIMEOUT || option == MYSQL_OPT_WRITE_TIMEOUT) &&
        *static_cast<const unsigned int*>(value) == 5) ++timeoutOptions;
    return 0;
}
MYSQL* STDCALL mysql_real_connect(MYSQL* handle, const char*, const char*, const char*, const char*, unsigned int, const char*, unsigned long) {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    return connectSucceeds ? handle : nullptr;
}
unsigned long STDCALL mysql_real_escape_string(MYSQL*, char*, const char*, unsigned long) { Unexpected("mysql_real_escape_string"); }
int STDCALL mysql_real_query(MYSQL*, const char*, unsigned long) { Unexpected("mysql_real_query"); }
int STDCALL mysql_set_character_set(MYSQL*, const char*) { Unexpected("mysql_set_character_set"); }
MYSQL_RES* STDCALL mysql_store_result(MYSQL*) { Unexpected("mysql_store_result"); }
unsigned long STDCALL mysql_thread_id(MYSQL*) { return 1; }
unsigned int STDCALL mysql_thread_safe(void) { Unexpected("mysql_thread_safe"); }
unsigned long long STDCALL mysql_stmt_affected_rows(MYSQL_STMT*) { return 1; }
my_bool STDCALL mysql_stmt_bind_param(MYSQL_STMT*, MYSQL_BIND*) { Unexpected("mysql_stmt_bind_param"); }
my_bool STDCALL mysql_stmt_close(MYSQL_STMT*) { return 0; }
int STDCALL mysql_stmt_execute(MYSQL_STMT*) { return executeConnectionFailure && ++executeCalls == 1 ? 1 : 0; }
MYSQL_STMT* STDCALL mysql_stmt_init(MYSQL*) { return reinterpret_cast<MYSQL_STMT*>(&script); }
unsigned long long STDCALL mysql_stmt_insert_id(MYSQL_STMT*) { return 0; }
unsigned long STDCALL mysql_stmt_param_count(MYSQL_STMT*) { return 0; }
int STDCALL mysql_stmt_prepare(MYSQL_STMT*, const char*, unsigned long) { const auto count = ++prepareCalls; return queryConnectionFailure && count == 1 ? 1 : 0; }
MYSQL_RES* STDCALL mysql_stmt_result_metadata(MYSQL_STMT*) { return nullptr; }

// The statement calls StorePreparedResult makes, scripted.
int STDCALL mysql_stmt_store_result(MYSQL_STMT*) { return 0; }
unsigned int STDCALL mysql_num_fields(MYSQL_RES*) { return static_cast<unsigned int>(script.fields.size()); }
MYSQL_FIELD* STDCALL mysql_fetch_fields(MYSQL_RES*) { return script.fields.data(); }
my_bool STDCALL mysql_stmt_bind_result(MYSQL_STMT*, MYSQL_BIND* binds) { script.bound = binds; return 0; }
int STDCALL mysql_stmt_fetch(MYSQL_STMT*)
{
    if (script.next >= script.rows.size())
        return MYSQL_NO_DATA;
    script.current = script.next++;
    bool truncated = false;
    for (size_t i = 0; i < script.fields.size(); ++i)
        truncated = CopyCell(script.bound[i], script.rows[script.current][i], 0) || truncated;
    return truncated ? MYSQL_DATA_TRUNCATED : 0;
}
int STDCALL mysql_stmt_fetch_column(MYSQL_STMT*, MYSQL_BIND* bind, unsigned int column, unsigned long offset)
{
    ++script.wholeColumnReads;
    if (script.failWholeColumn || script.current >= script.rows.size() || column >= script.fields.size())
        return 1;
    CopyCell(*bind, script.rows[script.current][column], offset);
    return 0;
}
my_bool STDCALL mysql_stmt_free_result(MYSQL_STMT*) { return 0; }
unsigned int STDCALL mysql_stmt_errno(MYSQL_STMT*) { return executeConnectionFailure ? CR_SERVER_LOST : 2051; }
const char* STDCALL mysql_stmt_error(MYSQL_STMT*) { return "scripted failure"; }
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("sql-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("sql-test");
    return logger;
}

namespace {
SQLResult* StoreRows(SQLMsg& msg, Rows rows)
{
    Load(std::move(rows));
    PreparedHarness sql;
    Check(sql.Store(msg), "prepared result stored");
    Check(msg.uiSQLErrno == 0, "a stored result clears the error");
    return msg.Get();
}

// The login query's shape: two password hashes, the status and numbers that
// the binary protocol hands back converted to text.
void ShortCellsComeBackWhole()
{
    const std::string hash = "*94BDCEBE19083CE2A1F959FD02F964C7AF4CFC29";
    SQLMsg msg;
    SQLResult* res = StoreRows(msg, { { hash, std::string("*0000000000000000000000000000000000000000"),
        std::string("OK"), std::string("12345"), std::string("-7"), std::string() } });
    Check(res && res->bPrepared && res->uiNumRows == 1, "one prepared row stored");
    MYSQL_ROW row = res->FetchRow();
    Check(row && Text(row[0]) == hash, "a password hash comes back whole");
    Check(Text(row[0]) != Text(row[1]), "different hashes stay different");
    Check(Text(row[2]) == "OK", "the status is not cut to its first character");
    Check(Text(row[3]) == "12345" && Text(row[4]) == "-7", "numbers keep every digit");
    Check(row[5] && Text(row[5]).empty(), "an empty string is not NULL");
    Check(!res->FetchRow(), "a single row");
    Check(script.wholeColumnReads == 0, "short cells need no second read");
}

void LongCellsAreReadAgainWhole()
{
    const std::string exact(256, 'e');
    const std::string oneOver(257, 'o');
    const std::string blob(70000, 'b');
    SQLMsg msg;
    SQLResult* res = StoreRows(msg, { { exact, oneOver, blob }, { std::string("x"), oneOver, std::string("y") } });
    Check(res && res->uiNumRows == 2, "two rows stored");
    MYSQL_ROW row = res->FetchRow();
    Check(row && Text(row[0]) == exact, "a value filling the first buffer is whole");
    Check(Text(row[1]) == oneOver, "a value one byte longer is read again whole");
    Check(Text(row[2]) == blob, "a large value is read again whole");
    row = res->FetchRow();
    Check(row && Text(row[0]) == "x" && Text(row[1]) == oneOver && Text(row[2]) == "y", "the next row is independent");
    Check(script.wholeColumnReads == 3, "only the truncated cells are read again");
}

void NullCellsStayNull()
{
    SQLMsg msg;
    SQLResult* res = StoreRows(msg, { { std::nullopt, std::string("a"), std::nullopt } });
    MYSQL_ROW row = res->FetchRow();
    Check(row && !row[0] && Text(row[1]) == "a" && !row[2], "NULL columns are null pointers, like mysql_fetch_row");
}

void RowsStayValidAcrossManyRows()
{
    Rows rows;
    for (int i = 0; i < 2000; ++i)
        rows.push_back({ std::to_string(i), std::string(250 + i % 13, static_cast<char>('a' + i % 26)) });
    const Rows expected = rows;
    SQLMsg msg;
    SQLResult* res = StoreRows(msg, std::move(rows));
    Check(res && res->uiNumRows == expected.size(), "every row counted");
    std::vector<MYSQL_ROW> fetched;
    for (MYSQL_ROW row = res->FetchRow(); row; row = res->FetchRow())
        fetched.push_back(row);
    Check(fetched.size() == expected.size(), "every row fetched");
    for (size_t i = 0; i < expected.size(); ++i)
        Check(Text(fetched[i][0]) == *expected[i][0] && Text(fetched[i][1]) == *expected[i][1],
            "earlier row pointers stay valid while later rows are stored");
}

void FailedSecondReadIsAnError()
{
    Load({ { std::string(400, 'z') } });
    script.failWholeColumn = true;
    SQLMsg msg;
    PreparedHarness sql;
    Check(!sql.Store(msg) && !msg.Get() && msg.uiSQLErrno == 2051,
        "a failed whole-column read fails the result instead of storing a cut value");
}

void EmptyResult()
{
    SQLMsg msg;
    Load({});
    PreparedHarness sql;
    Check(sql.Store(msg) && msg.Get() && msg.Get()->uiNumRows == 0 && !msg.Get()->FetchRow(), "no rows, no cells");
}

void ConnectionStartupAndQueryFailure()
{
    for (bool success : {false, true}) {
        connectSucceeds = success; timeoutOptions = 0;
        const auto closed = closedConnections.load();
        {
            CAsyncSQL sql;
            Check(sql.GetConnectionState() == CAsyncSQL::ConnectionState::NotStarted, "new SQL connection already ready");
            sql.SetTimeouts(5);
            Check(sql.Setup("test", "user", "secret", "account", "ascii"), "worker did not start");
            Check(!sql.WaitForConnection(std::chrono::milliseconds(1)), "thread startup reported SQL ready");
            Check(sql.WaitForConnection(std::chrono::seconds(1)) == success, "initial connection outcome lost");
            Check(sql.GetConnectionState() == (success ? CAsyncSQL::ConnectionState::Ready : CAsyncSQL::ConnectionState::Failed),
                "wrong SQL startup state");
            Check(timeoutOptions == 3, "connect/read/write timeouts missing");
        }
        Check(closedConnections == closed + 1, "initialized connector leaked on startup/shutdown");
    }

    for (bool duringExecution : {false, true}) {
    CAsyncSQL sql;
    sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("test", "user", "secret", "account", "ascii") && sql.WaitForConnection(std::chrono::seconds(1)), "query test not connected");
    queryConnectionFailure = !duringExecution; executeConnectionFailure = duringExecution;
    prepareCalls = 0; executeCalls = 0;
    sql.ReturnQueryPrepared("same-query", {}, nullptr);
    sql.ReturnQueryPrepared("same-query", {}, nullptr);
    std::vector<std::unique_ptr<SQLMsg>> results;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (results.size() < 2 && std::chrono::steady_clock::now() < deadline) {
        SQLMsg* msg = nullptr;
        if (sql.PopResult(&msg)) results.emplace_back(msg);
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    sql.Quit();
    queryConnectionFailure = false; executeConnectionFailure = false;
    Check(results.size() == 2, "connection failure left the worker queue stuck");
    Check(results[0]->uiSQLErrno == (duringExecution ? CR_SERVER_LOST : CR_SERVER_GONE_ERROR) && results[1]->uiSQLErrno == 0 && results[1]->Get()->uiAffectedRows == 1,
        "failed query did not complete before the following successful query");
    Check(prepareCalls == 2, "lost connection retained a stale prepared handle");
    }
}
}

int main()
{
    try {
        ShortCellsComeBackWhole();
        LongCellsAreReadAgainWhole();
        NullCellsStayNull();
        RowsStayValidAcrossManyRows();
        FailedSecondReadIsAnError();
        EmptyResult();
        ConnectionStartupAndQueryFailure();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        return 1;
    }
    std::cout << "async sql prepared results: " << checks << " checks passed\n";
    return 0;
}
