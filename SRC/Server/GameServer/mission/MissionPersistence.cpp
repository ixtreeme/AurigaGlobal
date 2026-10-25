#include "MissionPersistence.hpp"
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <charconv>
#include <openssl/rand.h>
#include <stdexcept>

namespace auriga::mission::persistence
{
namespace
{
using Message = _SQLMsg;
bool Nonzero(const Token &token)
{
    return std::any_of(token.begin(), token.end(), [](uint8_t b) { return b != 0; });
}
void Need(bool condition, const char *why)
{
    if (!condition)
        throw std::invalid_argument(why);
}
void CheckOwner(Owner owner)
{
    Need(owner.character && owner.epoch && owner.epoch < UINT64_MAX && Nonzero(owner.token),
         "Invalid persistence owner");
}
template <typename T> std::string Bytes(const T &bytes)
{
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}
std::string Hex(const ContentDigest &digest)
{
    std::string value;
    value.reserve(64);
    for (uint8_t b : digest)
    {
        value.push_back("0123456789abcdef"[b >> 4]);
        value.push_back("0123456789abcdef"[b & 15]);
    }
    return value;
}
struct Request
{
    std::vector<uint8_t> bytes;
    void Number(uint64_t value, size_t length)
    {
        for (size_t i = 0; i < length; ++i)
            bytes.push_back(uint8_t(value >> (8 * i)));
    }
    template <typename T> void Raw(const T &value)
    {
        bytes.insert(bytes.end(), value.begin(), value.end());
    }
};
ContentDigest RequestHash(Operation kind, CharacterId character, uint64_t epoch, Token owner, Token operation,
                          uint64_t revision, ContentDigest checkpoint = {})
{
    Request request;
    constexpr std::array<uint8_t, 8> domain{'M', 'E', 'O', 'P', 1, 0, 0, 0};
    request.Raw(domain);
    request.Number(uint8_t(kind), 1);
    request.Number(character, 4);
    request.Number(epoch, 8);
    request.Raw(owner);
    request.Raw(operation);
    request.Number(revision, 8);
    request.Raw(checkpoint);
    return ContentHash(request.bytes);
}
SqlTransaction Preconditions()
{
    // Version/engine/FK checks fail closed. A hand-created lookalike or a disabled
    // constraint session must not silently weaken archive/checkpoint references.
    return {{"SELECT schema_version FROM mission_engine_schema WHERE singleton=1 AND schema_version=1 "
             "AND @@foreign_key_checks=1 AND @@check_constraint_checks=1 "
             "AND (SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() "
             "AND ENGINE='InnoDB' AND TABLE_NAME IN ('mission_engine_schema','mission_engine_packages',"
             "'mission_engine_definitions','mission_engine_characters','mission_engine_checkpoint_refs',"
             "'mission_engine_operations'))=6 "
             "AND (SELECT COUNT(*) FROM information_schema.REFERENTIAL_CONSTRAINTS WHERE "
             "CONSTRAINT_SCHEMA=DATABASE() "
             "AND CONSTRAINT_NAME IN ('me_definition_package','me_ref_character','me_ref_definition',"
             "'me_operation_character_fk'))=4",
             {},
             {},
             1}};
}
void Receipt(Plan &plan)
{
    plan.statements.push_back(
        {"INSERT INTO mission_engine_operations(operation_id,character_id,operation_kind,request_digest,"
         "owner_epoch,storage_revision) SELECT ?,character_id,?,?,owner_epoch,storage_revision "
         "FROM mission_engine_characters WHERE character_id=? AND owner_epoch=?",
         {SqlParam::Blob(Bytes(plan.operation)), SqlParam::UInt(uint8_t(plan.kind)),
          SqlParam::Blob(Bytes(plan.request)), SqlParam::UInt(plan.character),
          SqlParam::UInt(plan.ownerEpoch)},
         1,
         {}});
}
Plan Invalid(const char *where, const std::exception &error)
{
    Plan plan;
    plan.errors.push_back({where, error.what()});
    return plan;
}
struct Row
{
    MYSQL_ROW cells = nullptr;
    unsigned long *lengths = nullptr;
    bool Present(size_t column) const
    {
        return cells[column] != nullptr;
    }
    std::string_view Text(size_t column) const
    {
        Need(Present(column), "Unexpected SQL NULL");
        return {cells[column], lengths[column]};
    }
    uint64_t UInt(size_t column, uint64_t maximum = UINT64_MAX) const
    {
        auto text = Text(column);
        uint64_t value = 0;
        auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        Need(result.ec == std::errc() && result.ptr == text.data() + text.size() && !text.empty() &&
                 value <= maximum,
             "Invalid SQL unsigned value");
        return value;
    }
    template <size_t N> std::array<uint8_t, N> Binary(size_t column) const
    {
        auto text = Text(column);
        Need(text.size() == N, "Invalid SQL binary length");
        std::array<uint8_t, N> value{};
        std::copy(text.begin(), text.end(), reinterpret_cast<char *>(value.data()));
        return value;
    }
};
Row Read(SQLResult *result, size_t fields)
{
    Need(result && result->bPrepared && result->uiNumRows <= 1 &&
             result->preparedRows.size() == result->uiNumRows,
         "Unexpected SQL result shape");
    if (!result->uiNumRows)
        return {};
    Need(result->preparedRows.front().size() == fields && result->preparedLengths.size() == 1 &&
             result->preparedLengths.front().size() == fields && result->preparedRowPos == 0,
         "Unexpected SQL columns/cursor");
    auto row = result->FetchRow();
    auto lengths = result->FetchLengths();
    Need(row && lengths, "Missing SQL row/lengths");
    return {row, lengths};
}
Row ReadSingle(Message &message, size_t fields)
{
    Need(!message.uiSQLErrno && !message.bTransaction && message.vec_pkResult.size() == 1, "Failed SQL read");
    return Read(message.Get(), fields);
}
} // namespace

Token NewToken()
{
    Token token{};
    do
    {
        if (RAND_bytes(token.data(), int(token.size())) != 1)
            throw std::runtime_error("Mission token generation failed");
    } while (!Nonzero(token));
    return token;
}
Plan ArchivePackage(std::span<const uint8_t> bytes)
{
    try
    {
        const auto loaded = LoadPackage(bytes);
        Need(bool(loaded), "Invalid archived mission package");
        const auto &package = *loaded.package;
        const auto &manifest = package.Manifest();
        Plan plan;
        plan.statements = Preconditions();
        plan.statements.push_back(
            {"INSERT INTO mission_engine_packages(package_digest,package_id,content_version,package_bytes) "
             "VALUES(?,?,?,?) "
             "ON DUPLICATE KEY UPDATE package_digest=package_digest",
             {SqlParam::Blob(Bytes(package.Digest())), SqlParam::Blob(manifest.id),
              SqlParam::UInt(manifest.version), SqlParam::Blob(Bytes(bytes))},
             {},
             {}});
        plan.statements.push_back(
            {"SELECT package_digest FROM mission_engine_packages WHERE package_digest=? AND package_id=? "
             "AND content_version=? AND OCTET_LENGTH(package_bytes)=? AND SHA2(package_bytes,256)=? FOR "
             "UPDATE",
             {SqlParam::Blob(Bytes(package.Digest())), SqlParam::Blob(manifest.id),
              SqlParam::UInt(manifest.version), SqlParam::UInt(bytes.size()),
              SqlParam::Text(Hex(ContentHash(bytes)))},
             {},
             1});
        for (const auto &graph : package.Graphs())
        {
            const auto &source = graph->Source();
            const auto fingerprint = FingerprintDefinition(*graph);
            plan.statements.push_back(
                {"INSERT INTO "
                 "mission_engine_definitions(mission_id,definition_version,definition_digest,package_digest) "
                 "VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE mission_id=mission_id",
                 {SqlParam::Blob(source.id), SqlParam::UInt(source.version),
                  SqlParam::Blob(Bytes(fingerprint)), SqlParam::Blob(Bytes(package.Digest()))},
                 {},
                 {}});
            plan.statements.push_back({"SELECT package_digest FROM mission_engine_definitions WHERE "
                                       "mission_id=? AND definition_version=? "
                                       "AND definition_digest=? FOR UPDATE",
                                       {SqlParam::Blob(source.id), SqlParam::UInt(source.version),
                                        SqlParam::Blob(Bytes(fingerprint))},
                                       {},
                                       1});
        }
        return plan;
    }
    catch (const std::exception &error)
    {
        return Invalid("archive", error);
    }
}
Plan Claim(CharacterId character, uint64_t expectedEpoch, Token newOwner, Token operation)
{
    try
    {
        Need(character && expectedEpoch < UINT64_MAX - 1 && Nonzero(newOwner) && Nonzero(operation),
             "Invalid claim identity/epoch");
        Plan plan;
        plan.character = character;
        plan.operation = operation;
        plan.kind = Operation::Claim;
        plan.ownerEpoch = expectedEpoch + 1;
        plan.request = RequestHash(plan.kind, character, expectedEpoch, newOwner, operation, 0);
        plan.statements = Preconditions();
        plan.statements.push_back({"INSERT INTO mission_engine_characters(character_id) VALUES(?) "
                                   "ON DUPLICATE KEY UPDATE character_id=character_id",
                                   {SqlParam::UInt(character)},
                                   {},
                                   {}});
        plan.statements.push_back(
            {"UPDATE mission_engine_characters SET owner_epoch=owner_epoch+1,owner_token=? "
             "WHERE character_id=? AND owner_epoch=? AND owner_epoch<18446744073709551614",
             {SqlParam::Blob(Bytes(newOwner)), SqlParam::UInt(character), SqlParam::UInt(expectedEpoch)},
             1,
             {}});
        Receipt(plan);
        return plan;
    }
    catch (const std::exception &error)
    {
        return Invalid("claim", error);
    }
}
Plan Save(const MissionRuntime::Checkpoint &checkpoint, const DefinitionRegistry &registry, Owner owner,
          uint64_t expectedStorageRevision, Token operation)
{
    try
    {
        CheckOwner(owner);
        Need(checkpoint.character == owner.character && Nonzero(operation) &&
                 expectedStorageRevision < UINT64_MAX - 1,
             "Invalid save identity/revision");
        auto encoded = EncodeCheckpoint(checkpoint, registry);
        Need(bool(encoded), "Invalid saved checkpoint");
        Plan plan;
        plan.character = owner.character;
        plan.ownerEpoch = owner.epoch;
        plan.operation = operation;
        plan.kind = Operation::Save;
        plan.storageRevision = expectedStorageRevision + 1;
        plan.request = RequestHash(plan.kind, owner.character, owner.epoch, owner.token, operation,
                                   expectedStorageRevision, encoded.metadata.digest);
        plan.statements = Preconditions();
        plan.statements.push_back(
            {"UPDATE mission_engine_characters SET storage_revision=storage_revision+1,runtime_revision=?,"
             "checkpoint_time=?,checkpoint_digest=?,checkpoint_bytes=? WHERE character_id=? AND "
             "owner_epoch=? "
             "AND owner_token=? AND storage_revision=? AND storage_revision<18446744073709551614 "
             "AND checkpoint_time<=? AND (checkpoint_bytes IS NULL OR runtime_revision<?)",
             {SqlParam::UInt(checkpoint.revision), SqlParam::UInt(uint64_t(checkpoint.now)),
              SqlParam::Blob(Bytes(encoded.metadata.digest)), SqlParam::Blob(Bytes(encoded.bytes)),
              SqlParam::UInt(owner.character), SqlParam::UInt(owner.epoch),
              SqlParam::Blob(Bytes(owner.token)), SqlParam::UInt(expectedStorageRevision),
              SqlParam::UInt(uint64_t(checkpoint.now)), SqlParam::UInt(checkpoint.revision)},
             1,
             {}});
        plan.statements.push_back({"DELETE FROM mission_engine_checkpoint_refs WHERE character_id=?",
                                   {SqlParam::UInt(owner.character)},
                                   {},
                                   {}});
        for (const auto &reference : encoded.metadata.definitions)
            plan.statements.push_back(
                {"INSERT INTO mission_engine_checkpoint_refs(character_id,mission_id,definition_version,"
                 "definition_digest) VALUES(?,?,?,?)",
                 {SqlParam::UInt(owner.character), SqlParam::Blob(reference.mission),
                  SqlParam::UInt(reference.version), SqlParam::Blob(Bytes(reference.fingerprint))},
                 1,
                 {}});
        Receipt(plan);
        return plan;
    }
    catch (const std::exception &error)
    {
        return Invalid("save", error);
    }
}
Plan Release(Owner owner, Token operation)
{
    try
    {
        CheckOwner(owner);
        Need(owner.epoch < UINT64_MAX - 1 && Nonzero(operation), "Invalid release epoch/token");
        Plan plan;
        plan.character = owner.character;
        plan.operation = operation;
        plan.kind = Operation::Release;
        plan.ownerEpoch = owner.epoch + 1;
        plan.request = RequestHash(plan.kind, owner.character, owner.epoch, owner.token, operation, 0);
        plan.statements = Preconditions();
        plan.statements.push_back(
            {"UPDATE mission_engine_characters SET owner_epoch=owner_epoch+1,owner_token=NULL "
             "WHERE character_id=? AND owner_epoch=? AND owner_token=? AND owner_epoch<18446744073709551614",
             {SqlParam::UInt(owner.character), SqlParam::UInt(owner.epoch),
              SqlParam::Blob(Bytes(owner.token))},
             1,
             {}});
        Receipt(plan);
        return plan;
    }
    catch (const std::exception &error)
    {
        return Invalid("release", error);
    }
}
ReadCommand ReadHead(CharacterId character)
{
    Need(character, "Invalid character head identity");
    return {"SELECT character_id,owner_epoch,storage_revision FROM mission_engine_characters WHERE "
            "character_id=?",
            {SqlParam::UInt(character)}};
}
HeadResult DecodeHead(Message &message, CharacterId character)
{
    if (message.uiSQLErrno)
        return {ReadStatus::SqlError, {}};
    try
    {
        Need(character, "Invalid head identity");
        const auto row = ReadSingle(message, 3);
        if (!row.cells)
            return {ReadStatus::Missing, {character, 0, 0}};
        Head head{uint32_t(row.UInt(0, UINT32_MAX)), row.UInt(1), row.UInt(2)};
        Need(head.character == character, "Wrong character head");
        return {ReadStatus::Found, head};
    }
    catch (const std::exception &)
    {
        return {ReadStatus::Invalid, {}};
    }
}
ReadCommand ReadSnapshot(Owner owner)
{
    CheckOwner(owner);
    return {
        "SELECT character_id,owner_epoch,owner_token,storage_revision,runtime_revision,checkpoint_time,"
        "checkpoint_digest,CASE WHEN checkpoint_bytes IS NULL OR OCTET_LENGTH(checkpoint_bytes) BETWEEN 64 AND ? "
        "THEN checkpoint_bytes ELSE X'' END FROM mission_engine_characters WHERE character_id=? AND "
        "owner_epoch=? AND owner_token=?",
        {SqlParam::UInt(CheckpointBytes), SqlParam::UInt(owner.character), SqlParam::UInt(owner.epoch),
         SqlParam::Blob(Bytes(owner.token))}};
}
SnapshotResult DecodeSnapshot(Message &message, Owner owner)
{
    if (message.uiSQLErrno)
        return {ReadStatus::SqlError, {}, {}};
    try
    {
        CheckOwner(owner);
        const auto row = ReadSingle(message, 8);
        if (!row.cells)
            return {ReadStatus::Missing, {}, {}};
        Snapshot snapshot;
        snapshot.owner = {uint32_t(row.UInt(0, UINT32_MAX)), row.UInt(1), row.Binary<16>(2)};
        Need(snapshot.owner == owner, "Snapshot owner differs");
        snapshot.storageRevision = row.UInt(3, UINT64_MAX - 1);
        const auto revision = row.UInt(4, UINT64_MAX - 1), now = row.UInt(5, INT64_MAX);
        if (!row.Present(7))
        {
            Need(!row.Present(6) && !snapshot.storageRevision && !revision && !now, "Invalid empty snapshot");
        }
        else
        {
            const auto data = row.Text(7);
            Need(data.size() <= CheckpointBytes, "SQL checkpoint size limit");
            snapshot.bytes.assign(reinterpret_cast<const uint8_t *>(data.data()),
                                  reinterpret_cast<const uint8_t *>(data.data()) + data.size());
            snapshot.metadata = ScanCheckpoint(snapshot.bytes);
            Need(bool(snapshot.metadata) && snapshot.storageRevision &&
                     snapshot.metadata.character == owner.character &&
                     snapshot.metadata.revision == revision && uint64_t(snapshot.metadata.now) == now &&
                     snapshot.metadata.digest == row.Binary<32>(6),
                 "SQL checkpoint metadata/content differs");
        }
        return {ReadStatus::Found, std::move(snapshot), {}};
    }
    catch (const std::exception &error)
    {
        return {ReadStatus::Invalid, {}, {{"snapshot", error.what()}}};
    }
}
ReadCommand ReadDefinitionArchive(const DefinitionReference &reference)
{
    Need(IsIdentifier(reference.mission) && reference.version, "Invalid definition reference");
    return {
        "SELECT d.mission_id,d.definition_version,d.definition_digest,p.package_digest,p.package_id,"
        "p.content_version,CASE WHEN OCTET_LENGTH(p.package_bytes) BETWEEN 64 AND ? "
        "THEN p.package_bytes ELSE X'' END FROM mission_engine_definitions d JOIN mission_engine_packages p "
        "ON p.package_digest=d.package_digest WHERE d.mission_id=? AND d.definition_version=? AND "
        "d.definition_digest=?",
        {SqlParam::UInt(PackageLimits::Bytes), SqlParam::Blob(reference.mission), SqlParam::UInt(reference.version),
         SqlParam::Blob(Bytes(reference.fingerprint))}};
}
PackageLoad DecodeDefinitionArchive(Message &message, const DefinitionReference &reference)
{
    try
    {
        const auto row = ReadSingle(message, 7);
        Need(row.cells, "Missing archived definition");
        Need(row.Text(0) == reference.mission && row.UInt(1, UINT32_MAX) == reference.version &&
                 row.Binary<32>(2) == reference.fingerprint,
             "Archived definition reference differs");
        const auto data = row.Text(6);
        Need(data.size() <= PackageLimits::Bytes, "SQL archive size limit");
        auto loaded = LoadPackage({reinterpret_cast<const uint8_t *>(data.data()), data.size()});
        Need(bool(loaded), "Corrupt archived package");
        const auto &package = *loaded.package;
        Need(package.Digest() == row.Binary<32>(3) && package.Manifest().id == row.Text(4) &&
                 package.Manifest().version == row.UInt(5, UINT32_MAX),
             "Archived package identity differs");
        const auto graph = std::find_if(
            package.Graphs().begin(), package.Graphs().end(), [&](const auto &g)
            { return g->Source().id == reference.mission && g->Source().version == reference.version; });
        Need(graph != package.Graphs().end() && FingerprintDefinition(**graph) == reference.fingerprint,
             "Archived graph fingerprint differs");
        return loaded;
    }
    catch (const std::exception &error)
    {
        return {nullptr, {{"archive", error.what()}}};
    }
}
SqlTransaction Resolve(const Plan &plan)
{
    Need(bool(plan) && plan.character && Nonzero(plan.operation),
         "Cannot resolve invalid/non-operation plan");
    auto statements = Preconditions();
    statements.push_back(
        {"SELECT character_id FROM mission_engine_characters WHERE character_id=? FOR UPDATE",
         {SqlParam::UInt(plan.character)},
         {},
         {}});
    statements.push_back({"SELECT character_id,operation_kind,request_digest,owner_epoch,storage_revision "
                          "FROM mission_engine_operations WHERE operation_id=? FOR UPDATE",
                          {SqlParam::Blob(Bytes(plan.operation))},
                          {},
                          {}});
    return statements;
}
Resolved DecodeResolution(Message &message, const Plan &plan)
{
    if (!Confirmed(message))
        return {Resolution::SqlError, 0, 0};
    try
    {
        Need(bool(plan) && plan.character && Nonzero(plan.operation) &&
                 message.transactionResultOffsets.size() == 3,
             "Invalid resolution plan/result");
        const auto offset = message.transactionResultOffsets[2];
        Need(offset < message.vec_pkResult.size(), "Missing receipt result");
        const auto row = Read(message.vec_pkResult[offset], 5);
        if (!row.cells)
            return {Resolution::Absent, 0, 0};
        const auto epoch = row.UInt(3), revision = row.UInt(4);
        if (row.UInt(0, UINT32_MAX) != plan.character || row.UInt(1) != uint8_t(plan.kind) ||
            row.Binary<32>(2) != plan.request || epoch != plan.ownerEpoch ||
            (plan.storageRevision && revision != *plan.storageRevision))
            return {Resolution::TokenConflict, 0, 0};
        return {Resolution::Committed, epoch, revision};
    }
    catch (const std::exception &)
    {
        return {Resolution::Invalid, 0, 0};
    }
}
bool Confirmed(const Message &message)
{
    return message.bTransaction && !message.uiSQLErrno &&
           message.transactionOutcome == SqlTransactionOutcome::Committed;
}
} // namespace auriga::mission::persistence
