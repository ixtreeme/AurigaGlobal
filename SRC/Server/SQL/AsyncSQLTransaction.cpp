#include "AsyncSQL.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace
{
uint32_t Error(MYSQL *connection)
{
    const auto error = mysql_errno(connection);
    return error ? error : CR_UNKNOWN_ERROR;
}

bool Valid(const SqlTransaction &transaction)
{
    if (transaction.empty() || transaction.size() > SqlTransactionLimits::Steps)
        return false;
    size_t bytes = 0, parameters = 0;
    for (const auto &step : transaction)
    {
        if (step.query.size() > SqlTransactionLimits::QueryBytes ||
            step.params.size() > SqlTransactionLimits::Parameters - parameters)
            return false;
        parameters += step.params.size();
        const auto add = [&](size_t count)
        {
            if (count > SqlTransactionLimits::TotalBytes - bytes)
                return false;
            bytes += count;
            return true;
        };
        if (!add(step.query.size()))
            return false;
        for (const auto &param : step.params)
            if (!add(param.bytes.size()))
                return false;

        // Intentionally conservative: no transaction control, DDL, CALL,
        // multi-statements, comments or embedded NUL in fixed statement text.
        // Triggers/functions and table engines remain the trusted caller's responsibility.
        std::string_view text = step.query;
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
            text.remove_prefix(1);
        size_t end = 0;
        while (end < text.size() && std::isalpha(static_cast<unsigned char>(text[end])))
            ++end;
        std::string verb(text.substr(0, end));
        std::transform(verb.begin(), verb.end(), verb.begin(),
                       [](unsigned char c) { return char(std::toupper(c)); });
        if (end == text.size() || !std::isspace(static_cast<unsigned char>(text[end])) ||
            (verb != "SELECT" && verb != "INSERT" && verb != "UPDATE" && verb != "DELETE") ||
            (verb == "SELECT" && step.expectedAffectedRows) || text.find_first_of(";#") != text.npos ||
            text.find('\0') != text.npos || text.find("/*") != text.npos || text.find("--") != text.npos)
            return false;
    }
    return true;
}

template <typename F> struct ScopeExit
{
    F cleanup;
    ~ScopeExit()
    {
        cleanup();
    }
};
} // namespace

SQLMsg *CAsyncSQL::DirectTransactionPrepared(SqlTransaction transaction)
{
    auto message = std::make_unique<SQLMsg>();
    message->m_pkSQL = &m_hDB;
    message->stQuery = "[prepared transaction]";
    message->bPrepared = message->bTransaction = true;
    message->transaction = std::move(transaction);
    if (!m_directConnection)
    {
        message->uiSQLErrno = SqlTransactionInvalid;
        message->transactionOutcome = SqlTransactionOutcome::Failed;
    }
    else
    {
        message->iID = ++m_iMsgCount;
        TryExecuteTransaction(message.get());
    }
    if (!message->Get())
        message->StorePrepared(0, 0);
    return message.release();
}

void CAsyncSQL::ReturnTransactionPrepared(SqlTransaction transaction, void *userData)
{
    if (!m_mtxQuery || !m_mtxResult || m_directConnection)
        throw std::logic_error("ReturnTransactionPrepared requires asynchronous Setup");
    auto message = std::make_unique<SQLMsg>();
    message->m_pkSQL = &m_hDB;
    message->iID = ++m_iMsgCount;
    message->stQuery = "[prepared transaction]";
    message->bPrepared = message->bTransaction = message->bReturn = true;
    message->pvUserData = userData;
    message->transaction = std::move(transaction);
    PushQuery(message.release());
}

