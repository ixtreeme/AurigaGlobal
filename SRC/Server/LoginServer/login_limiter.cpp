#include "login_limiter.h"

#include <algorithm>
#include <iterator>

void LoginLimiter::Touch(HostState& state, TimePoint now)
{
    state.lastSeen = now;
    m_hostOrder.splice(m_hostOrder.end(), m_hostOrder, state.order);
}

void LoginLimiter::Touch(AccountState& state, TimePoint now)
{
    state.lastSeen = now;
    m_accountOrder.splice(m_accountOrder.end(), m_accountOrder, state.order);
}

LoginLimiter::HostState* LoginLimiter::TrackHost(const std::string& host, TimePoint now)
{
    if (const auto it = m_hosts.find(host); it != m_hosts.end()) {
        Touch(it->second, now);
        return &it->second;
    }
    if (m_hosts.size() >= MAX_TRACKED_HOSTS)
        Collect(now);
    if (m_hosts.size() >= MAX_TRACKED_HOSTS)
        return nullptr;
    m_hostOrder.push_back(host);
    try {
        auto& state = m_hosts[host];
        state.order = std::prev(m_hostOrder.end());
        state.lastSeen = now;
        return &state;
    } catch (...) { m_hostOrder.pop_back(); throw; }
}

LoginLimiter::AccountState* LoginLimiter::TrackAccount(const std::string& account, TimePoint now)
{
    if (const auto it = m_accounts.find(account); it != m_accounts.end()) {
        Touch(it->second, now);
        return &it->second;
    }
    if (m_accounts.size() >= MAX_TRACKED_ACCOUNTS)
        Collect(now);
    if (m_accounts.size() >= MAX_TRACKED_ACCOUNTS)
        return nullptr;
    m_accountOrder.push_back(account);
    try {
        auto& state = m_accounts[account];
        state.order = std::prev(m_accountOrder.end());
        state.lastSeen = now;
        return &state;
    } catch (...) { m_accountOrder.pop_back(); throw; }
}

void LoginLimiter::RemoveAccount(const std::string& account)
{
    const auto it = m_accounts.find(account);
    if (it == m_accounts.end())
        return;
    m_accountOrder.erase(it->second.order);
    m_accounts.erase(it);
}

bool LoginLimiter::AllowConnect(const std::string& host, TimePoint now)
{
    auto* state = TrackHost(host, now);
    if (!state || state->activeSessions >= MAX_SESSIONS_PER_HOST)
        return false;
    if (state->blockedUntil) {
        if (now < *state->blockedUntil)
            return false;
        state->blockedUntil.reset();
        state->connectWindowStart.reset();
        state->connects = 0;
    }
    if (!state->connectWindowStart || now - *state->connectWindowStart >= CONNECT_WINDOW) {
        state->connectWindowStart = now;
        state->connects = 0;
    }
    if (++state->connects > CONNECT_LIMIT) {
        state->blockLevel = std::min(state->blockLevel + 1, 5);
        const auto seconds = std::min(CONNECT_BLOCK_BASE_SECONDS << (state->blockLevel - 1), CONNECT_BLOCK_MAX_SECONDS);
        state->blockedUntil = now + std::chrono::seconds(seconds);
        state->connects = 0;
        return false;
    }
    return true;
}

bool LoginLimiter::AllowLoginAttempt(const std::string& host, const std::string& account, TimePoint now)
{
    if (const auto it = m_accounts.find(account); it != m_accounts.end()) {
        Touch(it->second, now);
        if (it->second.blockedUntil) {
            if (now < *it->second.blockedUntil)
                return false;
            RemoveAccount(account);
        }
    }
    auto* hostState = TrackHost(host, now);
    if (!hostState)
        return false;
    if (hostState->loginBlockedUntil) {
        if (now < *hostState->loginBlockedUntil)
            return false;
        hostState->loginBlockedUntil.reset();
        hostState->loginFailures = 0;
        hostState->loginWindowStart.reset();
    }
    // Reserve state before querying SQL: an admitted failure must be tracked
    // even when other requests fill the map while that query is in flight.
    return TrackAccount(account, now) != nullptr;
}

void LoginLimiter::ReportLoginResult(const std::string& host, const std::string& account, bool success, TimePoint now)
{
    if (success) {
        RemoveAccount(account);
        const auto it = m_hosts.find(host);
        if (it != m_hosts.end()) {
            Touch(it->second, now);
            it->second.loginFailures = 0;
            it->second.loginWindowStart.reset();
        }
        return;
    }
    if (auto* state = TrackAccount(account, now)) {
        if (!state->windowStart || now - *state->windowStart >= LOGIN_WINDOW) {
            state->windowStart = now;
            state->failures = 0;
        }
        state->failures = std::min(state->failures + 1, LOGIN_FAIL_LIMIT);
        if (state->failures >= LOGIN_FAIL_LIMIT)
            state->blockedUntil = now + LOGIN_BLOCK;
    }
    if (auto* state = TrackHost(host, now)) {
        if (!state->loginWindowStart || now - *state->loginWindowStart >= LOGIN_WINDOW) {
            state->loginWindowStart = now;
            state->loginFailures = 0;
        }
        state->loginFailures = std::min(state->loginFailures + 1, HOST_LOGIN_FAIL_LIMIT);
        if (state->loginFailures >= HOST_LOGIN_FAIL_LIMIT)
            state->loginBlockedUntil = now + LOGIN_BLOCK;
    }
}

void LoginLimiter::SessionOpened(const std::string& host, TimePoint now)
{
    if (auto* state = TrackHost(host, now))
        ++state->activeSessions;
}

void LoginLimiter::SessionClosed(const std::string& host, TimePoint now)
{
    const auto it = m_hosts.find(host);
    if (it != m_hosts.end()) {
        Touch(it->second, now);
        if (it->second.activeSessions > 0)
            --it->second.activeSessions;
    }
}

void LoginLimiter::Collect(TimePoint now)
{
    // The lists are in last-use order. Each pass has bounded work, including
    // protected live hosts, so cleanup cannot stall a busy game loop.
    for (std::size_t checked = 0; checked < COLLECT_BUDGET && !m_hostOrder.empty(); ++checked) {
        const auto it = m_hosts.find(m_hostOrder.front());
        auto& state = it->second;
        if (now - state.lastSeen < IDLE_RETENTION)
            break;
        if (state.activeSessions > 0 || (state.blockedUntil && now < *state.blockedUntil) ||
            (state.loginBlockedUntil && now < *state.loginBlockedUntil)) {
            Touch(state, now);
            continue;
        }
        m_hostOrder.erase(state.order);
        m_hosts.erase(it);
    }
    for (std::size_t checked = 0; checked < COLLECT_BUDGET && !m_accountOrder.empty(); ++checked) {
        const auto it = m_accounts.find(m_accountOrder.front());
        auto& state = it->second;
        if (now - state.lastSeen < IDLE_RETENTION)
            break;
        if (state.blockedUntil && now < *state.blockedUntil) {
            Touch(state, now);
            continue;
        }
        m_accountOrder.erase(state.order);
        m_accounts.erase(it);
    }
}
