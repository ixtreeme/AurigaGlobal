#pragma once

#include <SQL/AsyncSQL.h>

#include <functional>
#include <string>

struct LoginConfig;

// Account DB access for the login server: one asynchronous connection whose
// results are delivered to per-query callbacks on the server loop.
class LoginDB
{
public:
	using Callback = std::function<void(SQLMsg*)>;

	LoginDB();
	~LoginDB();

	LoginDB(const LoginDB&) = delete;
	LoginDB& operator=(const LoginDB&) = delete;

	bool Connect(const LoginConfig& config);

	// Synchronous connection for startup work (blocked list, ...).
	bool ConnectDirect(const LoginConfig& config);
	SQLMsg* DirectQuery(const char* sql);

	// Runs a parameterized query; the callback fires from Process().
	void Query(const std::string& sql, const SqlParams& params, Callback callback);

	// Server loop: dispatches the finished queries.
	void Process();

private:
	struct PendingQuery
	{
		Callback callback;
	};

	CAsyncSQL m_sql;
	CAsyncSQL m_direct;
};