bool CAsyncSQL::TryExecuteTransaction(SQLMsg *message)
{
    message->transactionOutcome = SqlTransactionOutcome::Failed;
    if (m_inPreparedTransaction || !Valid(message->transaction))
    {
        message->uiSQLErrno = SqlTransactionInvalid;
        return false;
    }
    if (!EnsureConnectionReady())
    {
        message->uiSQLErrno = Error(&m_hDB);
        return false;
    }

    // Ping only before BEGIN, with reconnect disabled. A failed connection
    // can be recreated at the next job boundary; this job is never replayed.
    my_bool reconnect = 0;
    if (mysql_options(&m_hDB, MYSQL_OPT_RECONNECT, &reconnect) || mysql_ping(&m_hDB))
    {
        message->uiSQLErrno = Error(&m_hDB);
        InvalidateConnection();
        return false;
    }
    if (!(m_hDB.server_status & SERVER_STATUS_AUTOCOMMIT) ||
        (m_hDB.server_status & SERVER_STATUS_IN_TRANS))
    {
        // Never adopt/commit work left open by a previous raw SQL caller.
        message->uiSQLErrno = SqlTransactionInvalid;
        InvalidateConnection();
        return false;
    }
    if (!EnsureConnectionReady() || mysql_autocommit(&m_hDB, 0))
    {
        message->uiSQLErrno = Error(&m_hDB);
        InvalidateConnection();
        return false;
    }

    m_inPreparedTransaction = true;
    m_transactionThreadId = mysql_thread_id(&m_hDB);
    bool settled = false;
    const auto resetSession = [&]
    {
        reconnect = 1;
        // Turning autocommit on can COMMIT. Only do so after a confirmed
        // COMMIT/ROLLBACK on this exact connection. Otherwise discard it.
        if (mysql_autocommit(&m_hDB, 1) || mysql_options(&m_hDB, MYSQL_OPT_RECONNECT, &reconnect))
            InvalidateConnection();
    };
    ScopeExit cleanup{[&]
                      {
                          if (!settled && m_mysqlInitialized)
                          {
                              if (mysql_thread_id(&m_hDB) != m_transactionThreadId || mysql_rollback(&m_hDB))
                                  InvalidateConnection();
                              else
                                  resetSession();
                          }
                          m_inPreparedTransaction = false;
                      }};

    const auto rollback = [&](bool conflict)
    {
        settled = true;
        if (!m_mysqlInitialized)
            return;
        if (mysql_thread_id(&m_hDB) != m_transactionThreadId || mysql_rollback(&m_hDB))
        {
            InvalidateConnection();
            return;
        }
        message->transactionOutcome =
            conflict ? SqlTransactionOutcome::Conflict : SqlTransactionOutcome::RolledBack;
        resetSession();
    };

    for (size_t index = 0; index < message->transaction.size(); ++index)
    {
        auto &step = message->transaction[index];
        message->transactionFailedStep = index;
        SQLMsg result;
        result.m_pkSQL = &m_hDB;
        result.bPrepared = true;
        result.stQuery = step.query;
        result.vecParams = step.params;
        const bool executed = TryExecutePrepared(&result);
        const bool conflict = executed && step.expectedAffectedRows &&
                              (!result.Get() || result.Get()->uiAffectedRows != *step.expectedAffectedRows);
        message->transactionResultOffsets.push_back(message->vec_pkResult.size());
        message->vec_pkResult.insert(message->vec_pkResult.end(), result.vec_pkResult.begin(),
                                     result.vec_pkResult.end());
        result.vec_pkResult.clear();
        if (!executed || conflict)
        {
            message->uiSQLErrno = conflict ? SqlTransactionConflict : result.uiSQLErrno;
            if (!message->uiSQLErrno)
                message->uiSQLErrno = CR_UNKNOWN_ERROR;
            rollback(conflict);
            return false;
        }
    }

    message->transactionFailedStep = SqlTransactionNoStep;
    if (mysql_thread_id(&m_hDB) != m_transactionThreadId)
    {
        message->uiSQLErrno = CR_SERVER_LOST;
        rollback(false);
        return false;
    }
    settled = true;
    if (mysql_commit(&m_hDB))
    {
        message->uiSQLErrno = Error(&m_hDB);
        message->transactionOutcome = SqlTransactionOutcome::CommitUnknown;
        // No rollback, implicit commit, reconnect or retry after an unknown COMMIT.
        InvalidateConnection();
        return false;
    }
    message->uiSQLErrno = 0;
    message->transactionOutcome = SqlTransactionOutcome::Committed;
    resetSession();
    return true;
}
