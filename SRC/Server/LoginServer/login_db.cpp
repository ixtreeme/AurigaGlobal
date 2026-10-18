#include "login_db.h"

#include "login_config.h"

#include <Core/Logging.hpp>

LoginDB::LoginDB() = default;

LoginDB::~LoginDB()
{
	// No callbacks during shutdown. Tickets remain owned until the worker
	// stops, including queries that have not reached the result queue yet.
	m_sql.Quit();
	SQLMsg* msg = nullptr;
	while (m_sql.PopResult(&msg))
		delete msg;
	m_pending.clear();
}

bool LoginDB::Connect(const LoginConfig& config)
{
	if (config.sql.host.empty() || config.sql.database.empty())
	{
		LOG_ERROR("LoginServer: PLAYER_SQL is missing from the configuration");
		return false;
	}

	m_sql.SetTimeouts(5);
	m_sql.SetRetryConnectionErrors(false);
	m_sql.SetFlushOnQuit(false);
	if (!m_sql.Setup(config.sql.host.c_str(), config.sql.user.c_str(), config.sql.password.c_str(),
			config.sql.database.c_str(), config.locale.c_str(), false, config.sql.port) ||
		!m_sql.WaitForConnection(std::chrono::seconds(10)))
	{
		LOG_ERROR("LoginServer: cannot open the account database connection");
		return false;
	}

	LOG_INFO("LoginServer: account database connected ({}:{}/{})",
		config.sql.host, config.sql.port, config.sql.database);

	return true;
}

bool LoginDB::ConnectDirect(const LoginConfig& config)
{
	if (config.sql.host.empty() || config.sql.database.empty())
		return false;

	m_direct.SetTimeouts(5);
	return m_direct.Setup(config.sql.host.c_str(), config.sql.user.c_str(), config.sql.password.c_str(),
		config.sql.database.c_str(), config.locale.c_str(), true, config.sql.port);
}

SQLMsg* LoginDB::DirectQuery(const char* sql)
{
	return m_direct.DirectQuery(sql);
}

void LoginDB::Query(const std::string& sql, const SqlParams& params, Callback callback)
{
	auto pending = std::make_unique<PendingQuery>(PendingQuery { std::move(callback) });
	auto* ticket = pending.get();
	m_pending.emplace(ticket, std::move(pending));
	m_sql.ReturnQueryPrepared(sql.c_str(), params, ticket);
}

void LoginDB::Process()
{
	SQLMsg* msg = nullptr;

	while (m_sql.PopResult(&msg))
	{
		std::unique_ptr<SQLMsg> result(msg);
		const auto found = m_pending.find(static_cast<PendingQuery*>(msg->pvUserData));
		if (found != m_pending.end())
		{
			auto pending = std::move(found->second);
			m_pending.erase(found);
			if (pending->callback)
				pending->callback(msg);
		}
	}
}
