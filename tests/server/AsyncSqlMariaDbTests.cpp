#include "../../SRC/Server/SQL/AsyncSQL.h"
#include <Core/Logging.hpp>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace
{
int checks = 0;
using Message = std::unique_ptr<SQLMsg>;
using Outcome = SqlTransactionOutcome;
unsigned int port = 0;
std::string database;

void Check(bool condition, const char *reason)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(reason);
}

void Connect(CAsyncSQL &sql, bool direct = true, unsigned int targetPort = 0)
{
    sql.SetTimeouts(3);
    sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1", "root", "", database.c_str(), "utf8mb4", direct,
                    targetPort ? targetPort : port),
          "isolated MariaDB connection failed");
    if (!direct)
        Check(sql.WaitForConnection(std::chrono::seconds(5)), "isolated MariaDB worker not ready");
}

Message Query(CAsyncSQL &sql, const char *text, const SqlParams &parameters = {})
{
    Message message(sql.DirectQueryPrepared(text, parameters));
    if (message->uiSQLErrno)
        std::cerr << "SQL error " << message->uiSQLErrno << ": " << text << '\n';
    Check(!message->uiSQLErrno && message->Get(), "isolated prepared statement failed");
    return message;
}

uint64_t Number(CAsyncSQL &sql, const char *query, const SqlParams &parameters = {})
{
    auto result = Query(sql, query, parameters);
    const auto row = result->Get()->FetchRow();
    Check(row && row[0], "expected a scalar result");
    return std::stoull(row[0]);
}

void TextQuery(CAsyncSQL &sql, const std::string &text)
{
    Message result(sql.DirectQuery(text.c_str()));
    Check(!result->uiSQLErrno, "isolated setup/kill query failed");
}

SqlTransaction Save(uint64_t character, uint64_t revision, std::string receipt,
                    std::string blob = "checkpoint")
{
    return {{"UPDATE mission_sql_state SET revision=revision+1,checkpoint=? WHERE id=? AND revision=?",
             {SqlParam::Blob(std::move(blob)), SqlParam::UInt(character), SqlParam::UInt(revision)},
             1},
            {"INSERT INTO mission_sql_receipt(operation_id,character_id) VALUES(?,?)",
             {SqlParam::Text(std::move(receipt)), SqlParam::UInt(character)},
             1}};
}

