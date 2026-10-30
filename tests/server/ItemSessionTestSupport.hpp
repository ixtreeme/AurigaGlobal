#pragma once
// Test-only support for the GameServer item session/writer state machines:
// a recording executor and exact fabricated CAsyncSQL completions.
#include "../../SRC/Server/GameServer/item/ItemOwnerSessions.hpp"
#include <SQL/AsyncSQL.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace item_session_support
{
using namespace auriga::item;
namespace rt = auriga::item::runtime;
using Message = std::unique_ptr<SQLMsg>;
using Rows = std::vector<std::vector<std::optional<std::string>>>;

inline constexpr ItemOwner Owner{ItemOwnerKind::Character, 42};
inline rt::ItemSessionLease Lease(std::uint64_t generation, std::uint64_t entity)
{
    rt::ItemSessionLease lease;
    lease.key.generation = generation;
    lease.key.account = 7;
    lease.key.character = 42;
    lease.entity = entity;
    return lease;
}

struct FakeExecutor final : rt::ItemSqlExecutor
{
    bool refuse = false;
    std::vector<std::pair<std::uint64_t, SqlTransaction>> jobs;
    bool Submit(std::uint64_t ticket, SqlTransaction job) override
    {
        if (refuse) return false;
        jobs.emplace_back(ticket, std::move(job));
        return true;
    }
};

// The exact completion CAsyncSQL produces for a committed job: same steps, one
// prepared result per step, matching row expectations; SELECT rows as given.
inline Message Completed(const SqlTransaction& job, const std::map<std::size_t, Rows>& rows)
{
    auto m = std::make_unique<SQLMsg>();
    m->bPrepared = m->bTransaction = true;
    m->transaction = job;
    m->transactionOutcome = SqlTransactionOutcome::Committed;
    for (std::size_t i = 0; i < job.size(); ++i)
    {
        auto result = std::make_unique<SQLResult>();
        result->bPrepared = true;
        result->uiAffectedRows = job[i].expectedAffectedRows.value_or(0);
        Rows given;
        if (const auto found = rows.find(i); found != rows.end()) given = found->second;
        else if (job[i].expectedResultRows) given.assign(*job[i].expectedResultRows, {std::string("1")});
        result->uiNumRows = given.size();
        for (const auto& row : given)
        {
            std::vector<char*> cells;
            std::vector<unsigned long> lengths;
            for (const auto& cell : row)
            {
                if (cell)
                {
                    result->preparedCells.push_back(*cell);
                    cells.push_back(result->preparedCells.back().data());
                    lengths.push_back(static_cast<unsigned long>(cell->size()));
                }
                else
                {
                    cells.push_back(nullptr);
                    lengths.push_back(0);
                }
            }
            result->preparedRows.push_back(std::move(cells));
            result->preparedLengths.push_back(std::move(lengths));
        }
        m->transactionResultOffsets.push_back(m->vec_pkResult.size());
        m->vec_pkResult.push_back(result.release());
    }
    return m;
}
inline Message Failed(const SqlTransaction& job, SqlTransactionOutcome outcome, std::size_t step, std::uint32_t error)
{
    auto m = std::make_unique<SQLMsg>();
    m->bPrepared = m->bTransaction = true;
    m->transaction = job;
    m->transactionOutcome = outcome;
    m->transactionFailedStep = step;
    m->uiSQLErrno = error;
    return m;
}
inline Message Claimed(const SqlTransaction& job, std::uint64_t generation, std::uint64_t revision)
{
    return Completed(job, {{4, {{std::to_string(generation), std::to_string(revision)}}}});
}
inline Message LoadedRows(const SqlTransaction& job, std::uint64_t generation, std::uint64_t revision)
{
    return Completed(job, {{3, {{std::to_string(generation), std::to_string(revision)}}},
                           {4, {{"1", "1", "5", "9", "2"}, {"2", "1", "8", "1", "1"}}},
                           {5, {{"1", "1", "1000"}}}});
}
inline std::vector<rt::ItemSessionEvent> Events(rt::ItemOwnerSessions& sessions) { return sessions.TakeEvents(); }
inline bool SameSteps(const SqlTransaction& a, const SqlTransaction& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].query != b[i].query || a[i].params.size() != b[i].params.size() ||
            a[i].expectedAffectedRows != b[i].expectedAffectedRows || a[i].expectedResultRows != b[i].expectedResultRows) return false;
        for (std::size_t j = 0; j < a[i].params.size(); ++j)
        {
            const auto& x = a[i].params[j];
            const auto& y = b[i].params[j];
            if (x.type != y.type || x.i64 != y.i64 || x.u64 != y.u64 || x.bytes != y.bytes) return false;
        }
    }
    return true;
}

} // namespace item_session_support
