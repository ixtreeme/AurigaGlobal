#include "stdafx.h"
#include "player_cache_drain.h"
#include <algorithm>

namespace auriga::player_cache
{
std::optional<Drain::Stamp> Drain::Inspect(View view, uint32_t character, bool clean)
{
    if (!view.player || view.items.size() > MaximumItems || view.player->Get(false)->id != character ||
        (clean && view.player->HasPendingQuery()))
        return {};
    Stamp stamp{view.player->Version(), {}};
    stamp.items.reserve(view.items.size());
    for (auto *item : view.items)
    {
        if (!item || !item->Get(false)->id || item->Get(false)->owner != character ||
            (clean && item->HasPendingQuery()))
            return {};
        stamp.items.push_back({item->Get(false)->id, item->Version()});
    }
    std::ranges::sort(stamp.items, {}, &ItemStamp::id);
    for (size_t i = 1; i < stamp.items.size(); ++i)
        if (stamp.items[i - 1].id == stamp.items[i].id)
            return {};
    return stamp;
}
std::optional<Drain> Drain::Begin(const player_session::Authority &authority, player_session::Lease lease,
                                  player_session::Token challenge, View view,
                                  std::array<SqlQueuePosition, 2> origins, const Enqueue &enqueue)
{
    // Origins are process/worker origins, never a fresh position that masks an old error.
    if (!authority.Loaded(lease) || !lease.key.Versioned() || !player_session::Nonzero(challenge) ||
        !enqueue || !Inspect(view, lease.key.character, false))
        return {};
    for (const auto &origin : origins)
        if (!origin.lane || origin.sequence || origin.unconfirmed || origin.lastUnconfirmedSequence ||
            origin.lastSqlError)
            return {};
    if (origins[0].lane == origins[1].lane)
        return {};
    view.player->Flush();
    for (auto *item : view.items)
        item->Flush();
    Drain drain;
    drain.lease_ = lease;
    drain.challenge_ = challenge;
    drain.origins_ = origins;
    drain.tickets_ = enqueue();
    auto stamp = Inspect(view, lease.key.character, true);
    if (!stamp)
    {
        drain.status_ = Status::Blocked;
        drain.failure_ = Failure::CacheChanged;
    }
    else
        drain.stamp_ = std::move(*stamp);
    for (size_t lane = 0; lane < 2; ++lane)
    {
        const auto ticket = drain.tickets_[lane].Poll();
        if (ticket.position.lane != origins[lane].lane || !ticket.position.sequence)
        {
            drain.status_ = Status::Blocked;
            drain.failure_ = Failure::InvalidLanes;
        }
    }
    return drain;
}
Receipt Drain::Poll(const player_session::Authority &authority, View current)
{
    Receipt result{lease_, challenge_};
    for (size_t lane = 0; lane < 2; ++lane)
        result.lanes[lane] = tickets_[lane].Poll();
    if (!authority.Loaded(lease_))
        return {lease_, challenge_, Status::Stale, Failure::None, result.lanes};
    const auto stamp = Inspect(current, lease_.key.character, true);
    if (status_ != Status::Blocked && (!stamp || *stamp != stamp_))
    {
        status_ = Status::Blocked;
        failure_ = Failure::CacheChanged;
    }
    if (status_ != Status::Blocked)
    {
        bool complete = true;
        for (size_t lane = 0; lane < 2; ++lane)
        {
            const auto &ticket = result.lanes[lane];
            if (ticket.status == SqlQueueDrainStatus::Pending)
            {
                complete = false;
                continue;
            }
            if (ticket.status != SqlQueueDrainStatus::Complete ||
                !ticket.position.ConfirmsSince(origins_[lane]))
            {
                status_ = Status::Blocked;
                failure_ = ticket.status == SqlQueueDrainStatus::Cancelled ? Failure::Cancelled
                                                                           : Failure::SqlUnconfirmed;
                break;
            }
        }
        if (status_ != Status::Blocked && complete)
            status_ = Status::Flushed;
    }
    result.status = status_;
    result.failure = failure_;
    return result;
}
} // namespace auriga::player_cache
