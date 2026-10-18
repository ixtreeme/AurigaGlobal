#include "../../SRC/Server/LoginServer/login_db_link.h"
#include <Core/Logging.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// The production LoginServer/login_db_link.cpp against a scripted Database on
// a loopback socket, with the interserver key set: a valid challenge links,
// and every way the handshake can be refused - a wrong server MAC, a
// malformed challenge, a failed result - drops the link without touching the
// buffers the drop released (it used to read on and crash).
namespace {
using namespace std::chrono_literals;
using tcp = boost::asio::ip::tcp;

int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }

constexpr uint8_t DG_PING = 213;
constexpr uint8_t DG_AUTH_CHALLENGE = 215;
constexpr uint8_t DG_AUTH_RESULT = 216;
constexpr uint8_t GD_AUTH = 122;
constexpr uint8_t GD_AUTH_LOGIN = 100;
constexpr uint8_t DG_AUTH_LOGIN = 122;
constexpr size_t HEADER_SIZE = 9;

using Bytes = std::vector<uint8_t>;

Bytes Frame(uint8_t header, const Bytes& payload, uint32_t handle = 0)
{
    Bytes out(HEADER_SIZE + payload.size());
    const uint32_t size = static_cast<uint32_t>(payload.size());
    out[0] = header;
    std::memcpy(out.data() + 1, &handle, sizeof(handle));
    std::memcpy(out.data() + 5, &size, sizeof(size));
    if (!payload.empty())
        std::memcpy(out.data() + HEADER_SIZE, payload.data(), payload.size());
    return out;
}

Bytes& operator+=(Bytes& lhs, const Bytes& rhs)
{
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}

struct Challenge
{
    uint8_t nonce[PeerAuth::NONCE_SIZE] {};
    uint64_t timestamp { 0 };

    Bytes Payload(bool validMac)
    {
        PeerAuth::MakeNonce(nonce);
        timestamp = PeerAuth::NowSeconds();
        Bytes payload(PeerAuth::NONCE_SIZE + PeerAuth::TIMESTAMP_SIZE + PeerAuth::MAC_SIZE, 0);
        std::memcpy(payload.data(), nonce, PeerAuth::NONCE_SIZE);
        PeerAuth::EncodeTimestamp(timestamp, payload.data() + PeerAuth::NONCE_SIZE);
        if (validMac)
            PeerAuth::ComputeAuthMac(PeerAuth::ROLE_SERVER, nonce, timestamp,
                payload.data() + PeerAuth::NONCE_SIZE + PeerAuth::TIMESTAMP_SIZE);
        return payload;
    }
};

void SetReceiveTimeout(tcp::socket& socket, int milliseconds)
{
#ifdef _WIN32
    const DWORD value = static_cast<DWORD>(milliseconds);
    ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
#else
    timeval value { milliseconds / 1000, (milliseconds % 1000) * 1000 };
    ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
#endif
}

// The Database side: accepts one connection and runs a script on it, all on
// its own thread with blocking calls bounded by timeouts.
class ScriptedDatabase
{
public:
    using Script = std::function<void(ScriptedDatabase&, tcp::socket&)>;

    explicit ScriptedDatabase(Script script)
        : m_acceptor(m_io, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0))
    {
        m_thread = std::thread([this, script = std::move(script)] { Run(script); });
    }

    ~ScriptedDatabase()
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    uint16_t Port() const { return m_acceptor.local_endpoint().port(); }
    bool Finished() const { return m_finished; }
    bool Failed() const { return !m_failure.empty(); }
    const std::string& Failure() const { return m_failure; }

    // Blocks until the link closes its side; true when it did.
    bool WaitForClose(tcp::socket& socket)
    {
        char sink[256];
        boost::system::error_code ec;
        for (;;) {
            socket.read_some(boost::asio::buffer(sink), ec);
            if (!ec)
                continue;
            return ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset ||
                ec == boost::asio::error::connection_aborted;
        }
    }

    std::atomic<bool> sawClose { false };
    std::atomic<bool> clientMacValid { false };
    std::atomic<bool> releaseLinkedPeer { false };
    std::atomic<bool> registrationsRead { false };

