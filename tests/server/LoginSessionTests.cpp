#include "../../SRC/Server/LoginServer/login_auth.h"
#include "../../SRC/Server/LoginServer/login_db.h"
#include "../../SRC/Server/LoginServer/login_db_link.h"
#include "../../SRC/Server/LoginServer/login_session.h"

#include <Core/asio_engine.h>
#include <Core/Time.h>

#include <boost/asio.hpp>

#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using tcp = boost::asio::ip::tcp;

// A production LoginSession on a loopback socket, driven by a scripted client
// that speaks the real handshake and key agreement with its own Cipher, then
// sends LOGIN3 encrypted. TCP may split any packet; the session has to
// assemble it from the pieces whatever the cipher state.
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
    } catch (const std::exception& error) {
        std::cerr << "login session test failed: " << error.what() << '\n';
        result = 1;
    }
    auriga::net::Stop();
    if (result == 0)
        std::cout << "login session: " << checks << " checks passed\n";
    return result;
}
