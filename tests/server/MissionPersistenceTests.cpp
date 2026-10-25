#include "../../SRC/Server/GameServer/mission/MissionPersistence.hpp"
#include "MissionPersistenceFixture.hpp"
#include <Core/Logging.hpp>
#include <SQL/AsyncSQL.h>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <latch>
#include <memory>
#include <source_location>
#include <sstream>
#include <thread>

using namespace auriga::mission;
using namespace mission_fixture;
namespace db = auriga::mission::persistence;
namespace
{
using Message = std::unique_ptr<SQLMsg>;
unsigned int port = 0;
std::string database;
size_t checks = 0;
void Check(bool condition, std::string_view why,
           const std::source_location line = std::source_location::current())
{
    ++checks;
    if (!condition)
        throw std::runtime_error(std::string(why) + " at " + std::to_string(line.line()));
}
db::Token Token(uint8_t value)
{
    db::Token token{};
    token[0] = value;
    token[15] = uint8_t(value ^ 0xa5);
    return token;
}
template <typename T> std::string Binary(const T &value)
{
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}
void Connect(CAsyncSQL &sql, bool direct = true, unsigned int target = 0)
{
    sql.SetTimeouts(3);
    sql.SetRetryConnectionErrors(false);
    Check(sql.Setup("127.0.0.1", "root", "", database.c_str(), "utf8mb4", direct, target ? target : port),
          "isolated store connection failed");
    if (!direct)
        Check(sql.WaitForConnection(std::chrono::seconds(5)), "store worker not ready");
}
Message Query(CAsyncSQL &sql, const db::ReadCommand &command)
{
    Message result(sql.DirectQueryPrepared(command.query.c_str(), command.params));
    Check(!result->uiSQLErrno, "prepared store read failed");
    return result;
}
uint64_t Number(CAsyncSQL &sql, const char *query, SqlParams params = {})
{
    auto result = Query(sql, {query, std::move(params)});
    auto row = result->Get()->FetchRow();
    Check(row && row[0], "scalar missing");
    return std::stoull(row[0]);
}
void Text(CAsyncSQL &sql, const std::string &query)
{
    Message result(sql.DirectQuery(query.c_str()));
    if (result->uiSQLErrno)
        std::cerr << "setup SQL errno=" << result->uiSQLErrno << '\n';
    Check(!result->uiSQLErrno, "isolated setup statement failed");
}
Message Execute(CAsyncSQL &sql, const db::Plan &plan, bool success = true)
{
    Check(bool(plan), "invalid persistence plan");
    Message result(sql.DirectTransactionPrepared(plan.statements));
    Check(db::Confirmed(*result) == success, "unexpected persistence outcome");
    return result;
}
db::Resolved Resolve(CAsyncSQL &sql, const db::Plan &plan)
{
    Message result(sql.DirectTransactionPrepared(db::Resolve(plan)));
    auto resolved = db::DecodeResolution(*result, plan);
    Check(resolved.status != db::Resolution::SqlError && resolved.status != db::Resolution::Invalid,
          "resolution failed");
    return resolved;
}
db::SnapshotResult Snapshot(CAsyncSQL &sql, db::Owner owner)
{
    auto result = Query(sql, db::ReadSnapshot(owner));
    return db::DecodeSnapshot(*result, owner);
}
db::Owner Claim(CAsyncSQL &sql, uint32_t character, uint64_t epoch, db::Token token, db::Token operation)
{
    auto plan = db::Claim(character, epoch, token, operation);
    Execute(sql, plan);
    Check(Resolve(sql, plan).status == db::Resolution::Committed, "claim receipt not durable");
    return {character, epoch + 1, token};
}
MissionRuntime::Checkpoint Checkpoint(const DefinitionRegistry &registry, uint32_t character)
{
    CharacterSession owner{character, 17};
    MissionRuntime runtime(registry, owner, 1000);
    Progress(runtime, owner);
    return runtime.Save();
}
void Migration(CAsyncSQL &sql, const char *path)
{
    std::ifstream file(path, std::ios::binary);
    Check(bool(file), "migration file missing");
    std::string source, line;
    while (std::getline(file, line))
        if (!line.starts_with("--"))
            source += line + '\n';
    // This fixture contains no procedures or quoted semicolons; execute the
    // actual checked-in DDL individually so later statement errors cannot hide.
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        std::istringstream statements(source);
        std::string statement;
        while (std::getline(statements, statement, ';'))
            if (statement.find_first_not_of(" \r\n\t") != statement.npos)
                Text(sql, statement);
    }
    Check(Number(sql, "SELECT marker FROM account_mission_pass WHERE account_id=1") == 123,
          "additive migration changed seasonal pass sentinel");
    Check(Number(sql, "SELECT marker FROM quest WHERE id=1") == 456,
          "migration changed legacy quest sentinel");
}
void Archive(CAsyncSQL &sql)
{
    auto v1 = Package(1, 1), v2 = Package(2, 2), sameGraphs = Package(3, 1);
    Check(bool(v1) && bool(v2) && bool(sameGraphs), "archive fixture failed compile");
    Execute(sql, db::ArchivePackage(v1.bytes));
    Execute(sql, db::ArchivePackage(v1.bytes));
    Execute(sql, db::ArchivePackage(v2.bytes));
    Execute(sql, db::ArchivePackage(sameGraphs.bytes));
    Check(Number(sql, "SELECT COUNT(*) FROM mission_engine_packages") == 3 &&
              Number(sql, "SELECT COUNT(*) FROM mission_engine_definitions") == 4,
          "archive idempotence/version identities changed");
    auto packageConflict = Package(2, 1);
    auto conflict = Execute(sql, db::ArchivePackage(packageConflict.bytes), false);
    Check(conflict->transactionOutcome == SqlTransactionOutcome::Conflict,
          "package version content conflict not rejected");
    auto changed = Source(4, 1);
    changed.missions[0].nodes.back().reward = "persist.altered";
    auto changedBytes = CompilePackage(changed);
    Check(bool(changedBytes), "changed archive fixture invalid");
    conflict = Execute(sql, db::ArchivePackage(changedBytes.bytes), false);
    Check(conflict->transactionOutcome == SqlTransactionOutcome::Conflict &&
              Number(sql, "SELECT COUNT(*) FROM mission_engine_packages") == 3,
          "conflicting graph partly archived its package");
    auto corrupt = v1.bytes;
    corrupt.back() ^= 1;
    Check(!db::ArchivePackage(corrupt), "invalid package generated SQL");
}
void WarmSave(CAsyncSQL &sql, const DefinitionRegistry &registry)
{
    auto headRead = Query(sql, db::ReadHead(42));
    auto head = db::DecodeHead(*headRead, 42);
    Check(head.status == db::ReadStatus::Missing && head.head.epoch == 0, "new character head not missing");
    auto owner = Claim(sql, 42, 0, Token(1), Token(101));
    auto fresh = Snapshot(sql, owner);
    Check(fresh.status == db::ReadStatus::Found && fresh.snapshot.bytes.empty() &&
              fresh.snapshot.storageRevision == 0,
          "new owned state invalid");
    auto checkpoint = Checkpoint(registry, 42);
    auto plan = db::Save(checkpoint, registry, owner, 0, Token(102));
    Execute(sql, plan);
    auto receipt = Resolve(sql, plan);
    Check(receipt.status == db::Resolution::Committed && receipt.storageRevision == 1,
          "save receipt not durable");
    auto saved = Snapshot(sql, owner);
    Check(saved.status == db::ReadStatus::Found && saved.snapshot.storageRevision == 1,
          "saved checkpoint missing");
    auto loaded = DecodeCheckpoint(saved.snapshot.bytes, registry);
    Check(bool(loaded) && *loaded.checkpoint == checkpoint, "SQL round trip changed runtime state");
    Execute(sql, plan, false);
    Check(Resolve(sql, plan).status == db::Resolution::Committed,
          "identical operation replay not recognized");
    Check(
        Number(sql,
               "SELECT COUNT(*) FROM mission_engine_operations WHERE character_id=42 AND operation_kind=2") ==
            1,
        "save replay duplicated operation receipt");
    Check(!db::Save(checkpoint, registry, {43, 1, Token(1)}, 0, Token(103)),
          "cross-character checkpoint generated SQL");
}
void Fences(CAsyncSQL &sql, const DefinitionRegistry &registry)
{
    auto old = Claim(sql, 43, 0, Token(2), Token(104));
    auto checkpoint = Checkpoint(registry, 43);
    Execute(sql, db::Save(checkpoint, registry, old, 0, Token(105)));
    auto current = Claim(sql, 43, 1, Token(3), Token(106));
    auto next = checkpoint;
    ++next.revision;
    next.facts.flags.insert("persist.later");
    auto stale = db::Save(next, registry, old, 1, Token(107));
    Execute(sql, stale, false);
    Check(Resolve(sql, stale).status == db::Resolution::Absent &&
              Snapshot(sql, old).status == db::ReadStatus::Missing,
          "retired owner could read/write active state");
    auto rejectedClaim = db::Claim(43, 1, Token(4), Token(108));
    Execute(sql, rejectedClaim, false);
    Check(Resolve(sql, rejectedClaim).status == db::Resolution::Absent, "stale claim wrote receipt");
    Execute(sql, db::Save(next, registry, current, 1, Token(109)));
    auto release = db::Release(current, Token(110));
    Execute(sql, release);
    Check(Resolve(sql, release).status == db::Resolution::Committed &&
              Snapshot(sql, current).status == db::ReadStatus::Missing,
          "released owner remained valid");
    auto reusedToken = Claim(sql, 43, 3, old.token, Token(111));
    Check(reusedToken.epoch == 4, "fencing epoch reused");
    auto later = next;
    ++later.revision;
    later.facts.flags.insert("persist.later_again");
    Execute(sql, db::Save(later, registry, old, 2, Token(112)), false);
    Check(Snapshot(sql, reusedToken).snapshot.storageRevision == 2, "old epoch changed same-token state");
}
void MissingArchiveAndCorruption(CAsyncSQL &sql, const DefinitionRegistry &registry)
{
    auto owner = Claim(sql, 47, 0, Token(7), Token(113));
    DefinitionRegistry future;
    Check(future.Activate(Definitions(3)).status == Publication::Status::Published,
          "future registry invalid");
    MissionRuntime runtime(future, {47, 17}, 1000);
    Accepted(runtime.Start("persist.story", 3, 10, 1000));
    auto plan = db::Save(runtime.Save(), future, owner, 0, Token(114));
    Execute(sql, plan, false);
    Check(Resolve(sql, plan).status == db::Resolution::Absent && Snapshot(sql, owner).snapshot.bytes.empty(),
          "unarchived definition wrote an unrestorable checkpoint");
    auto v3 = Package(4, 3);
    Check(bool(v3), "future package invalid");
    Execute(sql, db::ArchivePackage(v3.bytes));
    Execute(sql, plan);

    owner = Claim(sql, 48, 0, Token(8), Token(115));
    Execute(sql, db::Save(Checkpoint(registry, 48), registry, owner, 0, Token(116)));
    auto original = Snapshot(sql, owner);
    Check(original.status == db::ReadStatus::Found, "corruption fixture missing");
    auto corrupt = original.snapshot.bytes;
    corrupt.back() ^= 1;
    Query(sql, {"UPDATE mission_engine_characters SET checkpoint_bytes=? WHERE character_id=48",
                {SqlParam::Blob(Binary(corrupt))}});
    Check(Snapshot(sql, owner).status == db::ReadStatus::Invalid, "corrupt checkpoint was accepted");
    Query(sql, {"UPDATE mission_engine_characters SET checkpoint_bytes=? WHERE character_id=48",
                {SqlParam::Blob(Binary(original.snapshot.bytes))}});
    auto reference = original.snapshot.metadata.definitions.front();
    auto read = Query(sql, db::ReadDefinitionArchive(reference));
    auto archived = db::DecodeDefinitionArchive(*read, reference);
    Check(bool(archived), "valid archive missing");
    const auto bytes = Package(1, 1).bytes;
    corrupt = bytes;
    corrupt.back() ^= 1;
    Query(sql, {"UPDATE mission_engine_packages SET package_bytes=? WHERE content_version=1",
                {SqlParam::Blob(Binary(corrupt))}});
    read = Query(sql, db::ReadDefinitionArchive(reference));
    Check(!db::DecodeDefinitionArchive(*read, reference), "corrupt archive restored a graph");
    Execute(sql, db::ArchivePackage(bytes), false);
    Query(sql, {"UPDATE mission_engine_packages SET package_bytes=? WHERE content_version=1",
                {SqlParam::Blob(Binary(bytes))}});
    Message blocked(
        sql.DirectQueryPrepared("DELETE FROM mission_engine_packages WHERE content_version=1", {}));
    Check(blocked->uiSQLErrno == ER_ROW_IS_REFERENCED_2, "referenced package could be deleted");

    // Simulate a corrupt/imported row whose original CHECK constraints were
    // bypassed. The read query must refuse it before CAsyncSQL buffers the blob.
    std::string oversized(CheckpointBytes + 1, 'x');
    Text(sql, "SET check_constraint_checks=0");
    Query(sql, {"UPDATE mission_engine_characters SET checkpoint_bytes=? WHERE character_id=48",
                {SqlParam::Blob(oversized)}});
    Text(sql, "SET check_constraint_checks=1");
    auto tooLarge = Query(sql, db::ReadSnapshot(owner));
    Check(tooLarge->Get()->preparedRows.size() == 1 && tooLarge->Get()->preparedLengths[0][7] == 0,
          "oversized checkpoint transferred to client");
    Check(db::DecodeSnapshot(*tooLarge, owner).status == db::ReadStatus::Invalid,
          "oversized checkpoint silently became fresh state");
    Query(sql, {"UPDATE mission_engine_characters SET checkpoint_bytes=? WHERE character_id=48",
                {SqlParam::Blob(Binary(original.snapshot.bytes))}});

    Text(sql, "SET check_constraint_checks=0");
    Query(sql, {"UPDATE mission_engine_packages SET package_bytes=? WHERE content_version=1",
                {SqlParam::Blob(std::move(oversized))}});
    Text(sql, "SET check_constraint_checks=1");
    tooLarge = Query(sql, db::ReadDefinitionArchive(reference));
    Check(tooLarge->Get()->preparedRows.size() == 1 && tooLarge->Get()->preparedLengths[0][6] == 0,
          "oversized package transferred to client");
    Check(!db::DecodeDefinitionArchive(*tooLarge, reference), "oversized package accepted");
    Query(sql, {"UPDATE mission_engine_packages SET package_bytes=? WHERE content_version=1",
                {SqlParam::Blob(Binary(bytes))}});

    auto emptyOwner = Claim(sql, 54, 0, Token(54), Token(130));
    Text(sql, "SET check_constraint_checks=0");
    Query(sql, {"UPDATE mission_engine_characters SET checkpoint_bytes=? WHERE character_id=54",
                {SqlParam::Blob(std::string(CheckpointBytes + 1, 'x'))}});
    Text(sql, "SET check_constraint_checks=1");
    Check(Snapshot(sql, emptyOwner).status == db::ReadStatus::Invalid,
          "oversized blob with empty metadata became fresh state");
    Query(sql, {"UPDATE mission_engine_characters SET checkpoint_bytes=NULL WHERE character_id=54", {}});

    Text(sql, "SET foreign_key_checks=0");
    Execute(sql, db::ArchivePackage(bytes), false);
    Text(sql, "SET foreign_key_checks=1");
    Query(sql, {"UPDATE mission_engine_schema SET schema_version=99 WHERE singleton=1", {}});
    Execute(sql, db::ArchivePackage(bytes), false);
    Query(sql, {"UPDATE mission_engine_schema SET schema_version=1 WHERE singleton=1", {}});
    Execute(sql, db::ArchivePackage(bytes));
}
void ConcurrentSaves(CAsyncSQL &admin, const DefinitionRegistry &registry)
{
    auto owner = Claim(admin, 46, 0, Token(6), Token(117));
    auto checkpoint = Checkpoint(registry, 46);
    auto first = db::Save(checkpoint, registry, owner, 0, Token(118));
    checkpoint.facts.flags.insert("persist.other_candidate");
    auto second = db::Save(checkpoint, registry, owner, 0, Token(119));
    std::array<db::Plan, 2> plans{first, second};
    std::array<SqlTransactionOutcome, 2> outcomes{};
    std::array<std::exception_ptr, 2> errors{};
    std::latch ready(2), start(1);
    std::array<std::thread, 2> threads;
    for (size_t index = 0; index < 2; ++index)
        threads[index] = std::thread(
            [&, index]
            {
                bool signaled = false;
                try
                {
                    CAsyncSQL sql;
                    sql.SetTimeouts(3);
                    sql.SetRetryConnectionErrors(false);
                    if (!sql.Setup("127.0.0.1", "root", "", database.c_str(), "utf8mb4", true, port))
                        throw std::runtime_error("race connection failed");
                    ready.count_down();
                    signaled = true;
                    start.wait();
                    Message result(sql.DirectTransactionPrepared(plans[index].statements));
                    outcomes[index] = result->transactionOutcome;
                }
                catch (...)
                {
                    errors[index] = std::current_exception();
                    if (!signaled)
                        ready.count_down();
                }
            });
    ready.wait();
    start.count_down();
    for (auto &thread : threads)
        thread.join();
    for (auto &error : errors)
        if (error)
            std::rethrow_exception(error);
    Check(
        (outcomes[0] == SqlTransactionOutcome::Committed && outcomes[1] == SqlTransactionOutcome::Conflict) ||
            (outcomes[1] == SqlTransactionOutcome::Committed &&
             outcomes[0] == SqlTransactionOutcome::Conflict),
        "two saves committed one storage revision");
    auto a = Resolve(admin, first), b = Resolve(admin, second);
    Check((a.status == db::Resolution::Committed && b.status == db::Resolution::Absent) ||
              (b.status == db::Resolution::Committed && a.status == db::Resolution::Absent),
          "CAS race receipts disagree");
    Check(Snapshot(admin, owner).snapshot.storageRevision == 1 &&
              Number(admin, "SELECT COUNT(*) FROM mission_engine_operations WHERE character_id=46 AND "
                            "operation_kind=2") == 1,
          "race duplicated checkpoint operation");
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
    Check(results.size() == count, "async persistence queue stalled");
    return results;
}
void AsyncAndDisconnect(CAsyncSQL &admin, const DefinitionRegistry &registry)
{
    auto owner = Claim(admin, 51, 0, Token(11), Token(120));
    auto plan = db::Save(Checkpoint(registry, 51), registry, owner, 0, Token(121));
    CAsyncSQL worker;
    Connect(worker, false);
    worker.ReturnTransactionPrepared(plan.statements, reinterpret_cast<void *>(1));
    auto command = db::ReadSnapshot(owner);
    worker.ReturnQueryPrepared(command.query.c_str(), command.params, reinterpret_cast<void *>(2));
    auto results = Wait(worker, 2);
    worker.Quit();
    Check(db::Confirmed(*results[0]) && results[0]->pvUserData == reinterpret_cast<void *>(1) &&
              results[1]->pvUserData == reinterpret_cast<void *>(2),
          "async store callbacks reordered");
    Check(db::DecodeSnapshot(*results[1], owner).snapshot.storageRevision == 1,
          "async committed checkpoint missing");

    owner = Claim(admin, 50, 0, Token(10), Token(122));
    plan = db::Save(Checkpoint(registry, 50), registry, owner, 0, Token(123));
    auto interrupted = plan;
    interrupted.statements.insert(interrupted.statements.begin() + 2, {"SELECT SLEEP(5)", {}, {}, {}});
    CAsyncSQL killed;
    Connect(killed, false);
    auto connection = mysql_thread_id(killed.GetSQLHandle());
    killed.ReturnTransactionPrepared(interrupted.statements, nullptr);
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
    Check(sleeping, "did not observe store write before disconnect");
    Text(admin, "KILL CONNECTION " + std::to_string(connection));
    results = Wait(killed, 1);
    killed.Quit();
    Check(!db::Confirmed(*results[0]) && Resolve(admin, plan).status == db::Resolution::Absent &&
              Snapshot(admin, owner).snapshot.bytes.empty(),
          "killed store transaction persisted checkpoint/receipt");
    Execute(admin, plan);
}
void LostCommit(CAsyncSQL &admin, const DefinitionRegistry &registry, unsigned int proxyPort)
{
    auto owner = Claim(admin, 45, 0, Token(5), Token(124));
    auto checkpoint = Checkpoint(registry, 45);
    auto plan = db::Save(checkpoint, registry, owner, 0, Token(125));
    CAsyncSQL proxied;
    Connect(proxied, true, proxyPort);
    auto result = Execute(proxied, plan, false);
    Check(result->transactionOutcome == SqlTransactionOutcome::CommitUnknown,
          "real lost store COMMIT not ambiguous");
    auto resolved = Resolve(admin, plan);
    Check(resolved.status == db::Resolution::Committed && resolved.storageRevision == 1,
          "lost COMMIT could not reconcile");
    Execute(proxied, plan, false);
    Check(Resolve(admin, plan).status == db::Resolution::Committed, "unknown save replay not idempotent");
    ++checkpoint.revision;
    checkpoint.facts.flags.insert("persist.token_reused");
    auto reused = db::Save(checkpoint, registry, owner, 1, plan.operation);
    Execute(admin, reused, false);
    Check(Resolve(admin, reused).status == db::Resolution::TokenConflict &&
              Snapshot(admin, owner).snapshot.storageRevision == 1,
          "operation ID reused with different content changed state");
}
void InFlightResolution(CAsyncSQL &admin, const DefinitionRegistry &registry)
{
    for (bool rollback : {false, true})
    {
        const uint32_t character = rollback ? 53 : 52;
        auto owner = Claim(admin, character, 0, Token(uint8_t(character)), Token(rollback ? 128 : 126));
        auto plan =
            db::Save(Checkpoint(registry, character), registry, owner, 0, Token(rollback ? 129 : 127));
        auto delayed = plan;
        delayed.statements.insert(delayed.statements.begin() + 2, {"SELECT SLEEP(2)", {}, {}, 1});
        if (rollback)
            delayed.statements.insert(
                delayed.statements.end() - 1,
                {"INSERT INTO mission_resolution_missing_table(id) VALUES(1)", {}, {}, {}});
        CAsyncSQL worker;
        Connect(worker, false);
        const auto connection = mysql_thread_id(worker.GetSQLHandle());
        worker.ReturnTransactionPrepared(delayed.statements, nullptr);
        bool sleeping = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline)
        {
            sleeping = Number(admin,
                              "SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE ID=? AND "
                              "INFO='SELECT SLEEP(2)'",
                              {SqlParam::UInt(connection)}) != 0;
            if (sleeping)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        Check(sleeping, "did not observe in-flight checkpoint write");
        // Resolve starts while the original writer owns the character row.
        // Its preflight creates an older RR snapshot. Receipt FOR UPDATE must
        // still see the later commit, or prove absence after the later rollback.
        auto resolved = Resolve(admin, plan);
        auto result = Wait(worker, 1);
        worker.Quit();
        Check(resolved.status == (rollback ? db::Resolution::Absent : db::Resolution::Committed),
              "resolution saw premature absence or a stale read view");
        Check(db::Confirmed(*result[0]) == !rollback, "in-flight writer outcome changed");
        Check(Snapshot(admin, owner).snapshot.storageRevision == (rollback ? 0u : 1u),
              "in-flight resolution disagrees with durable state");
    }
}
void ColdProcess(CAsyncSQL &sql)
{
    db::Owner owner{42, 1, Token(1)};
    auto saved = Snapshot(sql, owner);
    Check(saved.status == db::ReadStatus::Found && saved.snapshot.storageRevision == 1,
          "cold process snapshot missing");
    DefinitionRegistry registry;
    auto activePackage = LoadPackage(Package(2, 2).bytes);
    Check(bool(activePackage), "active package fixture invalid");
    auto current = activePackage.package->Graphs().front();
    DefinitionReference active{current->Source().id, current->Source().version,
                               FingerprintDefinition(*current)};
    auto read = Query(sql, db::ReadDefinitionArchive(active));
    auto loadedActive = db::DecodeDefinitionArchive(*read, active);
    Check(bool(loadedActive), "active archive missing after process restart");
    std::vector<Definition> sources;
    for (const auto &graph : loadedActive.package->Graphs())
        sources.push_back(graph->Source());
    Check(registry.Activate(sources).status == Publication::Status::Published, "cold active catalog failed");
    Check(!DecodeCheckpoint(saved.snapshot.bytes, registry),
          "cold restore used latest in place of old definition");
    sources.clear();
    for (const auto &pin : saved.snapshot.metadata.definitions)
    {
        read = Query(sql, db::ReadDefinitionArchive(pin));
        auto package = db::DecodeDefinitionArchive(*read, pin);
        Check(bool(package), "exact old package missing after process restart");
        for (const auto &graph : package.package->Graphs())
            if (graph->Source().id == pin.mission && graph->Source().version == pin.version)
                sources.push_back(graph->Source());
    }
    Check(registry.Retain(sources).status == Publication::Status::Published, "cold history import failed");
    const auto checkpoint = DecodeCheckpoint(saved.snapshot.bytes, registry);
    Check(bool(checkpoint), "cold checkpoint invalid");
    MissionRuntime runtime(registry, {42, 9999}, checkpoint.checkpoint->now);
    Accepted(runtime.Restore(*checkpoint.checkpoint, checkpoint.checkpoint->now));
    Check(runtime.Save() == *checkpoint.checkpoint &&
              runtime.Inspect(10)->definition->Source().version == 1 &&
              registry.Latest("persist.story")->Source().version == 2,
          "cold progress changed or pinned version replaced");
    Check(runtime.Facts().choices.at("persist.road") == "forest" &&
              runtime.Facts().flags.contains("persist.external"),
          "cold facts missing");
    Check(runtime.PendingActions() ==
              std::vector<ActionKey>{{42, "persist.story", 1, 10, "reward", "persist.reward"}},
          "cold reward key changed");
    auto duplicate =
        runtime.Dispatch({{42, 9999}, {9, 299}, 1001, EventKind::ItemCollected, "iron.c", 1, {}}, 1002);
    Check(duplicate.code == ResultCode::Duplicate, "cold dedup lost");
    Accepted(runtime.AdvanceTime(6001));
    Check(runtime.Inspect(20)->status == MissionStatus::Completed, "cold timer did not resume");
}
} // namespace
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("mission-persistence-test");
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
        if (argc != 5 && argc != 6)
            throw std::runtime_error(
                "usage: MissionPersistenceTests PORT FRESH_SCHEMA PROXY_PORT MIGRATION [--cold-check]");
        port = static_cast<unsigned int>(std::stoul(argv[1]));
        database = argv[2];
        Check(database.starts_with("auriga_mission_sql_test_") && database.size() <= 63 &&
                  database.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == database.npos,
              "isolated schema required");
        if (argc == 6)
        {
            Check(std::string_view(argv[5]) == "--cold-check", "unknown child mode");
            CAsyncSQL sql;
            Connect(sql);
            ColdProcess(sql);
            std::cout << "Mission persistence cold-process: " << checks << " checks passed\n";
            return 0;
        }
        CAsyncSQL bootstrap;
        Check(bootstrap.Setup("127.0.0.1", "root", "", "", "utf8mb4", true, port),
              "isolated bootstrap failed");
        Text(bootstrap, "CREATE DATABASE " + database + " CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
        CAsyncSQL sql;
        Connect(sql);
        Text(sql, "CREATE TABLE account_mission_pass(account_id INT PRIMARY KEY,marker INT NOT NULL) "
                  "ENGINE=InnoDB");
        Text(sql, "INSERT INTO account_mission_pass VALUES(1,123)");
        Text(sql, "CREATE TABLE quest(id INT PRIMARY KEY,marker INT NOT NULL) ENGINE=InnoDB");
        Text(sql, "INSERT INTO quest VALUES(1,456)");
        Migration(sql, argv[4]);
        Archive(sql);
        DefinitionRegistry registry;
        Check(registry.Activate(Definitions()).status == Publication::Status::Published,
              "runtime fixture failed");
        Check(!db::Claim(0, 0, Token(1), Token(1)) && !db::Claim(42, 0, {}, Token(1)) &&
                  !db::Claim(42, UINT64_MAX, Token(1), Token(1)),
              "invalid claim accepted");
        Check(db::NewToken() != db::NewToken(), "cryptographic owner token repeated");
        WarmSave(sql, registry);
        Fences(sql, registry);
        MissingArchiveAndCorruption(sql, registry);
        ConcurrentSaves(sql, registry);
        AsyncAndDisconnect(sql, registry);
        InFlightResolution(sql, registry);
        LostCommit(sql, registry, static_cast<unsigned int>(std::stoul(argv[3])));
        std::cout << "Mission persistence: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
