#pragma once

#include "login_db.h"
#include "login_hwid.h"

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>

struct LoginCGLogin3;
struct LoginConfig;
class LoginDBLink;
class LoginLimiter;
class LoginSession;

// Account authentication for the login server: credential validation against
// the account database, duplicate-login tracking and the login key reply.
class LoginAuth
{
public:
	using RandomBytes = int (*)(unsigned char*, int);
	explicit LoginAuth(RandomBytes randomBytes = nullptr);
	~LoginAuth();

	LoginAuth(const LoginAuth&) = delete;
	LoginAuth& operator=(const LoginAuth&) = delete;

	bool Init(const LoginConfig& config);

	// Database server link that registers the successful logins.
	void SetLink(LoginDBLink* link) { m_link = link; }

	// Abuse guard for the login attempts.
	void SetLimiter(LoginLimiter* limiter) { m_limiter = limiter; }

	// Server loop: dispatches the finished account queries.
	void Process(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

	// LOGIN3 from a session.
	void RequestLogin(LoginSession* session, const LoginCGLogin3* packet);

	// The session left the server.
	void OnSessionClosed(LoginSession* session);

private:
	void OnLoginResult(LoginSession* session, const std::string& login, SQLMsg* msg, const LoginCGLogin3& request);
	void FailLogin(LoginSession* session, const std::string& login, const char* status);
	void OnRegistrationResult(LoginSession* session, const std::string& login, uint32_t loginKey, bool success);
	uint32_t GenerateLoginKey();

	std::string m_blockLoginDate;

	LoginDB m_db;
	LoginHwid m_hwid;
	LoginDBLink* m_link { nullptr };
	LoginLimiter* m_limiter { nullptr };
	RandomBytes m_randomBytes;
	std::set<std::string> m_pendingLogins;
	std::unordered_map<std::string, LoginSession*> m_logins;
	struct Attempt
	{
		LoginSession* session;
		std::string login;
		std::chrono::steady_clock::time_point deadline;
	};
	// Callbacks hold weak tickets. Closing or timing out an attempt releases
	// the session immediately; a late SQL/Database result cannot access it.
	std::unordered_map<LoginSession*, std::shared_ptr<Attempt>> m_attempts;
};
