#include "MissionResourceEffects.hpp"
#include "MissionPersistenceInternal.hpp"
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <bit>
#include <charconv>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace auriga::mission::persistence
{
namespace
{
using Json = nlohmann::json;
void Need(bool value, const char *why)
{
    if (!value)
        throw std::invalid_argument(why);
}
template <class T> std::string Binary(const T &v)
{
    return {reinterpret_cast<const char *>(v.data()), v.size()};
}
template <class T> std::string Hex(const T &v)
{
    std::string result;
    for (unsigned char c : v)
    {
        result += "0123456789abcdef"[c >> 4];
        result += "0123456789abcdef"[c & 15];
    }
    return result;
}
ContentDigest Hash(const Json &json)
{
    const auto text = json.dump();
    return ContentHash({reinterpret_cast<const uint8_t *>(text.data()), text.size()});
}
Json Identity(const ResourceWriterIdentity &w)
{
    return Json::array({w.owner.character, w.owner.epoch, Hex(w.owner.token), w.coreGeneration,
                        w.sessionGeneration, Hex(w.generation)});
}
void ValidIdentity(const ResourceWriterIdentity &w)
{
    Need(w.owner.character && w.owner.epoch && w.owner.epoch < UINT64_MAX && w.coreGeneration &&
             w.sessionGeneration && w.owner.token != Token{} && w.generation != Token{},
         "Invalid resource writer identity");
}
std::string Name(const std::string &value)
{
    Need(!value.empty() && value.size() <= 64 &&
             value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
                 value.npos,
         "Invalid resource SQL identifier");
    return '`' + value + '`';
}
std::string Window(ResourceWindow window)
{
    switch (window)
    {
    case ResourceWindow::Inventory:
        return "INVENTORY";
    case ResourceWindow::ExtraInventory:
        return "EXTRA_INVENTORY";
    case ResourceWindow::Equipment:
        return "EQUIPMENT";
    case ResourceWindow::Belt:
        return "BELT_INVENTORY";
    case ResourceWindow::Ground:
        return "GROUND";
    default:
        throw std::invalid_argument("Account storage is outside the character resource snapshot");
    }
}
constexpr const char *Scope =
    "('INVENTORY','EXTRA_INVENTORY','EQUIPMENT','BELT_INVENTORY','GROUND')";
std::string ItemRows(const ResourceSnapshot &source)
{
    Json rows = Json::array();
    for (const auto &item : source.items)
    {
        Need(item.category == 0, "SQL adapter requires flat canonical item positions");
        rows.push_back(
            Json::array({item.id, item.owner, item.vnum, item.count, Window(item.window), item.cell}));
    }
    return rows.dump();
}
std::string ItemTable()
{
    return "JSON_TABLE(?, '$[*]' COLUMNS(jid BIGINT UNSIGNED PATH '$[0]',jowner BIGINT UNSIGNED PATH '$[1]',"
           "jvnum BIGINT UNSIGNED PATH '$[2]',jcount BIGINT UNSIGNED PATH '$[3]',"
           "jwindow VARCHAR(32) PATH '$[4]',jpos BIGINT UNSIGNED PATH '$[5]')) j";
}
std::string CostTable()
{
    return "JSON_TABLE(?, '$[*]' COLUMNS(jid BIGINT UNSIGNED PATH '$[0]',jafter BIGINT UNSIGNED PATH "
           "'$[1]')) j";
}
SqlParams ProgressParams(const ResourceSnapshot &s)
{
    const auto &p = s.progress;
    return {SqlParam::Int(s.gold),        SqlParam::UInt(p.level),      SqlParam::UInt(p.step),
            SqlParam::UInt(p.experience), SqlParam::UInt(p.stat),       SqlParam::UInt(p.skill),
            SqlParam::UInt(p.subSkill),   SqlParam::UInt(p.randomHP),   SqlParam::UInt(p.randomSP),
            SqlParam::UInt(p.job),        SqlParam::UInt(p.skillGroup), SqlParam::UInt(s.character)};
}
constexpr const char *ProgressMatch =
    "gold=? AND level=? AND level_step=? AND exp=? AND stat_point=? AND skill_point=? "
    "AND sub_skill_point=? AND random_hp=? AND random_sp=? AND job=? AND skill_group=? AND id=?";
constexpr const char *ProgressSet =
    "gold=?,level=?,level_step=?,exp=?,stat_point=?,skill_point=?,sub_skill_point=?,"
    "random_hp=?,random_sp=?,job=?,skill_group=?";
Plan Invalid(const std::exception &error)
{
    Plan plan;
    plan.errors.push_back({"resource_effects", error.what()});
    return plan;
}
uint64_t Number(std::string_view text)
{
    uint64_t number = 0;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    Need(!text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(),
         "Invalid resource receipt number");
    return number;
}
} // namespace
ResourceEffectsPlan ApplyResourceEffects(const ResourceFence &fence, const PinnedReward &reward,
                                         uint64_t storageRevision, ContentDigest checkpoint, Token operation,
                                         ItemAwardTarget target, ItemAwardRecipient recipient,
                                         const std::map<uint32_t, uint32_t> &caps)
{
    ResourceEffectsPlan result;
    try
    {
        Need(fence.Stage() == ResourceFenceStage::Ready, "Resource producers/SQL lanes are not fenced");
        const auto &r = fence.Resources();
        Need(r.Action() == reward.Action() && r.DefinitionDigest() == reward.DefinitionDigest() &&
                 r.PayloadDigest() == reward.PayloadDigest(),
             "Resource effect pin differs");
        result.writer = fence.Identity();
        ValidIdentity(result.writer);
        result.decision = r.Digest();
        result.sourceRevision = r.Before().revision;
        result.appliedRevision = r.After().revision;
        const auto sourceItems = ItemRows(r.Before());
        result.awards = detail::DraftItemAwards(result.writer.owner, reward, storageRevision, checkpoint,
                                                operation, target, recipient, caps);
        Need(bool(result.awards), "Invalid fenced resource entitlement/recipient");
        result.binding = Hash(Json::array(
            {"MERESULT1", Hex(ActionIdentity(r.Action())), Hex(r.DefinitionDigest()), Hex(r.PayloadDigest()),
             Hex(r.Digest()), target.playerSchema, target.accountSchema, target.playerPostfix,
             recipient.account, recipient.login, result.sourceRevision, result.appliedRevision}));
        auto &plan = result.awards.transaction;
        plan.statements.pop_back(); // Replace only the final operation receipt; retain the entire entitlement
                                    // transaction.
        plan.kind = Operation::ApplyResources;
        plan.request = Hash(Json::array({"MEAPPLY1", Hex(plan.request), Hex(result.binding),
                                         Identity(result.writer), Hex(fence.Challenge())}));
        const auto player = Name(target.playerSchema) + '.' + Name("player" + target.playerPostfix);
        const auto item = Name(target.playerSchema) + '.' + Name("item" + target.playerPostfix);
        // Check the additional canonical table before any DML, including entitlement INSERTs.
        plan.statements.insert(
            plan.statements.begin() + 1,
            {"SELECT 1 WHERE (SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=? AND "
             "TABLE_NAME=? AND ENGINE='InnoDB')=1 "
             "AND NOT EXISTS(SELECT 1 FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA=? AND "
             "EVENT_OBJECT_TABLE=?)",
             {SqlParam::Text(target.playerSchema), SqlParam::Text("item" + target.playerPostfix),
              SqlParam::Text(target.playerSchema), SqlParam::Text("item" + target.playerPostfix)},
             {},
             1});
        const auto id = ActionIdentity(r.Action());
        plan.statements.push_back(
            {"SELECT action_id FROM mission_engine_resource_plans WHERE action_id=? AND decision_digest=? "
             "AND definition_digest=? AND payload_digest=? AND source_revision=? AND BINARY plan_bytes=? FOR "
             "UPDATE",
             {SqlParam::Blob(Binary(id)), SqlParam::Blob(Binary(r.Digest())),
              SqlParam::Blob(Binary(r.DefinitionDigest())), SqlParam::Blob(Binary(r.PayloadDigest())),
              SqlParam::UInt(result.sourceRevision), SqlParam::Blob(Binary(r.Bytes()))},
             {},
             1});
        plan.statements.push_back(
            {"INSERT INTO "
             "mission_engine_resource_heads(character_id,owner_epoch,owner_token,writer_generation,resource_"
             "revision,publication_pending,core_generation,session_generation) "
             "VALUES(?,?,?,?,?,0,?,?) ON DUPLICATE KEY UPDATE character_id=character_id",
             {SqlParam::UInt(plan.character), SqlParam::UInt(plan.ownerEpoch),
              SqlParam::Blob(Binary(result.writer.owner.token)),
              SqlParam::Blob(Binary(result.writer.generation)), SqlParam::UInt(result.sourceRevision),
              SqlParam::UInt(result.writer.coreGeneration), SqlParam::UInt(result.writer.sessionGeneration)},
             {},
             {}});
        plan.statements.push_back({"SELECT character_id FROM mission_engine_resource_heads WHERE "
                                   "character_id=? AND resource_revision=? "
                                   "AND publication_pending=0 AND owner_epoch<=? FOR UPDATE",
                                   {SqlParam::UInt(plan.character), SqlParam::UInt(result.sourceRevision),
                                    SqlParam::UInt(plan.ownerEpoch)},
                                   {},
                                   1});
        plan.statements.push_back({"SELECT id FROM " + player + " WHERE " + ProgressMatch + " FOR UPDATE",
                                   ProgressParams(r.Before()),
                                   {},
                                   1});
        plan.statements.push_back({"SELECT id FROM " + item + " WHERE owner_id=? AND window IN " + Scope +
                                       " ORDER BY id FOR UPDATE",
                                   {SqlParam::UInt(plan.character)},
                                   {},
                                   r.Before().items.size()});
        plan.statements.push_back(
            {"SELECT i.id FROM " + item + " i JOIN " + ItemTable() +
                 " ON i.id=j.jid AND i.owner_id=j.jowner AND i.vnum=j.jvnum AND i.count=j.jcount AND BINARY "
                 "i.window=BINARY j.jwindow AND i.pos=j.jpos "
                 "WHERE i.owner_id=? FOR UPDATE",
             {SqlParam::Blob(sourceItems), SqlParam::UInt(plan.character)},
             {},
             r.Before().items.size()});
        Json costs = Json::array();
        size_t deleted = 0;
        for (const auto &c : r.Consumption())
        {
            costs.push_back(Json::array({c.id, c.after}));
            deleted += !c.after;
        }
        if (!r.Consumption().empty())
        {
            plan.statements.push_back(
                {"UPDATE " + item + " i JOIN " + CostTable() +
                     " ON i.id=j.jid SET i.count=j.jafter WHERE i.owner_id=? AND j.jafter>0",
                 {SqlParam::Blob(costs.dump()), SqlParam::UInt(plan.character)},
                 r.Consumption().size() - deleted,
                 {}});
            plan.statements.push_back({"DELETE i FROM " + item + " i JOIN " + CostTable() +
                                           " ON i.id=j.jid WHERE i.owner_id=? AND j.jafter=0",
                                       {SqlParam::Blob(costs.dump()), SqlParam::UInt(plan.character)},
                                       deleted,
                                       {}});
        }
        if (r.Before().progress != r.After().progress || r.Before().gold != r.After().gold)
            plan.statements.push_back({"UPDATE " + player + " SET " + ProgressSet + " WHERE id=?",
                                       ProgressParams(r.After()),
                                       1,
                                       {}});
        plan.statements.push_back(
            {"INSERT INTO "
             "mission_engine_resource_effects(action_id,decision_digest,binding_digest,source_revision,"
             "applied_revision,writer_generation) VALUES(?,?,?,?,?,?)",
             {SqlParam::Blob(Binary(id)), SqlParam::Blob(Binary(r.Digest())),
              SqlParam::Blob(Binary(result.binding)), SqlParam::UInt(result.sourceRevision),
              SqlParam::UInt(result.appliedRevision), SqlParam::Blob(Binary(result.writer.generation))},
             1,
             {}});
        plan.statements.push_back(
            {"UPDATE mission_engine_resource_heads SET "
             "owner_epoch=?,owner_token=?,writer_generation=?,core_generation=?,session_generation=?,"
             "resource_revision=?,"
             "publication_pending=1,last_action=?,last_decision=? WHERE character_id=? AND "
             "resource_revision=? AND publication_pending=0",
             {SqlParam::UInt(plan.ownerEpoch), SqlParam::Blob(Binary(result.writer.owner.token)),
              SqlParam::Blob(Binary(result.writer.generation)), SqlParam::UInt(result.writer.coreGeneration),
              SqlParam::UInt(result.writer.sessionGeneration), SqlParam::UInt(result.appliedRevision),
              SqlParam::Blob(Binary(id)), SqlParam::Blob(Binary(r.Digest())), SqlParam::UInt(plan.character),
              SqlParam::UInt(result.sourceRevision)},
             1,
             {}});
        detail::FinishResourceOperation(plan);
        return result;
    }
    catch (const std::exception &error)
    {
        result = {};
        result.awards.transaction = Invalid(error);
        return result;
    }
}
SqlTransaction ResolveResourceEffects(const ResourceEffectsPlan &plan)
{
    Need(bool(plan), "Invalid resource effect resolution plan");
    auto queries = ResolveItemAwards(plan.awards);
    queries.push_back(
        {"SELECT e.decision_digest,e.binding_digest,e.source_revision,e.applied_revision,e.writer_generation,"
         "h.resource_revision,h.publication_pending,h.last_action,h.last_decision "
         "FROM mission_engine_resource_effects e LEFT JOIN mission_engine_resource_heads h ON "
         "h.character_id=? WHERE e.action_id=? FOR UPDATE",
         {SqlParam::UInt(plan.awards.action.character),
          SqlParam::Blob(Binary(ActionIdentity(plan.awards.action)))},
         {},
         {}});
    return queries;
}
ResourceEffectsReceipt DecodeResourceEffects(_SQLMsg &message, const ResourceEffectsPlan &plan)
{
    if (!Confirmed(message))
    {
        ResourceEffectsReceipt failed{ReadStatus::SqlError, {}, {}};
        failed.errors.push_back(
            {"resource_effect_sql",
             "Resolution transaction failed: errno=" + std::to_string(message.uiSQLErrno) +
                 ", outcome=" + std::to_string(int(message.transactionOutcome))});
        return failed;
    }
    try
    {
        Need(bool(plan) && message.transactionResultOffsets.size() == 5, "Invalid resource resolution shape");
        const auto awards = DecodeItemAwards(message, plan.awards, 5);
        auto *rows = message.vec_pkResult.at(message.transactionResultOffsets.at(4));
        Need(rows && rows->bPrepared && rows->uiNumRows <= 1 && rows->preparedRowPos == 0 &&
                 rows->preparedRows.size() == rows->uiNumRows &&
                 rows->preparedLengths.size() == rows->uiNumRows,
             "Invalid effect receipt shape");
        if (!rows->uiNumRows)
        {
            Need(awards.status == ItemAwardsStatus::NotIssued || awards.status == ItemAwardsStatus::Cancelled,
                 "Unreceipted partial effect");
            return {ReadStatus::Missing, {}, {}};
        }
        Need(rows->preparedRows[0].size() == 9 && rows->preparedLengths[0].size() == 9,
             "Invalid effect receipt columns");
        auto row = rows->FetchRow();
        auto lengths = rows->FetchLengths();
        auto cell = [&](size_t column)
        {
            Need(row && lengths && row[column], "NULL resource receipt");
            return std::string_view(row[column], lengths[column]);
        };
        Need(cell(0) == Binary(plan.decision) && cell(1) == Binary(plan.binding) &&
                 Number(cell(2)) == plan.sourceRevision && Number(cell(3)) == plan.appliedRevision &&
                 cell(4).size() == 16 && cell(4) != Binary(Token{}),
             "Different/inconsistent resource effect receipt");
        Need(awards.status == ItemAwardsStatus::Issued, "Incomplete resource award mapping");
        const auto revision = Number(cell(5)), pending = Number(cell(6));
        Need(revision >= plan.appliedRevision && revision < UINT64_MAX && pending <= 1,
             "Resource head contradicts effect receipt");
        ResourceEffectsReceipt result{
            ReadStatus::Found,
            ResourceEffectReceipt{ActionIdentity(plan.awards.action), plan.decision, plan.appliedRevision},
            awards.awards};
        result.currentRevision = revision;
        if (revision == plan.appliedRevision)
        {
            Need(cell(7) == Binary(ActionIdentity(plan.awards.action)) && cell(8) == Binary(plan.decision),
                 "Resource head has a different effect at the same revision");
            result.publication =
                pending ? ResourcePublicationStatus::Pending : ResourcePublicationStatus::Published;
        }
        else
            result.publication = ResourcePublicationStatus::Superseded;
        return result;
    }
    catch (const std::exception &error)
    {
        ResourceEffectsReceipt rejected{ReadStatus::Invalid, {}, {}};
        rejected.errors.push_back({"resource_effect_receipt", error.what()});
        return rejected;
    }
}
Plan PublishResourceEffects(const ResourceFence &fence, const ResourceEffectsPlan &effects, Token operation)
{
    try
    {
        Need(fence.Stage() == ResourceFenceStage::Rebased && fence.Vitals(),
             "Resource postimage is not rebased");
        const auto &w = fence.Identity();
        ValidIdentity(w);
        const auto &r = fence.Resources();
        const auto &a = effects.awards;
        Need(
            bool(effects) && a.action == r.Action() && effects.decision == r.Digest() &&
                a.definition == r.DefinitionDigest() && a.payload == r.PayloadDigest() &&
                effects.sourceRevision == r.Before().revision &&
                effects.appliedRevision == r.After().revision &&
                effects.binding ==
                    Hash(Json::array({"MERESULT1", Hex(ActionIdentity(r.Action())), Hex(r.DefinitionDigest()),
                                      Hex(r.PayloadDigest()), Hex(r.Digest()), a.target.playerSchema,
                                      a.target.accountSchema, a.target.playerPostfix, a.recipient.account,
                                      a.recipient.login, effects.sourceRevision, effects.appliedRevision})),
            "Resource publication binding differs");
        const auto &v = *fence.Vitals();
        auto plan =
            detail::ResourceOperation(w.owner, Operation::PublishResources, operation,
                                      Hash(Json::array({"MEPUBLISH1", Identity(w), Hex(effects.binding),
                                                        r.After().revision, v.hp, v.sp, v.stamina})));
        plan.statements.push_back(
            {"SELECT action_id FROM mission_engine_resource_effects WHERE action_id=? AND decision_digest=? "
             "AND applied_revision=? AND binding_digest=? FOR UPDATE",
             {SqlParam::Blob(Binary(ActionIdentity(r.Action()))), SqlParam::Blob(Binary(r.Digest())),
              SqlParam::UInt(r.After().revision), SqlParam::Blob(Binary(effects.binding))},
             {},
             1});
        const auto player = Name(a.target.playerSchema) + '.' + Name("player" + a.target.playerPostfix);
        plan.statements.push_back(
            {"SELECT 1 WHERE DATABASE()<>? AND (SELECT COUNT(*) FROM information_schema.TABLES WHERE "
             "TABLE_SCHEMA=? AND TABLE_NAME=? AND ENGINE='InnoDB')=1 "
             "AND NOT EXISTS(SELECT 1 FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA=? AND "
             "EVENT_OBJECT_TABLE=?)",
             {SqlParam::Text(a.target.playerSchema), SqlParam::Text(a.target.playerSchema),
              SqlParam::Text("player" + a.target.playerPostfix), SqlParam::Text(a.target.playerSchema),
              SqlParam::Text("player" + a.target.playerPostfix)},
             {},
             1});
        plan.statements.push_back({"SELECT id FROM " + player + " WHERE " + ProgressMatch + " FOR UPDATE",
                                   ProgressParams(r.After()),
                                   {},
                                   1});
        if (r.RefillVitals())
            plan.statements.push_back({"UPDATE " + player + " SET hp=?,mp=?,stamina=? WHERE id=?",
                                       {SqlParam::UInt(v.hp), SqlParam::UInt(v.sp), SqlParam::UInt(v.stamina),
                                        SqlParam::UInt(w.owner.character)},
                                       {},
                                       {}});
        else
            plan.statements.push_back(
                {"SELECT id FROM " + player + " WHERE hp=? AND mp=? AND stamina=? AND id=? FOR UPDATE",
                 {SqlParam::UInt(v.hp), SqlParam::UInt(v.sp), SqlParam::UInt(v.stamina),
                  SqlParam::UInt(w.owner.character)},
                 {},
                 1});
        plan.statements.push_back(
            {"UPDATE mission_engine_resource_heads SET "
             "publication_pending=0,owner_epoch=?,owner_token=?,writer_generation=?,core_generation=?,"
             "session_generation=? "
             "WHERE character_id=? AND resource_revision=? AND last_action=? AND last_decision=? AND "
             "publication_pending=1 AND owner_epoch<=?",
             {SqlParam::UInt(w.owner.epoch), SqlParam::Blob(Binary(w.owner.token)),
              SqlParam::Blob(Binary(w.generation)), SqlParam::UInt(w.coreGeneration),
              SqlParam::UInt(w.sessionGeneration), SqlParam::UInt(w.owner.character),
              SqlParam::UInt(r.After().revision), SqlParam::Blob(Binary(ActionIdentity(r.Action()))),
              SqlParam::Blob(Binary(r.Digest())), SqlParam::UInt(w.owner.epoch)},
             1,
             {}});
        detail::FinishResourceOperation(plan);
        return plan;
    }
    catch (const std::exception &error)
    {
        return Invalid(error);
    }
}
Plan GuardResourceWrites(ResourceWriterIdentity writer, uint64_t revision, Token operation,
                         ItemAwardTarget target, SqlTransaction writes)
{
    try
    {
        ValidIdentity(writer);
        Need(revision && revision < UINT64_MAX - 1 && !writes.empty() && writes.size() <= 128,
             "Invalid guarded resource write batch");
        const auto player = Name(target.playerSchema) + '.' + Name("player" + target.playerPostfix);
        const auto item = Name(target.playerSchema) + '.' + Name("item" + target.playerPostfix);
        Json body = Json::array();
        size_t bytes = 0, parameters = 0;
        for (const auto &step : writes)
        {
            Need((step.query.starts_with("UPDATE " + player + " ") ||
                  step.query.starts_with("UPDATE " + item + " ") ||
                  step.query.starts_with("INSERT INTO " + item + " ") ||
                  step.query.starts_with("DELETE FROM " + item + " ") ||
                  step.query.starts_with("REPLACE INTO " + item + " ")) &&
                     step.query.find(';') == step.query.npos,
                 "Only fixed single-statement native cache DML is allowed");
            bytes += step.query.size();
            parameters += step.params.size();
            Json params = Json::array();
            Need(bytes <= SqlTransactionLimits::TotalBytes && parameters <= SqlTransactionLimits::Parameters,
                 "Resource write request too large");
            for (const auto &p : step.params)
            {
                bytes += p.bytes.size();
                Need(bytes <= SqlTransactionLimits::TotalBytes &&
                         parameters <= SqlTransactionLimits::Parameters,
                     "Resource write request too large");
                params.push_back(Json::array(
                    {uint8_t(p.type), p.i64, p.u64, std::bit_cast<uint64_t>(p.f64), Hex(p.bytes)}));
            }
            body.push_back(Json::array({step.query, params,
                                        step.expectedAffectedRows ? Json(*step.expectedAffectedRows) : Json(),
                                        step.expectedResultRows ? Json(*step.expectedResultRows) : Json()}));
        }
        auto plan =
            detail::ResourceOperation(writer.owner, Operation::ResourceWrite, operation,
                                      Hash(Json::array({"MECACHE1", Identity(writer), revision, body})));
        plan.statements.push_back(
            {"SELECT 1 WHERE DATABASE()<>? AND (SELECT COUNT(*) FROM information_schema.TABLES WHERE "
             "TABLE_SCHEMA=? AND TABLE_NAME IN (?,?) AND ENGINE='InnoDB')=2 "
             "AND NOT EXISTS(SELECT 1 FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA=? AND "
             "EVENT_OBJECT_TABLE IN (?,?))",
             {SqlParam::Text(target.playerSchema), SqlParam::Text(target.playerSchema),
              SqlParam::Text("player" + target.playerPostfix), SqlParam::Text("item" + target.playerPostfix),
              SqlParam::Text(target.playerSchema), SqlParam::Text("player" + target.playerPostfix),
              SqlParam::Text("item" + target.playerPostfix)},
             {},
             1});
        plan.statements.push_back(
            {"SELECT character_id FROM mission_engine_resource_heads WHERE character_id=? AND owner_epoch=? "
             "AND owner_token=? "
             "AND writer_generation=? AND core_generation=? AND session_generation=? AND resource_revision=? "
             "AND publication_pending=0 FOR UPDATE",
             {SqlParam::UInt(writer.owner.character), SqlParam::UInt(writer.owner.epoch),
              SqlParam::Blob(Binary(writer.owner.token)), SqlParam::Blob(Binary(writer.generation)),
              SqlParam::UInt(writer.coreGeneration), SqlParam::UInt(writer.sessionGeneration),
              SqlParam::UInt(revision)},
             {},
             1});
        plan.statements.insert(plan.statements.end(), std::make_move_iterator(writes.begin()),
                               std::make_move_iterator(writes.end()));
        plan.statements.push_back(
            {"UPDATE mission_engine_resource_heads SET resource_revision=resource_revision+1 WHERE "
             "character_id=? AND resource_revision=?",
             {SqlParam::UInt(writer.owner.character), SqlParam::UInt(revision)},
             1,
             {}});
        detail::FinishResourceOperation(plan);
        return plan;
    }
    catch (const std::exception &error)
    {
        return Invalid(error);
    }
}
} // namespace auriga::mission::persistence
