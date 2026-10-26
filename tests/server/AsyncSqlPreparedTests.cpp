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
struct TransactionScript {
    std::vector<std::string> calls;
    bool commitFails = false, rollbackFails = false, cleanupFails = false, pingFails = false;
    int failExecute = 0, executed = 0, changeConnection = 0;
    unsigned int executeError = 1062;
    uint64_t affected = 1;
    unsigned long connection = 1;
};
std::optional<TransactionScript> tx;
int resultSets = 1, resultSet = 0, nextResultFailure = 0;


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
    int CopyPendingForWorker() { return CopyQuery(); }
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
void STDCALL mysql_close(MYSQL*) { ++closedConnections; if (tx) tx->calls.push_back("close"); }
unsigned int STDCALL mysql_errno(MYSQL*) { return CR_SERVER_GONE_ERROR; }
unsigned int STDCALL mysql_field_count(MYSQL*) { Unexpected("mysql_field_count"); }
unsigned long* STDCALL mysql_fetch_lengths(MYSQL_RES*) { Unexpected("mysql_fetch_lengths"); }
const char* STDCALL mysql_error(MYSQL*) { return "scripted connection failure"; }
MYSQL_ROW STDCALL mysql_fetch_row(MYSQL_RES*) { Unexpected("mysql_fetch_row"); }
void STDCALL mysql_free_result(MYSQL_RES*) {}
MYSQL* STDCALL mysql_init(MYSQL* handle) { return handle; }
my_ulonglong STDCALL mysql_insert_id(MYSQL*) { Unexpected("mysql_insert_id"); }
int STDCALL mysql_next_result(MYSQL*) { Unexpected("mysql_next_result"); }
my_ulonglong STDCALL mysql_num_rows(MYSQL_RES*) { Unexpected("mysql_num_rows"); }
int STDCALL mysql_options(MYSQL*, enum mysql_option option, const void* value) {
    if ((option == MYSQL_OPT_CONNECT_TIMEOUT || option == MYSQL_OPT_READ_TIMEOUT || option == MYSQL_OPT_WRITE_TIMEOUT) &&
        *static_cast<const unsigned int*>(value) == 5) ++timeoutOptions;
    if (tx && option == MYSQL_OPT_RECONNECT)
        tx->calls.push_back(*static_cast<const my_bool*>(value) ? "reconnect-on" : "reconnect-off");
    return 0;
}
int STDCALL mysql_ping(MYSQL*) { if (!tx) return 0; tx->calls.push_back("ping"); return tx->pingFails; }
my_bool STDCALL mysql_autocommit(MYSQL*, my_bool enabled) {
    if (!tx) return 0;
    tx->calls.push_back(enabled ? "autocommit-on" : "begin");
    return enabled && tx->cleanupFails;
}
my_bool STDCALL mysql_commit(MYSQL*) { tx->calls.push_back("commit"); return tx->commitFails; }
my_bool STDCALL mysql_rollback(MYSQL*) { tx->calls.push_back("rollback"); return tx->rollbackFails; }
MYSQL* STDCALL mysql_real_connect(MYSQL* handle, const char*, const char*, const char*, const char*, unsigned int, const char*, unsigned long) {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    handle->server_status = SERVER_STATUS_AUTOCOMMIT;
    return connectSucceeds ? handle : nullptr;
}
unsigned long STDCALL mysql_real_escape_string(MYSQL*, char*, const char*, unsigned long) { Unexpected("mysql_real_escape_string"); }
int STDCALL mysql_real_query(MYSQL*, const char*, unsigned long) { Unexpected("mysql_real_query"); }
int STDCALL mysql_set_character_set(MYSQL*, const char*) { Unexpected("mysql_set_character_set"); }
MYSQL_RES* STDCALL mysql_store_result(MYSQL*) { Unexpected("mysql_store_result"); }
unsigned long STDCALL mysql_thread_id(MYSQL*) { return tx ? tx->connection : 1; }
unsigned int STDCALL mysql_thread_safe(void) { Unexpected("mysql_thread_safe"); }
unsigned long long STDCALL mysql_stmt_affected_rows(MYSQL_STMT*) { return tx ? tx->affected : 1; }
my_bool STDCALL mysql_stmt_bind_param(MYSQL_STMT*, MYSQL_BIND*) { return 0; }
my_bool STDCALL mysql_stmt_close(MYSQL_STMT* stmt) { delete reinterpret_cast<std::string*>(stmt); return 0; }
int STDCALL mysql_stmt_execute(MYSQL_STMT* stmt) {
    resultSet = 0;
    if (tx) {
        tx->calls.push_back(*reinterpret_cast<std::string*>(stmt));
        ++tx->executed;
        if (tx->changeConnection == tx->executed) ++tx->connection;
        return tx->failExecute == tx->executed;
    }
    return executeConnectionFailure && ++executeCalls == 1 ? 1 : 0;
}
MYSQL_STMT* STDCALL mysql_stmt_init(MYSQL*) { return reinterpret_cast<MYSQL_STMT*>(new std::string); }
unsigned long long STDCALL mysql_stmt_insert_id(MYSQL_STMT*) { return 0; }
unsigned long STDCALL mysql_stmt_param_count(MYSQL_STMT* stmt) { const auto& query = *reinterpret_cast<std::string*>(stmt); return static_cast<unsigned long>(std::count(query.begin(), query.end(), '?')); }
int STDCALL mysql_stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long size) { reinterpret_cast<std::string*>(stmt)->assign(query,size); const auto count = ++prepareCalls; return queryConnectionFailure && count == 1 ? 1 : 0; }
MYSQL_RES* STDCALL mysql_stmt_result_metadata(MYSQL_STMT*) {
    return resultSets > 1 && resultSet < resultSets-1 ? reinterpret_cast<MYSQL_RES*>(&script) : nullptr;
}
unsigned int STDCALL mysql_stmt_field_count(MYSQL_STMT*) { return resultSets > 1 && resultSet < resultSets-1 ? static_cast<unsigned int>(script.fields.size()) : 0; }
int STDCALL mysql_stmt_next_result(MYSQL_STMT*) {
    ++resultSet;
    if (nextResultFailure && resultSet == nextResultFailure) return 1;
    script.next = 0;
    return resultSet < resultSets ? 0 : -1;
}

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
unsigned int STDCALL mysql_stmt_errno(MYSQL_STMT*) { return tx ? tx->executeError : executeConnectionFailure ? CR_SERVER_LOST : queryConnectionFailure ? CR_SERVER_GONE_ERROR : 2051; }
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


