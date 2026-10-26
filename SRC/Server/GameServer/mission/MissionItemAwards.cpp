#include "MissionItemAwards.hpp"
#include "MissionPersistenceInternal.hpp"
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace auriga::mission::persistence
{
namespace
{
void Need(bool value, const char *why)
{
    if (!value)
        throw std::invalid_argument(why);
}
template <typename T> std::string Binary(const T &value)
{
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}
std::string Identifier(const std::string &name, size_t maximum = 64, bool empty = false)
{
    Need((empty || !name.empty()) && name.size() <= maximum &&
             name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
                 name.npos,
         "Invalid item-award SQL identifier");
    return '`' + name + '`';
}
std::string Table(const std::string &schema, const std::string &name)
{
    return Identifier(schema) + '.' + Identifier(name);
}
ContentDigest BindRequest(const ItemAwardsPlan &plan)
{
    std::string bytes("MEITEM\1\0", 8);
    bytes += Binary(plan.transaction.request);
    // Length prefix each field; boundaries must not be ambiguous.
    for (const auto &field : {plan.target.playerSchema, plan.target.accountSchema, plan.target.playerPostfix,
                              std::to_string(plan.recipient.account), plan.recipient.login})
    {
        bytes.push_back(char(field.size()));
        bytes += field;
    }
    return ContentHash({reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()});
}
std::string_view Cell(MYSQL_ROW row, unsigned long *lengths, size_t column)
{
    Need(row && lengths && row[column], "Missing item-award receipt field");
    return {row[column], lengths[column]};
}
uint64_t Number(std::string_view cell, uint64_t maximum = UINT64_MAX)
{
    uint64_t value = 0;
    auto parsed = std::from_chars(cell.data(), cell.data() + cell.size(), value);
    Need(!cell.empty() && parsed.ec == std::errc{} && parsed.ptr == cell.data() + cell.size() &&
             value <= maximum,
         "Invalid item-award receipt number");
    return value;
}
} // namespace
ItemAwardsPlan detail::DraftItemAwards(Owner owner, const PinnedReward &reward, uint64_t storageRevision,
                                       ContentDigest checkpoint, Token operation, ItemAwardTarget target,
                                       ItemAwardRecipient recipient,
                                       const std::map<uint32_t, uint32_t> &verifiedMaximumCounts)
{
    ItemAwardsPlan result;
    try
    {
        const auto &payload = reward.Payload();
        Need(ValidRewardBundle(payload), "Invalid reward bundle");
        Identifier(target.playerSchema);
        Identifier(target.accountSchema);
        for (auto schema : {target.playerSchema, target.accountSchema})
        {
            std::transform(schema.begin(), schema.end(), schema.begin(),
                           [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
            Need(schema != "mysql" && schema != "information_schema" && schema != "performance_schema" &&
                     schema != "sys",
                 "Item-award target cannot be a system schema");
        }
        Identifier(target.playerPostfix, 32, true);
        const auto playerTable = Table(target.playerSchema, "player" + target.playerPostfix);
        const auto accountTable = Table(target.accountSchema, "account");
        const auto awardTable = Table(target.playerSchema, "item_award");
        Need(recipient.account && !recipient.login.empty() && recipient.login.size() <= 30 &&
                 std::all_of(recipient.login.begin(), recipient.login.end(),
                             [](unsigned char c) { return c >= 33 && c < 127 && !(c >= 'A' && c <= 'Z'); }),
             "Invalid canonical account identity");
        for (const auto &[vnum, count] : payload.items)
        {
            auto cap = verifiedMaximumCounts.find(vnum);
            Need(cap != verifiedMaximumCounts.end() && cap->second &&
                     cap->second <= RewardLimits::ItemCount && count <= cap->second,
                 "Missing/insufficient verified item prototype stack cap");
        }
        result.transaction = RecordEntitlement(owner, reward, storageRevision, checkpoint, operation);
        Need(bool(result.transaction), "Invalid fenced entitlement plan");
        result.target = std::move(target);
        result.recipient = std::move(recipient);
        result.action = reward.Action();
        result.definition = reward.DefinitionDigest();
        result.payload = reward.PayloadDigest();
        result.items = payload.items;
        auto &plan = result.transaction;
        plan.request = BindRequest(result);
        // Preserve the existing durable operation protocol, now bound to the
        // recipient and SQL targets as well as the immutable full payload.
        auto operationReceipt = std::move(plan.statements.back());
        plan.statements.pop_back();
        operationReceipt.params[2] = SqlParam::Blob(Binary(plan.request));
        // Nothing below executes before the character/outbox fences above.
        plan.statements.push_back(
            {"SELECT 1 WHERE DATABASE()<>? AND DATABASE()<>? AND "
             "(SELECT COUNT(*) FROM information_schema.TABLES WHERE ENGINE='InnoDB' AND "
             "((TABLE_SCHEMA=? AND TABLE_NAME IN (?, 'item_award')) OR "
             "(TABLE_SCHEMA=? AND TABLE_NAME='account')))=3 AND "
             "NOT EXISTS(SELECT 1 FROM information_schema.TRIGGERS WHERE "
             "(TRIGGER_SCHEMA=? AND EVENT_OBJECT_TABLE IN (?, 'item_award')) OR "
             "(TRIGGER_SCHEMA=? AND EVENT_OBJECT_TABLE='account') OR TRIGGER_SCHEMA=DATABASE()) AND "
             "EXISTS(SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=? AND "
             "TABLE_NAME='item_award' AND COLUMN_NAME='id' AND EXTRA='auto_increment' AND COLUMN_KEY='PRI')",
             {SqlParam::Text(result.target.playerSchema), SqlParam::Text(result.target.accountSchema),
              SqlParam::Text(result.target.playerSchema),
              SqlParam::Text("player" + result.target.playerPostfix),
              SqlParam::Text(result.target.accountSchema), SqlParam::Text(result.target.playerSchema),
              SqlParam::Text("player" + result.target.playerPostfix),
              SqlParam::Text(result.target.accountSchema), SqlParam::Text(result.target.playerSchema)},
             {},
             1});
        plan.statements.push_back(
            {"SELECT id FROM " + playerTable + " WHERE id=? AND account_id=? FOR UPDATE",
             {SqlParam::UInt(owner.character), SqlParam::UInt(result.recipient.account)},
             {},
             1});
        plan.statements.push_back(
            {"SELECT id FROM " + accountTable + " WHERE id=? AND BINARY LOWER(TRIM(login))=? FOR UPDATE",
             {SqlParam::UInt(result.recipient.account), SqlParam::Blob(result.recipient.login)},
             {},
             1});
        uint32_t ordinal = 0;
        for (const auto &[vnum, count] : result.items)
        {
            plan.statements.push_back(
                {"INSERT INTO " + awardTable +
                     "(login,vnum,count,mall,why,given_time) VALUES(?,?,?,0,'Mission',NOW())",
                 {SqlParam::Text(result.recipient.login), SqlParam::UInt(vnum), SqlParam::UInt(count)},
                 1,
                 {}});
            // The owned transaction never changes connections between these
            // steps. No MAX(id), no second writer, no separate mapping commit.
            plan.statements.push_back(
                {"INSERT INTO mission_engine_item_awards(action_id,ordinal,payload_digest,player_schema,"
                 "account_schema,player_postfix,account_id,login,award_id,vnum,item_count) "
                 "SELECT ?,?,?,?,?,?,?,?,LAST_INSERT_ID(),?,? WHERE LAST_INSERT_ID() BETWEEN 1 AND "
                 "4294967295",
                 {SqlParam::Blob(Binary(ActionIdentity(result.action))), SqlParam::UInt(ordinal++),
                  SqlParam::Blob(Binary(result.payload)), SqlParam::Blob(result.target.playerSchema),
                  SqlParam::Blob(result.target.accountSchema), SqlParam::Blob(result.target.playerPostfix),
                  SqlParam::UInt(result.recipient.account), SqlParam::Blob(result.recipient.login),
                  SqlParam::UInt(vnum), SqlParam::UInt(count)},
                 1,
                 {}});
        }
        plan.statements.push_back(std::move(operationReceipt));
        return result;
    }
    catch (const std::exception &error)
    {
        result = {};
        result.transaction.errors.push_back({"item_awards", error.what()});
        return result;
    }
}
ItemAwardsPlan IssueItemAwards(Owner owner, const PinnedReward &reward, uint64_t revision,
                               ContentDigest checkpoint, Token operation, ItemAwardTarget target,
                               ItemAwardRecipient recipient, const std::map<uint32_t, uint32_t> &caps)
{
    const auto &payload = reward.Payload();
    if (payload.items.empty() || payload.experience || payload.gold || !payload.consumeItems.empty())
    {
        ItemAwardsPlan rejected;
        rejected.transaction.errors.push_back(
            {"item_awards", "Adapter requires an entirely item-only reward bundle"});
        return rejected;
    }
    return detail::DraftItemAwards(owner, reward, revision, checkpoint, operation, std::move(target),
                                   std::move(recipient), caps);
}
SqlTransaction ResolveItemAwards(const ItemAwardsPlan &plan)
{
    Need(bool(plan), "Cannot resolve invalid item-award plan");
    auto statements = Resolve(plan.transaction);
    const auto &action = plan.action;
    statements.push_back(
        {"SELECT o.action_state,o.definition_digest,l.owner_epoch,l.storage_revision,"
         "m.ordinal,m.receipt_format,m.payload_digest,m.player_schema,m.account_schema,m.player_postfix,"
         "m.account_id,m.login,m.award_id,m.vnum,m.item_count FROM mission_engine_action_outbox o "
         "LEFT JOIN mission_engine_reward_ledger l ON l.action_id=o.action_id "
         "LEFT JOIN mission_engine_item_awards m ON m.action_id=o.action_id WHERE "
         "o.action_id=? AND o.character_id=? AND o.mission_id=? AND o.definition_version=? AND "
         "o.instance_id=? AND o.node_id=? AND o.reward_id=? ORDER BY m.ordinal LIMIT 33 FOR UPDATE",
         {SqlParam::Blob(Binary(ActionIdentity(action))), SqlParam::UInt(action.character),
          SqlParam::Blob(action.mission), SqlParam::UInt(action.version), SqlParam::UInt(action.instance),
          SqlParam::Blob(action.node), SqlParam::Blob(action.reward)},
         {},
         {}});
    return statements;
}
ItemAwardsReceipt DecodeItemAwards(_SQLMsg &message, const ItemAwardsPlan &plan, size_t expectedSteps)
{
    if (!Confirmed(message))
        return {ItemAwardsStatus::SqlError, {}};
    try
    {
        const auto operation = DecodeResolution(message, plan.transaction, expectedSteps);
        if (operation.status == Resolution::TokenConflict)
            return {ItemAwardsStatus::Conflict, {}};
        Need(operation.status == Resolution::Committed || operation.status == Resolution::Absent,
             "Invalid item-award operation resolution");
        const auto offset = message.transactionResultOffsets.at(3);
        Need(offset < message.vec_pkResult.size(), "Missing item-award mapping result");
        auto *rows = message.vec_pkResult[offset];
        Need(rows && rows->bPrepared && rows->uiNumRows <= 32 && rows->preparedRowPos == 0 &&
                 rows->preparedRows.size() == rows->uiNumRows &&
                 rows->preparedLengths.size() == rows->uiNumRows,
             "Invalid bounded item-award receipt shape");
        if (!rows->uiNumRows)
        {
            Need(operation.status == Resolution::Absent, "Committed issuance lost its mapping");
            return {ItemAwardsStatus::NotIssued, {}};
        }
        ItemAwardsReceipt result{ItemAwardsStatus::Issued, {}};
        auto item = plan.items.begin();
        std::set<uint32_t> ids;
        for (size_t i = 0; i < rows->uiNumRows; ++i)
        {
            Need(rows->preparedRows[i].size() == 15 && rows->preparedLengths[i].size() == 15,
                 "Invalid item-award receipt columns");
            auto row = rows->FetchRow();
            auto lengths = rows->FetchLengths();
            auto cell = [&](size_t column) { return Cell(row, lengths, column); };
            const auto state = Number(cell(0));
            Need(cell(1) == Binary(plan.definition), "Item-award definition changed");
            if (state == 1 || state == 2)
            {
                Need(rows->uiNumRows == 1 && !row[2] && !row[3] && !row[4] &&
                         operation.status == Resolution::Absent,
                     "Unissued action has effects");
                return {state == 1 ? ItemAwardsStatus::NotIssued : ItemAwardsStatus::Cancelled, {}};
            }
            if (state == 3 && plan.items.empty() && plan.transaction.kind == Operation::ApplyResources)
            {
                Need(rows->uiNumRows == 1 && Number(cell(2)) && Number(cell(3)),
                     "Invalid currency-only entitlement");
                for (size_t column = 4; column < 15; ++column)
                    Need(!row[column], "Unexpected item mapping on currency-only effect");
                return result;
            }
            Need(state == 3 && Number(cell(2)) && Number(cell(3)) && rows->uiNumRows == plan.items.size() &&
                     item != plan.items.end(),
                 "Incomplete/unsupported item-award receipt");
            const auto id = uint32_t(Number(cell(12), UINT32_MAX));
            Need(Number(cell(4)) == i && Number(cell(5)) == 1 && cell(6) == Binary(plan.payload) &&
                     cell(7) == plan.target.playerSchema && cell(8) == plan.target.accountSchema &&
                     cell(9) == plan.target.playerPostfix && Number(cell(10)) == plan.recipient.account &&
                     cell(11) == plan.recipient.login && id && ids.insert(id).second &&
                     Number(cell(13)) == item->first && Number(cell(14)) == item->second,
                 "Item-award receipt does not match the whole request");
            result.awards.push_back({id, item->first, item->second});
            ++item;
        }
        return result;
    }
    catch (const std::exception &)
    {
        return {ItemAwardsStatus::Invalid, {}};
    }
}
} // namespace auriga::mission::persistence
