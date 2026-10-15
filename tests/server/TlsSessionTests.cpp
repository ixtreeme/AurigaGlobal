#include <Core/peer_session.h>
#include <Core/tls.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

// PeerSession over TLS on a loopback socket: an echo session behind the
// production TLS server context, clients on their own thread - a TLS client
// that trusts only the test certificate, a plaintext client, an outdated TLS
// client and one that never says anything.
namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }

using Done = std::function<void(const boost::system::error_code&)>;

// Runs one asynchronous client operation for at most five seconds and returns
// its result: a server that stops answering fails the scenario instead of
// hanging it.
boost::system::error_code Within(boost::asio::io_context& io, const std::function<void(Done)>& start)
{
    std::optional<boost::system::error_code> result;
    start([&](const boost::system::error_code& ec) { result = ec; });
    io.restart();
    io.run_for(5s);
    if (!result)
        throw std::runtime_error("the server did not answer within five seconds");
    return *result;
}

void Expect(const boost::system::error_code& ec, const char* what)
{
    if (ec)
        throw std::runtime_error(std::string(what) + ": " + ec.message());
}

// Read/write completion adapted to Within.
auto Transfer(Done done, std::size_t* bytes = nullptr)
{
    return [done, bytes](const boost::system::error_code& ec, std::size_t transferred) {
        if (bytes)
            *bytes = transferred;
        done(ec);
    };
}

struct Pem { std::string cert, key; };

std::string Drain(BIO* bio)
{
    char* data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    return std::string(data, static_cast<size_t>(size));
}

// A self-signed P-256 certificate, made fresh for the run.
Pem SelfSigned(const char* commonName)
{
    EVP_PKEY* key = EVP_EC_gen("P-256");
    X509* cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_getm_notBefore(cert), -60);
    X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
    X509_set_pubkey(cert, key);
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(commonName), -1, -1, 0);
    X509_set_issuer_name(cert, name);
    X509_sign(cert, key, EVP_sha256());

    BIO* certBio = BIO_new(BIO_s_mem());
    BIO* keyBio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(certBio, cert);
    PEM_write_bio_PrivateKey(keyBio, key, nullptr, nullptr, 0, nullptr, nullptr);
    Pem pem { Drain(certBio), Drain(keyBio) };
    BIO_free(certBio);
    BIO_free(keyBio);
    X509_free(cert);
    EVP_PKEY_free(key);
    return pem;
}

// Echoes every byte back; greets before the handshake is done, so the
// greeting must wait for the TLS stream.
class EchoSession : public PeerSession
{
public:
    void Accept(tcp::socket&& socket, auriga::net::TlsServerContext& context)
    {
        AttachTlsSocket(std::move(socket), context);
        CreateBuffers(64 * 1024, 1024 * 1024);
        QueueOutput("HELLO", 5);
        StartRead();
    }

    bool Gone() const { return m_gone; }
    const std::string& Received() const { return m_received; }
    int Inputs() const { return m_inputs; }

    void Close()
    {
        if (m_gone)
            return;
        m_gone = true;
        DestroySession();
    }

protected:
    void OnInput() override
    {
        ++m_inputs;
        const int size = static_cast<int>(buffer_size(m_lpInputBuffer));
        const char* data = static_cast<const char*>(buffer_read_peek(m_lpInputBuffer));
        m_received.append(data, static_cast<size_t>(size));
        QueueOutput(data, size);
        buffer_read_proceed(m_lpInputBuffer, size);
    }

    void OnPeerGone() override { Close(); }

private:
    bool m_gone { false };
    int m_inputs { 0 };
    std::string m_received;
};

struct Server
{
    tcp::acceptor acceptor { auriga::net::Io(), tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0) };
    uint16_t Port() const { return acceptor.local_endpoint().port(); }
};