private:
    void Run(const Script& script)
    {
        try {
            boost::system::error_code ec;
            tcp::socket socket(m_io);
            m_acceptor.non_blocking(true);
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            for (;;) {
                m_acceptor.accept(socket, ec);
                if (!ec)
                    break;
                if (ec != boost::asio::error::would_block || std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("the link never connected");
                std::this_thread::sleep_for(2ms);
            }
            socket.non_blocking(false);
            SetReceiveTimeout(socket, 5000);
            script(*this, socket);
        } catch (const std::exception& error) {
            m_failure = error.what();
        }
        m_finished = true;
    }

    boost::asio::io_context m_io;
    tcp::acceptor m_acceptor;
    std::thread m_thread;
    std::atomic<bool> m_finished { false };
    std::string m_failure;
};

// The game-thread side: drains the io completions until the condition holds.
bool PumpUntil(const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        auriga::net::DrainCompletions();
        if (condition())
            return true;
        std::this_thread::sleep_for(2ms);
    }
    auriga::net::DrainCompletions();
    return condition();
}

void Connect(LoginDBLink& link, const ScriptedDatabase& db)
{
    link.Configure("127.0.0.1", db.Port());
    link.Tick();
}

void Settle(LoginDBLink& link, ScriptedDatabase& db)
{
    Check(PumpUntil([&] { return db.Finished(); }), "the scripted database finished");
    Check(!db.Failed(), db.Failure().c_str());
    Check(PumpUntil([&] { return !link.HasPendingIoWork(); }), "no io work outlives the scenario");
}

// The refusal scripts send the refused frame and a ping right behind it, in
// one write, so the link reads on past the frame that dropped it.
void RefusedHandshake(const char* what, const std::function<Bytes()>& frames)
{
    ScriptedDatabase db([&](ScriptedDatabase& self, tcp::socket& socket) {
        Bytes bytes = frames();
        bytes += Frame(DG_PING, {});
        boost::asio::write(socket, boost::asio::buffer(bytes));
        self.sawClose = self.WaitForClose(socket);
    });
    LoginDBLink link;
    Connect(link, db);
    Settle(link, db);
    Check(db.sawClose, what);
    Check(!link.IsLinked(), "a refused handshake leaves the link down");
}

void WrongServerMacDropsTheLink()
{
    RefusedHandshake("a challenge with a wrong server MAC closes the link", [] {
        Challenge challenge;
        return Frame(DG_AUTH_CHALLENGE, challenge.Payload(false));
    });
}

void MalformedChallengeDropsTheLink()
{
    RefusedHandshake("a challenge of the wrong size closes the link", [] {
        return Frame(DG_AUTH_CHALLENGE, Bytes(10, 0x5a));
    });
}

// A valid challenge, then a refusing result.
void FailedResultDropsTheLink()
{
    ScriptedDatabase db([](ScriptedDatabase& self, tcp::socket& socket) {
        Challenge challenge;
        boost::asio::write(socket, boost::asio::buffer(Frame(DG_AUTH_CHALLENGE, challenge.Payload(true))));
        Bytes response(HEADER_SIZE + PeerAuth::MAC_SIZE);
        boost::asio::read(socket, boost::asio::buffer(response));
        Bytes bytes = Frame(DG_AUTH_RESULT, { 0 });
        bytes += Frame(DG_PING, {});
        boost::asio::write(socket, boost::asio::buffer(bytes));
        self.sawClose = self.WaitForClose(socket);
    });
    LoginDBLink link;
    Connect(link, db);
    Settle(link, db);
    Check(db.sawClose, "a refusing auth result closes the link");
    Check(!link.IsLinked(), "a refused result leaves the link down");
}

