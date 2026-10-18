#include "../../SRC/Server/LoginServer/login_limiter.h"
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }

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
}

int main()
{
    try {
        ExistingSessionsDoNotExemptNewConnections();
        SimultaneousSessionLimitAndRelease();
        CredentialLimitsStillApply();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
    std::cout << "login limiter: " << checks << " checks passed\n";
}