void CommitRollbackAndBinary(CAsyncSQL &sql)
{
    const std::string binary = std::string("before\0after", 12) + std::string(70000, '\x91');
    Message saved(sql.DirectTransactionPrepared(Save(1, 0, "first", binary)));
    Check(saved->transactionOutcome == Outcome::Committed && !saved->uiSQLErrno,
          "real transaction not committed");
    Check(saved->transactionResultOffsets == std::vector<size_t>{0, 1},
          "real transaction results not associated");
    auto read = Query(sql, "SELECT revision,checkpoint FROM mission_sql_state WHERE id=1");
    const auto row = read->Get()->FetchRow();
    const auto lengths = read->Get()->FetchLengths();
    Check(row && lengths && std::string(row[1], lengths[1]) == binary && std::string(row[0]) == "1",
          "real binary checkpoint changed");
    Check(Number(sql, "SELECT @@autocommit") == 1, "session remained inside transaction");

    Message duplicate(sql.DirectTransactionPrepared(Save(1, 1, "first", "must rollback")));
    Check(duplicate->transactionOutcome == Outcome::RolledBack && duplicate->uiSQLErrno == ER_DUP_ENTRY &&
              duplicate->transactionFailedStep == 1,
          "duplicate ledger insert did not rollback progress");
    Check(Number(sql, "SELECT revision FROM mission_sql_state WHERE id=1") == 1,
          "rolled-back progress persisted");
    Check(Number(sql, "SELECT COUNT(*) FROM mission_sql_receipt WHERE character_id=1") == 1,
          "duplicate ledger survived");

    Message conflict(sql.DirectTransactionPrepared(Save(1, 0, "stale")));
    Check(conflict->transactionOutcome == Outcome::Conflict && conflict->uiSQLErrno == SqlTransactionConflict,
          "stale revision accepted");
    Check(Number(sql, "SELECT COUNT(*) FROM mission_sql_receipt WHERE operation_id='stale'") == 0,
          "CAS conflict issued a receipt");

    Message syntax(sql.DirectQueryPrepared("SELECT FROM", {}));
    Check(syntax->uiSQLErrno == ER_PARSE_ERROR, "statement prepare error was lost when the handle closed");
    Message parameterCount(sql.DirectQueryPrepared("SELECT ?", {}));
    Check(parameterCount->uiSQLErrno == CR_PARAMS_NOT_BOUND, "real parameter mismatch looked successful");
    auto failedPrepare = Save(1, 1, "failed-prepare");
    failedPrepare[1].query = "INSERT INTO mission_sql_missing_table(operation_id,character_id) VALUES(?,?)";
    Message prepareRollback(sql.DirectTransactionPrepared(std::move(failedPrepare)));
    Check(prepareRollback->transactionOutcome == Outcome::RolledBack &&
              prepareRollback->uiSQLErrno == ER_NO_SUCH_TABLE && prepareRollback->transactionFailedStep == 1,
          "prepare failure did not rollback the preceding write");
    Check(Number(sql, "SELECT revision FROM mission_sql_state WHERE id=1") == 1,
          "write before failed prepare survived rollback");

    TextQuery(sql, "START TRANSACTION");
    Query(sql, "UPDATE mission_sql_state SET revision=999 WHERE id=1");
    Message nested(sql.DirectTransactionPrepared(Save(1, 999, "must-not-adopt-dirty-work")));
    Check(nested->transactionOutcome == Outcome::Failed && nested->uiSQLErrno == SqlTransactionInvalid,
          "owned transaction adopted another caller's uncommitted work");
    Check(Number(sql, "SELECT revision FROM mission_sql_state WHERE id=1") == 1,
          "dirty connection cleanup committed unrelated work");
}

void PreparedProcedures(CAsyncSQL &sql)
{
    TextQuery(sql,
              "CREATE PROCEDURE mission_sql_multi(IN content LONGBLOB) BEGIN SELECT content AS checkpoint; "
              "SELECT id FROM mission_sql_state WHERE id=999; SELECT 19 AS third; END");
    const std::string binary("a\0b\0c", 5);
    for (int iteration = 0; iteration < 25; ++iteration)
    {
        auto result = Query(sql, "CALL mission_sql_multi(?)", {SqlParam::Blob(binary)});
        Check(result->vec_pkResult.size() == 4, "real CALL did not consume three results plus OK");
        const auto row = result->vec_pkResult[0]->FetchRow();
        const auto lengths = result->vec_pkResult[0]->FetchLengths();
        Check(row && lengths && std::string(row[0], lengths[0]) == binary, "CALL binary payload changed");
        Check(result->vec_pkResult[1]->uiNumRows == 0 && !result->vec_pkResult[1]->FetchRow(),
              "empty middle result lost");
        Check(std::string(result->vec_pkResult[2]->FetchRow()[0]) == "19", "later CALL result lost");
    }
    TextQuery(sql, "CREATE PROCEDURE mission_sql_late_error() BEGIN SELECT 1; "
                   "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='expected test failure'; END");
    Message error(sql.DirectQueryPrepared("CALL mission_sql_late_error()", {}));
    Check(error->uiSQLErrno == 1644 && sql.GetConnectionState() == CAsyncSQL::ConnectionState::Failed,
          "late stored procedure error lost");
    Check(Number(sql, "SELECT 23") == 23, "unread result packets poisoned next job");
}