void ValidHandshakeLinks()
{
    ScriptedDatabase db([](ScriptedDatabase& self, tcp::socket& socket) {
        Challenge challenge;
        boost::asio::write(socket, boost::asio::buffer(Frame(DG_AUTH_CHALLENGE, challenge.Payload(true))));
        Bytes response(HEADER_SIZE + PeerAuth::MAC_SIZE);
        boost::asio::read(socket, boost::asio::buffer(response));
        uint32_t size = 0;
        std::memcpy(&size, response.data() + 5, sizeof(size));
        self.clientMacValid = response[0] == GD_AUTH && size == PeerAuth::MAC_SIZE &&
            PeerAuth::VerifyAuthMac(PeerAuth::ROLE_CLIENT, challenge.nonce, challenge.timestamp, response.data() + HEADER_SIZE);
        boost::asio::write(socket, boost::asio::buffer(Frame(DG_AUTH_RESULT, { 1 })));
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!self.releaseLinkedPeer && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(2ms);
        boost::system::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);
        socket.close(ec);
    });
    LoginDBLink link;
    Connect(link, db);
    Check(PumpUntil([&] { return link.IsLinked(); }), "a valid challenge links");
    Check(db.clientMacValid, "the link answers with the client MAC over the same challenge");
    db.releaseLinkedPeer = true;
    Check(PumpUntil([&] { return !link.IsLinked(); }), "the link is down once the database goes away");
    Settle(link, db);
}

void Authenticate(tcp::socket& socket)
{
    Challenge challenge;
    boost::asio::write(socket, boost::asio::buffer(Frame(DG_AUTH_CHALLENGE, challenge.Payload(true))));
    Bytes response(HEADER_SIZE + PeerAuth::MAC_SIZE);
    boost::asio::read(socket, boost::asio::buffer(response));
    if (response[0] != GD_AUTH || !PeerAuth::VerifyAuthMac(PeerAuth::ROLE_CLIENT,
        challenge.nonce, challenge.timestamp, response.data() + HEADER_SIZE))
        throw std::runtime_error("wrong client authentication");
    boost::asio::write(socket, boost::asio::buffer(Frame(DG_AUTH_RESULT, {1})));
}

uint32_t ReadRegistration(tcp::socket& socket, uint32_t expectedKey)
{
    LoginDBHeader header {};
    do {
        boost::asio::read(socket, boost::asio::buffer(&header, sizeof(header)));
    } while (header.bHeader == 120 && header.dwSize == 0); // Optional GD_PING.
    if (header.bHeader != GD_AUTH_LOGIN || header.dwSize != sizeof(LoginGDAuthLogin) || header.dwHandle == 0)
        throw std::runtime_error("invalid registration frame");
    LoginGDAuthLogin registration {};
    boost::asio::read(socket, boost::asio::buffer(&registration, sizeof(registration)));
    if (registration.dwLoginKey != expectedKey)
        throw std::runtime_error("registration payload changed");
    return header.dwHandle;
}

