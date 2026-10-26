#include "MissionResourceDrain.hpp"
#include <stdexcept>

namespace auriga::mission::persistence
{
ResourceDrain::ResourceDrain(const ResourceFence &fence, std::array<SqlQueuePosition, 2> baselines,
                             std::array<SqlQueueDrain, 2> tickets)
    : identity_(fence.Identity()), challenge_(fence.Challenge()), baselines_(baselines), tickets_(tickets)
{
    if (fence.Stage() != ResourceFenceStage::Draining || !baselines[0].lane || !baselines[1].lane ||
        baselines[0].lane == baselines[1].lane)
        throw std::invalid_argument("Resource drain requires two distinct verified SQL lanes");
    for (size_t lane = 0; lane < 2; ++lane)
    {
        const auto result = tickets[lane].Poll();
        if (result.position.lane != baselines[lane].lane ||
            result.position.sequence <= baselines[lane].sequence ||
            result.position.sequence != fence.Barriers()[lane])
            throw std::invalid_argument("Resource drain ticket/baseline/fence differs");
    }
}
ResourceDrainStatus ResourceDrain::Poll(ResourceFence &fence) const
{
    if (fence.Identity() != identity_ || fence.Challenge() != challenge_)
        return ResourceDrainStatus::Stale;
    for (size_t lane = 0; lane < 2; ++lane)
        if (fence.Barriers()[lane] != tickets_[lane].Sequence())
            return ResourceDrainStatus::Stale;
    if (fence.Stage() == ResourceFenceStage::Ready)
        return ResourceDrainStatus::Ready;
    if (fence.Stage() == ResourceFenceStage::Blocked)
        return ResourceDrainStatus::Blocked;
    if (fence.Stage() != ResourceFenceStage::Draining)
        return ResourceDrainStatus::Stale;
    for (size_t lane = 0; lane < 2; ++lane)
    {
        const auto result = tickets_[lane].Poll();
        if (result.status == SqlQueueDrainStatus::Pending)
            continue;
        const bool confirmed =
            result.status == SqlQueueDrainStatus::Complete && result.position.ConfirmsSince(baselines_[lane]);
        fence.LaneDrained(lane, tickets_[lane].Sequence(),
                          confirmed ? SqlTransactionOutcome::Committed : SqlTransactionOutcome::Failed);
        if (!confirmed)
            return ResourceDrainStatus::Blocked;
    }
    return fence.Stage() == ResourceFenceStage::Ready ? ResourceDrainStatus::Ready
                                                      : ResourceDrainStatus::Pending;
}
} // namespace auriga::mission::persistence
