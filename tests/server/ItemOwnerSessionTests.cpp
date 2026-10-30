// F4.2 (IC-034): durable item-owner sessions. --completion-only drives the
// GameServer session state machine with exact fabricated completions;
// --owned-sandbox runs provisioning, claims, two cores and late writes against
// a new owned MariaDB (see ItemOwnerSessionTests.py).
// length.h before AsyncSQL.h: the SQL header #defines QUERY_MAX_LEN, an EMisc name.
#include <common/length.h>
#include "PersistentItemFixture.hpp"
#include "CatalogSqlPopulate.hpp"
#include "../../SRC/Server/GameServer/item/ItemOwnerSessions.hpp"
#include "../../SRC/Server/GameServer/ecs/TypedItems.hpp"
#include <ItemPersistenceSql.hpp>
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <source_location>
#include <stdexcept>
#include <thread>

using namespace auriga::item;
namespace rt = auriga::item::runtime;
namespace p = auriga::item::persistence;
namespace f = persistent_item_fixture;
using E = ItemPersistenceError;
using S = rt::ItemSessionState;
using O = rt::ItemSessionOpen;
using U = SqlParam;
using Message = std::unique_ptr<SQLMsg>;
using Rows = std::vector<std::vector<std::optional<std::string>>>;

namespace
{
unsigned checks = 0;
void Check(bool good, const char* why, std::source_location at = std::source_location::current())
{
    ++checks;
    if (!good) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
constexpr ItemOwner Owner{ItemOwnerKind::Character, 42};
rt::ItemSessionLease Lease(std::uint64_t generation, std::uint64_t entity)
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
Message Completed(const SqlTransaction& job, const std::map<std::size_t, Rows>& rows)
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
Message Failed(const SqlTransaction& job, SqlTransactionOutcome outcome, std::size_t step, std::uint32_t error)
{
    auto m = std::make_unique<SQLMsg>();
    m->bPrepared = m->bTransaction = true;
    m->transaction = job;
    m->transactionOutcome = outcome;
    m->transactionFailedStep = step;
    m->uiSQLErrno = error;
    return m;
}
Message Claimed(const SqlTransaction& job, std::uint64_t generation, std::uint64_t revision)
{
    return Completed(job, {{4, {{std::to_string(generation), std::to_string(revision)}}}});
}
Message LoadedRows(const SqlTransaction& job, std::uint64_t generation, std::uint64_t revision)
{
    return Completed(job, {{3, {{std::to_string(generation), std::to_string(revision)}}},
                           {4, {{"1", "1", "5", "9", "2"}, {"2", "1", "8", "1", "1"}}},
                           {5, {{"1", "1", "1000"}}}});
}
std::vector<rt::ItemSessionEvent> Events(rt::ItemOwnerSessions& sessions) { return sessions.TakeEvents(); }
bool SameSteps(const SqlTransaction& a, const SqlTransaction& b)
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

void SqlCompletionBoundary(const ItemCatalog& c)
{
    // Claim: a missing owner is reported apart from a guard conflict.
    const auto claim = p::PrepareClaimItemOwner(c, Owner).value();
    Check(claim.size() == 5, "claim job is guards + increment + read");
    Check(p::ClaimItemOwnerCompleted(c, Owner, *Failed(claim, SqlTransactionOutcome::Conflict, 3, SqlTransactionConflict)).error() == E::MissingOwner,
          "missing owner not reported");
    Check(p::ClaimItemOwnerCompleted(c, Owner, *Failed(claim, SqlTransactionOutcome::Conflict, 1, SqlTransactionConflict)).error() == E::Conflict,
          "catalog pin conflict reported as missing owner");
    Check(p::ClaimItemOwnerCompleted(c, Owner, *Failed(claim, SqlTransactionOutcome::CommitUnknown, SqlTransactionNoStep, 2013)).error() == E::CommitUnknown,
          "unknown claim commit lost");
    const auto fence = p::ClaimItemOwnerCompleted(c, Owner, *Claimed(claim, 9, 4));
    Check(fence && *fence == ItemOwnerFence{Owner, 9, 4}, "claimed fence differs");
    Check(p::ClaimItemOwnerCompleted(c, Owner, *Claimed(claim, 1, 4)).error() == E::InvalidStoredData, "first generation after a claim accepted");
    Check(!p::ClaimItemOwnerCompleted(c, {ItemOwnerKind::Character, 43}, *Claimed(claim, 9, 4)), "claim of another owner accepted");
    auto tampered = Claimed(claim, 9, 4);
    tampered->transaction[3].params[1] = U::UInt(43);
    Check(p::ClaimItemOwnerCompleted(c, Owner, *tampered).error() == E::InvalidStoredData, "foreign job accepted as this claim");

    // Load: missing owner, stale and never-issued sessions.
    const auto load = p::PrepareLoadItemOwner(c, Owner, 9).value();
    Check(p::LoadItemOwnerCompleted(c, Owner, 9, *LoadedRows(load, 9, 4)).has_value(), "matching load rejected");
    Check(p::LoadItemOwnerCompleted(c, Owner, 9, *Completed(load, {{3, {}}})).error() == E::MissingOwner, "missing owner load");
    Check(p::LoadItemOwnerCompleted(c, Owner, 9, *LoadedRows(load, 10, 4)).error() == E::StaleSession, "stale session load");
    Check(p::LoadItemOwnerCompleted(c, Owner, 9, *LoadedRows(load, 8, 4)).error() == E::InvalidFence, "unissued session load");
    // The read does not depend on the session: the stored generation decides.
    Check(p::LoadItemOwnerCompleted(c, Owner, 8, *LoadedRows(load, 9, 4)).error() == E::StaleSession, "older session loaded");
    auto foreign = LoadedRows(load, 9, 4);
    foreign->transaction[3].params[1] = U::UInt(43);
    Check(p::LoadItemOwnerCompleted(c, Owner, 9, *foreign).error() == E::InvalidStoredData, "load job of another owner accepted");

    // Provision: validation before SQL; a duplicate owner is OwnerExists.
    const auto standard = rt::CharacterOwnerProvision(42, 1000);
    const auto provision = p::PrepareProvisionItemOwner(c, standard);
    Check(provision.has_value(), "standard character owner rejected");
    Check(p::ProvisionItemOwnerCompleted(c, standard, *Failed(*provision, SqlTransactionOutcome::RolledBack, 3, 1062)).error() == E::OwnerExists,
          "existing owner not reported");
    Check(p::ProvisionItemOwnerCompleted(c, standard, *Failed(*provision, SqlTransactionOutcome::RolledBack, 4, 1062)).error() == E::SqlFailure,
          "duplicate outside the fence reported as an existing owner");
    Check(*p::ProvisionItemOwnerCompleted(c, standard, *Completed(*provision, {})) == ItemOwnerFence{Owner, 1, 1}, "provisioned fence differs");
    auto bad = standard;
    bad.storages[1].rows = 2;
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidStorage, "multi-row equipment accepted");
    bad = standard;
    bad.storages.push_back(bad.storages[0]);
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidStorage, "duplicate storage accepted");
    bad = standard;
    bad.storages[0].storage.owner.id = 43;
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidStorage, "foreign storage accepted");
    bad = standard;
    bad.storages.clear();
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidStorage, "owner without storage accepted");
    bad = standard;
    bad.wallets[0].currency = WalletCurrency::DragonCoin;
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidWallet, "account currency on a character accepted");
    bad = standard;
    bad.wallets[0].revision = 2;
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidWallet, "wallet history invented");
    bad = standard;
    bad.wallets[0].balance = static_cast<std::uint64_t>(INT64_MAX) + 1;
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidWallet, "unbounded balance accepted");
    bad = standard;
    bad.owner.id = 0;
    Check(p::PrepareProvisionItemOwner(c, bad).error() == E::InvalidOwner, "owner zero accepted");

    const auto& inventory = standard.storages[0];
    Check(inventory.storage.kind == ItemStorageKind::Inventory && inventory.columns == INVENTORY_PAGE_COLUMN &&
          inventory.rows == INVENTORY_PAGE_ROW && inventory.pages == INVENTORY_PAGE_COUNT, "inventory grid differs from the game");
    Check(standard.storages[1].storage.kind == ItemStorageKind::Equipment && standard.storages[1].columns == 8, "equipment slots differ");
}

