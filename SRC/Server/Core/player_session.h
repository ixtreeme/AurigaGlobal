#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace auriga::player_session
{
using Token = std::array<uint8_t, 16>;
Token NewToken();
bool Nonzero(const Token &token);
inline constexpr size_t HelloBytes = 24, RequestBytes = 52, ReplyPrefixBytes = 80;
inline constexpr size_t MaximumPayload = 3 * 1024 * 1024;
struct Request
{
    Token link{}, request{};
    uint32_t account = 0, character = 0;
    uint8_t slot = 0;
};
struct Key
{
    Token link{}, request{}, session{};
    uint64_t generation = 0;
    uint32_t account = 0, character = 0;
    bool operator==(const Key &) const = default;
    bool Versioned() const
    {
        return Nonzero(link) && Nonzero(request);
    }
};
struct Lease
{
    Key key;
    uint32_t peer = 0, descriptor = 0;
    bool operator==(const Lease &) const = default;
};
std::vector<uint8_t> Hello(Token link, bool acknowledgement);
std::optional<Token> DecodeHello(std::span<const uint8_t> bytes, bool acknowledgement);
std::vector<uint8_t> EncodeRequest(const Request &request);
std::optional<Request> DecodeRequest(std::span<const uint8_t> bytes);
// The body retains the legacy server ABI; the envelope itself is explicit LE.
std::vector<uint8_t> ReplyPrefix(const Key &key, uint8_t header, size_t payloadBytes);
struct Reply
{
    Key key;
    uint8_t header = 0;
    std::span<const uint8_t> payload;
};
std::optional<Reply> DecodeReply(std::span<const uint8_t> bytes);
bool ValidPayload(uint8_t header, std::span<const uint8_t> bytes, uint32_t character);
bool IsLoadHeader(uint8_t header);

// Single native owner thread. Open only AFTER current logon/peer/slot checks.
// These leases authenticate a live load, not a durable Mission owner claim.
class Authority final
{
  public:
    std::optional<Lease> Open(uint32_t peer, uint32_t descriptor, uint32_t account, uint32_t character,
                              Token link = {}, Token request = {});
    bool Matches(const Lease &lease) const;
    bool TakePart(const Lease &lease, uint8_t header);
    std::optional<Lease> Find(uint32_t account) const;
    void InvalidateAccount(uint32_t account);
    void InvalidatePeer(uint32_t peer);
    bool Invalidate(const Lease &lease);

  private:
    struct Entry
    {
        Lease lease;
        uint32_t parts = 0;
    };
    std::map<uint32_t, Entry> accounts_;
    std::map<uint32_t, uint32_t> characters_;
    uint64_t generation_ = 0;
};

// Descriptor-owned request/session state. No ECS pointer or worker callback.
class GameSession final
{
  public:
    bool Begin(Request request);
    bool Accept(const Reply &reply);
    bool Matches(const Key &key) const;
    bool BindEntity(uint64_t entity);
    bool MatchesEntity(const Key &key, uint64_t entity) const;
    bool Versioned() const
    {
        return request_.has_value();
    }
    std::optional<Key> Current() const
    {
        return key_;
    }
    void Reset();

  private:
    std::optional<Request> request_;
    std::optional<Key> key_;
    std::optional<uint64_t> entity_;
    uint32_t parts_ = 0;
};
} // namespace auriga::player_session