void BinaryLengthsAndMultipleResults()
{
    const std::string binary("a\0b\0c", 5);
    SQLMsg rows;
    auto* result = StoreRows(rows, {{binary, std::nullopt, std::string()}, {std::string("last"), binary, std::nullopt}});
    Check(!result->FetchLengths(), "lengths available before fetch");
    auto row = result->FetchRow();
    auto lengths = result->FetchLengths();
    Check(row && lengths && lengths[0] == 5 && lengths[1] == 0 && lengths[2] == 0 &&
        std::string(row[0], lengths[0]) == binary, "embedded NUL bytes lost");
    row = result->FetchRow(); lengths = result->FetchLengths();
    Check(lengths && lengths[0] == 4 && lengths[1] == 5 && lengths[2] == 0, "NULL reused previous cell length");
    Check(!result->FetchRow() && !result->FetchLengths(), "lengths survive EOF");

    connectSucceeds = true;
    CAsyncSQL sql;
    Check(sql.Setup("test", "user", "secret", "mission", "ascii", true), "multi-result connection failed");
    for (int iteration = 0; iteration < 20; ++iteration) {
        Load({{binary}}); resultSets = 3;
        std::unique_ptr<SQLMsg> message(sql.DirectQueryPrepared("CALL mission_test()", {}));
        Check(!message->uiSQLErrno && message->vec_pkResult.size() == 3 && resultSet == 3,
            "prepared CALL left unread result sets");
        for (size_t set = 0; set < 2; ++set) {
            auto* res = message->vec_pkResult[set]; row = res->FetchRow(); lengths = res->FetchLengths();
            Check(res->uiNumRows == 1 && row && lengths && std::string(row[0], lengths[0]) == binary,
                "multi-result row changed");
        }
        Check(message->vec_pkResult[2]->uiAffectedRows == 1, "CALL final OK packet lost");
    }
    resultSets = 1;
    std::unique_ptr<SQLMsg> mismatch(sql.DirectQueryPrepared("SELECT ?", {}));
    Check(mismatch->uiSQLErrno == CR_PARAMS_NOT_BOUND, "parameter mismatch looks successful");
    Load({{std::string("partial")}}); resultSets = 3; nextResultFailure = 1;
    std::unique_ptr<SQLMsg> failed(sql.DirectQueryPrepared("CALL fails_later()", {}));
    Check(failed->uiSQLErrno == 2051 && sql.GetConnectionState() == CAsyncSQL::ConnectionState::Failed,
        "later result error reused a dirty connection");
    resultSets = 1; nextResultFailure = 0;
    std::unique_ptr<SQLMsg> next(sql.DirectQueryPrepared("UPDATE clean SET value=1", {}));
    Check(!next->uiSQLErrno && sql.GetConnectionState() == CAsyncSQL::ConnectionState::Ready,
        "discarded connection could not recover at next job boundary");
}

