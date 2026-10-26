#pragma once
#include "Cache.h"
#include <Core/player_session.h>
#include <SQL/QueueDrain.h>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace auriga::player_cache
{
inline constexpr size_t MaximumItems = 1024;
// Borrowed only for the current main-thread call. No cache pointers survive Begin/Poll.
struct View
{
    CPlayerTableCache *player = nullptr;
    std::span<CItemCache *const> items;
};
enum class Status
{
    Pending,
    Flushed,
    Blocked,
    Stale
};
enum class Failure
{
    None,
    CacheChanged,
    SqlUnconfirmed,
    Cancelled,
    InvalidLanes
};
struct Receipt
{
    player_session::Lease lease;
    player_session::Token challenge{};
    Status status = Status::Pending;
    Failure failure = Failure::None;
    std::array<SqlQueueDrainResult, 2> lanes;
};
// Confirms only a captured player/item cache flush. It is NOT a source-stop,
// resource fence, Mission owner claim, or permission to release a live writer.
class Drain final
{
  public:
    using Enqueue = std::function<std::array<SqlQueueDrain, 2>()>;
    static std::optional<Drain> Begin(const player_session::Authority &authority, player_session::Lease lease,
                                      player_session::Token challenge, View view,
                                      std::array<SqlQueuePosition, 2> origins, const Enqueue &enqueue);
    Receipt Poll(const player_session::Authority &authority, View current);
    const player_session::Lease &Session() const
    {
        return lease_;
    }
    const player_session::Token &Challenge() const
    {
        return challenge_;
    }

  private:
    struct ItemStamp
    {
        uint32_t id;
        CacheVersion version;
        bool operator==(const ItemStamp &) const = default;
    };
    struct Stamp
    {
        CacheVersion player;
        std::vector<ItemStamp> items;
        bool operator==(const Stamp &) const = default;
    };
    static std::optional<Stamp> Inspect(View view, uint32_t character, bool requireClean);
    player_session::Lease lease_;
    player_session::Token challenge_{};
    Stamp stamp_;
    std::array<SqlQueuePosition, 2> origins_;
    std::array<SqlQueueDrain, 2> tickets_;
    Status status_ = Status::Pending;
    Failure failure_ = Failure::None;
};
} // namespace auriga::player_cache