void SessionStateMachine(std::shared_ptr<const ItemCatalog> c)
{
    FakeExecutor sql;
    rt::ItemOwnerSessions sessions(c, sql);
    const auto a = Lease(1, 100), b = Lease(2, 200), d = Lease(3, 300);

    Check(sessions.Open({ItemOwnerKind::Character, 43}, a) == O::Invalid, "lease of another character accepted");
    Check(sessions.Open(Owner, Lease(0, 100)) == O::Invalid, "unversioned lease accepted");
    Check(sessions.Open(Owner, Lease(1, 0)) == O::Invalid, "lease without entity accepted");

    // Open -> claim -> load -> Active.
    Check(sessions.Open(Owner, a) == O::Started && sessions.State(Owner) == S::Claiming && sql.jobs.size() == 1, "claim not started");
    Check(sessions.Open(Owner, a) == O::AlreadyOpen && sessions.Open(Owner, b) == O::Busy, "second session admitted");
    auto [claimTicket, claimJob] = sql.jobs.back();
    Check(SameSteps(claimJob, p::PrepareClaimItemOwner(*c, Owner).value()), "claim job differs");
    sessions.Complete(claimTicket + 1000, *Claimed(claimJob, 5, 3));
    Check(sessions.State(Owner) == S::Claiming, "unknown ticket changed the session");
    sessions.Complete(claimTicket, *Claimed(claimJob, 5, 3));
    Check(sessions.State(Owner) == S::Loading && sql.jobs.size() == 2 && SameSteps(sql.jobs.back().second, p::PrepareLoadItemOwner(*c, Owner, 5).value()),
          "load of the claimed generation not started");
    Check(!sessions.Loaded(Owner, a) && !sessions.Fence(Owner, a) && !sessions.BeginWrite(Owner, a), "unloaded session admitted");
    sessions.Complete(claimTicket, *Claimed(claimJob, 5, 3));
    Check(sessions.State(Owner) == S::Loading, "replayed claim completion applied twice");
    auto [loadTicket, loadJob] = sql.jobs.back();
    sessions.Complete(loadTicket, *LoadedRows(loadJob, 5, 3));
    Check(sessions.State(Owner) == S::Active && sessions.Fence(Owner, a) == ItemOwnerFence{Owner, 5, 3}, "session not active");
    const auto* loaded = sessions.Loaded(Owner, a);
    Check(loaded && loaded->storages.size() == 2 && loaded->wallets.size() == 1 && loaded->wallets[0].balance == 1000, "snapshot lost");
    Check(!sessions.Loaded(Owner, b) && !sessions.Fence(Owner, b) && !sessions.BeginWrite(Owner, b), "other lease reads this session");
    Check(Events(sessions) == std::vector<rt::ItemSessionEvent>{{Owner, a, S::Active, {}}}, "activation event differs");

    // Writes move the fence of this session only.
    Check(sessions.BeginWrite(Owner, a), "write not admitted");
    sessions.EndWrite(Owner, a, ItemOwnerFence{Owner, 4, 9});
    Check(sessions.Fence(Owner, a)->revision == 3, "fence of another session accepted");
    Check(sessions.BeginWrite(Owner, a), "write not admitted");
    sessions.EndWrite(Owner, a, ItemOwnerFence{Owner, 5, 4});
    Check(sessions.Fence(Owner, a)->revision == 4, "committed fence not applied");

    // Close with a write in flight; a new login queues behind the drain.
    Check(sessions.BeginWrite(Owner, a), "write not admitted");
    sessions.Close(Owner, b);
    Check(sessions.State(Owner) == S::Active, "other lease closed this session");
    sessions.Close(Owner, a);
    Check(sessions.State(Owner) == S::Closing && !sessions.Loaded(Owner, a) && !sessions.BeginWrite(Owner, a), "closing session still admits");
    Check(sessions.Open(Owner, b) == O::Queued && sql.jobs.size() == 2, "queued login claimed before the drain");
    sessions.EndWrite(Owner, a, std::nullopt);
    Check(Events(sessions) == std::vector<rt::ItemSessionEvent>{{Owner, a, S::Closed, {}}}, "close event differs");
    Check(sessions.State(Owner) == S::Claiming && sql.jobs.size() == 3, "queued login did not claim after the drain");
    Check(sessions.Open(Owner, b) == O::AlreadyOpen, "queued lease not current");

    // A newer session elsewhere: the load sees a newer generation.
    sessions.Complete(sql.jobs.back().first, *Claimed(sql.jobs.back().second, 6, 4));
    sessions.Complete(sql.jobs.back().first, *LoadedRows(sql.jobs.back().second, 7, 4));
    Check(sessions.State(Owner) == S::Superseded && Events(sessions) == std::vector<rt::ItemSessionEvent>{{Owner, b, S::Superseded, E::StaleSession}},
          "stale load not reported as superseded");
    Check(!sessions.Loaded(Owner, b) && !sessions.BeginWrite(Owner, b), "superseded session admitted");
    sessions.Close(Owner, b);
    Check(sessions.State(Owner) == S::Closed && sessions.Tracked() == 0, "superseded session not forgotten");
    Events(sessions);

    // Close while claiming: the late claim result is dropped, nothing activates.
    Check(sessions.Open(Owner, d) == O::Started, "reopen after close failed");
    const auto [lateTicket, lateJob] = sql.jobs.back();
    sessions.Close(Owner, d);
    Check(sessions.State(Owner) == S::Closing, "close did not wait for the claim");
    sessions.Complete(lateTicket, *Claimed(lateJob, 8, 4));
    Check(sessions.State(Owner) == S::Closed && sql.jobs.back().first == lateTicket, "late claim activated or loaded");
    Check(Events(sessions) == std::vector<rt::ItemSessionEvent>{{Owner, d, S::Closed, {}}}, "late claim produced events");

    // Failures.
    Check(sessions.Open(Owner, a) == O::Started, "reopen failed");
    sessions.Complete(sql.jobs.back().first, *Failed(sql.jobs.back().second, SqlTransactionOutcome::Conflict, 3, SqlTransactionConflict));
    Check(sessions.State(Owner) == S::Failed && Events(sessions) == std::vector<rt::ItemSessionEvent>{{Owner, a, S::Failed, E::MissingOwner}},
          "missing owner not reported");
    Check(sessions.Open(Owner, b) == O::Started, "failed session blocked a new login");
    auto forged = Claimed(sql.jobs.back().second, 9, 4);
    forged->transaction[4].params[1] = U::UInt(43);
    sessions.Complete(sql.jobs.back().first, *forged);
    Check(sessions.State(Owner) == S::Failed && Events(sessions).back().error == E::InvalidStoredData, "forged completion accepted");
    sessions.Close(Owner, b);
    sql.refuse = true;
    Check(sessions.Open(Owner, a) == O::SubmitFailed && sessions.Tracked() == 0, "refused job left a session");
}