SqlTransaction SampleTransaction()
{
    return {{"UPDATE mission_state SET revision=revision+1 WHERE revision=?", {SqlParam::UInt(7)}, 1},
            {"INSERT INTO mission_receipt(id) VALUES(?)", {SqlParam::Text("durable-id")}, 1}};
}

size_t Calls(std::string_view call)
{
    return std::count(tx->calls.begin(), tx->calls.end(), call);
}

void TransactionFaults()
{
    connectSucceeds = true;
    using Outcome = SqlTransactionOutcome;
    for (int fault = 0; fault < 11; ++fault) {
        tx.emplace();
        {
            CAsyncSQL sql;
            Check(sql.Setup("test", "user", "secret", "mission", "ascii", true), "transaction connection failed");
            tx->calls.clear();
            if (fault == 1) tx->affected = 0;
            if (fault == 2) tx->failExecute = 2;
            if (fault == 3) tx->commitFails = true;
            if (fault == 4) { tx->failExecute = 2; tx->rollbackFails = true; }
            if (fault == 5) tx->cleanupFails = true;
            if (fault == 6) tx->pingFails = true;
            if (fault == 7) tx->changeConnection = 1;
            if (fault == 8) { tx->failExecute = 1; tx->executeError = CR_SERVER_LOST; }
            if (fault == 9) sql.GetSQLHandle()->server_status |= SERVER_STATUS_IN_TRANS;
            if (fault == 10) sql.GetSQLHandle()->server_status &= ~SERVER_STATUS_AUTOCOMMIT;
            std::unique_ptr<SQLMsg> result(sql.DirectTransactionPrepared(SampleTransaction()));
            const auto expected = fault == 0 || fault == 5 ? Outcome::Committed : fault == 1 ? Outcome::Conflict :
                fault == 2 || fault == 8 ? Outcome::RolledBack : fault == 3 ? Outcome::CommitUnknown : Outcome::Failed;
            if (result->transactionOutcome != expected) {
                std::cerr << "fault=" << fault << " expected=" << static_cast<int>(expected)
                          << " actual=" << static_cast<int>(result->transactionOutcome) << " errno=" << result->uiSQLErrno << '\n';
                for (const auto& call : tx->calls) std::cerr << call << '\n';
            }
            Check(result->transactionOutcome == expected, "wrong transaction outcome");
            Check((result->uiSQLErrno == 0) == (expected == Outcome::Committed), "outcome/error disagree");
            if (fault == 0) {
                Check(result->transactionResultOffsets == std::vector<size_t>{0,1} && result->vec_pkResult.size() == 2,
                    "transaction step results lost");
                Check(Calls("begin") == 1 && Calls("commit") == 1 && Calls("rollback") == 0 &&
                    Calls("autocommit-on") == 1 && Calls("reconnect-off") == 1, "transaction did not own session");
            }
            if (fault == 1) {
                Check(tx->executed == 1 && Calls("commit") == 0 && Calls("rollback") == 1 &&
                    result->transactionFailedStep == 0 && result->uiSQLErrno == SqlTransactionConflict,
                    "CAS conflict did not stop before receipt insert");
            }
            if (fault == 2 || fault == 4) Check(tx->executed == 2 && Calls("commit") == 0,
                "failed step committed");
            if (fault == 3) Check(Calls("commit") == 1 && Calls("rollback") == 0 && Calls("autocommit-on") == 0 &&
                Calls("close") == 1, "unknown commit retried/rolled back/reused");
            if (fault == 4) Check(Calls("autocommit-on") == 0 && Calls("close") == 1,
                "failed rollback implicitly committed during cleanup");
            if (fault == 5) Check(result->transactionOutcome == Outcome::Committed && Calls("close") == 1,
                "cleanup failure lost confirmed commit");
            if (fault == 6) Check(tx->executed == 0 && Calls("begin") == 0 && Calls("close") == 1,
                "failed ping started transaction");
            if (fault == 7) Check(tx->executed == 1 && Calls("commit") == 0 && Calls("rollback") == 0 && Calls("close") == 1,
                "transaction continued after session changed");
            if (fault == 8) Check(tx->executed == 1 && Calls("commit") == 0, "connection error replayed a step");
            if (fault >= 9) Check(tx->executed == 0 && Calls("begin") == 0 && Calls("commit") == 0 && Calls("close") == 1,
                "transaction adopted dirty work from previous queue job");
        }
        tx.reset();
    }
}

