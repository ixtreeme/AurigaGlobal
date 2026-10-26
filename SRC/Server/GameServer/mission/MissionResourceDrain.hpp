#pragma once
#include "MissionResourceFence.hpp"
#include <SQL/QueueDrain.h>

namespace auriga::mission::persistence
{
enum class ResourceDrainStatus
{
    Pending,
    Ready,
    Blocked,
    Stale
};
// Bind the actual Database FIFO tickets to one native session/challenge.
// The source-stop/cache-flush adapter still has to run before enqueuing them.
// Baselines are origins or boundaries whose canonical state was explicitly
// verified; taking a fresh baseline must never silently acknowledge an error.
class ResourceDrain final
{
  public:
    ResourceDrain(const ResourceFence &fence, std::array<SqlQueuePosition, 2> verifiedBaselines,
                  std::array<SqlQueueDrain, 2> tickets);
    ResourceDrainStatus Poll(ResourceFence &fence) const;

  private:
    ResourceWriterIdentity identity_;
    Token challenge_;
    std::array<SqlQueuePosition, 2> baselines_;
    std::array<SqlQueueDrain, 2> tickets_;
};
} // namespace auriga::mission::persistence
