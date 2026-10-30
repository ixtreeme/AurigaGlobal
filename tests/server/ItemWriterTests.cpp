// F4.4 (IC-034): the GameServer's durable item writer. --completion-only
// drives the lanes with exact fabricated completions; --owned-sandbox commits
// real operations, loses COMMIT acknowledgements on purpose and reloads after
// a simulated crash against a new owned MariaDB (see ItemWriterTests.py).
// length.h before AsyncSQL.h: the SQL header #defines QUERY_MAX_LEN, an EMisc name.
#include <common/length.h>
#include "PersistentItemFixture.hpp"
#include "CatalogSqlPopulate.hpp"
#include "ItemSessionTestSupport.hpp"
#include "../../SRC/Server/GameServer/item/ItemWriter.hpp"
#include "../../SRC/Server/GameServer/ecs/TypedItems.hpp"
#include <ItemPersistenceSql.hpp>
#include <Core/Logging.hpp>
#include <chrono>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>

using namespace auriga::item;
using namespace item_session_support;
namespace p = auriga::item::persistence;
namespace f = persistent_item_fixture;
namespace ti = ecs::item;
using E = ItemPersistenceError;
using W = rt::ItemWriteOutcome;
using U = SqlParam;

namespace
{
unsigned checks = 0;
void Check(bool good, const char* why, std::source_location at = std::source_location::current())
{
    ++checks;
    if (!good) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
ItemInstanceId Id(std::uint64_t value) { return ItemInstanceId::Create(value).value(); }
const ItemStorage Inventory{Owner, ItemStorageKind::Inventory, 1};
ItemPlacement Cell(std::uint32_t anchor) { return {ItemStorageKind::Inventory, 1, anchor}; }
PersistentItem Leather(std::uint64_t id, std::uint32_t count, std::uint32_t anchor)
{
    auto item = f::Leather(id);
    item.count = count;
    item.placement->anchor = anchor;
    return item;
}
PersistentItem Bracelet()
{
    auto item = f::Leather(5);
    item.key = item_catalog_fixture::Bracelet();
    item.count = 1;
    item.accessory = AccessorySocketState{3, 2, 1000, false};
    item.placement = ItemPlacement{ItemStorageKind::Equipment, 1, static_cast<unsigned>(EquipmentSlot::Wrist) - 1};
    return item;
}
std::vector<PersistentItem> StartItems() { return {f::Sword(), Leather(2, 3, 1), Leather(4, 1, 6), Bracelet()}; }

struct OperationCounter
{
    std::uint8_t next = 1;
    ItemOperationId operator()()
    {
        ItemOperationId op{};
        op[0] = next++;
        op[15] = 0x57;
        return op;
    }
};
// The ECS state, canonical, compared item by item.
std::vector<PersistentItem> Items(entt::registry& r)
{
    return ti::Project(r, ti::FindOwner(r, Owner)).value().items;
}
const PersistentItem* Find(const std::vector<PersistentItem>& items, std::uint64_t id)
{
    for (const auto& item : items)
        if (item.id.Value() == id) return &item;
    return nullptr;
}
bool IsWrite(const SqlTransaction& job)
{
    for (const auto& step : job)
        if (step.query.starts_with("INSERT INTO player.item_operation_receipt")) return true;
    return false;
}
// The receipt row the database holds for a committed write job.
Rows Receipt(const SqlTransaction& write)
{
    for (const auto& step : write)
        if (step.query.starts_with("INSERT INTO player.item_operation_receipt"))
        {
            const auto& v = step.params;
            const auto text = [](const SqlParam& param) { return std::string(param.bytes.begin(), param.bytes.end()); };
            return {{text(v[1]), std::to_string(v[2].u64), text(v[3]), std::to_string(v[4].u64), std::to_string(v[5].u64), text(v[6])}};
        }
    throw std::runtime_error("not a write job");
}

// --- state machine with fabricated completions ----------------------------
struct Bench
{
    std::shared_ptr<const ItemCatalog> c = BuildItemCatalog(f::Catalog()).value();
    FakeExecutor sessionSql, writerSql;
    rt::ItemOwnerSessions sessions{c, sessionSql};
    entt::registry registry;
    rt::ItemWriter writer{c, sessions, registry, writerSql, OperationCounter{}};
    rt::ItemSessionLease lease = Lease(1, 100);
    Bench()
    {
        Check(sessions.Open(Owner, lease) == rt::ItemSessionOpen::Started, "session start");
        sessions.Complete(sessionSql.jobs.back().first, *Claimed(sessionSql.jobs.back().second, 5, 3));
        sessions.Complete(sessionSql.jobs.back().first, *LoadedRows(sessionSql.jobs.back().second, 5, 3));
        Check(sessions.State(Owner) == rt::ItemSessionState::Active, "session active");
        sessions.TakeEvents();
        p::LoadedItemOwner loaded;
        loaded.fence = {Owner, 5, 3};
        loaded.storages = {f::Inventory(), {{Owner, ItemStorageKind::Equipment, 1}, 8, 1, 1}};
        loaded.wallets = {f::Gold()};
        loaded.items = StartItems();
        Check(ti::Materialize(registry, *c, loaded, entt::null).has_value(), "materialize");
    }
    std::pair<std::uint64_t, SqlTransaction> Job() { return writerSql.jobs.back(); }
    void Commit() { const auto [ticket, job] = Job(); writer.Complete(ticket, *Completed(job, {})); }
    std::vector<rt::ItemWriteResult> Results() { return writer.TakeResults(); }
    bool Consistent() { return ti::Verify(registry, *c, ti::FindOwner(registry, Owner)).has_value(); }
    std::uint64_t Revision() { return sessions.Fence(Owner, lease)->revision; }
};

void Basics()
{
    Bench b;
    Check(!b.writer.Request(Owner, Lease(2, 100), rt::DestroyIntent{Id(2)}), "foreign lease admitted");
    Check(!b.writer.Request({ItemOwnerKind::Character, 43}, b.lease, rt::DestroyIntent{Id(2)}), "unknown owner admitted");

    // One job per lane; the second intent is built from the committed state.
    const auto first = b.writer.Request(Owner, b.lease, rt::MoveIntent{Id(2), Cell(3)});
    const auto second = b.writer.Request(Owner, b.lease, rt::MoveIntent{Id(2), Cell(4)});
    Check(first && second && b.writer.Queued(Owner) == 2, "intents queued");
    b.writer.Pump();
    Check(b.writerSql.jobs.size() == 1 && b.writer.Busy(Owner) && b.writer.Queued(Owner) == 1, "lane not serialized");
    Check(Find(Items(b.registry), 2)->placement->anchor == 1, "ECS changed before the commit");
    b.writer.Pump();
    Check(b.writerSql.jobs.size() == 1, "second job started while the first is in SQL");
    b.Commit();
    const auto moved = b.Results();
    Check(moved.size() == 1 && moved[0].request == *first && moved[0].outcome == W::Committed && moved[0].operation[0] == 1, "first not committed");
    Check(Find(Items(b.registry), 2)->placement->anchor == 3 && Find(Items(b.registry), 2)->revision == 2 && b.Revision() == 4 && b.Consistent(),
          "commit not applied");
    b.writer.Pump();
    Check(b.writerSql.jobs.size() == 2, "second intent not started");
    b.Commit();
    Check(b.Results()[0].outcome == W::Committed && Find(Items(b.registry), 2)->placement->anchor == 4 && b.Revision() == 5, "second commit");

    // Refused by the rules: nothing reaches SQL.
    b.writer.Request(Owner, b.lease, rt::DestroyIntent{Id(99)});
    b.writer.Request(Owner, b.lease, rt::MergeIntent{Id(2), f::Sword().id, 1});
    b.writer.Pump();
    auto results = b.Results();
    Check(results.size() == 2 && results[0].outcome == W::Rejected && results[0].error == E::MissingItem &&
          results[1].outcome == W::Rejected && results[1].error == E::InvalidChange && b.writerSql.jobs.size() == 2, "rejections");

    // Rolled back by SQL: nothing changes, the session is free again.
    const auto before = Items(b.registry);
    b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(4), 1});
    b.writer.Pump();
    b.writer.Complete(b.Job().first, *Failed(b.Job().second, SqlTransactionOutcome::RolledBack, 9, 1452));
    results = b.Results();
    Check(results.size() == 1 && results[0].outcome == W::NotCommitted && Items(b.registry) == before && b.Revision() == 5 && !b.writer.Busy(Owner),
          "rolled-back write changed state");
    Check(b.sessions.BeginWrite(Owner, b.lease), "write admission leaked");
    b.sessions.EndWrite(Owner, b.lease, std::nullopt);

