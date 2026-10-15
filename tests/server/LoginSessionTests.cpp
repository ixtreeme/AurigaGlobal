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

// A production LoginSession on a loopback socket, driven by a scripted client
// that speaks the real handshake and key agreement with its own Cipher, then
// sends LOGIN3 encrypted. TCP may split any packet; the session has to
// assemble it from the pieces whatever the cipher state. Over TLS the same
// session skips the key agreement and reads LOGIN3 from the TLS stream.
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

// Lets the io thread deliver whatever was sent and the session consume it.
void Settle()
{
    const auto until = std::chrono::steady_clock::now() + 60ms;
    while (std::chrono::steady_clock::now() < until) {
        auriga::net::DrainCompletions();
        std::this_thread::sleep_for(2ms);
    }
}

class Client
{
public:
    explicit Client(uint16_t port) : m_socket(m_io)
    {
        m_socket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port));
        m_socket.set_option(tcp::no_delay(true));
    }

    template <typename T>
    T Receive()
    {
        T packet {};
        boost::asio::read(m_socket, boost::asio::buffer(&packet, sizeof(packet)));
        return packet;
    }

    // Sends in pieces of the given sizes (the rest goes last), letting the
    // server process each piece on its own.
    void Send(const void* data, size_t size, std::vector<size_t> pieces = {})
    {
        const auto* bytes = static_cast<const char*>(data);
        size_t sent = 0;
        for (const size_t piece : pieces) {
            boost::asio::write(m_socket, boost::asio::buffer(bytes + sent, piece));
            sent += piece;
            Settle();
        }
        boost::asio::write(m_socket, boost::asio::buffer(bytes + sent, size - sent));
        Settle();
    }

    void Close() { boost::system::error_code ec; m_socket.close(ec); }

    Cipher cipher;

private:
    boost::asio::io_context m_io;
    tcp::socket m_socket;
};

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