void ConcurrentOwners(CAsyncSQL &admin)
{
    std::barrier ready(3);
    std::array<Outcome, 2> outcomes{};
    std::array<std::exception_ptr, 2> errors{};
    std::array<std::thread, 2> threads;
    for (size_t index = 0; index < threads.size(); ++index)
        threads[index] = std::thread(
            [&, index]
            {
                try
                {
                    CAsyncSQL sql;
                    // Check() intentionally stays on the main test thread.
                    sql.SetTimeouts(3);
                    if (!sql.Setup("127.0.0.1", "root", "", database.c_str(), "utf8mb4", true, port))
                        throw std::runtime_error("race connection failed");
                    ready.arrive_and_wait();
                    Message result(
                        sql.DirectTransactionPrepared(Save(2, 0, "race-" + std::to_string(index))));
                    outcomes[index] = result->transactionOutcome;
                }
                catch (...)
                {
                    errors[index] = std::current_exception();
                    ready.arrive_and_drop();
                }
            });
    ready.arrive_and_wait();
    for (auto &thread : threads)
        thread.join();
    for (auto &error : errors)
        if (error)
            std::rethrow_exception(error);
    Check((outcomes[0] == Outcome::Committed && outcomes[1] == Outcome::Conflict) ||
              (outcomes[1] == Outcome::Committed && outcomes[0] == Outcome::Conflict),
          "two owners committed the same revision");
    Check(Number(admin, "SELECT revision FROM mission_sql_state WHERE id=2") == 1 &&
              Number(admin, "SELECT COUNT(*) FROM mission_sql_receipt WHERE character_id=2") == 1,
          "owner race duplicated progress or receipt");
}

