#include "stdafx.h"
#include "DatabaseConfig.h"

#include <Core/Logging.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

DatabaseConfig& DbConfig()
{
	static DatabaseConfig s_config;
	return s_config;
}

namespace
{
	void LoadSqlConnection(const nlohmann::json& section, SqlConnectionConfig& out)
	{
		out.host = section.value("host", std::string());
		out.port = section.value("port", 0);
		out.user = section.value("user", std::string());
		out.password = section.value("password", std::string());
		out.database = section.value("database", std::string());
	}
}

bool DatabaseConfig::Load(const char* path)
{
	std::ifstream in(path);

	if (!in)
	{
		LOG_ERROR("Database: cannot open config '{}'", path ? path : "(null)");
		return false;
	}

	nlohmann::json doc;

	try
	{
		doc = nlohmann::json::parse(in);
	}
	catch (const std::exception& ex)
	{
		LOG_ERROR("Database: invalid JSON in '{}': {}", path, ex.what());
		return false;
	}

	try
	{
		if (doc.contains("server"))
		{
			const auto& s = doc.at("server");
			server.test = s.value("test", false);
			server.log = s.value("log", true);
			server.locale = s.value("locale", std::string("utf8mb4"));
			server.tablePostfix = s.value("table_postfix", std::string());
			server.playerIdStart = s.value("player_id_start", 0u);
			server.nameColumn = s.value("name_column", std::string("name"));
			server.clientHeartFps = s.value("client_heart_fps", 50);
			server.logKeepDays = s.value("log_keep_days", 3);
			server.interserverKey = s.value("interserver_key", std::string());
			server.currentBattlePassId = s.value("current_battle_pass_id", 1);
		}

		if (doc.contains("network"))
		{
			const auto& n = doc.at("network");
			network.bindIp = n.value("bind_ip", std::string("0.0.0.0"));
			network.bindPort = static_cast<uint16_t>(n.value("bind_port", 5300));
			network.backupLimitSec = n.value("backup_limit_sec", 600);
		}

		if (doc.contains("item_id_range") && doc.at("item_id_range").is_array())
		{
			const auto& v = doc.at("item_id_range");
			itemIdRange[0] = v.size() > 0 ? v[0].get<uint32_t>() : 0u;
			itemIdRange[1] = v.size() > 1 ? v[1].get<uint32_t>() : 0u;
		}

		if (doc.contains("sql"))
		{
			const auto& sqlDoc = doc.at("sql");

			if (sqlDoc.contains("player"))
				LoadSqlConnection(sqlDoc.at("player"), sql.player);

			if (sqlDoc.contains("account"))
				LoadSqlConnection(sqlDoc.at("account"), sql.account);

			if (sqlDoc.contains("common"))
				LoadSqlConnection(sqlDoc.at("common"), sql.common);
		}

		if (doc.contains("cache"))
		{
			const auto& c = doc.at("cache");
			cache.playerFlushSeconds = c.value("player_flush_seconds", 300);
			cache.itemFlushSeconds = c.value("item_flush_seconds", 300);
			cache.pricelistFlushSeconds = c.value("pricelist_flush_seconds", 540);
			cache.flushLimitPerSecond = c.value("flush_limit_per_second", 200);
		}

		if (doc.contains("proto"))
		{
			const auto& p = doc.at("proto");
			proto.fromDb = p.value("from_db", false);
			proto.mirror2db = p.value("mirror2db", false);
		}

		if (doc.contains("guild"))
		{
			const auto& g = doc.at("guild");

			if (g.contains("poly_power") && g.at("poly_power").is_array())
			{
				const auto& v = g.at("poly_power");
				guild.polyPower[0] = v.size() > 0 ? v[0].get<uint32_t>() : 0u;
				guild.polyPower[1] = v.size() > 1 ? v[1].get<uint32_t>() : 0u;
			}

			if (g.contains("poly_handicap") && g.at("poly_handicap").is_array())
			{
				const auto& v = g.at("poly_handicap");
				guild.polyHandicap[0] = v.size() > 0 ? v[0].get<uint32_t>() : 0u;
				guild.polyHandicap[1] = v.size() > 1 ? v[1].get<uint32_t>() : 0u;
			}
		}
	}
	catch (const std::exception& ex)
	{
		LOG_ERROR("Database: config type error in '{}': {}", path, ex.what());
		return false;
	}

	if (!sql.player.IsValid())
	{
		LOG_ERROR("Database: 'sql.player' is missing from '{}'", path);
		return false;
	}

	return true;
}
