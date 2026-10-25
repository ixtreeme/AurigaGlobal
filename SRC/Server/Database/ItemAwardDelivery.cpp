#include "ItemAwardDelivery.hpp"
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <charconv>
#include <set>
#include <stdexcept>
#include <string_view>

namespace auriga::item_award
{
namespace
{
void Need(bool value, const char *why)
{
    if (!value)
        throw std::invalid_argument(why);
}
std::string Table(std::string_view name, const std::string &postfix)
{
    Need(postfix.size() <= 32 &&
             postfix.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
                 postfix.npos,
         "Invalid table postfix");
    return '`' + std::string(name) + postfix + '`';
}
void CheckSource(const Source &source)
{
    Need(source.id && source.vnum && source.count && !source.login.empty() && source.login.size() <= 31 &&
             std::none_of(source.login.begin(), source.login.end(),
                          [](unsigned char c) { return c < 33 || c == 127; }),
         "Invalid award source");
}
SqlTransaction Preconditions(const std::string &postfix)
{
    Table("item", postfix);
    return {{"SELECT 1 WHERE @@foreign_key_checks=1 AND @@check_constraint_checks=1 "
             "AND (SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() "
             "AND ENGINE='InnoDB' AND TABLE_NAME IN (?,?,?))=3 "
             "AND NOT EXISTS (SELECT 1 FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() "
             "AND EVENT_OBJECT_TABLE IN (?,?,?))",
             {SqlParam::Text("item" + postfix), SqlParam::Text("item_award"),
              SqlParam::Text("safebox" + postfix), SqlParam::Text("item" + postfix),
              SqlParam::Text("item_award"), SqlParam::Text("safebox" + postfix)},
             {},
             1}};
}
SqlTransactionStep AwardRead(const Source &source, bool pending)
{
    SqlTransactionStep step;
    step.query = "SELECT taken_time IS NOT NULL,item_id FROM item_award WHERE id=? "
                 "AND BINARY LOWER(TRIM(login))=? AND vnum=? AND count=? AND mall=?";
    step.params = {SqlParam::UInt(source.id), SqlParam::Blob(source.login), SqlParam::UInt(source.vnum),
                   SqlParam::UInt(source.count), SqlParam::UInt(source.mall)};
    for (size_t i = 0; i < source.sockets.size(); ++i)
    {
        step.query += " AND socket" + std::to_string(i) + "=?";
        step.params.push_back(SqlParam::UInt(source.sockets[i]));
    }
    for (size_t i = 0; i < source.attributes.size(); ++i)
    {
        step.query += " AND attrtype" + std::to_string(i) + "=? AND attrvalue" + std::to_string(i) + "=?";
        step.params.push_back(SqlParam::UInt(source.attributes[i].type));
        step.params.push_back(SqlParam::Int(source.attributes[i].value));
    }
    if (pending)
    {
        step.query += " AND taken_time IS NULL AND item_id=0";
        step.expectedResultRows = 1;
    }
    step.query += " FOR UPDATE";
    return step;
}
void CheckPlacement(const Request &request)
{
    // Matches the existing five-column vertical grid; bounded independently of
    // database bSize, untrusted positions, counts or supplied prototype heights.
    Need(request.rows > 0 && request.rows <= 86 && request.rows % 9 == 0 && request.storage.size() <= 432,
         "Invalid container bounds");
    std::vector<bool> cells(size_t(request.rows) * 5);
    std::set<uint32_t> ids;
    const auto put = [&](uint16_t position, uint8_t height)
    {
        Need(height && size_t(position) + size_t(height - 1) * 5 < cells.size(), "Invalid item footprint");
        for (size_t i = 0; i < height; ++i)
        {
            const size_t cell = size_t(position) + i * 5;
            Need(!cells[cell], "Occupied item footprint");
            cells[cell] = true;
        }
    };
    for (const auto &stored : request.storage)
    {
        Need(stored.id && stored.vnum && stored.id != request.item.id && ids.insert(stored.id).second,
             "Invalid or duplicate stored item ID");
        put(stored.position, stored.height);
    }
    put(request.item.position, request.itemHeight);
}
uint32_t Number(const char *value, unsigned long length)
{
    Need(value && length, "Missing receipt value");
    uint32_t number = 0;
    auto parsed = std::from_chars(value, value + length, number);
    Need(parsed.ec == std::errc() && parsed.ptr == value + length, "Invalid receipt integer");
    return number;
}
} // namespace

Plan Deliver(const Request &request, const std::string &postfix)
{
    Plan plan;
    try
    {
        const auto &source = request.source;
        const auto &item = request.item;
        CheckSource(source);
        Need(item.id && item.account && item.vnum == source.vnum && item.count && item.count <= 50000,
             "Invalid materialized item");
        CheckPlacement(request);
        const auto items = Table("item", postfix);
        const auto safebox = Table("safebox", postfix);
        const std::string window = source.mall ? "MALL" : "SAFEBOX";
        plan.statements = Preconditions(postfix);
        // A row exists even for accounts that never changed their default box.
        plan.statements.push_back({"INSERT INTO " + safebox +
                                       "(account_id) VALUES(?) ON DUPLICATE KEY UPDATE account_id=account_id",
                                   {SqlParam::UInt(item.account)},
                                   {},
                                   {}});
        plan.statements.push_back({"SELECT account_id FROM " + safebox + " WHERE account_id=? FOR UPDATE",
                                   {SqlParam::UInt(item.account)},
                                   {},
                                   1});
        plan.statements.push_back(AwardRead(source, true));
        plan.statements.push_back({"SELECT id FROM " + items + " WHERE owner_id=? AND window=? FOR UPDATE",
                                   {SqlParam::UInt(item.account), SqlParam::Text(window)},
                                   {},
                                   request.storage.size()});
        if (!request.storage.empty())
        {
            SqlTransactionStep exact;
            exact.query = "SELECT id FROM " + items + " WHERE owner_id=? AND window=? AND (";
            exact.params = {SqlParam::UInt(item.account), SqlParam::Text(window)};
            for (const auto &stored : request.storage)
            {
                if (exact.params.size() > 2)
                    exact.query += " OR ";
                exact.query += "(id=? AND pos=? AND vnum=?)";
                exact.params.push_back(SqlParam::UInt(stored.id));
                exact.params.push_back(SqlParam::UInt(stored.position));
                exact.params.push_back(SqlParam::UInt(stored.vnum));
            }
            exact.query += ") FOR UPDATE";
            exact.expectedResultRows = request.storage.size();
            plan.statements.push_back(std::move(exact));
        }
        SqlTransactionStep insert;
        insert.query = "INSERT INTO " + items + "(id,owner_id,window,pos,vnum,count,lockedattr";
        insert.params = {SqlParam::UInt(item.id),   SqlParam::UInt(item.account),
                         SqlParam::Text(window),    SqlParam::UInt(item.position),
                         SqlParam::UInt(item.vnum), SqlParam::UInt(item.count),
                         SqlParam::Int(-1)};
        for (size_t i = 0; i < item.sockets.size(); ++i)
        {
            insert.query += ",socket" + std::to_string(i);
            insert.params.push_back(SqlParam::Int(item.sockets[i]));
        }
        for (size_t i = 0; i < item.attributes.size(); ++i)
        {
            insert.query += ",attrtype" + std::to_string(i) + ",attrvalue" + std::to_string(i);
            insert.params.push_back(SqlParam::UInt(item.attributes[i].type));
            insert.params.push_back(SqlParam::Int(item.attributes[i].value));
        }
        insert.query += ") VALUES(";
        for (size_t i = 0; i < insert.params.size(); ++i)
            insert.query += i ? ",?" : "?";
        insert.query += ')';
        insert.expectedAffectedRows = 1;
        plan.statements.push_back(std::move(insert));
        plan.statements.push_back({"UPDATE item_award SET taken_time=NOW(),item_id=? "
                                   "WHERE id=? AND taken_time IS NULL AND item_id=0",
                                   {SqlParam::UInt(item.id), SqlParam::UInt(source.id)},
                                   1,
                                   {}});
    }
    catch (const std::exception &error)
    {
        plan.statements.clear();
        plan.errors.push_back(error.what());
    }
    return plan;
}
bool Confirmed(const _SQLMsg &message)
{
    return message.bTransaction && !message.uiSQLErrno &&
           message.transactionOutcome == SqlTransactionOutcome::Committed;
}
SqlTransaction Resolve(const Source &source, const std::string &postfix)
{
    CheckSource(source);
    auto statements = Preconditions(postfix);
    statements.push_back(AwardRead(source, false));
    return statements;
}
Receipt DecodeReceipt(_SQLMsg &message)
{
    if (!Confirmed(message))
        return {ReceiptState::SqlError};
    try
    {
        Need(message.transaction.size() == 2 && message.transactionResultOffsets.size() == 2,
             "Unexpected receipt transaction");
        const size_t offset = message.transactionResultOffsets[1];
        Need(offset < message.vec_pkResult.size(), "Missing receipt result");
        auto *result = message.vec_pkResult[offset];
        if (!result->uiNumRows)
            return {ReceiptState::Missing};
        Need(result->bPrepared && result->uiNumRows == 1 && result->preparedRows.size() == 1 &&
                 result->preparedRows.front().size() == 2,
             "Invalid receipt result");
        auto row = result->FetchRow();
        auto lengths = result->FetchLengths();
        Need(row && lengths, "Invalid receipt row");
        const auto taken = Number(row[0], lengths[0]);
        const auto item = Number(row[1], lengths[1]);
        Need((taken == 0 && item == 0) || (taken == 1 && item != 0), "Inconsistent award receipt");
        return {taken ? ReceiptState::Taken : ReceiptState::Pending, item};
    }
    catch (const std::exception &)
    {
        return {ReceiptState::Invalid};
    }
}
} // namespace auriga::item_award
