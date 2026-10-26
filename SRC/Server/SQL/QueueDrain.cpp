#include "QueueDrain.h"
#include <limits>
#include <mutex>

bool SqlQueuePosition::ConfirmsSince(const SqlQueuePosition &baseline) const
{
    return lane && lane == baseline.lane && sequence > baseline.sequence &&
           sequence < std::numeric_limits<uint64_t>::max() &&
           unconfirmed < std::numeric_limits<uint64_t>::max() && unconfirmed == baseline.unconfirmed &&
           lastUnconfirmedSequence == baseline.lastUnconfirmedSequence &&
           lastSqlError == baseline.lastSqlError;
}
struct SqlQueueDrain::State
{
    mutable std::mutex mutex;
    SqlQueueDrainResult result{SqlQueueDrainStatus::Pending, {}};
};
SqlQueueDrain::SqlQueueDrain(uint64_t lane) : state_(std::make_shared<State>())
{
    state_->result.position.lane = lane;
}
uint64_t SqlQueueDrain::Sequence() const
{
    return Poll().position.sequence;
}
SqlQueueDrainResult SqlQueueDrain::Poll() const
{
    if (!state_)
        return {};
    std::lock_guard lock(state_->mutex);
    return state_->result;
}
void SqlQueueDrain::Sequence(uint64_t sequence)
{
    std::lock_guard lock(state_->mutex);
    state_->result.position.sequence = sequence;
}
void SqlQueueDrain::Complete(SqlQueuePosition position)
{
    std::lock_guard lock(state_->mutex);
    if (state_->result.status != SqlQueueDrainStatus::Pending)
        return;
    state_->result = {SqlQueueDrainStatus::Complete, position};
}
void SqlQueueDrain::Cancel()
{
    if (!state_)
        return;
    std::lock_guard lock(state_->mutex);
    if (state_->result.status == SqlQueueDrainStatus::Pending)
        state_->result.status = SqlQueueDrainStatus::Cancelled;
}
