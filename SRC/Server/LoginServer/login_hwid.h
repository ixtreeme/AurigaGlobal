#pragma once

#include <set>
#include <string>

class LoginDB;
class LoginSession;

// HWID handling for the login server, mirroring the auth side of the game
// server: the blocked list is loaded at startup, blocked hardware gets the
// account banned, everyone else has the hardware id stored.
class LoginHwid
{
public:
	bool Initialize(LoginDB& db);

	// Returns false (and sends LOGIN_FAILURE) when the login must be
	// rejected; the account/hwid updates are queued otherwise.
	bool CheckAndUpdate(LoginSession* session, const std::string& login,
		const std::string& hwid, uint32_t accountId);

private:
	LoginDB* m_db { nullptr };
	std::set<std::string> m_blocked;
};