std::vector<Message> Wait(CAsyncSQL &sql, size_t count)
{
    std::vector<Message> results;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    while (results.size() < count && std::chrono::steady_clock::now() < deadline)
    {
        SQLMsg *message = nullptr;
        if (sql.PopResult(&message))
            results.emplace_back(message);
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    Check(results.size() == count, "worker queue did not complete");
    return results;
}

void WorkerOrder(CAsyncSQL &admin)
{
    CAsyncSQL sql;
    Connect(sql, false);
    sql.ReturnTransactionPrepared(Save(3, 0, "queue-first"), reinterpret_cast<void *>(1));
    sql.ReturnQueryPrepared("SELECT revision FROM mission_sql_state WHERE id=3", {},
                            reinterpret_cast<void *>(2));
    sql.ReturnTransactionPrepared(Save(3, 1, "queue-second"), reinterpret_cast<void *>(3));
    auto results = Wait(sql, 3);
    sql.Quit();
    Check(results[0]->transactionOutcome == Outcome::Committed &&
              results[2]->transactionOutcome == Outcome::Committed,
          "queued transaction failed");
    Check(results[1]->pvUserData == reinterpret_cast<void *>(2) &&
              std::string(results[1]->Get()->FetchRow()[0]) == "1",
          "ordinary queue job ran inside transaction");
    Check(Number(admin, "SELECT revision FROM mission_sql_state WHERE id=3") == 2, "queued progress lost");
}

SqlQueueDrainResult Drain(CAsyncSQL &sql)
{
    auto ticket = sql.DrainQueue();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ticket.Poll().status == SqlQueueDrainStatus::Pending &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Check(ticket.Poll().status == SqlQueueDrainStatus::Complete, "actual SQL queue did not drain");
    return ticket.Poll();
}
void QueueDrains(CAsyncSQL &admin)
{
    CAsyncSQL main, async;
    Connect(main, false);
    Connect(async, false);
    const auto baseline = main.QueueOrigin(), otherBaseline = async.QueueOrigin();
    main.AsyncQuery("SELECT 1; SELECT 2; SELECT 3");
    main.ReturnQueryPrepared("SELECT 19", {}, reinterpret_cast<void *>(19));
    auto first = Drain(main);
    auto success = Wait(main, 1);
    Check(!success[0]->uiSQLErrno && std::string(success[0]->Get()->FetchRow()[0]) == "19" &&
              first.position.ConfirmsSince(baseline),
          "fire-and-forget results poisoned the next query");
    // A successful SELECT after a failed non-returning cache write is not a
    // substitute for a verified drain receipt.
    main.AsyncQuery("INSERT INTO mission_sql_nonexistent VALUES(1)");
    main.ReturnQuery("SELECT 23", nullptr);
    auto result = Wait(main, 1);
    auto failed = Drain(main);
    Check(!result[0]->uiSQLErrno && failed.position.unconfirmed == 1 &&
              failed.position.lastSqlError == ER_NO_SUCH_TABLE && !failed.position.ConfirmsSince(baseline),
          "ordinary barrier hid a failed fire-and-forget cache write");
    async.AsyncQueryPrepared("SELECT FROM", {});
    async.ReturnQueryPrepared("SELECT 29", {}, nullptr);
    auto asyncSuccess = Wait(async, 1);
    auto failedAsync = Drain(async);
    Check(!asyncSuccess[0]->uiSQLErrno && failedAsync.position.unconfirmed == 1 &&
              failedAsync.position.lastSqlError == ER_PARSE_ERROR &&
              !failedAsync.position.ConfirmsSince(otherBaseline),
          "prepared async error disappeared at drain");
    Check(!failed.position.ConfirmsSince(otherBaseline), "other worker confirmed a foreign baseline");

    main.ReturnQuery("SELECT 31; SELECT * FROM mission_sql_nonexistent", nullptr);
    main.ReturnQueryPrepared("SELECT 37", {}, nullptr);
    auto late = Wait(main, 2);
    Check(late[0]->uiSQLErrno == ER_NO_SUCH_TABLE && late[0]->vec_pkResult.size() == 1 &&
              !late[1]->uiSQLErrno && std::string(late[1]->Get()->FetchRow()[0]) == "37",
          "late text result error lost or next query poisoned");
    auto afterLate = Drain(main);
    Check(afterLate.position.unconfirmed == 2 && afterLate.position.lastSqlError == ER_NO_SUCH_TABLE &&
              !afterLate.position.ConfirmsSince(failed.position),
          "late result error not included in drain");
    // Stored-procedure late errors must be recorded even with no returned SQLMsg.
    main.AsyncQuery("CALL mission_sql_late_error()");
    main.ReturnQueryPrepared("SELECT 41", {}, nullptr);
    auto afterCall = Wait(main, 1);
    auto callDrain = Drain(main);
    Check(!afterCall[0]->uiSQLErrno && callDrain.position.unconfirmed == 3 &&
              callDrain.position.lastSqlError == 1644,
          "non-returning procedure error was invisible");
    // The full rollback still means the requested cache batch was NOT saved.
    main.ReturnTransactionPrepared(Save(3, 9999, "drain-must-conflict"), nullptr);
    auto conflict = Wait(main, 1);
    auto afterConflict = Drain(main);
    Check(conflict[0]->transactionOutcome == Outcome::Conflict && afterConflict.position.unconfirmed == 4 &&
              !afterConflict.position.ConfirmsSince(callDrain.position),
          "rolled back work looked drained successfully");
    auto final = Drain(main);
    Check(final.position.ConfirmsSince(afterConflict.position) && !final.position.ConfirmsSince(baseline),
          "new drain silently cleared old failures");
    SQLMsg *unexpected = nullptr;
    Check(!main.PopResult(&unexpected), "drain was mixed into legacy SQL callbacks");
    Check(Number(admin,
                 "SELECT COUNT(*) FROM mission_sql_receipt WHERE operation_id='drain-must-conflict'") == 0,
          "failed drain test changed canonical progress");
}

void DisconnectBeforeCommit(CAsyncSQL &admin)
{
    CAsyncSQL sql;
    Connect(sql, false);
    // The worker is idle. This diagnostic handle read is not concurrent with SQL work.
    const auto connection = mysql_thread_id(sql.GetSQLHandle());
    auto transaction = Save(4, 0, "killed-before-commit");
    transaction.insert(transaction.begin() + 1, {"SELECT SLEEP(5)", {}, {}});
    sql.ReturnTransactionPrepared(std::move(transaction), nullptr);
    bool sleeping = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        sleeping =
            Number(
                admin,
                "SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE ID=? AND INFO='SELECT SLEEP(5)'",
                {SqlParam::UInt(connection)}) != 0;
        if (sleeping)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Check(sleeping, "did not observe the transaction's blocking step");
    TextQuery(admin, "KILL CONNECTION " + std::to_string(connection));
    sql.ReturnQueryPrepared("SELECT revision FROM mission_sql_state WHERE id=4", {}, nullptr);
    auto results = Wait(sql, 2);
    sql.Quit();
    Check(results[0]->transactionOutcome == Outcome::Failed && results[0]->uiSQLErrno,
          "disconnected transaction looks committed");
    Check(!results[1]->uiSQLErrno && std::string(results[1]->Get()->FetchRow()[0]) == "0",
          "worker replayed disconnected transaction or failed to reconnect");
    Check(Number(admin, "SELECT COUNT(*) FROM mission_sql_receipt WHERE character_id=4") == 0,
          "killed transaction committed a receipt");
}

void LostCommitAcknowledgement(CAsyncSQL &admin, unsigned int proxyPort)
{
    CAsyncSQL sql;
    Connect(sql, true, proxyPort);
    Message result(sql.DirectTransactionPrepared(Save(5, 0, "commit-response-lost")));
    Check(result->transactionOutcome == Outcome::CommitUnknown && result->uiSQLErrno,
          "lost COMMIT acknowledgement was reported as known");
    Check(Number(admin, "SELECT revision FROM mission_sql_state WHERE id=5") == 1 &&
              Number(admin,
                     "SELECT COUNT(*) FROM mission_sql_receipt WHERE operation_id='commit-response-lost'") ==
                  1,
          "proxy did not lose an actually committed response");
    Check(Number(sql, "SELECT COUNT(*) FROM mission_sql_receipt WHERE operation_id='commit-response-lost'") ==
              1,
          "unknown commit cannot be reconciled on a fresh connection");
    Message stale(sql.DirectTransactionPrepared(Save(5, 0, "commit-response-lost")));
    Check(stale->transactionOutcome == Outcome::Conflict, "unknown commit replayed progress/receipt");
}
} // namespace