// Runs the client on its own thread and pumps the game thread meanwhile;
// the session is accepted as soon as the client connected.
void Run(Server& server, EchoSession& session, auriga::net::TlsServerContext& context,
    const std::function<void()>& client, const std::function<bool()>& done = {})
{
    std::exception_ptr failure;
    std::thread thread([&] {
        try { client(); } catch (...) { failure = std::current_exception(); }
    });
    tcp::socket accepted(auriga::net::Io());
    server.acceptor.accept(accepted);
    session.Accept(std::move(accepted), context);
    // Without a condition of its own the scenario ends with the session.
    const auto finished = done ? done : [&] { return session.Gone() && !session.HasPendingIoWork(); };
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline && !finished()) {
        auriga::net::DrainCompletions();
        session.CheckHeartbeat();
        std::this_thread::sleep_for(1ms);
    }
    thread.join();
    auriga::net::DrainCompletions();
    if (failure)
        std::rethrow_exception(failure);
}

void WaitGone(EchoSession& session)
{
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline && !(session.Gone() && !session.HasPendingIoWork())) {
        auriga::net::DrainCompletions();
        session.CheckHeartbeat();
        std::this_thread::sleep_for(1ms);
    }
}

void RoundTrip(const Pem& pem)
{
    auto context = auriga::net::TlsServerContext::FromPem(pem.cert, pem.key);
    Check(context != nullptr, "the server context did not load the test certificate");
    Server server;
    auto* session = new EchoSession();

    std::string payload(300 * 1024 + 101, '\0');
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i * 31 + 7);
    std::string echoed;
    std::atomic<bool> clientDone { false };

    Run(server, *session, *context, [&] {
        boost::asio::io_context io;
        ssl::context clientContext(ssl::context::tls_client);
        clientContext.set_verify_mode(ssl::verify_peer);
        clientContext.add_certificate_authority(boost::asio::buffer(pem.cert));
        ssl::stream<tcp::socket> stream(io, clientContext);
        stream.next_layer().connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), server.Port()));
        Expect(Within(io, [&](Done done) { stream.async_handshake(ssl::stream_base::client, done); }),
            "the client handshake failed");

        char hello[5];
        Expect(Within(io, [&](Done done) { boost::asio::async_read(stream, boost::asio::buffer(hello), Transfer(done)); }),
            "the greeting was not readable");
        Check(std::string(hello, 5) == "HELLO", "the greeting queued before the handshake did not arrive");

        // Pieces of 1, 100 and the rest; the echo is read only at the end.
        const std::pair<size_t, size_t> pieces[] = {{0, 1}, {1, 100}, {101, payload.size() - 101}};
        for (const auto& [offset, size] : pieces)
            Expect(Within(io, [&](Done done) {
                boost::asio::async_write(stream, boost::asio::buffer(payload.data() + offset, size), Transfer(done));
            }), "the payload could not be written");
        echoed.resize(payload.size());
        Expect(Within(io, [&](Done done) { boost::asio::async_read(stream, boost::asio::buffer(echoed), Transfer(done)); }),
            "the echo was not readable");
        boost::system::error_code ec;
        stream.lowest_layer().close(ec);
        clientDone = true;
    }, [&] { return clientDone.load(); });

    Check(echoed == payload, "the echo did not match what was sent");
    WaitGone(*session);
    Check(session->IsTls() && session->IsTlsReady(), "the session was not a ready TLS session");
    Check(session->Received() == payload, "the server did not read the plaintext as sent");
    Check(session->Gone() && !session->HasPendingIoWork(), "the session did not wind down after the client left");
    delete session;
}

void PlaintextClient(const Pem& pem)
{
    auto context = auriga::net::TlsServerContext::FromPem(pem.cert, pem.key);
    Server server;
    auto* session = new EchoSession();
    std::string seen;

    Run(server, *session, *context, [&] {
        boost::asio::io_context io;
        tcp::socket socket(io);
        socket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), server.Port()));
        const std::string bytes(64, 'A');
        Expect(Within(io, [&](Done done) { boost::asio::async_write(socket, boost::asio::buffer(bytes), Transfer(done)); }),
            "the plaintext bytes could not be written");
        // Whatever the server sends until it drops the connection.
        char chunk[512];
        for (;;) {
            std::size_t n = 0;
            if (Within(io, [&](Done done) { socket.async_read_some(boost::asio::buffer(chunk), Transfer(done, &n)); }))
                break;
            seen.append(chunk, n);
        }
    });

    WaitGone(*session);
    Check(session->Gone() && !session->IsTlsReady() && session->Inputs() == 0,
        "a plaintext client got past the TLS handshake");
    Check(seen.find("HELLO") == std::string::npos, "queued output went out in plaintext");
    delete session;
}

