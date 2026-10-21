#pragma once

#include <cstdint>
namespace auriga::config { struct Section; }
#include <string>

// LoginServer configuration, resolved from the shared server.json.
// The schema below is the effective payload after common settings are merged.
//
// Schema:
// {
//   "port": 15000,
//   "locale": "utf8mb4",
//   "interserver_key": "...",          // optional, peer auth towards the DB
//   "block_login": "YYYYMMDD",         // optional, accounts created from here
//   "account_sql": { "host": "..", "port": 3306, "user": "..",
//                    "password": "..", "database": ".." },
//   "database_link": { "host": "..", "port": 5300 },
//   "tls": {                               // required: clients connect
//     "cert_file": "tls/login.crt",        // over TLS only (PEM chain
//     "key_file": "tls/login.key"          // and private key)
//   }
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

	struct
	{
		std::string	certFile;
		std::string	keyFile;
	} tls;
};

bool LoadLoginConfig(const auriga::config::Section& input, LoginConfig& cfg, std::string& error);