std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("mission-sql-integration");
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
        if (argc != 4)
            throw std::runtime_error("usage: AsyncSqlMariaDbTests PORT FRESH_TEST_SCHEMA COMMIT_PROXY_PORT");
        port = static_cast<unsigned int>(std::stoul(argv[1]));
        database = argv[2];
        if (!database.starts_with("auriga_mission_sql_test_") || database.size() > 63 ||
            database.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != database.npos)
            throw std::runtime_error("only a fresh isolated mission SQL test schema is allowed");
        // Root with empty password belongs only to the temporary, loopback-only
        // instance created by AsyncSqlMariaDbTests.py. Never run against a real DB.
        CAsyncSQL setup;
        Check(setup.Setup("127.0.0.1", "root", "", "", "utf8mb4", true, port), "isolated bootstrap failed");
        TextQuery(setup, "CREATE DATABASE " + database + " CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
        CAsyncSQL sql;
        Connect(sql);
        TextQuery(sql, "CREATE TABLE mission_sql_state(id BIGINT UNSIGNED PRIMARY KEY, revision BIGINT "
                       "UNSIGNED NOT NULL, "
                       "checkpoint LONGBLOB NOT NULL) ENGINE=InnoDB");
        TextQuery(sql, "CREATE TABLE mission_sql_receipt(operation_id VARBINARY(96) PRIMARY KEY, "
                       "character_id BIGINT UNSIGNED NOT NULL) ENGINE=InnoDB");
        Query(sql, "INSERT INTO mission_sql_state VALUES(1,0,''),(2,0,''),(3,0,''),(4,0,''),(5,0,'')");
        std::cout << "commit / rollback / binary\n";
        CommitRollbackAndBinary(sql);
        std::cout << "prepared multiple results\n";
        PreparedProcedures(sql);
        std::cout << "concurrent owners\n";
        ConcurrentOwners(sql);
        std::cout << "worker ordering\n";
        WorkerOrder(sql);
        std::cout << "verified queue drains\n";
        QueueDrains(sql);
        std::cout << "disconnect before commit\n";
        DisconnectBeforeCommit(sql);
        std::cout << "lost commit acknowledgement\n";
        LostCommitAcknowledgement(sql, static_cast<unsigned int>(std::stoul(argv[3])));
        std::cout << "MariaDB prepared transaction checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
