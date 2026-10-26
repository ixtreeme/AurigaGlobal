#include "player_session.h"
#include <algorithm>
#include <common/tables.h>
#include <cstring>
#include <openssl/rand.h>
#include <stdexcept>

namespace auriga::player_session
{
namespace
{
void Need(bool condition)
{
    if (!condition)
        throw std::invalid_argument("Invalid player session frame");
}
uint32_t Part(uint8_t header)
{
    switch (header)
    {
    case HEADER_DG_PLAYER_LOAD_SUCCESS:
        return 1;
    case HEADER_DG_PLAYER_LOAD_FAILED:
        return 2;
    case HEADER_DG_ITEM_LOAD:
        return 4;
    case HEADER_DG_QUEST_LOAD:
        return 8;
    case HEADER_DG_AFFECT_LOAD:
        return 16;
    case HEADER_DG_NEED_LOGIN_LOG:
        return 32;
#ifdef __SKILL_COLOR_SYSTEM__
    case HEADER_DG_SKILL_COLOR_LOAD:
        return 64;
#endif
    default:
        return 0;
    }
}
uint32_t RequiredParts()
{
    uint32_t mask = 1 | 4 | 8 | 16;
#ifdef __SKILL_COLOR_SYSTEM__
    mask |= 64;
#endif
    return mask;
}
void U32(std::vector<uint8_t> &out, uint32_t n)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(uint8_t(n >> (8 * i)));
}
void U64(std::vector<uint8_t> &out, uint64_t n)
{
    for (int i = 0; i < 8; ++i)
        out.push_back(uint8_t(n >> (8 * i)));
}
void Bytes(std::vector<uint8_t> &out, const Token &token)
{
    out.insert(out.end(), token.begin(), token.end());
}
std::vector<uint8_t> Prefix(uint8_t type)
{
    return {'A', 'M', 'S', '1', 1, 0, type, 0};
}
struct Reader
{
    std::span<const uint8_t> bytes;
    size_t at = 0;
    uint8_t Byte()
    {
        Need(at < bytes.size());
        return bytes[at++];
    }
    uint32_t U32()
    {
        uint32_t n = 0;
        for (int i = 0; i < 4; ++i)
            n |= uint32_t(Byte()) << (8 * i);
        return n;
    }
    uint64_t U64()
    {
        uint64_t n = 0;
        for (int i = 0; i < 8; ++i)
            n |= uint64_t(Byte()) << (8 * i);
        return n;
    }
    Token TokenValue()
    {
        Token token{};
        for (auto &b : token)
            b = Byte();
        return token;
    }
    void Prefix(uint8_t type)
    {
        const auto expected = player_session::Prefix(type);
        for (auto byte : expected)
            Need(Byte() == byte);
    }
};
bool KeyValid(const Key &key)
{
    return Nonzero(key.link) && Nonzero(key.request) && Nonzero(key.session) && key.generation &&
           key.account && key.character;
}
uint32_t NativeU32(std::span<const uint8_t> bytes, size_t at = 0)
{
    uint32_t n = 0;
    if (at + 4 <= bytes.size())
        std::memcpy(&n, bytes.data() + at, 4);
    return n;
}
bool Rows(std::span<const uint8_t> bytes, size_t prefix, uint32_t count, size_t row)
{
    return count <= 65536 && bytes.size() == prefix + size_t(count) * row;
}
} // namespace
bool Nonzero(const Token &token)
{
    return std::any_of(token.begin(), token.end(), [](auto b) { return b != 0; });
}
Token NewToken()
{
    Token token{};
    if (RAND_bytes(token.data(), int(token.size())) != 1 || !Nonzero(token))
        throw std::runtime_error("Player session nonce generation failed");
    return token;
}
bool IsLoadHeader(uint8_t header)
{
    return Part(header) != 0;
}
std::vector<uint8_t> Hello(Token link, bool ack)
{
    Need(Nonzero(link));
    auto out = Prefix(ack ? 2 : 1);
    Bytes(out, link);
    return out;
}
std::optional<Token> DecodeHello(std::span<const uint8_t> bytes, bool ack)
{
    try
    {
        Need(bytes.size() == HelloBytes);
        Reader r{bytes};
        r.Prefix(ack ? 2 : 1);
        auto link = r.TokenValue();
        Need(Nonzero(link));
        return link;
    }
    catch (const std::invalid_argument &)
    {
        return {};
    }
}
std::vector<uint8_t> EncodeRequest(const Request &request)
{
    Need(Nonzero(request.link) && Nonzero(request.request) && request.account && request.character &&
         request.slot < PLAYER_PER_ACCOUNT);
    auto out = Prefix(3);
    Bytes(out, request.link);
    Bytes(out, request.request);
    U32(out, request.account);
    U32(out, request.character);
    out.push_back(request.slot);
    out.insert(out.end(), 3, 0);
    return out;
}
std::optional<Request> DecodeRequest(std::span<const uint8_t> bytes)
{
    try
    {
        Need(bytes.size() == RequestBytes);
        Reader r{bytes};
        r.Prefix(3);
        Request request{r.TokenValue(), r.TokenValue(), r.U32(), r.U32(), r.Byte()};
        Need(!r.Byte() && !r.Byte() && !r.Byte());
        EncodeRequest(request);
        return request;
    }
    catch (const std::invalid_argument &)
    {
        return {};
    }
}
std::vector<uint8_t> ReplyPrefix(const Key &key, uint8_t header, size_t count)
{
    Need(KeyValid(key) && IsLoadHeader(header) && count <= MaximumPayload);
    auto out = Prefix(4);
    Bytes(out, key.link);
    Bytes(out, key.request);
    Bytes(out, key.session);
    U64(out, key.generation);
    U32(out, key.account);
    U32(out, key.character);
    out.push_back(header);
    out.insert(out.end(), 3, 0);
    U32(out, uint32_t(count));
    return out;
}
std::optional<Reply> DecodeReply(std::span<const uint8_t> bytes)
{
    try
    {
        Need(bytes.size() >= ReplyPrefixBytes && bytes.size() <= ReplyPrefixBytes + MaximumPayload);
        Reader r{bytes};
        r.Prefix(4);
        Key key{r.TokenValue(), r.TokenValue(), r.TokenValue(), r.U64(), r.U32(), r.U32()};
        const auto header = r.Byte();
        Need(!r.Byte() && !r.Byte() && !r.Byte());
        const auto size = r.U32();
        Need(KeyValid(key) && IsLoadHeader(header) && size == bytes.size() - r.at);
        auto body = bytes.subspan(r.at);
        Need(ValidPayload(header, body, key.character));
        return Reply{key, header, body};
    }
    catch (const std::invalid_argument &)
    {
        return {};
    }
}
bool ValidPayload(uint8_t header, std::span<const uint8_t> bytes, uint32_t character)
{
    if (bytes.size() > MaximumPayload || !character)
        return false;
    switch (header)
    {
    case HEADER_DG_PLAYER_LOAD_SUCCESS:
        return bytes.size() == sizeof(TPlayerTable) && NativeU32(bytes) == character;
    case HEADER_DG_PLAYER_LOAD_FAILED:
        return bytes.empty();
    case HEADER_DG_ITEM_LOAD:
        if (bytes.size() < 4 || !Rows(bytes, 4, NativeU32(bytes), sizeof(TPlayerItem)))
            return false;
        for (size_t i = 4; i < bytes.size(); i += sizeof(TPlayerItem))
            if (NativeU32(bytes, i + offsetof(TPlayerItem, owner)) != character)
                return false;
        return true;
    case HEADER_DG_QUEST_LOAD:
        if (bytes.size() < 4 || !Rows(bytes, 4, NativeU32(bytes), sizeof(TQuestTable)))
            return false;
        for (size_t i = 4; i < bytes.size(); i += sizeof(TQuestTable))
            if (NativeU32(bytes, i) != character)
                return false;
        return true;
    case HEADER_DG_AFFECT_LOAD:
        return bytes.size() >= 8 && NativeU32(bytes) == character &&
               Rows(bytes, 8, NativeU32(bytes, 4), sizeof(TPacketAffectElement));
    case HEADER_DG_NEED_LOGIN_LOG:
        return bytes.size() == sizeof(TPacketNeedLoginLogInfo) && NativeU32(bytes) == character;
#ifdef __SKILL_COLOR_SYSTEM__
    case HEADER_DG_SKILL_COLOR_LOAD:
        return bytes.size() == sizeof(TSkillColor::dwSkillColor);
#endif
    default:
        return false;
    }
}
std::optional<Lease> Authority::Open(uint32_t peer, uint32_t descriptor, uint32_t account, uint32_t character,
                                     Token link, Token request)
{
    if (!peer || !descriptor || !account || !character || accounts_.contains(account) ||
        characters_.contains(character) || accounts_.size() >= 100000 || generation_ == UINT64_MAX ||
        Nonzero(link) != Nonzero(request))
        return {};
    Lease lease{{link, request, NewToken(), ++generation_, account, character}, peer, descriptor};
    accounts_.emplace(account, Entry{lease});
    characters_.emplace(character, account);
    return lease;
}
bool Authority::Matches(const Lease &lease) const
{
    const auto it = accounts_.find(lease.key.account);
    return it != accounts_.end() && it->second.lease == lease;
}
bool Authority::TakePart(const Lease &lease, uint8_t header)
{
    if (!Matches(lease))
        return false;
    auto &entry = accounts_.at(lease.key.account);
    const auto part = Part(header);
    if (!part || (entry.parts & 2) || (entry.parts & part) || (!(entry.parts & 1) && part != 1 && part != 2))
        return false;
    entry.parts |= part;
    return true;
}
std::optional<Lease> Authority::Find(uint32_t account) const
{
    const auto it = accounts_.find(account);
    if (it == accounts_.end())
        return {};
    return it->second.lease;
}
bool Authority::Loaded(const Lease &lease) const
{
    if (!Matches(lease)) return false;
    const auto parts = accounts_.at(lease.key.account).parts;
    return (parts & RequiredParts()) == RequiredParts() && !(parts & 2);
}
void Authority::InvalidateAccount(uint32_t account)
{
    auto it = accounts_.find(account);
    if (it == accounts_.end())
        return;
    characters_.erase(it->second.lease.key.character);
    accounts_.erase(it);
}
bool Authority::Invalidate(const Lease &lease)
{
    if (!Matches(lease))
        return false;
    InvalidateAccount(lease.key.account);
    return true;
}
void Authority::InvalidatePeer(uint32_t peer)
{
    for (auto it = accounts_.begin(); it != accounts_.end();)
        if (it->second.lease.peer == peer)
        {
            characters_.erase(it->second.lease.key.character);
            it = accounts_.erase(it);
        }
        else
            ++it;
}
bool GameSession::Begin(Request request)
{
    if (request_ || !Nonzero(request.link) || !Nonzero(request.request) || !request.account ||
        !request.character || request.slot >= PLAYER_PER_ACCOUNT)
        return false;
    request_ = request;
    key_.reset();
    parts_ = 0;
    return true;
}
bool GameSession::Accept(const Reply &reply)
{
    if (!request_ || !KeyValid(reply.key) || reply.key.link != request_->link ||
        reply.key.request != request_->request || reply.key.account != request_->account ||
        reply.key.character != request_->character || (key_ && *key_ != reply.key) ||
        !ValidPayload(reply.header, reply.payload, reply.key.character))
        return false;
    const auto part = Part(reply.header);
    if (!part || (parts_ & 2) || (parts_ & part) || (!(parts_ & 1) && part != 1 && part != 2))
        return false;
    key_ = reply.key;
    parts_ |= part;
    return true;
}
bool GameSession::Matches(const Key &key) const
{
    return key_ && *key_ == key && (parts_ & 1) && !(parts_ & 2);
}
bool GameSession::BindEntity(uint64_t entity)
{
    if (!key_ || !Matches(*key_) || entity_)
        return false;
    entity_ = entity;
    return true;
}
bool GameSession::Loaded() const
{
    return key_ && Matches(*key_) && entity_ && (parts_ & RequiredParts()) == RequiredParts();
}
bool GameSession::MatchesEntity(const Key &key, uint64_t entity) const
{
    return Matches(key) && entity_ && *entity_ == entity;
}
void GameSession::Reset()
{
    request_.reset();
    key_.reset();
    entity_.reset();
    parts_ = 0;
}
} // namespace auriga::player_session