    // Split reserves a block of fresh ids first.
    b.writer.Request(Owner, b.lease, rt::SplitIntent{Id(2), 1, Cell(15)});
    b.writer.Pump();
    auto [reserveTicket, reserveJob] = b.Job();
    Check(!IsWrite(reserveJob) && b.writer.AvailableIds() == 0, "split did not reserve ids");
    b.writer.Complete(reserveTicket, *Completed(reserveJob, {{4, {{"1064"}}}}));
    Check(b.writer.AvailableIds() == 64, "reserved block");
    b.writer.Pump();
    Check(IsWrite(b.Job().second), "split not written after the reservation");
    b.Commit();
    Check(b.Results()[0].outcome == W::Committed && Find(Items(b.registry), 1000)->count == 1 && Find(Items(b.registry), 2)->count == 2 &&
          b.writer.AvailableIds() == 63 && b.Consistent(), "split result");
    b.writer.Request(Owner, b.lease, rt::SplitIntent{Id(2), 1, Cell(16)});
    b.writer.Pump();
    b.Commit();
    Check(b.Results()[0].outcome == W::Committed && Find(Items(b.registry), 1001) && b.writer.AvailableIds() == 62, "second split uses the block");
}

void LostAcknowledgements()
{
    // Committed: the receipt decides; the change is applied exactly once.
    {
        Bench b;
        b.writer.Request(Owner, b.lease, rt::DestroyIntent{f::Sword().id});
        b.writer.Pump();
        const auto write = b.Job();
        b.writer.Complete(write.first, *Failed(write.second, SqlTransactionOutcome::CommitUnknown, SqlTransactionNoStep, 2013));
        Check(b.writer.TakeResults().empty() && b.writer.Busy(Owner) && Find(Items(b.registry), f::Sword().id.Value()), "unknown commit decided early");
        const auto reconcile = b.Job();
        Check(!IsWrite(reconcile.second) && SameSteps(reconcile.second, p::PrepareReconcileItemCommit(*b.c, [&] {
                  auto view = ti::Project(b.registry, ti::FindOwner(b.registry, Owner)).value();
                  ItemOperationId op{};
                  op[0] = 1;
                  op[15] = 0x57;
                  return MakeDestroyCommit(*b.c, {view.fence, view.storages, view.items}, op, f::Sword().id).value();
              }()).value()), "reconcile is not the locking receipt read of this commit");
        b.writer.Complete(reconcile.first, *Completed(reconcile.second, {{reconcile.second.size() - 1, Receipt(write.second)}}));
        const auto results = b.Results();
        Check(results.size() == 1 && results[0].outcome == W::Committed && !Find(Items(b.registry), f::Sword().id.Value()) && b.Revision() == 4 &&
              b.Consistent(), "receipt did not commit exactly once");
    }
    // Not committed: no receipt after the locks; nothing changes, no resend.
    {
        Bench b;
        const auto before = Items(b.registry);
        b.writer.Request(Owner, b.lease, rt::DestroyIntent{f::Sword().id});
        b.writer.Pump();
        const auto write = b.Job();
        b.writer.Complete(write.first, *Failed(write.second, SqlTransactionOutcome::CommitUnknown, SqlTransactionNoStep, 2013));
        const auto reconcile = b.Job();
        b.writer.Complete(reconcile.first, *Completed(reconcile.second, {{reconcile.second.size() - 1, {}}}));
        const auto results = b.Results();
        Check(results.size() == 1 && results[0].outcome == W::NotCommitted && results[0].error == E::CommitUnknown && Items(b.registry) == before &&
              b.Revision() == 3 && b.writerSql.jobs.size() == 2, "absent receipt changed state or resent the write");
    }
    // A different receipt under the same id: the states disagree.
    {
        Bench b;
        b.writer.Request(Owner, b.lease, rt::DestroyIntent{f::Sword().id});
        b.writer.Request(Owner, b.lease, rt::DestroyIntent{Id(2)});
        b.writer.Pump();
        const auto write = b.Job();
        b.writer.Complete(write.first, *Failed(write.second, SqlTransactionOutcome::CommitUnknown, SqlTransactionNoStep, 2013));
        auto row = Receipt(write.second);
        (*row[0][0])[0] ^= 1;
        b.writer.Complete(b.Job().first, *Completed(b.Job().second, {{b.Job().second.size() - 1, row}}));
        const auto results = b.Results();
        Check(results.size() == 2 && results[0].outcome == W::Diverged && results[0].error == E::ReceiptMismatch &&
              results[1].outcome == W::Superseded && b.sessions.State(Owner) == rt::ItemSessionState::Superseded, "receipt mismatch");
    }
    // Reconciliation keeps failing: unresolved, the session must reload.
    {
        Bench b;
        b.writer.Request(Owner, b.lease, rt::DestroyIntent{f::Sword().id});
        b.writer.Pump();
        b.writer.Complete(b.Job().first, *Failed(b.Job().second, SqlTransactionOutcome::CommitUnknown, SqlTransactionNoStep, 2013));
        for (unsigned i = 0; i < rt::ItemWriter::ReconcileAttempts; ++i)
            b.writer.Complete(b.Job().first, *Failed(b.Job().second, SqlTransactionOutcome::Failed, 3, 2013));
        const auto results = b.Results();
        Check(results.size() == 1 && results[0].outcome == W::Unresolved && b.sessions.State(Owner) == rt::ItemSessionState::Superseded &&
              b.writerSql.jobs.size() == 1 + rt::ItemWriter::ReconcileAttempts, "unresolved commit");
    }
}

