#include "ItemWriter.hpp"
#include "../ecs/TypedItems.hpp"
#include <Core/player_session.h>
#include <SQL/AsyncSQL.h>
#include <stdexcept>
#include <utility>

namespace auriga::item::runtime
{
namespace p = auriga::item::persistence;
using E = ItemPersistenceError;
using O = ItemWriteOutcome;

ItemOperationId RandomOperationId()
{
    return player_session::NewToken();
}

ItemWriter::ItemWriter(std::shared_ptr<const ItemCatalog> catalog, ItemOwnerSessions& sessions, entt::registry& registry,
                       ItemSqlExecutor& executor, OperationIds operationIds)
    : catalog_(std::move(catalog)), sessions_(sessions), registry_(registry), executor_(executor), operationIds_(std::move(operationIds))
{
    if (!catalog_ || !operationIds_) throw std::invalid_argument("item writer needs a catalog and operation ids");
}

std::optional<std::uint64_t> ItemWriter::Request(ItemOwner owner, const ItemSessionLease& lease, ItemIntent intent)
{
    if (!sessions_.Fence(owner, lease) || ecs::item::FindOwner(registry_, owner) == entt::null) return std::nullopt;
    auto found = lanes_.find(owner);
    if (found != lanes_.end() && found->second.lease != lease)
    {
        // The old session's lane still has work in SQL: let it finish first.
        if (found->second.phase != Phase::Idle || !found->second.queue.empty()) return std::nullopt;
        lanes_.erase(found);
        found = lanes_.end();
    }
    if (found == lanes_.end()) found = lanes_.emplace(owner, Lane{lease}).first;
    if (found->second.closing) return std::nullopt;
    const auto id = nextRequest_++;
    found->second.queue.push_back({id, std::move(intent)});
    return id;
}

void ItemWriter::Pump()
{
    for (auto it = lanes_.begin(); it != lanes_.end();)
    {
        auto& lane = it->second;
        while (lane.phase == Phase::Idle && !lane.queue.empty())
            if (!Start(it->first, lane)) break;
        if (lane.phase == Phase::Idle && lane.queue.empty() && lane.closing) it = lanes_.erase(it);
        else ++it;
    }
}

std::optional<ItemInstanceId> ItemWriter::TakeId()
{
    while (!ids_.empty() && !ids_.front().count) ids_.pop_front();
    if (ids_.empty()) return std::nullopt;
    auto& range = ids_.front();
    const auto id = ItemInstanceId::Create(range.first);
    ++range.first;
    --range.count;
    return id ? std::optional{*id} : std::nullopt;
}

std::uint64_t ItemWriter::AvailableIds() const
{
    std::uint64_t total = 0;
    for (const auto& range : ids_) total += range.count;
    return total;
}

// Returns false when the lane cannot continue in this Pump.
bool ItemWriter::Start(ItemOwner owner, Lane& lane)
{
    const auto fence = sessions_.Fence(owner, lane.lease);
    if (!fence)
    {
        Abandon(owner, lane, sessions_.State(owner) == ItemSessionState::Superseded ? O::Superseded : O::Cancelled);
        return false;
    }
    const auto entity = ecs::item::FindOwner(registry_, owner);
    const auto loaded = entity == entt::null ? std::nullopt : std::optional{ecs::item::Project(registry_, entity)};
    if (!loaded || !*loaded || (*loaded)->fence != *fence)
    {
        sessions_.Superseded(owner, lane.lease);
        Abandon(owner, lane, O::Diverged);
        return false;
    }
    auto pending = std::move(lane.queue.front());
    lane.queue.pop_front();
    // A split needs one fresh instance id: reserve a block first.
    if (std::holds_alternative<SplitIntent>(pending.intent) && !AvailableIds())
    {
        lane.queue.push_front(std::move(pending));
        auto job = p::PrepareReserveItemInstanceIds(*catalog_, IdBlock);
        if (!job || !Submit(owner, lane, std::move(*job), Phase::Reserving))
        {
            auto failed = std::move(lane.queue.front());
            lane.queue.pop_front();
            Report(owner, lane, failed, O::NotCommitted, E::SqlFailure);
            return true;
        }
        return false;
    }

    const auto& state = **loaded;
    const ItemOwnerView view{state.fence, state.storages, state.items};
    const auto op = operationIds_();
    std::expected<ItemCommit, E> commit = std::unexpected(E::InvalidOperation);
    std::visit(
        [&](const auto& intent) {
            using T = std::decay_t<decltype(intent)>;
            if constexpr (std::is_same_v<T, MoveIntent>) commit = MakeMoveCommit(*catalog_, view, op, intent.item, intent.to);
            else if constexpr (std::is_same_v<T, MergeIntent>) commit = MakeMergeCommit(*catalog_, view, op, intent.source, intent.target, intent.count);
            else if constexpr (std::is_same_v<T, SplitIntent>)
            {
                if (const auto created = TakeId()) commit = MakeSplitCommit(*catalog_, view, op, intent.source, intent.count, intent.to, *created);
            }
            else if constexpr (std::is_same_v<T, DestroyIntent>) commit = MakeDestroyCommit(*catalog_, view, op, intent.item);
            else commit = MakeConsumeCommit(*catalog_, view, op, intent.item, intent.count);
        },
        pending.intent);
    if (!commit)
    {
        Report(owner, lane, pending, O::Rejected, commit.error(), op);
        return true;
    }
    auto job = p::PrepareItemWrite(*catalog_, *commit);
    if (!job)
    {
        Report(owner, lane, pending, O::Rejected, job.error(), op);
        return true;
    }
    if (!sessions_.BeginWrite(owner, lane.lease))
    {
        lane.queue.push_front(std::move(pending));
        Abandon(owner, lane, O::Cancelled);
        return false;
    }
    lane.current = std::move(pending);
    lane.commit = std::move(*commit);
    lane.attempts = 0;
    if (!Submit(owner, lane, std::move(*job), Phase::Writing))
    {
        sessions_.EndWrite(owner, lane.lease, std::nullopt);
        Finish(owner, lane, O::NotCommitted, E::SqlFailure);
        return true;
    }
    return false;
}

bool ItemWriter::Submit(ItemOwner owner, Lane& lane, SqlTransaction job, Phase phase)
{
    const auto ticket = nextTicket_++;
    if (!executor_.Submit(ticket, std::move(job))) return false;
    tickets_.emplace(ticket, owner);
    lane.ticket = ticket;
    lane.phase = phase;
    return true;
}

void ItemWriter::Complete(std::uint64_t ticket, _SQLMsg& message)
{
    const auto known = tickets_.find(ticket);
    if (known == tickets_.end()) return;
    const auto owner = known->second;
    tickets_.erase(known);
    const auto found = lanes_.find(owner);
    if (found == lanes_.end() || found->second.ticket != ticket) return;
    auto& lane = found->second;
    lane.ticket = 0;
    const auto phase = std::exchange(lane.phase, Phase::Idle);

    switch (phase)
    {
    case Phase::Reserving:
    {
        const auto range = p::ReserveItemInstanceIdsCompleted(*catalog_, IdBlock, message);
        if (range) ids_.push_back(*range);
        else if (!lane.queue.empty())
        {
            auto failed = std::move(lane.queue.front());
            lane.queue.pop_front();
            Report(owner, lane, failed, O::NotCommitted, range.error());
        }
        return;
    }
    case Phase::Writing:
    {
        const auto written = p::ItemWriteCompleted(*catalog_, *lane.commit, message);
        if (written) return Committed(owner, lane);
        if (written.error() == E::CommitUnknown)
        {
            // Never resend: decide by the receipt after the original finished.
            auto job = p::PrepareReconcileItemCommit(*catalog_, *lane.commit);
            if (job && Submit(owner, lane, std::move(*job), Phase::Reconciling)) return;
            sessions_.EndWrite(owner, lane.lease, std::nullopt);
            sessions_.Superseded(owner, lane.lease);
            return Finish(owner, lane, O::Unresolved, E::CommitUnknown);
        }
        sessions_.EndWrite(owner, lane.lease, std::nullopt);
        if (written.error() == E::Conflict)
        {
            // The fence or a "before" row moved: another session owns it now,
            // or this state is stale. Either way only a fresh load may continue.
            sessions_.Superseded(owner, lane.lease);
            Finish(owner, lane, O::Superseded, E::Conflict);
            return Abandon(owner, lane, O::Superseded);
        }
        return Finish(owner, lane, O::NotCommitted, written.error());
    }
    case Phase::Reconciling:
    {
        const auto receipt = p::ReconcileItemCommitCompleted(*catalog_, *lane.commit, message);
        if (receipt && *receipt) return Committed(owner, lane);
        if (receipt)
        {
            sessions_.EndWrite(owner, lane.lease, std::nullopt);
            return Finish(owner, lane, O::NotCommitted, E::CommitUnknown);
        }
        if (receipt.error() != E::ReceiptMismatch && ++lane.attempts < ReconcileAttempts)
        {
            auto job = p::PrepareReconcileItemCommit(*catalog_, *lane.commit);
            if (job && Submit(owner, lane, std::move(*job), Phase::Reconciling)) return;
        }
        sessions_.EndWrite(owner, lane.lease, std::nullopt);
        sessions_.Superseded(owner, lane.lease);
        Finish(owner, lane, receipt.error() == E::ReceiptMismatch ? O::Diverged : O::Unresolved, receipt.error());
        return Abandon(owner, lane, O::Superseded);
    }
    case Phase::Idle:
        return;
    }
}

void ItemWriter::Committed(ItemOwner owner, Lane& lane)
{
    const auto& commit = *lane.commit;
    const auto entity = ecs::item::FindOwner(registry_, owner);
    const bool applied = entity != entt::null && ecs::item::Apply(registry_, *catalog_, entity, commit).has_value();
    if (!applied)
    {
        // Durable, but this ECS state cannot follow it: reload from SQL.
        sessions_.EndWrite(owner, lane.lease, std::nullopt);
        sessions_.Superseded(owner, lane.lease);
        Finish(owner, lane, O::Diverged, std::nullopt);
        return Abandon(owner, lane, O::Superseded);
    }
    auto fence = commit.owners.front();
    ++fence.revision;
    sessions_.EndWrite(owner, lane.lease, fence);
    Finish(owner, lane, O::Committed, std::nullopt);
}

void ItemWriter::Finish(ItemOwner owner, Lane& lane, ItemWriteOutcome outcome, std::optional<ItemPersistenceError> error)
{
    if (lane.current) Report(owner, lane, *lane.current, outcome, error, lane.commit ? lane.commit->operationId : ItemOperationId{});
    lane.current.reset();
    lane.commit.reset();
}

void ItemWriter::Abandon(ItemOwner owner, Lane& lane, ItemWriteOutcome outcome)
{
    while (!lane.queue.empty())
    {
        Report(owner, lane, lane.queue.front(), outcome, std::nullopt);
        lane.queue.pop_front();
    }
    lane.closing = true;
}

void ItemWriter::Close(ItemOwner owner, const ItemSessionLease& lease)
{
    const auto found = lanes_.find(owner);
    if (found == lanes_.end() || found->second.lease != lease) return;
    Abandon(owner, found->second, O::Cancelled);
    if (found->second.phase == Phase::Idle) lanes_.erase(found);
}

void ItemWriter::Report(ItemOwner owner, const Lane& lane, const Pending& pending, ItemWriteOutcome outcome,
                        std::optional<ItemPersistenceError> error, ItemOperationId operation)
{
    results_.push_back({pending.id, owner, lane.lease, outcome, error, operation});
}

std::vector<ItemWriteResult> ItemWriter::TakeResults()
{
    return std::exchange(results_, {});
}

std::size_t ItemWriter::Queued(ItemOwner owner) const
{
    const auto found = lanes_.find(owner);
    return found == lanes_.end() ? 0 : found->second.queue.size();
}

bool ItemWriter::Busy(ItemOwner owner) const
{
    const auto found = lanes_.find(owner);
    return found != lanes_.end() && found->second.phase != Phase::Idle;
}
} // namespace auriga::item::runtime