// One connection through handshake and key agreement, the plaintext key
// agreement and the encrypted LOGIN3 split as given. (The handshake echo
// stays whole: it carries a timestamp the server checks to 50 ms.)
void Scenario(const std::vector<size_t>& agreementPieces, const std::vector<size_t>& loginPieces)
{
    queries.clear();
    tcp::acceptor acceptor(auriga::net::Io(), tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    LoginAuth auth;
    auto* session = new LoginSession(0x5eed1234, &auth);
    Client client(acceptor.local_endpoint().port());
    tcp::socket accepted(auriga::net::Io());
    acceptor.accept(accepted);
    Check(session->Accept(std::move(accepted)), "the session did not accept the connection");

    const auto hello = client.Receive<LoginGCHandshake>();
    Check(hello.bHeader == HEADER_GC_HANDSHAKE && hello.dwHandshake == 0x5eed1234, "no handshake from the server");
    LoginCGHandshake echo {};
    echo.bHeader = HEADER_CG_HANDSHAKE;
    echo.dwHandshake = hello.dwHandshake;
    echo.dwTime = get_dword_time();
    echo.lDelta = 0;
    client.Send(&echo, sizeof(echo));

    const auto offer = client.Receive<LoginKeyAgreement>();
    Check(offer.bHeader == HEADER_GC_KEY_AGREEMENT, "no key agreement after the handshake");
    LoginKeyAgreement answer {};
    answer.bHeader = HEADER_CG_KEY_AGREEMENT;
    size_t length = LoginKeyAgreement::MAX_DATA_LEN;
    const size_t agreed = client.cipher.Prepare(answer.data, &length);
    Check(agreed == offer.wAgreedLength, "client and server disagree on the key length");
    answer.wAgreedLength = static_cast<uint16_t>(agreed);
    answer.wDataLength = static_cast<uint16_t>(length);
    Check(client.cipher.Activate(true, offer.wAgreedLength, offer.data, offer.wDataLength), "client key agreement failed");
    client.Send(&answer, sizeof(answer), agreementPieces);

    const auto completed = client.Receive<LoginKeyAgreementCompleted>();
    Check(completed.bHeader == HEADER_GC_KEY_AGREEMENT_COMPLETED, "the key agreement was not completed");

    auto login = Login3("tester", "secret");
    client.cipher.Encrypt(&login, sizeof(login));
    client.Send(&login, sizeof(login), loginPieces);

    Check(PumpUntil([&] { return !queries.empty() || session->IsClosed(); }), "the session neither looked up nor closed");
    Check(!session->IsClosed() && queries.size() == 1 && queries[0].params.size() == 2 &&
        queries[0].params[0].bytes == "secret" && queries[0].params[1].bytes == "tester",
        "the encrypted LOGIN3 did not arrive intact");

    client.Close();
    Check(PumpUntil([&] { return session->IsClosed() && !session->HasPendingIoWork(); }),
        "the session did not wind down after the client left");
    session->EndDbQuery();
    delete session;
}

// A TLS client on its own thread (the session answers from the game thread
// once the TLS stream is up): handshake echo, then KEY_AGREEMENT_COMPLETED
// with no key agreement, then LOGIN3 split as given - or, with
// sendKeyAgreement, a key agreement in place of the echo, which the TLS
// session must answer by closing.
void TlsScenario(const Pem& pem, const std::vector<size_t>& loginPieces, bool sendKeyAgreement = false)
{
    queries.clear();
    auto context = auriga::net::TlsServerContext::FromPem(pem.cert, pem.key);
    Check(context != nullptr, "the test certificate did not load");
    tcp::acceptor acceptor(auriga::net::Io(), tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    const auto endpoint = acceptor.local_endpoint();
    LoginAuth auth;
    auto* session = new LoginSession(0x7151, &auth);

    std::atomic<bool> finished { false }, mayClose { false };
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
            if (sendKeyAgreement) {
                // Instead of the echo: the session must drop the connection
                // without answering.
                LoginKeyAgreement agreement {};
                agreement.bHeader = HEADER_CG_KEY_AGREEMENT;
                send(&agreement, sizeof(agreement));
                char extra[64];
                std::size_t got = 0;
                const auto ec = Within(io, [&](Done done) { stream.async_read_some(boost::asio::buffer(extra), Transfer(done, &got)); });
                Check(ec && got == 0, "the TLS session answered a key agreement");
            } else {
                LoginCGHandshake echo {};
                echo.bHeader = HEADER_CG_HANDSHAKE;
                echo.dwHandshake = hello.dwHandshake;
                echo.dwTime = get_dword_time();
                send(&echo, sizeof(echo));

                LoginKeyAgreementCompleted completed {};
                Expect(Within(io, [&](Done done) { boost::asio::async_read(stream, boost::asio::buffer(&completed, sizeof(completed)), Transfer(done)); }),
                    "no completion after the handshake");
                Check(completed.bHeader == HEADER_GC_KEY_AGREEMENT_COMPLETED,
                    "a TLS session did not go straight to the completion");

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
    Check(session->Accept(std::move(accepted), context.get()), "the session did not accept the connection");
    PumpUntil([&] { return !queries.empty() || session->IsClosed() || finished.load(); });
    mayClose = true;
    PumpUntil([&] { return finished.load(); });
    client.join();
    if (failure)
        std::rethrow_exception(failure);

    if (sendKeyAgreement) {
        Check(queries.empty() && session->IsClosed(), "a TLS session accepted a key agreement");
    } else {
        Check(session->IsTls() && session->IsTlsReady(), "the session was not a ready TLS session");
        Check(queries.size() == 1 && queries[0].params.size() == 2 &&
            queries[0].params[0].bytes == "secret" && queries[0].params[1].bytes == "tester",
            "LOGIN3 over TLS did not arrive intact");
    }
    Check(PumpUntil([&] { return session->IsClosed() && !session->HasPendingIoWork(); }),
        "the session did not wind down after the client left");
    if (!sendKeyAgreement)
        session->EndDbQuery();
    delete session;
}
}

// Database and link seams; the session, the auth and the cipher are real.
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
void LoginDBLink::SendAuthLogin(const LoginGDAuthLogin&) { Unexpected(__func__); }
MYSQL_ROW SQLResult::FetchRow() { Unexpected(__func__); }

int main()
{
    int result = 0;
    auriga::net::Start();
    try {
        Scenario({}, {});                       // every packet whole
        Scenario({7, 100}, {10});               // key agreement in three, LOGIN3 in two
        Scenario({}, {1, 40, 2, 30});           // LOGIN3 in five pieces
        const Pem pem = SelfSigned("login-tls-test");
        TlsScenario(pem, {});                   // over TLS, LOGIN3 whole
        TlsScenario(pem, {1, 40, 2, 30});       // over TLS, LOGIN3 in five pieces
        TlsScenario(pem, {}, true);             // a key agreement on a TLS session
    } catch (const std::exception& error) {
        std::cerr << "login session test failed: " << error.what() << '\n';
        result = 1;
    }
    auriga::net::Stop();
    if (result == 0)
        std::cout << "login session: " << checks << " checks passed\n";
    return result;
}