void RegistrationsUseCorrelatedAcknowledgements()
{
    ScriptedDatabase db([](ScriptedDatabase& self, tcp::socket& socket) {
        Authenticate(socket);
        const auto first = ReadRegistration(socket, 42), second = ReadRegistration(socket, 43);
        if (first == second) throw std::runtime_error("registration handles reused");
        self.registrationsRead = true;
        Bytes replies = Frame(DG_AUTH_LOGIN, {1}, second + 100); // Unknown request.
        replies += Frame(DG_AUTH_LOGIN, {0}, second);
        replies += Frame(DG_AUTH_LOGIN, {1}, first);
        replies += Frame(DG_AUTH_LOGIN, {1}, first); // Duplicate ACK.
        boost::asio::write(socket, boost::asio::buffer(replies));
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!self.releaseLinkedPeer && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(2ms);
        boost::system::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);
        socket.close(ec);
    });
    LoginDBLink link;
    Connect(link, db);
    Check(PumpUntil([&] { return link.IsLinked(); }), "registration fixture did not link");
    std::vector<std::pair<int, bool>> results;
    LoginGDAuthLogin request {}; request.dwLoginKey = 42;
    Check(link.SendAuthLogin(request, [&](bool ok) { results.emplace_back(1, ok); }), "first registration refused");
    request.dwLoginKey = 43;
    Check(link.SendAuthLogin(request, [&](bool ok) { results.emplace_back(2, ok); }), "second registration refused");
    Check(results.empty(), "registration completed before a reply");
    Check(PumpUntil([&] { return results.size() == 2; }), "registration ACKs were not dispatched");
    Check(results == std::vector<std::pair<int, bool>>{{2, false}, {1, true}}, "out-of-order ACKs reached wrong requests");
    // Expiring an unrelated period must not repeat already completed callbacks.
    link.Tick(std::chrono::steady_clock::now() + 11s);
    Check(results.size() == 2 && link.IsLinked(), "duplicate ACK or expired completed request ran twice");
    db.releaseLinkedPeer = true;
    Check(PumpUntil([&] { return !link.IsLinked(); }), "acknowledged link did not close");
    Settle(link, db);
    Check(results.size() == 2, "duplicate ACK completed a request more than once");
}

void RegistrationFailure(const std::string& mode)
{
    ScriptedDatabase db([&](ScriptedDatabase& self, tcp::socket& socket) {
        Authenticate(socket);
        const auto handle = ReadRegistration(socket, 42);
        self.registrationsRead = true;
        if (mode == "malformed") {
            boost::asio::write(socket, boost::asio::buffer(Frame(DG_AUTH_LOGIN, {}, handle)));
            self.sawClose = self.WaitForClose(socket);
        } else if (mode == "timeout") {
            self.sawClose = self.WaitForClose(socket);
        } // "disconnect": close the peer as the script returns.
    });
    LoginDBLink link;
    Connect(link, db);
    Check(PumpUntil([&] { return link.IsLinked(); }), "failure fixture did not link");
    int completed = 0;
    bool successful = true;
    LoginGDAuthLogin request {}; request.dwLoginKey = 42;
    Check(link.SendAuthLogin(request, [&](bool ok) { ++completed; successful = ok; }), "registration not queued");
    Check(PumpUntil([&] { return db.registrationsRead.load(); }), "script did not receive registration");
    if (mode == "timeout") link.Tick(std::chrono::steady_clock::now() + 11s);
    Check(PumpUntil([&] { return completed == 1 && !link.IsLinked(); }), "failure left a pending registration");
    Check(!successful, "failed registration reported success");
    Settle(link, db);
    Check(completed == 1, "failure completed a registration more than once");
}

void OfflineRegistrationIsNotQueued()
{
    LoginDBLink link; LoginGDAuthLogin request {};
    int calls = 0;
    Check(!link.SendAuthLogin(request, [&](bool) { ++calls; }), "offline link accepted a registration");
    link.Tick(std::chrono::steady_clock::now() + 11s);
    Check(calls == 0, "unaccepted registration retained its callback");
}
}

std::shared_ptr<spdlog::logger> logging::GetErrorLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("login-link-error-test");
    return logger;
}
std::shared_ptr<spdlog::logger> logging::GetLogger()
{
    static auto logger = std::make_shared<spdlog::logger>("login-link-test");
    return logger;
}

int main()
{
    int result = 0;
    PeerAuth::SetKey("login-db-link-test-key");
    auriga::net::Start();
    try {
        ValidHandshakeLinks();
        WrongServerMacDropsTheLink();
        MalformedChallengeDropsTheLink();
        FailedResultDropsTheLink();
        OfflineRegistrationIsNotQueued();
        RegistrationsUseCorrelatedAcknowledgements();
        for (const auto mode : {"disconnect", "malformed", "timeout"}) RegistrationFailure(mode);
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << "\n";
        result = 1;
    }
    auriga::net::Stop();
    if (result == 0)
        std::cout << "login db link: " << checks << " checks passed\n";
    return result;
}
