#pragma once

#include <chrono>
#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

// All calls run on the game thread. Existing sessions never exempt new
// connections from rate limits. Durations use only the monotonic clock.
class LoginLimiter
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr std::size_t MAX_TRACKED_HOSTS = 65536;
    static constexpr std::size_t MAX_TRACKED_ACCOUNTS = 65536;
    static constexpr std::size_t COLLECT_BUDGET = 256;
    static constexpr auto IDLE_RETENTION = std::chrono::minutes(15);

    LoginLimiter() = default;
    LoginLimiter(const LoginLimiter&) = delete;
    LoginLimiter& operator=(const LoginLimiter&) = delete;

    bool AllowConnect(const std::string& host, TimePoint now = Clock::now());
    bool AllowLoginAttempt(const std::string& host, const std::string& account, TimePoint now = Clock::now());
    void ReportLoginResult(const std::string& host, const std::string& account, bool success, TimePoint now = Clock::now());
    // An admitted connection is counted before the next game-loop collection.
    void SessionOpened(const std::string& host, TimePoint now = Clock::now());
    void SessionClosed(const std::string& host, TimePoint now = Clock::now());

    void Collect(TimePoint now = Clock::now());
    std::size_t TrackedHostCount() const { return m_hosts.size(); }
    std::size_t TrackedAccountCount() const { return m_accounts.size(); }

private:
    static constexpr auto CONNECT_WINDOW = std::chrono::seconds(10);
    static constexpr int CONNECT_LIMIT = 30;
    static constexpr int MAX_SESSIONS_PER_HOST = 128;
    static constexpr int CONNECT_BLOCK_BASE_SECONDS = 30;
    static constexpr int CONNECT_BLOCK_MAX_SECONDS = 600;
    static constexpr auto LOGIN_WINDOW = std::chrono::seconds(300);
    static constexpr int LOGIN_FAIL_LIMIT = 5;
    static constexpr auto LOGIN_BLOCK = std::chrono::seconds(300);
    static constexpr int HOST_LOGIN_FAIL_LIMIT = 10;

    using Order = std::list<std::string>;
    struct HostState
    {
        int connects { 0 };
        std::optional<TimePoint> connectWindowStart;
        std::optional<TimePoint> blockedUntil;
        int blockLevel { 0 };
        int activeSessions { 0 };
        int loginFailures { 0 };
        std::optional<TimePoint> loginWindowStart;
        std::optional<TimePoint> loginBlockedUntil;
        TimePoint lastSeen;
        Order::iterator order;
    };
    struct AccountState
    {
        int failures { 0 };
        std::optional<TimePoint> windowStart;
        std::optional<TimePoint> blockedUntil;
        TimePoint lastSeen;
        Order::iterator order;
    };
    HostState* TrackHost(const std::string& host, TimePoint now);
    AccountState* TrackAccount(const std::string& account, TimePoint now);
    void Touch(HostState& state, TimePoint now);
    void Touch(AccountState& state, TimePoint now);
    void RemoveAccount(const std::string& account);

    Order m_hostOrder;
    Order m_accountOrder;
    std::unordered_map<std::string, HostState> m_hosts;
    std::unordered_map<std::string, AccountState> m_accounts;
};
