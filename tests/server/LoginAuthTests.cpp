#include "../../SRC/Server/LoginServer/login_auth.h"
#include "../../SRC/Server/LoginServer/login_db.h"
#include "../../SRC/Server/LoginServer/login_db_link.h"
#include "../../SRC/Server/LoginServer/login_session.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// The production LoginAuth (login_auth.cpp) over a recording session and
// account database: what the server answers and what it asks the database
// for, starting from the raw LOGIN3 bytes a client sent.
namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
[[noreturn]] void Unexpected(const char* what) { throw std::runtime_error(std::string("unexpected call: ") + what); }

struct Failure { const LoginSession* session; std::string status; };
std::vector<Failure> failures;
struct Issued { std::string sql; SqlParams params; };
std::vector<Issued> queries;

void Reset() { failures.clear(); queries.clear(); }

LoginCGLogin3 Packet(const char* login, const char* passwd, const char* hwid)
{
    LoginCGLogin3 packet {};
    packet.header = HEADER_CG_LOGIN3;
    std::strncpy(packet.login, login, sizeof(packet.login) - 1);
    std::strncpy(packet.passwd, passwd, sizeof(packet.passwd) - 1);
    for (uint32_t& key : packet.adwClientKey)
        key = 0x61616161;
#ifdef ENABLE_HWID
    std::strncpy(packet.hwid, hwid, sizeof(packet.hwid) - 1);
#else
    (void)hwid;
#endif
    return packet;
}

bool Rejected(const LoginSession& session, const char* status)
{
    return failures.size() == 1 && failures[0].session == &session && failures[0].status == status &&
        queries.empty() && session.GetLogin().empty() && !session.HasPendingDbWork();
}

bool Queried(const LoginSession& session, const char* login, const char* passwd)
{
    return failures.empty() && queries.size() == 1 && queries[0].params.size() == 2 &&
        queries[0].params[0].bytes == passwd && queries[0].params[1].bytes == login &&
        session.GetLogin() == login && session.HasPendingDbWork();
}

void TerminatedFields()
{
    LoginAuth auth;
    LoginSession session(1, &auth);
    Reset();
    const auto packet = Packet("Tester", "secret", "HWID-1");
    auth.RequestLogin(&session, &packet);
    Check(Queried(session, "tester", "secret"), "a well-formed LOGIN3 was not looked up as sent");
}

void UnterminatedFields()
{
    // Each field filled to its last byte; the packet otherwise valid.
    {
        LoginAuth auth;
        LoginSession session(1, &auth);
        Reset();
        auto packet = Packet("tester", "secret", "HWID-1");
        std::memset(packet.passwd, 's', sizeof(packet.passwd));
        auth.RequestLogin(&session, &packet);
        Check(Rejected(session, "NOID"), "an unterminated password was read past its field");

        // The rejected name is not left pending: the next request for it runs.
        LoginSession next(2, &auth);
        Reset();
        const auto good = Packet("tester", "secret", "HWID-1");
        auth.RequestLogin(&next, &good);
        Check(Queried(next, "tester", "secret"), "a rejected LOGIN3 left its name pending");
    }
#ifdef ENABLE_HWID
    {
        LoginAuth auth;
        LoginSession session(1, &auth);
        Reset();
        auto packet = Packet("tester", "secret", "HWID-1");
        std::memset(packet.hwid, 'h', sizeof(packet.hwid));
        auth.RequestLogin(&session, &packet);
        Check(Rejected(session, "NOID"), "an unterminated hwid was accepted for the account lookup");
    }
#endif
    {
        LoginAuth auth;
        LoginSession session(1, &auth);
        Reset();
        auto packet = Packet("tester", "secret", "HWID-1");
        std::memset(packet.login, 'a', sizeof(packet.login));
        auth.RequestLogin(&session, &packet);
        Check(Rejected(session, "NOID"), "an unterminated login was accepted");
    }
}

void NoTerminatorAnywhere()
{
    // The packet is the last thing in its allocation and holds no zero byte:
    // reading any field as a C string runs off the end (ASan reports it).
    LoginAuth auth;
    LoginSession session(1, &auth);
    Reset();
    auto storage = std::make_unique<unsigned char[]>(sizeof(LoginCGLogin3));
    std::memset(storage.get(), 'a', sizeof(LoginCGLogin3));
    auth.RequestLogin(&session, reinterpret_cast<const LoginCGLogin3*>(storage.get()));
    Check(Rejected(session, "NOID"), "a LOGIN3 without any terminator was accepted");
}
}

// Session, database and link seams. The session is a real LoginSession object
// whose network half is never started.
LoginSession::LoginSession(uint32_t handshake, LoginAuth* auth) : m_handshake(handshake), m_auth(auth) {}
void LoginSession::OnInput() { Unexpected(__func__); }
void LoginSession::OnPeerGone() { Unexpected(__func__); }
void LoginSession::SendLoginFailure(const char* status) { failures.push_back({this, status}); }
void LoginSession::SendAuthSuccess(uint32_t) { Unexpected(__func__); }

CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
LoginDB::LoginDB() = default;
LoginDB::~LoginDB() = default;
bool LoginDB::Connect(const LoginConfig&) { Unexpected(__func__); }
bool LoginDB::ConnectDirect(const LoginConfig&) { Unexpected(__func__); }
SQLMsg* LoginDB::DirectQuery(const char*) { Unexpected(__func__); }
void LoginDB::Query(const std::string& sql, const SqlParams& params, Callback) { queries.push_back({sql, params}); }
void LoginDB::Process() {}
void LoginDBLink::SendAuthLogin(const LoginGDAuthLogin&) { Unexpected(__func__); }
MYSQL_ROW SQLResult::FetchRow() { Unexpected(__func__); }

int main()
{
    try {
        TerminatedFields();
        UnterminatedFields();
        NoTerminatorAnywhere();
    } catch (const std::exception& error) {
        std::cerr << "login auth test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "login auth: " << checks << " checks passed\n";
    return 0;
}
