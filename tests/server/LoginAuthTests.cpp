#include "../../SRC/Server/LoginServer/login_auth.h"
#include "../../SRC/Server/LoginServer/login_db.h"
#include "../../SRC/Server/LoginServer/login_db_link.h"
#include "../../SRC/Server/LoginServer/login_session.h"
#include "../../SRC/Server/LoginServer/login_limiter.h"

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
std::vector<LoginDB::Callback> queryCallbacks;
struct Registration { LoginGDAuthLogin data; LoginDBLink::RegistrationCallback callback; };
std::vector<Registration> registrations;
std::vector<uint32_t> successes;
bool linkAvailable = true;
std::vector<uint32_t> randomSamples;
int randomStatus = 1;
size_t randomCalls = 0;

int ScriptedRandom(unsigned char* output, int length)
{
    Check(length == sizeof(uint32_t), "login key random request changed size");
    const auto value = randomCalls < randomSamples.size() ? randomSamples[randomCalls] : uint32_t(0);
    ++randomCalls;
    std::memcpy(output, &value, sizeof(value));
    return randomStatus;
}

void Reset() {
    failures.clear(); queries.clear(); queryCallbacks.clear();
    registrations.clear(); successes.clear(); linkAvailable = true;
}

struct Session : LoginSession {
    using LoginSession::LoginSession;
    using LoginSession::OnPeerGone;
};

void CompleteQuery(size_t index, bool passwordMatches = true)
{
    SQLMsg msg;
    auto* result = new SQLResult;
    result->uiNumRows = 1;
    result->bPrepared = true;
    result->preparedCells = {"HASH", passwordMatches ? "HASH" : "OTHER", "000000000000000000", "42"};
#ifdef ENABLE_MULTI_LANGUAGE
    result->preparedCells.push_back("0");
#endif
    result->preparedCells.push_back("OK");
    result->preparedCells.push_back("0");
    for (int i = 0; i < 8; ++i) result->preparedCells.push_back("0");
    std::vector<char*> row;
    for (auto& cell : result->preparedCells) row.push_back(cell.data());
    result->preparedRows.push_back(std::move(row));
    msg.vec_pkResult.push_back(result);
    auto callback = std::move(queryCallbacks.at(index));
    callback(&msg);
}

void CompleteRegistration(size_t index, bool success)
{
    auto callback = std::move(registrations.at(index).callback);
    callback(success);
}

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

void OneAttemptPerSessionAndDatabaseAck()
{
    Reset(); LoginAuth auth; LoginDBLink link; auth.SetLink(&link);
    Session session(1, &auth);
    const auto alice = Packet("alice", "secret", "HWID-1");
    const auto bob = Packet("bob", "secret", "HWID-2");
    auth.RequestLogin(&session, &alice);
    auth.RequestLogin(&session, &bob);
    Check(queries.size() == 1 && session.GetLogin() == "alice" && failures.back().status == "ALREADY",
        "a second account overwrote an in-flight attempt");
    CompleteQuery(0);
    Check(successes.empty() && registrations.size() == 1 && session.HasPendingDbWork(),
        "authentication succeeded or released the session before Database ACK");
    Check(registrations[0].data.dwID == 42 && std::string(registrations[0].data.szLogin) == "alice" &&
        registrations[0].data.dwLoginKey != 0 && registrations[0].data.dwLoginKey <= 0x7fffffff,
        "registration lost the authenticated identity or key range");
    Session duplicate(2, &auth);
    auth.RequestLogin(&duplicate, &alice);
    auth.RequestLogin(&session, &bob);
    Check(queries.size() == 1 && session.GetLogin() == "alice", "registration wait allowed another lookup");
    CompleteRegistration(0, true);
    Check(successes.size() == 1 && successes[0] == registrations[0].data.dwLoginKey &&
        !session.HasPendingDbWork() && !session.CanStartLogin(), "Database ACK did not complete exactly one login");
    auth.RequestLogin(&session, &bob);
    Check(queries.size() == 1 && session.GetLogin() == "alice", "authenticated session switched accounts");
    session.OnPeerGone(); auth.OnSessionClosed(&session);
    auth.RequestLogin(&duplicate, &alice);
    Check(queries.size() == 2, "closed session left its account stuck ALREADY");
    CompleteQuery(1, false);
    Check(!duplicate.HasPendingDbWork() && duplicate.CanStartLogin() && duplicate.GetLogin().empty(),
        "failed credentials did not release the session and account");
}