void SelectTransactionPreconditions()
{
    connectSucceeds = true;
    for (int scenario = 0; scenario < 3; ++scenario) {
        tx.emplace();
        {
            CAsyncSQL sql;
            Check(sql.Setup("test", "user", "secret", "mission", "ascii", true), "select guard connection failed");
            tx->calls.clear();
            Load(scenario == 0 ? Rows{{std::string("1")}} : Rows{});
            resultSets = 2;
            SqlTransaction statements{{"SELECT guard FROM mission_guard", {}, {}, scenario == 2 ? 0u : 1u}};
            if (scenario == 1) statements.push_back({"UPDATE must_not_execute SET n=1", {}, {}, {}});
            std::unique_ptr<SQLMsg> result(sql.DirectTransactionPrepared(std::move(statements)));
            Check(result->transactionOutcome == (scenario == 1 ? SqlTransactionOutcome::Conflict : SqlTransactionOutcome::Committed),
                "SELECT expected row count ignored");
            Check(tx->executed == 1 && Calls("commit") == (scenario == 1 ? 0u : 1u),
                "failed SELECT guard executed later writes");
            resultSets = 1;
            std::unique_ptr<SQLMsg> invalid(sql.DirectTransactionPrepared({{"UPDATE t SET n=1", {}, {}, 1}}));
            Check(invalid->uiSQLErrno == SqlTransactionInvalid && tx->executed == 1,
                "DML accepted a SELECT row-count expectation");
        }
        tx.reset();
    }
}

void RejectUnsafeTransactions()
{
    tx.emplace();
    {
        CAsyncSQL sql;
        Check(sql.Setup("test", "user", "secret", "mission", "ascii", true), "validation connection failed");
        tx->calls.clear();
        std::vector<SqlTransaction> invalid = {{}, {{"CREATE TABLE implicit_commit(id INT)", {}, {}}},
            {{"CALL owns_transaction()", {}, {}}}, {{"COMMIT", {}, {}}}, {{"UPDATE t SET n=1; DELETE FROM t", {}, {}}},
            {{"/*hidden*/ UPDATE t SET n=1", {}, {}}}, {{"SELECT 1", {}, 1}}, {{std::string("SELECT 1\0hidden",15), {}, {}}},
            SqlTransaction(SqlTransactionLimits::Steps + 1, {"SELECT 1", {}, {}}),
            {{"SELECT ?", SqlParams(SqlTransactionLimits::Parameters + 1), {}}},
            {{"SELECT ?", {SqlParam::Blob(std::string(SqlTransactionLimits::TotalBytes, 'x'))}, {}}}};
        for (auto& transaction : invalid) {
            std::unique_ptr<SQLMsg> result(sql.DirectTransactionPrepared(std::move(transaction)));
            Check(result->uiSQLErrno == SqlTransactionInvalid && result->transactionOutcome == SqlTransactionOutcome::Failed,
                "invalid transaction accepted");
        }
        Check(tx->calls.empty(), "invalid transaction touched SQL session");
        bool rejected = false;
        try { sql.ReturnTransactionPrepared(SampleTransaction(), nullptr); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected, "queued transaction accepted on direct connection");
    }
    tx.reset();
    CAsyncSQL notStarted;
    std::unique_ptr<SQLMsg> result(notStarted.DirectTransactionPrepared(SampleTransaction()));
    Check(result->uiSQLErrno == SqlTransactionInvalid, "transaction ran before connection setup");
}

