#include "TestTls.h"

#include "../../SRC/Server/LoginServer/login_auth.h"
#include "../../SRC/Server/LoginServer/login_db.h"
#include "../../SRC/Server/LoginServer/login_db_link.h"
#include "../../SRC/Server/LoginServer/login_session.h"

#include <Core/asio_engine.h>
#include <Core/Time.h>
#include <Core/tls.h>

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
using namespace test_tls;

// A production LoginSession on a loopback socket over TLS, driven by a
// scripted client: the handshake echo, HANDSHAKE_COMPLETED, then LOGIN3 -
// which TCP may split anywhere - read from the TLS stream.
namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
[[noreturn]] void Unexpected(const char* what) { throw std::runtime_error(std::string("unexpected call: ") + what); }

struct Issued { SqlParams params; };
std::vector<Issued> queries;

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

LoginCGLogin3 Login3(const char* login, const char* passwd)
{
    LoginCGLogin3 packet {};
    packet.header = HEADER_CG_LOGIN3;
    std::strncpy(packet.login, login, sizeof(packet.login) - 1);
    std::strncpy(packet.passwd, passwd, sizeof(packet.passwd) - 1);
#ifdef ENABLE_HWID
    std::strncpy(packet.hwid, "HWID-TEST", sizeof(packet.hwid) - 1);
#endif
    return packet;
}

