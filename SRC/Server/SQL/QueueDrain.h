#pragma once
#include <cstdint>
#include <memory>

// A FIFO boundary is evidence about this worker's queued work, not a SQL
// transaction or proof that a particular character's producers have stopped.
struct SqlQueuePosition
{
    uint64_t lane = 0, sequence = 0, unconfirmed = 0;
    uint64_t lastUnconfirmedSequence = 0;
    uint32_t lastSqlError = 0;
    bool ConfirmsSince(const SqlQueuePosition &verifiedBaseline) const;
};

enum class SqlQueueDrainStatus : uint8_t
{
    Pending,
    Complete,
    Cancelled
};
struct SqlQueueDrainResult
{
    SqlQueueDrainStatus status = SqlQueueDrainStatus::Cancelled;
    SqlQueuePosition position;
};

// Owned, pollable completion: no raw callback/session pointer and no entry in
// the legacy SQL result queue. Destruction of unexecuted work cancels it.
class SqlQueueDrain
{
  public:
    SqlQueueDrain() = default;
    uint64_t Sequence() const;
    SqlQueueDrainResult Poll() const;

  private:
    friend class CAsyncSQL;
    friend struct _SQLMsg;
    struct State;
    explicit SqlQueueDrain(uint64_t lane);
    void Sequence(uint64_t sequence);
    void Complete(SqlQueuePosition position);
    void Cancel();
    std::shared_ptr<State> state_;
};