void Lifecycle()
{
    // A fence conflict means another session: everything queued is dropped.
    {
        Bench b;
        b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(2), 1});
        b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(2), 1});
        b.writer.Pump();
        b.writer.Complete(b.Job().first, *Failed(b.Job().second, SqlTransactionOutcome::Conflict, 4, SqlTransactionConflict));
        const auto results = b.Results();
        Check(results.size() == 2 && results[0].outcome == W::Superseded && results[0].error == E::Conflict && results[1].outcome == W::Superseded &&
              b.sessions.State(Owner) == rt::ItemSessionState::Superseded, "conflict not superseded");
        Check(!b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(2), 1}), "superseded session admitted");
    }
    // Close: queued intents are cancelled, the one in SQL still lands.
    {
        Bench b;
        b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(2), 1});
        b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(2), 1});
        b.writer.Pump();
        b.writer.Close(Owner, b.lease);
        b.sessions.Close(Owner, b.lease);
        auto results = b.Results();
        Check(results.size() == 1 && results[0].outcome == W::Cancelled && b.sessions.State(Owner) == rt::ItemSessionState::Closing, "close order");
        b.Commit();
        results = b.Results();
        Check(results.size() == 1 && results[0].outcome == W::Committed && Find(Items(b.registry), 2)->count == 2 &&
              b.sessions.State(Owner) == rt::ItemSessionState::Closed, "in-flight write lost on close");
        b.writer.Pump();
        Check(!b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(2), 1}), "closed lane admitted");
    }
    // ECS changed behind the writer's back: durable, but the session reloads.
    {
        Bench b;
        b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(2), 1});
        b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(4), 1});
        b.writer.Pump();
        const auto state = b.registry.get<ti::OwnerState>(ti::FindOwner(b.registry, Owner));
        b.registry.get<ti::Revision>(state.items.at(2)).value = 9;
        b.Commit();
        const auto results = b.Results();
        Check(results.size() == 2 && results[0].outcome == W::Diverged && results[1].outcome == W::Superseded &&
              b.sessions.State(Owner) == rt::ItemSessionState::Superseded, "diverged ECS not reported");
    }
    // A failed reservation fails only the split.
    {
        Bench b;
        b.writer.Request(Owner, b.lease, rt::SplitIntent{Id(2), 1, Cell(15)});
        b.writer.Request(Owner, b.lease, rt::ConsumeIntent{Id(4), 1});
        b.writer.Pump();
        b.writer.Complete(b.Job().first, *Failed(b.Job().second, SqlTransactionOutcome::Conflict, 3, SqlTransactionConflict));
        auto results = b.Results();
        Check(results.size() == 1 && results[0].outcome == W::NotCommitted && results[0].error == E::LimitExceeded, "exhausted ids");
        b.writer.Pump();
        b.Commit();
        Check(b.Results()[0].outcome == W::Committed, "lane stopped after a failed reservation");
    }
}

