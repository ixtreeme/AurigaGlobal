#include "login_limiter.h"

#include <algorithm>
#include <ctime>

bool LoginLimiter::AllowConnect(const std::string& host)
{
	const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
	HostState& state = m_hosts[host];

	if (state.blockedUntil != 0 && now < state.blockedUntil)
	{
		// Never cut off an address that has live players behind it (NAT,
		// shared connections); the flood guard is for new bursts only.
		return state.activeSessions > 0;
	}

	if (state.blockedUntil != 0)
	{
		state.blockedUntil = 0;
		state.connectWindowStart = 0;
		state.connects = 0;
	}

	if (state.connectWindowStart == 0 || now - state.connectWindowStart >= CONNECT_WINDOW_SECONDS)
	{
		state.connectWindowStart = now;
		state.connects = 0;
	}

	if (++state.connects > CONNECT_LIMIT)
	{
		state.blockLevel = std::min(state.blockLevel + 1, 5);

		uint64_t block = CONNECT_BLOCK_BASE_SECONDS << (state.blockLevel - 1);

		if (block > CONNECT_BLOCK_MAX_SECONDS)
			block = CONNECT_BLOCK_MAX_SECONDS;

		state.blockedUntil = now + block;
		state.connects = 0;
		return false;
	}

	return true;
}

bool LoginLimiter::AllowLoginAttempt(const std::string& host, const std::string& account)
{
	const uint64_t now = static_cast<uint64_t>(std::time(nullptr));

	const auto accountIt = m_accounts.find(account);

	if (accountIt != m_accounts.end() && accountIt->second.blockedUntil != 0)
	{
		if (now < accountIt->second.blockedUntil)
			return false;

		m_accounts.erase(accountIt);
	}

	const auto hostIt = m_hosts.find(host);

	if (hostIt != m_hosts.end() && hostIt->second.loginBlockedUntil != 0)
	{
		if (now < hostIt->second.loginBlockedUntil)
			return false;

		hostIt->second.loginBlockedUntil = 0;
		hostIt->second.loginFailures = 0;
		hostIt->second.loginWindowStart = 0;
	}

	return true;
}

void LoginLimiter::ReportLoginResult(const std::string& host, const std::string& account, bool success)
{
	const uint64_t now = static_cast<uint64_t>(std::time(nullptr));

	if (success)
	{
		m_accounts.erase(account);

		const auto hostIt = m_hosts.find(host);

		if (hostIt != m_hosts.end())
		{
			hostIt->second.loginFailures = 0;
			hostIt->second.loginWindowStart = 0;
		}

		return;
	}

	AccountState& accountState = m_accounts[account];

	if (accountState.windowStart == 0 || now - accountState.windowStart >= LOGIN_WINDOW_SECONDS)
	{
		accountState.windowStart = now;
		accountState.failures = 0;
	}

	if (++accountState.failures >= LOGIN_FAIL_LIMIT)
		accountState.blockedUntil = now + LOGIN_BLOCK_SECONDS;

	HostState& hostState = m_hosts[host];

	if (hostState.loginWindowStart == 0 || now - hostState.loginWindowStart >= LOGIN_WINDOW_SECONDS)
	{
		hostState.loginWindowStart = now;
		hostState.loginFailures = 0;
	}

	if (++hostState.loginFailures >= HOST_LOGIN_FAIL_LIMIT)
		hostState.loginBlockedUntil = now + LOGIN_BLOCK_SECONDS;
}

void LoginLimiter::SessionOpened(const std::string& host)
{
	++m_hosts[host].activeSessions;
}

void LoginLimiter::SessionClosed(const std::string& host)
{
	const auto it = m_hosts.find(host);

	if (it != m_hosts.end() && it->second.activeSessions > 0)
		--it->second.activeSessions;
}
