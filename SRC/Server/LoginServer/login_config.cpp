#include "login_config.h"

#include <Core/Logging.hpp>

#include <fstream>
#include <sstream>

// The conf.txt format is line based: "token value...". Unknown tokens are
// ignored so the login server can share the layout with the game confs.
bool LoadLoginConfig(const char* path, LoginConfig& cfg)
{
	std::ifstream in(path);

	if (!in)
	{
		LOG_ERROR("LoginServer: cannot open config '{}'", path ? path : "(null)");
		return false;
	}

	std::string line;

	while (std::getline(in, line))
	{
		std::istringstream iss(line);
		std::string token;

		if (!(iss >> token))
			continue;

		if (token == "port")
		{
			int value = 0;
			iss >> value;
			cfg.listenPort = static_cast<uint16_t>(value);
		}
		else if (token == "LOCALE")
		{
			iss >> cfg.locale;
		}
		else if (token == "AUTH_SERVER")
		{
			std::string first;
			iss >> first;

			if (first == "master")
				cfg.authMaster = "master";
			else
			{
				int port = 0;
				iss >> port;
				cfg.authMaster = first + " " + std::to_string(port);
			}
		}
		else if (token == "PLAYER_SQL")
		{
			iss >> cfg.sqlHost >> cfg.sqlUser >> cfg.sqlPassword >> cfg.sqlDatabase >> cfg.sqlPort;
		}
		else if (token == "db_addr")
		{
			iss >> cfg.dbLinkHost;

			// Some confs carry the port on the same line, others in the
			// separate db_port token.
			int value = 0;
			if (iss >> value)
				cfg.dbLinkPort = static_cast<uint16_t>(value);
		}
		else if (token == "db_port")
		{
			int value = 0;
			iss >> value;
			cfg.dbLinkPort = static_cast<uint16_t>(value);
		}
		else if (token == "INTERSERVER_KEY")
		{
			iss >> cfg.interserverKey;
		}
		else if (token == "BLOCK_LOGIN")
		{
			iss >> cfg.blockLoginDate;
		}
	}

	if (cfg.listenPort == 0)
	{
		LOG_ERROR("LoginServer: 'port' is missing from '{}'", path);
		return false;
	}

	return true;
}