// --- owned MariaDB -------------------------------------------------------
std::string Scalar(CAsyncSQL& db, const std::string& query, const SqlParams& params = {})
{
    Message m(db.DirectQueryPrepared(query.c_str(), params));
    Check(m && !m->uiSQLErrno && m->Get(), "isolated query failed");
    auto row = m->Get()->FetchRow();
    auto lengths = m->Get()->FetchLengths();
    Check(row && row[0] && lengths, "scalar missing");
    return {row[0], lengths[0]};
}
void Execute(CAsyncSQL& db, const std::string& query, const SqlParams& params = {})
{
    Message m(db.DirectQueryPrepared(query.c_str(), params));
    Check(m && !m->uiSQLErrno, "isolated statement failed");
}
void Direct(CAsyncSQL& db, unsigned port, const std::string& pw, const char* user)
{
    db.SetTimeouts(30);
    db.SetRetryConnectionErrors(false);
    Check(db.Setup("127.0.0.1", user, pw.c_str(), "player", "utf8mb4", true, port), "owned SQL connection");
}
void Async(CAsyncSQL& db, unsigned port, const std::string& pw)
{
    db.SetTimeouts(30);
    db.SetRetryConnectionErrors(false);
    Check(db.Setup("127.0.0.1", "item_writer", pw.c_str(), "player", "utf8mb4", false, port) &&
          db.WaitForConnection(std::chrono::seconds(10)), "owned async SQL connection");
}
// Runs writes for real but can lose the COMMIT acknowledgement on purpose.
struct LossyExecutor final : rt::ItemSqlExecutor
{
    enum class Mode { Pass, CommitThenLose, Lose };
    rt::AsyncItemSqlExecutor& inner;
    CAsyncSQL& direct;
    Mode mode = Mode::Pass;
    std::vector<std::pair<std::uint64_t, Message>> lost;
    LossyExecutor(rt::AsyncItemSqlExecutor& i, CAsyncSQL& d) : inner(i), direct(d) {}
    bool Submit(std::uint64_t ticket, SqlTransaction job) override
    {
        if (mode == Mode::Pass || !IsWrite(job)) return inner.Submit(ticket, std::move(job));
        if (mode == Mode::CommitThenLose)
        {
            Message real(direct.DirectTransactionPrepared(job));
            Check(real && real->transactionOutcome == SqlTransactionOutcome::Committed, "lossy write did not commit");
        }
        lost.emplace_back(ticket, Failed(job, SqlTransactionOutcome::CommitUnknown, SqlTransactionNoStep, 2013));
        mode = Mode::Pass;
        return true;
    }
    void Pump(rt::ItemWriter& writer)
    {
        inner.Pump([&](std::uint64_t ticket, SQLMsg& m) { writer.Complete(ticket, m); });
        for (auto& [ticket, message] : std::exchange(lost, {})) writer.Complete(ticket, *message);
    }
};
struct Core
{
    std::shared_ptr<const ItemCatalog> c;
    CAsyncSQL sessionSql, writerSql;
    std::unique_ptr<rt::AsyncItemSqlExecutor> sessionExec, writerExec;
    std::unique_ptr<LossyExecutor> lossy;
    std::unique_ptr<rt::ItemOwnerSessions> sessions;
    entt::registry registry;
    std::unique_ptr<rt::ItemWriter> writer;
    rt::ItemSessionLease lease;
    Core(std::shared_ptr<const ItemCatalog> catalog, unsigned port, const std::string& pw, CAsyncSQL& direct, rt::ItemSessionLease l)
        : c(std::move(catalog)), lease(l)
    {
        Async(sessionSql, port, pw);
        Async(writerSql, port, pw);
        sessionExec = std::make_unique<rt::AsyncItemSqlExecutor>(sessionSql);
        writerExec = std::make_unique<rt::AsyncItemSqlExecutor>(writerSql);
        lossy = std::make_unique<LossyExecutor>(*writerExec, direct);
        sessions = std::make_unique<rt::ItemOwnerSessions>(c, *sessionExec);
        writer = std::make_unique<rt::ItemWriter>(c, *sessions, registry, *lossy, rt::RandomOperationId);
        Check(sessions->Open(Owner, lease) == rt::ItemSessionOpen::Started, "core session start");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (sessions->State(Owner) != rt::ItemSessionState::Active)
        {
            sessionExec->Pump(*sessions);
            // Polling rounds are not counted as checks.
            if (std::chrono::steady_clock::now() >= deadline || sessions->State(Owner) == rt::ItemSessionState::Failed)
                throw std::runtime_error("core session not active");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        Check(ti::Materialize(registry, *c, *sessions->Loaded(Owner, lease), entt::null).has_value(), "core materialize");
    }
    rt::ItemWriteResult Run(rt::ItemIntent intent)
    {
        Check(writer->Request(Owner, lease, std::move(intent)).has_value(), "intent refused");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (true)
        {
            writer->Pump();
            lossy->Pump(*writer);
            auto results = writer->TakeResults();
            if (!results.empty()) return results.front();
            if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("write did not finish");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    // ECS == a fresh database load of this session.
    bool MatchesDatabase(CAsyncSQL& db)
    {
        const auto fence = sessions->Fence(Owner, lease);
        const auto fresh = p::LoadItemOwner(db, *c, Owner, fence->sessionGeneration);
        const auto ecs = ti::Project(registry, ti::FindOwner(registry, Owner));
        if (!fresh || !ecs) return false;
        auto storages = fresh->storages;
        std::ranges::sort(storages, {}, [](const auto& l) { return l.storage; });
        return ecs->fence == fresh->fence && ecs->fence == *fence && ecs->items == fresh->items && ecs->storages == storages &&
               ecs->wallets == fresh->wallets && ti::Verify(registry, *c, ti::FindOwner(registry, Owner)).has_value();
    }
};

void OwnedSandbox(unsigned port, const std::string& pw, const std::string& token)
{
    CAsyncSQL admin, writer, lossyDirect;
    Direct(admin, port, pw, "root");
    Check(Scalar(admin, "SELECT token FROM protos.ic_writer_owner") == token, "owned sandbox identity");
    Direct(writer, port, pw, "item_writer");
    Direct(lossyDirect, port, pw, "item_writer");
    const auto c = BuildItemCatalog(f::Catalog()).value();
    catalog_sql_populate::Populate(admin, f::Catalog());
    catalog_sql_populate::Pin(admin, c->Revision(), c->ContentHash());
    // Fixture ids stay below 1000 (and at the top of the range); new ids start at 1000.
    Execute(admin, "UPDATE player.item_instance_id_allocator SET next_id=1000");

    // Reservation: consecutive, disjoint ranges; exhaustion is refused.
    Check(p::ReserveItemInstanceIds(writer, *c, 10) == p::ItemInstanceIdRange{1000, 10}, "first id range");
    Check(p::ReserveItemInstanceIds(writer, *c, 5) == p::ItemInstanceIdRange{1010, 5}, "second id range");
    Check(p::ReserveItemInstanceIds(writer, *c, 0).error() == E::LimitExceeded, "empty range");
    Execute(admin, "UPDATE player.item_instance_id_allocator SET next_id=18446744073709551610");
    Check(p::ReserveItemInstanceIds(writer, *c, 10).error() == E::LimitExceeded, "64-bit overflow reserved");
    Check(Scalar(admin, "SELECT next_id FROM player.item_instance_id_allocator") == "18446744073709551610", "refused reservation moved the counter");
    Execute(admin, "UPDATE player.item_instance_id_allocator SET next_id=2000");

    // An owner with items, then a live session on core A.
    const auto provision = rt::CharacterOwnerProvision(42, 1000);
    Check(p::ProvisionItemOwner(writer, *c, provision).has_value(), "provision");
    auto grant = f::Grant(*c, StartItems());
    grant.operationId = {};
    grant.operationId[0] = 0xEE;
    grant.owners = {{Owner, 1, 1}};
    grant.storages = provision.storages;
    Check(p::CommitItems(writer, *c, grant).has_value(), "initial grant");
    auto a = std::make_unique<Core>(c, port, pw, lossyDirect, Lease(1, 100));
    Check(a->MatchesDatabase(writer) && Items(a->registry).size() == 4, "core A loaded");

    // Real operations: each one durable and applied once.
    Check(a->Run(rt::MoveIntent{Id(2), Cell(3)}).outcome == W::Committed && a->MatchesDatabase(writer), "move");
    Check(a->Run(rt::MoveIntent{Id(2), Cell(6)}).outcome == W::Committed && a->MatchesDatabase(writer), "swap");
    Check(Find(Items(a->registry), 4)->placement->anchor == 3, "swapped partner");
    Check(a->Run(rt::SplitIntent{Id(2), 2, Cell(15)}).outcome == W::Committed && a->MatchesDatabase(writer), "split");
    Check(Find(Items(a->registry), 2000) && Find(Items(a->registry), 2000)->count == 2 && Find(Items(a->registry), 2)->count == 1, "split ids");
    Check(Scalar(admin, "SELECT next_id FROM player.item_instance_id_allocator") == std::to_string(2000 + rt::ItemWriter::IdBlock), "id block reserved once");
    Check(a->Run(rt::MergeIntent{Id(2000), Id(4), 2}).outcome == W::Committed && a->MatchesDatabase(writer), "merge");
    Check(!Find(Items(a->registry), 2000) && Find(Items(a->registry), 4)->count == 3, "merged stack");
    Check(a->Run(rt::MoveIntent{f::Sword().id, {ItemStorageKind::Equipment, 1, 0}}).outcome == W::Committed && a->MatchesDatabase(writer), "equip");
    const auto rejected = a->Run(rt::MoveIntent{Id(5), {ItemStorageKind::Equipment, 1, 0}});
    Check(rejected.outcome == W::Rejected && a->MatchesDatabase(writer), "illegal equip reached SQL");

    // Lost acknowledgement of a real commit: applied exactly once.
    const auto receiptsBefore = Scalar(admin, "SELECT COUNT(*) FROM player.item_operation_receipt");
    a->lossy->mode = LossyExecutor::Mode::CommitThenLose;
    Check(a->Run(rt::ConsumeIntent{Id(4), 1}).outcome == W::Committed && a->MatchesDatabase(writer), "lost ack of a commit");
    Check(Find(Items(a->registry), 4)->count == 2 && Scalar(admin, "SELECT COUNT(*) FROM player.item_operation_receipt") ==
          std::to_string(std::stoul(receiptsBefore) + 1), "lost ack applied twice");
    // Lost acknowledgement of a write that never reached the server: no change.
    a->lossy->mode = LossyExecutor::Mode::Lose;
    const auto lost = a->Run(rt::DestroyIntent{Id(5)});
    Check(lost.outcome == W::NotCommitted && lost.error == E::CommitUnknown && Find(Items(a->registry), 5) && a->MatchesDatabase(writer),
          "lost write invented");
    Check(Scalar(admin, "SELECT COUNT(*) FROM player.item_operation_receipt WHERE operation_id=?",
                 {U::Blob(std::string(lost.operation.begin(), lost.operation.end()))}) == "0", "lost write left a receipt");
    Check(a->Run(rt::DestroyIntent{Id(5)}).outcome == W::Committed && !Find(Items(a->registry), 5) && a->MatchesDatabase(writer),
          "retry after a lost write");

    // Crash: core A vanishes without closing; core B loads exactly A's state.
    const auto beforeCrash = Items(a->registry);
    a.reset();
    auto b = std::make_unique<Core>(c, port, pw, lossyDirect, Lease(2, 200));
    Check(Items(b->registry) == beforeCrash && b->MatchesDatabase(writer), "state after crash differs");
    Check(b->Run(rt::ConsumeIntent{Id(4), 1}).outcome == W::Committed && b->MatchesDatabase(writer), "new core writes");
    Check(Scalar(admin, "SELECT SUM(destroyed) FROM player.item_instance WHERE owner_kind=1 AND owner_id=42") == "2", "tombstones kept");
}
} // namespace

std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("item-writer-tests");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("expected --completion-only or --owned-sandbox");
        const std::string_view mode = argv[1];
        Basics();
        LostAcknowledgements();
        Lifecycle();
        if (mode == "--owned-sandbox")
        {
            std::string portText, pw, token;
            std::getline(std::cin, portText);
            std::getline(std::cin, pw);
            std::getline(std::cin, token);
            Check(token.size() == 32, "owned token required");
            OwnedSandbox(static_cast<unsigned>(std::stoul(portText)), pw, token);
        }
        else Check(mode == "--completion-only", "unknown test mode");
        std::cout << "ItemWriterTests passed " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ItemWriterTests failed: " << error.what() << '\n';
        return 1;
    }
}
