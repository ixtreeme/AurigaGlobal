#include "login_config.h"

#include <Core/Logging.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

bool LoadLoginConfig(const char* path, LoginConfig& cfg)
{
	std::ifstream in(path);

	if (!in)
	{
		LOG_ERROR("LoginServer: cannot open config '{}'", path ? path : "(null)");
		return false;
	}

	nlohmann::json doc;

	try
	{
		doc = nlohmann::json::parse(in);
	}
	catch (const std::exception& ex)
	{
		LOG_ERROR("LoginServer: invalid JSON in '{}': {}", path, ex.what());
		return false;
	}

	try
	{
		cfg.listenPort = doc.value("port", 0);
		cfg.locale = doc.value("locale", std::string("utf8mb4"));
		cfg.interserverKey = doc.value("interserver_key", std::string());
		cfg.blockLoginDate = doc.value("block_login", std::string());

		if (doc.contains("account_sql"))
		{
			const auto& sql = doc.at("account_sql");

			cfg.sql.host = sql.value("host", std::string());
			cfg.sql.port = sql.value("port", 0);
			cfg.sql.user = sql.value("user", std::string());
			cfg.sql.password = sql.value("password", std::string());
			cfg.sql.database = sql.value("database", std::string());
		}

		if (doc.contains("database_link"))
		{
			const auto& link = doc.at("database_link");

			cfg.dbLink.host = link.value("host", std::string());
			cfg.dbLink.port = static_cast<uint16_t>(link.value("port", 0));
		}
	}
	catch (const std::exception& ex)
	{
		LOG_ERROR("LoginServer: config type error in '{}': {}", path, ex.what());
		return false;
	}

	if (cfg.listenPort == 0)
	{
		LOG_ERROR("LoginServer: 'port' is missing or zero in '{}'", path);
		return false;
	}

	return true;
}