// --- owned MariaDB -------------------------------------------------------
Message Query(CAsyncSQL& db, const std::string& query, const SqlParams& params = {})
{
    Message m(db.DirectQueryPrepared(query.c_str(), params));
    if (m && m->uiSQLErrno) std::cerr << "SQL fixture error=" << m->uiSQLErrno << " query=" << query << '\n';
    Check(m && !m->uiSQLErrno && m->Get(), "isolated query failed");
    return m;
}
std::string Scalar(CAsyncSQL& db, const std::string& query, const SqlParams& params = {})
{
    auto m = Query(db, query, params);
    auto row = m->Get()->FetchRow();
    auto lengths = m->Get()->FetchLengths();
    Check(row && row[0] && lengths, "scalar missing");
    return {row[0], lengths[0]};
}
void Raw(CAsyncSQL& db, const char* query)
{
    Message m(db.DirectQuery(query));
    Check(m && !m->uiSQLErrno, "raw SQL failed");
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
// Pumps until the session leaves Claiming/Loading/Closing.
void Settle(rt::AsyncItemSqlExecutor& executor, rt::ItemOwnerSessions& sessions)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (true)
    {
        executor.Pump(sessions);
        const auto state = sessions.State(Owner);
        if (state != S::Claiming && state != S::Loading && state != S::Closing) return;
        Check(std::chrono::steady_clock::now() < deadline, "session did not settle");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
ItemCommit GrantWith(const ItemCatalog& c, ItemOwnerFence fence, std::vector<ItemStorageLayout> storages, std::uint8_t op, std::vector<PersistentItem> items)
{
    auto r = f::Grant(c, std::move(items));
    r.operationId = {};
    r.operationId[0] = op;
    r.operationId[15] = 0x42;
    r.owners = {fence};
    r.storages = std::move(storages);
    return r;
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
// F4.3 gate on real SQL: database load -> ECS entities -> persistence model.
void EcsRoundTrip(const ItemCatalog& c, const p::LoadedItemOwner& loaded, std::size_t items)
{
    auto canonical = loaded;
    std::ranges::sort(canonical.storages, {}, [](const auto& l) { return l.storage; });
    std::ranges::sort(canonical.wallets, {}, &ItemWallet::currency);
    entt::registry registry;
    const auto owner = ecs::item::Materialize(registry, c, loaded, entt::null);
    Check(owner && ecs::item::Verify(registry, c, *owner).has_value(), "loaded owner not materialized");
    const auto projected = ecs::item::Project(registry, *owner);
    Check(projected && projected->fence == canonical.fence && projected->storages == canonical.storages &&
          projected->wallets == canonical.wallets && projected->items == canonical.items && projected->items.size() == items,
          "database load -> ECS -> model differs");
}
std::string Items(CAsyncSQL& db)
{
    return Scalar(db, "SELECT COUNT(*) FROM player.item_instance WHERE owner_kind=1 AND owner_id=42");
}

void OwnedSandbox(unsigned port, const std::string& pw, const std::string& token)
{
    CAsyncSQL admin, writer;
    Direct(admin, port, pw, "root");
    Check(Scalar(admin, "SELECT token FROM protos.ic_session_owner") == token, "owned sandbox identity");
    Direct(writer, port, pw, "item_writer");
    const auto c = BuildItemCatalog(f::Catalog()).value();
    catalog_sql_populate::Populate(admin, f::Catalog());
    catalog_sql_populate::Pin(admin, c->Revision(), c->ContentHash());

    // Not provisioned: neither a claim nor a load invents an owner.
    Check(p::ClaimItemOwner(writer, *c, Owner).error() == E::MissingOwner, "claim invented an owner");
    Check(p::LoadItemOwner(writer, *c, Owner, 1).error() == E::MissingOwner, "load of a missing owner");
    Check(Scalar(admin, "SELECT COUNT(*) FROM player.item_owner_fence") == "0", "missing owner left a fence");

    // Provision once; a second attempt changes nothing.
    const auto standard = rt::CharacterOwnerProvision(42, 1000);
    Check(p::ProvisionItemOwner(writer, *c, standard) == ItemOwnerFence{Owner, 1, 1}, "standard owner not provisioned");
    auto again = standard;
    again.wallets[0].balance = 5;
    Check(p::ProvisionItemOwner(writer, *c, again).error() == E::OwnerExists, "second provisioning not refused");
    Check(Scalar(admin, "SELECT CONCAT(f.session_generation,':',f.state_revision,':',w.balance,':',(SELECT COUNT(*) FROM player.item_storage s WHERE s.owner_kind=1 AND s.owner_id=42)) "
                        "FROM player.item_owner_fence f JOIN player.item_wallet w ON w.owner_kind=f.owner_kind AND w.owner_id=f.owner_id WHERE f.owner_kind=1 AND f.owner_id=42") == "1:1:1000:2",
          "refused provisioning changed the owner");
    const auto fresh = p::LoadItemOwner(writer, *c, Owner, 1);
    Check(fresh && fresh->items.empty() && fresh->storages.size() == 2 && fresh->wallets.size() == 1 && fresh->wallets[0].balance == 1000,
          "provisioned owner does not load");
    Check(p::LoadItemOwner(writer, *c, Owner, 2).error() == E::InvalidFence, "never-issued session loaded");
    const auto inventory = *std::ranges::find(fresh->storages, ItemStorageKind::Inventory, [](const auto& l) { return l.storage.kind; });

    // Core A claims, loads and writes.
    CAsyncSQL sqlA, sqlB;
    Async(sqlA, port, pw);
    Async(sqlB, port, pw);
    rt::AsyncItemSqlExecutor execA(sqlA), execB(sqlB);
    rt::ItemOwnerSessions coreA(c, execA), coreB(c, execB);
    const auto leaseA = Lease(11, 1001), leaseB = Lease(12, 2002);
    Check(coreA.Open(Owner, leaseA) == O::Started, "core A did not start");
    Settle(execA, coreA);
    Check(coreA.State(Owner) == S::Active && coreA.Fence(Owner, leaseA) == ItemOwnerFence{Owner, 2, 1}, "core A not active on generation 2");
    Check(coreA.TakeEvents().size() == 1, "core A events");
    Check(coreA.BeginWrite(Owner, leaseA), "core A write not admitted");
    const auto grant = GrantWith(*c, *coreA.Fence(Owner, leaseA), fresh->storages, 1, {f::Sword(), f::Leather(), Bracelet()});
    Check(p::CommitItems(writer, *c, grant).has_value(), "core A grant failed");
    coreA.EndWrite(Owner, leaseA, ItemOwnerFence{Owner, 2, 2});
    Check(coreA.Fence(Owner, leaseA)->revision == 2 && Items(admin) == "3", "core A grant not recorded");

    // Core B takes the character over and sees A's committed state.
    Check(coreB.Open(Owner, leaseB) == O::Started, "core B did not start");
    Settle(execB, coreB);
    Check(coreB.State(Owner) == S::Active && coreB.Fence(Owner, leaseB) == ItemOwnerFence{Owner, 3, 2}, "core B not active on generation 3");
    Check(coreB.Loaded(Owner, leaseB)->items.size() == 3, "core B did not see core A's grant");
    EcsRoundTrip(*c, *coreB.Loaded(Owner, leaseB), 3);

    // Late write and reload of A are refused by the database, not by A's memory.
    Check(coreA.State(Owner) == S::Active, "core A learned about B without SQL");
    const auto late = GrantWith(*c, *coreA.Fence(Owner, leaseA), {inventory}, 2, {f::Leather(7)});
    const auto refused = p::CommitItems(writer, *c, late);
    Check(!refused && refused.error() == E::Conflict && Items(admin) == "3", "late write of the old session committed");
    Check(p::LoadItemOwner(writer, *c, Owner, 2).error() == E::StaleSession, "old session reloaded");
    coreA.Superseded(Owner, leaseA);
    Check(coreA.State(Owner) == S::Superseded && !coreA.BeginWrite(Owner, leaseA), "superseded core still admits");
    coreA.Close(Owner, leaseA);
    Check(coreA.State(Owner) == S::Closed, "superseded core not closed");

    // A claim waits for an in-flight writer and then sees its commit.
    CAsyncSQL blocker, claimer;
    Direct(blocker, port, pw, "item_writer");
    Direct(claimer, port, pw, "item_writer");
    Raw(blocker, "START TRANSACTION");
    Raw(blocker, "UPDATE player.item_owner_fence SET state_revision=state_revision+1 WHERE owner_kind=1 AND owner_id=42");
    auto pending = std::async(std::launch::async, [&] { return p::ClaimItemOwner(claimer, *c, Owner); });
    Check(pending.wait_for(std::chrono::milliseconds(400)) == std::future_status::timeout, "claim did not wait for the writer");
    Raw(blocker, "COMMIT");
    const auto waited = pending.get();
    Check(waited && *waited == ItemOwnerFence{Owner, 4, 3}, "claim did not see the in-flight writer's commit");
    const auto oldB = GrantWith(*c, *coreB.Fence(Owner, leaseB), {inventory}, 3, {f::Leather(8)});
    Check(p::CommitItems(writer, *c, oldB).error() == E::Conflict, "write of core B accepted after a newer claim");

    // Same core: logout with a write in flight, fast relog queued behind it.
    const auto leaseA2 = Lease(13, 1002), leaseA3 = Lease(14, 1003);
    Check(coreA.Open(Owner, leaseA2) == O::Started, "relog did not start");
    Settle(execA, coreA);
    Check(coreA.Fence(Owner, leaseA2) == ItemOwnerFence{Owner, 5, 3}, "relog generation differs");
    Check(coreA.BeginWrite(Owner, leaseA2), "relog write not admitted");
    coreA.Close(Owner, leaseA2);
    Check(coreA.Open(Owner, leaseA3) == O::Queued, "fast relog not queued");
    coreA.EndWrite(Owner, leaseA2, std::nullopt);
    Settle(execA, coreA);
    Check(coreA.State(Owner) == S::Active && coreA.Fence(Owner, leaseA3) == ItemOwnerFence{Owner, 6, 3} &&
          coreA.Loaded(Owner, leaseA3)->items.size() == 3, "queued relog not active after the drain");
    EcsRoundTrip(*c, *coreA.Loaded(Owner, leaseA3), 3);
    Check(Scalar(admin, "SELECT session_generation FROM player.item_owner_fence WHERE owner_kind=1 AND owner_id=42") == "6", "durable generation differs");

    // The executor needs an asynchronous connection.
    rt::AsyncItemSqlExecutor direct(writer);
    rt::ItemOwnerSessions misconfigured(c, direct);
    Check(misconfigured.Open(Owner, Lease(15, 1004)) == O::SubmitFailed && misconfigured.Tracked() == 0, "direct connection accepted async jobs");
}
} // namespace

std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("item-owner-session-tests");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("expected --completion-only or --owned-sandbox");
        const std::string_view mode = argv[1];
        const auto catalog = BuildItemCatalog(f::Catalog()).value();
        SqlCompletionBoundary(*catalog);
        SessionStateMachine(catalog);
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
        std::cout << "ItemOwnerSessionTests passed " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ItemOwnerSessionTests failed: " << error.what() << '\n';
        return 1;
    }
}
