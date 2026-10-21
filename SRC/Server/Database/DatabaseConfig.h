#pragma once

#include <cstdint>
namespace auriga::config { struct Section; }
#include <string>

// Typed Database server configuration, resolved from the shared server.json.
// The schema below is the effective payload after common settings are merged.
//
// Schema:
// {
//   "server":  { "test": false, "log": true, "locale": "utf8mb4",
//                "table_postfix": "", "player_id_start": 1,
//                "name_column": "name", "client_heart_fps": 50,
//                "log_keep_days": 3, "interserver_key": "",
//                "current_battle_pass_id": 1 },
//   "network": { "bind_ip": "0.0.0.0", "bind_port": 5300,
//                "backup_limit_sec": 600 },
//   "sql":     { "player":  { "host": "", "port": 0, "user": "",
//                             "password": "", "database": "" },
//                "account": { ... }, "common": { ... } },
//   "cache":   { "player_flush_seconds": 300, "item_flush_seconds": 300,
//                "pricelist_flush_seconds": 540, "flush_limit_per_second": 200 },
//   "proto":   { "from_db": false, "mirror2db": false },
//   "guild":   { "poly_power": [0, 0], "poly_handicap": [0, 0] }
// }
struct SqlConnectionConfig
{
	std::string	host;
	int		port { 0 };
	std::string	user;
	std::string	password;
	std::string	database;

	bool IsValid() const { return !host.empty() && !database.empty(); }
};

struct DatabaseConfig
{
	// [start_item_id, end_item_id]
	uint32_t itemIdRange[2] { 0, 0 };

	struct ServerConfig
	{
		bool		test { false };
		bool		log { true };
		std::string	locale { "utf8mb4" };
		std::string	tablePostfix;
		uint32_t	playerIdStart { 0 };
		std::string	nameColumn { "name" };
		int		clientHeartFps { 50 };
		int		logKeepDays { 3 };
		std::string	interserverKey;
		int		currentBattlePassId { 1 };
	} server;

	struct NetworkConfig
	{
		std::string	bindIp { "0.0.0.0" };
		uint16_t	bindPort { 5300 };
		int		backupLimitSec { 600 };
	} network;

	struct SqlConfig
	{
		SqlConnectionConfig	player;
		SqlConnectionConfig	account;
		SqlConnectionConfig	common;
	} sql;

	struct CacheConfig
	{
		int	playerFlushSeconds { 300 };
		int	itemFlushSeconds { 300 };
		int	pricelistFlushSeconds { 540 };
		int	flushLimitPerSecond { 200 };
	} cache;

	struct ProtoConfig
	{
		bool	fromDb { false };
		bool	mirror2db { false };
	} proto;

	struct GuildConfig
	{
		uint32_t	polyPower[2] { 0, 0 };
		uint32_t	polyHandicap[2] { 0, 0 };
	} guild;

	bool Load(const auriga::config::Section& input, std::string& error);
};

// Process-wide configuration instance, loaded once at startup.
DatabaseConfig& DbConfig();