void RegistrationFailuresReleaseTheAttempt()
{
    for (bool offline : {false, true}) {
        Reset(); LoginAuth auth; LoginDBLink link; auth.SetLink(&link);
        Session session(1, &auth);
        const auto packet = Packet("tester", "secret", "HWID-1");
        linkAvailable = !offline;
        auth.RequestLogin(&session, &packet); CompleteQuery(0);
        if (!offline) {
            Check(successes.empty() && session.HasPendingDbWork(), "registration was not pending");
            CompleteRegistration(0, false); // Refusal, timeout and link loss use this completion.
        }
        Check(successes.empty() && failures.size() == 1 && failures[0].status == "SHUTDOWN" &&
            !session.HasPendingDbWork() && session.CanStartLogin() && session.GetLogin().empty(),
            "failed/offline registration yielded success or stranded the attempt");
        linkAvailable = true;
        auth.RequestLogin(&session, &packet); CompleteQuery(1); CompleteRegistration(offline ? 0 : 1, true);
        Check(successes.size() == 1 && !session.HasPendingDbWork(), "registration failure prevented retry");
        session.OnPeerGone(); auth.OnSessionClosed(&session);
    }
}

void DisconnectWhileWaiting()
{
    for (bool duringRegistration : {false, true}) {
        Reset(); LoginAuth auth; LoginDBLink link; auth.SetLink(&link);
        Session session(1, &auth);
        const auto packet = Packet("tester", "secret", "HWID-1");
        auth.RequestLogin(&session, &packet);
        if (duringRegistration) CompleteQuery(0);
        session.OnPeerGone();
        Check(!session.HasPendingDbWork(), "disconnect retained an active attempt");
        if (duringRegistration) CompleteRegistration(0, true);
        else CompleteQuery(0);
        Check(successes.empty() && failures.empty() && !session.HasPendingDbWork(),
            "late result answered a closed session or retained it");
        auth.OnSessionClosed(&session);
        Session next(2, &auth); auth.RequestLogin(&next, &packet);
        Check(queries.size() == 2, "disconnect left a pending or successful login behind");
        CompleteQuery(1, false);
    }
}

void AttemptDeadlinesAndLateResults()
{
    for (bool duringRegistration : {false, true}) {
        Reset(); LoginAuth auth; LoginDBLink link; auth.SetLink(&link);
        auto session = std::make_unique<Session>(1, &auth);
        const auto packet = Packet("tester", "secret", "HWID-1");
        auth.RequestLogin(session.get(), &packet);
        if (duringRegistration) CompleteQuery(0);
        auth.Process(std::chrono::steady_clock::now() + std::chrono::seconds(11));
        Check(!session->HasPendingDbWork() && session->CanStartLogin() && failures.size() == 1 &&
            failures.back().status == "SHUTDOWN", "expired attempt retained the session or account");
        auth.RequestLogin(session.get(), &packet);
        Check(queries.size() == 2 && session->HasPendingDbWork(), "timeout prevented a fresh attempt");
        if (duringRegistration) CompleteRegistration(0, true);
        else CompleteQuery(0);
        Check(successes.empty() && session->HasPendingDbWork(), "stale completion affected the replacement attempt");
        session->OnPeerGone();
        Check(!session->HasPendingDbWork(), "closed replacement retained its callback reference");
        session.reset();
        CompleteQuery(1); // ASAN must not find a dangling session.
        Check(successes.empty() && registrations.size() == (duringRegistration ? 1 : 0),
            "late query authenticated a destroyed session");
    }
}

void SqlFailureReleasesAttempt()
{
    Reset(); LoginAuth auth; Session session(1, &auth);
    const auto packet = Packet("tester", "secret", "HWID-1");
    auth.RequestLogin(&session, &packet);
    SQLMsg failure; failure.uiSQLErrno = CR_SERVER_LOST;
    queryCallbacks[0](&failure);
    Check(!session.HasPendingDbWork() && session.CanStartLogin() && failures.back().status == "SHUTDOWN",
        "SQL failure was treated as valid credentials or retained the attempt");
    auth.RequestLogin(&session, &packet);
    CompleteQuery(1, false);
    Check(queries.size() == 2 && !session.HasPendingDbWork(), "SQL failure prevented retry");
}

