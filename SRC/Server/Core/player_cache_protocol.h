#pragma once
#include "player_session.h"

namespace auriga::player_cache_wire
{
inline constexpr size_t HelloBytes = 24, RequestBytes = 96, ReceiptBytes = 176;
enum class Action : uint8_t
{
    Begin = 1,
    Poll = 2
};
enum class Status : uint8_t
{
    Rejected = 1,
    Pending = 2,
    Flushed = 3,
    Blocked = 4,
    Stale = 5
};
enum class Failure : uint8_t
{
    None = 0,
    CacheChanged = 1,
    SqlUnconfirmed = 2,
    Cancelled = 3,
    InvalidLanes = 4
};
enum class LaneStatus : uint8_t
{
    Pending = 0,
    Complete = 1,
    Cancelled = 2
};
struct Request
{
    player_session::Key key;
    player_session::Token challenge{};
    Action action = Action::Begin;
};
struct Lane
{
    uint64_t id = 0, sequence = 0, unconfirmed = 0, lastUnconfirmed = 0;
    uint32_t sqlError = 0;
    LaneStatus status = LaneStatus::Cancelled;
    bool operator==(const Lane &) const = default;
};
struct Receipt
{
    player_session::Key key;
    player_session::Token challenge{};
    Status status = Status::Rejected;
    Failure failure = Failure::None;
    std::array<Lane, 2> lanes;
    bool operator==(const Receipt &) const = default;
};
std::vector<uint8_t> Hello(player_session::Token link, bool acknowledgement);
std::optional<player_session::Token> DecodeHello(std::span<const uint8_t> bytes, bool acknowledgement);
std::vector<uint8_t> Encode(const Request &request);
std::optional<Request> DecodeRequest(std::span<const uint8_t> bytes);
std::vector<uint8_t> Encode(const Receipt &receipt);
std::optional<Receipt> DecodeReceipt(std::span<const uint8_t> bytes);
bool Valid(const Receipt &receipt);
bool MayMutateCache(uint8_t gdHeader);

// A cache receipt never means SourceStopped or grants a Mission writer lease.
// Caller supplies the live session and full ECS entity generation on every use.
class Client final
{
  public:
    bool Begin(player_session::Key key, uint64_t entity, player_session::Token challenge);
    std::optional<Request> Next() const;
    bool Accept(const Receipt &receipt, const player_session::Key &current, uint64_t entity);
    std::optional<Receipt> Result(const player_session::Key &current, uint64_t entity) const;
    bool Matches(const player_session::Key &current, uint64_t entity) const;
    bool Waiting() const
    {
        return waiting_;
    }
    bool Started() const
    {
        return request_.has_value();
    }
    void Invalidate();
    void ObservePacket(uint8_t gdHeader);
    void Reset();

  private:
    std::optional<Request> request_;
    std::optional<Receipt> receipt_;
    uint64_t entity_ = 0;
    bool valid_ = false, waiting_ = false, acknowledged_ = false;
};
} // namespace auriga::player_cache_wire
