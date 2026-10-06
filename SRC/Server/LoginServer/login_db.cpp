#include "login_db.h"

#include "login_config.h"

#include <Core/Logging.hpp>

LoginDB::LoginDB() = default;

LoginDB::~LoginDB() = default;

bool LoginDB::Connect(const LoginConfig& config)
{
	if (config.sql.host.empty() || config.sql.database.empty())
	{
		LOG_ERROR("LoginServer: PLAYER_SQL is missing from the configuration");
		return false;
	}

	if (!m_sql.Setup(config.sql.host.c_str(), config.sql.user.c_str(), config.sql.password.c_str(),
			config.sql.database.c_str(), config.locale.c_str(), false, config.sql.port))
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

	return m_direct.Setup(config.sql.host.c_str(), config.sql.user.c_str(), config.sql.password.c_str(),
		config.sql.database.c_str(), config.locale.c_str(), true, config.sql.port);
}

SQLMsg* LoginDB::DirectQuery(const char* sql)
{
	return m_direct.DirectQuery(sql);
}

void LoginDB::Query(const std::string& sql, const SqlParams& params, Callback callback)
{
	auto* pending = new PendingQuery { std::move(callback) };

	m_sql.ReturnQueryPrepared(sql.c_str(), params, pending);
}

void LoginDB::Process()
{
	SQLMsg* msg = nullptr;

	while (m_sql.PopResult(&msg))
	{
		auto* pending = static_cast<PendingQuery*>(msg->pvUserData);

		if (pending)
		{
			if (pending->callback)
				pending->callback(msg);

			delete pending;
		}

		delete msg;
	}
}
