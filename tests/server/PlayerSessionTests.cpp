#include "../../SRC/Server/Database/player_load.h"
#include <Core/Logging.hpp>
#include <Core/player_session.h>
#include <Core/stdafx.h>
#include <SQL/AsyncSQL.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

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
    session::Token t{};
    t.fill(n);
    return t;
}
template <class T> std::vector<uint8_t> Data(const T &v)
{
    const auto *p = reinterpret_cast<const uint8_t *>(&v);
    return {p, p + sizeof(v)};
}
std::vector<uint8_t> Frame(const session::Key &key, uint8_t header, std::span<const uint8_t> payload)
{
    auto bytes = session::ReplyPrefix(key, header, payload.size());
    Check(bytes.size() == session::ReplyPrefixBytes, "session prefix layout drift");
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}
TAccountTable Account()
{
    TAccountTable account{};
    account.id = 900;
    for (size_t i = 0; i < PLAYER_PER_ACCOUNT; ++i)
        account.players[i].dwID = 100 + uint32_t(i);
    return account;
}
void Authorization()
{
    const auto account = Account();
    PlayerLoadContext context{7, 7, true, true, false, false};
    for (uint8_t i = 0; i < PLAYER_PER_ACCOUNT; ++i)
    {
        TPlayerLoadPacket request{account.id, account.players[i].dwID, i};
        Check(AuthorizePlayerLoad(account, request, context), "owned slot refused");
        for (uint8_t j = 0; j < PLAYER_PER_ACCOUNT; ++j)
            if (i != j)
            {
                auto wrong = request;
                wrong.account_index = j;
                Check(!AuthorizePlayerLoad(account, wrong, context), "wrong slot accepted");
            }
        for (int failure = 0; failure < 9; ++failure)
        {
            auto c = context;
            auto r = request;
            if (failure == 0)
                c.authenticated = false;
            if (failure == 1)
                c.loggedOn = false;
            if (failure == 2)
                c.playing = true;
            if (failure == 3)
                c.deleted = true;
            if (failure == 4)
                c.peer = 8;
            if (failure == 5)
                r.account_id++;
            if (failure == 6)
                r.player_id = 0;
            if (failure == 7)
                r.account_index = PLAYER_PER_ACCOUNT;
            if (failure == 8)
                c.peer = c.accountPeer = 0;
            Check(!AuthorizePlayerLoad(account, r, c), "invalid native load authorization accepted");
        }
    }
}
void Protocol()
{
    const auto token = session::NewToken();
    Check(session::Nonzero(token) && token != session::NewToken(), "nonces reused");
    for (bool ack : {false, true})
    {
        auto bytes = session::Hello(token, ack);
        Check(bytes.size() == session::HelloBytes && session::DecodeHello(bytes, ack) == token,
              "hello round trip failed");
        Check(!session::DecodeHello(bytes, !ack), "hello role confused");
        for (size_t cut = 0; cut < bytes.size(); ++cut)
            Check(!session::DecodeHello({bytes.data(), cut}, ack), "truncated hello accepted");
        bytes[4] = 2;
        Check(!session::DecodeHello(bytes, ack), "unknown hello version accepted");
    }
    session::Request request{Token(1), Token(2), 900, 100, 0};
    auto bytes = session::EncodeRequest(request);
    Check(bytes.size() == session::RequestBytes && session::DecodeRequest(bytes)->character == 100,
          "request round trip failed");
    for (size_t cut = 0; cut < bytes.size(); ++cut)
        Check(!session::DecodeRequest({bytes.data(), cut}), "truncated request accepted");
    for (size_t index : {size_t(4), size_t(7), size_t(48), size_t(49), size_t(51)})
    {
        auto bad = bytes;
        bad[index] = 255;
        Check(!session::DecodeRequest(bad), "unknown/reserved request data accepted");
    }
    session::Key key{Token(1), Token(2), Token(3), 17, 900, 100};
    TPlayerTable player{};
    player.id = 100;
    auto body = Data(player);
    auto reply = Frame(key, HEADER_DG_PLAYER_LOAD_SUCCESS, body);
    auto parsed = session::DecodeReply(reply);
    Check(parsed && parsed->key == key && parsed->payload.size() == body.size(), "reply round trip failed");
    for (size_t cut = 0; cut < session::ReplyPrefixBytes; ++cut)
        Check(!session::DecodeReply({reply.data(), cut}), "truncated envelope accepted");
    for (size_t index : {size_t(4), size_t(6), size_t(7), size_t(72), size_t(73), size_t(76)})
    {
        auto bad = reply;
        bad[index] = 255;
        Check(!session::DecodeReply(bad), "invalid version/type/reserved/length accepted");
    }
    auto wrong = player;
    wrong.id++;
    Check(!session::DecodeReply(Frame(key, HEADER_DG_PLAYER_LOAD_SUCCESS, Data(wrong))),
          "foreign character payload accepted");
    reply.push_back(0);
    Check(!session::DecodeReply(reply), "extra envelope bytes accepted");
    for (uint32_t count : {0u, 1u, UINT32_MAX})
    {
        auto items = Data(count);
        Check(session::ValidPayload(HEADER_DG_ITEM_LOAD, items, 100) == (count == 0),
              "item count/length mismatch accepted");
    }
    TQuestTable quest{};
    TPlayerItem item{};
    item.owner = 101;
    auto itemRows = Data(uint32_t(1));
    auto itemBody = Data(item);
    itemRows.insert(itemRows.end(), itemBody.begin(), itemBody.end());
    Check(!session::ValidPayload(HEADER_DG_ITEM_LOAD, itemRows, 100), "foreign item owner accepted");
    quest.dwPID = 101;
    auto quests = Data(uint32_t(1));
    auto row = Data(quest);
    quests.insert(quests.end(), row.begin(), row.end());
    Check(!session::ValidPayload(HEADER_DG_QUEST_LOAD, quests, 100), "foreign quest payload accepted");
    auto affect = Data(uint32_t(101));
    auto count = Data(uint32_t(0));
    affect.insert(affect.end(), count.begin(), count.end());
    Check(!session::ValidPayload(HEADER_DG_AFFECT_LOAD, affect, 100), "foreign affect owner accepted");
    std::vector<uint8_t> tooBig(session::ReplyPrefixBytes + session::MaximumPayload + 1);
    Check(!session::DecodeReply(tooBig), "oversized frame accepted");
}
void Lifetimes()
{
    session::Authority authority;
    session::Request request{Token(1), Token(2), 900, 100, 0};
    auto lease = authority.Open(7, 50, 900, 100, request.link, request.request);
    Check(bool(lease), "native load lease failed");
    Check(!authority.Open(7, 51, 900, 101) && !authority.Open(8, 52, 901, 100),
          "duplicate account/character load allowed");
    Check(!authority.TakePart(*lease, HEADER_DG_ITEM_LOAD), "item load preceded player");
    session::GameSession game;
    Check(game.Begin(request) && !game.Begin(request), "parallel descriptor requests allowed");
    TPlayerTable player{};
    player.id = 100;
    const auto body = Data(player);
    auto encoded = Frame(lease->key, HEADER_DG_PLAYER_LOAD_SUCCESS, body);
    auto reply = session::DecodeReply(encoded);
    Check(authority.TakePart(*lease, reply->header) && game.Accept(*reply) && game.Matches(lease->key),
          "native load not accepted");
    Check(!authority.TakePart(*lease, reply->header) && !game.Accept(*reply),
          "duplicate player callback accepted");
    Check(game.BindEntity(19) && !game.BindEntity(20) && game.MatchesEntity(lease->key, 19) &&
              !game.MatchesEntity(lease->key, 20),
          "ECS entity generation was not pinned");
    for (uint8_t part : {uint8_t(HEADER_DG_ITEM_LOAD), uint8_t(HEADER_DG_QUEST_LOAD)})
    {
        auto bytes = Data(uint32_t(0));
        auto frame = Frame(lease->key, part, bytes);
        auto r = session::DecodeReply(frame);
        Check(r && authority.TakePart(*lease, part) && game.Accept(*r), "load part refused");
        Check(!authority.TakePart(*lease, part) && !game.Accept(*r), "duplicate load part accepted");
    }
    auto forged = *lease;
    forged.peer++;
    Check(!authority.Matches(forged), "foreign peer lease accepted");
    forged = *lease;
    forged.descriptor++;
    Check(!authority.Matches(forged), "foreign descriptor lease accepted");
    for (int field = 0; field < 6; ++field)
    {
        auto key = lease->key;
        if (field == 0)
            key.link[0]++;
        if (field == 1)
            key.request[0]++;
        if (field == 2)
            key.session[0]++;
        if (field == 3)
            key.generation++;
        if (field == 4)
            key.account++;
        if (field == 5)
            key.character++;
        Check(!game.Matches(key), "different session identity matched");
    }
    authority.InvalidateAccount(900);
    game.Reset();
    Check(!authority.Matches(*lease) && !game.Matches(lease->key), "logout retained callback authority");
    Check(!game.MatchesEntity(lease->key, 19), "logout retained ECS callback authority");
    request.request = Token(4);
    Check(game.Begin(request), "relogin could not start");
    const auto replacement = authority.Open(7, 50, 900, 100, request.link, request.request);
    Check(replacement && replacement->key.generation > lease->key.generation &&
              replacement->key.session != lease->key.session,
          "same IDs reused an old lease");
    Check(!authority.Invalidate(*lease) && authority.Matches(*replacement) && !game.Accept(*reply),
          "old callback changed replacement");
    auto currentBytes = Frame(replacement->key, HEADER_DG_PLAYER_LOAD_SUCCESS, body);
    auto current = session::DecodeReply(currentBytes);
    Check(game.Accept(*current), "replacement player refused");
    auto failureBytes = Frame(replacement->key, HEADER_DG_PLAYER_LOAD_FAILED, {});
    auto failure = session::DecodeReply(failureBytes);
    Check(game.Accept(*failure) && !game.Matches(replacement->key) && !game.Accept(*current),
          "failed load became active again");
    authority.InvalidatePeer(8);
    Check(authority.Matches(*replacement), "other peer invalidated session");
    authority.InvalidatePeer(7);
    Check(!authority.Matches(*replacement), "peer loss retained session");
    // Recorded native requests cannot cross a reconnected Database link.
    game.Reset();
    request.link = Token(6);
    request.request = Token(7);
    game.Begin(request);
    Check(!game.Accept(*current), "old link response accepted after reconnect");
    for (int i = 0; i < 256; ++i)
    {
        auto l = authority.Open(7, 50, 900, 100, Token(8), session::NewToken());
        Check(bool(l), "bounded churn refused");
        Check(!authority.Matches(*lease), "old SQL callback regained authority after churn");
        authority.InvalidateAccount(900);
    }
}
void Database(unsigned int port, const std::string &schema)
{
    Check(schema.starts_with("auriga_mission_sql_test_") &&
              schema.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == schema.npos,
          "only isolated schema allowed");
    CAsyncSQL worker;
    worker.SetTimeouts(3);
    worker.SetRetryConnectionErrors(false);
    Check(worker.Setup("127.0.0.1", "root", "", "", "utf8mb4", false, port) &&
              worker.WaitForConnection(std::chrono::seconds(5)),
          "isolated SQL unavailable");
    session::Authority authority;
    session::GameSession game;
    session::Request request{Token(1), Token(2), 900, 100, 0};
    game.Begin(request);
    auto old = authority.Open(7, 50, 900, 100, request.link, request.request);
    worker.ReturnQueryPrepared("SELECT SLEEP(0.1), ?", {SqlParam::UInt(100)}, nullptr);
    authority.InvalidateAccount(900);
    game.Reset();
    request.request = Token(3);
    game.Begin(request);
    auto current = authority.Open(7, 50, 900, 100, request.link, request.request);
    worker.ReturnQueryPrepared("SELECT 0, ?", {SqlParam::UInt(100)}, nullptr);
    std::vector<std::unique_ptr<SQLMsg>> replies;
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    while (replies.size() < 2 && std::chrono::steady_clock::now() < end)
    {
        SQLMsg *message = nullptr;
        if (worker.PopResult(&message))
            replies.emplace_back(message);
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(replies.size() == 2, "delayed load replies missing");
    TPlayerTable player{};
    player.id = 100;
    auto oldBytes = Frame(old->key, HEADER_DG_PLAYER_LOAD_SUCCESS, Data(player));
    Check(!authority.Matches(*old) && !game.Accept(*session::DecodeReply(oldBytes)),
          "delayed SQL callback attached to relogin");
    auto row = replies[1]->Get()->FetchRow();
    Check(row && std::string(row[1]) == "100" && !replies[1]->uiSQLErrno, "fresh SQL read failed");
    auto freshBytes = Frame(current->key, HEADER_DG_PLAYER_LOAD_SUCCESS, Data(player));
    Check(authority.Matches(*current) && game.Accept(*session::DecodeReply(freshBytes)),
          "fresh SQL reply refused");
    authority.InvalidatePeer(7);
    game.Reset();
    Check(!authority.Matches(*current) && !game.Matches(current->key),
          "disconnect retained callback identity");
}
} // namespace
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto p = std::make_shared<spdlog::logger>("player-session-test");
    return p;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    return logging::GetLogger();
}
int main(int argc, char **argv)
{
    try
    {
        Authorization();
        Protocol();
        Lifetimes();
        if (argc == 4)
            Database(static_cast<unsigned int>(std::stoul(argv[1])), argv[2]);
        else
            Check(argc == 1, "unsupported test arguments");
        std::cout << "player session: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
