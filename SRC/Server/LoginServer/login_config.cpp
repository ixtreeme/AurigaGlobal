#include "login_config.h"

#include <Core/server_config.h>

#include <nlohmann/json.hpp>

#include <utility>

bool LoadLoginConfig(const auriga::config::Section& input, LoginConfig& cfg, std::string& error)
{
	LoginConfig parsed;
	const auto& doc = input.values;
	try
	{
		parsed.listenPort = doc.value("port", 0);
		parsed.locale = doc.value("locale", std::string("utf8mb4"));
		parsed.interserverKey = doc.value("interserver_key", std::string());
		parsed.blockLoginDate = doc.value("block_login", std::string());

		if (doc.contains("account_sql"))
		{
			const auto& sql = doc.at("account_sql");

			parsed.sql.host = sql.value("host", std::string());
			parsed.sql.port = sql.value("port", 0);
			parsed.sql.user = sql.value("user", std::string());
			parsed.sql.password = sql.value("password", std::string());
			parsed.sql.database = sql.value("database", std::string());
		}

		if (doc.contains("database_link"))
		{
			const auto& link = doc.at("database_link");

			parsed.dbLink.host = link.value("host", std::string());
			parsed.dbLink.port = static_cast<uint16_t>(link.value("port", 0));
		}

		if (doc.contains("tls"))
		{
			const auto& tls = doc.at("tls");

			parsed.tls.certFile = tls.value("cert_file", std::string());
			parsed.tls.keyFile = tls.value("key_file", std::string());
		}
	}
	catch (const std::exception&)
	{
		error = "LoginServer configuration contains an invalid field type";
		return false;
	}

	if (parsed.listenPort == 0)
	{
		error = "LoginServer: 'port' is missing or zero";
		return false;
	}

	if (parsed.tls.certFile.empty() || parsed.tls.keyFile.empty())
	{
		error = "LoginServer: 'tls' with 'cert_file' and 'key_file' is required";
		return false;
	}

	cfg = std::move(parsed);
	error.clear();
	return true;
}
