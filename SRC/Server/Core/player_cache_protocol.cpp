#include "player_cache_protocol.h"
#include <stdexcept>
#include <common/tables.h>

namespace auriga::player_cache_wire
{
namespace
{
void Need(bool value)
{
    if (!value)
        throw std::invalid_argument("Invalid player cache frame");
}
bool ValidKey(const player_session::Key &key)
{
    return key.Versioned() && player_session::Nonzero(key.session) && key.generation && key.account &&
           key.character;
}
struct Writer
{
    std::vector<uint8_t> bytes;
    explicit Writer(uint8_t type) : bytes{'A', 'C', 'D', '1', 1, 0, type, 0} {}
    void Integer(uint64_t n, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
            bytes.push_back(uint8_t(n >> (8 * i)));
    }
    void Token(player_session::Token token)
    {
        bytes.insert(bytes.end(), token.begin(), token.end());
    }
    void Key(const player_session::Key &key)
    {
        Token(key.link);
        Token(key.request);
        Token(key.session);
        Integer(key.generation, 8);
        Integer(key.account, 4);
        Integer(key.character, 4);
    }
};
struct Reader
{
    std::span<const uint8_t> bytes;
    size_t at = 0;
    void Prefix(uint8_t type)
    {
        for (auto n : Writer(type).bytes)
            Need(Integer(1) == n);
    }
    uint64_t Integer(size_t size)
    {
        Need(size <= 8 && at + size <= bytes.size());
        uint64_t n = 0;
        for (size_t i = 0; i < size; ++i)
            n |= uint64_t(bytes[at++]) << (8 * i);
        return n;
    }
    player_session::Token Token()
    {
        player_session::Token token{};
        for (auto &b : token)
            b = uint8_t(Integer(1));
        return token;
    }
    player_session::Key Key()
    {
        auto link = Token(), request = Token(), session = Token();
        auto generation = Integer(8);
        auto account = uint32_t(Integer(4)), character = uint32_t(Integer(4));
        return {link, request, session, generation, account, character};
    }
};
bool ValidRequest(const Request &request)
{
    return ValidKey(request.key) && player_session::Nonzero(request.challenge) &&
           (request.action == Action::Begin || request.action == Action::Poll);
}
} // namespace
std::vector<uint8_t> Hello(player_session::Token link, bool ack)
{
    Need(player_session::Nonzero(link));
    Writer w(ack ? 4 : 3);
    w.Token(link);
    return w.bytes;
}
std::optional<player_session::Token> DecodeHello(std::span<const uint8_t> bytes, bool ack)
{
    try
    {
        Need(bytes.size() == HelloBytes);
        Reader r{bytes};
        r.Prefix(ack ? 4 : 3);
        auto link = r.Token();
        Need(player_session::Nonzero(link));
        return link;
    }
    catch (const std::invalid_argument &)
    {
        return {};
    }
}
std::vector<uint8_t> Encode(const Request &request)
{
    Need(ValidRequest(request));
    Writer w(1);
    w.Key(request.key);
    w.Token(request.challenge);
    w.Integer(uint8_t(request.action), 1);
    w.Integer(0, 7);
    return w.bytes;
}
std::optional<Request> DecodeRequest(std::span<const uint8_t> bytes)
{
    try
    {
        Need(bytes.size() == RequestBytes);
        Reader r{bytes};
        r.Prefix(1);
        Request request{r.Key(), r.Token(), Action(r.Integer(1))};
        Need(!r.Integer(7) && ValidRequest(request));
        return request;
    }
    catch (const std::invalid_argument &)
    {
        return {};
    }
}
bool Valid(const Receipt &receipt)
{
    if (!ValidKey(receipt.key) || !player_session::Nonzero(receipt.challenge) ||
        receipt.status < Status::Rejected || receipt.status > Status::Stale ||
        receipt.failure > Failure::InvalidLanes)
        return false;
    for (const auto &lane : receipt.lanes)
        if (lane.status > LaneStatus::Cancelled ||
            (lane.unconfirmed == 0 && (lane.lastUnconfirmed || lane.sqlError)))
            return false;
    if (receipt.status == Status::Blocked)
        return receipt.failure != Failure::None;
    if (receipt.failure != Failure::None)
        return false;
    if (receipt.status == Status::Rejected || receipt.status == Status::Stale)
        return true;
    if (!receipt.lanes[0].id || !receipt.lanes[1].id || receipt.lanes[0].id == receipt.lanes[1].id)
        return false;
    bool pending = false;
    for (const auto &lane : receipt.lanes)
    {
        if (!lane.sequence || lane.sequence == UINT64_MAX || lane.unconfirmed || lane.lastUnconfirmed ||
            lane.sqlError || lane.status == LaneStatus::Cancelled)
            return false;
        pending |= lane.status == LaneStatus::Pending;
    }
    return receipt.status == Status::Pending ? pending : !pending;
}
std::vector<uint8_t> Encode(const Receipt &receipt)
{
    Need(Valid(receipt));
    Writer w(2);
    w.Key(receipt.key);
    w.Token(receipt.challenge);
    w.Integer(uint8_t(receipt.status), 1);
    w.Integer(uint8_t(receipt.failure), 1);
    w.Integer(0, 6);
    for (const auto &lane : receipt.lanes)
    {
        w.Integer(lane.id, 8);
        w.Integer(lane.sequence, 8);
        w.Integer(lane.unconfirmed, 8);
        w.Integer(lane.lastUnconfirmed, 8);
        w.Integer(lane.sqlError, 4);
        w.Integer(uint8_t(lane.status), 1);
        w.Integer(0, 3);
    }
    return w.bytes;
}
std::optional<Receipt> DecodeReceipt(std::span<const uint8_t> bytes)
{
    try
    {
        Need(bytes.size() == ReceiptBytes);
        Reader r{bytes};
        r.Prefix(2);
        Receipt receipt{r.Key(), r.Token(), Status(r.Integer(1)), Failure(r.Integer(1))};
        Need(!r.Integer(6));
        for (auto &lane : receipt.lanes)
        {
            lane.id = r.Integer(8);
            lane.sequence = r.Integer(8);
            lane.unconfirmed = r.Integer(8);
            lane.lastUnconfirmed = r.Integer(8);
            lane.sqlError = uint32_t(r.Integer(4));
            lane.status = LaneStatus(r.Integer(1));
            Need(!r.Integer(3));
        }
        Need(Valid(receipt));
        return receipt;
    }
    catch (const std::invalid_argument &)
    {
        return {};
    }
}
bool Client::Begin(player_session::Key key, uint64_t entity, player_session::Token challenge)
{
    if (request_)
        return valid_ && request_->key == key && request_->challenge == challenge && entity_ == entity;
    if (!ValidRequest({key, challenge}))
        return false;
    request_ = Request{key, challenge};
    entity_ = entity;
    valid_ = waiting_ = true;
    return true;
}
std::optional<Request> Client::Next() const
{
    if (!valid_ || !waiting_ || !request_)
        return {};
    auto request = *request_;
    request.action = acknowledged_ ? Action::Poll : Action::Begin;
    return request;
}
bool Client::Matches(const player_session::Key &current, uint64_t entity) const
{
    return valid_ && request_ && request_->key == current && entity_ == entity;
}
bool Client::Accept(const Receipt &receipt, const player_session::Key &current, uint64_t entity)
{
    if (!waiting_ || !Matches(current, entity) || !Valid(receipt) || receipt.key != request_->key ||
        receipt.challenge != request_->challenge)
        return false;
    if (receipt_ && (receipt.status == Status::Pending || receipt.status == Status::Flushed))
        for (size_t lane = 0; lane < 2; ++lane)
            if (receipt.lanes[lane].id != receipt_->lanes[lane].id ||
                receipt.lanes[lane].sequence != receipt_->lanes[lane].sequence)
                return false;
    receipt_ = receipt;
    acknowledged_ = true;
    if (receipt.status != Status::Pending)
        waiting_ = false;
    return true;
}
std::optional<Receipt> Client::Result(const player_session::Key &current, uint64_t entity) const
{
    return Matches(current, entity) ? receipt_ : std::nullopt;
}
void Client::Invalidate()
{
    valid_ = waiting_ = false;
}
bool MayMutateCache(uint8_t header)
{
    switch (header)
    {
    case HEADER_GD_PING:
    case HEADER_GD_PONG:
    case HEADER_GD_AUTH:
    case HEADER_GD_PLAYER_COUNT:
    case HEADER_GD_UPDATE_CHANNELSTATUS:
    case HEADER_GD_PLAYER_SESSION_HELLO:
    case HEADER_GD_PLAYER_CACHE_HELLO:
    case HEADER_GD_PLAYER_CACHE_REQUEST:
        return false;
    default:
        return true; // New/unknown or mixed-purpose requests invalidate conservatively.
    }
}
void Client::ObservePacket(uint8_t header)
{
    if (MayMutateCache(header))
        Invalidate();
}
void Client::Reset()
{
    request_.reset();
    receipt_.reset();
    entity_ = 0;
    valid_ = waiting_ = acknowledged_ = false;
}
} // namespace auriga::player_cache_wire
