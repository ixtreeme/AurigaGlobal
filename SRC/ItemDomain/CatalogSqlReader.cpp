#include "CatalogSqlReader.hpp"
#include <SQL/AsyncSQL.h>
#include <memory>

namespace auriga::item::sql
{
namespace
{
std::vector<std::string> Columns(const TableSpec& spec)
{
    std::vector<std::string> columns;
    std::size_t start = 0;
    while (start < spec.columns.size())
    {
        const auto end = spec.columns.find(',', start);
        columns.emplace_back(spec.columns.substr(start, end == spec.columns.npos ? end : end - start));
        if (end == spec.columns.npos) break;
        start = end + 1;
    }
    return columns;
}
std::string BudgetGuard()
{
    std::string query = "SELECT 1 FROM (";
    bool first = true;
    for (const auto& spec : Tables())
    {
        if (!first) query += " UNION ALL ";
        first = false;
        query += "SELECT COUNT(*) AS n," + std::to_string(spec.maximumRows) + " AS cap,";
        query += "COUNT(*)*" + std::to_string(ColumnCount(spec)*CellBudgetOverhead) + "+COALESCE(SUM(";
        bool firstColumn = true;
        for (const auto& column : Columns(spec))
        {
            if (!firstColumn) query += "+";
            firstColumn = false;
            query += "COALESCE(OCTET_LENGTH(`" + column + "`),0)";
        }
        query += "),0) AS b FROM `protos`.`" + std::string(spec.name) + "`";
    }
    query += ") AS catalog_budget HAVING MIN(n<=cap)=1 AND SUM(b)<=?";
    return query;
}
std::string EngineGuard()
{
    std::string query = "SELECT 1 FROM information_schema.TABLES WHERE TABLE_SCHEMA='protos' AND TABLE_NAME IN (";
    bool first = true;
    for (const auto& spec : Tables())
    {
        if (!first) query += ",";
        first = false;
        query += "'" + std::string(spec.name) + "'";
    }
    query += ") HAVING COUNT(*)=? AND SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE')=?";
    return query;
}
ReadError Failure(_SQLMsg& message)
{
    ReadError error{ReadErrorCode::SqlFailure,{}};
    error.sqlError = message.uiSQLErrno;
    error.failedStep = message.transactionFailedStep;
    if (error.failedStep >= ReadGuardSteps && error.failedStep - ReadGuardSteps < TableCount)
        error.table = static_cast<Table>(error.failedStep - ReadGuardSteps);
    if (message.transactionOutcome == SqlTransactionOutcome::CommitUnknown)
        error.code = ReadErrorCode::SnapshotUnconfirmed;
    else if (message.transactionOutcome == SqlTransactionOutcome::Conflict)
    {
        switch (message.transactionFailedStep)
        {
        case 0: error.code = ReadErrorCode::IsolationRequired; break;
        case 1: case 3: error.code = ReadErrorCode::FormatMismatch; break;
        case 2: error.code = ReadErrorCode::ByteBudget; break;
        default: break;
        }
    }
    return error;
}
}
SqlTransaction PrepareRead()
{
    SqlTransaction plan{
        {"SELECT 1 WHERE @@tx_isolation='REPEATABLE-READ'",{}, {},1},
        {"SELECT 1 FROM protos.catalog_format WHERE singleton=1 AND schema_version=? AND BINARY canonical_domain=?",
            {SqlParam::UInt(CatalogSchemaVersion),SqlParam::Text(std::string(CanonicalDomain))},{},1},
        {BudgetGuard(),{SqlParam::UInt(CapturedByteBudget)},{},1},
        {EngineGuard(),{SqlParam::UInt(TableCount),SqlParam::UInt(TableCount)},{},1},
    };
    // The metadata guard starts the InnoDB read view. Budget scans acquire MDL
    // on every source table before the engine check; all following reads use
    // that same snapshot. No locking reads, reconnects or per-table jobs.
    for (const auto& spec : Tables())
        plan.push_back({"SELECT " + std::string(spec.columns) + " FROM `protos`.`" +
            std::string(spec.name) + "` LIMIT ?", {SqlParam::UInt(spec.maximumRows + 1)}, {}, {}});
    return plan;
}
std::expected<CapturedCatalog, ReadError> CaptureCompleted(_SQLMsg& message, ReadPin pin)
{
    if (!pin.revision) return std::unexpected(ReadError{ReadErrorCode::InvalidPin,{}});
    if (!message.bTransaction || message.uiSQLErrno || message.transactionOutcome != SqlTransactionOutcome::Committed)
        return std::unexpected(Failure(message));
    const auto count = ReadGuardSteps + TableCount;
    const auto malformed = [] { return std::unexpected(ReadError{ReadErrorCode::ResultShape,{}}); };
    if (message.transactionResultOffsets.size() != count || message.vec_pkResult.size() != count) return malformed();
    for (std::size_t step = 0; step < count; ++step)
        if (message.transactionResultOffsets[step] != step || !message.vec_pkResult[step] || !message.vec_pkResult[step]->bPrepared)
            return malformed();
    for (std::size_t step = 0; step < ReadGuardSteps; ++step)
    {
        const auto& result = *message.vec_pkResult[step];
        if (result.uiNumRows != 1 || result.preparedRows.size() != 1 || result.preparedRows.front().size() != 1 ||
            result.preparedLengths.size() != 1 || result.preparedLengths.front().size() != 1 ||
            result.preparedLengths.front()[0] != 1 || !result.preparedRows.front()[0] ||
            result.preparedRows.front()[0][0] != '1') return malformed();
    }
    Snapshot snapshot;
    std::size_t bytes = 0;
    for (const auto& spec : Tables())
    {
        auto& result = *message.vec_pkResult[ReadGuardSteps + static_cast<std::size_t>(spec.table)];
        if (result.uiNumRows != result.preparedRows.size() || result.preparedRows.size() > spec.maximumRows)
            return std::unexpected(ReadError{ReadErrorCode::RowLimit,spec.table});
        if (result.preparedLengths.size() != result.preparedRows.size()) return malformed();
        const auto columns = ColumnCount(spec);
        for (std::size_t index = 0; index < result.preparedRows.size(); ++index)
        {
            const auto& raw = result.preparedRows[index];
            const auto& lengths = result.preparedLengths[index];
            if (raw.size() != columns || lengths.size() != columns) return malformed();
            Row row;
            for (std::size_t column = 0; column < columns; ++column)
            {
                const auto length = raw[column] ? lengths[column] : 0;
                const auto cost = CellBudgetOverhead + static_cast<std::size_t>(length);
                if (cost > CapturedByteBudget - bytes)
                    return std::unexpected(ReadError{ReadErrorCode::ByteBudget,spec.table,index,column});
                bytes += cost;
                row.push_back(raw[column] ? Cell{std::string(raw[column],length)} : std::nullopt);
            }
            snapshot[spec.table].push_back(std::move(row));
        }
    }
    auto catalog = DecodeSnapshot(snapshot,pin);
    if (!catalog) return std::unexpected(catalog.error());
    return CapturedCatalog{std::move(snapshot),*catalog};
}
std::expected<std::shared_ptr<const ItemCatalog>, ReadError> ReadCompleted(_SQLMsg& message, ReadPin pin)
{
    auto captured = CaptureCompleted(message,pin);
    if (!captured) return std::unexpected(captured.error());
    return captured->catalog;
}
std::expected<std::shared_ptr<const ItemCatalog>, ReadError> ReadCatalog(CAsyncSQL& connection, ReadPin pin)
{
    if (!pin.revision) return std::unexpected(ReadError{ReadErrorCode::InvalidPin,{}});
    std::unique_ptr<SQLMsg> message(connection.DirectTransactionPrepared(PrepareRead()));
    if (!message) return std::unexpected(ReadError{ReadErrorCode::SqlFailure,{}});
    return ReadCompleted(*message,pin);
}
} // namespace auriga::item::sql
