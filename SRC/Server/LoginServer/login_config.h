#pragma once

#include <cstdint>
#include <string>

// LoginServer configuration, parsed from the shared conf.txt format. Only
// the tokens the login/auth role needs are read; every other token is
// ignored, so the login server can run from a trimmed auth conf.
struct LoginConfig
{
	uint16_t	listenPort { 0 };
	std::string	locale { "utf8mb4" };

	std::string	authMaster;      // "master" or "<ip> <port>" (AUTH_SERVER)
	std::string	sqlHost;
	std::string	sqlUser;
	std::string	sqlPassword;
	std::string	sqlDatabase;
	int		sqlPort { 0 };

	// Link towards the Database server (db_addr / db_port) and the shared
	// inter-server key used for the peer authentication handshake.
	std::string	dbLinkHost;
	uint16_t	dbLinkPort { 0 };
	std::string	interserverKey;

	// BLOCK_LOGIN: accounts created on/after this date are rejected.
	std::string	blockLoginDate;
};

bool LoadLoginConfig(const char* path, LoginConfig& cfg);
