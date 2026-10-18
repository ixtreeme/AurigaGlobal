#include "../../SRC/Server/LoginServer/login_limiter.h"
#include <iostream>
#include <stdexcept>
#include <chrono>

namespace {
int checks = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
using namespace std::chrono_literals;
using TimePoint = LoginLimiter::TimePoint;
static_assert(LoginLimiter::Clock::is_steady);

void ExistingSessionsDoNotExemptNewConnections()
{
    LoginLimiter limiter;
    const std::string host = "192.0.2.1";
    Check(limiter.AllowConnect(host), "first connection rejected");
    limiter.SessionOpened(host);
    for (int i = 1; i < 30; ++i) Check(limiter.AllowConnect(host), "normal NAT burst rejected early");
    Check(!limiter.AllowConnect(host), "connection burst was not blocked");
    for (int i = 0; i < 100; ++i) Check(!limiter.AllowConnect(host), "live session bypassed the block");
    Check(limiter.AllowConnect("192.0.2.2"), "one address blocked another address");
    limiter.SessionClosed(host);
    Check(!limiter.AllowConnect(host), "closing the old session lifted the time block");
}

void SimultaneousSessionLimitAndRelease()
{
    LoginLimiter limiter;
    const std::string host = "192.0.2.3";
    // Existing sessions may have been admitted across several rate windows.
    for (int i = 0; i < 128; ++i) limiter.SessionOpened(host);
    Check(!limiter.AllowConnect(host), "unbounded simultaneous sessions allowed");
    limiter.SessionClosed(host);
    Check(limiter.AllowConnect(host), "released capacity was not reusable");
    for (int i = 0; i < 200; ++i) limiter.SessionClosed(host);
    Check(limiter.AllowConnect(host), "extra closes underflowed session accounting");
}

void CredentialLimitsStillApply()
{
    LoginLimiter limiter;
    for (int i = 0; i < 5; ++i) limiter.ReportLoginResult("192.0.2.4", "alice", false);
    Check(!limiter.AllowLoginAttempt("192.0.2.5", "alice"), "account lock did not cross addresses");
    Check(limiter.AllowLoginAttempt("192.0.2.4", "bob"), "account lock became an early host lock");
    for (int i = 0; i < 5; ++i) limiter.ReportLoginResult("192.0.2.4", "bob", false);
    Check(!limiter.AllowLoginAttempt("192.0.2.4", "charlie"), "host credential lock missing");
    Check(limiter.AllowLoginAttempt("192.0.2.5", "charlie"), "host lock affected another address");
}

void MonotonicWindowsAndBlockBoundaries()
{
    const TimePoint start {};
    LoginLimiter limiter;
    for (int i = 0; i < 30; ++i) Check(limiter.AllowConnect("burst", start), "zero-epoch window reset between requests");
    Check(!limiter.AllowConnect("burst", start), "burst threshold changed");
    limiter.Collect(start + 29999ms);
    Check(!limiter.AllowConnect("burst", start + 29999ms), "block expired before its monotonic deadline");
    for (int i = 0; i < 30; ++i) Check(limiter.AllowConnect("burst", start + 30s), "block not released at its deadline");
    Check(!limiter.AllowConnect("burst", start + 30s), "second burst not blocked");
    Check(!limiter.AllowConnect("burst", start + 89999ms), "escalated block expired early");
    Check(limiter.AllowConnect("burst", start + 90s), "escalated block did not expire");

    LoginLimiter window;
    for (int i = 0; i < 30; ++i) Check(window.AllowConnect("normal", start), "connection window threshold changed");
    Check(window.AllowConnect("normal", start + 10s), "connection window did not reset at ten seconds");

    LoginLimiter credentials;
    for (int i = 0; i < 5; ++i) credentials.ReportLoginResult("host", "locked", false, start);
    credentials.Collect(start + 299999ms);
    Check(!credentials.AllowLoginAttempt("other", "locked", start + 299999ms), "cleanup lifted an active account lock");
    Check(credentials.AllowLoginAttempt("other", "locked", start + 300s), "account lock deadline changed");

    LoginLimiter failures;
    for (int i = 0; i < 4; ++i) failures.ReportLoginResult("host", "window", false, start);
    failures.ReportLoginResult("host", "window", false, start + 300s);
    Check(failures.AllowLoginAttempt("host", "window", start + 300s), "old failures survived the credential window");
}

void CleanupIsBoundedAndKeepsLiveSessions()
{
    const TimePoint start {};
    LoginLimiter limiter;
    const auto count = LoginLimiter::COLLECT_BUDGET * 3 + 17;
    for (size_t i = 0; i < count; ++i)
        Check(limiter.AllowLoginAttempt("host" + std::to_string(i), "account" + std::to_string(i), start), "fixture admission failed");
    limiter.SessionOpened("host0", start);
    limiter.Collect(start + LoginLimiter::IDLE_RETENTION - 1ms);
    Check(limiter.TrackedHostCount() == count && limiter.TrackedAccountCount() == count, "state collected before retention elapsed");
    const auto expired = start + LoginLimiter::IDLE_RETENTION;
    for (int pass = 0; pass < 5; ++pass) {
        const auto hosts = limiter.TrackedHostCount(), accounts = limiter.TrackedAccountCount();
        limiter.Collect(expired);
        Check(hosts - limiter.TrackedHostCount() <= LoginLimiter::COLLECT_BUDGET &&
            accounts - limiter.TrackedAccountCount() <= LoginLimiter::COLLECT_BUDGET, "cleanup exceeded its per-map budget");
    }
    Check(limiter.TrackedHostCount() == 1 && limiter.TrackedAccountCount() == 0, "inactive state leaked or active host was removed");
    for (int i = 1; i < 128; ++i) limiter.SessionOpened("host0", expired);
    limiter.Collect(expired + LoginLimiter::IDLE_RETENTION);
    Check(!limiter.AllowConnect("host0", expired + LoginLimiter::IDLE_RETENTION), "cleanup forgot the active session count");
    for (int i = 0; i < 128; ++i) limiter.SessionClosed("host0", expired + LoginLimiter::IDLE_RETENTION);
    limiter.Collect(expired + LoginLimiter::IDLE_RETENTION * 2);
    Check(limiter.TrackedHostCount() == 0, "closed host never expired");
}

void RecentlyUsedEntriesSurviveCleanup()
{
    const TimePoint start {};
    LoginLimiter limiter;
    limiter.AllowLoginAttempt("first", "first", start);
    limiter.AllowLoginAttempt("stale", "stale", start + 1s);
    limiter.AllowLoginAttempt("young", "young", start + 2s);
    limiter.AllowLoginAttempt("first", "first", start + 899s);
    limiter.Collect(start + 901s);
    Check(limiter.TrackedHostCount() == 2 && limiter.TrackedAccountCount() == 2,
        "recent touches prevented stale cleanup or were themselves collected");
    limiter.ReportLoginResult("first", "first", true, start + 901s);
    limiter.Collect(start + 1801s);
    Check(limiter.TrackedHostCount() == 0 && limiter.TrackedAccountCount() == 0,
        "successful-account removal corrupted cleanup order");
}

void HostCapacityDoesNotEvictProtection()
{
    const TimePoint start {};
    LoginLimiter limiter;
    for (size_t i = 0; i < LoginLimiter::MAX_TRACKED_HOSTS - 2; ++i)
        Check(limiter.AllowConnect("host" + std::to_string(i), start), "host capacity reached early");
    limiter.AllowConnect("live", start);
    for (int i = 0; i < 128; ++i) limiter.SessionOpened("live", start);
    for (int i = 0; i < 31; ++i) limiter.AllowConnect("blocked", start);
    for (int i = 0; i < 1024; ++i)
        Check(!limiter.AllowConnect("overflow" + std::to_string(i), start), "host state grew past its cap");
    Check(limiter.TrackedHostCount() == LoginLimiter::MAX_TRACKED_HOSTS, "host capacity bound changed");
    Check(!limiter.AllowConnect("live", start) && !limiter.AllowConnect("blocked", start), "capacity pressure removed live/blocked protection");
    limiter.SessionClosed("live", start);
    Check(limiter.AllowConnect("live", start), "tracked host could not reuse released session capacity");
    Check(limiter.AllowConnect("fresh", start + LoginLimiter::IDLE_RETENTION), "expired host capacity could not be reclaimed");
    Check(limiter.TrackedHostCount() <= LoginLimiter::MAX_TRACKED_HOSTS, "reclaimed host capacity exceeded its cap");
}

void AccountCapacityReservesAdmittedFailures()
{
    const TimePoint start {};
    LoginLimiter limiter;
    for (int i = 0; i < 5; ++i) limiter.ReportLoginResult("lockhost", "protected", false, start);
    for (size_t i = 0; i < LoginLimiter::MAX_TRACKED_ACCOUNTS - 1; ++i)
        Check(limiter.AllowLoginAttempt("shared", "account" + std::to_string(i), start), "account capacity reached early");
    for (int i = 0; i < 1024; ++i)
        Check(!limiter.AllowLoginAttempt("shared", "overflow" + std::to_string(i), start), "account state grew past its cap");
    Check(limiter.TrackedAccountCount() == LoginLimiter::MAX_TRACKED_ACCOUNTS, "account capacity bound changed");
    Check(!limiter.AllowLoginAttempt("elsewhere", "protected", start), "capacity pressure lifted an account lock");
    Check(limiter.AllowLoginAttempt("shared", "account0", start), "tracked account was rejected at capacity");
    for (int i = 0; i < 5; ++i) limiter.ReportLoginResult("shared", "account0", false, start);
    Check(!limiter.AllowLoginAttempt("elsewhere", "account0", start), "admitted failures were lost at capacity");
    limiter.ReportLoginResult("shared", "account0", true, start);
    Check(limiter.AllowLoginAttempt("shared", "fresh", start), "success did not release account capacity");
    const auto expired = start + LoginLimiter::IDLE_RETENTION;
    for (size_t pass = 0; pass <= LoginLimiter::MAX_TRACKED_ACCOUNTS / LoginLimiter::COLLECT_BUDGET; ++pass)
        limiter.Collect(expired);
    Check(limiter.TrackedAccountCount() == 0, "inactive account state did not drain");
    Check(limiter.AllowLoginAttempt("newhost", "newaccount", expired), "limiter did not recover after a full stale map");
}
}

int main()
{
    try {
        ExistingSessionsDoNotExemptNewConnections();
        SimultaneousSessionLimitAndRelease();
        CredentialLimitsStillApply();
        MonotonicWindowsAndBlockBoundaries();
        CleanupIsBoundedAndKeepsLiveSessions();
        RecentlyUsedEntriesSurviveCleanup();
        HostCapacityDoesNotEvictProtection();
        AccountCapacityReservesAdmittedFailures();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
    std::cout << "login limiter: " << checks << " checks passed\n";
}