void RandomFailuresAndZeroSamples()
{
    // A provider failure can leave bytes in its output. None of those bytes
    // may become a registered key; repeated zero output must also terminate.
    for (int status : {0, -1, 1}) {
        Reset(); LoginAuth auth(&ScriptedRandom); LoginDBLink link; auth.SetLink(&link);
        Session session(1, &auth);
        randomStatus = status; randomCalls = 0;
        randomSamples = status == 1 ? std::vector<uint32_t>{} : std::vector<uint32_t>{123};
        const auto packet = Packet("tester", "secret", "HWID-1");
        auth.RequestLogin(&session, &packet); CompleteQuery(0);
        Check(registrations.empty() && successes.empty() && failures.size() == 1 && failures[0].status == "SHUTDOWN",
            "random failure registered or issued a key");
        Check(!session.HasPendingDbWork() && session.CanStartLogin() && session.GetLogin().empty(),
            "random failure stranded the session or account");
        Check(randomCalls == (status == 1 ? 8 : 1), "random failure was not bounded");

        randomStatus = 1; randomCalls = 0;
        randomSamples = {0, 0x80000000, 0xffffffff};
        auth.RequestLogin(&session, &packet); CompleteQuery(1);
        Check(registrations.size() == 1 && registrations[0].data.dwLoginKey == 0x7fffffff && randomCalls == 3,
            "zero rejection or the compatible key range is broken");
        Check(successes.empty(), "random key bypassed Database acknowledgement");
        CompleteRegistration(0, true);
        Check(successes.size() == 1 && successes[0] == 0x7fffffff && !session.HasPendingDbWork(),
            "random failure prevented a successful retry");
        session.OnPeerGone();
    }
}

void LimiterCapacityRejectsBeforeSql()
{
    Reset(); LoginLimiter limiter;
    const auto now = LoginLimiter::Clock::now();
    for (size_t i = 0; i < LoginLimiter::MAX_TRACKED_ACCOUNTS; ++i)
        Check(limiter.AllowLoginAttempt("192.0.2.1", "capacity" + std::to_string(i), now), "auth fixture reached capacity early");
    LoginAuth auth; auth.SetLimiter(&limiter);
    Session session(1, &auth); session.SetHost("192.0.2.1");
    const auto overflow = Packet("overflow", "secret", "HWID-1");
    auth.RequestLogin(&session, &overflow);
    Check(Rejected(session, "BLOCK"), "full limiter admitted an untracked SQL lookup");
    const auto tracked = Packet("capacity0", "secret", "HWID-1");
    auth.RequestLogin(&session, &tracked);
    Check(queries.size() == 1 && session.HasPendingDbWork(), "capacity rejected an already tracked account");
    CompleteQuery(0, false);
    Check(!session.HasPendingDbWork() && failures.back().status == "WRONGPWD", "admitted lookup lost its failure at capacity");
}
}

// Session, database and link seams. The session is a real LoginSession object
// whose network half is never started.
LoginSession::LoginSession(uint32_t handshake, LoginAuth* auth) : m_handshake(handshake), m_auth(auth) {}
void LoginSession::OnInput() { Unexpected(__func__); }
void LoginSession::OnPeerGone() { m_closed = true; if (m_auth) m_auth->OnSessionClosed(this); }
void LoginSession::SendLoginFailure(const char* status) { failures.push_back({this, status}); }
void LoginSession::SendAuthSuccess(uint32_t key) { successes.push_back(key); }

CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
CAsyncSQL::CAsyncSQL() : m_queueLane(0) {}
CAsyncSQL::~CAsyncSQL() = default;
LoginDB::LoginDB() = default;
LoginDB::~LoginDB() = default;
bool LoginDB::Connect(const LoginConfig&) { Unexpected(__func__); }
bool LoginDB::ConnectDirect(const LoginConfig&) { Unexpected(__func__); }
SQLMsg* LoginDB::DirectQuery(const char*) { Unexpected(__func__); }
void LoginDB::Query(const std::string& sql, const SqlParams& params, Callback callback) {
    queries.push_back({sql, params}); queryCallbacks.push_back(std::move(callback));
}
void LoginDB::Process() {}
LoginDBLink::LoginDBLink() = default;
void LoginDBLink::OnInput() { Unexpected(__func__); }
void LoginDBLink::OnPeerGone() { Unexpected(__func__); }
bool LoginDBLink::SendAuthLogin(const LoginGDAuthLogin& data, RegistrationCallback callback) {
    if (!linkAvailable) return false;
    registrations.push_back({data, std::move(callback)}); return true;
}
MYSQL_ROW SQLResult::FetchRow() { return preparedRowPos < preparedRows.size() ? preparedRows[preparedRowPos++].data() : nullptr; }

int main()
{
    try {
        TerminatedFields();
        UnterminatedFields();
        NoTerminatorAnywhere();
        OneAttemptPerSessionAndDatabaseAck();
        RegistrationFailuresReleaseTheAttempt();
        DisconnectWhileWaiting();
        AttemptDeadlinesAndLateResults();
        SqlFailureReleasesAttempt();
        RandomFailuresAndZeroSamples();
        LimiterCapacityRejectsBeforeSql();
    } catch (const std::exception& error) {
        std::cerr << "login auth test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "login auth: " << checks << " checks passed\n";
    return 0;
}
