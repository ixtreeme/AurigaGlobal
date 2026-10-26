#include <Core/player_cache_protocol.h>
#include <common/tables.h>
#include <common/time_64_packets.h>
#include <iostream>
#include <stdexcept>

namespace wire = auriga::player_cache_wire;
namespace session = auriga::player_session;
namespace
{
int checks = 0;
void Check(bool value, const char *why)
{
    ++checks;
    if (!value)
        throw std::runtime_error(why);
}
session::Token Token(uint8_t n)
{
    session::Token token{};
    token.fill(n);
    return token;
}
session::Key Key()
{
    return {Token(1), Token(2), Token(3), 0x0102030405060708, 900, 100};
}
wire::Receipt Pending()
{
    wire::Receipt receipt{Key(), Token(4), wire::Status::Pending};
    receipt.lanes[0] = {11, 20, 0, 0, 0, wire::LaneStatus::Complete};
    receipt.lanes[1] = {12, 30, 0, 0, 0, wire::LaneStatus::Pending};
    return receipt;
}
void Codec()
{
    for (bool ack : {false, true})
    {
        auto hello = wire::Hello(Token(1), ack);
        Check(hello.size() == wire::HelloBytes && wire::DecodeHello(hello, ack) == Token(1),
              "cache capability round trip failed");
        Check(!wire::DecodeHello(hello, !ack) && !session::DecodeHello(hello, ack),
              "cache capability role/magic confusion");
        for (size_t cut = 0; cut < hello.size(); ++cut)
            Check(!wire::DecodeHello({hello.data(), cut}, ack), "truncated cache capability accepted");
        hello.push_back(0);
        Check(!wire::DecodeHello(hello, ack), "oversized cache capability accepted");
    }
    for (auto action : {wire::Action::Begin, wire::Action::Poll})
    {
        auto bytes = wire::Encode(wire::Request{Key(), Token(4), action});
        const auto parsed = wire::DecodeRequest(bytes);
        Check(bytes.size() == wire::RequestBytes && parsed && parsed->key == Key() &&
                  parsed->challenge == Token(4) && parsed->action == action,
              "cache request round trip failed");
        Check(bytes[56] == 8 && bytes[63] == 1 && bytes[64] == 0x84 && bytes[65] == 3,
              "cache key is not explicit little endian");
        for (size_t cut = 0; cut < bytes.size(); ++cut)
            Check(!wire::DecodeRequest({bytes.data(), cut}), "truncated cache request accepted");
        for (size_t index : {size_t(0), size_t(4), size_t(6), size_t(7), size_t(88), size_t(89), size_t(95)})
        {
            auto bad = bytes;
            bad[index] = 255;
            Check(!wire::DecodeRequest(bad), "bad cache request version/action/reserved bytes accepted");
        }
        bytes.push_back(0);
        Check(!wire::DecodeRequest(bytes), "extra cache request bytes accepted");
    }
    auto original = Pending();
    for (auto status : {wire::Status::Rejected, wire::Status::Pending, wire::Status::Flushed,
                        wire::Status::Blocked, wire::Status::Stale})
    {
        auto receipt = original;
        receipt.status = status;
        if (status == wire::Status::Flushed)
            receipt.lanes[1].status = wire::LaneStatus::Complete;
        if (status == wire::Status::Blocked)
        {
            receipt.failure = wire::Failure::SqlUnconfirmed;
            receipt.lanes[1].unconfirmed = 1;
            receipt.lanes[1].lastUnconfirmed = 29;
            receipt.lanes[1].sqlError = 1064;
        }
        const auto bytes = wire::Encode(receipt);
        auto decoded = wire::DecodeReceipt(bytes);
        Check(bytes.size() == wire::ReceiptBytes && decoded && *decoded == receipt,
              "cache receipt round trip failed");
        for (size_t cut = 0; cut < bytes.size(); ++cut)
            Check(!wire::DecodeReceipt({bytes.data(), cut}), "truncated cache receipt accepted");
        for (size_t index : {size_t(4), size_t(6), size_t(7), size_t(88), size_t(89), size_t(90), size_t(95),
                             size_t(132), size_t(133), size_t(175)})
        {
            auto bad = bytes;
            bad[index] = 255;
            Check(!wire::DecodeReceipt(bad), "bad cache receipt type/status/lane/reserved data accepted");
        }
    }
    auto complete = original;
    complete.status = wire::Status::Flushed;
    complete.lanes[1].status = wire::LaneStatus::Complete;
    for (int failure = 0; failure < 14; ++failure)
    {
        auto bad = complete;
        if (failure == 0)
            bad.key.link = {};
        if (failure == 1)
            bad.key.request = {};
        if (failure == 2)
            bad.key.session = {};
        if (failure == 3)
            bad.key.generation = 0;
        if (failure == 4)
            bad.key.account = 0;
        if (failure == 5)
            bad.key.character = 0;
        if (failure == 6)
            bad.challenge = {};
        if (failure == 7)
            bad.lanes[1].id = bad.lanes[0].id;
        if (failure == 8)
            bad.lanes[0].sequence = 0;
        if (failure == 9)
            bad.lanes[0].sequence = UINT64_MAX;
        if (failure == 10)
            bad.lanes[0].status = wire::LaneStatus::Pending;
        if (failure == 11)
            bad.lanes[0].status = wire::LaneStatus::Cancelled;
        if (failure == 12)
        {
            bad.lanes[0].unconfirmed = 1;
            bad.lanes[0].lastUnconfirmed = 19;
            bad.lanes[0].sqlError = 1064;
        }
        if (failure == 13)
            bad.failure = wire::Failure::CacheChanged;
        Check(!wire::Valid(bad), "unproved SQL boundary accepted as flushed");
    }
    Check(time64::ValidDatabasePacketSize(HEADER_DG_PLAYER_CACHE_HELLO, wire::HelloBytes) &&
              !time64::ValidDatabasePacketSize(HEADER_DG_PLAYER_CACHE_HELLO, wire::HelloBytes + 1) &&
              time64::ValidDatabasePacketSize(HEADER_DG_PLAYER_CACHE_RECEIPT, wire::ReceiptBytes) &&
              !time64::ValidDatabasePacketSize(HEADER_DG_PLAYER_CACHE_RECEIPT, wire::ReceiptBytes - 1),
          "outer cache frame size guard failed");
}
void Lifecycle()
{
    constexpr uint64_t entity = 0x01000001, replacementEntity = 0x02000001;
    wire::Client client;
    Check(!client.Started() && !client.Next() && !client.Result(Key(), entity),
          "empty cache operation has authority");
    Check(client.Begin(Key(), entity, Token(4)) && client.Begin(Key(), entity, Token(4)) &&
              !client.Begin(Key(), replacementEntity, Token(4)) && !client.Begin(Key(), entity, Token(5)),
          "cache begin is not scoped/idempotent");
    Check(client.Started() && client.Next()->action == wire::Action::Begin &&
              client.Next()->action == wire::Action::Begin,
          "unacknowledged cache begin cannot be retried");
    auto pending = Pending();
    for (int field = 0; field < 8; ++field)
    {
        auto receipt = pending;
        if (field == 0)
            receipt.key.link[0]++;
        if (field == 1)
            receipt.key.request[0]++;
        if (field == 2)
            receipt.key.session[0]++;
        if (field == 3)
            receipt.key.generation++;
        if (field == 4)
            receipt.key.account++;
        if (field == 5)
            receipt.key.character++;
        if (field == 6)
            receipt.challenge[0]++;
        Check(!client.Accept(receipt, Key(), field == 7 ? replacementEntity : entity),
              "foreign/stale cache receipt accepted");
    }
    Check(client.Accept(pending, Key(), entity) && client.Next()->action == wire::Action::Poll,
          "acknowledged cache operation did not switch to poll");
    Check(client.Accept(pending, Key(), entity), "duplicate pending reply broke cache polling");
    auto complete = pending;
    complete.status = wire::Status::Flushed;
    complete.lanes[1].status = wire::LaneStatus::Complete;
    for (auto status : {wire::Status::Pending, wire::Status::Flushed})
        for (size_t field = 0; field < 4; ++field)
        {
            auto wrong = status == wire::Status::Pending ? pending : complete;
            if (field % 2)
                wrong.lanes[field / 2].sequence += 10;
            else
                wrong.lanes[field / 2].id += 10;
            Check(wire::Valid(wrong) && !client.Accept(wrong, Key(), entity),
                  "cache operation changed its acknowledged SQL lane/boundary");
        }
    Check(client.Accept(complete, Key(), entity) && !client.Waiting() && !client.Next() &&
              client.Result(Key(), entity) == complete,
          "cache completion not retained");
    Check(!client.Accept(pending, Key(), entity) && !client.Accept(complete, Key(), entity) &&
              !client.Result(Key(), replacementEntity),
          "late reply or entity replacement changed completed proof");
    client.ObservePacket(HEADER_GD_PLAYER_COUNT);
    client.ObservePacket(HEADER_GD_PLAYER_CACHE_REQUEST);
    Check(client.Result(Key(), entity) == complete, "read/control packet invalidated cache proof");
    for (uint8_t header :
         {uint8_t(HEADER_GD_PLAYER_SAVE), uint8_t(HEADER_GD_ITEM_SAVE), uint8_t(HEADER_GD_ITEM_DESTROY),
          uint8_t(HEADER_GD_QUEST_SAVE), uint8_t(HEADER_GD_FLUSH_CACHE), uint8_t(HEADER_GD_ITEM_FLUSH),
          uint8_t(0), uint8_t(253)})
    {
        client.Reset();
        client.Begin(Key(), entity, Token(4));
        client.Accept(complete, Key(), entity);
        client.ObservePacket(header);
        Check(!client.Result(Key(), entity) && !client.Begin(Key(), entity, Token(4)),
              "new/split/unknown write retained an old cache proof");
    }
    client.Reset();
    client.Begin(Key(), entity, Token(4));
    client.Invalidate();
    Check(client.Started() && !client.Accept(complete, Key(), entity) && !client.Result(Key(), entity),
          "timeout/disconnect revived cache authority");
    client.Reset();
    Check(!client.Started(), "new login did not reset snapshot preparation");
    auto next = Key();
    next.generation++;
    next.session = Token(8);
    client.Begin(next, entity, Token(4));
    Check(!client.Accept(complete, next, entity), "same handle/PID/challenge reused a prior login receipt");
    for (int i = 0; i < 256; ++i)
    {
        client.Reset();
        next.generation++;
        Check(client.Begin(next, entity, Token(4)) && !client.Accept(complete, next, entity),
              "old receipt regained authority after session churn");
    }
}
} // namespace
int main()
{
    try
    {
        Codec();
        Lifecycle();
        std::cout << "player cache protocol: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