void OutdatedClient(const Pem& pem)
{
    auto context = auriga::net::TlsServerContext::FromPem(pem.cert, pem.key);
    Server server;
    auto* session = new EchoSession();
    bool clientFailed = false;

    Run(server, *session, *context, [&] {
        boost::asio::io_context io;
        ssl::context clientContext(ssl::context::tls_client);
        SSL_CTX_set_security_level(clientContext.native_handle(), 0);
        SSL_CTX_set_min_proto_version(clientContext.native_handle(), TLS1_VERSION);
        SSL_CTX_set_max_proto_version(clientContext.native_handle(), TLS1_1_VERSION);
        clientContext.set_verify_mode(ssl::verify_none);
        ssl::stream<tcp::socket> stream(io, clientContext);
        stream.next_layer().connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), server.Port()));
        clientFailed = static_cast<bool>(
            Within(io, [&](Done done) { stream.async_handshake(ssl::stream_base::client, done); }));
        boost::system::error_code ec;
        stream.lowest_layer().close(ec);
    });

    WaitGone(*session);
    Check(clientFailed, "a TLS 1.1 client completed the handshake");
    Check(session->Gone() && !session->IsTlsReady() && session->Inputs() == 0, "a TLS 1.1 session became ready");
    delete session;
}

void SilentClient(const Pem& pem)
{
    auto context = auriga::net::TlsServerContext::FromPem(pem.cert, pem.key);
    Server server;
    auto* session = new EchoSession();
    session->SetTlsHandshakeTimeout(200ms);
    const auto started = std::chrono::steady_clock::now();
    std::atomic<bool> stop { false };

    Run(server, *session, *context, [&] {
        boost::asio::io_context io;
        tcp::socket socket(io);
        socket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), server.Port()));
        // Gives up on its own, so a missing deadline fails instead of hanging.
        while (!stop && std::chrono::steady_clock::now() - started < 6s)
            std::this_thread::sleep_for(5ms);
    }, [&] {
        if (session->Gone() && !session->HasPendingIoWork())
            stop = true;
        return stop.load();
    });

    const auto elapsed = std::chrono::steady_clock::now() - started;
    Check(session->Gone() && !session->IsTlsReady(), "a peer that never handshakes kept its session");
    Check(elapsed >= 200ms && elapsed < 5s, "the handshake deadline was not what was set");
    delete session;
}

void BadCredentials(const Pem& pem)
{
    const Pem other = SelfSigned("other");
    Check(auriga::net::TlsServerContext::FromPem(pem.cert, other.key) == nullptr, "a key that does not match was accepted");
    Check(auriga::net::TlsServerContext::FromPem("not a certificate", pem.key) == nullptr, "garbage was accepted as a certificate");
    Check(auriga::net::TlsServerContext::FromPem(pem.cert, "not a key") == nullptr, "garbage was accepted as a key");
    Check(auriga::net::TlsServerContext::FromFiles("missing-cert.pem", "missing-key.pem") == nullptr, "missing files were accepted");
}
}

int main()
{
    int result = 0;
    auriga::net::Start();
    try {
        const Pem pem = SelfSigned("tls-test");
        RoundTrip(pem);
        PlaintextClient(pem);
        OutdatedClient(pem);
        SilentClient(pem);
        BadCredentials(pem);
    } catch (const std::exception& error) {
        std::cerr << "tls session test failed: " << error.what() << '\n';
        result = 1;
    }
    auriga::net::Stop();
    if (result == 0)
        std::cout << "tls session: " << checks << " checks passed\n";
    return result;
}
