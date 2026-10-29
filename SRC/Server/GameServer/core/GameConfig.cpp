#include "GameConfig.h"
#include "../../common/CommonDefines.h"
#include <stdexcept>

#include <Core/server_config.h>

#include <nlohmann/json.hpp>

#include <utility>

namespace
{
	void LoadSqlConnection(const nlohmann::json& section, GameSqlConfig& out)
	{
		out.host = section.value("host", std::string());
		out.port = section.value("port", 0);
		out.user = section.value("user", std::string());
		out.password = section.value("password", std::string());
		out.database = section.value("database", std::string());
	}
}

bool GameConfig::Load(const auriga::config::Section& input, std::string& error)
{
	GameConfig parsed;
	auto& server = parsed.server;
	auto& network = parsed.network;
	auto& sql = parsed.sql;
	auto& interserverKey = parsed.interserverKey;
	auto& mapAllow = parsed.mapAllow;
	auto& events = parsed.events;
	auto& shutdown = parsed.shutdown;
	auto& logs = parsed.logs;
	auto& quest = parsed.quest;
	auto& spamBlock = parsed.spamBlock;
	auto& guild = parsed.guild;
	auto& version = parsed.version;
	auto& itemDestroy = parsed.itemDestroy;
	auto& gameplay = parsed.gameplay;
	const auto& doc = input.values;
	try
	{
		if (doc.contains("server"))
		{
			const auto& s = doc.at("server");
			server.hostname = s.value("hostname", std::string());
			server.channel = static_cast<uint8_t>(s.value("channel", 0));
			server.test = s.value("test", 0);
			server.tablePostfix = s.value("table_postfix", std::string());
			server.serverId = s.value("server_id", 0);
			server.userLimit = s.value("user_limit", 32768);
			server.maxLevel = s.value("max_level", 99);
			server.stoneChance = s.value("stone_chance", 30);
			server.blockLogin = s.value("block_login", std::string("30000705"));
			server.blockCharCreation = s.value("block_char_creation", false);
			server.empireWhisper = s.value("empire_whisper", true);
			server.protectNormalPlayer = s.value("protect_normal_player", false);
			server.noticeBattleZone = s.value("notice_battle_zone", false);
			server.pkProtectLevel = s.value("pk_protect_level", -1);
			server.skillDisable = s.value("skill_disable", false);
			server.noWander = s.value("no_wander", false);
			server.shutdowned = s.value("shutdowned", false);
			server.noRegen = s.value("no_regen", false);
			server.trafficProfile = s.value("traffic_profile", false);
			server.attrTransferLimit = s.value("attr_transfer_limit", 0);
			server.passesPerSec = s.value("passes_per_sec", 25);
			server.viewRange = s.value("view_range", 5000);
			server.mallUrl = s.value("mall_url", std::string("https://www.wonder2.org/index.php/shop/login?pid=$playerID&key=$codice"));
			server.locale = s.value("locale", std::string("latin1"));
			server.dataPath = s.value("data_path", std::string("data"));
		}

		if (doc.contains("network"))
		{
			const auto& n = doc.at("network");
			network.port = static_cast<uint16_t>(n.value("port", 50080));
			network.p2pPort = static_cast<uint16_t>(n.value("p2p_port", 50900));
			network.dbAddr = n.value("db_addr", std::string());
			network.dbPort = static_cast<uint16_t>(n.value("db_port", 0));
			network.bindIp = n.value("bind_ip", std::string());

			if (n.contains("tls"))
			{
				const auto& tls = n.at("tls");
				network.tls.certFile = tls.value("cert_file", std::string());
				network.tls.keyFile = tls.value("key_file", std::string());
			}
		}

		if (doc.contains("sql"))
		{
			const auto& sqlDoc = doc.at("sql");

			if (sqlDoc.contains("player"))
				LoadSqlConnection(sqlDoc.at("player"), sql.player);

			if (sqlDoc.contains("common"))
				LoadSqlConnection(sqlDoc.at("common"), sql.common);

			if (sqlDoc.contains("log"))
				LoadSqlConnection(sqlDoc.at("log"), sql.log);
		}

		interserverKey = doc.value("interserver_key", std::string());

		if (doc.contains("map_allow") && doc.at("map_allow").is_array())
		{
			for (const auto& v : doc.at("map_allow"))
				mapAllow.push_back(v.get<int32_t>());
			if(mapAllow.size()>MAP_ALLOW_LIMIT)throw std::runtime_error("map_allow exceeds the interserver routing capacity of "+std::to_string(MAP_ALLOW_LIMIT));
		}

		if (doc.contains("events"))
		{
			const auto& e = doc.at("events");
			events.saveSecondCycle = e.value("save_second_cycle", 120);
			events.pingSecondCycle = e.value("ping_second_cycle", 60);
		}

		if (doc.contains("shutdown"))
		{
			const auto& sh = doc.at("shutdown");
			shutdown.age = sh.value("age", 0);
			shutdown.enable = sh.value("enable", 0);
		}

		if (doc.contains("logs"))
		{
			const auto& l = doc.at("logs");
			logs.keepDays = l.value("keep_days", 2);
			logs.dbLevel = l.value("db_level", 3);
			logs.sysLevel = l.value("sys_level", 3);
		}

		if (doc.contains("quest"))
		{
			const auto& q = doc.at("quest");
			quest.dir = q.value("dir", std::string());

			if (q.contains("object_dir") && q.at("object_dir").is_array())
			{
				for (const auto& v : q.at("object_dir"))
					quest.objectDir.push_back(v.get<std::string>());
			}
		}

		if (doc.contains("spam_block"))
		{
			const auto& sp = doc.at("spam_block");
			spamBlock.duration = sp.value("duration", 900u);
			spamBlock.score = sp.value("score", 100u);
			spamBlock.reloadCycle = sp.value("reload_cycle", 600u);
			spamBlock.maxLevel = sp.value("max_level", 10);
		}

		if (doc.contains("guild"))
		{
			const auto& g = doc.at("guild");
			guild.markServer = g.value("mark_server", true);
			guild.markMinLevel = static_cast<uint8_t>(g.value("mark_min_level", 3));
			guild.inviteLimit = g.value("invite_limit", false);
			guild.infiniteMembers = g.value("infinite_members", false);
		}

		if (doc.contains("version"))
		{
			const auto& v = doc.at("version");
			version.checkServer = v.value("check_server", true);
			version.value = v.value("value", std::string("20251204"));
		}

		if (doc.contains("item_destroy"))
		{
			const auto& d = doc.at("item_destroy");
			itemDestroy.autogive = d.value("autogive", 300);
			itemDestroy.dropgold = d.value("dropgold", 150);
			itemDestroy.dropitem = d.value("dropitem", 30);
		}

		if (doc.contains("gameplay"))
		{
			const auto& gp = doc.at("gameplay");
#ifdef ENABLE_NEW_STACK_LIMIT
			gameplay.itemCountLimit = gp.value("item_count_limit", 50000);
#else
			gameplay.itemCountLimit = gp.value("item_count_limit", 200);
#endif
			gameplay.empireShopPriceTripleDisable = gp.value("empire_shop_price_triple_disable", false);
			gameplay.shoutAddonEnable = gp.value("shout_addon_enable", false);
			gameplay.globalShoutEnable = gp.value("global_shout_enable", false);
			gameplay.disablePrismNeed = gp.value("disable_prism_need", false);
			gameplay.disableEmotionMask = gp.value("disable_emotion_mask", false);
			gameplay.itemBonusChangeTime = gp.value("item_bonus_change_time", 60u);
			gameplay.allMountAttack = gp.value("all_mount_attack", true);
			gameplay.enableBootaryCheck = gp.value("enable_bootary_check", false);
			gameplay.gmHostCheck = gp.value("gm_host_check", false);
			gameplay.statusPointGetLevelLimit = gp.value("status_point_get_level_limit", 90);
			gameplay.statusPointSetMaxValue = gp.value("status_point_set_max_value", 90);
			gameplay.shoutLimitLevel = gp.value("shout_limit_level", 15);
			gameplay.disableEmpireLanguageCheck = gp.value("disable_empire_language_check", false);
			gameplay.skillBookNextReadMin = gp.value("skillbook_nextread_min", 28800u);
			gameplay.skillBookNextReadMax = gp.value("skillbook_nextread_max", 43200u);
		}
	}
	catch (const std::exception&)
	{
		error = "GameServer configuration contains an invalid field type";
		return false;
	}

	if (!sql.player.IsValid())
	{
		error = "GameServer: 'sql.player' is missing";
		return false;
	}

	if (!sql.common.IsValid())
	{
		error = "GameServer: 'sql.common' is missing";
		return false;
	}

	if (!sql.log.IsValid())
	{
		error = "GameServer: 'sql.log' is missing";
		return false;
	}

	*this = std::move(parsed);
	error.clear();
	return true;
}