void QueuedTransactions()
{
    for (bool lostCommit : {false, true}) {
        tx.emplace(); tx->commitFails = lostCommit;
        {
            CAsyncSQL sql;
            Check(sql.Setup("test", "user", "secret", "mission", "ascii") &&
                sql.WaitForConnection(std::chrono::seconds(1)), "queue connection failed");
            std::unique_ptr<SQLMsg> illegal(sql.DirectTransactionPrepared(SampleTransaction()));
            Check(illegal->uiSQLErrno == SqlTransactionInvalid, "direct transaction accessed worker connection");
            tx->calls.clear();
            sql.ReturnTransactionPrepared(SampleTransaction(), reinterpret_cast<void*>(1));
            sql.ReturnQueryPrepared("UPDATE between_jobs SET n=1", {}, reinterpret_cast<void*>(2));
            sql.ReturnTransactionPrepared(SampleTransaction(), reinterpret_cast<void*>(3));
            std::vector<std::unique_ptr<SQLMsg>> results;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (results.size() < 3 && std::chrono::steady_clock::now() < deadline) {
                SQLMsg* message = nullptr;
                if (sql.PopResult(&message)) results.emplace_back(message);
                else std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            sql.Quit();
            Check(results.size() == 3, "transaction failure stranded following queue work");
            for (size_t index = 0; index < results.size(); ++index)
                Check(results[index]->pvUserData == reinterpret_cast<void*>(index+1), "queue callbacks reordered");
            Check(results[0]->transactionOutcome == (lostCommit ? SqlTransactionOutcome::CommitUnknown : SqlTransactionOutcome::Committed) &&
                results[2]->transactionOutcome == results[0]->transactionOutcome && !results[1]->uiSQLErrno,
                "queue outcome lost");
            Check(tx->executed == 5 && Calls("commit") == 2, "worker automatically replayed transaction");
            std::vector<std::string> work;
            for (const auto& call : tx->calls)
                if (call.starts_with("UPDATE") || call.starts_with("INSERT") || call == "commit") work.push_back(call);
            const auto sample = SampleTransaction();
            Check(work == std::vector<std::string>{sample[0].query,sample[1].query,"commit","UPDATE between_jobs SET n=1",
                sample[0].query,sample[1].query,"commit"}, "queue query interleaved transaction steps");
        }
        tx.reset();
    }
    tx.emplace();
    {
        CAsyncSQL sql;
        Check(sql.Setup("test", "user", "secret", "mission", "ascii") && sql.WaitForConnection(std::chrono::seconds(1)),
            "shutdown flush connection failed");
        sql.ReturnTransactionPrepared(SampleTransaction(), nullptr);
        sql.ReturnTransactionPrepared(SampleTransaction(), nullptr);
        sql.Quit();
        Check(tx->executed == 4 && Calls("commit") == 2, "shutdown flush split/dropped transaction");
    }
    tx.reset();
}

SqlQueueDrainResult Drain(CAsyncSQL &sql)
{
    auto ticket = sql.DrainQueue();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (ticket.Poll().status == SqlQueueDrainStatus::Pending && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Check(ticket.Poll().status != SqlQueueDrainStatus::Pending, "queue drain stranded");
    return ticket.Poll();
}
void QueueDrainBoundaries()
{
    {
        // With no live worker, model the exact queue state left by a retry:
        // producer queue empty, copied FIFO still contains unexecuted work.
        connectSucceeds = false;
        PreparedHarness sql;
        Check(sql.Setup("test", "user", "secret", "mission", "ascii") &&
              !sql.WaitForConnection(std::chrono::seconds(1)), "failed-worker harness setup failed");
        sql.Quit(); // Join before the harness manually drives the copied FIFO.
        sql.AsyncQueryPrepared("UPDATE pending_after_retry SET n=1", {});
        Check(sql.CopyPendingForWorker() == 1 && sql.CopyPendingForWorker() == 1,
              "retry stranded the copied FIFO when no new producer work arrived");
    }
    connectSucceeds = true;
    CAsyncSQL absent;
    Check(absent.DrainQueue().Poll().status == SqlQueueDrainStatus::Cancelled,
          "unstarted worker returned a valid boundary");
    Check(!SqlQueuePosition{}.ConfirmsSince({}), "invalid lanes confirmed each other");
    const auto origin = absent.QueueOrigin();
    CAsyncSQL other;
    Check(origin.lane && other.QueueOrigin().lane != origin.lane, "SQL worker lane identity reused");
    {
        CAsyncSQL direct;
        Check(direct.Setup("test", "user", "secret", "mission", "ascii", true), "direct setup failed");
        Check(direct.DrainQueue().Poll().status == SqlQueueDrainStatus::Cancelled,
              "direct connection pretended to be a FIFO worker");
    }
    {
        CAsyncSQL sql;
        Check(sql.Setup("test", "user", "secret", "mission", "ascii") &&
              sql.WaitForConnection(std::chrono::seconds(1)), "drain worker startup failed");
        const auto initial = sql.QueueOrigin();
        auto first = Drain(sql);
        Check(first.status == SqlQueueDrainStatus::Complete && first.position.ConfirmsSince(initial),
              "empty healthy lane failed to drain");
        // The first execution loses its acknowledgement; the legacy retry
        // eventually succeeds. That MUST NOT erase the ambiguous attempt.
        executeConnectionFailure = true; executeCalls = 0; prepareCalls = 0;
        sql.AsyncQueryPrepared("UPDATE legacy_cache SET n=1", {});
        auto retry = Drain(sql);
        executeConnectionFailure = false;
        Check(retry.status == SqlQueueDrainStatus::Complete && retry.position.unconfirmed == 1 &&
              retry.position.lastSqlError == CR_SERVER_LOST && !retry.position.ConfirmsSince(initial),
              "successful legacy retry erased an uncertain write");
        Check(executeCalls == 2 && retry.position.lastUnconfirmedSequence < retry.position.sequence,
              "retry drain raced preceding SQL work");
        auto later = Drain(sql);
        Check(!later.position.ConfirmsSince(initial) && later.position.ConfirmsSince(retry.position),
              "new boundary cleared failure history or could not follow a verified baseline");
        Check(!later.position.ConfirmsSince(other.QueueOrigin()) && !later.position.ConfirmsSince(later.position),
              "foreign/future boundary accepted");
        SQLMsg *unexpected = nullptr;
        Check(!sql.PopResult(&unexpected), "drain metadata entered the legacy result queue");
        sql.Quit();
        Check(first.position.ConfirmsSince(initial), "retained completion changed after worker shutdown");
    }
    {
        CAsyncSQL sql;
        sql.SetFlushOnQuit(false);
        Check(sql.Setup("test", "user", "secret", "mission", "ascii"), "discard startup failed");
        auto pending = sql.DrainQueue();
        sql.Quit();
        Check(pending.Poll().status != SqlQueueDrainStatus::Pending,
              "shutdown without flush stranded its drain ticket");
    }
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
            auto startupDrain = sql.DrainQueue();
            Check(!sql.WaitForConnection(std::chrono::milliseconds(1)), "thread startup reported SQL ready");
            Check(sql.WaitForConnection(std::chrono::seconds(1)) == success, "initial connection outcome lost");
            Check(sql.GetConnectionState() == (success ? CAsyncSQL::ConnectionState::Ready : CAsyncSQL::ConnectionState::Failed),
                "wrong SQL startup state");
            Check(timeoutOptions == 3, "connect/read/write timeouts missing");
            sql.Quit();
            Check(startupDrain.Poll().status == (success ? SqlQueueDrainStatus::Complete : SqlQueueDrainStatus::Cancelled),
                  "startup drain did not settle/cancel");
            Check(sql.DrainQueue().Poll().status == SqlQueueDrainStatus::Cancelled,
                  "stopped worker accepted a new drain");
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
        BinaryLengthsAndMultipleResults();
        TransactionFaults();
        RejectUnsafeTransactions();
        QueuedTransactions();
        QueueDrainBoundaries();
        SelectTransactionPreconditions();
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        return 1;
    }
    std::cout << "async sql prepared results: " << checks << " checks passed\n";
    return 0;
}