// The client runs on its own thread (the session answers from the game
// thread once the TLS stream is up): handshake echo, HANDSHAKE_COMPLETED, then
// LOGIN3 split as given - or, with retiredHeader, the old key agreement
// header in place of the echo, which the session must answer by closing.
void Scenario(const Pem& pem, const std::vector<size_t>& loginPieces, bool retiredHeader = false, int idleStage = 0)
{
    queries.clear();
    auto context = auriga::net::TlsServerContext::FromPem(pem.cert, pem.key);
    Check(context != nullptr, "the test certificate did not load");
    tcp::acceptor acceptor(auriga::net::Io(), tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    const auto endpoint = acceptor.local_endpoint();
    LoginAuth auth;
    auto* session = new LoginSession(0x7151, &auth);

    std::atomic<bool> finished { false }, mayClose { false };
    std::atomic<bool> readyForExpiry { false };
    std::exception_ptr failure;
    std::thread client([&] {
        try {
            boost::asio::io_context io;
            ssl::context clientContext(ssl::context::tls_client);
            clientContext.set_verify_mode(ssl::verify_peer);
            clientContext.add_certificate_authority(boost::asio::buffer(pem.cert));
            ssl::stream<tcp::socket> stream(io, clientContext);
            stream.next_layer().connect(endpoint);
            stream.next_layer().set_option(tcp::no_delay(true));
            Expect(Within(io, [&](Done done) { stream.async_handshake(ssl::stream_base::client, done); }),
                "the TLS handshake failed");
            const auto send = [&](const void* data, size_t size) {
                Expect(Within(io, [&](Done done) {
                    boost::asio::async_write(stream, boost::asio::buffer(data, size), Transfer(done));
                }), "a client packet could not be written");
            };

            LoginGCHandshake hello {};
            Expect(Within(io, [&](Done done) { boost::asio::async_read(stream, boost::asio::buffer(&hello, sizeof(hello)), Transfer(done)); }),
                "no handshake from the server");
            Check(hello.bHeader == HEADER_GC_HANDSHAKE && hello.dwHandshake == 0x7151, "the handshake packet was wrong");
            if (retiredHeader) {
                // 0xec was KEY_AGREEMENT: no longer a packet, so the session
                // drops the connection without answering.
                const uint8_t retired[4] = { 0xec, 0, 0, 0 };
                send(retired, sizeof(retired));
                char extra[64];
                std::size_t got = 0;
                const auto ec = Within(io, [&](Done done) { stream.async_read_some(boost::asio::buffer(extra), Transfer(done, &got)); });
                Check(ec && got == 0, "the session answered a retired key agreement header");
            } else if (idleStage == 1) {
                readyForExpiry = true; // TLS is ready, no application handshake.
            } else {
                LoginCGHandshake echo {};
                echo.bHeader = HEADER_CG_HANDSHAKE;
                echo.dwHandshake = hello.dwHandshake;
                echo.dwTime = get_dword_time();
                send(&echo, sizeof(echo));

                LoginHandshakeCompleted completed {};
                Expect(Within(io, [&](Done done) { boost::asio::async_read(stream, boost::asio::buffer(&completed, sizeof(completed)), Transfer(done)); }),
                    "no completion after the handshake");
                Check(completed.bHeader == HEADER_GC_HANDSHAKE_COMPLETED, "the handshake was not completed");

                if (idleStage == 2) {
                    readyForExpiry = true; // Handshake complete, no LOGIN3.
                } else {

                const auto login = Login3("tester", "secret");
                const auto* bytes = reinterpret_cast<const char*>(&login);
                size_t sent = 0;
                for (const size_t piece : loginPieces) {
                    send(bytes + sent, piece);
                    sent += piece;
                    std::this_thread::sleep_for(60ms);
                }
                send(bytes + sent, sizeof(login) - sent);
                }
            }
            const auto until = std::chrono::steady_clock::now() + 5s;
            while (!mayClose && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(2ms);
            boost::system::error_code ec;
            stream.lowest_layer().close(ec);
        } catch (...) {
            failure = std::current_exception();
        }
        finished = true;
    });

    tcp::socket accepted(auriga::net::Io());
    acceptor.accept(accepted);
    Check(session->Accept(std::move(accepted), *context), "the session did not accept the connection");
    if (idleStage) {
        Check(PumpUntil([&] { return readyForExpiry.load() || finished.load(); }), "idle client did not reach its phase");
        Check(readyForExpiry, "idle client failed before its deadline");
        if (idleStage == 2) {
            session->CheckTimeouts(std::chrono::steady_clock::now() + 21s);
            Check(!session->IsClosed(), "LOGIN3 idle deadline did not reset after handshake");
        }
        session->CheckTimeouts(std::chrono::steady_clock::now() + (idleStage == 1 ? 21s : 31s));
        Check(session->IsClosed() && !session->HasPendingDbWork(), "idle protocol phase did not close");
    } else {
        PumpUntil([&] { return !queries.empty() || session->IsClosed() || finished.load(); });
    }
    mayClose = true;
    PumpUntil([&] { return finished.load(); });
    client.join();
    if (failure)
        std::rethrow_exception(failure);

    if (retiredHeader) {
        Check(queries.empty() && session->IsClosed(), "the session accepted a retired key agreement header");
    } else if (!idleStage) {
        Check(session->IsTls() && session->IsTlsReady(), "the session was not a ready TLS session");
        Check(queries.size() == 1 && queries[0].params.size() == 2 &&
            queries[0].params[0].bytes == "secret" && queries[0].params[1].bytes == "tester",
            "LOGIN3 did not arrive intact");
    }
    Check(PumpUntil([&] { return session->IsClosed() && !session->HasPendingIoWork(); }),
        "the session did not wind down after the client left");
    Check(!session->HasPendingDbWork(), "closed session retained a query reference");
    delete session;
}
}

// Database and link seams; the session and the auth are real.
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
LoginDB::LoginDB() = default;
LoginDB::~LoginDB() = default;
bool LoginDB::Connect(const LoginConfig&) { Unexpected(__func__); }
bool LoginDB::ConnectDirect(const LoginConfig&) { Unexpected(__func__); }
SQLMsg* LoginDB::DirectQuery(const char*) { Unexpected(__func__); }
void LoginDB::Query(const std::string&, const SqlParams& params, Callback) { queries.push_back({params}); }
void LoginDB::Process() {}
bool LoginDBLink::SendAuthLogin(const LoginGDAuthLogin&, RegistrationCallback) { Unexpected(__func__); }
MYSQL_ROW SQLResult::FetchRow() { Unexpected(__func__); }

int main()
{
    int result = 0;
    auriga::net::Start();
    try {
        const Pem pem = SelfSigned("login-tls-test");
        Scenario(pem, {});                      // LOGIN3 whole
        Scenario(pem, {1, 40, 2, 30});          // LOGIN3 in five pieces
        Scenario(pem, {}, true);                // the retired key agreement header
        Scenario(pem, {}, false, 1);            // TLS ready, no application handshake
        Scenario(pem, {}, false, 2);            // Handshake complete, no LOGIN3
    } catch (const std::exception& error) {
        std::cerr << "login session test failed: " << error.what() << '\n';
        result = 1;
    }
    auriga::net::Stop();
    if (result == 0)
        std::cout << "login session: " << checks << " checks passed\n";
    return result;
}
