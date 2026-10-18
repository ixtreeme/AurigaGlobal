#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

// Login server abuse guards.
//
// Two independent layers:
//  - connection flood guard: a permissive per-host new-connection window
//    (slow lines and a single long handshake never trip it). Hosts with an
//    established session keep it; the limit applies only to new connections.
//  - login attempt guard: brute force is limited on the credential check,
//    per account (lockout) and per host (a looser limit).
class LoginLimiter
{
public:
	// Game thread. Returns false when the new connection is rejected.
	bool AllowConnect(const std::string& host);

	// Game thread. Checked when a LOGIN3 arrives.
	bool AllowLoginAttempt(const std::string& host, const std::string& account);

	// Game thread. Outcome of the credential validation.
	void ReportLoginResult(const std::string& host, const std::string& account, bool success);

	void SessionOpened(const std::string& host);
	void SessionClosed(const std::string& host);

private:
	static constexpr int CONNECT_WINDOW_SECONDS = 10;
	static constexpr int CONNECT_LIMIT = 30;
	static constexpr int MAX_SESSIONS_PER_HOST = 128;
	static constexpr int CONNECT_BLOCK_BASE_SECONDS = 30;
	static constexpr int CONNECT_BLOCK_MAX_SECONDS = 600;

	static constexpr int LOGIN_WINDOW_SECONDS = 300;
	static constexpr int LOGIN_FAIL_LIMIT = 5;
	static constexpr int LOGIN_BLOCK_SECONDS = 300;
	static constexpr int HOST_LOGIN_FAIL_LIMIT = 10;

	struct HostState
	{
		int		connects { 0 };
		uint64_t	connectWindowStart { 0 };
		uint64_t	blockedUntil { 0 };
		int		blockLevel { 0 };
		int		activeSessions { 0 };

		int		loginFailures { 0 };
		uint64_t	loginWindowStart { 0 };
		uint64_t	loginBlockedUntil { 0 };
	};

	struct AccountState
	{
		int		failures { 0 };
		uint64_t	windowStart { 0 };
		uint64_t	blockedUntil { 0 };
	};

	std::unordered_map<std::string, HostState> m_hosts;
	std::unordered_map<std::string, AccountState> m_accounts;
};
