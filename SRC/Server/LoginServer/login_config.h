#pragma once

#include <cstdint>
#include <string>

// LoginServer configuration, loaded from conf.json.
//
// Schema:
// {
//   "port": 15000,
//   "locale": "utf8mb4",
//   "interserver_key": "...",          // optional, peer auth towards the DB
//   "block_login": "YYYYMMDD",         // optional, accounts created from here
//   "account_sql": { "host": "..", "port": 3306, "user": "..",
//                    "password": "..", "database": ".." },
//   "database_link": { "host": "..", "port": 5300 }
// }
struct LoginConfig
{
	uint16_t	listenPort { 0 };
	std::string	locale { "utf8mb4" };

	std::string	interserverKey;
	std::string	blockLoginDate;

	struct
	{
		std::string	host;
		int		port { 0 };
		std::string	user;
		std::string	password;
		std::string	database;
	} sql;

	struct
	{
		std::string	host;
		uint16_t	port { 0 };
	} dbLink;
};

bool LoadLoginConfig(const char* path, LoginConfig& cfg);
